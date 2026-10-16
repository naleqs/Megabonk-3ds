"""Bake Unity generic (transform) AnimationClips onto a SkinnedMeshRenderer.

Unity stores runtime clips in a "muscle clip": streamed curves (cubic segments),
dense curves (fixed-rate samples) and constant curves, in that order, bound to
transforms through m_ClipBindingConstant.genericBindings by CRC32 of the
transform path relative to the Animator. We evaluate those curves, rebuild the
bone hierarchy and CPU-skin the mesh for each baked frame.
"""
import math
import struct
import zlib

import numpy as np

ATTR_POS, ATTR_QUAT, ATTR_SCALE, ATTR_EULER = 1, 2, 3, 4
TYPE_TRANSFORM = 4
_CURVES_PER_ATTR = {ATTR_POS: 3, ATTR_QUAT: 4, ATTR_SCALE: 3, ATTR_EULER: 3}


def quat_mat(x, y, z, w):
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ])


def euler_mat(ex, ey, ez):
    """Unity euler angles (degrees), applied Z, X, then Y."""
    x, y, z = (math.radians(a) for a in (ex, ey, ez))
    rx = np.array([[1, 0, 0], [0, math.cos(x), -math.sin(x)], [0, math.sin(x), math.cos(x)]])
    ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]])
    rz = np.array([[math.cos(z), -math.sin(z), 0], [math.sin(z), math.cos(z), 0], [0, 0, 1]])
    return ry @ rx @ rz


class ClipSampler:
    def __init__(self, clip):
        mc = clip.m_MuscleClip
        c = mc.m_Clip.data if hasattr(mc.m_Clip, "data") else mc.m_Clip
        self.start, self.stop = mc.m_StartTime, mc.m_StopTime
        self.length = max(self.stop - self.start, 1e-3)

        # streamed: list of (time, [(curve, coeffs)]) frames
        raw = struct.pack(f"<{len(c.m_StreamedClip.data)}I", *c.m_StreamedClip.data)
        self.n_streamed = c.m_StreamedClip.curveCount
        self.stream_keys = [[] for _ in range(self.n_streamed)]   # per curve: [(time, c0..c3)]
        pos = 0
        while pos + 8 <= len(raw):
            t, nkeys = struct.unpack_from("<fi", raw, pos)
            pos += 8
            for _ in range(nkeys):
                idx, c0, c1, c2, c3 = struct.unpack_from("<iffff", raw, pos)
                pos += 20
                if 0 <= idx < self.n_streamed:
                    self.stream_keys[idx].append((t, c0, c1, c2, c3))
        self.stream_times = [np.array([k[0] for k in keys]) for keys in self.stream_keys]

        d = c.m_DenseClip
        self.n_dense = d.m_CurveCount
        self.dense = np.array(d.m_SampleArray, dtype=np.float64).reshape(-1, self.n_dense) if self.n_dense else None
        self.dense_rate, self.dense_begin = d.m_SampleRate, d.m_BeginTime
        self.constant = list(c.m_ConstantClip.data)

        # map curve index -> (path hash, attribute, component)
        self.curve_map = []
        for b in clip.m_ClipBindingConstant.genericBindings:
            n = _CURVES_PER_ATTR.get(b.attribute, 1) if b.typeID == TYPE_TRANSFORM else 1
            for k in range(n):
                self.curve_map.append((b.path, b.attribute, k, b.typeID))

    def _eval_streamed(self, i, t):
        keys = self.stream_keys[i]
        if not keys:
            return 0.0
        j = int(np.searchsorted(self.stream_times[i], t, side="right")) - 1
        j = max(0, min(j, len(keys) - 1))
        kt, c0, c1, c2, c3 = keys[j]
        if not math.isfinite(kt):   # leading sentinel key
            return c3
        dt = t - kt
        return ((c0 * dt + c1) * dt + c2) * dt + c3

    def sample(self, t):
        """-> {path_hash: {attr: [values]}} for transform curves at clip time t (seconds)."""
        t = self.start + (t % self.length)
        values = []
        for i in range(self.n_streamed):
            values.append(self._eval_streamed(i, t))
        if self.n_dense:
            f = (t - self.dense_begin) * self.dense_rate
            f0 = int(math.floor(f))
            f0 = max(0, min(f0, len(self.dense) - 1))
            f1 = min(f0 + 1, len(self.dense) - 1)
            a = f - f0
            row = self.dense[f0] * (1 - a) + self.dense[f1] * a
            values.extend(row.tolist())
        values.extend(self.constant)
        out = {}
        for (path, attr, k, type_id), v in zip(self.curve_map, values):
            if type_id != TYPE_TRANSFORM:
                continue
            out.setdefault(path, {}).setdefault(attr, [0.0] * 4)[k] = v
        return out


class Skeleton:
    """Transform hierarchy under an Animator root, addressable by Unity path hash."""

    def __init__(self, root_tr):
        self.nodes = []        # (transform, parent index)
        self.by_pid = {}
        self.hash_to_node = {}

        def walk(tr, parent, path):
            idx = len(self.nodes)
            self.nodes.append((tr, parent))
            self.by_pid[tr.object_reader.path_id] = idx
            self.hash_to_node[zlib.crc32(path.encode()) if path else 0] = idx
            for ch in tr.m_Children:
                c = ch.read()
                name = c.m_GameObject.read().m_Name
                walk(c, idx, f"{path}/{name}" if path else name)
        walk(root_tr, -1, "")

    def world_matrices(self, curves):
        mats = []
        for i, (tr, parent) in enumerate(self.nodes):
            p, q, s = tr.m_LocalPosition, tr.m_LocalRotation, tr.m_LocalScale
            pos = [p.x, p.y, p.z]
            rot = quat_mat(q.x, q.y, q.z, q.w)
            scale = [s.x, s.y, s.z]
            ch = curves.get(self._hash_of(i)) if curves else None
            if ch:
                if ATTR_POS in ch:
                    pos = ch[ATTR_POS][:3]
                if ATTR_QUAT in ch:
                    rot = quat_mat(*ch[ATTR_QUAT][:4])
                elif ATTR_EULER in ch:
                    rot = euler_mat(*ch[ATTR_EULER][:3])
                if ATTR_SCALE in ch:
                    scale = ch[ATTR_SCALE][:3]
            m = np.eye(4)
            m[:3, :3] = rot @ np.diag(scale)
            m[:3, 3] = pos
            if i == 0:
                m = np.eye(4)           # the character's own placement is applied in-game
            mats.append(m if parent < 0 else mats[parent] @ m)
        return mats

    def _hash_of(self, i):
        if not hasattr(self, "_rev"):
            self._rev = {v: k for k, v in self.hash_to_node.items()}
        return self._rev.get(i)


def skin(verts, normals, bone_idx, bone_w, bone_nodes, bindposes, world):
    """CPU linear-blend skinning -> (positions, normals) in Animator-root space."""
    nb = len(bone_nodes)
    skin_m = np.zeros((nb, 4, 4))
    for b in range(nb):
        skin_m[b] = world[bone_nodes[b]] @ bindposes[b] if bone_nodes[b] >= 0 else np.eye(4)
    vh = np.hstack([verts, np.ones((len(verts), 1))])
    outp = np.zeros((len(verts), 3))
    outn = np.zeros((len(verts), 3))
    for k in range(4):
        w = bone_w[:, k:k + 1]
        if not w.any():
            continue
        m = skin_m[bone_idx[:, k]]                       # (V,4,4)
        outp += w * np.einsum("vij,vj->vi", m, vh)[:, :3]
        outn += w * np.einsum("vij,vj->vi", m[:, :3, :3], normals)
    ln = np.linalg.norm(outn, axis=1, keepdims=True)
    outn /= np.maximum(ln, 1e-9)
    return outp, outn


def matrix4(m):
    """UnityPy Matrix4x4f -> numpy (Unity stores column-major e00,e10,... fields)."""
    if hasattr(m, "e00"):
        return np.array([[m.e00, m.e01, m.e02, m.e03], [m.e10, m.e11, m.e12, m.e13],
                         [m.e20, m.e21, m.e22, m.e23], [m.e30, m.e31, m.e32, m.e33]])
    return np.array(m, dtype=np.float64).reshape(4, 4)
