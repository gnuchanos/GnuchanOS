#!/usr/bin/env python3
# =============================================================================
# GnuChanBrowser - build and install (Debian)
#     python3 makefile.py            build and install
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and run it here
#     python3 makefile.py uninstall  remove the binary
#
# GnuChanBrowser is a C++/Qt6 program: a navigation bar over a QWebEngineView
# and nothing else. It is tuned for the 2007 laptop it runs on — an Intel
# 965GM whose GLES tops out at 2.0, which Chromium (and so Qt WebEngine)
# cannot use, since it runs only on GLES 3.0. The engine is therefore pinned
# to software rendering (see main.cpp), WebGL is off, and the background
# machinery is trimmed for two cores and little memory. It is a light browser,
# not a test rig: the GPU path was measured and cannot start on this hardware,
# so nothing here asks for it.
#
# Debian only, on purpose.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent.parent
BUILD = REPO_ROOT / "_temp" / "gcl_BROWSER_WEB-build"

PROGRAM = "GnuChanBrowser"
BIN_DIR = Path("/usr/local/bin")
APPS_DIR = Path("/usr/share/applications")
DESKTOP_FILE = APPS_DIR / "gnuchanbrowser.desktop"

SOURCES = (
    "url_utils.cpp",
    "browser_theme.cpp",
    "bookmarks.cpp",
    "browser_window.cpp",
    "main.cpp",
)
HEADERS = (
    "url_utils.h",
    "browser_theme.h",
    "bookmarks.h",
    "browser_window.h",
)

ELEVATED_VARIABLE = "GNUCHANBROWSER_ELEVATED"

# The pkg-config modules the program is compiled and linked against. Qt6 names
# one module per library, and these two are the whole of what this program
# uses: the widgets it is drawn with, and the web engine it shows pages in.
PKG_CONFIG_MODULES = ("Qt6Widgets", "Qt6WebEngineWidgets")

# What a machine is missing to build this, named as the Debian packages. The
# engine's own package pulls in the Qt packages it needs.
PACKAGE_CHECKS = (
    ("QtWidgets/QApplication", "qt6-base-dev"),
    ("QtWebEngineWidgets/QWebEngineView", "qt6-webengine-dev"),
)


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(command: list[str], capture: bool = False,
        environment: dict | None = None, cwd: Path | None = None):
    if capture:
        return subprocess.run(command, check=False, capture_output=True,
                              text=True, env=environment, cwd=cwd)
    return subprocess.run(command, check=False, text=True, env=environment,
                          cwd=cwd)


def apt_environment() -> dict:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root() -> None:
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit("error: still not root after sudo")
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: this installs packages and writes /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str) -> bool:
    """Whether the C++ compiler can preprocess a header.

    Qt's headers are found through pkg-config rather than a fixed include
    directory, so the test asks the compiler with the same flags the build
    will use; a machine that has the package but nowhere the compiler looks
    is a machine that cannot build this either.
    """
    cxx = shutil.which("g++")
    if cxx is None:
        return False
    cflags, _ = build_flags()
    command = [cxx, "-E", "-xc++", "-", *cflags]
    result = subprocess.run(command, input=f"#include <{header}>\n",
                            text=True, capture_output=True, check=False)
    return result.returncode == 0


def missing_build_dependencies() -> list[str]:
    needed: list[str] = []
    if shutil.which("g++") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    for header, package in PACKAGE_CHECKS:
        if not header_present(header):
            needed.append(package)
    return needed


def ensure_build_dependencies() -> None:
    needed = missing_build_dependencies()
    if not needed:
        return
    step("Installing the build dependencies")
    detail("missing: " + ", ".join(needed))
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *needed]
    detail("running: " + " ".join(command))
    if run(command, environment=apt_environment()).returncode != 0:
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def build_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for Qt6 widgets and the web engine.

    pkg-config is the one that knows where Qt6 keeps its includes and which
    libraries go with them. Qt6 requires C++17, which is added by the caller
    rather than here so the version is stated once.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", *PKG_CONFIG_MODULES],
                     capture=True)
        libs = run([pkg_config, "--libs", *PKG_CONFIG_MODULES], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    # A machine with the packages but no pkg-config cannot be guessed at:
    # Qt6's includes live under several directories this cannot know.
    return [], []


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS)
               if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    ensure_build_dependencies()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    cflags, libs = build_flags()
    if not cflags or not libs:
        raise SystemExit(
            "error: Qt6 was not found by pkg-config; install qt6-base-dev and "
            "qt6-webengine-dev")
    command = [
        "g++", "-std=c++17", "-Wall", "-Wextra", "-O2",
        "-I", str(ROOT),
        *cflags,
    ]
    command += [str(ROOT / name) for name in SOURCES]
    command += ["-o", str(output)]
    command += libs

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def desktop_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        "Name=GnuChanBrowser",
        "Comment=GnuchanOS web browser (Qt WebEngine)",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "Categories=Network;WebBrowser;",
        "Icon=web-browser",
        "",
    ])


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    APPS_DIR.mkdir(parents=True, exist_ok=True)

    target = BIN_DIR / PROGRAM
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(target))
    detail(f"installed {target}")

    temporary = DESKTOP_FILE.with_name(DESKTOP_FILE.name + ".new")
    temporary.write_text(desktop_entry(), encoding="utf-8")
    os.replace(str(temporary), str(DESKTOP_FILE))
    DESKTOP_FILE.chmod(0o644)
    detail(f"installed {DESKTOP_FILE}")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, DESKTOP_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")


def run_program(binary: Path, extra_args: tuple[str, ...] = ()) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set")
    step("Starting on the current display")
    return run([str(binary), *extra_args], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanBrowser is a Debian program; this is "
            f"{platform.system()}")
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's system "
            "directories; it is Debian-only by design")

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "software", "uninstall")
    if action not in known:
        print(f"error: unknown action '{action}'", file=sys.stderr)
        print("usage: python3 makefile.py [" + "|".join(known) + "]",
              file=sys.stderr)
        return 2

    if action in ("install", "uninstall"):
        ensure_root()

    if action == "uninstall":
        uninstall()
        return 0

    if action == "build":
        binary = build()
        note(f"Built {binary}.")
        return 0

    if action == "run":
        return run_program(build())

    if action == "software":
        return run_program(build(), ("--software",))

    binary = build()
    install(binary)
    note("")
    note("GnuChanBrowser is installed.")
    note(f"{BIN_DIR / PROGRAM} runs it. `--software` forces CPU rendering.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
