#!/usr/bin/env python3
"""GCL Compiler — Test Suite"""
import subprocess, sys, os

BASE = os.path.dirname(os.path.abspath(__file__))
PARENT = os.path.dirname(BASE)
BUILD = os.path.join(PARENT, "build")
GCL = os.path.join(BUILD, "gcl.exe" if sys.platform == "win32" else "gcl")

def run_test(name, code, expect_fail=False):
    gcsf = os.path.join(BASE, "t_%s.gcsf" % name)
    with open(gcsf, "w") as f:
        f.write(code)
    r = subprocess.run([GCL, gcsf, "-o", gcsf+".c"],
                       capture_output=True, text=True, cwd=PARENT)
    ok = (r.returncode == 0) != expect_fail
    status = "PASS" if ok else "FAIL"
    print("[%s] %s" % (status, name))
    if r.returncode != 0:
        for line in r.stderr.strip().split("\n"):
            print("       " + line)
    return ok

def main():
    print("=" * 60)
    print("GCL Compiler Test Suite")
    print("=" * 60)
    tests = [
        ("basic", "int a = 10;\n"),
        ("noval", "int b;\n"),
        ("multi", "int a=10;\nint b=20;\n"),
        ("empty", "\n"),
    ]
    errs = [
        ("miss_val", "int a = ;\n", True),
        ("miss_name", "int = 10;\n", True),
    ]
    passed = 0
    for name, code in tests:
        ok = run_test(name, code)
        if ok: passed += 1
    for name, code, exp in errs:
        ok = run_test(name, code, expect_fail=exp)
        if ok: passed += 1
    total = len(tests) + len(errs)
    print("\n%s" % ("=" * 60))
    print("Result: %d/%d passed" % (passed, total))
    print("%s" % ("=" * 60))
    return 0 if passed == total else 1

if __name__ == "__main__":
    sys.exit(main())
