#!/usr/bin/env python3
"""GnuchanOS Language Compiler (gcl) build script.
Usage:
    python makefile.py           # build gcl.exe
    python makefile.py clean     # remove build artifacts
    python makefile.py run <args># build and run gcl with args
"""

import os
import sys
import subprocess
import glob

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(PROJECT_DIR, "src")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
OUTPUT_EXE = os.path.join(BUILD_DIR, "gcl.exe")

# Source files relative to SRC_DIR
SOURCES = [
    "main.c",
    "common/error.c",
    "common/defines.c",
    "frontend/lexer.c",
    "frontend/parser.c",
    "backend/codegen.c",
    "backend/codegen_ast.c",
    "backend/codegen_ir.c",
    "backend/codegen_c.c",
    "backend/codegen_debug.c",
]

# Include directories relative to SRC_DIR (each gets -I flag)
INCLUDES = [
    ".",
    "types",
    "common",
    "frontend",
    "backend",
]

# Compiler flags
CC = os.environ.get("CC", "gcc")
CFLAGS = os.environ.get("CFLAGS", "-Wall -Wextra -std=c11 -O2").split()
LDFLAGS = os.environ.get("LDFLAGS", "").split()


def cmd_run(cmd):
    print(f"[CMD] {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=SRC_DIR).returncode


def build():
    os.makedirs(BUILD_DIR, exist_ok=True)

    include_flags = []
    for inc in INCLUDES:
        include_flags.extend(["-I", inc])

    src_paths = [os.path.join(SRC_DIR, s) for s in SOURCES]

    cmd = [CC] + CFLAGS + include_flags + src_paths + LDFLAGS + ["-o", OUTPUT_EXE]
    ret = cmd_run(cmd)
    if ret == 0:
        print(f"[OK] Built: {OUTPUT_EXE}")
    else:
        print(f"[FAIL] Compilation failed with code {ret}")
        sys.exit(ret)


def clean():
    if os.path.exists(OUTPUT_EXE):
        os.remove(OUTPUT_EXE)
        print(f"[OK] Removed: {OUTPUT_EXE}")
    # Also clean any .o files in build
    for f in glob.glob(os.path.join(BUILD_DIR, "*.o")):
        os.remove(f)
        print(f"[OK] Removed: {f}")
    print("[OK] Clean complete.")


def run():
    """Build then run with user-provided arguments."""
    build()
    args = sys.argv[2:] if len(sys.argv) > 2 else []
    print(f"[RUN] {OUTPUT_EXE} {' '.join(args)}")
    return subprocess.run([OUTPUT_EXE] + args).returncode


def main():
    if len(sys.argv) < 2 or sys.argv[1] == "build":
        build()
    elif sys.argv[1] == "clean":
        clean()
    elif sys.argv[1] == "run":
        sys.exit(run())
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
