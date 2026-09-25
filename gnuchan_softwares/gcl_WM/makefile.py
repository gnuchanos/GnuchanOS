#!/usr/bin/env python3
"""
GnuChanWM build and install (Debian only).

Usage:
  python3 makefile.py            # build
  python3 makefile.py install    # build, then install system-wide
  python3 makefile.py uninstall  # remove what install put down
  python3 makefile.py run        # build, then start it on the current display

`install` is what a user runs once: it puts the binary in /usr/local/bin and a
session entry in /usr/share/xsessions, so the display manager lists "GnuChanWM"
and the user picks it from the login screen's session menu. That is the whole
integration — nothing edits anyone's dotfiles.

Debian is the only target. The dependencies are Debian's package names, the
session directory is Debian's, and there is no attempt to detect another
distribution.

    apt install build-essential libx11-dev
"""
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
#: The sources live beside this script, not in a subdirectory: the project is
#: small enough that a src/ level would be a level for its own sake.
SRC = ROOT
BUILD = ROOT / "build"

#: The binary's name, and so the name of the file in /usr/local/bin.
PROGRAM = "GnuChanWM"

#: Where `install` puts the binary and the session entry.
BIN_DIR = Path("/usr/local/bin")
SESSION_DIR = Path("/usr/share/xsessions")
SESSION_FILE = SESSION_DIR / "gnuchanwm.desktop"

#: The sources, in the order they are compiled. Each one is a module or the
#: core; there is no generated file.
SOURCES = [
    "wm_core.c",
    "wm_manage.c",
    "wm_focus.c",
    "wm_spawn.c",
    "wm_keys.c",
    "GnuChanWM.c",
]

#: What the build needs, as Debian package names. A missing one is named and
#: the build stops, rather than failing somewhere inside gcc's output.
APT_PACKAGES = ("build-essential", "libx11-dev")

#: The X libraries the binary links against.
LIBS = ("-lX11",)


def is_linux() -> bool:
    return platform.system().lower() == "linux"


def run(cmd: list[str], cwd: Path | None = None) -> int:
    print(f"[gcl_wm] {' '.join(str(x) for x in cmd)}", flush=True)
    return subprocess.run(cmd, cwd=cwd, check=False).returncode


def missing_tools() -> list[str]:
    """The compilers and tools the build needs that are not on PATH."""
    return [name for name in ("gcc", "pkg-config") if shutil.which(name) is None]


def x11_flags() -> tuple[list[str], list[str]]:
    """The include and link flags for X11, from pkg-config.

    pkg-config is the authority on where a library's headers are: a machine
    with X11 in a prefix of its own gets the right -I without anyone guessing.
    The fallback is Debian's own location and the library name, which is where
    X11 always is on Debian.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = subprocess.run(
            [pkg_config, "--cflags", "x11"],
            capture_output=True, text=True, check=False,
        )
        libs = subprocess.run(
            [pkg_config, "--libs", "x11"],
            capture_output=True, text=True, check=False,
        )
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    return ["-I/usr/include"], list(LIBS)


def build() -> Path:
    """Compile the WM into build/GnuChanWM and return its path."""
    if not is_linux():
        raise SystemExit(
            f"error: GnuChanWM is a Debian X11 window manager; "
            f"this is {platform.system()}"
        )

    missing = missing_tools()
    if missing:
        raise SystemExit(
            "error: missing build tools: " + ", ".join(missing)
            + "\n       install them with: sudo apt install "
            + " ".join(APT_PACKAGES)
        )

    missing_sources = [name for name in SOURCES if not (SRC / name).is_file()]
    if missing_sources:
        raise SystemExit("error: missing sources: " + ", ".join(missing_sources))

    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    cflags, libs = x11_flags()

    cmd = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-O2", "-D_DEFAULT_SOURCE",
        "-I", str(SRC),
        *cflags,
    ]
    cmd += [str(SRC / name) for name in SOURCES]
    cmd += ["-o", str(output)]
    cmd += libs
    cmd += ["-lm"]

    if run(cmd, cwd=ROOT) != 0:
        raise SystemExit("error: the build failed")

    print(f"[gcl_wm] built {output}", flush=True)
    return output


def session_entry() -> str:
    """The .desktop a display manager reads to offer this session.

    Exec starts the WM itself. A session with no panel, no terminal and no
    desktop is exactly what this is, and the session entry says so by naming
    the WM and nothing else. The TryExec line is what makes the entry
    disappear from the menu on a machine where the binary is not installed —
    a session offered for a program that is not there is a login that fails.
    """
    return "\n".join([
        "[Desktop Entry]",
        "Name=GnuChanWM",
        "Comment=GnuchanOS window manager",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Type=Application",
        "DesktopNames=GnuChanWM",
        "",
    ])


def ensure_root() -> bool:
    """Whether this process can write /usr/local/bin and /usr/share/xsessions."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def install(binary: Path) -> None:
    """Copy the binary and the session entry into place, as root.

    Everything is written through sudo when this is not already root, because
    both destinations are root's. The session entry is what makes the WM
    selectable at the login screen; the binary is what the entry runs.
    """
    if ensure_root():
        # Only root can create /usr/share/xsessions. Without root the file is
        # written through sudo below, and the directory already exists on any
        # Debian with a display manager installed.
        SESSION_DIR.mkdir(parents=True, exist_ok=True)
    else:
        print("[gcl_wm] installing to /usr/local and /usr/share needs root; "
              "using sudo", flush=True)

    def put(source: Path, target: Path, mode: int) -> None:
        if ensure_root():
            shutil.copyfile(source, target)
            target.chmod(mode)
            print(f"[gcl_wm] installed {target}", flush=True)
            return
        sudo = shutil.which("sudo")
        if sudo is None:
            raise SystemExit(
                "error: not root and sudo is not installed; re-run as root"
            )
        if run([sudo, "install", "-m", oct(mode)[2:], str(source), str(target)]) != 0:
            raise SystemExit(f"error: could not install {target}")

    put(binary, BIN_DIR / PROGRAM, 0o755)

    # The session file is written to a temporary name first and moved into
    # place, so a display manager reading it never sees a half-written file.
    temporary = BUILD / "gnuchanwm.desktop"
    temporary.write_text(session_entry(), encoding="utf-8")
    put(temporary, SESSION_FILE, 0o644)

    print("", flush=True)
    print("Installed. Pick 'GnuChanWM' from the session menu on the login", flush=True)
    print("screen; it starts the window manager with Alt+Enter as the", flush=True)
    print("terminal shortcut.", flush=True)


def uninstall() -> None:
    """Remove the binary and the session entry, and nothing else."""
    targets = (BIN_DIR / PROGRAM, SESSION_FILE)

    def drop(target: Path) -> None:
        if not target.exists():
            return
        if ensure_root():
            target.unlink()
            print(f"[gcl_wm] removed {target}", flush=True)
            return
        sudo = shutil.which("sudo")
        if sudo is None:
            raise SystemExit(
                "error: not root and sudo is not installed; re-run as root"
            )
        run([sudo, "rm", "-f", str(target)])

    for target in targets:
        drop(target)
    print("[gcl_wm] removed. The build/ directory and the sources are untouched.",
          flush=True)


def run_session(binary: Path) -> int:
    """Start the WM on the display this shell is on, for a quick test.

    It refuses to start when a window manager is already running, because
    two of them on one display fight over every window — the X server tells
    the second one it cannot have the substructure, and this reports that
    rather than leaving a half-started WM behind.
    """
    if not os.environ.get("DISPLAY"):
        raise SystemExit(
            "error: DISPLAY is not set, so there is no X server to run on"
        )
    print("[gcl_wm] starting; quit with the same key you would use for any WM "
          "(the process ends on SIGTERM)", flush=True)
    return run([str(binary)], cwd=ROOT)


def main() -> int:
    action = sys.argv[1] if len(sys.argv) > 1 else "build"

    if action not in ("build", "install", "uninstall", "run"):
        print(f"error: unknown action '{action}'", file=sys.stderr)
        print("usage: python3 makefile.py [build|install|uninstall|run]",
              file=sys.stderr)
        return 2

    if action == "uninstall":
        uninstall()
        return 0

    binary = build()

    if action == "install":
        install(binary)
    elif action == "run":
        return run_session(binary)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
