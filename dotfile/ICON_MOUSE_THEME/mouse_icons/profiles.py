"""The LXDE files: LXSession's own cursor setting, GTK 2, and the autostart.

Writing the theme name into the GTK 3 settings is not enough on LXDE, and the
reason is worth stating plainly because it explains the whole complaint: LXDE is
not one program. It is LXSession, which starts everything else; LXPanel, which
in the versions people are still running is a GTK 2 program; Openbox, which is
not a GTK program at all; and PCManFM, which draws the desktop background and
the cursor over it. Each of them reads the cursor theme from somewhere different:

    LXSession       ~/.config/lxsession/<profile>/desktop.conf, in the [GTK]
                    group, as sGtk/CursorThemeName. This is the file lxappearance
                    writes when a cursor theme is chosen in it, and the one
                    LXSession reads at the start of every session - which is why
                    a theme chosen in lxappearance sometimes sticks and
                    sometimes does not: it is written for the session manager,
                    and a session that was already running never sees it.
    GTK 2 programs   ~/.gtkrc-2.0, as gtk-cursor-theme-name.
    GTK 3 programs   ~/.config/gtk-3.0/settings.ini, which :mod:`select` writes.
    everything else  the X server's resource database - Xcursor.theme - which is
                    what Openbox and PCManFM fall back to, and which is only
                    read when something runs xrdb. Nothing does on LXDE by
                    default, and that is the other half of the problem.

So all four are written. The fourth is the one that has no file of its own: the
autostart list below is where an LXDE session is told to load the resource
database, and adding that line is what makes the cursor apply to the window
manager and the desktop as well as to the programs that follow the GTK setting.

The shapes of these files were not guessed at. ``strings`` on the installed
lxappearance gives ``lxsession/``, ``/desktop.conf``, ``sGtk/CursorThemeName``,
``iGtk/CursorThemeSize``, ``.icons/default/index.theme`` and ``gtkrc``, so the
names and the paths below are the ones that program uses.
"""

from __future__ import annotations

import os
import shutil
from pathlib import Path

from .install import MARKER, merge_block
from .theme import THEME_NAME

#: The size written alongside the name. Thirty two, the middle of the three
#: sizes the theme ships, so it is drawn rather than scaled.
CURSOR_SIZE = 32

#: The profile LXSession uses when it is not told otherwise, and therefore the
#: one to write when a machine has no profile of its own yet.
DEFAULT_PROFILE = "LXDE"

#: The GTK 2 cursor pair, and the LXSession pair, as key and value. The
#: different spellings are not a mistake: GTK 2's rc files use hyphens and
#: LXSession's desktop.conf uses the type-prefixed LXDE spelling.
#: GTK 2's rc format wants a string property quoted and an integer one not.
#: Both spellings parse, but every example in the wild is written this way and a
#: file someone edits by hand should look like the ones around it. The check
#: below strips the quotes before comparing, so the two are the same fact.
GTK2_KEYS: tuple[tuple[str, str], ...] = (
    ("gtk-cursor-theme-name", f'"{THEME_NAME}"'),
    ("gtk-cursor-theme-size", str(CURSOR_SIZE)),
)
LXSESSION_KEYS: tuple[tuple[str, str], ...] = (
    ("sGtk/CursorThemeName", THEME_NAME),
    ("iGtk/CursorThemeSize", str(CURSOR_SIZE)),
)

#: The line an LXDE session is told to run to load the X resource database.
AUTOSTART_LINE = "@xrdb -merge ~/.Xresources"

#: The markers around it, so a second run replaces it rather than adding it
#: again and an uninstall can take it back out.
AUTOSTART_BEGIN = f"# >>> GnuchanPurple cursor - {MARKER}"
AUTOSTART_END = "# <<< GnuchanPurple cursor"


def home_dir() -> Path:
    return Path(os.path.expanduser("~")).resolve()


def lxsession_dir() -> Path:
    return home_dir() / ".config" / "lxsession"


def profile_dirs() -> tuple[Path, ...]:
    """Every LXSession profile to write, the default one always included.

    Each profile is a session of its own - LXDE for the plain one, and whatever
    a distribution adds beside it - and LXSession reads exactly one of them at a
    time. Writing all of them costs a few lines and means the cursor is right
    whichever one is started.
    """
    root = lxsession_dir()
    found: list[Path] = []
    try:
        names = sorted(entry.name for entry in root.iterdir() if entry.is_dir())
    except OSError:
        names = []
    if DEFAULT_PROFILE not in names:
        names.append(DEFAULT_PROFILE)
    for name in names:
        found.append(root / name)
    return tuple(found)


def gtk2_rc_file() -> Path:
    return home_dir() / ".gtkrc-2.0"


#: The programs that mean LXDE is what starts this session. Any one of them is
#: enough: a machine can have the session manager without the panel, or a
#: display manager without either, and all of them read the cursor from one of
#: the files below.
LXDE_PROGRAMS = ("lxsession", "startlxde", "lxpanel", "pcmanfm", "lxappearance")


def is_lxde() -> bool:
    """Whether this machine runs LXDE, by the programs it has.

    The files below are LXDE's, and writing them on a machine that has never
    heard of it leaves a configuration nobody reads - which is not harmful, but
    it is also not something an installer should do silently. A machine that
    installs LXDE later runs this script again and gets them then.
    """
    if lxsession_dir().is_dir():
        return True
    return any(shutil.which(program) for program in LXDE_PROGRAMS)


def set_keys(existing: str, keys: tuple[tuple[str, str], ...],
             section: str | None = None) -> str:
    """Set ``keys`` in a key file, keeping every other line of it.

    Written out rather than done with ``configparser`` for the reason the LXDM
    installer gives for the same job: these are files people edit by hand,
    comments and key order in them are theirs, and a parser that rewrites the
    file loses both - and raises on a duplicate key, which is a file somebody
    has edited.
    """
    wanted = dict(keys)
    written: set[str] = set()
    out: list[str] = []
    current: str | None = None
    seen_section = section is None

    for line in existing.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            # Leaving a group: anything this group still owes goes in before the
            # next heading, so a key never lands in the wrong section.
            if current == section and seen_section:
                for key, value in keys:
                    if key not in written:
                        out.append(f"{key}={value}")
                        written.add(key)
            current = stripped[1:-1].strip()
            if current == section:
                seen_section = True
            out.append(line)
            continue
        if stripped and not stripped.startswith(("#", ";", "!")) and "=" in stripped:
            key = stripped.split("=", 1)[0].strip()
            if (section is None or current == section) and key in wanted:
                # A second copy of a key is dropped rather than rewritten: the
                # first one was already set, and writing it again is how the same
                # setting ends up in a file twice.
                if key in written:
                    continue
                out.append(f"{key}={wanted[key]}")
                written.add(key)
                continue
        out.append(line)

    # The end of the file is reached in one of three states: in the middle of the
    # group being written, after it, or in a file with no groups at all - which
    # is what `.gtkrc-2.0` usually is. The keys still owed go here, once each,
    # and are recorded so the branch below cannot add them a second time. That
    # pairing is the whole of this function: the bug it was written to fix was a
    # gtkrc with every key in it twice.
    if section is None or (seen_section and current == section):
        for key, value in keys:
            if key not in written:
                out.append(f"{key}={value}")
                written.add(key)
    if section is not None and not seen_section:
        # The group is not in the file at all, which is the state a fresh home
        # directory is in. It goes at the end with the keys under it.
        if out and out[-1].strip():
            out.append("")
        out.append(f"[{section}]")
        out.extend(f"{key}={value}" for key, value in keys)

    text = "\n".join(out).strip("\n")
    return text + "\n" if text else ""


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def install(log) -> None:
    """Write the cursor into every file an LXDE session reads it from."""
    if not is_lxde():
        log.detail("not an LXDE session; its files are left alone")
        return
    for profile in profile_dirs():
        desktop = profile / "desktop.conf"
        write(desktop, set_keys(read(desktop), LXSESSION_KEYS, section="GTK"))
        log.detail(f"wrote {desktop}")

        autostart = profile / "autostart"
        existing = read(autostart)
        merged = merge_block(existing, AUTOSTART_BEGIN, AUTOSTART_END,
                             autostart_block())
        if merged.strip() != existing.strip():
            write(autostart, merged)
            log.detail(f"wrote {autostart}")

    gtk2 = gtk2_rc_file()
    write(gtk2, set_keys(read(gtk2), GTK2_KEYS))
    log.detail(f"wrote {gtk2}")


def autostart_block() -> str:
    """The LXDE autostart lines that load the X resource database.

    LXSession runs every line of this file that begins with an @, so this is
    where an LXDE session is told something the session manager has no setting
    for. Without it Xcursor.theme never reaches the X server, and the window
    manager and the desktop keep the cursor they started with - which is the
    half of this that a GTK setting cannot fix.
    """
    return "\n".join(
        [
            AUTOSTART_BEGIN,
            "# Load the X resource database, so the window manager and the",
            "# desktop use the cursor named there. Nothing runs xrdb on LXDE by",
            "# default, and a cursor theme that is only in a GTK setting reaches",
            "# the GTK programs and nothing else.",
            AUTOSTART_LINE,
            AUTOSTART_END,
        ]
    )


def uninstall(log) -> None:
    """Take the lines back out, leaving whatever else was in the files."""
    for profile in profile_dirs():
        autostart = profile / "autostart"
        if not autostart.is_file():
            continue
        merged = merge_block(read(autostart), AUTOSTART_BEGIN, AUTOSTART_END, "")
        if merged.strip():
            write(autostart, merged)
        else:
            autostart.unlink()
        log.detail(f"removed our lines from {autostart}")
    log.detail(
        "the GTK settings were left; clear gtk-cursor-theme-name and "
        "sGtk/CursorThemeName to change them back"
    )


def check() -> list[str]:
    """Whether a session would find the cursor, and from which file."""
    problems: list[str] = []
    if not is_lxde():
        return problems
    found = False
    for profile in profile_dirs():
        desktop = profile / "desktop.conf"
        text = read(desktop)
        if f"sGtk/CursorThemeName={THEME_NAME}" in text:
            found = True
        if not desktop.is_file():
            continue
        if f"sGtk/CursorThemeName={THEME_NAME}" not in text:
            problems.append(
                f"{desktop} does not name {THEME_NAME}, so LXSession starts the "
                "session with whatever cursor it names"
            )
    if not found:
        problems.append(
            "no LXSession profile names the cursor theme, so an LXDE session "
            "will start with the system default"
        )
    if f"gtk-cursor-theme-name={THEME_NAME}" not in read(gtk2_rc_file()).replace(
        '"', ""
    ):
        problems.append(
            f"{gtk2_rc_file()} does not name {THEME_NAME}, so GTK 2 programs - "
            "which on LXDE includes the panel - use the default cursor"
        )
    return problems
