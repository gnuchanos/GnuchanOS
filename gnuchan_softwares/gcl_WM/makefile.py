#!/usr/bin/env python3
# =============================================================================
# GnuChanWM - build and install everything (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# Installs the build dependencies, a terminal if the machine has none, builds
# the window manager, and installs it with a session entry so the display
# manager offers "GnuChanWM". It needs root and re-runs itself through sudo.
#
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py uninstall  remove the binary and the session entry
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
BUILD = ROOT / "build"

PROGRAM = "GnuChanWM"
BIN_DIR = Path("/usr/local/bin")
SESSION_DIR = Path("/usr/share/xsessions")
SESSION_FILE = SESSION_DIR / "gnuchanwm.desktop"

# The settings script, installed into the user's own config directory. The
# window manager reads it from there, so a machine that never had a script
# needs one put in place of it — otherwise the desktop falls back to its
# built-in defaults and looks nothing like what GnuchanOS shipped.
CONFIG_SOURCE = ROOT / "GnuChanWM_config" / "GnuChanWM.py"
CONFIG_DIR_NAME = "GnuChanWM"
CONFIG_FILE_NAME = "GnuChanWM.py"

SOURCES = (
    "wm_core.c",
    "wm_style.c",
    "wm_config_parser.c",
    "wm_config_file.c",
    "wm_theme.c",
    "wm_desktop.c",
    "wm_frame.c",
    "wm_manage.c",
    "wm_focus.c",
    "wm_spawn.c",
    "wm_autostart.c",
    "wm_menu.c",
    "wm_input.c",
    "wm_keys.c",
    "wm_workspace.c",
    "GnuChanWM.c",
)
HEADERS = (
    "wm_module.h", "wm_core.h", "wm_style.h", "wm_frame.h", "wm_spawn.h",
    "wm_theme.h",
    "wm_config.h", "wm_config_parser.h",
    "wm_workspace.h", "wm_desktop.h",
)

FALLBACK_TERMINAL = "xterm"
TERMINAL_CANDIDATES = (
    "x-terminal-emulator", "gnome-terminal", "konsole",
    "xfce4-terminal", "alacritty", "kitty", "xterm",
)

ELEVATED_VARIABLE = "GNUCHANWM_ELEVATED"


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(
    command: list[str],
    capture: bool = False,
    environment: dict[str, str] | None = None,
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True,
            env=environment, cwd=cwd,
        )
    return subprocess.run(command, check=False, text=True, env=environment, cwd=cwd)


def apt_environment() -> dict[str, str]:
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
        raise SystemExit("error: this installs packages and writes /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(Path(__file__).resolve())], environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed.

    The check is a real compile of a one-line translation unit rather than a
    test for a file on disk, because where a distribution keeps a header is
    not fixed and what matters is whether the compiler the build will use can
    find it. Xft pulls freetype in through its own header, so the freetype
    include directory is passed for it: the header is present exactly when
    both packages are.
    """
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(
        command, input=f"#include <{header}>\n",
        text=True, capture_output=True, check=False,
    )
    return result.returncode == 0


def x11_headers_present() -> bool:
    return header_present("X11/Xlib.h")


def xft_headers_present() -> bool:
    return header_present("X11/Xft/Xft.h", ("/usr/include/freetype2",))


def program_exists(name: str) -> bool:
    if "/" in name:
        return os.access(name, os.X_OK)
    return shutil.which(name) is not None


def apt_install(packages: tuple[str, ...]) -> bool:
    if not packages:
        return True
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *packages]
    detail("running: " + " ".join(command))
    return run(command, environment=apt_environment()).returncode == 0


def ensure_build_dependencies() -> None:
    needed: list[str] = []
    if not x11_headers_present():
        needed.append("libx11-dev")
    # Xft is what the text is drawn with, and its header is in a package of
    # its own that libx11-dev does not pull in. freetype and fontconfig, which
    # Xft's own header includes, come with it as dependencies.
    if not xft_headers_present():
        needed.append("libxft-dev")
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not needed:
        return
    step("Installing the build dependencies")
    detail("missing: " + ", ".join(needed))
    if not apt_install(tuple(needed)):
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def ensure_terminal() -> None:
    for name in TERMINAL_CANDIDATES:
        if program_exists(name):
            detail(f"terminal: {name}")
            return
    step("Installing a terminal")
    if not apt_install((FALLBACK_TERMINAL,)):
        raise SystemExit(
            f"error: no terminal is installed and apt-get could not install "
            f"{FALLBACK_TERMINAL}"
        )
    detail(f"installed {FALLBACK_TERMINAL}")


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the X libraries the WM uses.

    x11 is needed for everything and xft for the text, and the two are asked
    for together because Xft's own header includes freetype's: pkg-config is
    what knows where that lives, and a machine whose freetype is somewhere
    unusual is exactly the case these flags exist for. The fallback names the
    two libraries and freetype's include directory directly, for a machine with
    no pkg-config, and is right on every Debian where the development packages
    are installed.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", "x11", "xft"], capture=True)
        libs = run([pkg_config, "--libs", "x11", "xft"], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    return (["-I/usr/include", "-I/usr/include/freetype2"],
            ["-lX11", "-lXft"])


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS) if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
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


def session_entry() -> str:
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


def install(binary: Path) -> None:
    step("Installing")
    SESSION_DIR.mkdir(parents=True, exist_ok=True)

    # The window manager already running a session is the one this install is
    # replacing, so writing to its path would fail with ETXTBSY. The new
    # binary is written beside it and renamed over it: a rename swaps the
    # directory entry and never opens the file being replaced, and the running
    # session keeps its copy until it is logged out and back in.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
    detail(f"installed {BIN_DIR / PROGRAM}")

    temporary = BUILD / "gnuchanwm.desktop.new"
    temporary.write_text(session_entry(), encoding="utf-8")
    os.replace(str(temporary), str(SESSION_FILE))
    SESSION_FILE.chmod(0o644)
    detail(f"installed {SESSION_FILE}")


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root.

    The script belongs to the person who logs in, and the window manager reads
    it from their home. Under sudo, HOME and the ids are root's, so the
    original user is read back from SUDO_USER — the one piece of the invoking
    session sudo keeps. Returns None when there is nothing sensible to write
    to, which is a machine installing without a login user.
    """
    name = os.environ.get("SUDO_USER")
    if not name:
        return None
    try:
        import pwd
        info = pwd.getpwnam(name)
        return Path(info.pw_dir), info.pw_uid, info.pw_gid
    except (ImportError, KeyError):
        return None


def install_config() -> None:
    """Put the settings script where the window manager will read it.

    An existing script that has something in it is left exactly as it is. It
    is the user's file — they may have edited it, and an installer that
    overwrites it on every run is an installer that silently undoes their
    work.

    An empty file is not that. It is what this installer used to leave behind
    when it was interrupted between creating the file and filling it, and it
    is worse than no file at all: the window manager reads it, finds a script
    that asks for no bar and one workspace, and half-erases the built-in
    desktop instead of falling back to it. So a file of zero bytes is treated
    as absent and written over.
    """
    if not CONFIG_SOURCE.is_file():
        return
    user = invoking_user()
    if user is None:
        detail("skipped the config: no login user to install it for")
        return

    home, uid, gid = user
    directory = home / ".config" / CONFIG_DIR_NAME
    target = directory / CONFIG_FILE_NAME

    if target.exists() and target.stat().st_size > 0:
        detail(f"kept the existing {target}")
        return

    directory.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(CONFIG_SOURCE, target)

    # The source is shipped with the window manager, so it is never empty; a
    # check here is what turns "the desktop came up with no bar" into one line
    # naming the file that caused it.
    if target.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the window manager would "
            f"have nothing to read"
        )

    # The file was written by root and has to belong to the user who will read
    # it, or the window manager opens a file it can see but not change.
    try:
        os.chown(directory, uid, gid)
        os.chown(target, uid, gid)
    except OSError:
        pass

    detail(f"installed {target}")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, SESSION_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")


def run_session(binary: Path) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set, so there is no X server to run on")
    step("Starting on the current display")
    return run([str(binary)], cwd=ROOT).returncode


def install_everything() -> int:
    ensure_build_dependencies()
    ensure_terminal()
    binary = build()
    install(binary)
    install_config()
    note("")
    note("GnuChanWM is installed. It is in the display manager's session menu.")
    return 0


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanWM is a Debian X11 window manager; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's session "
            "directory; it is Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "uninstall")
    if action not in known:
        print(f"error: unknown action '{action}'", file=sys.stderr)
        print("usage: python3 makefile.py [" + "|".join(known) + "]", file=sys.stderr)
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
        return run_session(build())

    return install_everything()


if __name__ == "__main__":
    raise SystemExit(main())
