#!/usr/bin/env python3
# =============================================================================
# GnuChanWifi - build and install the wifi manager (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# Builds the manager and installs it to /usr/local/bin, with its settings file
# put where it will be read from — ~/.config/GnuChanWifi/config.py. It needs
# root for the install and re-runs itself through sudo, exactly as GnuChanWM's
# and GnuChanRunner's own makefiles do.
#
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py list       compile and print the networks in range
#     python3 makefile.py uninstall  remove the binary
#
# Debian only, on purpose: it installs with apt-get and writes /usr/local.
#
# It is a separate program from the window manager and installs on its own. The
# window manager starts it like any other program —
#
#     gcl_key.MultiKey(keys=["Mod1", "w"],
#                      action=gcl_spawn.RunProgram(command="GnuChanWifi"))
#
# — so the two are installed separately and neither needs the other to build.
#
# The manager talks to NetworkManager through nmcli, which is already installed
# on any machine that uses NetworkManager, so there is no runtime library to
# install beyond what the build needs.
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
# Build output under _temp/, not in the source tree: a build is not a change to
# the project, and the manager's directory holds only sources.
BUILD = ROOT.parent.parent / "_temp" / "gnuchanwifi-build"

PROGRAM = "GnuChanWifi"
BIN_DIR = Path("/usr/local/bin")

# The settings file, installed into the user's own config directory. The manager
# reads it from there, so a machine that never had one gets the shipped defaults
# written where it looks for them.
CONFIG_SOURCE = ROOT / "GnuChanWifi_config" / "config.py"
CONFIG_DIR_NAME = "GnuChanWifi"
CONFIG_FILE_NAME = "config.py"

SOURCES = (
    "wifi_shell.c",
    "wifi_nm.c",
    "wifi_radio.c",
    "wifi_saved.c",
    "wifi_config.c",
    "wifi_style.c",
    "wifi_draw.c",
    "wifi_ui.c",
    "wifi_manager.c",
)
HEADERS = (
    "wifi_shell.h",
    "wifi_nm.h",
    "wifi_radio.h",
    "wifi_saved.h",
    "wifi_config.h",
    "wifi_style.h",
    "wifi_draw.h",
    "wifi_ui.h",
)

ELEVATED_VARIABLE = "GNUCHANWIFI_ELEVATED"


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
        raise SystemExit("error: this installs into /usr/local; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(Path(__file__).resolve())],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed.

    The check is a real compile of a one-line translation unit rather than a
    test for a file on disk, because where a distribution keeps a header is not
    fixed and what matters is whether the compiler the build will use can find
    it. Xft pulls freetype in through its own header, so freetype's include
    directory is passed for it: the header is present exactly when both
    packages are.
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
    # Xft is what the text is drawn with, and its header is in a package of its
    # own that libx11-dev does not pull in. freetype and fontconfig, which Xft's
    # own header includes, come with it as dependencies.
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


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the X libraries the manager uses.

    x11 is needed for everything and xft for the text, and the two are asked for
    together because Xft's own header includes freetype's: pkg-config is what
    knows where that lives. The fallback names the two libraries and freetype's
    include directory directly, for a machine with no pkg-config, and is right
    on every Debian where the development packages are installed.
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

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    # The manager this install is replacing may be running — it is a program,
    # and the one open in another terminal is the one being replaced. Writing to
    # its path would fail with ETXTBSY, so the new binary is written beside it
    # and renamed over it: a rename swaps the directory entry and never opens
    # the file being replaced.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
    detail(f"installed {BIN_DIR / PROGRAM}")


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root.

    The settings file belongs to the person who logs in, and the manager reads
    it from their home. Under sudo, HOME and the ids are root's, so the original
    user is read back from SUDO_USER — the one piece of the invoking session
    sudo keeps. Returns None when there is nothing sensible to write to, which
    is a machine installing without a login user.
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
    """Put the settings file where the manager will read it.

    A settings file the user has edited is theirs to keep — the manager reads
    the file live — so an existing one is left alone and only a missing one is
    written. That is the same choice the launcher's installer makes, and for the
    same reason: the settings are few and personal, and losing them to an
    install would be losing the colours someone chose.
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

    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the manager would have "
            f"nothing to read"
        )

    directory.mkdir(parents=True, exist_ok=True)

    if target.exists():
        detail(f"kept the existing {target}")
        try:
            os.chown(directory, uid, gid)
        except OSError:
            pass
        return

    shutil.copyfile(CONFIG_SOURCE, target)
    try:
        os.chown(directory, uid, gid)
        os.chown(target, uid, gid)
    except OSError:
        pass

    detail(f"installed {target}")


def uninstall() -> None:
    step("Uninstalling")
    target = BIN_DIR / PROGRAM
    if target.exists():
        target.unlink()
        detail(f"removed {target}")
    note("The settings file under ~/.config/GnuChanWifi/ was left alone.")


def run_program(binary: Path, arguments: list[str]) -> int:
    if not os.environ.get("DISPLAY") and not arguments:
        raise SystemExit("error: DISPLAY is not set, so there is no X server to open on")
    return run([str(binary), *arguments], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanWifi is a Debian X11 manager; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes /usr/local; it is "
            "Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "list", "uninstall")
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
        return run_program(build(), [])

    if action == "list":
        # The one way to see what the manager will offer without opening a
        # window, which is what turns "no networks" from a guess into a line of
        # output.
        return run_program(build(), ["--list"])

    ensure_build_dependencies()
    binary = build()
    install(binary)
    install_config()
    note("")
    note("GnuChanWifi is installed. Start it with Super+W if the window")
    note("manager's own settings script binds it, or run GnuChanWifi.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
