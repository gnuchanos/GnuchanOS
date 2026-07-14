#!/usr/bin/env python3
"""
build_raylib_raygui_binding.py — GCL Lua 5.4 binding for raylib + raygui

Builds TWO separate DLLs:
  _build/gcl_raylib.dll   (raylib bindings only)
  _build/gcl_raygui.dll   (raygui bindings only, links to gcl_raylib's import lib)

Dependencies (already present, no git clone):
  - raylib_source/src/  (raylib headers + sources)
  - raygui_source/src/  (raygui header + implementation)
  - lua_source/         (Lua 5.4 headers)
  - language/_output_test/dll/lua55.dll  (prebuilt Lua DLL)

Usage: python build_raylib_raygui_binding.py
"""

import os, sys, subprocess, shutil

BASE        = os.path.dirname(os.path.abspath(__file__))
SRC         = os.path.join(BASE, "src")
OUT         = os.path.join(BASE, "_build")
RAYLIB_SRC  = os.path.join(BASE, "raylib_source")
RAYGUI_SRC  = os.path.join(BASE, "raygui_source")
LUA_DIR     = os.path.join(BASE, "lua_source")

CC   = os.environ.get("CC", "gcc")
CFLAGS = "-std=c99 -g -O2 -DPLATFORM_DESKTOP -w".split()

def find_raylib_inc():
    d = os.path.join(RAYLIB_SRC, "src")
    return d if os.path.isfile(os.path.join(d, "raylib.h")) else None

def find_lua_inc():
    return LUA_DIR if os.path.isfile(os.path.join(LUA_DIR, "lua.h")) else None

def find_raygui_inc():
    d = os.path.join(RAYGUI_SRC, "src")
    return d if os.path.isfile(os.path.join(d, "raygui.h")) else None

def find_lua_dll():
    candidates = [
        os.path.join(OUT, "lua55.dll"),
        os.path.normpath(os.path.join(BASE, "..", "_output_test", "dll", "lua55.dll")),
        os.path.normpath(os.path.join(BASE, "..", "..", "language", "_output_test", "dll", "lua55.dll")),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return None

def ensure_lua_dll_in_build():
    dst = os.path.join(OUT, "lua55.dll")
    if os.path.isfile(dst):
        print(f"[build] lua55.dll already in _build/")
        return dst
    src = find_lua_dll()
    if src is None:
        print("[build] ERROR: lua55.dll not found!")
        sys.exit(1)
    print(f"[build] Copying lua55.dll: {src} -> {dst}")
    shutil.copy2(src, dst)
    return dst

def run(cmd, desc=None):
    if desc: print(f"[build] {desc}")
    print(f"[build] {' '.join(cmd)}")
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print(f"[build] FAILED (code {r.returncode})")
        sys.exit(r.returncode)

def build_raylib_static(raylib_inc):
    print("\n[build] === Building libraylib.a ===")
    obj_dir = os.path.join(OUT, "raylib_obj")
    os.makedirs(obj_dir, exist_ok=True)
    c_files = [f for f in os.listdir(raylib_inc) if f.endswith(".c") and f != "rglfw.c"]
    objs = []
    for cf in sorted(c_files):
        obj = os.path.join(obj_dir, cf.replace(".c", ".o"))
        objs.append(obj)
        if not os.path.isfile(obj) or os.path.getmtime(os.path.join(raylib_inc, cf)) > os.path.getmtime(obj):
            run([CC] + CFLAGS + ["-c", "-o", obj, os.path.join(raylib_inc, cf)], f"compile {cf}")
    lib = os.path.join(OUT, "libraylib.a")
    run(["ar", "rcs", lib] + objs, "archive libraylib.a")
    return lib

def main():
    os.makedirs(OUT, exist_ok=True)

    raylib_inc = find_raylib_inc()
    lua_inc    = find_lua_inc()
    raygui_inc = find_raygui_inc()

    if not raylib_inc:
        print("[build] ERROR: raylib.h not found at raylib_source/src/")
        sys.exit(1)
    if not lua_inc:
        print("[build] ERROR: lua.h not found at lua_source/")
        sys.exit(1)

    ensure_lua_dll_in_build()
    lua_dll = os.path.join(OUT, "lua55.dll")

    raylib_lib = os.path.join(OUT, "libraylib.a")
    if not os.path.isfile(raylib_lib):
        raylib_lib = build_raylib_static(raylib_inc)
    else:
        print(f"[build] Using existing {raylib_lib}")

    inc = ["-I", raylib_inc]
    inc += ["-I", lua_inc]
    rglfw = os.path.join(raylib_inc, "rglfw.c")

    # =====================================================================
    # 1) BUILD gcl_raylib.dll (raylib bindings ONLY)
    # =====================================================================
    print("\n[build] === Building gcl_raylib.dll (raylib only) ===")
    raylib_src = [os.path.join(SRC, "gcl_raylib_binding.c")]
    if os.path.isfile(rglfw):
        raylib_src.append(rglfw)

    raylib_implib = os.path.join(OUT, "libgcl_raylib.a")  # import library
    cmd = [CC] + CFLAGS + inc + [
        "-shared",
        "-o", os.path.join(OUT, "gcl_raylib.dll"),
    ] + raylib_src + [
        "-Wl,--export-all-symbols",
        "-Wl,--out-implib," + raylib_implib,
        raylib_lib, lua_dll,
        "-lopengl32", "-lgdi32", "-lwinmm"
    ]
    run(cmd, "link gcl_raylib.dll")

    # =====================================================================
    # 2) BUILD gcl_raygui.dll (raygui bindings ONLY, links to gcl_raylib)
    # =====================================================================
    if raygui_inc:
        print("\n[build] === Building gcl_raygui.dll (raygui only) ===")
        inc_raygui = ["-I", raylib_inc, "-I", raygui_inc, "-I", lua_inc]
        raygui_src = [
            os.path.join(SRC, "gcl_raygui_binding.c"),
            os.path.join(SRC, "raygui_impl.c"),
        ]
        cmd = [CC] + CFLAGS + inc_raygui + [
            "-shared",
            "-o", os.path.join(OUT, "gcl_raygui.dll"),
        ] + raygui_src + [
            "-Wl,--export-all-symbols",
            raylib_implib,   # import symbols from gcl_raylib.dll
            lua_dll,         # Lua API symbols
            "-lopengl32", "-lgdi32", "-lwinmm"
        ]
        run(cmd, "link gcl_raygui.dll")
    else:
        print("[build] WARNING: raygui headers not found, skipping gcl_raygui.dll")

    # =====================================================================
    # 3) Copy to language/_output_test/dll/
    # =====================================================================
    dst_dir = os.path.normpath(os.path.join(BASE, "..", "_output_test", "dll"))
    if os.path.isdir(dst_dir):
        for dll in ["gcl_raylib.dll", "gcl_raygui.dll"]:
            src_path = os.path.join(OUT, dll)
            if os.path.isfile(src_path):
                shutil.copy2(src_path, os.path.join(dst_dir, dll))
                print(f"[build] Copied {dll} to {dst_dir}")

    print(f"\n[build] DONE! Output in: {OUT}")
    for f in sorted(os.listdir(OUT)):
        fp = os.path.join(OUT, f)
        if os.path.isfile(fp) and (f.endswith(".dll") or f.endswith(".a")):
            print(f"[build]   {f:30s} {os.path.getsize(fp):>8,} bytes")

if __name__ == "__main__":
    main()
