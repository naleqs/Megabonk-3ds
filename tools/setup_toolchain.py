"""Install a self-contained devkitARM + libctru + citro3d toolchain.

Downloads packages straight from the devkitPro pacman repos and unpacks them,
so nothing is installed system-wide. Re-running is safe (already-installed
packages are skipped). Works on Windows, Linux and macOS hosts.

    python tools/setup_toolchain.py [destination]     (default: ./toolchain)

The toolchain ends up in <destination>/opt/devkitpro (point DEVKITPRO there).
"""
import io
import os
import platform
import shutil
import sys
import tarfile
import urllib.request
from pathlib import Path

import zstandard

ROOT = Path(__file__).resolve().parent.parent
DEST = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "toolchain"   # holds opt/devkitpro/...
CACHE = DEST / ".pkgcache"


def host_repo():
    """devkitPro repo with host tools (compiler etc.) for this OS/CPU."""
    machine = platform.machine().lower()
    if sys.platform == "win32":
        return "dkp-windows", "windows/x86_64"
    arch = "aarch64" if machine in ("aarch64", "arm64") else "x86_64"
    if sys.platform == "darwin":
        return "dkp-osx", "osx/" + ("arm64" if arch == "aarch64" else "x86_64")
    return "dkp-linux", "linux/" + arch


HOST_DB, HOST_PATH = host_repo()
UA = "pacman/6.0.2 (MINGW64_NT-10.0 x86_64) libalpm/13.0.2"   # the repo rejects unknown user agents
REPOS = {
    HOST_DB: f"https://pkg.devkitpro.org/packages/{HOST_PATH}/",
    "dkp-libs": "https://pkg.devkitpro.org/packages/",
}
WANTED = [
    "devkitarm-gcc", "devkitarm-binutils", "devkitarm-newlib", "devkitarm-crtls",
    "devkitarm-rules", "general-tools", "3dstools", "tex3ds", "picasso", "3dslink",
    "libctru", "citro3d", "citro2d",
    "devkitarm-cmake", "3ds-cmake", "dkp-cmake-common-utils", "3ds-pkg-config",
    "dkp-toolchain-vars",
]
# Virtual/provided names that we satisfy ourselves.
IGNORE = {"devkitARM", "devkitpro-keyring", "pkg-config", "cmake", "devkitpro-pkgbuild-helpers"}


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req) as r:
        return r.read()


def parse_db(data: bytes) -> dict:
    pkgs = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as t:
        for m in t.getmembers():
            if not m.name.endswith("/desc"):
                continue
            sec, cur = {}, None
            for line in t.extractfile(m).read().decode().splitlines():
                if line.startswith("%") and line.endswith("%"):
                    cur = line.strip("%")
                    sec[cur] = []
                elif line:
                    sec[cur].append(line)
            pkgs[sec["NAME"][0]] = sec
    return pkgs


def main() -> None:
    CACHE.mkdir(parents=True, exist_ok=True)
    index = {}
    for repo, base in REPOS.items():
        for name, sec in parse_db(fetch(base + repo + ".db")).items():
            index[name] = (base, sec)
            for prov in sec.get("PROVIDES", []):
                index.setdefault(prov.split("=")[0], (base, sec))

    todo, seen = list(WANTED), set()
    while todo:
        name = todo.pop()
        for op in (">=", "<=", "=", ">", "<"):
            name = name.split(op)[0]
        if name in seen or name in IGNORE:
            continue
        if name not in index:
            sys.exit(f"package not found: {name}")
        seen.add(name)
        todo.extend(index[name][1].get("DEPENDS", []))

    for name in sorted(seen):
        base, sec = index[name]
        fn = sec["FILENAME"][0]
        marker = CACHE / (fn + ".done")
        if marker.exists():
            continue
        print("installing", fn)
        data = fetch(base + fn)
        if fn.endswith(".zst"):
            raw, mode = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)), "r|"
        else:
            raw, mode = io.BytesIO(data), "r|*"
        links = []
        with tarfile.open(fileobj=raw, mode=mode) as t:
            for m in t:
                if m.name.startswith("."):  # .PKGINFO, .MTREE, .INSTALL
                    continue
                if m.issym() or m.islnk():
                    # materialise links as copies after extraction (works without symlink rights on Windows)
                    links.append(m)
                    continue
                if os.path.isabs(m.name) or ".." in Path(m.name).parts:
                    continue   # never write outside DEST
                try:
                    t.extract(m, DEST, filter="data")
                except TypeError:          # Python < 3.11.4: no extraction filters
                    t.extract(m, DEST)
        for m in links:
            dst = DEST / m.name
            if m.islnk():
                src = DEST / m.linkname
            else:
                src = (dst.parent / m.linkname).resolve()
            if src.is_file() and DEST in src.parents:
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
            else:
                print("  skipped link", m.name, "->", m.linkname)
        marker.write_text("")
    print("toolchain ready at", DEST / "opt" / "devkitpro")


if __name__ == "__main__":
    main()
