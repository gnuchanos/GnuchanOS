#!/usr/bin/env python3
# =============================================================================
# GnuChanNotification - build and install (Debian)
#     python3 makefile.py            build and install
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and run the daemon here
#     python3 makefile.py test       compile and show one test notification
#     python3 makefile.py uninstall  remove the binary and the desktop entries
#
# GnuChanNotification is a notification daemon in the shape of dunst: it holds
# the name org.freedesktop.Notifications on the session bus and draws what other
# programs send it. Its libraries are the X stack (the window, the font, the
# shape) and libdbus, which every desktop that has a session bus already has.
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
BUILD = REPO_ROOT / "_temp" / "gcl_NOTIFICATION-build"

PROGRAM = "GnuChanNotification"
BIN_DIR = Path("/usr/local/bin")
APPS_DIR = Path("/usr/share/applications")
AUTOSTART_DIR = Path("/etc/xdg/autostart")
DESKTOP_FILE = APPS_DIR / "gnuchannotification.desktop"
AUTOSTART_FILE = AUTOSTART_DIR / "gnuchannotification.desktop"

CONFIG_SOURCE = (ROOT / "GnuChanNotification_config"
                 / "GnuChanNotification.py")
CONFIG_DIR_NAME = "GnuChanNotification"
CONFIG_FILE_NAME = "GnuChanNotification.py"

LOGO_SOURCE = REPO_ROOT / "assets" / "logo.png"

SOURCES = (
    "notif_parser.c",
    "notif_config.c",
    "notif_shape.c",
    "notif_image.c",
    "notif_item.c",
    "notif_render.c",
    "notif_dbus.c",
    "GnuChanNotification.c",
)
HEADERS = (
    "notif_config.h",
    "notif_parser.h",
    "notif_shape.h",
    "notif_image.h",
    "notif_item.h",
    "notif_render.h",
    "notif_dbus.h",
)

ELEVATED_VARIABLE = "GNUCHANNOTIFICATION_ELEVATED"

PACKAGE_CHECKS = (
    ("X11/Xlib.h", "libx11-dev"),
    ("X11/Xft/Xft.h", "libxft-dev"),
    ("ft2build.h", "libfreetype-dev"),
    ("X11/extensions/shape.h", "libxext-dev"),
    ("Imlib2.h", "libimlib2-dev"),
    ("dbus/dbus.h", "libdbus-1-dev"),
)

PKG_CONFIG_MODULES = ("x11", "xft", "xext", "fontconfig", "freetype2",
                      "imlib2", "dbus-1")
FALLBACK_LIBS = ["-lX11", "-lXft", "-lXext", "-lfontconfig", "-lfreetype",
                 "-lImlib2", "-ldbus-1"]


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


def freetype_includes() -> tuple[str, ...]:
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


def dbus_includes() -> tuple[str, ...]:
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        result = run([pkg_config, "--cflags-only-I", "dbus-1"], capture=True)
        if result.returncode == 0:
            paths = tuple(word[2:] for word in result.stdout.split()
                          if word.startswith("-I"))
            if paths:
                return paths
    return ("/usr/include/dbus-1.0",
            "/usr/lib/x86_64-linux-gnu/dbus-1.0/include")


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(command, input=f"#include <{header}>\n",
                            text=True, capture_output=True, check=False)
    return result.returncode == 0


def extra_includes() -> tuple[str, ...]:
    return (*freetype_includes(), *dbus_includes())


def missing_build_dependencies() -> list[str]:
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    for header, package in PACKAGE_CHECKS:
        if not header_present(header, extra_includes()):
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
    """The compiler and linker flags for the X stack and for D-Bus."""
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", *PKG_CONFIG_MODULES],
                     capture=True)
        libs = run([pkg_config, "--libs", *PKG_CONFIG_MODULES], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    fallback_includes = ["-I/usr/include"]
    fallback_includes += ["-I" + path for path in extra_includes()]
    return fallback_includes, list(FALLBACK_LIBS)


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


def desktop_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        "Name=GnuChanNotification",
        "Comment=GnuchanOS notification daemon",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "Categories=System;Utility;",
        "Icon=notification-message-im",
        "NoDisplay=true",
        "",
    ])


def autostart_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        "Name=GnuChanNotification",
        "Comment=GnuchanOS notification daemon",
        f"Exec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "X-GNOME-Autostart-enabled=true",
        "",
    ])


def invoking_user() -> tuple[Path, int, int] | None:
    name = os.environ.get("SUDO_USER")
    if not name:
        return None
    try:
        import pwd
        info = pwd.getpwnam(name)
        return Path(info.pw_dir), info.pw_uid, info.pw_gid
    except (ImportError, KeyError):
        return None


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    APPS_DIR.mkdir(parents=True, exist_ok=True)
    AUTOSTART_DIR.mkdir(parents=True, exist_ok=True)

    target = BIN_DIR / PROGRAM
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(target))
    detail(f"installed {target}")

    for path, text in ((DESKTOP_FILE, desktop_entry()),
                       (AUTOSTART_FILE, autostart_entry())):
        temporary = path.with_name(path.name + ".new")
        temporary.write_text(text, encoding="utf-8")
        os.replace(str(temporary), str(path))
        path.chmod(0o644)
        detail(f"installed {path}")

    install_config()


def install_config() -> None:
    if not CONFIG_SOURCE.is_file():
        return
    user = invoking_user()
    if user is None:
        detail("skipped the config: no login user to install it for")
        return

    home, uid, gid = user
    directory = home / ".config" / CONFIG_DIR_NAME
    directory.mkdir(parents=True, exist_ok=True)

    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the daemon would have "
            f"nothing to read"
        )

    script = directory / CONFIG_FILE_NAME
    if script.exists():
        script.unlink()
    shutil.copyfile(CONFIG_SOURCE, script)
    try:
        os.chown(script, uid, gid)
    except OSError:
        pass
    detail(f"installed {script}")

    if LOGO_SOURCE.is_file():
        logo = directory / "logo.png"
        shutil.copyfile(LOGO_SOURCE, logo)
        try:
            os.chown(logo, uid, gid)
        except OSError:
            pass
        detail(f"installed {logo}")

    try:
        os.chown(directory, uid, gid)
    except OSError:
        pass


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, DESKTOP_FILE, AUTOSTART_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")
    note(f"The settings under ~/.config/{CONFIG_DIR_NAME}/ were left alone.")


def run_program(binary: Path, extra_args: tuple[str, ...] = ()) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set")
    step("Starting on the current display")
    return run([str(binary), *extra_args], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanNotification is a Debian X11 program; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's system "
            "directories; it is Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "test", "uninstall")
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

    if action == "test":
        return run_program(build(), ("--test",))

    binary = build()
    install(binary)
    note("")
    note("GnuChanNotification is installed. It starts with the session, and")
    note(f"{BIN_DIR / PROGRAM} runs it from a shell.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
