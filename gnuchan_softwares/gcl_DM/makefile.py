#!/usr/bin/env python3
# =============================================================================
# GnuChanDM - build and install (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# Installs everything the greeter needs, builds it, installs it, and enables it
# as the system's display manager so it starts at boot. Requires root and
# re-runs itself through sudo.
#
#     python3 makefile.py build      compile only
#     python3 makefile.py uninstall  remove and disable everything
#
# A display manager owns the X server, so the install also writes a small
# launcher that starts X on tty1 and puts the greeter on it, and the systemd
# unit runs that launcher.
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

PROGRAM = "GnuChanDM"
BIN_DIR = Path("/usr/local/bin")
LAUNCHER = BIN_DIR / "gnuchandm-session"

XGREETER_FILE = Path("/usr/share/xgreeters/gnuchandm.desktop")
XSESSION_FILE = Path("/usr/share/xsessions/gnuchandm.desktop")

SERVICE_NAME = "gnuchandm.service"
SERVICE_FILE = Path("/etc/systemd/system") / SERVICE_NAME

RIVAL_SERVICES = ("lightdm.service", "lxdm.service", "gdm3.service",
                  "sddm.service", "xdm.service", "gdm.service")

SOURCES = (
    "dm_style.c", "dm_core.c", "dm_form.c", "dm_draw.c", "dm_login.c",
    "dm_input.c", "dm_auth.c", "dm_session.c", "dm_power.c", "GnuChanDM.c",
)
HEADERS = ("dm_module.h", "dm_style.h", "dm_core.h")

ELEVATED_VARIABLE = "GNUCHANDM_ELEVATED"


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(command, capture=False, environment=None, cwd=None):
    if capture:
        return subprocess.run(command, check=False, capture_output=True,
                              text=True, env=environment, cwd=cwd)
    return subprocess.run(command, check=False, text=True, env=environment, cwd=cwd)


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
        raise SystemExit("error: this installs packages and writes /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(Path(__file__).resolve())], environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def headers_present() -> bool:
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    probe = "#include <X11/Xlib.h>\n#include <security/pam_appl.h>\n"
    result = subprocess.run([gcc, "-E", "-xc", "-"], input=probe, text=True,
                            capture_output=True, check=False)
    return result.returncode == 0


def program_exists(name: str) -> bool:
    if "/" in name:
        return os.access(name, os.X_OK)
    return shutil.which(name) is not None


def apt_install(packages) -> bool:
    if not packages:
        return True
    run(["apt-get", "update", "-o", "Acquire::Retries=3"], capture=True,
        environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *packages]
    detail("running: " + " ".join(command))
    return run(command, environment=apt_environment()).returncode == 0


def ensure_packages() -> None:
    needed = []
    if not headers_present():
        needed += ["libx11-dev", "libpam0g-dev"]
    if shutil.which("gcc") is None:
        needed.append("gcc")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if shutil.which("xauth") is None:
        needed.append("xauth")
    if shutil.which("Xorg") is None:
        needed += ["xserver-xorg", "xserver-xorg-legacy"]
    if shutil.which("xdpyinfo") is None:
        needed.append("x11-utils")
    if not any(program_exists(t) for t in ("x-terminal-emulator", "xterm",
                                           "gnome-terminal", "konsole",
                                           "alacritty", "kitty")):
        needed.append("xterm")
    if not needed:
        return
    step("Installing what is missing")
    detail("missing: " + ", ".join(needed))
    if not apt_install(tuple(needed)):
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))


def flags() -> tuple:
    pkg_config = shutil.which("pkg-config")
    cflags, libs = [], []
    if pkg_config is not None:
        c = run([pkg_config, "--cflags", "x11"], capture=True)
        l = run([pkg_config, "--libs", "x11"], capture=True)
        if c.returncode == 0:
            cflags += c.stdout.split()
        if l.returncode == 0:
            libs += l.stdout.split()
    if not libs:
        libs = ["-lX11"]
    libs.append("-lpam")
    return cflags, libs


def check_sources() -> None:
    missing = [n for n in (*SOURCES, *HEADERS) if not (ROOT / n).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM
    cflags, libs = flags()

    command = ["gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
               "-O2", "-D_DEFAULT_SOURCE", "-I", str(ROOT), *cflags]
    command += [str(ROOT / n) for n in SOURCES]
    command += ["-o", str(output)]
    command += libs

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def launcher_text() -> str:
    return f"""#!/bin/sh
# gnuchandm-session - start X on tty1, then run the greeter on it.
# Written by GnuChanDM's makefile; edits here are overwritten on reinstall.
set -e

DISPLAY="${{DISPLAY:-:0}}"
RUNDIR=/run/gnuchandm
AUTH="$RUNDIR/auth"
LOG=/tmp/gnuchandm-session.log

mkdir -p "$RUNDIR"
chmod 700 "$RUNDIR"
rm -f "$AUTH"
touch "$AUTH"
chmod 600 "$AUTH"

mcookie=$(dd if=/dev/urandom bs=16 count=1 2>/dev/null | od -An -tx1 | tr -d ' \\n')
xauth -f "$AUTH" add "$DISPLAY" . "$mcookie"
export XAUTHORITY="$AUTH"
export DISPLAY

/usr/bin/Xorg "$DISPLAY" vt1 -nolisten tcp -auth "$AUTH" -noreset \\
    >>"$LOG" 2>&1 &
Xorg_pid=$!

for _ in $(seq 1 100); do
    if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then break; fi
    if ! kill -0 "$Xorg_pid" 2>/dev/null; then
        echo "gnuchandm: the X server exited; see $LOG" >&2
        exit 1
    fi
    sleep 0.2
done

exec {BIN_DIR / PROGRAM}
"""


def service_text() -> str:
    return f"""\
[Unit]
Description=GnuChanOS Display Manager
Documentation=file:{LAUNCHER}
Conflicts=getty@tty1.service
After=getty@tty1.service systemd-user-sessions.service systemd-udev-settle.service
Before=display-manager.service

[Service]
Type=simple
ExecStart={LAUNCHER}
Restart=always
RestartSec=1
KillMode=mixed
StandardInput=tty
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes

[Install]
Alias=display-manager.service
WantedBy=graphical.target
"""


def greeter_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Name=GnuChanDM",
        "Comment=GnuchanOS login screen",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Type=Application",
        "",
    ])


def session_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Name=GnuChanWM",
        "Comment=GnuchanOS window manager",
        f"Exec={BIN_DIR / 'GnuChanWM'}",
        f"TryExec={BIN_DIR / 'GnuChanWM'}",
        "Type=Application",
        "DesktopNames=GnuChanWM",
        "",
    ])


def write(path: Path, text: str, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".new")
    temporary.write_text(text, encoding="utf-8")
    shutil.move(str(temporary), str(path))
    path.chmod(mode)
    detail(f"installed {path}")


def systemctl(*arguments: str) -> int:
    return run(["systemctl", *arguments]).returncode


def stop_rival_managers() -> None:
    for name in RIVAL_SERVICES:
        present = run(["systemctl", "list-unit-files", name], capture=True)
        if name not in present.stdout:
            continue
        step(f"Disabling {name}")
        systemctl("disable", "--now", name)


def enable_service() -> None:
    step("Enabling as the display manager")
    systemctl("daemon-reload")
    systemctl("enable", SERVICE_NAME)
    systemctl("restart", SERVICE_NAME)


def disable_service() -> None:
    systemctl("disable", "--now", SERVICE_NAME)
    systemctl("daemon-reload")


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    shutil.copyfile(binary, BIN_DIR / PROGRAM)
    (BIN_DIR / PROGRAM).chmod(0o755)
    detail(f"installed {BIN_DIR / PROGRAM}")

    write(LAUNCHER, launcher_text(), 0o755)
    write(XGREETER_FILE, greeter_entry(), 0o644)
    write(XSESSION_FILE, session_entry(), 0o644)
    write(SERVICE_FILE, service_text(), 0o644)

    if shutil.which("systemctl") is not None:
        stop_rival_managers()
        enable_service()
    else:
        note("")
        note(f"systemctl is not present; start the greeter by hand with {LAUNCHER}")


def uninstall() -> None:
    step("Uninstalling")
    if shutil.which("systemctl") is not None:
        disable_service()
    for target in (BIN_DIR / PROGRAM, LAUNCHER, XGREETER_FILE, XSESSION_FILE,
                   SERVICE_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(f"error: this is a Debian X11 display manager; this is "
                         f"{platform.system()}")
    if not is_debian():
        raise SystemExit("error: this installs with apt-get and writes Debian's "
                         "session directories; it is Debian-only by design")

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "uninstall")
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

    check_sources()
    ensure_packages()
    binary = build()
    install(binary)
    note("")
    note("GnuChanDM is installed and enabled as the display manager.")
    note("It starts GnuChanWM as the session.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
