"""Build megabonk3ds.3dsx using the local toolchain (replaces the usual devkitPro Makefile).

    .venv\\Scripts\\python tools\\build.py           # incremental build
    .venv\\Scripts\\python tools\\build.py clean
    .venv\\Scripts\\python tools\\build.py debug=170   # dev build: run clock starts at 170s, huge HP;
                                                       # written to build/debug/, never dist/
    ... debug=60 calm spawn=Miniboss_Golem               # dev: no spawns, one enemy that keeps coming back

Output: dist/3ds/megabonk3ds/megabonk3ds.3dsx  (romfs is embedded; copy this folder
to the root of the SD card so it ends up at sdmc:/3ds/megabonk3ds/).
"""
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# DEVKITPRO may point at any install; default is the project-local one from tools/setup_toolchain.py
DKP = Path(os.environ.get("DEVKITPRO", ROOT / "toolchain" / "opt" / "devkitpro"))
EXE = ".exe" if os.name == "nt" else ""
BIN = DKP / "devkitARM" / "bin"
TOOLS = DKP / "tools" / "bin"
CTRU = DKP / "libctru"
BUILD = ROOT / "build"
DIST = ROOT / "dist" / "3ds" / "megabonk3ds"
APP = "megabonk3ds"

ARCH = ["-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft"]
CFLAGS = ["-g", "-Wall", "-Wno-missing-braces", "-O2", "-mword-relocations", "-ffunction-sections",
          "-ffast-math", "-std=gnu11", *ARCH, "-D__3DS__", f"-I{CTRU / 'include'}", f"-I{ROOT / 'source'}"]
LDFLAGS = ["-specs=3dsx.specs", "-g", *ARCH, f"-Wl,-Map,{BUILD / (APP + '.map')}"]
LIBS = [f"-L{CTRU / 'lib'}", "-lcitro2d", "-lcitro3d", "-lctru", "-lm"]


def run(cmd):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        sys.stdout.write(r.stdout + r.stderr)
    if r.returncode != 0:
        raise SystemExit(f"command failed: {Path(str(cmd[0])).name}")


def newer(src, dst, extra=()):
    if not dst.exists():
        return True
    t = dst.stat().st_mtime
    return src.stat().st_mtime > t or any(p.stat().st_mtime > t for p in extra)


def main():
    global BUILD, DIST
    debug = next((a.split("=", 1)[1] for a in sys.argv[1:] if a.startswith("debug=")), None)
    if debug is not None:
        BUILD = ROOT / "build" / "debug"
        DIST = BUILD
        CFLAGS.append(f"-DMB_DEBUG_START_TIME={float(debug)}f")
        if "calm" in sys.argv[1:]:
            CFLAGS.append("-DMB_DEBUG_CALM")   # no enemy spawns
        spawn = next((a.split("=", 1)[1] for a in sys.argv[1:] if a.startswith("spawn=")), None)
        if spawn:
            CFLAGS.append(f'-DMB_DEBUG_SPAWN="{spawn}"')   # this enemy keeps respawning next to you
        LDFLAGS[-1] = f"-Wl,-Map,{BUILD / (APP + '.map')}"
    if len(sys.argv) > 1 and sys.argv[1] == "clean":
        shutil.rmtree(BUILD, ignore_errors=True)
        shutil.rmtree(ROOT / "dist", ignore_errors=True)
        return
    os.environ["PATH"] = f"{BIN}{os.pathsep}{TOOLS}{os.pathsep}" + os.environ["PATH"]
    os.environ["DEVKITPRO"] = str(DKP)
    os.environ["DEVKITARM"] = str(DKP / "devkitARM")
    (BUILD / "obj").mkdir(parents=True, exist_ok=True)

    # shaders -> romfs/shaders/*.shbin
    shader_out = ROOT / "romfs" / "shaders"
    shader_out.mkdir(parents=True, exist_ok=True)
    for sh in (ROOT / "shaders").glob("*.v.pica"):
        out = shader_out / (sh.name[:-len(".v.pica")] + ".shbin")
        if newer(sh, out):
            print("shader", sh.name)
            run([TOOLS / f"picasso{EXE}", "-o", out, sh])

    # rebuild everything when compiler flags change (e.g. a different debug= value)
    flag_file = BUILD / "cflags.txt"
    flags = " ".join(CFLAGS)
    if not flag_file.exists() or flag_file.read_text() != flags:
        for o in (BUILD / "obj").glob("*.o"):
            o.unlink()
        flag_file.write_text(flags)

    headers = list((ROOT / "source").rglob("*.h"))
    sources = list((ROOT / "source").rglob("*.c"))
    objs = []
    jobs = []
    for src in sources:
        obj = BUILD / "obj" / (src.relative_to(ROOT / "source").as_posix().replace("/", "_") + ".o")
        objs.append(obj)
        if newer(src, obj, headers):
            jobs.append((src, obj))
    with ThreadPoolExecutor() as ex:
        def cc(job):
            src, obj = job
            print("cc", src.relative_to(ROOT).as_posix())
            run([BIN / f"arm-none-eabi-gcc{EXE}", *CFLAGS, "-c", src, "-o", obj])
        list(ex.map(cc, jobs))

    elf = BUILD / f"{APP}.elf"
    if jobs or not elf.exists():
        print("link", elf.name)
        run([BIN / f"arm-none-eabi-gcc{EXE}", *LDFLAGS, *objs, *LIBS, "-o", elf])

    icon = ROOT / "meta" / "icon.png"
    if not icon.exists():
        icon = CTRU / "default_icon.png"
    smdh = BUILD / f"{APP}.smdh"
    run([TOOLS / f"smdhtool{EXE}", "--create", "Extreme Megabonk 3D", "Megabonk for the 3ds",
         "megabonk-3ds", icon, smdh])
    DIST.mkdir(parents=True, exist_ok=True)
    out = DIST / f"{APP}.3dsx"
    run([TOOLS / f"3dsxtool{EXE}", elf, out, f"--smdh={smdh}", f"--romfs={ROOT / 'romfs'}"])
    print(f"built {out.relative_to(ROOT)} ({out.stat().st_size / 1e6:.1f} MB)")

    # Dev tree only: keep the GitHub repo folder in sync with every build.
    # (export_repo.py isn't part of the repo, so this does nothing when the builder runs there.)
    export = ROOT / "tools" / "export_repo.py"
    if export.exists() and (ROOT / "Megabonk 3ds" / ".git").exists():
        run([sys.executable, export])
        print("synced the repo folder (Megabonk 3ds)")


if __name__ == "__main__":
    main()
