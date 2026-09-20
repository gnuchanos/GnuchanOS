"""The install itself: build the theme, put it where it is found, select it.

This is the only module that knows the order the steps go in, and the order
matters in one place: the theme has to exist on disk before anything is told to
use it, because a session pointed at a theme that is not there falls back to the
default cursor and stays there until the next login.

Nothing here is fatal. A machine with no X server, no GTK and no settings daemon
still gets a theme written into its home directory, which is what the next login
will pick up; the steps that could not run are reported and the run continues.
The alternative - stopping at the first missing program - leaves a half-written
theme behind, which is the one outcome worse than doing nothing.
"""

from __future__ import annotations

import os
import shutil

from . import names, palette, profiles, select, system, verify
from .install import (
    BLOCK_BEGIN,
    BLOCK_END,
    cursor_resource_block,
    default_index_text,
    default_theme_dir,
    merge_block,
    theme_dirs,
    write_merged,
    xresources_file,
)
from .theme import THEME_NAME, write_theme


class Reporter:
    """The two kinds of progress line the helpers below expect.

    The build takes a bare callable and the selection steps take a logger with
    a ``detail`` method, because the two were written for different callers.
    Adapting here keeps :mod:`select` from having to know about either.
    """

    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet

    def __call__(self, message: str) -> None:
        if not self.quiet:
            print(message, flush=True)

    def detail(self, message: str) -> None:
        if not self.quiet:
            print(f"    {message}", flush=True)

    def step(self, message: str) -> None:
        if not self.quiet:
            print(f"==> {message}", flush=True)

    def note(self, message: str) -> None:
        if not self.quiet:
            print(message, flush=True)


def install(log: Reporter) -> dict[str, int]:
    """Write the theme everywhere it is looked for, and select it everywhere.

    ``log`` prints the progress: the build is silent for a second or so per
    state, and a run with no output looks like a run that has hung.
    """
    log.step(f"Building the {THEME_NAME} cursor theme")
    # The theme is drawn once and the second location is linked to the first
    # rather than drawn twice. The files are the frames of an animation at three
    # sizes, so building both copies is fourteen megabytes of the same cursor,
    # and a link costs one inode. The directory is linked rather than each
    # cursor inside it, so the two locations are one theme by construction and
    # cannot drift apart.
    primary, *mirrors = theme_dirs()
    log.note(f"    into {primary}")
    totals = write_theme(primary, log)

    for mirror in mirrors:
        if mirror.is_symlink():
            mirror.unlink()
        elif mirror.is_dir():
            shutil.rmtree(mirror)
        mirror.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.symlink(primary, mirror)
            log.note(f"    linked {mirror}")
        except OSError:
            # A filesystem that refuses links gets a second copy after all: a
            # theme in only one of the two places is a theme half the desktop
            # cannot see, and that is worse than the disk the copy costs.
            log.note(f"    {mirror} could not be linked; writing a second copy")
            write_theme(mirror, log)

    # The theme a program finds when it is told nothing. This is the step that
    # makes the cursor change everywhere rather than in the programs that
    # happen to have a configuration file of their own.
    default = default_theme_dir()
    default.mkdir(parents=True, exist_ok=True)
    (default / "index.theme").write_text(default_index_text(), encoding="utf-8")
    log.note(f"    selected as the default in {default}")

    log.step("Telling the session")
    select.gtk_set(log)
    select.gsettings_set(log)
    # The LXDE files come before the resource load, because one of them is what
    # asks the session to load the resource database at all: without the
    # autostart line, the Xcursor.theme written here is read by nothing on an
    # LXDE session, and the window manager and the desktop keep the cursor they
    # started with while the GTK programs change.
    profiles.install(log)
    write_merged(log, xresources_file(), cursor_resource_block())
    select.xrdb_load(log)
    system.install(log)
    return totals


def check(log: Reporter) -> list[str]:
    """Verify the installed theme, returning the problems found."""
    problems: list[str] = []
    checked: set[str] = set()
    for root in theme_dirs():
        # The mirror is a link to the theme that was just checked, so following
        # it would check the same files twice and report every problem twice.
        resolved = str(root.resolve()) if root.is_symlink() else str(root)
        if resolved in checked:
            log.note(f"    {root}: linked to {resolved}")
            continue
        checked.add(resolved)
        if not root.is_dir():
            problems.append(f"{root} is not installed")
            continue
        problems.extend(verify.check_theme(root, names.REQUIRED, palette.SIZES))
        log.note(f"    {root}: {verify.summary(root)}")
    index = default_theme_dir() / "index.theme"
    if not index.is_file():
        problems.append(
            f"{index} is missing; a program with no cursor setting of its own "
            "will use the black default"
        )
    return problems + profiles.check() + system.check()


def uninstall(log: Reporter) -> None:
    """Remove the theme and the settings, and put back what was there before."""
    for root in theme_dirs():
        # A link and a directory need different removal, and rmtree refuses a
        # link outright - which is the case the second location is in whenever
        # the build managed to link it.
        if root.is_symlink():
            root.unlink()
            log.detail(f"removed the link {root}")
        elif root.is_dir():
            shutil.rmtree(root)
            log.detail(f"removed {root}")

    index = default_theme_dir() / "index.theme"
    if index.is_file():
        index.unlink()
        log.detail(f"removed {index}")
    # The directory itself is left alone when something else is in it: it is not
    # ours, and another theme may be installed as the default.
    parent = default_theme_dir()
    if parent.is_dir() and not any(parent.iterdir()):
        parent.rmdir()

    path = xresources_file()
    if path.is_file():
        merged = merge_block(path.read_text(encoding="utf-8"), BLOCK_BEGIN,
                             BLOCK_END, "")
        if merged.strip():
            path.write_text(merged, encoding="utf-8")
            log.detail(f"removed our block from {path}")
        else:
            path.unlink()
            log.detail(f"removed {path}")
    backup = path.with_name(path.name + ".gnuchan-backup")
    if backup.exists() and not path.exists():
        shutil.move(str(backup), str(path))
        log.detail(f"restored {path}")

    profiles.uninstall(log)
    system.uninstall(log)

    log.note("    the GTK settings were left alone; clear gtk-cursor-theme-name")
    log.note("    in ~/.config/gtk-3.0/settings.ini to change them")


def main(report=None) -> int:
    """Build, install, select and verify. The whole job, and no options.

    There is nothing to choose. The artwork, the names, the sizes and the
    destinations are decisions made in the modules around this one, and a flag
    that changed one of them would be a way to install a theme that is not the
    one in the source. Running the script again rebuilds and replaces what is
    there, so a run after an edit is a run that shows the edit - which is what
    makes "no options" a feature rather than a gap.
    """
    log = Reporter() if report is None else Adapted(report)

    totals = install(log)

    log.step("Checking the result")
    problems = check(log)
    for problem in problems:
        log.note(f"  ! {problem}")

    log.note("")
    log.note(
        f"{totals['names']} cursor name(s) over {totals['states']} drawing(s), "
        f"{totals['frames']} frame(s), {totals['links']} of the names linked."
    )
    log.note("")
    log.note("The cursor is purple in every state, round, and animated: it")
    log.note("pulses while it is idle and spins while a program is working.")
    log.note("Log out and back in if a program does not pick it up at once.")
    return 1 if problems else 0


class Adapted:
    """A reporter that forwards every line to the caller's function.

    The icon installer takes a callable that flushes each line, and this script
    accepts the same shape so the two can be driven the same way. Every method
    forwards to that one function rather than printing directly, which keeps the
    choice of where the output goes in the caller.
    """

    def __init__(self, report) -> None:
        self._report = report

    def __call__(self, message: str) -> None:
        self._report(message)

    def detail(self, message: str) -> None:
        self._report(f"    {message}")

    def step(self, message: str) -> None:
        self._report(f"==> {message}")

    def note(self, message: str) -> None:
        self._report(message)
