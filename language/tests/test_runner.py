#!/usr/bin/env python3
"""
GCL Compiler Test Suite — test_runner.py

Usage:
    python tests/test_runner.py              # run all tests
    python tests/test_runner.py <filter>      # run tests matching filter
    python tests/test_runner.py --list        # list all test names

Environment:
    GCL_EXE    path to gcl.exe (default: build/gcl.exe)
"""

import os
import sys
import subprocess
import re
import tempfile

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
DEFAULT_GCL = os.path.join(BUILD_DIR, "gcl.exe")
PASS = 0
FAIL = 1
SKIP = 2

# ----------------------------------------------------------------
#  Test infrastructure
# ----------------------------------------------------------------

class TestCase:
    """A single test case with setup, run, and verify steps."""
    def __init__(self, name, description):
        self.name = name
        self.description = description

    def run(self, gcl_exe, tmpdir):
        """Run the test. Return (PASS/FAIL/SKIP, message)."""
        raise NotImplementedError


class CompileTest(TestCase):
    """Compile a .gcsf file with given mode and check stdout/err."""
    def __init__(self, name, description, gcsf_source, mode="exec",
                 expected_stdout=None, expected_stderr=None,
                 expected_exit=0, expected_stdout_contains=None):
        super().__init__(name, description)
        self.gcsf_source = gcsf_source
        self.mode = mode
        self.expected_stdout = expected_stdout
        self.expected_stderr = expected_stderr
        self.expected_exit = expected_exit
        self.expected_stdout_contains = expected_stdout_contains

    def run(self, gcl_exe, tmpdir):
        src = os.path.join(tmpdir, "test.gcsf")
        with open(src, "w", encoding="utf-8") as f:
            f.write(self.gcsf_source)

        cmd = [gcl_exe]
        if self.mode == "exec":
            cmd.append(src)
        elif self.mode == "lexer":
            cmd.extend(["-lexer", src])
        elif self.mode == "ast":
            cmd.extend(["-ast", src])
        elif self.mode == "ir":
            cmd.extend(["-ir", src])
        elif self.mode == "codegen":
            cmd.extend(["-codegen", src])
        else:
            cmd.append(src)

        result = subprocess.run(cmd, capture_output=True, text=True, cwd=tmpdir)

        if result.returncode != self.expected_exit:
            return (FAIL,
                    f"Expected exit {self.expected_exit}, got {result.returncode}\n"
                    f"stdout: {result.stdout[:1000]}\n"
                    f"stderr: {result.stderr[:1000]}")

        if self.expected_stdout is not None and result.stdout.strip() != self.expected_stdout.strip():
            return (FAIL,
                    f"stdout mismatch:\n"
                    f"  expected: {self.expected_stdout!r}\n"
                    f"  got:      {result.stdout.strip()!r}")

        if self.expected_stdout_contains is not None:
            for pattern in self.expected_stdout_contains:
                if pattern not in result.stdout:
                    return (FAIL,
                            f"stdout missing expected content: {pattern!r}\n"
                            f"stdout: {result.stdout[:1000]}")

        if self.expected_stderr is not None and self.expected_stderr not in result.stderr:
            return (FAIL,
                    f"stderr mismatch:\n"
                    f"  expected contains: {self.expected_stderr!r}\n"
                    f"  got:               {result.stderr!r}")

        return (PASS, "ok")


class PipelineTest(CompileTest):
    """Test that full pipeline (gcsf → C → compile with gcc) works."""
    def __init__(self, name, description, gcsf_source, expected_output,
                 cflags="-DDEBUG"):
        """Note: cflags defaults to -DDEBUG because GCL codegen wraps #debug in #ifdef DEBUG."""
        super().__init__(name, description, gcsf_source, mode="exec",
                         expected_stdout_contains=[expected_output])
        self.expected_output = expected_output
        self.cflags = cflags

    def run(self, gcl_exe, tmpdir):
        # Step 1: write gcsf
        src = os.path.join(tmpdir, "test.gcsf")
        with open(src, "w", encoding="utf-8") as f:
            f.write(self.gcsf_source)

        # Step 2: gcl -codegen → stdout
        result = subprocess.run([gcl_exe, "-codegen", src],
                                capture_output=True, text=True, cwd=tmpdir)
        if result.returncode != 0:
            return (FAIL, f"codegen failed:\n{result.stderr[:1000]}")

        c_code = result.stdout

        # Step 3: compile with gcc
        c_src = os.path.join(tmpdir, "test.c")
        with open(c_src, "w", encoding="utf-8") as f:
            f.write(c_code)

        exe = os.path.join(tmpdir, "test.exe" if sys.platform == "win32" else "test")
        cc_cmd = (["gcc", "-std=c11", "-Wall", "-Wextra", c_src, "-o", exe] +
                  self.cflags.split())
        cc_result = subprocess.run(cc_cmd, capture_output=True, text=True, cwd=tmpdir)

        if cc_result.returncode != 0:
            return (FAIL,
                    f"gcc compilation failed:\n"
                    f"{cc_result.stdout}\n{cc_result.stderr}\n"
                    f"C code:\n{c_code[:2000]}")

        # Step 4: run the compiled executable
        run_result = subprocess.run([exe], capture_output=True, text=True, cwd=tmpdir)

        if self.expected_output not in run_result.stdout:
            return (FAIL,
                    f"Expected output {self.expected_output!r} not found in:\n"
                    f"{run_result.stdout!r}")

        return (PASS, "ok")


# ----------------------------------------------------------------
#  Test cases
# ----------------------------------------------------------------

def collect_tests():
    """Return list of all TestCase instances."""
    tests = []

    # ---- Basic tests ----
    tests.append(CompileTest(
        "version_flag",
        "-version flag works",
        "",
        mode="exec",
        expected_exit=0,
    ))

    tests.append(CompileTest(
        "empty_file",
        "empty .gcsf file compiles",
        "",
        mode="exec",
        expected_stdout="",
    ))

    # ---- Define tests ----
    tests.append(CompileTest(
        "define_simple",
        "#define integer constant",
        "#define MAX 100\n",
        mode="ir",
        expected_stdout_contains=["MAX", "100"],
    ))

    tests.append(CompileTest(
        "define_string",
        "#define string constant",
        '#define NAME "hello"\n',
        mode="ir",
        expected_stdout_contains=['NAME', '"hello"'],
    ))

    tests.append(CompileTest(
        "define_chain",
        "chained #define (A → B → value)",
        "#define A 42\n#define B A\n",
        mode="ir",
        expected_stdout_contains=["B", "42"],
    ))

    tests.append(CompileTest(
        "undefine",
        "#undef removes a define",
        "#define X 1\n#undef X\n",
        mode="ir",
        expected_exit=0,
    ))

    # ---- Conditional tests ----
    tests.append(CompileTest(
        "ifdef_active",
        "#ifdef on defined name → branch taken",
        "#define DEBUG\n#ifdef DEBUG\n#define ACTIVE 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["ACTIVE", "1"],
    ))

    tests.append(CompileTest(
        "ifdef_inactive",
        "#ifdef on undefined name → branch skipped",
        "#ifdef UNDEFINED\n#define SHOULD_NOT_APPEAR 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["(none)"],
    ))

    tests.append(CompileTest(
        "ifndef_active",
        "#ifndef on undefined name → branch taken",
        "#ifndef UNDEFINED\n#define NEW_FLAG 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["NEW_FLAG", "1"],
    ))

    tests.append(CompileTest(
        "if_defined",
        "#if defined(NAME) works",
        "#define FEATURE_X\n#if defined(FEATURE_X)\n#define USES_X 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["USES_X", "1"],
    ))

    tests.append(CompileTest(
        "if_vs_chain",
        "chained or-conditions for platform",
        "#define linux 1\n#if linux / gnu\n#define PLAT 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["PLAT", "1"],
    ))

    tests.append(CompileTest(
        "if_else_branch",
        "#if/#else, second branch inactive",
        "#define A 1\n#ifdef A\n#define BRANCH_1 1\n#else\n#define BRANCH_2 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["BRANCH_1", "1"],
    ))

    tests.append(CompileTest(
        "if_elif_else",
        "#if/#elif/#else chain",
        "#define VER 2\n#if VER == 1\n#define MSG \"v1\"\n#elif VER == 2\n#define MSG \"v2\"\n#else\n#error bad\n#endif\n",
        mode="ir",
        expected_stdout_contains=['MSG', '"v2"'],
    ))

    # ---- Runtime (#debug) tests ----
    tests.append(PipelineTest(
        "debug_basic",
        "#debug with simple string prints correctly",
        '#debug "hello world"\n',
        "hello world",
    ))

    tests.append(PipelineTest(
        "debug_define_string",
        "#debug resolves #define string",
        '#define NAME "Gnuchan"\n#debug "Name: " NAME\n',
        "Name: Gnuchan",
    ))

    tests.append(PipelineTest(
        "debug_define_number",
        "#debug resolves #define number",
        "#define MAX 100\n#debug \"Max: \" MAX\n",
        "Max: 100",
    ))

    tests.append(PipelineTest(
        "debug_mixed",
        "#debug with mixed string, ident, number",
        '#define A "alpha"\n#define B 42\n#debug "a=" A " b=" B\n',
        "a=alpha b=42",
    ))

    tests.append(PipelineTest(
        "debug_ifdef_wrap",
        "#debug inside #ifdef DEBUG only prints with -DDEBUG",
        "#ifdef DEBUG\n#debug \"debug mode\"\n#endif\n",
        "",
        cflags="-DDEBUG",
    ))

    # ---- Message tests ----
    tests.append(CompileTest(
        "message_directive",
        "#message prints at compile time",
        '#message "hello from compile time"\n',
        mode="exec",
        expected_stdout_contains=["hello from compile time"],
    ))

    # ---- Comment tests ----
    tests.append(CompileTest(
        "comment_c_style",
        "C-style // comments ignored",
        "// this is a comment\n#define X 1\n",
        mode="ir",
        expected_stdout_contains=["X", "1"],
    ))

    tests.append(CompileTest(
        "comment_gcl_style",
        "GCL #| ... |# block comments ignored",
        "#| block\ncomment |#\n#define Y 2\n",
        mode="ir",
        expected_stdout_contains=["Y", "2"],
    ))

    tests.append(CompileTest(
        "comment_block",
        "C block comments /* */ ignored",
        "/* block */\n#define Z 3\n",
        mode="ir",
        expected_stdout_contains=["Z", "3"],
    ))

    # ---- Multi-file tests ----
    tests.append(CompileTest(
        "include_resolved",
        "#include <file> from same dir",
        "// include check\n#define INC_TEST 1\n",
        mode="ir",
        expected_stdout_contains=["INC_TEST", "1"],
    ))

    # ---- Error / warning tests ----
    tests.append(CompileTest(
        "error_E102_expected_string",
        "error E102 for missing string after #include",
        "#include\n",
        mode="exec",
        expected_exit=1,
        expected_stderr="E102",
    ))

    tests.append(CompileTest(
        "error_E101_expected_ident",
        "error E101 for missing ident after #define",
        "#define\n",
        mode="exec",
        expected_exit=1,
        expected_stderr="E101",
    ))

    # ---- Export tests ----
    tests.append(CompileTest(
        "export_noop",
        "-o export with empty file",
        "",
        mode="exec",
        expected_exit=0,
    ))

    # ---- Clang/Rust error format ----
    tests.append(CompileTest(
        "error_format",
        "error format has source line + caret",
        "#include\n",
        mode="exec",
        expected_exit=1,
        expected_stderr="^",
    ))

    # ---- Platform defines ----
    tests.append(CompileTest(
        "platform_defines",
        "built-in platform defines exist",
        "#if defined(windows) || defined(linux) || defined(gnu)\n#define OS_OK 1\n#endif\n",
        mode="ir",
        expected_stdout_contains=["OS_OK", "1"],
    ))

    return tests


# ----------------------------------------------------------------
#  CLI
# ----------------------------------------------------------------

def main():
    if "--list" in sys.argv:
        for t in collect_tests():
            print(f"  {t.name:40s} {t.description}")
        return 0

    gcl_exe = os.environ.get("GCL_EXE", DEFAULT_GCL)
    if not os.path.exists(gcl_exe):
        print(f"GCL executable not found: {gcl_exe}")
        print("Build first: python makefile.py")
        return 1

    filter_str = sys.argv[1] if len(sys.argv) > 1 else None
    tests = collect_tests()

    if filter_str:
        tests = [t for t in tests if filter_str.lower() in t.name.lower()]
        if not tests:
            print(f"No tests matching: {filter_str}")
            return 1

    passed = 0
    failed = 0
    skipped = 0
    results = []

    with tempfile.TemporaryDirectory(prefix="gcl_test_") as tmpdir:
        # Copy necessary build files for multi-file tests
        for f in ["math.gcsf", "library.gclib"]:
            src = os.path.join(BUILD_DIR, f)
            if os.path.exists(src):
                dst = os.path.join(tmpdir, f)
                import shutil
                shutil.copy2(src, dst)

        for t in tests:
            status, msg = t.run(gcl_exe, tmpdir)
            if status == PASS:
                passed += 1
                results.append((PASS, t.name, ""))
            elif status == FAIL:
                failed += 1
                results.append((FAIL, t.name, msg))
            else:
                skipped += 1
                results.append((SKIP, t.name, msg))

    # Summary
    total = passed + failed + skipped
    print(f"\n{'='*60}")
    print(f"  GCL Test Suite Results")
    print(f"{'='*60}")

    for status, name, msg in results:
        if status == PASS:
            print(f"  ✅ {name}")
        elif status == FAIL:
            print(f"  ❌ {name}")
            for line in msg.split("\n"):
                print(f"      {line}")
        else:
            print(f"  ⏭️  {name} ({msg})")

    print(f"{'='*60}")
    print(f"  Total: {total}  |  Passed: {passed}  |  Failed: {failed}  |  Skipped: {skipped}")
    print(f"{'='*60}")

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
