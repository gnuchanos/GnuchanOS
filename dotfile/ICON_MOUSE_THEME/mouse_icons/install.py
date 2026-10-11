"""Installing the built theme, and making the session use it.

Building the theme is the easy half. The hard half is that a cursor theme on a
modern desktop is chosen in three different places, and a script that sets one
of them leaves the cursor purple in the terminal and black in the file manager:

    ~/.icons/default/index.theme    the fallback the X cursor library reads.
                                    Every toolkit falls back to this when it is
                                    not told otherwise, which makes it the one
                                    setting that reaches the programs with no
                                    configuration file of their own.
    ~/.Xresources                   what the X server is told at login, for
                                    xterm and the other X11 programs that read
                                    the cursor size from the server rather than
                                    from a settings daemon.
    gsettings / settings.ini        GTK 3 and 4, which each keep their own
                                    cursor name and neither of which reads the
                                    other.

All three are written. The two that can fail - gsettings with no session bus,
GTK 4 on a machine that has no GTK 4 - are best effort and say so, because the
alternative is a script that stops half way through on a machine that only
needed the first one.

Everything this module writes into a file the user also has settings in goes
between markers, so running it twice is harmless and ``--uninstall`` can take it
back out without knowing what was there before.
"""

from __future__ import annotations

import os
import shutil
from pathlib import Path

from .theme import THEME_NAME

SCRIPT_NAME = "settings_mouse_icon.py"
MARKER = f"written by {SCRIPT_NAME}"
BACKUP_SUFFIX = ".gnuchan-backup"
BLOCK_BEGIN = f"! >>> GnuchanPurple cursor - {MARKER}"
BLOCK_END = "! <<< GnuchanPurple cursor"

#: The size the X server is told to draw at, when nothing else says. Thirty two
#: is the size most desktops pick on a 1080p screen, and it is the middle of the
#: three the theme ships, so it is drawn rather than scaled.
CURSOR_SIZE = 32


def home_dir() -> Path:
    return Path(os.path.expanduser("~")).resolve()


def _xdg(variable: str, fallback: Path) -> Path:
    value = os.environ.get(variable, "").strip()
    return Path(value).expanduser() if value else fallback


def xdg_config_home() -> Path:
    return _xdg("XDG_CONFIG_HOME", home_dir() / ".config")


def xdg_data_home() -> Path:
    return _xdg("XDG_DATA_HOME", home_dir() / ".local" / "share")


def theme_dirs() -> tuple[Path, ...]:
    """Where the theme is written.

    Two places on purpose. ``~/.icons`` is the directory the X cursor library
    has looked in since before there was an XDG specification, and some older
    programs still look there and nowhere else; ``~/.local/share/icons`` is
    where every modern toolkit looks. A theme in only one of them is a theme
    that works in half the desktop.
    """
    return (
        home_dir() / ".icons" / THEME_NAME,
        xdg_data_home() / "icons" / THEME_NAME,
    )


def default_theme_dir() -> Path:
    """The ``default`` theme: the one a program finds when it is told nothing.

    This is the trick that makes a cursor theme apply everywhere without a
    settings daemon. ``default`` is a theme of its own whose only job is to
    inherit from the real one, and every toolkit that resolves a cursor asks for
    ``default`` when its configuration is silent.
    """
    return home_dir() / ".icons" / "default"


def xresources_file() -> Path:
    return home_dir() / ".Xresources"


def gtk_settings_file(version: int) -> Path:
    return xdg_config_home() / f"gtk-{version}.0" / "settings.ini"
# --- editing files that are the user's ---------------------------------------


def _find(lines: list[str], marker: str, start: int = 0) -> int | None:
    for index in range(start, len(lines)):
        if lines[index].strip() == marker:
            return index
    return None


def merge_block(existing: str, begin: str, end: str, block: str) -> str:
    """Replace the region between the markers with ``block``, keeping the rest.

    The same rule the other installers in this repository use: a first run
    appends, a later run replaces in place so the block does not creep down the
    file, and an empty block removes it.
    """
    lines = existing.splitlines()
    start = _find(lines, begin)
    if start is None:
        kept = list(lines)
    else:
        finish = _find(lines, end, start + 1)
        kept = lines[:start] + ([] if finish is None else lines[finish + 1:])
    while kept and not kept[-1].strip():
        kept.pop()
    addition = block.splitlines() if block.strip() else []
    if addition:
        if kept:
            kept.append("")
        kept += addition
    text = "\n".join(kept).strip("\n")
    return text + "\n" if text else ""


def write_merged(log, path: Path, block: str) -> None:
    """Put our block in a file the user also has settings in."""
    existing = ""
    if path.exists():
        try:
            existing = path.read_text(encoding="utf-8")
        except OSError:
            existing = ""
        if MARKER not in existing:
            backup = path.with_name(path.name + BACKUP_SUFFIX)
            if not backup.exists():
                shutil.copy2(path, backup)
    path.parent.mkdir(parents=True, exist_ok=True)
    merged = merge_block(existing, BLOCK_BEGIN, BLOCK_END, block)
    path.write_text(merged, encoding="utf-8")
    log.detail(f"wrote {path}")


def cursor_resource_block() -> str:
    """What ``~/.Xresources`` is told, for the programs that read it there."""
    return "\n".join(
        [
            BLOCK_BEGIN,
            "! The cursor theme and the size the X server draws it at.",
            "! Programs that read their cursor from the server rather than from a",
            "! settings daemon - xterm, and most of the older X11 tools - take it",
            "! from these two lines, so removing them leaves those programs on the",
            "! black default while the rest of the desktop stays purple.",
            f"Xcursor.theme: {THEME_NAME}",
            f"Xcursor.size: {CURSOR_SIZE}",
            BLOCK_END,
        ]
    )


def default_index_text() -> str:
    """The theme a program finds when it is told nothing."""
    return "\n".join(
        [
            f"# {MARKER}",
            "[Icon Theme]",
            f"Inherits={THEME_NAME}",
            "",
        ]
    )
