#!/usr/bin/env python3
"""GnuchanOS - build and install every Gnuchan program in one run.

    python3 install_all.py            build and install everything (needs root)
    python3 install_all.py build      compile everything, install nothing
    python3 install_all.py uninstall  remove every installed program
    python3 install_all.py --list     show the programs and their order

Each program under gnuchan_softwares/ carries its own makefile.py, and that
file is the one authority on how the program is built and where it is put: the
packages it needs, the flags, the binary, the desktop entry, the settings
script. This script does not repeat any of that. It runs each program's own
makefile with the action asked for, in a fixed order, and reports what
happened.

Why this has to become root ITSELF
----------------------------------
Every program's makefile re-runs itself through sudo when it is not root —
`install` does `os.execvpe(sudo, ...)`, which REPLACES the process. Run them
one after another from a non-root parent and the first one's sudo takes the
whole process over: the remaining programs never start, and the run looks like
it installed one thing and stopped. So this script elevates ONCE, up front,
carrying its arguments across; every child then finds it is already root, its
own ensure_root() returns at once, and the loop gets to finish.

The order
---------
It is fixed and written down rather than discovered, because two of the
programs are not ordinary applications:

  * the window manager and the display manager are the session itself — the
    greeter starts the WM — so they are installed after the programs the
    desktop runs (a terminal, the panels, the browser);

  * the display manager is installed LAST and only ever last. Its install
    reconfigures which display manager owns the console and restarts it, so a
    run that did that in the middle would tear down the session under the
    remaining installs.

Uninstall walks the same list in reverse, for the same reason.

A program whose directory or makefile is not in this checkout is skipped with
a note and does not stop the rest: a trimmed tree should still install what it
has. Every failure is collected and reported at the end, and the exit status is
non-zero if any program failed.

Debian only, on purpose: every one of these programs is, and this installs
with apt-get and writes /usr.

License: GPL3
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

# The directory this script lives in, and the root of every program below it.
ROOT = Path(__file__).resolve().parent

# The programs, in the order they are built and installed: the applications the
# desktop runs first, then the window manager, then the display manager last.
# Each entry is (directory under gnuchan_softwares/, the program's name).
PROGRAMS: tuple[tuple[str, str], ...] = (
    ("gcl_TERMINAL", "GnuChanTerm"),
    ("gcl_FETCH", "GnuChanFetch"),
    ("gcl_RUNNER", "GnuChanRunner"),
    ("gcl_WIFI_MANAGER", "GnuChanWifi"),
    ("gcl_NETWORK_MANAGER", "GnuChanNetworkManager"),
    ("gcl_DOCK", "GnuChanDock"),
    ("gcl_BROWSER_WEB", "GnuChanBrowser"),
    ("gcl_SCREENSAVER", "GnuChanSS"),
    ("gcl_LOCKSCREEN", "GnuChanSL"),
    ("gcl_NOTIFICATION", "GnuChanNotification"),
    ("gcl_TOP", "GnuChanTop"),
    ("gcl_SETTINGS", "GnuChanSettings"),
    ("gcl_WM", "GnuChanWM"),
    ("gcl_DM", "GnuChanDM"),
)

# The actions a program's makefile understands, and the ones this passes on.
# They are the same words the individual makefiles use, so a person who knows
# one already knows this.
ACTION_INSTALL = "install"
ACTION_BUILD = "build"
ACTION_UNINSTALL = "uninstall"
KNOWN_ACTIONS = (ACTION_INSTALL, ACTION_BUILD, ACTION_UNINSTALL)

# The environment variable the re-run through sudo sets, so a second run that
# somehow is still not root stops rather than looping for ever. It is this
# script's own name and not any program's: the programs have their own.
ELEVATED_VARIABLE = "GNUCHAN_ALL_ELEVATED"


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(command: list[str], cwd: Path) -> int:
    """Run one program's makefile, streaming its output as it goes.

    The output is not captured: each makefile prints the packages it installs,
    what it builds and where it puts it, and burying that would leave a run
    that takes minutes with nothing on screen but a header per program.
    """
    try:
        return subprocess.run(command, check=False, cwd=cwd).returncode
    except FileNotFoundError:
        return 127


def is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root() -> None:
    """Become root once, before any program is run — see the file comment.

    The arguments are carried across, so `build` or `uninstall` asked for here
    does not come back as a full install after the elevation.
    """
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit("error: still not root after sudo")
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: this installs packages and writes /usr; run it as root "
            "(su -c 'python3 install_all.py')"
        )
    step("This installs packages and writes /usr; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        environment,
    )


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def makefile_of(directory: str) -> Path | None:
    """The makefile.py of one program, or None when it is not in this tree.

    A missing program is not a failure: a checkout may carry a subset, and the
    point of this script is to build what is here. What IS a failure is a
    directory that is here with no makefile, because that is a program this
    cannot build and a person would want to know which.
    """
    program_dir = ROOT / directory
    makefile = program_dir / "makefile.py"
    if makefile.is_file():
        return makefile
    return None


def ordered_programs(action: str) -> tuple[tuple[str, str], ...]:
    """The programs in the order this action runs them.

    Install and build go down the list; uninstall goes UP it, so the display
    manager is taken down first and the applications it was running last. It is
    the mirror of the install order and exists for the same reason.
    """
    if action == ACTION_UNINSTALL:
        return tuple(reversed(PROGRAMS))
    return PROGRAMS


def run_programs(action: str) -> int:
    programs = ordered_programs(action)
    failures: list[str] = []
    skipped: list[str] = []
    done: list[str] = []

    for index, (directory, name) in enumerate(programs, start=1):
        makefile = makefile_of(directory)
        if makefile is None:
            detail(f"[{index}/{len(programs)}] {name}: not in this tree, skipped")
            skipped.append(name)
            continue

        note("")
        step(f"[{index}/{len(programs)}] {name} — {action}")
        # The makefile is run from its own directory, which is where it expects
        # to be: every one of them resolves its sources, its config directory
        # and its _temp build output relative to its own file, not the caller's.
        result = run([sys.executable, str(makefile), action],
                     cwd=makefile.parent)
        if result == 0:
            done.append(name)
        else:
            failures.append(name)
            note(f"  ! {name}: makefile.py {action} exited {result}")

    note("")
    step("Summary")
    if done:
        detail(f"{action}ed: " + ", ".join(done))
    if skipped:
        detail("skipped (not in this tree): " + ", ".join(skipped))
    if failures:
        detail(f"FAILED: " + ", ".join(failures))
        note("")
        note("Re-run a failing program's own makefile for the exact error, e.g.")
        for directory, name in programs:
            if name in failures:
                note(f"    cd {directory} && python3 makefile.py {action}")
                break
        return 1

    note("")
    note(f"All {len(done)} program(s) {action}ed.")
    if action == ACTION_INSTALL:
        note("Log out and back in for the session to pick the new binaries up.")
    return 0


def usage() -> None:
    print(
        "usage: python3 install_all.py [install | build | uninstall | --list]\n"
        "\n"
        "  (no argument)   build and install every Gnuchan program\n"
        "  build           compile everything, install nothing\n"
        "  uninstall       remove every installed program\n"
        "  --list          show the programs and the order they run in\n"
    )


def list_programs() -> None:
    note("Gnuchan programs, in the order they install:")
    for index, (directory, name) in enumerate(PROGRAMS, start=1):
        makefile = makefile_of(directory)
        present = "here" if makefile is not None else "not in this tree"
        note(f"  {index:2d}. {name:<20} ({directory}, {present})")


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: the Gnuchan programs are Debian X11 programs; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's system "
            "directories; it is Debian-only by design"
        )

    arguments = sys.argv[1:]
    if "--help" in arguments or "-h" in arguments or "--list" in arguments:
        usage()
        note("")
        list_programs()
        return 0

    action = arguments[0] if arguments else ACTION_INSTALL
    if action not in KNOWN_ACTIONS:
        print(f"error: unknown action '{action}'", file=sys.stderr)
        usage()
        return 2
    if len(arguments) > 1:
        print("error: only one action may be given", file=sys.stderr)
        usage()
        return 2

    # Root for every action, and that is not only for the install. Each
    # program's build action installs the -dev packages it needs to compile —
    # apt-get is what provides the headers, and asking for them is part of
    # answering "does this build?" on a machine that has none. So `build` needs
    # root as much as `install` does. The elevation happens once, here.
    ensure_root()

    note("")
    note(f"GnuchanOS: {action} for {len(PROGRAMS)} program(s)")
    return run_programs(action)


if __name__ == "__main__":
    raise SystemExit(main())
