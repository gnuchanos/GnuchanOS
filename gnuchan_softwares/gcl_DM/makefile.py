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
# A display manager owns the X server, so the install writes a small launcher
# that starts X and puts the greeter on it, and the systemd unit runs that
# launcher. The unit owns no terminal: the X server takes a virtual terminal of
# its own, and a unit that also held one would leave two owners for a single
# console — which is what stops Ctrl+Alt+F from switching to a text login.
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
                  "sddm.service", "xdm.service", "gdm.service",
                  "getty@tty1.service", "autovt@tty1.service")

SOURCES = (
    "dm_style.c", "dm_core.c", "dm_form.c", "dm_draw.c", "dm_login.c",
    "dm_input.c", "dm_auth.c", "dm_sessions.c", "dm_session.c", "dm_power.c",
    "GnuChanDM.c",
)
HEADERS = ("dm_module.h", "dm_style.h", "dm_sessions.h", "dm_core.h")

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
# gnuchandm-session - put the greeter on its own X server.
# Written by GnuChanDM's makefile; edits here are overwritten on reinstall.
set -e

# The display this launcher owns is a constant, and it is deliberately not read
# from the environment.
#
# It used to be `DISPLAY="${DISPLAY:-:0}"`, which is wrong in exactly the case
# this script runs in. A display manager is often started from something that
# already has a DISPLAY — a shell, a session, another greeter being replaced —
# and inheriting that number makes the launcher start a *second* X server on a
# *second* display number and a second virtual terminal. The console is then
# switched to one server's VT while the greeter connects to the other's: the
# screen goes black, the greeter draws onto a display nobody is looking at, and
# the next attempt does it again. Xorg.1.log is what that looks like from the
# server's side — a greeter's server logged as display :1 on "VT number 8",
# with vt7 already taken.
#
# A display manager owns :0. That is the number the session entries, the
# cookie, and every client expect, and it is not a value to inherit.
DISPLAY=:0
RUNDIR=/run/gnuchandm
AUTH="$RUNDIR/auth"
LOG=/tmp/gnuchandm-session.log

mkdir -p "$RUNDIR"
chmod 700 "$RUNDIR"

# The greeter's own key file is named before anything talks to the display.
#
# This has to come first, and it is the whole reason it is not where the
# export used to be. X was started with -auth "$AUTH", so a client that has
# not been told about that file cannot authenticate: every check below that
# asks the server a question — is a server already up, has the one just
# started finished starting — would be refused, answer "no", and the launcher
# would take the wrong branch. A reuse check that always says "no server" is a
# launcher that tries to start a second X on a display already in use, and a
# readiness check that always says "not ready" is a launcher that waits out
# its whole timeout and then starts the greeter anyway, twenty seconds of a
# black screen after every boot.
#
# XAUTHORITY is also what the greeter passes on to the session it starts, so
# naming it here is what makes the whole chain use one key file.
if [ ! -f "$AUTH" ]; then
    touch "$AUTH"
    chmod 600 "$AUTH"
fi
export XAUTHORITY="$AUTH"

# If a server is already answering on this display, reuse it. The greeter is
# restarted without the server being torn down, and starting a second X on a
# display already in use fails — which is what leaves a bare screen while the
# service dies and restarts.
if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
    echo "gnuchandm: reusing the X server already on $DISPLAY"
else
    # A fresh key for a fresh server. The old file is emptied rather than
    # removed: a server from a previous run may still be holding it open, and
    # replacing the file under a running X is how a launcher ends up with two
    # cookies for one display and a greeter that cannot tell which is right.
    : > "$AUTH"
    chmod 600 "$AUTH"

    mcookie=$(dd if=/dev/urandom bs=16 count=1 2>/dev/null | od -An -tx1 | tr -d ' \\n')
    xauth -f "$AUTH" add "$DISPLAY" . "$mcookie"

    # X is asked for vt7. Debian runs text logins on tty1 to tty6 and keeps vt7
    # for X, so Ctrl+Alt+F1 to F6 always reach a login and Ctrl+Alt+F7 comes
    # back to the greeter. Every display manager uses this same terminal for the
    # same reason: it is the one no getty holds.
    #
    # It is a request, not a guarantee. If vt7 is taken — by a server from a
    # previous run, by a rival display manager that has not been masked — Xorg
    # takes the next free VT and says so in its log, while the console stays
    # wherever it was. So the console is not switched here at all: it is
    # switched below, to the terminal the server reports it actually used.
    #
    # One line, one command. Split across a continuation the trailing redirect
    # would belong to a command of its own: X would start without its log and,
    # worse, without the & that puts it in the background, which leaves the
    # launcher waiting on the server forever instead of starting the greeter.
    /usr/bin/Xorg "$DISPLAY" vt7 -nolisten tcp -auth "$AUTH" -noreset >>"$LOG" 2>&1 &
    Xorg_pid=$!

    # Wait for the server to answer, not merely to be running. A server that
    # has forked but not yet finished initialising accepts the connection and
    # then fails the handshake, so "the process is alive" is not "the display
    # is usable" — the greeter would be started against a server that is still
    # setting itself up and would exit on its first request.
    ready=0
    for _ in $(seq 1 150); do
        if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
            ready=1
            break
        fi
        if ! kill -0 "$Xorg_pid" 2>/dev/null; then
            echo "gnuchandm: the X server exited; see $LOG" >&2
            exit 1
        fi
        sleep 0.2
    done

    # Out of time with the server still running: say so rather than starting a
    # greeter that will fail its first request. The next attempt starts from a
    # clean display, which is the only way a stuck server is recoverable.
    if [ "$ready" -ne 1 ]; then
        echo "gnuchandm: the X server on $DISPLAY never became ready" >&2
        kill "$Xorg_pid" 2>/dev/null || true
        exit 1
    fi

    # The console is put on the terminal the server actually took, read back
    # from what it wrote when it started. This is what makes the greeter land
    # on the screen the user is looking at: the server may have been given vt7
    # and used vt8, and switching to the number that was asked for instead of
    # the number that was granted leaves the screen showing one X server while
    # the greeter draws on another — the black flash this replaced.
    server_vt=$(sed -n 's/.*using VT number \\([0-9][0-9]*\\).*/\\1/p' "$LOG" | tail -n 1)
    if [ -n "$server_vt" ]; then
        chvt "$server_vt" >/dev/null 2>&1 || true
    fi
fi

export DISPLAY
exec {BIN_DIR / PROGRAM}
"""


def service_text() -> str:
    return f"""\
[Unit]
Description=GnuChanOS Display Manager
Documentation=file:{LAUNCHER}

# A display manager and tty1 getty are mutually exclusive on a real Linux boot.
# If both own the same VT, the X server cannot reclaim the console cleanly and
# the login screen never becomes the active graphical session.
Conflicts=getty@tty1.service autovt@tty1.service
After=systemd-user-sessions.service systemd-udev-settle.service
Before=display-manager.service

# A greeter that cannot start — no usable X server, a broken configuration —
# is tried a few times and then left alone. Restarting it forever would spin a
# dead login screen; a machine with no display manager falls back to its text
# logins, which is a system that can still be repaired.
StartLimitIntervalSec=30
StartLimitBurst=3

[Service]
Type=simple
ExecStart={LAUNCHER}
Restart=on-failure
RestartSec=2
KillMode=mixed

# No StandardInput=tty and no TTYPath, on purpose. The X server this starts
# takes the virtual terminal for itself, and a unit that also held that
# terminal would leave two owners for one console: the kernel could then no
# longer switch virtual terminals, so the screen would keep whatever X left on
# it and no Ctrl+Alt+F key would reach a getty. Owning no terminal is what a
# display manager does.

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
    """Write a file in place, whatever is running from it.

    The file is written beside its destination and then renamed over it. A
    rename replaces the directory entry; it does not open the old file, so it
    succeeds even when the process being replaced is still running. Writing to
    the destination directly would fail with ETXTBSY for the launcher, which
    the running greeter is executing.

    The newline is forced to \\n: a launcher and a systemd unit are read by sh
    and by systemd, and a stray \\r at the end of a line makes the last word of
    every line a different word. On a machine where an editor has saved this
    file with CRLF, the generated shell would be silently wrong.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".new")
    temporary.write_text(text, encoding="utf-8", newline="\n")
    os.replace(str(temporary), str(path))
    path.chmod(mode)
    detail(f"installed {path}")


def systemctl(*arguments: str) -> int:
    return run(["systemctl", *arguments]).returncode


def stop_rival_managers() -> None:
    for name in RIVAL_SERVICES:
        present = run(["systemctl", "list-unit-files", name], capture=True)
        if name not in present.stdout:
            continue
        step(f"Disabling and masking {name}")
        systemctl("stop", name)
        systemctl("disable", "--now", name)
        systemctl("mask", "--now", name)


def enable_service() -> None:
    step("Enabling as the display manager")
    systemctl("daemon-reload")
    systemctl("set-default", "graphical.target")

    alias_path = Path("/etc/systemd/system/display-manager.service")
    if alias_path.exists() or alias_path.is_symlink():
        alias_path.unlink()
    try:
        os.symlink(str(SERVICE_FILE), str(alias_path))
        detail(f"linked {alias_path} -> {SERVICE_FILE}")
    except OSError as exc:
        detail(f"display-manager alias setup skipped: {exc}")

    # The real unit is gnuchandm.service. graphical.target only knows the alias
    # display-manager.service, so the alias must point at the real service and the
    # real service must be enabled explicitly.
    systemctl("enable", "--force", SERVICE_NAME)
    if alias_path.exists() or alias_path.is_symlink():
        systemctl("enable", "--force", "display-manager.service")
    systemctl("restart", SERVICE_NAME)


def disable_service() -> None:
    systemctl("disable", "--now", SERVICE_NAME)
    systemctl("daemon-reload")


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    # The old greeter is what is running this install — a display manager has
    # no one else to be replaced by. Writing to its path would fail with
    # ETXTBSY, so the new binary is put beside it and renamed over it: a rename
    # swaps the directory entry and never opens the file being replaced, and
    # the running process keeps its copy until it is restarted below.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
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
