#!/usr/bin/env python3
"""
GCL build chain (C version).

Kullanım:
  python makefile.py        # tek komut: bağımlılık + build (parametre yok)

Build çıktısı (simple_doc.md):
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
    """_temp/<os>/ — bağımlılıklar platforma göre ayrı tutulur."""
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
FREEFONT_EMBED = ROOT / "src" / "embed_freemono.c"

# ---------- Program (CLI) kaynakları — IDE ayrı DLL'e taşındı (Programs/ide.dll) ----------
GCL_SRCS = [
    "src/Build/gcbundle_reader.c",
    "src/Build/gcbundle_pack.c",
    "src/Build/gcbundle_build.c",
    "src/SharedPipeline/gcl_error.c",
    "src/SharedPipeline/gcl_lexer.c",
    "src/SharedPipeline/gcl_parser.c",
    "src/GCL/SimpleRunner/gcl_runner.c",
    "src/GCL/SimpleRunner/gcl_simple_runner.c",
    "src/gcl_os.c",
    "src/gcl_main.c",
]

# ---------- IDE kaynakları — Programs/ide.dll|.so (gcl.exe'den bağımsız) ----------
IDE_SRCS = [
    "src/IDE/ide_buffer.c",
    "src/IDE/ide_font.c",
    "src/IDE/ide_fs_tree.c",
    "src/IDE/ide_editor.c",
    "src/IDE/ide_clipboard.c",
    "src/IDE/ide_widgets.c",
    "src/IDE/ide_project.c",
    "src/IDE/ide_menu.c",
    "src/IDE/ide_main.c",
    "src/IDE/ide_new_project.c",
    "src/IDE/ide_proc.c",
    "src/IDE/ide_native_dialog.c",
    "src/IDE/ide_settings.c",
    "src/IDE/gcl_settings_panel.c",
    "src/IDE/gcl_lsp_scan.c",
    "src/Build/gcbundle_reader.c",
    "src/Build/gcbundle_pack.c",
    "src/Build/gcbundle_build.c",
    "src/embed_freemono.c",
    "src/gcl_os.c",
]

# simple_doc.md: Library/Math.dll|.so, Stdio.dll|.so, Embed.dll|.so
MODULES = [
    ("Math", "src/Modules/gcl_math.c"),
    ("Stdio", "src/Modules/gcl_stdio.c"),
    ("Embed", "src/Modules/gcl_embed.c"),
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


def gcl_env() -> dict:
    env = os.environ.copy()
    if PYTHON_DIR.exists():
        env["PATH"] = str(PYTHON_DIR) + os.pathsep + env.get("PATH", "")
    return env


def _py_include_dir(py_dir: Path) -> Path:
    """Python embed include dizinini bul.

    Windows embed: include/Python.h
    Linux build-standalone: include/python3.14/Python.h (alt dizin)
    """
    direct = py_dir / "include" / "Python.h"
    if direct.exists():
        return py_dir / "include"
    for sub in sorted(py_dir.glob("include/python3*/Python.h")):
        return sub.parent
    return py_dir / "include"  # fallback (yoksa python_config sistem python'una düşer)


def find_python_dir() -> Path:
    """_temp/<os>/ altında platforma uygun Python 3 embed dizinini bul (python/ veya Python/).

    Windows: python*.dll + libs/python*.lib içeren dizin.
    Linux:   lib/libpython*.so* veya libpython*.so* içeren dizin.
    Platform dizinleri ayrı olduğu için yanlış platform embed'i karışmaz.
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
    # Platforma uygun embed yoksa geçersiz yol döndür — build scriptleri
    # `py_dir.exists()` ile gömülü embed'i atlar, python_config() sistem python'una düşer.
    return _plat_temp() / "no-platform-python"


def _ensure_python_so(py_dir: Path) -> None:
    """WSL/DrvFs: symlink'ler atlanıyor — libpython3.14.so oluşmaz.

    -lpython3.14 linker'ı bulamaz; gerçek .so.1.0'dan .so kopyası üret.
    Hem hazır dizinde hem yeni indirilen dizinde çağrılmalı.
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
            print(f"[gcl] .so kopyası oluşturuldu: {target}", flush=True)


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
    """python-build-standalone paketindeki symlink/hardlink'leri atla.

    WSL/DrvFs (NTFS) üzerinde tar çıkarımı symlink oluşturamaz — "Too many
    levels of symbolic links" (ELOOP) hatası verir. Bu linkler stdlib için
    gerekmez; düz dosya/dizinler çıkarılır.
    """
    if member.issym() or member.islnk():
        return None
    return member


def clone_python_embed() -> None:
    """Hazır Python 3.14.x standalone embed build'i _temp altına indir."""
    existing = find_python_dir()
    if existing.exists():
        _ensure_python_so(existing)
        print(f"[gcl] Python embed hazır: {existing}", flush=True)
        return
    # Linux'ta da Python embed indir — hem Windows hem Linux build çıktısında
    # Library/Embeded/Python_Runtime/Python/ (stdlib) kopyası üretilsin.
    # (Eskiden Linux'ta sistem python3-dev kullanılıyordu; bu yüzden build
    #  çıktısında Python/ klasörü hiç oluşmuyordu.)
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    import json
    import urllib.request
    import tarfile

    if os_name() == "windows":
        plat_tag = "x86_64-pc-windows-msvc"
    else:
        plat_tag = "x86_64-unknown-linux-gnu"

    # Platform dizini zaten _plat_temp() tarafından oluşturuldu.
    # Sadece bu platforma ait python/ veya Python/ kalıntısı varsa temizle.
    for name in ("python", "Python"):
        d = _plat_temp() / name
        if d.exists():
            try:
                shutil.rmtree(d, ignore_errors=True)
                print(f"[gcl] platform Python embed silindi: {d}", flush=True)
            except OSError as e:
                print(f"[gcl] uyarı: {d} silinemedi: {e}", flush=True)

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
        print("[gcl] Python 3.14 asset bulunamadı — atlanıyor", flush=True)
        return
    candidates.sort()
    fname = candidates[-1]
    url = f"https://github.com/astral-sh/python-build-standalone/releases/download/{release_tag}/{fname}"
    tmp = _plat_temp() / fname
    print(f"[gcl] Python {fname} indiriliyor", flush=True)
    urllib.request.urlretrieve(url, tmp)
    with tarfile.open(tmp, "r:gz") as t:
        # filter: symlink/hardlink'leri atla (WSL/DrvFs ELOOP koruması)
        t.extractall(_plat_temp(), filter=_tar_filter_drop_symlinks)
    tmp.unlink()
    extracted = _plat_temp() / "python"
    if extracted.exists() and not find_python_dir().exists():
        _ensure_python_so(extracted)
        extracted.rename(PYTHON_DIR)
        print(f"[gcl] Python embed dizini taşındı: {PYTHON_DIR}", flush=True)
    _ensure_python_so(find_python_dir())
    print(f"[gcl] Python embed hazır: {find_python_dir()}", flush=True)


def download_freefont() -> None:
    if (FREEFONT_DIR / "FreeMono.ttf").exists():
        print(f"[gcl] FreeFont hazır: {FREEFONT_DIR}", flush=True)
        return
    import urllib.request
    import zipfile
    TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    FREEFONT_DIR.mkdir(parents=True, exist_ok=True)
    tmp_zip = _plat_temp() / "freefont.zip"
    tmp_dir = _plat_temp() / "freefont_tmp"
    print(f"[gcl] GNU FreeFont indiriliyor", flush=True)
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
    print(f"[gcl] FreeFont indirildi: {FREEFONT_DIR}", flush=True)


def embed_font() -> Path:
    if not (FREEFONT_DIR / "FreeMono.ttf").exists():
        download_freefont()
    data = (FREEFONT_DIR / "FreeMono.ttf").read_bytes()
    with open(FREEFONT_EMBED, "w", encoding="utf-8") as f:
        f.write("/* generated: FreeMono.ttf embedded byte array */\n")
        f.write("const unsigned char gcl_embed_freemono_ttf[] = {\n")
        for i in range(0, len(data), 12):
            f.write("    " + ",".join(str(b) for b in data[i:i + 12]) + ",\n")
        f.write("    0\n};\n")
        f.write(f"const unsigned int gcl_embed_freemono_ttf_size = {len(data)};\n")
    print(f"[gcl] font embedded: {FREEFONT_EMBED}", flush=True)
    return FREEFONT_EMBED


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
        print(f"[gcl] Raylib kütüphanesi hazır: {lib}", flush=True)
        return RAYLIB_SRC
    # Uyumsuz platform arşivi (örn. Windows mingw .a, Linux'ta) → hem .a hem .o sil,
    # yerel derle. MinGW objeleri kaldığı sürece `make` onları yeniden arşivler ve
    # sonuç yine COFF olur (R_AMD64_IMAGEBASE).
    if lib.exists():
        for old in list(RAYLIB_SRC.glob("libraylib*.a")) + list(RAYLIB_SRC.glob("*.o")):
            try:
                old.unlink()
                print(f"[gcl] uyumsuz Raylib dosyası silindi: {old.name}", flush=True)
            except OSError:
                pass
    make_tool = "mingw32-make" if os_name() == "windows" else "make"
    run([make_tool, "clean", "PLATFORM=PLATFORM_DESKTOP", "RAYLIB_LIBTYPE=STATIC"], cwd=RAYLIB_SRC)
    run([make_tool, "PLATFORM=PLATFORM_DESKTOP", "RAYLIB_LIBTYPE=STATIC"], cwd=RAYLIB_SRC)
    stems = list(RAYLIB_SRC.glob("libraylib*.a"))
    if stems:
        print(f"[gcl] Raylib derlendi: {stems[0]}", flush=True)
    return RAYLIB_SRC


def normalize_nix_py_lib(py_lib):
    """Linux libpython adını -l flag'ine çevir:
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
            # Windows embed lib'leri (.lib) — yalnızca Windows'ta.
            if os_name() == "windows":
                libs_dir = py_dir / "libs"
                if libs_dir.exists():
                    for name in sorted(p.name for p in libs_dir.glob("python*.lib")):
                        if (libs_dir / name).exists():
                            return str(inc), str(libs_dir), name
    try:
        py_inc = sysconfig.get_path('include')
        py_libdir = sysconfig.get_config_var('LIBDIR')
        # Linux: paylaşımlı lib (LDLIBRARY) tercih et; yoksa LIBRARY (statik).
        py_lib = sysconfig.get_config_var('LDLIBRARY') or sysconfig.get_config_var('LIBRARY')
        if not py_libdir:
            py_libdir = sysconfig.get_config_var('LIBPL')
        if not py_inc or not py_libdir or not py_lib:
            return None, None, None
        if os_name() != "windows":
            py_lib = normalize_nix_py_lib(py_lib)
        # build-standalone embed include dizini: /usr/include/python3.14 şeklindedir;
        # burada sysconfig zaten doğru yolu verir.
        return py_inc, py_libdir, py_lib
    except Exception:
        return None, None, None


def build_modules(build_dir: Path) -> None:
    """simple_doc.md: Library/Math.dll|.so, Stdio.dll|.so, Embed.dll|.so"""
    lib_dir = build_dir / "Library"
    lib_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()

    py_inc, py_libdir, py_lib = python_config()

    raylib_src = build_raylib() if os_name() == "windows" else None
    for name, src in MODULES:
        out = lib_dir / f"{name}.{ext}"
        sources = [src]

        # Python: Linux'ta libpython ile link (GCL_HAVE_PYTHON),
        # Windows'ta MSVC .lib MinGW ile uyumsuz olduğu için dinamik
        # yükleme (GCL_EMBED_PYTHON_DYNAMIC) kullanılır. Her iki durumda da
        # Python.h include path'i gereklidir.
        use_python_link = (os_name() != "windows") and py_inc and py_libdir and py_lib
        use_python_dynamic = (os_name() == "windows") and py_inc

        # Embed.dll: Lua/Python runtime + LuaRaylib/LuaRaygui binding'lerini içerir.
        # gcl -luarun / -pyrun artık Embed.dll'den dinamik yüklenir (gcl_main.c).
        cmd_get_raylib = False
        if name == "Embed":
            sources += ["src/embed/gcl_embed_lua.c",
                        "src/embed/gcl_luaraylib.c",
                        "src/embed/gcl_luaraygui.c"]
            if use_python_link or use_python_dynamic:
                sources += ["src/embed/gcl_embed_python.c"]
            # Lua kaynaklarını statik olarak göm (liblua.a build edilmiyor).
            # onelua.c (amalgamasyon), lua.c (standalone yorumlayıcı),
            # luac.c (derleyici) ve ltests.c (test) — çift tanım hataları verir.
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
        cmd += ["-I", "src/include",
                "-I", "src/embed",
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
        # Python embed lib'ini yalnızca doğrudan link modunda bağla
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
        # Embed.dll: LuaRaylib/LuaRaygui raylib fonksiyonlarına ihtiyaç duyar →
        # raylib link et. Windows'ta statik libraylib.a kullan (modüller hâlâ import lib).
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
            # Embed.so statik libraylib.a bağlar → X11/GL sistem kütüphaneleri gerekli.
            # Eksikse dlopen "undefined symbol: XFree" hatası verir.
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
           "-I", "src/include",
           "-I", str(RAYLIB_SRC),
           "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
           "src/Modules/gcl_raylib.c",
           str(libs[0]),
           "-o", str(out)]
    if os_name() == "windows":
        cmd += ["-Wl,--export-all-symbols", "-lwinmm", "-lgdi32", "-lopengl32", "-luser32", "-lshell32", "-lole32"]
    else:
        cmd += ["-lGL", "-lpthread", "-ldl", "-lrt", "-lX11"]
    cmd += ["-lm"]
    run(cmd, cwd=ROOT)
    print(f"[gcl] modül: {out}", flush=True)

    # Raylib.dll'den import library üret (Raygui/LuaRaylib/PyRaylib tek raylib instance kullansın).
    # Aksi halde Raygui.dll kendi libraylib.a'sını gömer → ikinci, başlatılmamış raylib state → crash.
    if os_name() == "windows":
        imp_lib = lib_dir / "libraylib.a"
        if out.exists() and not imp_lib.exists():
            import subprocess
            # gendef - <dll> → stdout'a .def dökümü. Bunu lib_dir/raylib.def'e yaz.
            r1 = subprocess.run(["gendef", "-", str(out)], capture_output=True, text=True)
            def_content = r1.stdout
            if def_content:
                def_path = lib_dir / "raylib.def"
                def_path.write_text(def_content, encoding="utf-8")
                r2 = subprocess.run(["dlltool", "-d", str(def_path), "-l", str(imp_lib), "-D", str(out)],
                                    capture_output=True, text=True)
                if imp_lib.exists():
                    print(f"[gcl] import lib üretildi: {imp_lib}", flush=True)
                else:
                    print(f"[gcl] dlltool çıktısı: {r2.stderr or r2.stdout}", flush=True)
            else:
                print(f"[gcl] gendef çıktı yok: {r1.stderr}", flush=True)


def build_raygui_module(build_dir: Path) -> None:
    """simple_doc.md: Library/Raygui.dll|.so — gcl_raygui.c + raygui.h."""
    lib_dir = build_dir / "Library"
    lib_dir.mkdir(parents=True, exist_ok=True)
    ext = dll_ext()
    out = lib_dir / f"Raygui.{ext}"
    # gcl_raygui.c: raygui.h'i RAYGUI_IMPLEMENTATION ile derler.
    # Raygui, raylib fonksiyonlarına ihtiyaç duyar → Raylib.dll'e link et (import lib).
    # Aksi halde kendi libraylib.a'sını gömer → ikinci, başlatılmamış raylib state → crash.
    raylib_stack_libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    import_lib = lib_dir / "libraylib.a"
    # GCL native Raygui.dll: Raylib.dll ile AYNI process'te yüklenir → import lib kullan
    # (tek raylib state). Aksi halde kendi libraylib.a'sını gömer → ikinci, başlatılmamış
    # raylib state → crash.
    raylib_link = import_lib if (os_name() == "windows" and import_lib.exists()) else (raylib_stack_libs[0] if raylib_stack_libs else None)
    cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
           "-DRAYGUI_IMPLEMENTATION",
           "-I", "src/include",
           "-I", str(RAYGUI_SRC),
           "-I", str(RAYLIB_SRC),
           "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
           "src/Modules/gcl_raygui.c",
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

    # LuaRaylib / LuaRaygui — gerçek Lua C modülleri (gcl_luaraylib.c / gcl_luaraygui.c).
    # luaopen_LuaRaylib / luaopen_LuaRaygui export edilir.
    # NOT: raygui.h header-only → gcl_luaraygui.c'de RAYGUI_IMPLEMENTATION gerekli.
    # Lua kaynaklarını her modüle statik göm (ayrı lua_State pointer'ı paylaşılır).
    # Windows'ta Raylib.dll'e import lib ile bağlan (tek raylib instance); Linux'ta statik arşiv.
    raylib_libs = list(RAYLIB_SRC.glob("libraylib*.a"))
    # Statik arşiv: raw raylib fonksiyonları (DrawGrid, DrawCube, Audio...) import lib'de yok.
    raylib_link = raylib_libs[0] if raylib_libs else None
    for src_base, out_name, needs_raygui in (
        ("gcl_luaraylib", "LuaRaylib", False),
        ("gcl_luaraygui", "LuaRaygui", True),
    ):
        out_dll = embed_dir / f"{out_name}.{ext}"
        cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
               "-I", "src/embed",
               "-I", str(LUA_SRC),
               "-I", str(RAYLIB_SRC),
               "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
               "-I", str(RAYGUI_SRC)]
        if needs_raygui:
            cmd += ["-DRAYGUI_IMPLEMENTATION"]
        cmd += [f"src/embed/{src_base}.c"]
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
    # Python/ kopyası (stdlib)
    py_copy = embed_dir / "Python"
    if py_dir.exists() and not py_copy.exists():
        shutil.copytree(py_dir, py_copy, ignore=shutil.ignore_patterns("__pycache__"))
        print(f"[gcl] Python/ kopyalandı: {py_copy}", flush=True)
    # Python.dll/.so — mevcut python embed DLL'ini kopyala (Windows) veya libpython link et
    if os_name() == "windows" and py_dir.exists():
        dlls = sorted(py_dir.glob("python*.dll"), key=lambda p: p.name)
        if dlls:
            src_dll = dlls[-1]
            out = embed_dir / f"Python.{ext}"
            shutil.copy2(src_dll, out)
            print(f"[gcl] modül: {out} (kopyalandı)", flush=True)
    else:
        # Linux: Python.dll yerine libpython.so kopyala veya link et.
        # py_lib "python3.14" normalize adı; libpython3.14.so.1.0 ile doğrudan eşleşmez.
        # Bu yüzden lib dizinindeki gerçek libpython*.so* dosyasını bul.
        py_inc, py_libdir, py_lib = python_config()
        if py_libdir:
            lib_candidates = list(Path(py_libdir).glob("libpython*.so*")) + list(Path(py_libdir).glob("libpython*.a"))
            if lib_candidates:
                src_lib = lib_candidates[0]
                out = embed_dir / f"Python.{ext}"
                shutil.copy2(src_lib, out)
                print(f"[gcl] modül: {out} (kopyalandı)", flush=True)
                # Embed.so/gcl.so vb. libpython3.14.so.1.0 adına DT_NEEDED bağımlılığı
                # taşır. Orijinal adıyla da kopyala — yoksa dlopen "unknown module" verir.
                for lib_so in lib_candidates:
                    if lib_so.name.startswith("libpython"):
                        orig_out = embed_dir / lib_so.name
                        if not orig_out.exists():
                            shutil.copy2(lib_so, orig_out)
                            print(f"[gcl] modül: {orig_out} (orijinal ad kopyalandı)", flush=True)

    # PyRaylib / PyRaygui — gerçek Python C extension modülleri.
    # Python tarafında `import raylib` / `import raygui` ile yüklenir.
    # NOT: raygui.h header-only → gcl_pyraygui.c'de RAYGUI_IMPLEMENTATION gerekli.
    # Windows'ta Raylib.dll'e import lib ile bağlan (tek raylib instance); Linux'ta statik arşiv.
    py_inc, py_libdir, py_lib = python_config()
    import_lib = lib_dir / "libraylib.a"
    # gcl.pyd — Python tarafında `import gcl` + `gcl.init()` için.
    # Raylib'e bağlanmaz (safe) ama Python C-API'sine bağlanır.
    ext_suffix = "pyd" if os_name() == "windows" else "so"
    gcl_out = embed_dir / f"gcl.{ext_suffix}"
    gcl_cmd = ["gcc", "-std=c99", "-shared", "-fPIC", "-D_POSIX_C_SOURCE=200809L",
               "-I", "src/embed"]
    if py_inc:
        gcl_cmd += ["-I", str(py_inc)]
    gcl_cmd += ["src/embed/gcl_pygcl.c", "-o", str(gcl_out)]
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
               "-I", "src/embed",
               "-I", str(RAYLIB_SRC),
               "-I", str(RAYLIB_SRC / "external" / "glfw" / "include"),
               "-I", str(RAYGUI_SRC)]
        if needs_raygui:
            cmd += ["-DRAYGUI_IMPLEMENTATION"]
        if py_inc:
            cmd += ["-I", str(py_inc)]
        cmd += [f"src/embed/{src_base}.c", "-o", str(out_mod)]
        if raylib_link:
            cmd += [str(raylib_link)]
        if os_name() == "windows" and py_libdir and py_lib:
            cmd += ["-L", str(py_libdir)]
            # MinGW: python314.lib MSVC import lib OK; fallback: "-lpython314"
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

    # simple_doc.md: PyRaylib.dll|.so / PyRaygui.dll|.so — aynı modülün kopyası.
    lib_dir = build_dir / "Library"
    for dep_name, out_name in (("Raylib", "PyRaylib"), ("Raygui", "PyRaygui")):
        src_dll = lib_dir / f"{dep_name}.{ext}"
        out_dll = embed_dir / f"{out_name}.{ext}"
        if src_dll.exists() and not out_dll.exists():
            shutil.copy2(src_dll, out_dll)
            print(f"[gcl] modül: {out_dll} (kopyalandı)", flush=True)


def build_ide(build_dir: Path) -> None:
    """simple_doc.md: Programs/ide.dll|.so — IDE ayrı DLL olarak derlenir.

    IDE, gcl.exe'den ayrılır; gcl.exe küçülür. IDE kendi raylib instance'ıyla
    pencereyi açar (gcl_ide_run). Embed runtime'lar (Lua/Python) CLI'da kaldığı
    için burada gerekmez; yalnızca raylib + gcl_os + LSP + IDE kaynakları derlenir.
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
           "-I", "src/IDE", "-I", "src/Build",
           "-I", "src/SharedPipeline", "-I", "src/GCL/SimpleRunner",
           "-I", "src/Modules", "-I", "src/include", "-I", "src/embed",
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
    raylib_src = build_raylib()

    build_dir = BUILD_ROOT / os_name()
    build_dir.mkdir(parents=True, exist_ok=True)
    exe = build_dir / exe_name()
    build_modules(build_dir)
    build_raylib_module(build_dir)
    build_raygui_module(build_dir)
    build_lua_runtime(build_dir)
    build_python_runtime(build_dir)
    # IDE kaynakları eksikse build'i KIRMA (CI geçsin) ama NET UYARI ver.
    # Programs/ide.so ancak language/src/IDE/ + src/Build/ kaynakları repo'da
    # olduğunda üretilir. embed_freemono.c OTOMATİK üretilir (embed_font());
    # CI'da git'te olmasa bile build sırasında oluşur — o yüzden hariç tutulur.
    missing_ide = [s for s in IDE_SRCS
                   if s != "src/embed_freemono.c" and not (ROOT / s).exists()]
    if not missing_ide:
        build_ide(build_dir)
    else:
        print(f"[gcl] UYARI: IDE kaynakları eksik ({len(missing_ide)}/{len(IDE_SRCS)} dosya) — Programs/ üretilmedi (IDE yok).", file=sys.stderr)
        for m in missing_ide:
            print(f"  - {m}", file=sys.stderr)
        print("[gcl] UYARI: IDE çıktısı için eksik dosyalar repo'ya eklenmeli.", file=sys.stderr)

    assets_dir = build_dir / "Assets"
    assets_dir.mkdir(parents=True, exist_ok=True)
    if (FREEFONT_DIR / "FreeMono.ttf").exists():
        shutil.copy2(FREEFONT_DIR / "FreeMono.ttf", assets_dir / "FreeMono.ttf")
        print(f"[gcl] font kopyalandı: {assets_dir / 'FreeMono.ttf'}", flush=True)

    if os_name() == "windows":
        for old_dll in list(build_dir.glob("python*.dll")):
            try:
                old_dll.unlink()
                print(f"[gcl] eski Python DLL silindi: {old_dll}", flush=True)
            except OSError:
                pass

    res_file = None
    rc_file = ROOT / "gcl.rc"
    if os_name() == "windows" and rc_file.exists():
        res_file = build_dir / "gcl.res"
        run(["windres", str(rc_file), "-O", "coff", "-o", str(res_file)], cwd=ROOT)

    # --- Seçenek B: gcl.exe SADECE GCL dili (~400KB) ---
    # Lua/Python runtime + raylib + tüm binding'ler gcl.exe'den ÇIKARILDI.
    # gcl -luarun / -pyrun artık Library/Embed.dll ve Library/Embeded/*.dll'den
    # DİNAMİK yükler (gcl_main.c'de). O DLL'ler yoksa çalışmaz.
    cmd = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
           "-Wno-unused-function", "-Wno-unused-variable", "-Wno-format-truncation",
           "-Wno-discarded-qualifiers",
           "-I", "src/IDE",
           "-I", "src/Build",
           "-I", "src/SharedPipeline",
           "-I", "src/GCL/SimpleRunner",
           "-I", "src/Modules",
           "-I", "src/include",
           "-I", "src/embed",
           "-D_POSIX_C_SOURCE=200809L"]
    # CI'da src/Build/*.c repo'da yok → gcBundle kaynaklarını atla ve
    # gcl_main.c'de -DGCL_SKIP_BUNDLE ile bundle kodunu devre dışı bırak.
    gcl_srcs = [s for s in GCL_SRCS if (ROOT / s).exists()]
    if not (ROOT / "src" / "Build" / "gcbundle_reader.c").exists():
        cmd += ["-DGCL_SKIP_BUNDLE"]
    cmd += gcl_srcs
    # gcl_runner.c fmod kullanır → libm gerekli (Linux makefile'sinde -lm eklenmeli,
    # Windows'ta -lm MinGW'de sorunsuz ama zarar yok).
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


def main() -> int:
    build_dir = build_gcl()
    print(f"[gcl] çıktı: {build_dir}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as e:
        print(f"[gcl] hata: {e}", file=sys.stderr)
        sys.exit(1)
