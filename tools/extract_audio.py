"""Convert selected Megabonk AudioClips to raw PCM16 mono for ndsp.

    .venv\\Scripts\\python tools\\extract_audio.py

Writes romfs/sfx/<id>.snd and romfs/music/<id>.snd:
    char magic[4] = "SND1"; u32 sampleRate; u32 numSamples; s16 samples[numSamples]
The ids must match the tables in source/audio.c.
"""
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from unity_env import load_env  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
ROMFS = ROOT / "romfs"

SFX = {
    "hit": "Hit1", "enemy_die": "retro_poof2", "player_hurt": "PlayerDamage", "xp": "Xp2",
    "levelup": "Levelup3", "jump": "Player_Jump", "fireball": "retro_shoot_fireball3",
    "explosion": "retro_explosion_fireball1", "throw": "BoneThrow", "sword": "SwordSwing",
    "gun": "Revolver", "lightning": "retro_lightningstrike", "heal": "HeartPickup",
    "chest": "ChestTake", "menu": "Select2", "select": "MenuClick",
    "pot": "PotBreak", "shrine": "shrine_start", "swarm": "SwarmApproaching", "boss": "BossSpawned",
    "purchase": "ItemPurchased", "gold": "gold",
}
# Graveyard has no stage track of its own in the game data (only a boss track); it uses an "Other" track.
MUSIC = {"menu": "Menu", "forest": "Forest1_Loop", "forest_boss": "ForestBoss_Loop",
         "desert": "Desert1_Loop", "desert_boss": "DesertBoss_Loop",
         "graveyard": "Other1_Loop", "graveyard_boss": "GraveyardBoss_Loop"}
SFX_RATE = 22050
MUSIC_RATE = 22050


def parse_wav(data: bytes):
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body)
        elif cid == b"data":
            raw = body
        pos += 8 + size + (size & 1)
    tag, ch, rate, _, _, bits = fmt
    if tag == 3 or bits == 32:
        a = np.frombuffer(raw, "<f4").astype(np.float32)
    elif bits == 16:
        a = np.frombuffer(raw, "<i2").astype(np.float32) / 32768.0
    elif bits == 8:
        a = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128.0
    else:
        raise ValueError(f"unsupported wav: tag={tag} bits={bits}")
    a = a[: len(a) // ch * ch].reshape(-1, ch).mean(axis=1)
    return a, rate


def resample(a, src, dst):
    if src == dst:
        return a
    n = int(len(a) * dst / src)
    # simple low-pass (box) before decimating to limit aliasing
    k = max(1, int(round(src / dst)))
    if k > 1:
        a = np.convolve(a, np.ones(k) / k, mode="same")
    return np.interp(np.linspace(0, len(a) - 1, n), np.arange(len(a)), a)


def write_snd(path, a, rate, gain=1.0):
    a = np.clip(a * gain, -1, 1)
    pcm = (a * 32767).astype("<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<4sII", b"SND1", rate, len(pcm)) + pcm.tobytes())


IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]
IMA_STEP = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767]


def write_snd_adpcm(path, a, rate, gain=1.0):
    """Music: 4-bit IMA ADPCM (a quarter the size of PCM16), decoded on the 3DS while streaming.

    SND2: magic, rate, samples, then samples/2 bytes (low nibble first). Decoding starts from
    predictor 0 / step index 0, so the stream can simply be restarted to loop.
    """
    pcm = (np.clip(a * gain, -1, 1) * 32767).astype(np.int32).tolist()
    pred, index = 0, 0
    out = bytearray((len(pcm) + 1) // 2)
    for i, s in enumerate(pcm):
        step = IMA_STEP[index]
        diff = s - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        delta = step >> 3
        if diff >= step:
            code |= 4; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 2; diff -= step; delta += step
        step >>= 1
        if diff >= step:
            code |= 1; delta += step
        pred = max(-32768, min(32767, pred - delta if code & 8 else pred + delta))
        index = max(0, min(88, index + IMA_INDEX[code & 7]))
        if i & 1:
            out[i >> 1] |= code << 4
        else:
            out[i >> 1] = code
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<4sII", b"SND2", rate, len(pcm)) + bytes(out))


def main():
    env = load_env()
    wanted = {v: ("sfx", k) for k, v in SFX.items()}
    wanted.update({v: ("music", k) for k, v in MUSIC.items()})
    done = set()
    for o in env.objects:
        if o.type.name != "AudioClip":
            continue
        name = o.peek_name()
        if name not in wanted or name in done:
            continue
        kind, out = wanted[name]
        clip = o.read()
        samples = clip.samples
        if not samples:
            continue
        a, rate = parse_wav(next(iter(samples.values())))
        dst = SFX_RATE if kind == "sfx" else MUSIC_RATE
        a = resample(a, rate, dst)
        if kind == "sfx":
            a = a[: dst * 2]  # cap one-shots at 2s
        if kind == "music":
            write_snd_adpcm(ROMFS / kind / f"{out}.snd", a, dst, 0.9)
        else:
            write_snd(ROMFS / kind / f"{out}.snd", a, dst)
        done.add(name)
        print(f"{kind} {out} <- {name} ({len(a) / dst:.1f}s)")
    missing = set(wanted) - done
    if missing:
        print("missing:", ", ".join(sorted(missing)))


if __name__ == "__main__":
    main()
