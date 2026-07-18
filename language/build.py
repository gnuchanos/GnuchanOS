#!/usr/bin/env python3
"""
build.py — GCL Compiler Builder
Usage: python build.py

Builds gcl.exe to _output_test/
"""

import subprocess, sys, os

CC = "gcc"
CFLAGS = "-std=c99 -g -Isrc"
SRC = [
    "src/main.c",
    "src/lexer/token.c",
    "src/lexer/lexer.c",
    "src/parser/ast.c",
    "src/parser/parser.c",
    "src/type/type.c",
    "src/semantic/semantic.c",
    "src/ir/ir.c",
    "src/ir/ir_builder.c",
    "src/codegen/codegen.c",
    "src/runtime/runtime.c",
    "src/vm/bytecode.c",
    "src/vm/ir_to_bc.c",
    "src/vm/vm.c",
]
OUT_DIR = "_output_test"
OUT = os.path.join(OUT_DIR, "gcl.exe")

def run(cmd):
    print(f"  {cmd}")
    r = subprocess.run(cmd, shell=True)
    if r.returncode:
        sys.exit(r.returncode)

if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    all_src = " ".join(SRC)
    run(f"{CC} {CFLAGS} {all_src} -o {OUT} -mconsole")
    print(f"Build OK: {OUT}")
