#!/usr/bin/env python3
"""
build.py — gcLang build script (like Makefile)

Usage:
    python build.py              build gcl.exe (default)
    python build.py gcl          build main gcl.exe
    python build.py test-flags   build flag test
    python build.py test-lua     build Lua embed test
    python build.py all          build all targets
    python build.py clean        remove build artifacts

Flags:
    --cc=...       compiler (default: gcc)
    --cflags=...   extra compiler flags
    --out=...      output directory (default: current dir)
"""

import sys
import os
import subprocess
import glob

# ── Configuration ────────────────────────────────────────────────
CC = "gcc"
CFLAGS = "-std=c99 -Wall -Wextra -pedantic -g"
OUTDIR = "_output_test"
SRCDIR = "src"
TESTDIR = "_output_test"

def run(cmd, cwd=None):
    print(f"[build] {' '.join(cmd)}")
    r = subprocess.run(cmd, cwd=cwd)
    if r.returncode != 0:
        print(f"[build] FAILED (exit={r.returncode})")
        sys.exit(r.returncode)

# ── Source lists ─────────────────────────────────────────────────
FLAG_SRCS = [
    "src/flags/flag_registry.c",
    "src/flags/flag_version.c",
    "src/flags/flag_help.c",
    "src/flags/flag_run.c",
    "src/flags/flag_build.c",
    "src/flags/flag_lexer.c",
    "src/flags/flag_parser.c",
    "src/flags/flag_ast.c",
    "src/flags/flag_ir.c",
    "src/flags/flag_codegen.c",
    "src/flags/flag_all_flags.c",
    "src/flags/flag_linclude.c",
    "src/flags/flag_llib.c",
    "src/flags/flag_lextend.c",
    "src/flags/flag_wasm.c",
    "src/flags/flag_debug.c",
    "src/flags/flag_luarun.c",
]

LUAEMBED_SRCS = [
    "src/luarun_from_dll_so/lua55_embed.c",
    "src/luarun_from_dll_so/lua_loader.c",
    "src/luarun_from_dll_so/lua_runner.c",
    "src/luarun_from_dll_so/run_lua.c",
]

GCL_SRCS = FLAG_SRCS + LUAEMBED_SRCS

# ── Targets ──────────────────────────────────────────────────────
def build_gcl():
    print("─── build: gcl.exe ───")
    objs = []
    for s in GCL_SRCS:
        obj = s.replace("/", "_").replace(".c", ".o")
        if not os.path.isfile(s):
            print(f"[build] SKIP (not found): {s}")
            continue
        run([CC, *CFLAGS.split(), "-Isrc", "-c", s, "-o", obj])
        objs.append(obj)
    if not os.path.isfile("src/main.c"):
        print("[build] ERROR: src/main.c not found")
        sys.exit(1)
    run([CC, *CFLAGS.split(), "-Isrc", "-c", "src/main.c", "-o", "main.o"])
    objs.append("main.o")
    out = os.path.join(OUTDIR, "gcl.exe")
    run([CC, *CFLAGS.split(), *objs, "-o", out])
    print(f"[build] → {out}")
    for o in objs:
        try: os.remove(o)
        except: pass

def build_test_flags():
    test_src = os.path.join(TESTDIR, "test_flags.c")
    if not os.path.isfile(test_src):
        print(f"[build] SKIP test_flags: {test_src} not found")
        return
    print("─── build: test_flags.exe ───")
    objs = []
    for s in FLAG_SRCS:
        obj = s.replace("/", "_").replace(".c", ".o")
        run([CC, *CFLAGS.split(), "-Isrc", "-c", s, "-o", obj])
        objs.append(obj)
    run([CC, *CFLAGS.split(), "-Isrc", "-c", test_src, "-o", "test_flags.o"])
    objs.append("test_flags.o")
    out = os.path.join(TESTDIR, "test_flags.exe")
    run([CC, *CFLAGS.split(), *objs, "-o", out])
    print(f"[build] → {out}")
    for o in objs:
        try: os.remove(o)
        except: pass

def build_test_lua():
    test_src = os.path.join(TESTDIR, "test_lua_embed.c")
    if not os.path.isfile(test_src):
        print(f"[build] SKIP test_lua: {test_src} not found")
        return
    print("─── build: test_lua_embed.exe ───")
    objs = []
    for s in LUAEMBED_SRCS:
        obj = s.replace("/", "_").replace(".c", ".o")
        run([CC, *CFLAGS.split(), "-Isrc", "-c", s, "-o", obj])
        objs.append(obj)
    run([CC, *CFLAGS.split(), "-Isrc", "-c", test_src, "-o", "test_lua.o"])
    objs.append("test_lua.o")
    out = os.path.join(TESTDIR, "test_lua_embed.exe")
    run([CC, *CFLAGS.split(), *objs, "-o", out])
    print(f"[build] → {out}")
    for o in objs:
        try: os.remove(o)
        except: pass

def clean():
    print("─── clean ───")
    patterns = [
        "*.o", "*.obj", "*.exe",
    ]
    for p in patterns:
        for f in glob.glob(p):
            print(f"  rm {f}")
            try: os.remove(f)
            except: pass
    test_exe = os.path.join(TESTDIR, "*.exe")
    for f in glob.glob(test_exe):
        print(f"  rm {f}")
        try: os.remove(f)
        except: pass

# ── Main ─────────────────────────────────────────────────────────
if __name__ == "__main__":
    targets = []
    for arg in sys.argv[1:]:
        if arg.startswith("--cc="):
            CC = arg.split("=", 1)[1]
        elif arg.startswith("--cflags="):
            CFLAGS = arg.split("=", 1)[1]
        elif arg.startswith("--out="):
            OUTDIR = arg.split("=", 1)[1]
        elif arg == "--help" or arg == "-h":
            print(__doc__)
            sys.exit(0)
        else:
            targets.append(arg)

    if not targets:
        targets = ["gcl"]

    for t in targets:
        if t == "all":
            build_gcl()
            build_test_flags()
            build_test_lua()
        elif t == "gcl":
            build_gcl()
        elif t == "test-flags":
            build_test_flags()
        elif t == "test-lua":
            build_test_lua()
        elif t == "clean":
            clean()
        else:
            print(f"unknown target: {t}")
            print(__doc__)
            sys.exit(1)

    print("[build] done.")
