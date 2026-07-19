#!/usr/bin/env python3
"""
Gnuchan C-Like Build System
===========================
Odak:  GNU/Linux
Destek: Windows (MinGW)

Output:
  language/build/
  ├── gcl[.exe]        → compiler binary
  ├── obj/             → object files
  └── lib/             → (future) static/shared libraries

Usage:
  cd language/src
  python makefile.py            # build
  python makefile.py clean      # clean
  python makefile.py test       # build + test
  python makefile.py run <file> # compile input.gcsf + run
"""

import os, sys, subprocess, shutil

# ── paths ──────────────────────────────────────────────────
BASE   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC    = os.path.join(BASE, "src")
BUILD  = os.path.join(BASE, "build")
OBJ    = os.path.join(BUILD, "obj")
INCDIR = os.path.join(SRC, "include")
TSTDIR = os.path.join(BASE, "_NOTES")

# ── compiler flags ─────────────────────────────────────────
CC  = os.environ.get("CC", "gcc")
BASE_CFLAGS = ["-std=gnu99", "-Wall", "-Wextra", "-I", INCDIR]
BASE_LDFLAGS = ["-lm"]

# detect platform
IS_WINDOWS = os.name == "nt"
BIN_EXT = ".exe" if IS_WINDOWS else ""
BIN = os.path.join(BUILD, f"gcl{BIN_EXT}")

SOURCES = [
    os.path.join(SRC, "main.c"),
    os.path.join(SRC, "cli/cli.c"),
    os.path.join(SRC, "lexer/lexer.c"),
    os.path.join(SRC, "ast/ast.c"),
    os.path.join(SRC, "parser/parser.c"),
    os.path.join(SRC, "codegen/codegen.c"),
    os.path.join(SRC, "error/error.c"),
    os.path.join(SRC, "runtime/memory.c"),
    os.path.join(SRC, "semantic/semantic.c"),
    os.path.join(SRC, "version/version.c"),
    os.path.join(SRC, "linker/linker.c"),
    os.path.join(SRC, "shell/shell.c"),
]

OBJECTS = [os.path.join(OBJ, os.path.splitext(os.path.basename(s))[0] + ".o")
           for s in SOURCES]

# ── helpers ────────────────────────────────────────────────
def run(cmd, cwd=None):
    sys.stdout.write(f"  {' '.join(cmd)}\n")
    sys.stdout.flush()
    r = subprocess.run(cmd, cwd=cwd)
    if r.returncode != 0:
        sys.stderr.write("FAILED\n")
        sys.exit(1)

def ensure_dirs():
    for d in [BUILD, OBJ]:
        if not os.path.exists(d):
            os.makedirs(d)

def banner(s):
    sys.stdout.write(f"\n===== {s} =====\n")
    sys.stdout.flush()

# ── build ──────────────────────────────────────────────────
def build():
    banner("GCL Build")
    ensure_dirs()
    for i in range(len(SOURCES)):
        cmd = [CC] + BASE_CFLAGS + ["-c", SOURCES[i], "-o", OBJECTS[i]]
        run(cmd)
    cmd = [CC] + BASE_CFLAGS + OBJECTS + ["-o", BIN] + BASE_LDFLAGS
    run(cmd)
    sz = os.path.getsize(BIN)
    sys.stdout.write(f"OK: {BIN} ({sz} bytes)\n")

def clean():
    banner("Clean")
    if os.path.exists(OBJ):
        shutil.rmtree(OBJ)
    if os.path.exists(BIN):
        os.remove(BIN)
    sys.stdout.write("OK\n")

def test():
    build()
    banner("Test Suite")
    py = "python3" if not IS_WINDOWS else "python"
    runner = os.path.join(TSTDIR, "run_tests.py")
    r = subprocess.run([py, runner], cwd=BASE)
    if r.returncode != 0:
        sys.exit(r.returncode)

def run_file(args):
    if not args:
        sys.stderr.write("usage: python makefile.py run <file.gcsf>\n")
        sys.exit(1)
    build()
    infile = args[0]
    outfile = os.path.join(BUILD, os.path.splitext(os.path.basename(infile))[0] + ".c")
    r = subprocess.run([BIN, infile, "-o", outfile])
    if r.returncode != 0:
        sys.exit(r.returncode)
    exe = os.path.join(BUILD, os.path.splitext(os.path.basename(infile))[0] + BIN_EXT)
    r = subprocess.run([CC, "-std=gnu99", outfile, "-o", exe, "-lm"])
    if r.returncode != 0:
        sys.exit(r.returncode)
    sys.stdout.write(f"Running {exe}:\n")
    r = subprocess.run([exe])

def main():
    if len(sys.argv) < 2:
        build()
        return
    cmd = sys.argv[1]
    if cmd == "clean":
        clean()
    elif cmd == "test":
        test()
    elif cmd == "run":
        run_file(sys.argv[2:])
    elif cmd in ("-h", "--help", "help"):
        print(__doc__)
    else:
        sys.stderr.write(f"unknown: {cmd}\n")
        sys.stderr.write("usage: python makefile.py [clean|test|run <file>]\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
