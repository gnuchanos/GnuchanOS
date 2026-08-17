#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
makefile.py - GCL + GnuChanIDE build

    python makefile.py           -> FULL BUILD (varsayilan, parametre gerekmez):
                                     gcl + gcl-lsp + Lua embed + Python embed
                                     + bridge + GnuChanIDE (hepsi)
    python makefile.py gcl       -> yalnizca gcl[.exe]
    python makefile.py lua       -> gcl + Lua modulleri
    python makefile.py python    -> gcl + Python modulleri
    python makefile.py ide       -> yalnizca GnuChanIDE

    NOT: "full" parametresi yazmaniza GEREK YOK — hicbir arguman verilmezse
    full build calisir (varsayilan mod "full" dur).

Organize cikti (Windows ornegi) - gnuchanos.md FILE TREE:
    build/windows/
    ├─ gcl.exe                     <- GCL calistirici
    ├─ GnuChanIDE.exe              <- GnuChanIDE (Electron baslatir)
    ├─ GnuChanIDE_JUNKS/           <- GUI LIBRARY (electron runtime copu:
    │                                 dll, pak, locales, resources)
    └─ Library/
       ├─ Lua/
       │  ├─ lua.gcDL              <- Lua 5.4.7 embed
       │  ├─ lua_raylib.gcDL       <- Lua raylib binding
       │  └─ luaLibrary/           <- Lua kullanici kutuphaneleri (.lua, .gcDL)
       └─ Python/
          ├─ python.gcDL           <- Python 3.14 embed
          ├─ python_raylib.gcDL    <- Python raylib binding
          ├─ pyRaylib.py           <- Python raylib yardimcisi
          └─ pyLibrary/            <- Python embed runtime (python314.dll + Lib/)
"""

import os
import platform
import shutil
import subprocess
import sys

argv = sys.argv
ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(ROOT)          # GnuchanOS repo koku
TEMP = os.path.join(REPO, "_temp")    # raylib + Python paketleri
SRC = os.path.join(ROOT, "src")
GCS = os.path.join(SRC, "gcBuild_System")
LUA = os.path.join(GCS, "lua")
LUA_CORE = os.path.join(LUA, "lua-5.4.7", "src")
LUA_RAYLIB = os.path.join(LUA, "raylib")
PY = os.path.join(GCS, "python")
LSP_DIR = os.path.join(SRC, "lsp")
IDE_DIR = os.path.join(ROOT, "src", "ide")
CC = os.environ.get("CC", "gcc")

IS_WINDOWS = platform.system().lower() == "windows"
NPM = "npm.cmd" if IS_WINDOWS else "npm"
NPX = "npx.cmd" if IS_WINDOWS else "npx"
LSP_EXE = "gcl-lsp.exe" if IS_WINDOWS else "gcl-lsp"

# -- GnuChanIDE (Electron) ------------------------------------------------
IDE_EXE = "GnuChanIDE.exe" if IS_WINDOWS else "GnuChanIDE"
IDE_RUNTIME_DIR = "GnuChanIDE_JUNKS"
LAUNCHER_C = os.path.join(IDE_DIR, "electron", "launcher.c")
GEN_REF_PY = os.path.join(IDE_DIR, "tools", "gen_reference.py")
PYTHON = "py.exe" if IS_WINDOWS else "python3"

# -- ikonlar --------------------------------------------------------------
ASSETS = os.path.join(REPO, "assets")
ICO = os.path.join(ASSETS, "logo.ico")
LINUX_ICON_PNG = os.path.join(ASSETS, "logo.png")
RC_DIR = os.path.join(TEMP, "ide")


def build_icon_obj():
    """logo.ico'yu Windows RES objesine cevirir (gcc ile linklenir).
    Yalnizca Windows + ikon varsa. Yoksa None doner (derleme ikonsuz)."""
    if not IS_WINDOWS or not os.path.isfile(ICO):
        return None
    os.makedirs(RC_DIR, exist_ok=True)
    rc = os.path.join(RC_DIR, "app.rc")
    obj = os.path.join(RC_DIR, "app_icon.o")
    # windres .rc icindeki ters cizgili yollari escape sanar
    # (D:\...\logo.ico -> \G gecersiz). Path'i forward slash'a cevir.
    ico_rc = ICO.replace("\\", "/")
    with open(rc, "w", encoding="utf-8") as f:
        f.write('IDI_ICON1 ICON "' + ico_rc + '"\n')
    if subprocess.call(["windres", rc, "-o", obj]) != 0:
        print("makefile.py: warning: windres basarisiz, ikon gomulemedi",
              file=sys.stderr)
        return None
    return obj


def write_linux_desktop(out_dir):
    """Linux icin GnuChanIDE.desktop (ikon + launcher)."""
    if IS_WINDOWS:
        return
    if not os.path.isfile(LINUX_ICON_PNG):
        return
    icon_dst = os.path.join(out_dir, "GnuChanIDE.png")
    shutil.copy2(LINUX_ICON_PNG, icon_dst)
    desktop = os.path.join(out_dir, "GnuChanIDE.desktop")
    with open(desktop, "w", encoding="utf-8") as f:
        f.write("[Desktop Entry]\n")
        f.write("Type=Application\n")
        f.write("Name=GnuChanIDE\n")
        f.write("Comment=GCL + embed (Lua/Python) development environment\n")
        f.write("Exec=" + out_dir + "/GnuChanIDE\n")
        f.write("Icon=" + icon_dst + "\n")
        f.write("Terminal=false\n")
        f.write("Categories=Development;IDE;\n")
    print("makefile.py: GnuChanIDE.desktop -> " + desktop)


# -- gnuchanos.md FILE TREE ------------------------------------------------
LUA_MOD_DIR = "Lua"                 # Library/Lua  (gcDL burada durur)
LUA_LIB_DIR = "luaLibrary"          # Library/Lua/luaLibrary
PY_MOD_DIR = "Python"               # Library/Python (gcDL burada durur)
PY_LIB_DIR = "pyLibrary"            # Library/Python/pyLibrary (embed runtime)
BRIDGE_MOD_DIR = "bridge"           # Library/bridge (cross-language data bridge)
BRIDGE_C = os.path.join(GCS, "gcdl_bridge.c")


# ---------------------------------------------------------------------------
# yardimcilar
# ---------------------------------------------------------------------------

def sh(cmd, what, cwd=None, env=None):
    print("makefile.py: building " + what)
    if subprocess.call(cmd, cwd=cwd, env=env) != 0:
        print("makefile.py: error: " + what + " derlenemedi", file=sys.stderr)
        sys.exit(1)


def need(path, what):
    if not os.path.exists(path):
        print("makefile.py: error: " + what + " bulunamadi: " + path,
              file=sys.stderr)
        sys.exit(1)


def copy_tree(src, dst):
    os.makedirs(dst, exist_ok=True)
    for name in os.listdir(src):
        s = os.path.join(src, name)
        d = os.path.join(dst, name)
        if os.path.isdir(s):
            shutil.copytree(s, d, dirs_exist_ok=True)
        else:
            shutil.copy2(s, d)


# -- raylib ---------------------------------------------------------------
RAYLIB_CLONE = os.path.join(TEMP, "raylib")
RAYLIB_SRC = os.path.join(RAYLIB_CLONE, "src")
RAYLIB_GLFW_INC = os.path.join(RAYLIB_SRC, "external", "glfw", "include")
RAYLIB_BIND_C = os.path.join(LUA_RAYLIB, "gcl_raylib_bind.c")
RAYLIB_CORE_SRCS = ["rcore.c", "rshapes.c", "rtext.c", "rtextures.c",
                    "rmodels.c", "raudio.c"]
RAYLIB_SUPPORT_FLAGS = [
    "-DGRAPHICS_API_OPENGL_33",
    "-DSUPPORT_MODULE_RTEXT", "-DSUPPORT_MODULE_RSHAPES",
    "-DSUPPORT_MODULE_RTEXTURES", "-DSUPPORT_MODULE_RMODELS",
    "-DSUPPORT_MODULE_RAUDIO", "-DSUPPORT_CAMERA_SYSTEM",
    "-DSUPPORT_GESTURES_SYSTEM", "-DSUPPORT_MOUSE_GESTURES",
]


def ensure_raylib():
    if os.path.isfile(os.path.join(RAYLIB_SRC, "raylib.h")):
        return
    os.makedirs(TEMP, exist_ok=True)
    print("makefile.py: cloning raylib -> " + RAYLIB_CLONE)
    if subprocess.call(["git", "clone",
                        "https://github.com/raysan5/raylib.git", RAYLIB_CLONE]) != 0:
        print("makefile.py: error: raylib clone basarisiz", file=sys.stderr)
        sys.exit(1)


# Python embed dev paketi (python-build-standalone):
#     _temp/python_win/   -> python/ (include/ + libs/ + python314.dll + DLLs/ + Lib/)
#     _temp/python_linux/ -> include/python3.14 + lib/
# Windows: EMBED zip (embed-amd64.zip) KULLANILMAZ — o pakette include/ ve
# libs/ HICBIR ZAMAN bulunmaz (CI hatasi: "_temp/python_win/include bulunamadi").
# INSTALL_ONLY .tar.gz tek pakette hem derleme (include/libs) hem calisma
# zamani runtime'ini (python314.dll + DLLs/*.pyd + Lib/) tasir.
# Gercek surum: 3.14.7+20260807 (release adi 20260807).
PY_EMBED_TAR = os.environ.get(
    "PY_EMBED_TAR",
    os.path.join(TEMP, "cpython-3.14.7+20260807-x86_64-unknown-linux-gnu-install_only.tar.gz"),
)
PY_VERSION_TAG = "3.14.7+20260807"
PY_WIN_INSTALL_URL = (
    "https://github.com/astral-sh/python-build-standalone/releases/download/"
    "20260807/cpython-3.14.7+20260807-x86_64-pc-windows-msvc-install_only.tar.gz"
)
PY_LINUX_TAR_URL = (
    "https://github.com/astral-sh/python-build-standalone/releases/download/"
    "20260807/cpython-3.14.7+20260807-x86_64-unknown-linux-gnu-install_only.tar.gz"
)


def python_dev_url():
    """Windows install_only paketi icin stable URL.

    Dikkat: CI'da "_temp/python_win/include bulunamadi" hatasinin kaynagi
    buydu — onceden EMBED zip (embed-amd64.zip) indiriliyordu; o pakette
    yalnizca runtime (python314.dll + python314.zip + *.pyd) vardir,
    include/ ve libs/ HICBIR ZAMAN bulunmaz. Yerelde gizleniyordu cunku
    _temp elle kuruluydu. Python.h + .lib icin INSTALL_ONLY paketi gerekir.
    """
    return os.environ.get("PY_DOWNLOAD_URL", PY_WIN_INSTALL_URL)


def _download_py(url, dst):
    os.makedirs(TEMP, exist_ok=True)
    print("makefile.py: indiriliyor -> " + url)
    if subprocess.call(["curl", "-L", "-o", dst, url]) != 0:
        print("makefile.py: error: python embed indirilemedi", file=sys.stderr)
        sys.exit(1)


def _unpack_tar(tar_path, out_dir, flatten=False):
    os.makedirs(out_dir, exist_ok=True)
    # python-build-standalone tarball'i python/ tepesinde acilir
    if subprocess.call(["tar", "-xzf", tar_path, "-C", out_dir]) != 0:
        print("makefile.py: error: python embed acilamadi", file=sys.stderr)
        sys.exit(1)
    # LINUX: CI'da "_temp/python_linux/include bulunamadi" hatasinin kaynagi —
    # tarball icerigi python/ alt klasorune acilir (python/include, python/lib...)
    # ama linux kodu root'ta include/lib bekler. python/ klasoru kok icine tasinir.
    # WINDOWS'T A UYGULANMAZ: windows kodu python_win/python/ hiyerarsisini bekler
    # (py_inc = python_win/python/include, runtime = python_win/python).
    if flatten:
        nested = os.path.join(out_dir, "python")
        if os.path.isdir(nested):
            for name in os.listdir(nested):
                s = os.path.join(nested, name)
                d = os.path.join(out_dir, name)
                if os.path.exists(d):
                    if os.path.isdir(d):
                        shutil.rmtree(d)
                    else:
                        os.remove(d)
                os.rename(s, d)
            os.rmdir(nested)
            print("makefile.py: python/ flatten -> " + out_dir)


def ensure_python_dev(platform_name):
    """Windows: include/libs/runtime; Linux: include/python3.14 + lib.
    Var olan kuruluma dokunmaz (CI'da bir kez indirilir, local'de korunur)."""
    if platform_name == "windows":
        # TEK PAKET: install_only .tar.gz. EMBED zip KULLANILMAZ (onda
        # include/ + libs/ HICBIR ZAMAN bulunmaz -> CI: "_temp/python_win/
        # include bulunamadi"). install_only icinde:
        #   python/include/      -> Python.h
        #   python/libs/         -> libpython314.a + python314.lib
        #   python/python314.dll + python/DLLs/*.pyd + python/Lib/  (runtime)
        out = os.path.join(TEMP, "python_win")
        need_py = os.path.join(out, "python", "include", "Python.h")
        if os.path.isfile(need_py):
            return
        tar = os.path.join(TEMP, "cpython-win-install_only.tar.gz")
        if not os.path.isfile(tar):
            _download_py(python_dev_url(), tar)
        _unpack_tar(tar, out)
        print("makefile.py: python_win hazir -> " + out)
    else:
        inc = os.path.join(TEMP, "python_linux", "include", "python3.14",
                           "Python.h")
        if os.path.isfile(inc):
            return
        tar = PY_EMBED_TAR
        if not os.path.isfile(tar):
            _download_py(PY_LINUX_TAR_URL, tar)
        out = os.path.join(TEMP, "python_linux")
        # LINUX: tarball python/ tepesine acilir; include/lib root'ta
        # bekleniyor -> flatten (python/ icerigi koke tasinir).
        _unpack_tar(tar, out, flatten=True)
        print("makefile.py: python_linux hazir -> " + out)


# ---------------------------------------------------------------------------
# Windows builder
# ---------------------------------------------------------------------------

class windows:
    def __init__(self):
        self.out = os.path.join(ROOT, "build", "windows")
        self.lib = os.path.join(self.out, "Library")
        self.luamod = os.path.join(self.lib, LUA_MOD_DIR)
        self.lualib = os.path.join(self.luamod, LUA_LIB_DIR)
        self.pymod = os.path.join(self.lib, PY_MOD_DIR)
        self.pylib = os.path.join(self.pymod, PY_LIB_DIR)
        self.pydev = os.path.join(TEMP, "python_win")
        self.bridgemod = os.path.join(self.lib, BRIDGE_MOD_DIR)
        self.luamod_gcdl = self.luamod
        self.pymod_gcdl = self.pymod
        os.makedirs(self.lib, exist_ok=True)

    # -- gcl-lsp (dil sunucusu) -------------------------------------------
    def lsp_BUILD(self):
        """GnuChan dil sunucusu (C): NDJSON stdio. Ikon gomulu."""
        cmd = [CC, "-std=c11", "-O2",
               os.path.join(LSP_DIR, "gcl_lsp.c"),
               os.path.join(LSP_DIR, "python_syntax.c"),
               "-o", os.path.join(self.out, LSP_EXE)]
        if IS_WINDOWS:
            # gcl-lsp.exe de GnuChanIDE ikonunu tasir (build kokundeki
            # gcl.exe ile tutarli gorunum; CI'dan inen pakette ikon yoktu).
            icon_obj = build_icon_obj()
            if icon_obj:
                cmd.append(icon_obj)
        sh(cmd, "gcl-lsp")

    # -- gcl --------------------------------------------------------------
    def gcl_BUILD(self):
        icon_obj = build_icon_obj()
        cmd = [CC, "-std=c11", "-Wall", "-Wextra", "-O2",
               "-I" + SRC, "-I" + GCS,
               os.path.join(ROOT, "main.c"),
               os.path.join(GCS, "gcdl_loader.c"),
               os.path.join(GCS, "gclib_utils.c"),
               "-o", os.path.join(self.out, "gcl.exe"), "-lm", "-lws2_32"]
        if icon_obj:
            cmd.append(icon_obj)
        sh(cmd, "gcl.exe")

    # -- lua --------------------------------------------------------------
    LUA_SRCS = ["lapi.c", "lcode.c", "lctype.c", "ldebug.c", "ldo.c",
                "ldump.c", "lfunc.c", "lgc.c", "llex.c", "lmem.c",
                "lobject.c", "lopcodes.c", "lparser.c", "lstate.c",
                "lstring.c", "ltable.c", "ltm.c", "lundump.c", "lvm.c",
                "lzio.c", "lauxlib.c", "lbaselib.c", "lcorolib.c",
                "ldblib.c", "liolib.c", "lmathlib.c", "loadlib.c",
                "loslib.c", "lstrlib.c", "ltablib.c", "lutf8lib.c",
                "linit.c"]

    def lua_gcDL_BUILD(self):
        os.makedirs(self.luamod, exist_ok=True)
        os.makedirs(self.lualib, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-w", "-shared",
            "-DLUA_USE_WINDOWS", "-I" + LUA_CORE, "-I" + LUA] +
           [os.path.join(LUA_CORE, f) for f in self.LUA_SRCS] +
           [os.path.join(LUA, "gcl_lua_embed.c"),
            "-o", os.path.join(self.luamod, "lua.gcDL"),
            "-lm", "-lws2_32"], "lua.gcDL")

    def raylib_flags(self):
        return [CC, "-std=c11", "-O2", "-w", "-shared",
                "-DPLATFORM_DESKTOP_WIN32"] + RAYLIB_SUPPORT_FLAGS + [
                    "-I" + RAYLIB_SRC, "-I" + RAYLIB_GLFW_INC,
                    "-I" + os.path.join(RAYLIB_SRC, "external")] + \
            [os.path.join(RAYLIB_SRC, n) for n in RAYLIB_CORE_SRCS]

    def raylib_link(self):
        return ["-lopengl32", "-lwinmm", "-lgdi32", "-lws2_32",
                "-lole32", "-lshell32"]

    def lua_raylib_BUILD(self):
        need(RAYLIB_BIND_C, "gcl_raylib_bind.c")
        ensure_raylib()
        os.makedirs(self.luamod, exist_ok=True)
        sh(self.raylib_flags() + [
            "-I" + LUA_CORE, "-I" + LUA_RAYLIB, RAYLIB_BIND_C,
            "-o", os.path.join(self.luamod, "lua_raylib.gcDL")] +
           self.raylib_link(), "lua_raylib.gcDL")

    # -- python -----------------------------------------------------------
    @property
    def py_inc(self):
        # install_only paketi python/ tepesine acilir:
        # python_win/python/include
        return os.path.join(self.pydev, "python", "include")

    @property
    def py_libdir(self):
        return os.path.join(self.pydev, "python", "libs")

    def python_gcDL_BUILD(self):
        need(self.py_inc, "_temp/python_win/include")
        need(self.py_libdir, "_temp/python_win/libs")
        os.makedirs(self.pymod, exist_ok=True)
        os.makedirs(self.pylib, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-w", "-shared",
            "-I" + PY, "-I" + self.py_inc, "-L" + self.py_libdir,
            os.path.join(PY, "gcl_python_embed.c"),
            "-o", os.path.join(self.pymod, "python.gcDL"),
            "-lpython314", "-lm", "-lws2_32"], "python.gcDL")

    def python_runtime_COPY(self):
        """Embed runtime -> Library/Python/pyLibrary/ (gnuchanos.md).

        Kaynak: install_only paketinin python/ kokunde runtime durur:
            python/python314.dll
            python/DLLs/*.pyd          (C uzantilari)
            python/Lib/                (stdlib + site-packages)
        include/ ve libs/ yalnizca derleme icindir -> pyLibrary'ye
        KOPYALANMAZ. Eski kod `python_win/runtime` ariyordu; o klasor
        yalnizca EMBED zip duzenindeydi (CI: "runtime bulunamadi")."""
        need(self.pydev, "_temp/python_win")
        src = os.path.join(self.pydev, "python")
        need(src, "_temp/python_win/python")
        os.makedirs(self.pylib, exist_ok=True)
        for name in os.listdir(src):
            if name in ("include", "libs"):
                continue
            s = os.path.join(src, name)
            d = os.path.join(self.pylib, name)
            if os.path.isdir(s):
                if os.path.exists(d):
                    shutil.rmtree(d)
                shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)
        print("makefile.py: python embed runtime -> Library/Python/pyLibrary")

    def pyraylib_py_COPY(self):
        need(os.path.join(PY, "tools", "gcl_python_raylib.py"),
             "gcl_python_raylib.py")
        os.makedirs(self.pymod, exist_ok=True)
        shutil.copy2(os.path.join(PY, "tools", "gcl_python_raylib.py"),
                     os.path.join(self.pymod, "pyRaylib.py"))

    def python_raylib_BUILD(self):
        need(os.path.join(PY, "gcl_python_raylib.c"), "gcl_python_raylib.c")
        need(self.py_inc, "Python dev basliklari")
        ensure_raylib()
        os.makedirs(self.pymod, exist_ok=True)
        sh(self.raylib_flags() + [
            "-I" + self.py_inc, "-L" + self.py_libdir,
            os.path.join(PY, "gcl_python_raylib.c"),
            "-o", os.path.join(self.pymod, "python_raylib.gcDL"),
            "-lpython314"] + self.raylib_link(), "python_raylib.gcDL")

    # -- bridge (Lua <-> Python veri koprusu) -----------------------------
    def bridge_gcDL_BUILD(self):
        need(BRIDGE_C, "gcdl_bridge.c")
        os.makedirs(self.bridgemod, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-shared",
            BRIDGE_C,
            "-o", os.path.join(self.bridgemod, "bridge.gcDL"),
            "-lws2_32"], "bridge.gcDL")

    # -- IDE --------------------------------------------------------------
    def ide_npm_install(self):
        if os.path.isdir(os.path.join(IDE_DIR, "node_modules")):
            print("makefile.py: node_modules mevcut, npm install atlandi")
            return
        sh([NPM, "install", "--no-audit", "--no-fund"], "IDE npm install",
           cwd=IDE_DIR)

    def ide_build(self):
        self.ide_npm_install()
        sh([NPM, "run", "build"], "IDE renderer + electron", cwd=IDE_DIR)

    def ide_package(self):
        """GnuChanIDE.exe + runtime uretir (imzalamasiz, --dir)."""
        out_dir = os.path.join(IDE_DIR, "release_build")
        unpacked = os.path.join(out_dir, "win-unpacked")
        if os.path.isdir(unpacked):
            shutil.rmtree(unpacked, ignore_errors=True)
        env = dict(os.environ)
        env["CSC_IDENTITY_AUTO_DISCOVERY"] = "false"
        sh([NPX, "electron-builder", "--win", "--dir",
            "--config.directories.output=" + out_dir,
            "--config.win.signAndEditExecutable=false"],
           "GnuChanIDE paketleme (electron-builder)", cwd=IDE_DIR, env=env)
        need(os.path.join(unpacked, IDE_EXE), "paketlenmis GnuChanIDE.exe")
        return unpacked

    def ide_deploy(self):
        """Electron runtime GnuChanIDE_JUNKS/'e, kok icin launcher uretir."""
        unpacked = self.ide_package()
        runtime_dst = os.path.join(self.out, IDE_RUNTIME_DIR)
        if os.path.isdir(runtime_dst):
            shutil.rmtree(runtime_dst, ignore_errors=True)
        os.makedirs(runtime_dst, exist_ok=True)
        try:
            shutil.copy2(os.path.join(unpacked, IDE_EXE),
                         os.path.join(runtime_dst, IDE_EXE))
        except OSError as e:
            print("makefile.py: warning: " + IDE_EXE
                  + " kopyalanamadi (calisan IDE kilitli?):", e, file=sys.stderr)
        for name in os.listdir(unpacked):
            if name == IDE_EXE:
                continue
            s = os.path.join(unpacked, name)
            d = os.path.join(runtime_dst, name)
            try:
                if os.path.isdir(s):
                    shutil.copytree(s, d, dirs_exist_ok=True)
                else:
                    shutil.copy2(s, d)
            except OSError as e:
                print("makefile.py: warning: " + name
                      + " kopyalanamadi (calisan IDE kilitli?):", e,
                      file=sys.stderr)

        launcher_out = os.path.join(self.out, IDE_EXE)
        icon_obj = build_icon_obj()
        if IS_WINDOWS:
            cmd = [CC, "-std=c11", "-O2", "-mwindows",
                   LAUNCHER_C, "-o", launcher_out,
                   "-luser32", "-lkernel32"]
            if icon_obj:
                cmd.append(icon_obj)
            sh(cmd, "GnuChanIDE launcher")
        else:
            sh([CC, "-std=c11", "-O2", LAUNCHER_C,
                "-o", launcher_out], "GnuChanIDE launcher")
        need(launcher_out, "paketlenmis GnuChanIDE launcher")
        write_linux_desktop(self.out)

        print("makefile.py: GnuChanIDE launcher -> " + self.out)
        print("makefile.py: electron runtime (GnuChanIDE_JUNKS) -> "
              + runtime_dst)

    def IDE_build(self):
        self.ide_build()
        self.ide_deploy()

    # -- orkestrasyon -----------------------------------------------------
    def RUN(self):
        """Windows RUN: gcl + lua/python modulleri + bridge + ide."""
        mode = argv[1] if len(argv) > 1 else "full"
        self.lsp_BUILD()
        self.gcl_BUILD()
        if mode in ("full", "lua"):
            self.lua_gcDL_BUILD()
            self.lua_raylib_BUILD()
        if mode in ("full", "python"):
            ensure_python_dev("windows")
            self.python_gcDL_BUILD()
            self.python_runtime_COPY()
            self.pyraylib_py_COPY()
            self.python_raylib_BUILD()
        self.bridge_gcDL_BUILD()
        self.gen_reference()
        if mode in ("full", "ide"):
            self.IDE_build()
        print("makefile.py: build tamam -> " + self.out)
        if len(argv) > 2 and argv[2] == "run":
            exe = os.path.join(self.out, IDE_EXE)
            need(exe, "GnuChanIDE.exe")
            subprocess.Popen([exe], cwd=self.out)

    def gen_reference(self):
        """Library modullerinin .gcReference dosyalarini uretir (IDE icin)."""
        if os.path.isfile(GEN_REF_PY):
            sh([PYTHON, GEN_REF_PY, self.out], "gcReference dosyalari")
        else:
            print("makefile.py: warning: gen_reference.py yok, atlandi")


class gnuLinux(windows):
    """Linux build: gcl, .gcDL moduller (Lua raylib = X11/GL), python3.14
    embed."""

    def __init__(self):
        self.out = os.path.join(ROOT, "build", "gnuLinux")
        self.lib = os.path.join(self.out, "Library")
        self.luamod = os.path.join(self.lib, LUA_MOD_DIR)
        self.lualib = os.path.join(self.luamod, LUA_LIB_DIR)
        self.pymod = os.path.join(self.lib, PY_MOD_DIR)
        self.pylib = os.path.join(self.pymod, PY_LIB_DIR)
        self.pydev = os.path.join(TEMP, "python_linux")
        self.bridgemod = os.path.join(self.lib, BRIDGE_MOD_DIR)
        os.makedirs(self.lib, exist_ok=True)

    # Python dev: `_temp/python_linux` install_only tarball duzeni.
    @property
    def py_inc(self):
        base = os.path.join(self.pydev, "include")
        alt = os.path.join(base, "python3.14")
        return alt if os.path.isdir(alt) else base

    @property
    def py_libdir(self):
        base = os.path.join(self.pydev, "lib")
        alt = os.path.join(self.pydev, "libs")
        return base if os.path.isdir(base) else alt

    # -- gcl ---------------------------------------------------------------
    def gcl_BUILD(self):
        sh([CC, "-std=c11", "-Wall", "-Wextra", "-O2",
            "-I" + SRC, "-I" + GCS,
            os.path.join(ROOT, "main.c"),
            os.path.join(GCS, "gcdl_loader.c"),
            os.path.join(GCS, "gclib_utils.c"),
            "-o", os.path.join(self.out, "gcl"), "-lm", "-ldl"], "gcl")

    # -- lua ---------------------------------------------------------------
    def lua_gcDL_BUILD(self):
        os.makedirs(self.luamod, exist_ok=True)
        os.makedirs(self.lualib, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-fPIC", "-shared",
            "-DLUA_USE_LINUX", "-I" + LUA_CORE, "-I" + LUA] +
           [os.path.join(LUA_CORE, f) for f in self.LUA_SRCS] +
           [os.path.join(LUA, "gcl_lua_embed.c"),
            "-o", os.path.join(self.luamod, "lua.gcDL"),
            "-lm", "-ldl"], "lua.gcDL (linux)")

    def raylib_flags(self):
        # Dikkat: Linux'ta `-I src/external` VERILMEZ. Raylib'in
        # external/dirent.h'i bir Windows uyumluluk basligidir ve <io.h>
        # ister; -I ile sistem <dirent.h> yerine bu dosya yakalanir →
        # "fatal error: io.h: No such file or directory". Raylib'in kendi
        # Linux Makefile'i de yalnizca "-Isrc -Isrc/external/glfw/include"
        # kullanir (external dosyalari "external/miniaudio.h" gibi alt yol
        # ile zaten cagrilir; yalin <dirent.h> sistemden gelir).
        return [CC, "-std=c11", "-O2", "-fPIC", "-shared",
                "-DPLATFORM_DESKTOP",
                "-DGRAPHICS_API_OPENGL_33",
                "-DSUPPORT_MODULE_RTEXT", "-DSUPPORT_MODULE_RSHAPES",
                "-DSUPPORT_MODULE_RTEXTURES", "-DSUPPORT_MODULE_RMODELS",
                "-DSUPPORT_MODULE_RAUDIO", "-DSUPPORT_CAMERA_SYSTEM",
                "-DSUPPORT_GESTURES_SYSTEM", "-DSUPPORT_MOUSE_GESTURES",
                "-I" + RAYLIB_SRC, "-I" + RAYLIB_GLFW_INC] + \
            [os.path.join(RAYLIB_SRC, n) for n in RAYLIB_CORE_SRCS]

    def raylib_link(self):
        return ["-Wl,--no-as-needed", "-lm", "-ldl", "-lpthread", "-lGL",
                "-lX11", "-lXrandr", "-lXi", "-lXcursor"]

    def lua_raylib_BUILD(self):
        need(RAYLIB_BIND_C, "gcl_raylib_bind.c")
        ensure_raylib()
        os.makedirs(self.luamod, exist_ok=True)
        sh(self.raylib_flags() + [
            "-I" + LUA_CORE, "-I" + LUA_RAYLIB, RAYLIB_BIND_C,
            "-o", os.path.join(self.luamod, "lua_raylib.gcDL")] +
           self.raylib_link(), "lua_raylib.gcDL (linux)")

    # -- python ------------------------------------------------------------
    def python_gcDL_BUILD(self):
        need(self.py_inc, "_temp/python_linux/include")
        need(self.py_libdir, "_temp/python_linux/lib")
        os.makedirs(self.pymod, exist_ok=True)
        os.makedirs(self.pylib, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-fPIC", "-shared",
            "-I" + PY, "-I" + self.py_inc, "-L" + self.py_libdir,
            # python.gcDL, Library/Python/ icinden yuklenir; calisma
            # zamaninda libpython3.14.so yan dizindeki pyLibrary/ icinde
            # aranir (gcl_python_embed.c doc'u: "rpaths to $ORIGIN/pyLibrary").
            "-Wl,-rpath,$ORIGIN/pyLibrary",
            os.path.join(PY, "gcl_python_embed.c"),
            "-o", os.path.join(self.pymod, "python.gcDL"),
            "-lpython3.14", "-lm", "-ldl"], "python.gcDL (linux)")

    def python_raylib_BUILD(self):
        need(os.path.join(PY, "gcl_python_raylib.c"), "gcl_python_raylib.c")
        need(self.py_inc, "Python dev basliklari")
        ensure_raylib()
        os.makedirs(self.pymod, exist_ok=True)
        sh(self.raylib_flags() + [
            "-I" + self.py_inc, "-L" + self.py_libdir,
            # python_raylib.gcDL de Library/Python/ icinden yuklenir:
            # libpython3.14.so yan dizindeki pyLibrary/ icinde aranir.
            "-Wl,-rpath,$ORIGIN/pyLibrary",
            os.path.join(PY, "gcl_python_raylib.c"),
            "-o", os.path.join(self.pymod, "python_raylib.gcDL"),
            "-lpython3.14"] + self.raylib_link(), "python_raylib.gcDL (linux)")

    # -- bridge (Linux) ---------------------------------------------------
    def bridge_gcDL_BUILD(self):
        need(BRIDGE_C, "gcdl_bridge.c")
        os.makedirs(self.bridgemod, exist_ok=True)
        sh([CC, "-std=c11", "-O2", "-fPIC", "-shared",
            BRIDGE_C,
            "-o", os.path.join(self.bridgemod, "bridge.gcDL"),
            "-lm", "-ldl"], "bridge.gcDL (linux)")

    def python_runtime_COPY(self):
        """Linux embed runtime -> Library/Python/pyLibrary/.

        install_only tarball duzeni (`_temp/python_linux`):
            lib/libpython3.14.so*  ve  lib/python3.14/  (stdlib + lib-dynload)
          - `runtime/` klasoru LINUX'ta YOKTUR (o, Windows embed zip'inin
            duzenidir — eski kod burada duruyordu):
              fatal: _temp/python_linux/runtime bulunamadi
          - gcl_python_embed.c Linux beklentisi (PY_LIB_DIR = pyLibrary):
              pyLibrary/libpython3.14.so      -> python.gcDL rpath: $ORIGIN/pyLibrary
              pyLibrary/python3.14/           -> stdlib (module_search_paths)"""
        need(self.pydev, "_temp/python_linux")
        libdir = self.py_libdir
        need(libdir, "_temp/python_linux/lib")
        os.makedirs(self.pylib, exist_ok=True)

        # libpython3.14.so*  ->  pyLibrary/
        for name in os.listdir(libdir):
            s = os.path.join(libdir, name)
            if os.path.isfile(s) and name.startswith("libpython"):
                shutil.copy2(s, os.path.join(self.pylib, name))

        # lib/python3.14/ (stdlib)  ->  pyLibrary/python3.14/
        stdlib_src = os.path.join(libdir, "python3.14")
        if os.path.isdir(stdlib_src):
            copy_tree(stdlib_src, os.path.join(self.pylib, "python3.14"))

        print("makefile.py: python embed runtime -> Library/Python/pyLibrary")

    def RUN(self):
        """Linux RUN: gcl + lua/python modulleri + bridge + ide."""
        mode = argv[1] if len(argv) > 1 else "full"
        self.lsp_BUILD()
        self.gcl_BUILD()
        if mode in ("full", "lua"):
            self.lua_gcDL_BUILD()
            self.lua_raylib_BUILD()
        if mode in ("full", "python"):
            ensure_python_dev("linux")
            self.python_gcDL_BUILD()
            self.python_runtime_COPY()
            self.pyraylib_py_COPY()
            self.python_raylib_BUILD()
        self.bridge_gcDL_BUILD()
        if mode in ("full", "ide"):
            self.IDE_build()
        print("makefile.py: build tamam -> " + self.out)
        if len(argv) > 2 and argv[2] == "run":
            exe = os.path.join(self.out, IDE_EXE)
            need(exe, "GnuChanIDE")
            os.spawnv(os.P_NOWAIT, exe, [exe])

    def IDE_build(self):
        self.ide_build()
        unpacked = os.path.join(IDE_DIR, "release", "linux-unpacked")
        if os.path.isdir(unpacked):
            shutil.rmtree(unpacked)
        sh([NPX, "electron-builder", "--linux", "--dir"],
           "GnuChanIDE paketleme (electron-builder)", cwd=IDE_DIR)
        # package.json linux.executableName="GnuChanIDE" (yeni) ya da name
        # fallback "gnuchanide" (eski) — CI'da birini bize dondursun.
        if not os.path.isfile(os.path.join(unpacked, IDE_EXE)):
            for alt in ("gnuchanide", "GnuChanIDE"):
                cand = os.path.join(unpacked, alt)
                if os.path.isfile(cand):
                    shutil.copy2(cand, os.path.join(unpacked, IDE_EXE))
                    break
        need(os.path.join(unpacked, IDE_EXE), "paketlenmis GnuChanIDE")
        runtime_dst = os.path.join(self.out, IDE_RUNTIME_DIR)
        if os.path.isdir(runtime_dst):
            shutil.rmtree(runtime_dst)
        os.makedirs(runtime_dst)
        shutil.copy2(os.path.join(unpacked, IDE_EXE),
                     os.path.join(runtime_dst, IDE_EXE))
        for name in os.listdir(unpacked):
            if name == IDE_EXE:
                continue
            s = os.path.join(unpacked, name)
            d = os.path.join(runtime_dst, name)
            if os.path.isdir(s):
                shutil.copytree(s, d, dirs_exist_ok=True)
            else:
                shutil.copy2(s, d)
        launcher_out = os.path.join(self.out, IDE_EXE)
        sh([CC, "-std=c11", "-O2", LAUNCHER_C,
            "-o", launcher_out], "GnuChanIDE launcher")
        write_linux_desktop(self.out)
        print("makefile.py: GnuChanIDE launcher -> " + self.out)
        print("makefile.py: electron runtime (GnuChanIDE_JUNKS) -> "
              + runtime_dst)


if __name__ == "__main__":
    if IS_WINDOWS:
        windows().RUN()
    else:
        gnuLinux().RUN()
