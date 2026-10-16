"""Convert Megabonk's Unity assets into 3DS-ready files.

    .venv\\Scripts\\python tools\\extract_assets.py

Outputs
  romfs/mesh/*.mbm      meshes (multi-frame for animated enemies), see source/render.c
  romfs/tex/*.tex       textures, pre-swizzled into the PICA200 tiled layout
  romfs/font/*.bcfnt    the game's pixel font
  source/gen/*.h        generated C tables (enemy stats, props, characters, timeline, strings)
  meta/icon.png         app icon

Everything is converted from the user's own Steam install; nothing from the game
is checked in.
"""
import io
import json
import math
import os
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from UnityPy.helpers.MeshHelper import MeshHandler

sys.path.insert(0, str(Path(__file__).parent))
from unity_env import attach_typetrees, load_env  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
ROMFS = ROOT / "romfs"
GEN = ROOT / "source" / "gen"
MAX_TEX = 256
DEVKITPRO = Path(os.environ.get("DEVKITPRO", ROOT / "toolchain" / "opt" / "devkitpro"))


def dkp_tool(name):
    """Path to a devkitPro host tool (e.g. mkbcfnt) for this OS."""
    return DEVKITPRO / "tools" / "bin" / (name + (".exe" if os.name == "nt" else ""))


# Character roster, in character-select order (CharacterData asset names).
ROSTER = ["Fox", "Calcium", "Knight", "Clank", "Megachad", "Ogre", "Athena", "Birdo", "Bush", "Bandit", "Monke",
          "Noelle", "Robinette", "TonyMcZoom", "Amog", "Spaceman", "Ninja", "Vlad", "Dicehead", "Chadwell", "Roberto"]
# CharacterData name -> localization key prefix (Characters table) where they differ
CHAR_LOC_KEY = {"Clank": "CL4NK", "Amog": "HAZMAT", "TonyMcZoom": "JIMMY"}
# WeaponData asset -> WeaponId in source/game_types.h
WEAPON_ENUM = {
    "WeaponFireStaff": "WEAPON_FIRESTAFF", "WeaponAura": "WEAPON_AURA", "WeaponBone": "WEAPON_BONE",
    "WeaponSword": "WEAPON_SWORD", "WeaponRevolver": "WEAPON_REVOLVER", "WeaponLightningBolt": "WEAPON_LIGHTNING",
    "WeaponAxe": "WEAPON_AXE", "WeaponDice": "WEAPON_DICE", "WeaponPoisonFlask": "WEAPON_POISONFLASK",
    "WeaponAegis": "WEAPON_AEGIS", "WeaponDexecutioner": "WEAPON_DEXECUTIONER", "WeaponTornado": "WEAPON_TORNADO",
    "WeaponSniper": "WEAPON_SNIPER", "WeaponCorruptedSword": "WEAPON_CORRUPTEDSWORD",
    "WeaponBananarang": "WEAPON_BANANARANG", "WeaponKatana": "WEAPON_KATANA",
    "WeaponFrostWalker": "WEAPON_FROSTWALKER", "WeaponScythe": "WEAPON_SCYTHE", "WeaponBow": "WEAPON_BOW",
    "WeaponBlackHole": "WEAPON_BLACKHOLE", "WeaponWirelessDagger": "WEAPON_WIRELESSDAGGER",
    "WeaponBloodMagic": "WEAPON_BLOODMAGIC",
}
# baked animations: (name, clip-name keywords, fps, loop, max frames)
CHAR_ANIMS = [("idle", ("idle",), 6, True, 40), ("run", ("run", "walk", "move", "cruise", "skat"), 12, True, 24),
              ("jump", ("jump",), 12, False, 14),
              ("push", ("push",), 12, True, 16)]   # Calcium kicking his skateboard (others don't have one)

# UI images: Texture2D name -> (output name, display width in pixels)
UI_IMAGES = {"MegabonkLogo": ("ui_logo", 360)}

# Plain prop meshes looked up by GameObject name (projectiles / pickups / interactables).
EXTRA_PROPS = {
    "B_Bones1": "bone",
    "Sword": "sword",
    "Axe": "axe",
    "DiceProjectile": "dice",
    "B_Skull": "skull",
    "Chest": "chest",
    "PotSmall": "pot",
    # interactables
    "ChestFree": "chest_free",
    "PotBig": "pot_big",
    "ShrineEnergy": "shrine_charge",
    "GreedShrine": "shrine_greed",
    "CursedShrine": "shrine_cursed",
    "MagnetShrine": "shrine_magnet",
    "ShrineMaoi": "shrine_moai",
    "ChallengeShrine": "shrine_challenge",
    "Microwave": "microwave",
    "BossSpawner": "boss_portal",
    "B_Coins": "coins",
    # weapon projectiles (particle-only prefabs simply export nothing and fall back to primitives)
    "BananaProjectile": "proj_banana", "BowProjectile": "proj_arrow", "BluetoothProjectile": "proj_dagger",
    "KatanaProjectilePrefab": "proj_katana", "ScytheAttackProjectile": "proj_scythe",
    "TornadoProjectile": "proj_tornado", "BlackHoleProjectile": "proj_blackhole",
    "PoisonFlaskProjectile": "proj_flask", "SniperProjectile": "proj_sniper",
    "CorruptedSwordProjectile": "proj_corruptedsword", "DexecutionerProjectilePrefab": "proj_dexecutioner",
}

# HUD icons: weapon icon textures (tomes/items are exported by name prefix/suffix)
WEAPON_ICONS = {"Firestaff", "Aura", "Bone", "Sword", "Revolver", "LightningStaff", "Axe", "Dice",
                "Poison Flask", "Aegis", "Dexecutioner", "Tornado", "Sniper", "CorruptedSword", "Bananarang",
                "Katana", "Frostwalker", "Scythe", "Bow", "BlackHole", "Bluetooth Knife", "BloodMagic",
                "Shotgun", "Mine", "SluttyRocket", "Space Noodle", "HeroSword", "Chunkers", "Firewalker",
                "DragonsBreath"}
MISC_ICONS = {"icon_coin", "psd_Chest", "ProjectileSpeedTOme", "BloodTOme", "Balance Tome",
              # item icons whose texture names don't start with "Item"
              "itemClover", "Bloody Cleaver", "Chonkplate", "Gasmask", "GrandmasSecretTonic", "IdleJuice",
              "JoesDagger", "Kevin", "Speed boi", "ToxicBarrel", "Unstable Transfusion", "GamerLens"}

EXTRA_TEXTURES = {"TopGrass12": "ground", "TopGrass2": "ground2", "sprite_0001": "grass",
                  "Cliffs": "cliffs"}


# --------------------------------------------------------------------------- utils

def quat_to_mat(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ], dtype=np.float64)


def euler_to_mat(e):
    """Unity euler (degrees) -> rotation matrix (Unity applies Z, then X, then Y)."""
    x, y, z = (math.radians(a) for a in e)
    rx = np.array([[1, 0, 0], [0, math.cos(x), -math.sin(x)], [0, math.sin(x), math.cos(x)]])
    ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]])
    rz = np.array([[math.cos(z), -math.sin(z), 0], [math.sin(z), math.cos(z), 0], [0, 0, 1]])
    return ry @ rx @ rz


def trs(t):
    """4x4 local matrix for a Unity Transform."""
    m = np.eye(4)
    q = t.m_LocalRotation
    s = t.m_LocalScale
    m[:3, :3] = quat_to_mat((q.x, q.y, q.z, q.w)) @ np.diag([s.x, s.y, s.z])
    p = t.m_LocalPosition
    m[:3, 3] = (p.x, p.y, p.z)
    return m


def rot_only(t):
    m = np.eye(4)
    q = t.m_LocalRotation
    s = t.m_LocalScale
    m[:3, :3] = quat_to_mat((q.x, q.y, q.z, q.w)) @ np.diag([s.x, s.y, s.z])
    return m


def safe_name(s):
    return "".join(c if c.isalnum() or c in "_-" else "_" for c in s).lower()


class Ctx:
    def __init__(self, env):
        self.env = env
        self.files = {}
        for f in env.files.values():
            self._index(f)
        self.textures_written = {}
        self.enemy_enum = {}

    def _index(self, f):
        if hasattr(f, "objects") and hasattr(f, "externals") and getattr(f, "name", None):
            self.files[Path(f.name).name.lower()] = f
        for sub in (getattr(f, "files", None) or {}).values():
            self._index(sub)

    def resolve(self, af, pptr):
        fid, pid = pptr["m_FileID"], pptr["m_PathID"]
        if pid == 0:
            return None
        if fid == 0:
            f = af
        else:
            f = self.files.get(Path(af.externals[fid - 1].path).name.lower())
            if f is None:
                return None
        return f.objects.get(pid)


# --------------------------------------------------------------------------- textures

def morton_tile_order():
    # 8x8 tile pixel order used by the PICA200 (Z-order curve)
    order = np.zeros((8, 8), dtype=np.int32)
    for y in range(8):
        for x in range(8):
            i = 0
            for b in range(3):
                i |= ((x >> b) & 1) << (2 * b)
                i |= ((y >> b) & 1) << (2 * b + 1)
            order[y, x] = i
    return order


MORTON = morton_tile_order()


def tile_image(arr):
    """arr: HxWxC numpy (top row first). Returns bytes in 3DS tiled order.

    The PICA200 samples memory row 0 at v=1, so storing rows top-first gives
    v=0 at the image bottom, matching Unity UVs (verified in the emulator
    with an asymmetric texture).
    """
    h, w = arr.shape[:2]
    c = arr.shape[2]
    out = np.zeros((h // 8, w // 8, 64, c), dtype=arr.dtype)
    tiles = arr.reshape(h // 8, 8, w // 8, 8, c).transpose(0, 2, 1, 3, 4).reshape(h // 8, w // 8, 64, c)
    out[:, :, MORTON.reshape(-1), :] = tiles
    return out.tobytes()


def pow2(n):
    p = 8
    while p < n:
        p *= 2
    return p


def write_texture(ctx, tex_obj, out_name=None, ui_width=None):
    """Write a Texture2D to romfs/tex/<name>.tex; returns the name used.

    Model textures are stretched to power-of-two sizes (UVs are 0..1 anyway).
    UI images (ui_width set) are scaled to that width and padded instead, and the
    used size is recorded so the game can draw just that sub-rectangle.
    """
    t = tex_obj.read() if hasattr(tex_obj, "read") else tex_obj
    name = out_name or safe_name(t.m_Name)
    if name in ctx.textures_written:
        return name
    img = t.image.convert("RGBA")
    if ui_width:
        w, h = img.size
        uw, uh = ui_width, max(1, round(h * ui_width / w))
        img = img.resize((uw, uh), Image.NEAREST if uw >= w else Image.LANCZOS)
        nw, nh = pow2(uw), pow2(uh)
        canvas = Image.new("RGBA", (nw, nh), (0, 0, 0, 0))
        canvas.paste(img, (0, 0))
        img = canvas
    else:
        w, h = img.size
        nw, nh = min(pow2(w), MAX_TEX), min(pow2(h), MAX_TEX)
        if (nw, nh) != (w, h):
            # Pixel-art textures: keep nearest-neighbour when upscaling small ones.
            resample = Image.NEAREST if nw >= w and nh >= h else Image.BILINEAR
            img = img.resize((nw, nh), resample)
        uw, uh = nw, nh
    a = np.asarray(img)
    has_alpha = bool((a[:, :, 3] < 250).any())
    if has_alpha:
        fmt = 0  # GPU_RGBA8, stored as ABGR bytes
        data = tile_image(a[:, :, ::-1].copy())
    else:
        fmt = 1  # GPU_RGB565
        r = (a[:, :, 0].astype(np.uint16) >> 3) << 11
        g = (a[:, :, 1].astype(np.uint16) >> 2) << 5
        b = a[:, :, 2].astype(np.uint16) >> 3
        data = tile_image((r | g | b)[:, :, None].astype("<u2"))
    d = ROMFS / "tex"
    d.mkdir(parents=True, exist_ok=True)
    (d / f"{name}.tex").write_bytes(struct.pack("<4sHHBBHHH", b"TEX2", nw, nh, fmt, 0, 0, uw, uh) + data)
    ctx.textures_written[name] = (nw, nh, fmt)
    return name


def material_texture(ctx, mat):
    if mat is None:
        return None
    m = mat.read() if hasattr(mat, "read") else mat
    envs = dict(m.m_SavedProperties.m_TexEnvs)
    for key in ("_MainTex", "_AlbedoMap", "_BaseMap"):
        if key in envs and envs[key].m_Texture.path_id:
            try:
                return write_texture(ctx, envs[key].m_Texture)
            except Exception as e:
                print("  texture failed:", e)
    return None


# --------------------------------------------------------------------------- meshes

def mesh_arrays(mesh):
    h = MeshHandler(mesh)
    h.process()
    v = np.array(h.m_Vertices, dtype=np.float64)[:, :3]
    n = np.array(h.m_Normals, dtype=np.float64)[:, :3] if h.m_Normals else np.tile([0, 1, 0], (len(v), 1))
    uv = np.array(h.m_UV0, dtype=np.float64)[:, :2] if h.m_UV0 else np.zeros((len(v), 2))
    subs = []
    for sm, tris in zip(mesh.m_SubMeshes, h.get_triangles()):
        idx = np.array(tris, dtype=np.int64).reshape(-1)
        idx = idx + getattr(sm, "baseVertex", 0)
        subs.append(idx)
    return v, n, uv, subs


def transform(v, n, m):
    v2 = v @ m[:3, :3].T + m[:3, 3]
    nm = np.linalg.inv(m[:3, :3]).T
    n2 = n @ nm.T
    n2 /= np.maximum(np.linalg.norm(n2, axis=1, keepdims=True), 1e-9)
    return v2, n2


class MeshBuilder:
    """Accumulates static geometry (possibly multi-part) or frame-animated geometry."""

    def __init__(self):
        self.frames = []    # list of (pos Nx3, nrm Nx3) -- all frames share uv/indices
        self.uv = None
        self.parts = []     # (texname, indices)
        self.anims = []     # (name, first frame, count, fps, loop)

    def add_static(self, v, n, uv, subs, texnames):
        base = 0 if self.uv is None else len(self.uv)
        if not self.frames:
            self.frames.append((v, n))
            self.uv = uv
        else:
            p, q = self.frames[0]
            self.frames[0] = (np.vstack([p, v]), np.vstack([q, n]))
            self.uv = np.vstack([self.uv, uv])
        for idx, tex in zip(subs, texnames):
            self.parts.append((tex, idx + base))

    def write(self, name, flip_winding=False):
        nverts = len(self.uv)
        if nverts > 65535:
            raise ValueError(f"{name}: too many vertices ({nverts})")
        # merge parts sharing a texture
        merged = {}
        for tex, idx in self.parts:
            merged.setdefault(tex or "", []).append(idx)
        parts, indices = [], []
        first = 0
        for tex, lst in merged.items():
            idx = np.concatenate(lst).astype(np.uint16)
            if flip_winding:
                idx = idx.reshape(-1, 3)[:, ::-1].reshape(-1)
            parts.append((tex, first, len(idx)))
            indices.append(idx)
            first += len(idx)
        indices = np.concatenate(indices) if indices else np.zeros(0, np.uint16)

        allpos = np.vstack([f[0] for f in self.frames])
        radius = float(np.sqrt((allpos[:, 0] ** 2 + allpos[:, 2] ** 2).max()))
        miny, maxy = float(allpos[:, 1].min()), float(allpos[:, 1].max())
        pos_scale = max(float(np.abs(allpos).max()), 1e-4) / 32767.0

        # MBM2: quantized frames (s16 pos x4, s8 normal x4 = 12 bytes per vertex)
        b = io.BytesIO()
        b.write(struct.pack("<4sHHHHIffff", b"MBM2", nverts, len(self.frames), len(parts), len(self.anims),
                            len(indices), pos_scale, radius, miny, maxy))
        for aname, afirst, acount, afps, aloop in self.anims:
            b.write(struct.pack("<16sHHfB3x", aname.encode()[:15], afirst, acount, afps, 1 if aloop else 0))
        for tex, first, count in parts:
            b.write(struct.pack("<32sII", tex.encode()[:31], first, count))
        for pos, nrm in self.frames:
            q = np.zeros((nverts, 4), "<i2")
            q[:, :3] = np.clip(np.round(pos / pos_scale), -32767, 32767)
            nq = np.zeros((nverts, 4), "i1")
            nq[:, :3] = np.clip(np.round(nrm * 127), -127, 127)
            b.write(np.hstack([q.view(np.uint8), nq.view(np.uint8)]).tobytes())
        b.write(self.uv.astype("<f4").tobytes())
        b.write(indices.astype("<u2").tobytes())
        if len(indices) % 2:
            b.write(b"\0\0")
        d = ROMFS / "mesh"
        d.mkdir(parents=True, exist_ok=True)
        (d / f"{name}.mbm").write_bytes(b.getvalue())
        return dict(radius=radius, miny=miny, maxy=maxy, verts=nverts, frames=len(self.frames))


def collect_hierarchy(ctx, go, parent_m, out, root=False):
    """Collect (mesh, materials, matrix) for every mesh renderer under go."""
    tr = go.m_Transform.read()
    m = parent_m @ (rot_only(tr) if root else trs(tr))
    mf = mr = None
    for c in go.m_Components:
        tn = c.type.name
        if tn == "MeshFilter":
            mf = c.read()
        elif tn == "MeshRenderer":
            mr = c.read()
            if not mr.m_Enabled:
                mr = None
        elif tn == "SkinnedMeshRenderer":
            # bind pose, placed by the renderer's own transform (like the characters)
            smr = c.read()
            if smr.m_Enabled and smr.m_Mesh.path_id:
                out.append((smr.m_Mesh.read(), smr.m_Materials, m))
    if mf is not None and mr is not None and mf.m_Mesh.path_id:
        out.append((mf.m_Mesh.read(), mr.m_Materials, m))
    for ch in tr.m_Children:
        cgo = ch.read().m_GameObject.read()
        if not cgo.m_IsActive:
            continue
        # skip LOD / collider-only / fx / billboard-icon children by name
        if any(k in cgo.m_Name.lower() for k in ("collider", "lod1", "lod2", "shadow", "fx", "particle",
                                                 "icon", "zone", "arrow", "spark")):
            continue
        collect_hierarchy(ctx, cgo, m, out)


def export_prefab(ctx, go, name):
    items = []
    collect_hierarchy(ctx, go, np.eye(4), items, root=True)
    if not items:
        print("  no meshes in", name)
        return None
    mb = MeshBuilder()
    for mesh, mats, m in items:
        v, n, uv, subs = mesh_arrays(mesh)
        v, n = transform(v, n, m)
        texs = [material_texture(ctx, mats[i] if i < len(mats) else (mats[-1] if mats else None))
                for i in range(len(subs))]
        mb.add_static(v, n, uv, subs, texs)
    return mb.write(name)


# --------------------------------------------------------------------------- exporters

def find_animator_root(go):
    """The GameObject carrying the Animator (curve paths are relative to it), searching down."""
    tr = go.m_Transform.read()
    stack = [tr]
    while stack:
        t = stack.pop()
        g = t.m_GameObject.read()
        for c in g.m_Components:
            if c.type.name == "Animator":
                return g, c.read()
        stack.extend(ch.read() for ch in t.m_Children)
    return go, None


def controller_clips(animator):
    if animator is None or not animator.m_Controller.path_id:
        return []
    ctrl = animator.m_Controller.read()
    clips = []
    if hasattr(ctrl, "m_Clips"):            # AnimatorOverrideController
        for pair in ctrl.m_Clips:
            p = pair.m_OverrideClip if pair.m_OverrideClip.path_id else pair.m_OriginalClip
            if p.path_id:
                clips.append(p.read())
        base = ctrl.m_Controller.read() if ctrl.m_Controller.path_id else None
        if base is not None and not clips:
            clips = [c.read() for c in base.m_AnimationClips if c.path_id]
    else:
        clips = [c.read() for c in ctrl.m_AnimationClips if c.path_id]
    return clips


def bake_animated(ctx, prefab_go, out_name, anim_specs):
    """Skin every renderer under the prefab for each wanted clip -> one multi-frame MBM2."""
    import anim_bake as ab
    root_go, animator = find_animator_root(prefab_go)
    skel = ab.Skeleton(root_go.m_Transform.read())
    clips = controller_clips(animator)

    # renderers (active, LOD0 only)
    parts = []   # dict(kind, mesh arrays, bones, bindposes, node)
    def walk(tr):
        g = tr.m_GameObject.read()
        lname = g.m_Name.lower()
        if not g.m_IsActive or "collider" in lname:
            return
        # LOD copies / shadows / billboards: skip the renderer but keep walking (bones can sit below)
        skip_render = lname.endswith(("low", "lowest")) or any(k in lname for k in ("lod1", "lod2", "shadow", "icon"))
        mf = mr = None
        for c in ([] if skip_render else g.m_Components):
            if c.type.name == "SkinnedMeshRenderer":
                r = c.read()
                if r.m_Enabled and r.m_Mesh.path_id:
                    mesh = r.m_Mesh.read()
                    v, n, uv, subs = mesh_arrays(mesh)
                    h = MeshHandler(mesh)
                    h.process()
                    texs = [material_texture(ctx, r.m_Materials[min(i, len(r.m_Materials) - 1)]) if r.m_Materials else None
                            for i in range(len(subs))]
                    parts.append(dict(kind="skin", v=v, n=n, uv=uv, subs=subs, texs=texs,
                                      bi=np.array(h.m_BoneIndices, dtype=int) if h.m_BoneIndices else None,
                                      bw=np.array(h.m_BoneWeights, dtype=np.float64) if h.m_BoneWeights else None,
                                      bones=[skel.by_pid.get(b.path_id, -1) for b in r.m_Bones],
                                      bind=[ab.matrix4(m) for m in mesh.m_BindPose],
                                      node=skel.by_pid.get(tr.object_reader.path_id, 0)))
            elif c.type.name == "MeshFilter":
                mf = c.read()
            elif c.type.name == "MeshRenderer":
                mr = c.read() if c.read().m_Enabled else None
        if mf is not None and mr is not None and mf.m_Mesh.path_id:
            v, n, uv, subs = mesh_arrays(mf.m_Mesh.read())
            texs = [material_texture(ctx, mr.m_Materials[min(i, len(mr.m_Materials) - 1)]) if mr.m_Materials else None
                    for i in range(len(subs))]
            parts.append(dict(kind="rigid", v=v, n=n, uv=uv, subs=subs, texs=texs,
                              node=skel.by_pid.get(tr.object_reader.path_id, 0)))
        for ch in tr.m_Children:
            walk(ch.read())
    walk(root_go.m_Transform.read())
    if not parts:
        print("  no renderers in", out_name)
        return None

    def pose(curves):
        world = skel.world_matrices(curves)
        ps, ns = [], []
        for p in parts:
            if p["kind"] == "skin" and p["bi"] is not None and p["bw"] is not None and p["bones"]:
                v, n = ab.skin(p["v"], p["n"], p["bi"], p["bw"], p["bones"], p["bind"], world)
            else:
                v, n = transform(p["v"], p["n"], world[p["node"]])
            ps.append(v)
            ns.append(n)
        return np.vstack(ps), np.vstack(ns)

    mb = MeshBuilder()
    # geometry/uv/parts from the rest pose, then replace frames with baked ones
    for p in parts:
        mb.add_static(p["v"], p["n"], p["uv"], p["subs"], p["texs"])
    mb.frames = []
    used = set()
    for name, keys, fps, loop, max_frames in anim_specs:
        clip = None
        for cl in clips:
            ln = cl.m_Name.lower()
            if cl.m_Name in used or "menu" in ln or "bored" in ln:
                continue
            if any(k in ln for k in keys):
                clip = cl
                break
        if clip is None:
            continue
        used.add(clip.m_Name)
        sampler = ab.ClipSampler(clip)
        count = max(2, min(max_frames, int(round(sampler.length * fps))))
        step = sampler.length / count if loop else sampler.length / max(1, count - 1)
        first = len(mb.frames)
        for f in range(count):
            mb.frames.append(pose(sampler.sample(min(f * step, sampler.length - 1e-4))))
        mb.anims.append((name, first, count, count / sampler.length if loop else (count - 1) / sampler.length, loop))
    if not mb.frames:
        mb.frames.append(pose(None))
    return mb.write(out_name)


def scan_refs(ctx, o):
    """Resolve every (fileID, pathID) reference in a MonoBehaviour's raw bytes (IL2CPP-safe)."""
    raw = o.get_raw_data()
    af = o.assets_file
    out = []
    for off in range(0, len(raw) - 12, 4):
        fid, pid = struct.unpack_from("<iq", raw, off)
        if not (0 <= fid <= len(af.externals)) or pid == 0 or abs(pid) > 1e12:
            continue
        t = ctx.resolve(af, {"m_FileID": fid, "m_PathID": pid})
        if t is not None:
            out.append(t)
    return out


REUSE_CHARACTERS = "--reuse-characters" in sys.argv

# PassiveData asset -> PassiveId in source/game_types.h
PASSIVE_ENUM = {
    "Rng Blessing": "PASSIVE_RNG", "Speed Demon": "PASSIVE_SPEEDDEMON", "Reinforced": "PASSIVE_REINFORCED",
    "Crit Happens": "PASSIVE_CRIT", "Flex": "PASSIVE_FLEX", "Warrior": "PASSIVE_WARRIOR", "LockIn": "PASSIVE_LOCKIN",
    "Float": "PASSIVE_FLOAT", "Bullseye": "PASSIVE_BULLSEYE", "Flowstate": "PASSIVE_FLOWSTATE",
    "WallClimb": "PASSIVE_MONKEGRIP", "Enduring": "PASSIVE_ENDURING", "Stonks": "PASSIVE_STONKS",
    "Zoombonk": "PASSIVE_ZAP", "Plague": "PASSIVE_PLAGUE", "Quantum": "PASSIVE_QUANTUM",
    "Shadowstep": "PASSIVE_SHADOWSTEP", "Vampire": "PASSIVE_VAMPIRE", "Gamba": "PASSIVE_GAMBA",
    "Curse": "PASSIVE_CURSE", "Hoarder": "PASSIVE_HOARDER",
}


def read_mbm_meta(path):
    _m, nverts, nframes, _np, _na, _ni, _ps, radius, miny, maxy = struct.unpack_from("<4sHHHHIffff", path.read_bytes())
    return dict(radius=radius, miny=miny, maxy=maxy, verts=nverts, frames=nframes)


def export_characters(ctx, env, loc):
    info = {}
    for o in env.objects:
        if o.type.name != "MonoBehaviour":
            continue
        mb = o.read(check_read=False)
        try:
            if mb.m_Script.read().m_Name != "CharacterData" or mb.m_Name not in ROSTER:
                continue
        except Exception:
            continue
        refs = scan_refs(ctx, o)
        portrait = next((r for r in refs if r.type.name == "Texture2D"), None)
        prefab = next((r for r in refs if r.type.name == "GameObject"), None)
        weapon = passive = None
        for r in refs:
            if r.type.name == "MonoBehaviour":
                rm = r.read(check_read=False)
                if rm.m_Name in WEAPON_ENUM and weapon is None:
                    weapon = WEAPON_ENUM[rm.m_Name]
                elif rm.m_Name in PASSIVE_ENUM and passive is None:
                    passive = PASSIVE_ENUM[rm.m_Name]
        cid = safe_name(mb.m_Name)
        key = CHAR_LOC_KEY.get(mb.m_Name, mb.m_Name.upper())
        disp = loc.get("Characters", {}).get(f"{key}_NAME", mb.m_Name)
        if portrait is not None:
            write_texture(ctx, portrait, f"portrait_{cid}", ui_width=64)
            if cid == "fox":
                icon = portrait.read().image.convert("RGBA").resize((48, 48), Image.NEAREST)
                bg = Image.new("RGBA", (48, 48), (40, 30, 60, 255))
                bg.alpha_composite(icon)
                (ROOT / "meta").mkdir(exist_ok=True)
                bg.save(ROOT / "meta" / "icon.png")
        out = ROMFS / "mesh" / f"char_{cid}.mbm"
        if REUSE_CHARACTERS and out.exists():
            meta = read_mbm_meta(out)        # dev shortcut: baking all 21 takes a while
        else:
            meta = bake_animated(ctx, prefab.read(), f"char_{cid}", CHAR_ANIMS) if prefab else None
        if not meta or not weapon:
            print("character skipped:", mb.m_Name, "(no model)" if not meta else "(unknown weapon)")
            continue
        print(f"character {cid} ({disp}, {weapon}, {passive}): verts {meta['verts']}")
        info[mb.m_Name] = (cid, disp, weapon, meta, passive or "PASSIVE_NONE")
    return [info[n] for n in ROSTER if n in info]


def export_shadyguy(ctx, env):
    for o in env.objects:
        if o.type.name == "GameObject" and o.peek_name() == "ShadyGuy":
            go = o.read()
            _, animator = find_animator_root(go)
            if animator is None:
                continue
            meta = bake_animated(ctx, go, "obj_shadyguy", [("idle", ("idle",), 6, True, 72)])
            print("shady guy:", meta)
            return


def export_enemies(ctx, env, stage_objs):
    """Every enemy that can appear in an exported stage (random spawns, minibosses, bosses)."""
    targets = []
    for o in env.objects:
        if o.type.name != "MonoBehaviour":
            continue
        mb = o.read(check_read=False)
        try:
            if mb.m_Script.read().m_Name == "EnemyData":
                targets.append(o)
        except Exception:
            pass
    attach_typetrees(env)
    datas = []
    for o in targets:
        d = o.read_typetree()
        ctx.enemy_enum[d["enemyName"]] = d["m_Name"]   # EEnemy value -> asset name (stage timelines)
        datas.append((o, d))

    # enemies the stage timelines refer to directly
    required, ctx.boss_names, ctx.miniboss_names = set(), set(), set()
    mask = 0
    for sid, o in stage_objs.items():
        mask |= next(b for i, _a, b, *_ in STAGES if i == sid)
        st = o.read_typetree()["stageTimeline"]
        for ev in st["events"]:
            if ev["eTimelineEvent"] == 1:
                ctx.miniboss_names.update(ctx.enemy_enum.get(v) for v in ev["enemies"])
            required.update(ctx.enemy_enum.get(v) for v in ev["enemies"])
        ctx.miniboss_names.update(ctx.enemy_enum.get(v) for v in st.get("minibosses", []))
        boss_o = ctx.resolve(o.assets_file, st["boss"])
        if boss_o is not None:
            ctx.boss_names.add(boss_o.read_typetree()["m_Name"])
    ctx.miniboss_names.discard(None)
    required |= ctx.miniboss_names | ctx.boss_names
    required.discard(None)

    enemies = []
    for o, d in datas:
        name = d["m_Name"]
        if not (d["maps"] & mask or name in required) or name.startswith("_"):
            continue
        af = o.assets_file
        anim_o = ctx.resolve(af, d["animation"])
        mat_o = ctx.resolve(af, d["material"])
        if anim_o is None:
            print("enemy without animation", name)
            continue
        anim = anim_o.read_typetree()
        rot = np.eye(4)
        rot[:3, :3] = euler_to_mat((d["rendererRotationOffset"][k] for k in "xyz"))
        rot[:3, :3] *= d["rendererScale"]
        ro = d["rendererOffset"]
        rot[:3, 3] = (ro["x"], ro["y"], ro["z"])
        tex = material_texture(ctx, mat_o.read()) if mat_o else None
        tint = (1.0, 1.0, 1.0)
        if mat_o:
            cols = dict(mat_o.read().m_SavedProperties.m_Colors)
            if "_Color" in cols:
                c = cols["_Color"]
                tint = (c.r, c.g, c.b)

        mbld = MeshBuilder()
        fps = float(anim.get("AnimationFPS", 8))
        # use the first animation (movement); enemies only have one in practice
        clip = anim["Animations"][0]
        uv0 = None
        for mp in clip["Meshes"]:
            mo = ctx.resolve(anim_o.assets_file, mp)
            if mo is None:
                continue
            v, n, uv, subs = mesh_arrays(mo.read())
            v, n = transform(v, n, rot)
            if uv0 is None:
                uv0 = uv
                mbld.uv = uv
                mbld.parts = [(tex, s) for s in subs]
            elif len(v) != len(uv0):
                continue
            mbld.frames.append((v, n))
        mbld.anims = [("move", 0, len(mbld.frames), fps, True)]
        cname = safe_name(name)
        meta = mbld.write(f"enemy_{cname}")
        print(f"enemy {name}: {meta}")
        enemies.append(dict(id=cname, name=name, d=d, meta=meta, tint=tint))
    return enemies


# EnemyData.specialAttacks[].attackName -> the port's attack pattern (source/attacks.c)
ATTACK_KINDS = {
    "CactusProjectile": "ATK_PROJECTILE",
    "RootAttackContinous": "ATK_CHASE", "GhostRootAttackContinous": "ATK_CHASE",
    "RootAttackLines3": "ATK_LINES3",
    "RootAttackLineSingle": "ATK_LINE", "StoneAttackContinous": "ATK_LINE",
    "RootAttackSinglePredictive": "ATK_PREDICT", "PigMudSplashPredictive": "ATK_PREDICT",
    "ScorpionSpikesPreditctive": "ATK_PREDICT",
    "RootAttackClose": "ATK_SLAM", "StoneAttack": "ATK_SLAM", "StoneAttackClose": "ATK_SLAM",
    "SphereSlam": "ATK_SLAM",
    "PigMudSplash": "ATK_SPLASH",
    "ScorpionSpikesMultiple": "ATK_MULTI", "GhostSpikesMultiple": "ATK_MULTI",
    "BonePillarsPredictiveMultiple": "ATK_MULTI", "DesertPillarsPredictiveMultiple": "ATK_MULTI",
    "BonePillarsCircles": "ATK_CIRCLE", "DesertPillarsCircles": "ATK_CIRCLE",
    "ScorpionSpikesHitscan": "ATK_HITSCAN",
    "LaserTargetAttack": "ATK_LASER", "LaserDiscoAttack": "ATK_DISCO",
}

STAGES = [  # (id, StageData asset, EnemyData.maps bit, fallback display name, music, boss music)
    # the first STAGE_SELECT_COUNT entries are the stage-select choices; the rest are later stages of a run
    ("forest", "StageForest1", 1, "Forest", "MUSIC_FOREST", "MUSIC_FOREST_BOSS"),
    ("desert", "StageDesert1", 2, "Desert", "MUSIC_DESERT", "MUSIC_DESERT_BOSS"),
    ("graveyard", "StageGraveyard", 8, "Graveyard", "MUSIC_GRAVEYARD", "MUSIC_GRAVEYARD_BOSS"),
    ("forest2", "StageForest2", 1, "Forest", "MUSIC_FOREST", "MUSIC_FOREST_BOSS"),
    ("forest3", "StageForest3", 1, "Forest", "MUSIC_FOREST", "MUSIC_FOREST_BOSS"),
    ("desert2", "StageDesert2", 2, "Desert", "MUSIC_DESERT", "MUSIC_DESERT_BOSS"),
    ("desert3", "StageDesert3", 2, "Desert", "MUSIC_DESERT", "MUSIC_DESERT_BOSS"),
]
STAGE_SELECT_COUNT = 3
# a run continues to the next stage after its boss
STAGE_NEXT = {"forest": "forest2", "forest2": "forest3", "desert": "desert2", "desert2": "desert3"}
STAGE_NUMBER = {"forest2": 2, "forest3": 3, "desert2": 2, "desert3": 3}
STAGE_ICONS = {"forest": "ForestIconBig", "desert": "DesertIconBig", "graveyard": "GraveyardIconBig"}


def material_tex(ctx, af, pptr, keys):
    m = ctx.resolve(af, pptr)
    if m is None:
        return None
    envs = dict(m.read().m_SavedProperties.m_TexEnvs)
    for k in keys:
        if k in envs and envs[k].m_Texture.path_id:
            return envs[k].m_Texture
    return None


def export_stages(ctx, env, stage_objs):
    """Props, ground/cliff textures, lighting and timeline for every stage."""
    stages, props, prop_done = [], [], {}
    for sid, _asset, bit, disp, music, boss_music in STAGES:
        if sid not in stage_objs:
            continue
        o = stage_objs[sid]
        af = o.assets_file
        st = o.read_typetree()
        first = len(props)
        for entry in st["randomMapObjects"]:
            for p in entry["prefabs"]:
                go = ctx.resolve(af, p).read()
                name = "prop_" + safe_name(go.m_Name)
                if name not in prop_done:
                    prop_done[name] = export_prefab(ctx, go, name)
                meta = prop_done[name]
                if meta:
                    props.append(dict(name=name, amount=entry["amount"] // len(entry["prefabs"]),
                                      smin=entry["scaleMin"], smax=entry["scaleMax"], meta=meta))
        # ground: first flat material, else the terrain's top texture; cliffs: the terrain's side texture
        if st["flatMaterials"]:
            ground = material_tex(ctx, af, st["flatMaterials"][0], ("_MainTex", "_AlbedoMap"))
        else:
            ground = material_tex(ctx, af, st["triplanarMaterial"], ("_MainTex",))
        cliff = material_tex(ctx, af, st["triplanarMaterial"], ("_MainTexSide", "_MainTex"))
        if ground is not None:
            write_texture(ctx, ground, f"ground_{sid}")
        if cliff is not None:
            write_texture(ctx, cliff, f"cliff_{sid}")
        stages.append(dict(id=sid, bit=bit, name=disp, st=st, af=af, prop_first=first,
                           prop_count=len(props) - first, music=music, boss_music=boss_music))
        print(f"stage {sid}: {len(props) - first} prop kinds")
    for o in env.objects:
        if o.type.name == "Texture2D" and o.peek_name() in STAGE_ICONS.values():
            sid = next(k for k, v in STAGE_ICONS.items() if v == o.peek_name())
            write_texture(ctx, o, f"stage_{sid}", ui_width=128)
    return stages, props


def export_extra(ctx, env):
    wanted = set(EXTRA_PROPS)
    done = set()
    for o in env.objects:
        if o.type.name != "GameObject" or o.peek_name() not in wanted:
            continue
        go = o.read()
        out = EXTRA_PROPS[go.m_Name]
        if out in done:
            continue
        meta = export_prefab(ctx, go, f"obj_{out}")
        if meta:
            done.add(out)
            print(f"object {out}: {meta}")
    for o in env.objects:
        if o.type.name == "Texture2D" and o.peek_name() in EXTRA_TEXTURES:
            write_texture(ctx, o, EXTRA_TEXTURES[o.peek_name()])


# --------------------------------------------------------------------------- C tables

def c_str(s):
    return json.dumps(s)


def write_tables(ctx, chars, enemies, stages, props):
    GEN.mkdir(parents=True, exist_ok=True)
    L = ["// Generated by tools/extract_assets.py -- do not edit.", "#pragma once", '#include "../game_types.h"', ""]

    L.append(f"#define CHARACTER_COUNT {len(chars)}")
    L.append("static const CharacterDef g_characters[CHARACTER_COUNT] = {")
    for cid, disp, weapon, meta, passive in chars:
        L.append(f'\t{{ {c_str(disp)}, "romfs:/mesh/char_{cid}.mbm", {weapon}, {meta["maxy"]:.3f}f, '
                 f'"portrait_{cid}", {passive} }},')
    L.append("};\n")

    # special attacks, referenced from EnemyDef by index range
    attacks = []
    for e in enemies:
        e["atk_first"] = len(attacks)
        for a in e["d"].get("specialAttacks", []):
            kind = ATTACK_KINDS.get(a["attackName"])
            if a["isEnabled"] and kind:
                attacks.append((kind, a))
            elif a["isEnabled"]:
                print("unmapped enemy attack", e["name"], a["attackName"])
        e["atk_count"] = min(len(attacks) - e["atk_first"], 5)
    L.append(f"#define ENEMY_ATTACK_COUNT {max(len(attacks), 1)}")
    L.append("// kind, chargeTime, radius, cooldown, cooldownMax, initialCooldown, duration, triggerDistance, endLag, damageMult")
    L.append("static const EnemyAttack g_enemyAttacks[ENEMY_ATTACK_COUNT] = {")
    for kind, a in attacks:
        L.append(f'\t{{ {kind}, {a["attackChargeTime"]:.3f}f, {a["attackRadius"]:.3f}f, {a["attackCooldown"]:.2f}f, '
                 f'{a["attackCooldownMax"]:.2f}f, {a["initialCooldown"]:.2f}f, {a["duration"]:.2f}f, '
                 f'{min(a["triggerDistance"], 9999):.1f}f, {a["endLag"]:.3f}f, {a["damageMultiplier"]:.3f}f }},')
    if not attacks:
        L.append("\t{ 0 },")
    L.append("};\n")

    L.append(f"#define ENEMY_COUNT {len(enemies)}")
    L.append("static const EnemyDef g_enemyDefs[ENEMY_COUNT] = {")
    for e in enemies:
        d = e["d"]
        flags = []
        if d["isFlying"]:
            flags.append("ENEMY_FLYING")
        if e["name"] in ctx.miniboss_names or e["name"].startswith("Miniboss"):
            flags.append("ENEMY_MINIBOSS")
        if e["name"] in ctx.boss_names:
            flags.append("ENEMY_BOSS")
        if not d["maps"]:
            flags.append("ENEMY_NO_RANDOM_SPAWN")
        fl = "|".join(flags) or "0"
        t = e["tint"]
        # the collider lives on the scaled renderer transform in Unity
        L.append(
            f'\t{{ {c_str(e["name"])}, "romfs:/mesh/enemy_{e["id"]}.mbm", {d["hp"]}, {d["damage"]}, '
            f'{d["speed"]:.3f}f, {d["xp"]}, {d["colliderRadius"] * d["rendererScale"]:.3f}f, {d["mass"]:.2f}f, '
            f'{d["creditCost"]:.3f}f, {d["canSpawnAfterTime"]:.2f}f, {fl}, '
            f'{{ {t[0]:.3f}f, {t[1]:.3f}f, {t[2]:.3f}f }}, {d["maps"]}, {e["atk_first"]}, {e["atk_count"]} }},')
    L.append("};\n")

    L.append(f"#define PROP_COUNT {len(props)}")
    L.append("static const PropDef g_propDefs[PROP_COUNT] = {")
    for p in props:
        L.append(f'\t{{ "romfs:/mesh/{p["name"]}.mbm", {p["amount"]}, {p["smin"]:.3f}f, {p["smax"]:.3f}f, '
                 f'{p["meta"]["radius"]:.3f}f }},')
    L.append("};\n")

    # Stages. Timeline enemies are EEnemy values, mapped to our table.
    idx = {e["name"]: i for i, e in enumerate(enemies)}

    def ids(lst):
        return [idx[ctx.enemy_enum[v]] for v in lst if ctx.enemy_enum.get(v) in idx]

    def rgb(c):
        return f"{{ {c['r']:.3f}f, {c['g']:.3f}f, {c['b']:.3f}f }}"

    def arr4(v):
        v = list(v)[:4]
        return "{ " + ", ".join(map(str, v + [0] * (4 - len(v)))) + " }"

    for s in stages:
        tl = s["st"]["stageTimeline"]
        L.append(f"static const StageEvent g_events_{s['id']}[] = {{")
        for ev in tl["events"]:
            e = ids(ev["enemies"])
            L.append(f"\t{{ {ev['eTimelineEvent']}, {ev['timeMinutes']:.2f}f, {ev['duration']:.1f}f, "
                     f"{arr4(e)}, {min(len(e), 4)} }},")
        L.append("};")
    order = [s["id"] for s in stages]
    L.append(f"#define STAGE_COUNT {len(stages)}")
    L.append(f"#define STAGE_SELECT_COUNT {min(STAGE_SELECT_COUNT, len(stages))}")
    L.append("static const StageDef g_stages[STAGE_COUNT] = {")
    for s in stages:
        st, tl = s["st"], s["st"]["stageTimeline"]
        start = ids(tl["startEnemies"])
        minis = ids(tl.get("minibosses", []))
        boss_o = ctx.resolve(s["af"], tl["boss"])
        boss = idx.get(boss_o.read_typetree()["m_Name"], -1) if boss_o is not None else -1
        icon = next((k for k in STAGE_ICONS if s["id"].startswith(k)), s["id"])
        L.append(f'\t{{ {c_str(s["name"])}, "stage_{icon}", {s["bit"]}, {tl["stageTime"]:.1f}f, '
                 f'{tl["checkNewEnemyInterval"]:.1f}f, {rgb(st["fogColor"])}, {rgb(st["ambienceColor"])}, '
                 f'{rgb(st["lightColor"])}, "ground_{s["id"]}", "cliff_{s["id"]}", {arr4(start)}, '
                 f'{min(len(start), 4)}, g_events_{s["id"]}, {len(tl["events"])}, {arr4(minis)}, '
                 f'{min(len(minis), 4)}, {boss}, {s["prop_first"]}, {s["prop_count"]}, '
                 f'{s["music"]}, {s["boss_music"]}, '
                 f'{order.index(STAGE_NEXT[s["id"]]) if STAGE_NEXT.get(s["id"]) in order else -1}, '
                 f'{STAGE_NUMBER.get(s["id"], 1)} }},')
    L.append("};")
    (GEN / "assets_gen.h").write_text("\n".join(L) + "\n")


def export_icons(ctx, env):
    """Weapon / tome / item icons -> romfs/tex/icon_<name>.tex (kept at native pixel size)."""
    n_out = 0
    for o in env.objects:
        if o.type.name != "Texture2D":
            continue
        n = o.peek_name()
        if not (n.startswith("Item") or n.endswith("Tome") or n in WEAPON_ICONS or n in MISC_ICONS):
            continue
        t = o.read()
        if t.m_Width > 64 or t.m_Width != t.m_Height:   # skip same-named model textures
            continue
        name = "icon_" + safe_name(n)
        if name in ctx.textures_written:
            continue
        write_texture(ctx, t, name, ui_width=t.m_Width)
        n_out += 1
    print(f"icons: {n_out}")


LOC_TABLES = ("Game_ShadyGuy", "Game_Moai", "Characters", "Weapons")


def load_loc():
    """English localization tables -> {table: {key: text}}."""
    import UnityPy
    from unity_env import DATA_DIR
    aa = DATA_DIR / "StreamingAssets" / "aa" / "StandaloneWindows64"
    env = UnityPy.load(str(aa / "localization-string-tables-english(en)_assets_all.bundle"),
                       str(aa / "localization-assets-shared_assets_all.bundle"))
    wanted = {f"{t}_en" for t in LOC_TABLES} | {f"{t} Shared Data" for t in LOC_TABLES}
    objs = []
    for o in env.objects:
        if o.type.name == "MonoBehaviour":
            mb = o.read(check_read=False)
            if mb.m_Name in wanted:
                objs.append((mb.m_Name, o))
    attach_typetrees(env)
    trees = {n: o.read_typetree() for n, o in objs}
    out = {}
    for name in LOC_TABLES:
        keys = {e["m_Id"]: e["m_Key"] for e in trees[f"{name} Shared Data"]["m_Entries"]}
        out[name] = {keys.get(e["m_Id"], ""): e["m_Localized"] for e in trees[f"{name}_en"]["m_TableData"]}
    return out


def export_strings(loc):
    """Flavor text from the English localization tables -> source/gen/strings_gen.h."""
    def table(name):
        return loc[name]

    def c_ascii(s):
        s = s.replace("’", "'").replace("�", "'")   # curly/garbled apostrophes
        s = s.encode("ascii", "ignore").decode()                # 3DS font: ASCII only
        return json.dumps(s)
    shady = [v for k, v in sorted(table("Game_ShadyGuy").items(), key=lambda kv: int(kv[0].split("_")[-1]))]
    moai = table("Game_Moai")
    L = ["// Generated by tools/extract_assets.py -- do not edit.", "#pragma once", ""]
    L.append(f"#define SHADY_LINE_COUNT {len(shady)}")
    L.append("static const char* const g_shadyLines[] = {")
    L += [f"\t{c_ascii(s)}," for s in shady]
    L.append("};")
    L.append("// [judgement 0..3][variant 0..2]")
    L.append("static const char* const g_moaiLines[4][3] = {")
    for j in range(4):
        L.append("\t{ " + ", ".join(c_ascii(moai.get(f"MOAI_{j}_{v}", "")) for v in range(3)) + " },")
    L.append("};")
    (GEN / "strings_gen.h").write_text("\n".join(L) + "\n", encoding="ascii")
    print("strings ok")


def export_ui(ctx, env):
    """The logo (portraits and the app icon come from export_characters)."""
    for o in env.objects:
        if o.type.name == "Texture2D" and o.peek_name() in UI_IMAGES:
            out, width = UI_IMAGES[o.peek_name()]
            write_texture(ctx, o, out, ui_width=width)
    print("ui images ok")


def export_font(env):
    """alagard (the game's title/UI pixel font) -> romfs/font/alagard.bcfnt via mkbcfnt."""
    import subprocess
    import tempfile
    mk = dkp_tool("mkbcfnt")
    for o in env.objects:
        if o.type.name == "Font" and o.peek_name() == "alagard":
            data = bytes(o.read().m_FontData)
            with tempfile.TemporaryDirectory() as td:
                ttf = Path(td) / "alagard.ttf"
                ttf.write_bytes(data)
                wl = Path(td) / "ascii.txt"
                wl.write_text(" ".join(str(c) for c in range(32, 127)))
                out = ROMFS / "font" / "alagard.bcfnt"
                out.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run([str(mk), "-s", "16", "-w", str(wl), "-o", str(out), str(ttf)], check=True)
            print("font alagard ok")
            return


def main():
    loc = load_loc()
    env = load_env()
    ctx = Ctx(env)
    export_font(env)
    export_ui(ctx, env)
    export_icons(ctx, env)
    chars = export_characters(ctx, env, loc)
    if not (REUSE_CHARACTERS and (ROMFS / "mesh" / "obj_shadyguy.mbm").exists()):
        export_shadyguy(ctx, env)
    export_extra(ctx, env)
    # must be located before typetrees are attached (peek_name breaks afterwards)
    assets = {a: sid for sid, a, *_ in STAGES}
    stage_objs = {assets[o.peek_name()]: o for o in env.objects
                  if o.type.name == "MonoBehaviour" and o.peek_name() in assets}
    enemies = export_enemies(ctx, env, stage_objs)   # attaches typetrees
    stages, props = export_stages(ctx, env, stage_objs)
    write_tables(ctx, chars, enemies, stages, props)
    export_strings(loc)
    print(f"textures: {len(ctx.textures_written)}")


if __name__ == "__main__":
    main()
