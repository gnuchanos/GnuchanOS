#!/usr/bin/env python3
"""
GCL build chain (C version).

Usage:
  python makefile.py        # single command: dependencies + build (no arguments)

Build output (simple_doc.md):
  build/<os>/
      gcl[.exe]
      Programs/
        Ide.dll|.so
      Library/
          Math.dll|.so
          Stdio.dll|.so
          Embed.dll|.so
          Raylib.dll|.so
          Raygui.dll|.so
          Embeded/
              Lua_Runtime/
                  lua.dll|.so
                  LuaRaylib.dll|.so
                  LuaRaygui.dll|.so
              Python_Runtime/
                  Python.dll|.so
                  PyRaylib.dll|.so
                  PyRaygui.dll|.so
                  Python/
      Assets/
"""

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')
except Exception:
    pass

ROOT = Path(__file__).resolve().parent                 # language/
REPO_ROOT = ROOT.parent                                 # d:/GnuchanOS
BUILD_ROOT = ROOT / "build"                            # language/build
TEMP_ROOT = REPO_ROOT / "_temp"


def os_name() -> str:
    name = platform.system().lower()
    if name == "linux":
        return "gnuLinux"
    return name


def _plat_temp() -> Path:
    """_temp/<os>/ — dependencies are stored separately per platform."""
    d = TEMP_ROOT / os_name()
    d.mkdir(parents=True, exist_ok=True)
    return d


PYTHON_DIR = _plat_temp() / "Python"
LUA_DIR = _plat_temp() / "Lua"
LUA_SRC = LUA_DIR
RAYLIB_DIR = _plat_temp() / "Raylib"
RAYLIB_SRC = RAYLIB_DIR / "src"
RAYGUI_DIR = _plat_temp() / "Raygui"
RAYGUI_SRC = RAYGUI_DIR / "src"

FREEFONT_URL = "https://ftp.gnu.org/gnu/freefont/freefont-ttf-20120503.zip"
FREEFONT_DIR = _plat_temp() / "FreeFont"
# REAL source directory is language/_SRC (not src/). Linux is case-sensitive: src/IDE vs _SRC/ide
# and that difference breaks the build, so all paths use _SRC/ with the correct case.
FREEFONT_EMBED = ROOT / "_SRC" / "embed_freemono.c"

# Default project icon (gnuchan logo). `gcl -new` writes this PNG to the new project's
# assets/icon.png; it also becomes the exe icon during build (gcl_icon.c).
DEFAULT_ICON_PNG = REPO_ROOT / "assets" / "icon.png"
ICON_EMBED = ROOT / "_SRC" / "embed_icon.c"

# ---------- Program (CLI) sources — the IDE moved to a separate DLL (Programs/ide.dll) ----------
GCL_SRCS = [
    "_SRC/build/gcbundle_reader.c",
    "_SRC/build/gcbundle_pack.c",
    "_SRC/build/gcbundle_build.c",
    "_SRC/SharedPipeline/gcl_error.c",
    "_SRC/SharedPipeline/gcl_lexer.c",
    "_SRC/SharedPipeline/gcl_parser.c",
    "_SRC/GCL/SimpleRunner/gcl_runner.c",
    "_SRC/GCL/SimpleRunner/gcl_simple_runner.c",
    "_SRC/GCL/SimpleRunner/gcl_terminal.c",
    "_SRC/gcl_os.c",
    "_SRC/gcl_icon.c",
    "_SRC/embed_icon.c",
    "_SRC/gcl_main.c",
]

# ---------- IDE sources — Programs/ide.dll|.so (independent from gcl.exe) ----------
IDE_SRCS = [
    "_SRC/ide/ide_buffer.c",
    "_SRC/ide/ide_font.c",
    "_SRC/ide/ide_fs_tree.c",
    "_SRC/ide/ide_editor.c",
    "_SRC/ide/ide_clipboard.c",
    "_SRC/ide/ide_widgets.c",
    "_SRC/ide/ide_project.c",
    "_SRC/ide/ide_menu.c",
    "_SRC/ide/ide_main.c",
    "_SRC/ide/ide_new_project.c",
    "_SRC/ide/ide_proc.c",
    "_SRC/ide/ide_native_dialog.c",
    "_SRC/ide/ide_settings.c",
    "_SRC/ide/gcl_settings_panel.c",
    "_SRC/ide/gcl_lsp_scan.c",
    "_SRC/ide/ide_complete_ui.c",
    # SMART completion engine (language/_SRC/complete) — independent modules.
    "_SRC/complete/gcl_complete.c",
    "_SRC/complete/complete_context.c",
    "_SRC/complete/complete_scope.c",
    "_SRC/complete/complete_type.c",
    "_SRC/complete/complete_native.c",
    "_SRC/complete/complete_native_db.c",
    "_SRC/complete/complete_project.c",
    "_SRC/complete/complete_rank.c",
    "_SRC/complete/complete_index.c",
    "_SRC/complete/complete_diag.c",
    "_SRC/build/gcbundle_reader.c",
    "_SRC/build/gcbundle_pack.c",
    "_SRC/build/gcbundle_build.c",
    "_SRC/embed_freemono.c",
    "_SRC/embed_icon.c",
    "_SRC/gcl_os.c",
    "_SRC/gcl_icon.c",
]

# simple_doc.md: Library/Math.dll|.so, Stdio.dll|.so, Embed.dll|.so
MODULES = [
    ("Math", "_SRC/Modules/gcl_math.c"),
    ("Stdio", "_SRC/Modules/gcl_stdio.c"),
    ("Embed", "_SRC/Modules/gcl_embed.c"),
]

# ---------- Başlıksız (headless) regresyon testleri ----------
# İki test de düz C programıdır ve derleyici-nötr katmanları DOĞRUDAN çağırır
# (lexer/parser, ve "SMART" tamamlama motoru); raylib link etmezler, bu yüzden
# pencere açmadan saniyeler içinde koşarlar. Her DERLEMEDE çalıştırılırlar:
# bu katmanlardaki bir gerileme, IDE'de "yanlış öneri" olarak görünmeden ÖNCE
# burada yakalanır. Elle derleme satırları eskiden yalnızca test dosyalarının
# başlık yorumunda duruyordu ve sessizce eskiyebiliyordu.
PARSER_TEST_SRCS = [
    "language/tests/parser/test_parser.c",
    "language/_SRC/SharedPipeline/gcl_lexer.c",
    "language/_SRC/SharedPipeline/gcl_parser.c",
    "language/_SRC/SharedPipeline/gcl_error.c",
]

COMPLETE_TEST_SRCS = [
    "language/tests/complete/test_complete.c",
    "language/_SRC/complete/gcl_complete.c",
    "language/_SRC/complete/complete_context.c",
    "language/_SRC/complete/complete_scope.c",
    "language/_SRC/complete/complete_type.c",
    "language/_SRC/complete/complete_native.c",
    "language/_SRC/complete/complete_native_db.c",
    "language/_SRC/complete/complete_project.c",
    "language/_SRC/complete/complete_rank.c",
    "language/_SRC/complete/complete_index.c",
    "language/_SRC/complete/complete_diag.c",
]


def exe_name() -> str:
    return "gcl.exe" if os_name() == "windows" else "gcl"


def dll_ext() -> str:
    return "dll" if os_name() == "windows" else "so"


def run(cmd: list[str], cwd: Path, env: dict | None = None) -> None:
    print(f"[gcl] {' '.join(str(x) for x in cmd)}", flush=True)
    if env is None:
        env = os.environ.copy()
    subprocess.run(cmd, cwd=cwd, check=True, env=env)


def run_optional(cmd: list[str], cwd: Path, env: dict | None = None) -> bool:
    """Run with check=False; on failure return False without breaking the build.

    MK-2: for cleanup steps such as `make clean`. If the tool is missing (FileNotFoundError)
    or the target fails, only a warning is shown and the build continues.
    Return: True (success) / False (failed or tool missing).
    """
    print(f"[gcl] {' '.join(str(x) for x in cmd)}", flush=True)
    if env is None:
        env = os.environ.copy()
    try:
        result = subprocess.run(cmd, cwd=cwd, check=False, env=env)
        return result.returncode == 0
    except FileNotFoundError:
        print(f"[gcl] warning: '{cmd[0]}' not found — skipped", flush=True)
        return False
    except OSError as e:
        print(f"[gcl] warning: command could not run: {e}", flush=True)
        return False


def gcl_env() -> dict:
    env = os.environ.copy()
    if PYTHON_DIR.exists():
        env["PATH"] = str(PYTHON_DIR) + os.pathsep + env.get("PATH", "")
    return env


def _py_include_dir(py_dir: Path) -> Path:
    """Find the Python embed include directory.

    Windows embed: include/Python.h
    Linux build-standalone: include/python3.14/Python.h (subdirectory)
    """
    direct = py_dir / "include" / "Python.h"
    if direct.exists():
        return py_dir / "include"
    for sub in sorted(py_dir.glob("include/python3*/Python.h")):
        return sub.parent
    return py_dir / "include"  # fallback (if absent, python_config falls back to the system Python)


def find_python_dir() -> Path:
    """Find the correct Python 3 embed directory under _temp/<os> (python/ or Python/).

    Windows: directory containing python*.dll + libs/python*.lib.
    Linux:   directory containing lib/libpython*.so* or libpython*.so*.
    Platform directories are separated so the wrong platform embed is never mixed in.
    """
    plat = os_name()
    for name in ("python", "Python"):
        d = _plat_temp() / name
        inc = _py_include_dir(d)
        if not (d.exists() and (inc / "Python.h").exists()):
            continue
        if plat == "windows":
            # Windows embed: libs/python*.lib veya python*.dll
            if list(d.glob("libs/python*.lib")) or list(d.glob("python*.dll")):
                return d
        elif plat == "gnuLinux":
            # Linux embed: lib/libpython*.so* veya libpython*.so*.
            if list(d.glob("lib/libpython*.so*")) or list(d.glob("lib/libpython*.a")) or list(d.glob("libpython*.so*")):
                return d
    # If no matching embed exists, return an invalid path — build scripts
    # skip the embedded version via `py_dir.exists()` and fall back to the system Python in python_config().
    return _plat_temp() / "no-platform-python"


def _ensure_python_so(py_dir: Path) -> None:
    """WSL/DrvFs: symlinks are skipped — libpython3.14.so is not created.

    The linker cannot find -lpython3.14; create a .so copy from the real .so.1.0.
    This must be done both for the existing directory and for a newly downloaded one.
    """
    if os_name() != "gnuLinux":
        return
    lib_dir = py_dir / "lib"
    if not lib_dir.exists():
        return
    for so in sorted(lib_dir.glob("libpython*.so.*"), key=lambda p: p.name):
        stem = so.name.split(".so")[0]  # "libpython3.14"
        target = lib_dir / f"{stem}.so"
        if so.exists() and not target.exists():
            shutil.copy2(so, target)
            print(f"[gcl] .so copy created: {target}", flush=True)


def clone_lua() -> None:
    if (LUA_DIR / "lua.h").exists():
        print(f"[gcl] Lua hazır: {LUA_DIR}", flush=True)
        return
    if LUA_DIR.exists():
        shutil.rmtree(LUA_DIR)
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--depth", "1", "https://github.com/lua/lua.git", str(LUA_DIR)],
        cwd=REPO_ROOT)


def clone_raylib() -> None:
    if (RAYLIB_SRC / "raylib.h").exists():
        print(f"[gcl] Raylib hazır: {RAYLIB_SRC}", flush=True)
        return
    if RAYLIB_DIR.exists():
        shutil.rmtree(RAYLIB_DIR)
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--depth", "1", "https://github.com/raysan5/raylib.git", str(RAYLIB_DIR)],
        cwd=REPO_ROOT)


def clone_raygui() -> None:
    if (RAYGUI_SRC / "raygui.h").exists():
        print(f"[gcl] Raygui hazır: {RAYGUI_SRC}", flush=True)
        return
    if RAYGUI_DIR.exists():
        shutil.rmtree(RAYGUI_DIR)
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--depth", "1", "https://github.com/raysan5/raygui.git", str(RAYGUI_DIR)],
        cwd=REPO_ROOT)


def _tar_filter_drop_symlinks(member, path):
    """Skip symlinks/hardlinks from the python-build-standalone package.

    Tar extraction on WSL/DrvFs (NTFS) cannot create symlinks — it raises
    "Too many levels of symbolic links" (ELOOP). These links are not needed
    for the stdlib; plain files/directories are extracted instead.
    """
    if member.issym() or member.islnk():
        return None
    return member


def clone_python_embed() -> None:
    """Download the ready-made Python 3.14.x standalone embed build under _temp."""
    existing = find_python_dir()
    if existing.exists():
        _ensure_python_so(existing)
        print(f"[gcl] Python embed ready: {existing}", flush=True)
        return
    # Download the Python embed on Linux too — so that both Windows and Linux build outputs
    # generate the Library/Embeded/Python_Runtime/Python/ (stdlib) copy.
    # (Previously Linux used system python3-dev; because of that the build output
    #  sometimes did not contain the Python/ directory at all.)
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    import json
    import urllib.request
    import tarfile

    if os_name() == "windows":
        plat_tag = "x86_64-pc-windows-msvc"
    else:
        plat_tag = "x86_64-unknown-linux-gnu"

    # The platform directory is already created by _plat_temp().
    # Only remove the python/ or Python/ leftover for this platform.
    for name in ("python", "Python"):
        d = _plat_temp() / name
        if d.exists():
            try:
                shutil.rmtree(d, ignore_errors=True)
                print(f"[gcl] platform Python embed removed: {d}", flush=True)
            except OSError as e:
                print(f"[gcl] warning: {d} could not be removed: {e}", flush=True)

    latest = json.loads(urllib.request.urlopen(
        "https://api.github.com/repos/astral-sh/python-build-standalone/releases/latest",
        timeout=30).read())
    release_tag = latest["tag_name"]
    candidates = [a["name"] for a in latest["assets"]
                  if "cpython-3.14." in a["name"]
                  and plat_tag in a["name"]
                  and "install_only" in a["name"]
                  and a["name"].endswith(".tar.gz")]
    if not candidates:
        candidates = [a["name"] for a in latest["assets"]
                      if "cpython-3.14." in a["name"]
                      and "install_only" in a["name"]
                      and a["name"].endswith(".tar.gz")]
    if not candidates:
        print("[gcl] Python 3.14 asset not found — skipping", flush=True)
        return
    candidates.sort()
    fname = candidates[-1]
    url = f"https://github.com/astral-sh/python-build-standalone/releases/download/{release_tag}/{fname}"
    tmp = _plat_temp() / fname
    print(f"[gcl] Downloading Python {fname}", flush=True)
    urllib.request.urlretrieve(url, tmp)
    with tarfile.open(tmp, "r:gz") as t:
        # filter: skip symlinks/hardlinks (WSL/DrvFs ELOOP protection)
        t.extractall(_plat_temp(), filter=_tar_filter_drop_symlinks)
    tmp.unlink()
    extracted = _plat_temp() / "python"
    if extracted.exists() and not find_python_dir().exists():
        _ensure_python_so(extracted)
        extracted.rename(PYTHON_DIR)
        print(f"[gcl] Python embed directory moved: {PYTHON_DIR}", flush=True)
    _ensure_python_so(find_python_dir())
    print(f"[gcl] Python embed ready: {find_python_dir()}", flush=True)


def download_freefont() -> None:
    if (FREEFONT_DIR / "FreeMono.ttf").exists():
        print(f"[gcl] FreeFont ready: {FREEFONT_DIR}", flush=True)
        return
    import urllib.request
    import zipfile
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    FREEFONT_DIR.mkdir(parents=True, exist_ok=True)
    tmp_zip = _plat_temp() / "freefont.zip"
    tmp_dir = _plat_temp() / "freefont_tmp"
    print(f"[gcl] Downloading GNU FreeFont", flush=True)
    urllib.request.urlretrieve(FREEFONT_URL, tmp_zip)
    if tmp_dir.exists():
        shutil.rmtree(tmp_dir)
    tmp_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(tmp_zip, "r") as z:
        z.extractall(tmp_dir)
    for ttf in list(tmp_dir.rglob("*.ttf")):
        shutil.copy2(ttf, FREEFONT_DIR / ttf.name)
    shutil.rmtree(tmp_dir)
    tmp_zip.unlink()
    print(f"[gcl] FreeFont downloaded: {FREEFONT_DIR}", flush=True)


def embed_font() -> Path:
    if not (FREEFONT_DIR / "FreeMono.ttf").exists():
        download_freefont()
    data = (FREEFONT_DIR / "FreeMono.ttf").read_bytes()
    parts = ["/* generated: FreeMono.ttf embedded byte array */\n",
             "const unsigned char gcl_embed_freemono_ttf[] = {\n"]
    for i in range(0, len(data), 12):
        parts.append("    " + ",".join(str(b) for b in data[i:i + 12]) + ",\n")
    parts.append("    0\n};\n")
    parts.append(f"const unsigned int gcl_embed_freemono_ttf_size = {len(data)};\n")
    content = "".join(parts)
    # MK-3: do not rewrite the file if content is unchanged. Otherwise embed_freemono.c's
    # mtime changes on every build and recompiles the IDE DLL unnecessarily.
    if FREEFONT_EMBED.exists():
        try:
            if FREEFONT_EMBED.read_text(encoding="utf-8") == content:
                print(f"[gcl] font embed up to date (not rewritten): {FREEFONT_EMBED}", flush=True)
                return FREEFONT_EMBED
        except OSError:
            pass
    FREEFONT_EMBED.parent.mkdir(parents=True, exist_ok=True)
    FREEFONT_EMBED.write_text(content, encoding="utf-8")
    print(f"[gcl] font embedded: {FREEFONT_EMBED}", flush=True)
    return FREEFONT_EMBED


def embed_icon() -> Path:
    """assets/icon.png → _SRC/embed_icon.c (gcl_embed_icon_png byte array).

    `gcl -new` writes these bytes as the new project's assets/icon.png
    (gcl_icon_write_default), so the default gnuchan logo is always ready.
    If content is unchanged, the file is not rewritten again (same MK-3 rule as embed_font) —
    no unnecessary recompilation occurs.
    """
    if not DEFAULT_ICON_PNG.exists():
        print(f"[gcl] warning: default icon not found, skipping: {DEFAULT_ICON_PNG}", flush=True)
        return ICON_EMBED
    data = DEFAULT_ICON_PNG.read_bytes()
    parts = ["/* generated: assets/icon.png embedded byte array */\n",
             "const unsigned char gcl_embed_icon_png[] = {\n"]
    for i in range(0, len(data), 12):
        parts.append("    " + ",".join(str(b) for b in data[i:i + 12]) + ",\n")
    parts.append("    0\n};\n")
    parts.append(f"const unsigned int gcl_embed_icon_png_size = {len(data)};\n")
    content = "".join(parts)
    if ICON_EMBED.exists():
        try:
            if ICON_EMBED.read_text(encoding="utf-8") == content:
                print(f"[gcl] icon embed up to date (not rewritten): {ICON_EMBED}", flush=True)
                return ICON_EMBED
        except OSError:
            pass
    ICON_EMBED.parent.mkdir(parents=True, exist_ok=True)
    ICON_EMBED.write_text(content, encoding="utf-8")
    print(f"[gcl] icon embedded: {ICON_EMBED}", flush=True)
    return ICON_EMBED


def raylib_arch_matches(lib_path: Path) -> bool:
    """libraylib.a'nın ilk objesi mevcut platforma uygun mu?

    Windows (MinGW): COFF objesi → ilk 2 byte little-endian 0x8664 (AMD64).
    Linux (gcc): ELF objesi → ilk 4 byte '\\x7fELF'.
    Uyumsuzsa (örn. Windows'ta derlenen .a'yı Linux kullanıyorsa) yeniden derle.
    """
    try:
        import subprocess
        # ar t ile ilk üyeyi al
        listing = subprocess.run(["ar", "t", str(lib_path)], capture_output=True, text=True)
        first = listing.stdout.splitlines()[0] if listing.stdout.splitlines() else None
        if not first:
            return False
        data = subprocess.run(["ar", "p", str(lib_path), first], capture_output=True).stdout
        if not data:
            return False
        if os_name() == "windows":
            # COFF AMD64: 64 86 (little endian 0x8664)
            return data[0] == 0x64 and data[1] == 0x86
        else:
            # ELF: 7f 45 4c 46
            return data[:4] == b"\x7fELF"
    except Exception:
        return False


def build_raylib() -> Path:
    clone_raylib()
    lib = RAYLIB_SRC / "libraylib.a"
    if lib.exists() and raylib_arch_matches(lib):
        print(f"[gcl] Raylib library ready: {lib}", flush=True)
        return RAYLIB_SRC
    # Incompatible platform archive (for example, Windows mingw .a used on Linux) → delete both .a and .o,
    # then build locally. While MinGW objects remain, `make` rebuilds the archive and the result is still COFF (R_AMD64_IMAGEBASE).
    if lib.exists():
        for old in list(RAYLIB_SRC.glob("libraylib*.a")) + list(RAYLIB_SRC.glob("*.o")):
            try:
                old.unlink()
                print(f"[gcl] incompatible Raylib file removed: {old.name}", flush=True)
            except OSError:
                pass
    make_tool = "mingw32-make" if os_name() == "windows" else "make"
    # MK-2: fail fast if the build tool is missing (otherwise FileNotFoundError is ambiguous).
    if shutil.which(make_tool) is None:
        hint = "MinGW (mingw32-make)" if os_name() == "windows" else "make + build-essential"
        print(f"[gcl] ERROR: '{make_tool}' not found. To build Raylib, install {hint} "
              f"and make sure it is on PATH.", file=sys.stderr, flush=True)
        raise RuntimeError(f"required build tool not found: {make_tool}")
    # MK-2: `make clean` is a cleanup step; even if it fails, the build continues.
    if not run_optional([make_tool, "clean", "PLATFORM=PLATFORM_DESKTOP", "RAYLIB_LIBTYPE=STATIC"], cwd=RAYLIB_SRC):
        print(f"[gcl] warning: '{make_tool} clean' did not complete — cleanup skipped, "
              f"build continues anyway.", flush=True)
    run([make_tool, "PLATFORM=PLATFORM_DESKTOP", "RAYLIB_LIBTYPE=STATIC"], cwd=RAYLIB_SRC)
    stems = list(RAYLIB_SRC.glob("libraylib*.a"))
    if stems:
        print(f"[gcl] Raylib built: {stems[0]}", flush=True)
    return RAYLIB_SRC


def normalize_nix_py_lib(py_lib):
    """Convert the Linux libpython name to the -l linker flag:
       libpython3.13.a / libpython3.13.so.1.0 → python3.13."""
    if not py_lib:
        return py_lib
    name = py_lib
    if name.startswith("lib"):
        name = name[3:]
    i = name.find(".so")
    if i >= 0:
        name = name[:i]
    elif name.endswith(".a"):
        name = name[:-2]
    return name


def python_config():
    py_dir = find_python_dir()
    if py_dir.exists():
        inc = _py_include_dir(py_dir)
        if (inc / "Python.h").exists():
            if os_name() == "windows":
                libs_dir = py_dir / "libs"
                if libs_dir.exists():
                    libs = sorted([p for p in libs_dir.glob("python*.lib")], key=lambda p: p.name)
                    for name in reversed([p.name for p in libs]):
                        if (libs_dir / name).exists():
                            return str(inc), str(libs_dir), name
                dlls = list(py_dir.glob("python*.dll"))
                dlls.sort(key=lambda p: p.name)
                if dlls:
                    dll = dlls[-1]
                    return str(inc), str(dll.parent), dll.name
            for lib in list(py_dir.glob("lib/libpython*.so*")) + list(py_dir.glob("lib/libpython*.a")):
                if lib.exists():
                    return str(inc), str(lib.parent), normalize_nix_py_lib(lib.name)
            # Windows embed libraries (.lib) — Windows only.
            if os_name() == "windows":
                libs_dir = py_dir / "libs"
                if libs_dir.exists():
                    for name in sorted(p.name for p in libs_dir.glob("python*.lib")):
                        if (libs_dir / name).exists():
                            return str(inc), str(libs_dir), name
    try:
        py_inc = sysconfig.get_path('include')
        py_libdir = sysconfig.get_config_var('LIBDIR')
        # Linux: prefer the shared library (LDLIBRARY); otherwise use LIBRARY (static).
        py_lib = sysconfig.get_config_var('LDLIBRARY') or sysconfig.get_config_var('LIBRARY')
        if not py_libdir:
            py_libdir = sysconfig.get_config_var('LIBPL')
        if not py_inc or not py_libdir or not py_lib:
            return None, None, None
        if os_name() != "windows":
            py_lib = normalize_nix_py_lib(py_lib)
        # The build-standalone embed include directory is under /usr/include/python3.14;
        # sysconfig already provides the correct path here.
        return py_inc, py_libdir, py_lib
    except Exception:
        return None, None, None


def build_modules(build_dir: Path) -> None:
    """simple_doc.md: Library/Math.dll|.so, Stdio.dll|.so, Embed.dll|.so"""
    lib_dir = build_dir / "Library"
    lib_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()

    py_inc, py_libdir, py_lib = python_config()

    # Embed.so/dll, LuaRaylib, Raygui, etc. all link against raylib. On Linux too,
    # libraylib.a must be built before the modules so that Embed.so can resolve the
    # raylib symbols; otherwise it produces an "undefined symbol" error.
    raylib_src = build_raylib()
    for name, src in MODULES:
        out = lib_dir / f"{name}.{ext}"
        sources = [src]

        # Python: link with libpython on Linux (GCL_HAVE_PYTHON),
        # but on Windows the MSVC .lib is incompatible with MinGW so dynamic
        # loading is used (GCL_EMBED_PYTHON_DYNAMIC). In both cases the
        # Python.h include path is required.
        use_python_link = (os_name() != "windows") and py_inc and py_libdir and py_lib
        use_python_dynamic = (os_name() == "windows") and py_inc

        # Embed.dll contains the Lua/Python runtime + LuaRaylib/LuaRaygui bindings.
        # gcl -luarun / -pyrun are now dynamically loaded from Embed.dll (gcl_main.c).
        cmd_get_raylib = False
        if name == "Embed":
            sources += ["_SRC/embed/gcl_embed_lua.c",
                        "_SRC/embed/gcl_luaraylib.c",
                        "_SRC/embed/gcl_luaraygui.c"]
            if use_python_link or use_python_dynamic:
                sources += ["_SRC/embed/gcl_embed_python.c"]
            # Embed the Lua sources statically (no liblua.a build is created).
            # onelua.c (amalgamation), lua.c (standalone interpreter),
            # luac.c (compiler), and ltests.c (tests) — duplicate definition errors occur.
            skip_lua = {"onelua.c", "lua.c", "luac.c", "ltests.c"}
            for lua_src in sorted(LUA_SRC.glob("*.c"), key=lambda p: p.name):
                if lua_src.name not in skip_lua:
                    sources.append(str(lua_src))
            cmd_get_raylib = True

        cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L"]
        if use_python_link:
            cmd += ["-DGCL_HAVE_PYTHON"]
        elif use_python_dynamic:
            cmd += ["-DGCL_EMBED_PYTHON_DYNAMIC"]
        cmd += ["-I", "_SRC/include",
                "-I", "_SRC/embed",
                "-I", str(LUA_SRC)]
        if cmd_get_raylib:
            cmd += ["-I", str(RAYLIB_SRC),
                    "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
                    "-I", str(RAYGUI_SRC),
                    "-DRAYGUI_IMPLEMENTATION"]
        if py_inc:
            cmd += ["-I", str(py_inc)]
        py_dir = find_python_dir()
        if py_dir.exists():
            cmd += ["-DPYTHON_EMBED_DIR=\"%s\"" % str(py_dir).replace("\\", "\\\\")]
        cmd += ["-DLUA_EMBED_DIR=\"%s\"" % str(LUA_SRC).replace("\\", "\\\\")]
        cmd += sources
        cmd += ["-o", str(out)]
        if os_name() == "windows":
            cmd += ["-Wl,--export-all-symbols"]
        # Link the Python embed library only in direct link mode.
        if use_python_link:
            if os_name() == "windows":
                lib_path = Path(py_libdir) / py_lib
                if lib_path.exists():
                    cmd += [str(lib_path)]
                else:
                    cmd += ["-L", str(py_libdir)]
                    stem = py_lib[:-4] if py_lib.endswith(".lib") else py_lib
                    cmd += ["-l" + stem]
            else:
                cmd += ["-L", str(py_libdir)]
                cmd += ["-l" + py_lib]
        # Embed.dll needs raylib functions from LuaRaylib/LuaRaygui → link raylib.
        # On Windows use the static libraylib.a (modules still use import libs).
        raylib_a = RAYLIB_SRC / "libraylib.a"
        if cmd_get_raylib and raylib_a.exists():
            cmd += [str(raylib_a)]
        elif cmd_get_raylib:
            stems = list(RAYLIB_SRC.glob("libraylib*.a"))
            if stems:
                cmd += [str(stems[0])]
        cmd += ["-lm"]
        if os_name() == "windows" and cmd_get_raylib:
            cmd += ["-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
        elif os_name() != "windows" and cmd_get_raylib:
            # Embed.so links against the static libraylib.a → X11/GL system libraries are required.
            # If missing, dlopen raises "undefined symbol: XFree".
            cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
        run(cmd, cwd=ROOT)
        print(f"[gcl] modül: {out}", flush=True)


def build_raylib_module(build_dir: Path) -> None:
    """simple_doc.md: Library/Raylib.dll|.so — gcl_raylib.c + libraylib.a."""
    lib_dir = build_dir / "Library"
    lib_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    out = lib_dir / f"Raylib.{ext}"
    raylib_src = build_raylib()
    libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    if not libs:
        print(f"[gcl] uyarı: libraylib.a bulunamadı, Raylib atlanıyor", flush=True)
        return
    cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
           "-I", "_SRC/include",
           "-I", str(RAYLIB_SRC),
           "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
           "_SRC/Modules/gcl_raylib.c",
           str(libs[0]),
           "-o", str(out)]
    if os_name() == "windows":
        cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
    else:
        cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
    cmd += ["-lm"]
    run(cmd, cwd=ROOT)
    print(f"[gcl] modül: {out}", flush=True)

    # Generate an import library from Raylib.dll so Raygui/LuaRaylib/PyRaylib share the same raylib instance.
    # Otherwise Raygui.dll embeds its own libraylib.a → second, uninitialized raylib state → crash.
    if os_name() == "windows":
        imp_lib = lib_dir / "libraylib.a"
        if out.exists() and not imp_lib.exists():
            import subprocess
            # gendef - <dll> → outputs a .def dump to stdout. Write it to lib_dir/raylib.def.
            r1 = subprocess.run(["gendef", "-", str(out)], capture_output=True, text=True)
            def_content = r1.stdout
            if def_content:
                def_path = lib_dir / "raylib.def"
                def_path.write_text(def_content, encoding="utf-8")
                r2 = subprocess.run(["dlltool", "-d", str(def_path), "-l", str(imp_lib), "-D", str(out)],
                                    capture_output=True, text=True)
                if imp_lib.exists():
                    print(f"[gcl] import lib created: {imp_lib}", flush=True)
                else:
                    print(f"[gcl] dlltool output: {r2.stderr or r2.stdout}", flush=True)
            else:
                print(f"[gcl] no gendef output: {r1.stderr}", flush=True)


def build_raygui_module(build_dir: Path) -> None:
    """simple_doc.md: Library/Raygui.dll|.so — gcl_raygui.c + raygui.h."""
    lib_dir = build_dir / "Library"
    lib_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    out = lib_dir / f"Raygui.{ext}"
    # gcl_raygui.c: compiles raygui.h with RAYGUI_IMPLEMENTATION.
    # Raygui needs raylib functions → link to Raylib.dll (import lib).
    # Otherwise it embeds its own libraylib.a → second, uninitialized raylib state → crash.
    raylib_stack_libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    import_lib = lib_dir / "libraylib.a"
    # GCL native Raygui.dll is loaded in the SAME process as Raylib.dll → use import lib
    # (single raylib state). Otherwise it embeds its own libraylib.a → second, uninitialized
    # raylib state → crash.
    raylib_link = import_lib if (os_name() == "windows" and import_lib.exists()) else (raylib_stack_libs[0] if raylib_stack_libs else None)
    cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
           "-DRAYGUI_IMPLEMENTATION",
           "-I", "_SRC/include",
           "-I", str(RAYGUI_SRC),
           "-I", str(RAYLIB_SRC),
           "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
           "_SRC/Modules/gcl_raygui.c",
           "-o", str(out)]
    if raylib_link:
        cmd += [str(raylib_link)]
    if os_name() == "windows":
        cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
    else:
        cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
    cmd += ["-lm"]
    run(cmd, cwd=ROOT)
    print(f"[gcl] modül: {out}", flush=True)


def build_lua_runtime(build_dir: Path) -> None:
    """simple_doc.md: Embeded/Lua_Runtime/{lua,LuaRaylib,LuaRaygui}.dll|.so"""
    lib_dir = build_dir / "Library"
    embed_dir = lib_dir / "Embeded" / "Lua_Runtime"
    embed_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    out = embed_dir / f"lua.{ext}"
    skip_lua = {"onelua.c", "lua.c", "luac.c", "ltests.c"}
    sources = [str(p) for p in sorted(LUA_SRC.glob("*.c"), key=lambda p: p.name) if p.name not in skip_lua]
    cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L", "-I", str(LUA_SRC)]
    cmd += sources
    cmd += ["-o", str(out)]
    if os_name() == "windows":
        cmd += ["-Wl,--export-all-symbols"]
    cmd += ["-lm"]
    run(cmd, cwd=ROOT)
    print(f"[gcl] modül: {out}", flush=True)

    # LuaRaylib / LuaRaygui — real Lua C modules (gcl_luaraylib.c / gcl_luaraygui.c).
    # luaopen_LuaRaylib / luaopen_LuaRaygui are exported.
    # NOTE: raygui.h is header-only → RAYGUI_IMPLEMENTATION is required in gcl_luaraygui.c.
    # Embed the Lua sources statically into each module (the Lua state pointer is shared).
    # On Windows link to Raylib.dll via import lib (single raylib instance); on Linux use the static archive.
    raylib_libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    # Statik arşiv: raw raylib fonksiyonları (DrawGrid, DrawCube, Audio...) import lib'de yok.
    raylib_link = raylib_libs[0] if raylib_libs else None
    for src_base, out_name, needs_raygui in (
        ("gcl_luaraylib", "LuaRaylib", False),
        ("gcl_luaraygui", "LuaRaygui", True),
    ):
        out_dll = embed_dir / f"{out_name}.{ext}"
        cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
               "-I", "_SRC/embed",
               "-I", str(LUA_SRC),
               "-I", str(RAYLIB_SRC),
               "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
               "-I", str(RAYGUI_SRC)]
        if needs_raygui:
            cmd += ["-DRAYGUI_IMPLEMENTATION"]
        cmd += [f"_SRC/embed/{src_base}.c"]
        cmd += sources   # Lua kaynakları (lapi.c, lauxlib.c, ...)
        cmd += ["-o", str(out_dll)]
        if raylib_link:
            cmd += [str(raylib_link)]
        if os_name() == "windows":
            cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
        else:
            cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
        cmd += ["-lm"]
        run(cmd, cwd=ROOT)
        print(f"[gcl] modül: {out_dll}", flush=True)


def build_python_runtime(build_dir: Path) -> None:
    """simple_doc.md: Embeded/Python_Runtime/{Python,PyRaylib,PyRaygui}.dll|.so + Python/"""
    lib_dir = build_dir / "Library"
    embed_dir = lib_dir / "Embeded" / "Python_Runtime"
    embed_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    py_dir = find_python_dir()
    # Python/ copy (stdlib)
    py_copy = embed_dir / "Python"
    if py_dir.exists() and not py_copy.exists():
        shutil.copytree(py_dir, py_copy, ignore=shutil.ignore_patterns("__pycache__"))
        print(f"[gcl] Python/ copied: {py_copy}", flush=True)
    # Python.dll/.so — copy the current Python embed DLL (Windows) or link to libpython
    if os_name() == "windows" and py_dir.exists():
        dlls = sorted(py_dir.glob("python*.dll"), key=lambda p: p.name)
        if dlls:
            src_dll = dlls[-1]
            out = embed_dir / f"Python.{ext}"
            shutil.copy2(src_dll, out)
            print(f"[gcl] modül: {out} (kopyalandı)", flush=True)
    else:
        # Linux: Python.dll yerine libpython.so kopyala veya link et.
        # py_lib is the normalized name "python3.14"; it does not directly match libpython3.14.so.1.0.
        # Therefore locate the real libpython*.so* file in the lib directory.
        py_inc, py_libdir, py_lib = python_config()
        if py_libdir:
            # MK-1: glob order is not guaranteed; copying the static "libpython*.a"
            # as "Python.so" breaks dlopen. Check for the shared .so first; only then fall back to static .a.
            lib_candidates = sorted(Path(py_libdir).glob("libpython*.so*")) or \
                             sorted(Path(py_libdir).glob("libpython*.a"))
            if lib_candidates:
                src_lib = lib_candidates[0]
                out = embed_dir / f"Python.{ext}"
                shutil.copy2(src_lib, out)
                print(f"[gcl] module: {out} (copied)", flush=True)
                # Embed.so/gcl.so etc. carry a DT_NEEDED dependency on libpython3.14.so.1.0.
                # Copy it with its original name as well — otherwise dlopen reports "unknown module".
                for lib_so in lib_candidates:
                    if lib_so.name.startswith("libpython"):
                        orig_out = embed_dir / lib_so.name
                        if not orig_out.exists():
                            shutil.copy2(lib_so, orig_out)
                            print(f"[gcl] module: {orig_out} (copy with original name)", flush=True)

    # PyRaylib / PyRaygui — real Python C extension modules.
    # They are loaded from Python via `import raylib` / `import raygui`.
    # NOTE: raygui.h is header-only → RAYGUI_IMPLEMENTATION is required in gcl_pyraygui.c.
    # On Windows link to Raylib.dll via import lib (single raylib instance); on Linux use the static archive.
    py_inc, py_libdir, py_lib = python_config()
    import_lib = lib_dir / "libraylib.a"
    # gcl.pyd — used from Python as `import gcl` + `gcl.init()`.
    # It does not link to Raylib (safe) but it does link to the Python C-API.
    ext_suffix = "pyd" if os_name() == "windows" else "so"
    gcl_out = embed_dir / f"gcl.{ext_suffix}"
    gcl_cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
               "-I", "_SRC/embed"]
    if py_inc:
        gcl_cmd += ["-I", str(py_inc)]
    gcl_cmd += ["_SRC/embed/gcl_pygcl.c", "-o", str(gcl_out)]
    if os_name() == "windows" and py_libdir and py_lib:
        gcl_cmd += ["-L", str(py_libdir)]
        stem = py_lib[:-4] if py_lib.endswith(".lib") else py_lib
        gcl_cmd += ["-l" + stem]
    elif os_name() != "windows" and py_libdir and py_lib:
        gcl_cmd += ["-L", str(py_libdir), "-l" + py_lib]
    if os_name() == "windows":
        gcl_cmd += ["-Wl,--export-all-symbols"]
    gcl_cmd += ["-lm"]
    run(gcl_cmd, cwd=ROOT)
    print(f"[gcl] modül: {gcl_out}", flush=True)

    raylib_libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    # Statik arşiv: raw raylib fonksiyonları (DrawGrid, DrawCube, Audio...) import lib'de yok.
    raylib_link = raylib_libs[0] if raylib_libs else None
    for src_base, mod_name, needs_raygui in (
        ("gcl_pyraylib", "raylib", False),
        ("gcl_pyraygui", "raygui", True),
    ):
        ext_suffix = "pyd" if os_name() == "windows" else "so"
        out_mod = embed_dir / f"{mod_name}.{ext_suffix}"
        cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
               "-I", "_SRC/embed",
               "-I", str(RAYLIB_SRC),
               "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
               "-I", str(RAYGUI_SRC)]
        if needs_raygui:
            cmd += ["-DRAYGUI_IMPLEMENTATION"]
        if py_inc:
            cmd += ["-I", str(py_inc)]
        cmd += [f"_SRC/embed/{src_base}.c", "-o", str(out_mod)]
        if raylib_link:
            cmd += [str(raylib_link)]
        if os_name() == "windows" and py_libdir and py_lib:
            cmd += ["-L", str(py_libdir)]
            # MinGW: python314.lib MSVC import lib is OK; fallback: "-lpython314"
            stem = py_lib[:-4] if py_lib.endswith(".lib") else py_lib
            cmd += ["-l" + stem]
        elif os_name() != "windows" and py_libdir and py_lib:
            cmd += ["-L", str(py_libdir), "-l" + py_lib]
        if os_name() == "windows":
            cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
        else:
            cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
        cmd += ["-lm"]
        run(cmd, cwd=ROOT)
        print(f"[gcl] modül: {out_mod}", flush=True)

    # simple_doc.md: PyRaylib.dll|.so / PyRaygui.dll|.so — copy of the same module.
    lib_dir = build_dir / "Library"
    for dep_name, out_name in (("Raylib", "PyRaylib"), ("Raygui", "PyRaygui")):
        src_dll = lib_dir / f"{dep_name}.{ext}"
        out_dll = embed_dir / f"{out_name}.{ext}"
        if src_dll.exists() and not out_dll.exists():
            shutil.copy2(src_dll, out_dll)
            print(f"[gcl] modül: {out_dll} (kopyalandı)", flush=True)


def ensure_native_db() -> None:
    """Regenerate _SRC/complete/complete_native_db.c from the module bindings.

    The Raylib/Raygui completion tables are DERIVED from _SRC/Modules/gcl_raylib.c
    and gcl_raygui.c. Without this step the IDE keeps describing an older API: a
    function whose binding changed keeps suggesting the previous return type and
    chained completion dies silently (todo.md -> "language full bug hunting" #7).
    It also cross-checks each binding against the real raylib.h/raygui.h and prints
    the module gaps it finds (no-op stubs that silently return 0 instead of a value).
    The generator is idempotent: it does not rewrite the file when the content is
    unchanged, so the IDE DLL is not rebuilt for nothing.
    """
    script = REPO_ROOT / "tools" / "gen_native_db.py"
    if not script.exists():
        print(f"[gcl] warning: native DB generator not found - skipped: {script}", flush=True)
        return
    run_optional([sys.executable, str(script)], cwd=REPO_ROOT)


def build_ide(build_dir: Path) -> None:
    """simple_doc.md: Programs/ide.dll|.so — IDE is compiled as a separate DLL.

    The IDE is split from gcl.exe; gcl.exe becomes smaller. The IDE opens its own
    window with its own raylib instance (gcl_ide_run). The Embed runtimes (Lua/Python)
    remain in the CLI, so they are not needed here; only raylib + gcl_os + LSP + IDE sources are built.
    """
    programs_dir = build_dir / "Programs"
    programs_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    out = programs_dir / f"ide.{ext}"

    raylib_src = build_raylib()
    raylib_a = raylib_src / "libraylib.a"
    if not raylib_a.exists():
        stems = list(raylib_src.glob("libraylib*.a"))
        if stems:
            raylib_a = stems[0]

    py_inc, py_libdir, py_lib = python_config()
    use_python_dynamic = (os_name() == "windows") and py_inc
    use_python_link = (os_name() != "windows") and py_inc and py_libdir and py_lib

    cmd = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
           "-Wno-unused-function", "-Wno-unused-variable", "-Wno-format-truncation",
           "-Wno-discarded-qualifiers",
           "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
           "-I", "_SRC/ide", "-I", "_SRC/build",
           "-I", "_SRC/complete",
           "-I", "_SRC/SharedPipeline", "-I", "_SRC/GCL/SimpleRunner",
           "-I", "_SRC/Modules", "-I", "_SRC/include", "-I", "_SRC/embed",
           "-I", str(RAYLIB_SRC),
           "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
           "-I", str(RAYGUI_SRC),
           "-I", str(LUA_SRC),
           "-DRAYGUI_IMPLEMENTATION"]
    if use_python_link:
        cmd += ["-DGCL_HAVE_PYTHON"]
    elif use_python_dynamic:
        cmd += ["-DGCL_EMBED_PYTHON_DYNAMIC"]
    if py_inc:
        cmd += ["-I", str(py_inc)]
    py_dir = find_python_dir()
    if py_dir.exists():
        cmd += ["-DPYTHON_EMBED_DIR=\"%s\"" % str(py_dir).replace("\\", "\\\\")]
    cmd += ["-DLUA_EMBED_DIR=\"%s\"" % str(LUA_SRC).replace("\\", "\\\\")]
    cmd += IDE_SRCS
    if raylib_a.exists():
        cmd += [str(raylib_a)]
    cmd += ["-o", str(out)]
    if os_name() == "windows":
        cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32",
                "-luser32", "-lshell32", "-lcomdlg32", "-lole32"]
    else:
        cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
    cmd += ["-lm"]
    run(cmd, cwd=ROOT)
    print(f"[gcl] modül: {out}", flush=True)


def build_gcl() -> Path:
    clone_lua()
    clone_raygui()
    clone_python_embed()
    download_freefont()
    embed_font()
    embed_icon()
    raylib_src = build_raylib()

    build_dir = BUILD_ROOT / os_name()
    build_dir.mkdir(parents=True, exist_ok=True)
    exe = build_dir / exe_name()
    build_modules(build_dir)
    build_raylib_module(build_dir)
    build_raygui_module(build_dir)
    build_lua_runtime(build_dir)
    build_python_runtime(build_dir)
    # If the IDE sources are missing, fail the build for CI purposes but emit a warning.
    # Programs/ide.so is generated only when the language/_SRC/ide/ + _SRC/build/ sources exist in the repo.
    # embed_freemono.c / embed_icon.c are generated automatically (embed_font()/embed_icon());
    # even if they are not tracked in CI, they appear during the build, so they are excluded.
    generated = {"_SRC/embed_freemono.c", "_SRC/embed_icon.c"}
    missing_ide = [s for s in IDE_SRCS
                   if s not in generated and not (ROOT / s).exists()]
    if not missing_ide:
        # The native completion tables must match the bindings actually being
        # built, otherwise the IDE completes against a stale API (todo #7).
        ensure_native_db()
        build_ide(build_dir)
    else:
        print(f"[gcl] WARNING: IDE sources missing ({len(missing_ide)}/{len(IDE_SRCS)} files) — Programs/ was not generated (IDE unavailable).", file=sys.stderr)
        for m in missing_ide:
            print(f"  - {m}", file=sys.stderr)
        print("[gcl] WARNING: Missing files for the IDE output must be added to the repo.", file=sys.stderr)

    assets_dir = build_dir / "Assets"
    assets_dir.mkdir(parents=True, exist_ok=True)
    if (FREEFONT_DIR / "FreeMono.ttf").exists():
        shutil.copy2(FREEFONT_DIR / "FreeMono.ttf", assets_dir / "FreeMono.ttf")
        print(f"[gcl] font copied: {assets_dir / 'FreeMono.ttf'}", flush=True)

    if os_name() == "windows":
        for old_dll in list(build_dir.glob("python*.dll")):
            try:
                old_dll.unlink()
                print(f"[gcl] old Python DLL removed: {old_dll}", flush=True)
            except OSError:
                pass

    res_file = None
    rc_file = ROOT / "gcl.rc"
    if os_name() == "windows" and rc_file.exists():
        res_file = build_dir / "gcl.res"
        run(["windres", str(rc_file), "-O", "coff", "-o", str(res_file)], cwd=ROOT)

    # --- Option B: gcl.exe only contains the GCL language (~400KB) ---
    # The Lua/Python runtime + raylib + all bindings were removed from gcl.exe.
    # gcl -luarun / -pyrun now loads dynamically from Library/Embed.dll and Library/Embeded/*.dll
    # in gcl_main.c. They will not work if those DLLs are missing.
    cmd = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
           "-Wno-unused-function", "-Wno-unused-variable", "-Wno-format-truncation",
           "-Wno-discarded-qualifiers",
           "-I", "_SRC/ide",
           "-I", "_SRC/build",
           "-I", "_SRC/SharedPipeline",
           "-I", "_SRC/GCL/SimpleRunner",
           "-I", "_SRC/Modules",
           "-I", "_SRC/include",
           "-I", "_SRC/embed",
           "-D_POSIX_C_SOURCE=200809L"]
    # If _SRC/build/*.c is absent in CI, skip the gcBundle sources and disable the bundle code
    # in gcl_main.c via -DGCL_SKIP_BUNDLE.
    gcl_srcs = [s for s in GCL_SRCS if (ROOT / s).exists()]
    if not (ROOT / "_SRC" / "build" / "gcbundle_reader.c").exists():
        cmd += ["-DGCL_SKIP_BUNDLE"]
    cmd += gcl_srcs
    # gcl_runner.c uses fmod → libm is required (it should be added in the Linux makefile as -lm;
    # on Windows -lm is harmless and works fine with MinGW).
    cmd += ["-lm"]
    if os_name() == "windows":
        cmd += ["-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lcomdlg32", "-lole32"]
    else:
        cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
    if os_name() == "windows" and res_file is not None:
        cmd += [str(res_file)]
    cmd += ["-o", str(exe)]
    run(cmd, cwd=ROOT)
    print(f"[gcl] built {exe}", flush=True)
    return build_dir


def run_test_suite(name: str, sources: list[str], include_dir: str) -> bool:
    """Bir başlıksız test paketini derle ve koş. Başarıda True.

    Derleme/koşma hataları AÇIK olarak raporlanır: derlenemeyen bir test
    sessizce "her şey yolunda" gibi görünmemelidir. Çalışma dizini repo
    köküdür; test_complete proje taklidi dosyaları _temp/gcl_ctest/proj
    altına yazar.

    İki ayrıntı Windows'ta görünmez, Linux'ta ise testi düşürür:
      * Derleme satırı, üretim derlemelerinin kullandığı
        -D_POSIX_C_SOURCE=200809L bayrağını da geçirir. -std=c99 tek başına
        strict ANSI modudur ve POSIX bildirimlerini kapatır; strdup() gibi
        fonksiyonlar dolaylı olarak bildirilir, int döner ve 64 bit'te
        pointer'ı KIRPAR (ptest bu yüzden exit=-11 ile düşüyordu).
      * İkili _temp/tests/ altına kurulur, doğrudan _temp/ içine DEĞİL. Aksi
        halde _temp/ctest ikilisinin kendisi ile testin kurduğu _temp/ctest/
        fixture klasörü aynı yola düşer; Linux'ta (".exe" son eki yok) mkdir
        EEXIST ile başarısız olur, bütün fixture yazımları ENOTDIR verir ve
        ctest "proje kurulumu" adımında düşer.
    """
    missing = [s for s in sources if not (REPO_ROOT / s).exists()]
    if missing:
        print(f"[gcl] uyarı: test atlandı ({name}) — eksik kaynak: "
              f"{', '.join(missing)}", file=sys.stderr, flush=True)
        return True

    # İkili ile test fixture'ları AYRI ağaçlarda kalmalı — bkz. docstring.
    bin_dir = TEMP_ROOT / "tests"
    bin_dir.mkdir(parents=True, exist_ok=True)
    ext = ".exe" if os_name() == "windows" else ""
    exe = bin_dir / f"{name}{ext}"
    # Üretim derlemeleriyle aynı feature sözleşmesi (bkz. docstring).
    cmd = ["gcc", "-std=c99", "-D_POSIX_C_SOURCE=200809L",
           "-I", include_dir] + sources + ["-o", str(exe), "-lm"]
    if not run_optional(cmd, cwd=REPO_ROOT):
        print(f"[gcl] ERROR: {name} derlenemedi (test paketi çalıştırılamadı)",
              file=sys.stderr, flush=True)
        return False

    print(f"[gcl] test: {name}", flush=True)
    result = subprocess.run([str(exe)], cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        print(f"[gcl] ERROR: {name} BAŞARISIZ (exit={result.returncode})",
              file=sys.stderr, flush=True)
        return False
    return True


def main() -> int:
    build_dir = build_gcl()
    print(f"[gcl] output: {build_dir}", flush=True)

    # Başlıksız testler derlemeden SONRA koşar: IDE DLL'i ile aynı kaynakları
    # kullandıkları için gerçek çıktının da tutarlı olduğunu doğrularlar.
    # İkisi de her koşuda çalışır (kısa devre yok) — biri patlarsa diğeri de
    # sonucunu bildirsin.
    results = [
        run_test_suite("ptest", PARSER_TEST_SRCS, "language/_SRC/SharedPipeline"),
        run_test_suite("ctest", COMPLETE_TEST_SRCS, "language/_SRC/complete"),
    ]
    if not all(results):
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as e:
        print(f"[gcl] hata: {e}", file=sys.stderr)
        sys.exit(1)
