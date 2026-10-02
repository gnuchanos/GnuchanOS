#!/usr/bin/env python3
# =============================================================================
# GnuChanSL - build and install (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py            build and install the program
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py uninstall  remove the binary and the desktop entry
#
# GnuChanSL is a C program, built against X11, Xft (the text) and PAM (the
# password check). PAM is not optional: a lock screen that could not check a
# password would be a lock screen that opens for anyone, so libpam-dev is a
# dependency of the build, unlike GnuChanSS's optional D-Bus.
#
# A PAM policy for the service "gnuchansl" is installed when none is there. It
# is kept separate from the display manager's "gnuchandm" — see sl_auth.h — and
# the file it writes is the common include, which is what `login` uses.
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
BUILD = ROOT.parent.parent / "_temp" / "gcl_LOCKSCREEN-build"

PROGRAM = "GnuChanSL"
BIN_DIR = Path("/usr/local/bin")
APPS_DIR = Path("/usr/share/applications")
DESKTOP_FILE = APPS_DIR / "gnuchansl.desktop"

# The PAM policy this program reads, and the common include it points at. The
# service name is the program's own, kept apart from the display manager's, so a
# change made for one does not become a change for the other.
PAM_DIR = Path("/etc/pam.d")
PAM_SERVICE = "gnuchansl"
PAM_COMMON = "common-auth"

CONFIG_SOURCE = ROOT / "GnuChanSL_config" / "GnuChanSL.py"
CONFIG_DIR_NAME = "GnuChanSL"
CONFIG_FILE_NAME = "GnuChanSL.py"

SOURCES = (
    "sl_parser.c",
    "sl_config.c",
    "sl_auth.c",
    "GnuChanSL.c",
)
HEADERS = (
    "sl_config.h",
    "sl_parser.h",
    "sl_auth.h",
)

ELEVATED_VARIABLE = "GNUCHANSL_ELEVATED"


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
        raise SystemExit("error: this installs packages and writes /etc and "
                         "/usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed. A real compile of a one-line
    translation unit rather than a test for a file, because where a distribution
    keeps a header is not fixed."""
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(command, input=f"#include <{header}>\n",
                            text=True, capture_output=True, check=False)
    return result.returncode == 0


def x11_headers_present() -> bool:
    return header_present("X11/Xlib.h")


def ft2_includes() -> tuple[str, ...]:
    """Where freetype keeps its headers, which is not /usr/include.

    Xft's own header includes ft2build.h on its third line, so a machine with
    libxft-dev and no freetype cannot be compiled against. pkg-config knows the
    path; the Debian fallback is used for a machine without it.
    """
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


def xft_headers_present() -> bool:
    return header_present("X11/Xft/Xft.h", ft2_includes())


def freetype_headers_present() -> bool:
    return header_present("ft2build.h", ft2_includes())


def pam_headers_present() -> bool:
    return header_present("security/pam_appl.h")


def missing_build_dependencies() -> list[str]:
    """Which packages this machine is missing to build the program.

    Each entry is chosen by asking the compiler about the HEADER the package
    provides. libpam0g-dev is the password check and cannot be absent: see the
    file comment.
    """
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not x11_headers_present():
        needed.append("libx11-dev")
    if not xft_headers_present():
        needed.append("libxft-dev")
    if not freetype_headers_present():
        needed.append("libfreetype-dev")
    if not pam_headers_present():
        needed.append("libpam0g-dev")
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


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the libraries the program uses.

    x11 is the window, xft the text; pam is the password check. pkg-config is
    what knows where the includes live; the fallback names the libraries
    directly, with freetype's include directory, for a machine without it.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", "x11", "xft"], capture=True)
        libs = run([pkg_config, "--libs", "x11", "xft"], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            # PAM is not one of the pkg-config modules above — it has no .pc
            # entry — but it is not optional either, so it is named here.
            # Without it the link fails on pam_start and the whole build stops,
            # which is what happened before this line was added.
            return cflags.stdout.split(), libs.stdout.split() + ["-lpam"]

    includes = ["-I/usr/include"]
    includes += ["-I" + path for path in ft2_includes()]
    return (includes, ["-lX11", "-lXft", "-lpam"])


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


def desktop_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        "Name=GnuChanSL",
        "Comment=GnuchanOS lock screen",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "Categories=System;Security;",
        "Icon=system-lock-screen",
        "",
    ])


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root."""
    name = os.environ.get("SUDO_USER")
    if not name:
        return None
    try:
        import pwd
        info = pwd.getpwnam(name)
        return Path(info.pw_dir), info.pw_uid, info.pw_gid
    except (ImportError, KeyError):
        return None


def install_pam_service() -> None:
    """Write the PAM policy for this program when none is there.

    The file is the common include, which is what `login` uses: a lock screen
    that checked a password differently from a login would be a lock screen
    someone could get past. An existing file is left alone, so an administrator
    who has written their own policy keeps it.
    """
    target = PAM_DIR / PAM_SERVICE
    if target.exists():
        detail(f"kept the existing {target}")
        return
    PAM_DIR.mkdir(parents=True, exist_ok=True)
    content = (
        f"# GnuChanSL - the lock screen's own policy.\n"
        f"# Written by the installer because none was here. It is the same\n"
        f"# include `login` uses, so the password check is the system's.\n"
        f"@include {PAM_COMMON}\n"
        f"@include common-account\n"
    )
    target.write_text(content, encoding="utf-8")
    target.chmod(0o644)
    detail(f"wrote {target}")


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

    temporary = APPS_DIR / "gnuchansl.desktop.new"
    temporary.write_text(desktop_entry(), encoding="utf-8")
    os.replace(str(temporary), str(DESKTOP_FILE))
    DESKTOP_FILE.chmod(0o644)
    detail(f"installed {DESKTOP_FILE}")

    install_pam_service()
    install_config()


def install_config() -> None:
    """Put the settings file where the program reads it."""
    if not CONFIG_SOURCE.is_file():
        return
    user = invoking_user()
    if user is None:
        detail("skipped the config: no login user to install it for")
        return

    home, uid, gid = user
    directory = home / ".config" / CONFIG_DIR_NAME
    file_target = directory / CONFIG_FILE_NAME

    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the lock screen would "
            f"have nothing to read"
        )

    directory.mkdir(parents=True, exist_ok=True)
    if file_target.exists():
        file_target.unlink()
        detail(f"removed the old {file_target}")

    shutil.copyfile(CONFIG_SOURCE, file_target)
    try:
        os.chown(directory, uid, gid)
        os.chown(file_target, uid, gid)
    except OSError:
        pass
    detail(f"installed {file_target}")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, DESKTOP_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")
    # The PAM policy is left in place: removing a file under /etc that an
    # administrator may have edited is not this installer's to do.
    note(f"The PAM policy {PAM_DIR / PAM_SERVICE} was left in place.")
    note(f"The settings under ~/.config/{CONFIG_DIR_NAME}/ were left alone.")


def run_program(binary: Path) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set, so there is no X server to run on")
    step("Starting on the current display")
    return run([str(binary)], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanSL is a Debian X11 lock screen; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's system "
            "directories; it is Debian-only by design"
        )

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
    note("GnuChanSL is installed. It is in the applications menu, and")
    note(f"{BIN_DIR / PROGRAM} locks the screen from a shell.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
