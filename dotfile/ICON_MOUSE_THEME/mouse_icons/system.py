"""The system wide half: the theme outside the home directory, and the default.

A cursor theme installed in one user's home directory is invisible to two kinds
of program, and they are the two that made the user say the cursor sometimes
stays the default:

    the display manager's greeter   lxdm runs its greeter as the ``lxdm``
                                    account, or as root, before anyone has
                                    logged in. That account has no home
                                    directory full of themes, so it reads the
                                    system icon directories and nothing else.
    anything started before the     the window manager, the desktop's own file
    session's settings are applied  manager, and whatever the session starts
                                    first - all of them resolve their cursor
                                    before the GTK settings that name ours have
                                    been read.

Both are fixed by the same two things: the theme in a system icon directory, and
``default/index.theme`` in one of them naming it. That second file is what
``libXcursor`` falls back to when a program has asked for no theme of its own,
which is exactly the situation the greeter and the window manager are in - and
on the machine this was written for it still said ``Inherits=Adwaita``, which is
why the cursor was sometimes the arrow from 2003 and sometimes this one.

Where "the system icon directories" are is not hard coded: it is
``XDG_DATA_DIRS``, the same environment variable every toolkit uses to find a
theme, with each entry followed by ``/icons``. A machine that has put its themes
somewhere of its own is then treated correctly, and a test can point the whole
module at a temporary directory instead of at the real machine - which is why
this is written the way it is rather than with ``/usr/share`` spelled out.

This is the only part of the install that may need root, and it is written so
that it is the only part that can: the directories are written to directly when
they are writable - which they are on a machine where the user owns
``/usr/local``, and in every test - and only otherwise is ``sudo`` used.
Re-running the whole script under sudo instead would put root-owned files in a
user's home directory, which is the kind of thing that is only noticed a year
later.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

from .install import BACKUP_SUFFIX, MARKER, theme_dirs
from .theme import THEME_NAME

#: The default search path, which is what ``XDG_DATA_DIRS`` means when nothing
#: has set it. ``/usr/local`` first, because that is the one a machine's own
#: packages do not own and a distribution upgrade cannot take away.
DEFAULT_DATA_DIRS = "/usr/local/share:/usr/share"


def system_icon_dirs() -> tuple[Path, ...]:
    """Every system icon directory, in the order a program searches them."""
    value = os.environ.get("XDG_DATA_DIRS", "").strip() or DEFAULT_DATA_DIRS
    return tuple(
        Path(entry).expanduser() / "icons"
        for entry in value.split(":")
        if entry.strip()
    )


def system_theme_dir() -> Path:
    """Where the theme is copied to, system wide: the first icon directory."""
    return system_icon_dirs()[0] / THEME_NAME


def system_default_dir() -> Path:
    """The ``default`` theme as the system sees it.

    The directory is the first system icon directory's ``default`` child, and
    for the same reason the theme is: a program searches these directories in
    order, and the first one wins.
    """
    return system_icon_dirs()[0] / "default"


def is_root() -> bool:
    """Whether this process can write anywhere without help.

    ``os.geteuid`` does not exist on Windows, where this module has nothing to
    do; reading it through ``getattr`` keeps the file importable there so the
    other modules can be exercised and the file linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def _writable(path: Path) -> bool:
    """Whether this process may create or replace things in ``path``.

    A directory that does not exist is judged by its nearest existing ancestor,
    because that is the one that has to be writable to make it.
    """
    probe = path
    while not probe.exists() and probe != probe.parent:
        probe = probe.parent
    return os.access(probe, os.W_OK)


def run(command: list[str], parent: Path, input_text: str | None = None) -> bool:
    """Run one command, as root only when writing to ``parent`` needs it.

    The command is a list rather than a shell string, so a path with a space in
    it cannot become two arguments, and the ``sudo`` prompt is left where it is:
    a script that needs root should ask for the password where the user can
    answer it, which is their terminal.
    """
    if not is_root() and not _writable(parent):
        sudo = shutil.which("sudo")
        if sudo is None:
            return False
        command = [sudo, *command]
    if input_text is None:
        result = subprocess.run(command, check=False)
    else:
        result = subprocess.run(command, check=False, input=input_text, text=True)
    return result.returncode == 0


def system_default_text() -> str:
    """The machine's default cursor theme, as ``libXcursor`` reads it."""
    return "\n".join(
        [
            f"# {MARKER}",
            "[Icon Theme]",
            f"Inherits={THEME_NAME}",
            "",
        ]
    )


def manual_hint(source: Path) -> str:
    """What to run by hand when this script cannot become root itself."""
    target = system_theme_dir()
    index = system_default_dir() / "index.theme"
    return "\n".join(
        [
            "run these as root to make the cursor apply to the greeter too:",
            f"  rm -rf {target}",
            f"  cp -a {source} {target}",
            f"  mkdir -p {index.parent}",
            "  printf '[Icon Theme]\\nInherits=" + THEME_NAME + "\\n' "
            "> " + str(index),
        ]
    )


def install(log) -> bool:
    """Put the theme in a system directory and make it the machine's default.

    Returns whether it worked. A failure here is reported and the run continues:
    the per-user install is complete and correct without it, and a machine where
    sudo is unavailable is a machine where the commands that need it are printed
    instead.
    """
    source_root = theme_dirs()[0]
    if not source_root.is_dir():
        log.detail("there is nothing built to install system wide yet")
        return False

    target = system_theme_dir()
    parent = target.parent
    # The directory is replaced rather than merged: a cursor removed from the
    # theme would otherwise stay behind in the system copy and keep being served
    # to the greeter, which is the one program that cannot be asked to reload.
    if not run(["rm", "-rf", str(target)], parent):
        log.detail(f"could not write {target}")
        log.detail(manual_hint(source_root))
        return False
    # The parent has to exist before the copy: `cp -a source target` creates
    # `target` when `target` is the last component and there is a directory to
    # put it in, and fails when there is not. On a machine that has never had a
    # theme in /usr/local/share - which is most of them, because the distribution
    # puts its own in /usr/share - that directory is the missing one.
    if not run(["mkdir", "-p", str(parent)], parent):
        log.detail(f"could not create {parent}")
        log.detail(manual_hint(source_root))
        return False
    if not run(["cp", "-a", str(source_root), str(target)], parent):
        log.detail(f"could not copy the theme into {target}")
        log.detail(manual_hint(source_root))
        return False
    log.detail(f"installed {target}")

    index = system_default_dir() / "index.theme"
    backup = index.with_name(index.name + BACKUP_SUFFIX)
    # The existing default is kept: it is the distribution's, it names Adwaita,
    # and putting it back is what an uninstall has to do to leave the machine as
    # it was found.
    if index.exists() and not backup.exists():
        if run(["cp", "-a", str(index), str(backup)], index.parent):
            log.detail(f"backed up {index.name} to {backup.name}")
    if not run(["mkdir", "-p", str(index.parent)], index.parent):
        log.detail(f"could not create {index.parent}")
        return False
    if not run(["tee", str(index)], index.parent, input_text=system_default_text()):
        log.detail(f"could not write {index}")
        return False
    log.detail(f"made {THEME_NAME} the machine's default in {index}")
    return True


def uninstall(log) -> None:
    """Remove the system copy and put the machine's default back."""
    target = system_theme_dir()
    if target.is_dir():
        if run(["rm", "-rf", str(target)], target.parent):
            log.detail(f"removed {target}")
        else:
            log.detail(f"could not remove {target}; remove it as root")

    index = system_default_dir() / "index.theme"
    backup = index.with_name(index.name + BACKUP_SUFFIX)
    if not index.is_file():
        return
    if backup.is_file():
        if run(["mv", str(backup), str(index)], index.parent):
            log.detail(f"restored {index} from its backup")
        else:
            log.detail(f"could not restore {index}; put it back as root")
        return
    # Nothing was there before, so nothing is put back: the file this script
    # created is removed instead of being left naming a theme that is gone.
    if run(["rm", "-f", str(index)], index.parent):
        log.detail(f"removed {index}")


def check() -> list[str]:
    """What is wrong with the system wide half, or nothing."""
    problems: list[str] = []
    target = system_theme_dir()
    if not target.is_dir():
        problems.append(
            f"{target} is missing: the display manager's greeter, and anything "
            "the session starts before it reads the settings, will use the "
            "system default cursor instead"
        )
    elif not (target / "cursors").is_dir():
        problems.append(f"{target} has no cursors directory")
    index = system_default_dir() / "index.theme"
    if not index.is_file():
        problems.append(f"{index} is missing")
    else:
        try:
            text = index.read_text(encoding="utf-8")
        except OSError:
            text = ""
        if f"Inherits={THEME_NAME}" not in text:
            problems.append(
                f"{index} does not name {THEME_NAME}, so a program that asks "
                "for no theme of its own gets whatever it does name"
            )
    return problems
