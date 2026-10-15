#!/usr/bin/env python3
# =============================================================================
# GnuChanInstall - build and install (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py            build and install
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py uninstall  remove what this installed
#
# GnuChanInstall is the GnuchanOS installer's window: an X11 program (Xlib and
# Xft, like GnuChanSL, GnuChanDM and GnuChanDock) that asks which disk and the
# account's name and password, then runs the install.
#
# --- the two halves, and why they install together --------------------------
#
# The WINDOW (this directory) does not touch a disk. The INSTALL is the shell
# script beside it, gnuchanos-install.sh, which partitions, unsquashfs's the
# live system, writes GRUB and makes the account - printing every step. The
# window runs that script and shows its output. So the two install together, as
# one thing:
#
#     /usr/local/bin/GnuChanInstall      the window
#     /usr/local/sbin/gnuchanos-install  the script (the real work)
#     /usr/local/bin/gnuchanos-installer the launcher: runs the window as root
#     the two .desktop entries           the menu, the desktop, the autostart
#
# Debian only, on purpose: it installs with apt-get and writes Debian's system
# directories.
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
PACKAGE = ROOT.parent                       # installer/
BUILD = PACKAGE.parent / "_temp" / "GnuChanInstall-build"

PROGRAM = "GnuChanInstall"
BIN_DIR = Path("/usr/local/bin")
SBIN_DIR = Path("/usr/local/sbin")
APPS_DIR = Path("/usr/share/applications")
XDG_DIR = Path("/etc/xdg/autostart")
PIXMAP_DIR = Path("/usr/share/pixmaps/gnuchanos")

ENGINE_SOURCE = PACKAGE / "gnuchanos-install.sh"
ENGINE_TARGET = SBIN_DIR / "gnuchanos-install"
LAUNCHER = BIN_DIR / "gnuchanos-installer"

SOURCES = (
    "GnuChanInstall.c",
    "inst_disks.c",
    "inst_engine.c",
    "inst_ui.c",
)
HEADERS = ("inst.h",)

ELEVATED_VARIABLE = "GNUCHANINSTALL_ELEVATED"


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
        raise SystemExit("error: this installs to /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header - how a machine is asked whether a
    development package is installed. A real one-line compile rather than a test
    for a file, because where a distribution keeps a header is not fixed."""
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(command, input=f"#include <{header}>\n",
                            text=True, capture_output=True, check=False)
    return result.returncode == 0


def ft2_includes() -> tuple[str, ...]:
    """Where freetype keeps its headers (Xft includes ft2build.h)."""
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        result = run([pkg_config, "--cflags-only-I", "freetype2"],
                     capture=True)
        if result.returncode == 0:
            paths = tuple(word[2:] for word in result.stdout.split()
                          if word.startswith("-I"))
            if paths:
                return paths
    return ("/usr/include/freetype2",)


def missing_build_dependencies() -> list[str]:
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not header_present("X11/Xlib.h"):
        needed.append("libx11-dev")
    if not header_present("X11/Xft/Xft.h", ft2_includes()):
        needed.append("libxft-dev")
    if not header_present("ft2build.h", ft2_includes()):
        needed.append("libfreetype-dev")
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
    if run(command, environment=apt_environment()).returncode != 0:
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def x11_flags() -> tuple[list[str], list[str]]:
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", "x11", "xft"], capture=True)
        libs = run([pkg_config, "--libs", "x11", "xft"], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    includes = ["-I/usr/include"] + ["-I" + p for p in ft2_includes()]
    return includes, ["-lX11", "-lXft"]


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS)
               if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))
    if not ENGINE_SOURCE.is_file():
        raise SystemExit(f"error: missing the install engine {ENGINE_SOURCE}")


def build() -> Path:
    check_sources()
    ensure_build_dependencies()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    cflags, libs = x11_flags()
    command = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-O2", "-D_DEFAULT_SOURCE",
        "-I", str(ROOT),
        *cflags,
    ]
    command += [str(ROOT / name) for name in SOURCES]
    command += ["-o", str(output)]
    command += libs
    command += ["-lm"]

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def launcher_text() -> str:
    return "\n".join([
        "#!/bin/sh",
        "# gnuchanos-installer - run the installer window as root.",
        "#",
        "# The window partitions a disk, so it needs root. The live account has",
        "# passwordless sudo (0050-liveuser), so sudo -n runs it with no prompt;",
        "# DISPLAY and XAUTHORITY are passed explicitly because sudo resets the",
        "# environment and a window that cannot open the display never appears.",
        "set -u",
        'DISPLAY="${DISPLAY:-:0}"',
        'XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}"',
        "export DISPLAY XAUTHORITY",
        f'exec sudo -n env DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" {BIN_DIR / PROGRAM} "$@"',
        "",
    ])


def desktop_text(autostart: bool) -> str:
    lines = [
        "[Desktop Entry]",
        "Type=Application",
        "Name=Install GnuChanOS",
        "Comment=Install GnuChanOS to this computer",
        f"Exec={LAUNCHER}",
        f"TryExec={LAUNCHER}",
        "Terminal=false",
        "Categories=System;",
    ]
    icon = PIXMAP_DIR / "logo.png"
    if icon.is_file():
        lines.append(f"Icon={icon}")
    if autostart:
        lines += [
            "X-GNOME-Autostart-enabled=true",
            "X-KDE-autostart-after=panel",
        ]
    lines.append("")
    return "\n".join(lines)


def write_atomically(target: Path, text: str, mode: int) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".new")
    temporary.write_text(text, encoding="utf-8")
    os.replace(str(temporary), str(target))
    target.chmod(mode)


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    SBIN_DIR.mkdir(parents=True, exist_ok=True)
    APPS_DIR.mkdir(parents=True, exist_ok=True)

    # the window
    target = BIN_DIR / PROGRAM
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(target))
    detail(f"installed {target}")

    # the engine: the script that does the real work
    engine_staged = SBIN_DIR / "gnuchanos-install.new"
    shutil.copyfile(ENGINE_SOURCE, engine_staged)
    engine_staged.chmod(0o755)
    os.replace(str(engine_staged), str(ENGINE_TARGET))
    detail(f"installed {ENGINE_TARGET}")

    # the launcher, and the two desktop entries that call it
    write_atomically(LAUNCHER, launcher_text(), 0o755)
    detail(f"installed {LAUNCHER}")
    write_atomically(APPS_DIR / "gnuchanos-installer.desktop",
                     desktop_text(False), 0o644)
    write_atomically(XDG_DIR / "gnuchanos-installer.desktop",
                     desktop_text(True), 0o644)
    detail("installed the desktop entries")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, ENGINE_TARGET, LAUNCHER,
                   APPS_DIR / "gnuchanos-installer.desktop",
                   XDG_DIR / "gnuchanos-installer.desktop"):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")


def run_program(binary: Path) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set, so there is no X server")
    step("Starting on the current display")
    return run([str(binary)], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(f"error: this is a Debian X11 program; this is "
                         f"{platform.system()}")
    if not is_debian():
        raise SystemExit("error: this installs with apt-get and writes Debian's "
                         "system directories; it is Debian-only by design")

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "uninstall")
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

    binary = build()
    install(binary)
    note("")
    note("GnuChanInstall is installed. It is in the applications menu, and")
    note(f"{LAUNCHER} starts it from a shell.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
