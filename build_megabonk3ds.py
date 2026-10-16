#!/usr/bin/env python3
"""Build Megabonk 3DS from your own copy of Megabonk.

    python build_megabonk3ds.py                 # finds Megabonk through Steam
    python build_megabonk3ds.py --game-dir "D:/Games/Megabonk"

What it does:
  1. finds your Megabonk install (the folder containing Megabonk_Data)
  2. creates a temporary build environment: a Python venv with the extraction
     libraries and the devkitARM/libctru/citro3d toolchain (downloaded from devkitPro)
  3. converts the game's models, textures, animations, audio and data for the 3DS
  4. compiles the game into output/3ds/megabonk3ds/megabonk3ds.3dsx
  5. deletes the temporary environment again (use --keep-env to keep it for debugging)

Nothing is installed system-wide. Needs Python 3.9+ and an internet connection
(for the Python packages and the devkitPro toolchain).
"""
import argparse
import os
import shutil
import stat
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUTPUT = HERE / "output"
REQUIREMENTS = [
    "UnityPy==1.25.4",
    "TypeTreeGeneratorAPI==0.0.10",
    "zstandard==0.25.0",
    "Pillow==12.3.0",
    "numpy==2.4.6",
]
# what gets copied into the temporary work tree (generated files land there, not in the repo)
COPY_DIRS = ["source", "shaders", "tools"]


def log(msg):
    print(f"[megabonk3ds] {msg}", flush=True)


# --------------------------------------------------------------------------- finding the game

def is_game_dir(p: Path) -> bool:
    return (p / "Megabonk_Data").is_dir() and (p / "GameAssembly.dll").is_file()


def steam_roots():
    roots = []
    if sys.platform == "win32":
        try:
            import winreg
            for hive, key in ((winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam"),
                              (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam")):
                try:
                    with winreg.OpenKey(hive, key) as k:
                        for name in ("SteamPath", "InstallPath"):
                            try:
                                roots.append(Path(winreg.QueryValueEx(k, name)[0]))
                            except OSError:
                                pass
                except OSError:
                    pass
        except ImportError:
            pass
        roots += [Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Steam"]
    else:
        home = Path.home()
        roots += [home / ".steam" / "steam", home / ".local" / "share" / "Steam",
                  home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam",
                  home / "Library" / "Application Support" / "Steam"]
    return [r for r in roots if r.is_dir()]


def steam_libraries(root: Path):
    libs = [root]
    vdf = root / "steamapps" / "libraryfolders.vdf"
    if vdf.is_file():
        for line in vdf.read_text(encoding="utf-8", errors="ignore").splitlines():
            parts = line.strip().split('"')
            if len(parts) >= 4 and parts[1] == "path":
                libs.append(Path(parts[3].replace("\\\\", "\\")))
    return libs


def find_game(explicit):
    if explicit:
        p = Path(explicit).expanduser()
        if is_game_dir(p):
            return p
        sys.exit(f"'{p}' doesn't look like a Megabonk install (expected Megabonk_Data and GameAssembly.dll).")
    if os.environ.get("MEGABONK_DIR") and is_game_dir(Path(os.environ["MEGABONK_DIR"])):
        return Path(os.environ["MEGABONK_DIR"])
    for root in steam_roots():
        for lib in steam_libraries(root):
            p = lib / "steamapps" / "common" / "Megabonk"
            if is_game_dir(p):
                return p
    sys.exit("Couldn't find Megabonk. Install it through Steam, or pass --game-dir <folder with Megabonk_Data>.")


# --------------------------------------------------------------------------- environment

def run(cmd, env=None, cwd=None):
    log("$ " + " ".join(str(c) for c in cmd))
    r = subprocess.run([str(c) for c in cmd], env=env, cwd=cwd)
    if r.returncode != 0:
        raise SystemExit(f"step failed (exit {r.returncode}): {Path(str(cmd[0])).name} {cmd[1] if len(cmd) > 1 else ''}")


def remove_tree(path: Path):
    """rmtree that also handles read-only files (Windows) and briefly-locked files."""
    def make_writable(func, p, *_):
        try:
            os.chmod(p, stat.S_IWRITE)
            func(p)
        except OSError:
            pass
    for attempt in range(3):
        if not path.exists():
            return
        if sys.version_info >= (3, 12):
            shutil.rmtree(path, onexc=make_writable)
        else:
            shutil.rmtree(path, onerror=make_writable)
        if path.exists():
            time.sleep(1)
    if path.exists():
        log(f"warning: couldn't fully delete {path}; you can remove it by hand")


def venv_python(venv: Path) -> Path:
    return venv / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game-dir", help="folder containing Megabonk_Data (default: find it through Steam)")
    ap.add_argument("--keep-env", action="store_true", help="don't delete the temporary build environment")
    args = ap.parse_args()

    if sys.version_info < (3, 9):
        sys.exit("Python 3.9 or newer is required.")
    game = find_game(args.game_dir)
    log(f"using Megabonk at {game}")

    # the temporary environment lives next to this script (nothing is written elsewhere)
    env_dir = HERE / ".build-env"
    if env_dir.exists():
        remove_tree(env_dir)          # leftovers from an interrupted run
    env_dir.mkdir()
    log(f"temporary environment: {env_dir}")
    try:
        work = env_dir / "work"
        for d in COPY_DIRS:
            shutil.copytree(HERE / d, work / d, ignore=shutil.ignore_patterns("gen", "__pycache__"))

        # 1. Python environment
        venv = env_dir / "venv"
        run([sys.executable, "-m", "venv", venv])
        py = venv_python(venv)
        run([py, "-m", "pip", "install", "--quiet", "--disable-pip-version-check", *REQUIREMENTS])

        # 2. devkitPro toolchain (downloaded packages, unpacked into the environment)
        toolchain = env_dir / "toolchain"
        run([py, work / "tools" / "setup_toolchain.py", toolchain])

        env = dict(os.environ)
        env["MEGABONK_DIR"] = str(game)
        env["DEVKITPRO"] = str(toolchain / "opt" / "devkitpro")
        env["PYTHONUTF8"] = "1"

        # 3. convert the game's assets, 4. compile
        run([py, work / "tools" / "extract_assets.py"], env=env, cwd=work)
        run([py, work / "tools" / "extract_audio.py"], env=env, cwd=work)
        run([py, work / "tools" / "build.py"], env=env, cwd=work)

        built = work / "dist" / "3ds" / "megabonk3ds" / "megabonk3ds.3dsx"
        dest = OUTPUT / "3ds" / "megabonk3ds"
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copy2(built, dest / built.name)
        log(f"done: {dest / built.name} ({built.stat().st_size / 1e6:.1f} MB)")
        log("copy the 'output/3ds' folder to the root of your SD card, then start it from the Homebrew Launcher.")
    finally:
        if args.keep_env:
            log(f"kept the build environment at {env_dir}")
        else:
            log("deleting the temporary build environment")
            remove_tree(env_dir)


if __name__ == "__main__":
    main()
