"""Telling the session which cursor to use, in the three places it asks.

The theme is already on disk by the time this runs. What is left is the part
that has no single answer: GTK 3 and GTK 4 each keep their own cursor name and
neither reads the other, the X server keeps its own and only reads it from the
resource database, and a GNOME or KDE session keeps a third in a settings
daemon that overrides both.

Every step here is best effort. A missing ``gsettings`` is a machine with no
GNOME, not a failure - and a script that stopped because it could not run a
program the machine does not have would be a script that never finished on the
machine that only needed the file written.

The one step that always works is the ``default`` theme index, which is written
by :mod:`apply` rather than here: it is a file, and a file cannot fail.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

from .install import gtk_settings_file
from .theme import THEME_NAME

#: The settings GTK keeps its cursor in, per version. GTK 2 read the same pair
#: from ``~/.gtkrc-2.0``, which is a different syntax and is not written here:
#: a GTK 2 application on a modern desktop is rare enough that the ``default``
#: theme covers it, and the rc syntax is not the ini syntax below.
GTK_CURSOR_KEYS: tuple[tuple[str, str], ...] = (
    ("gtk-cursor-theme-name", THEME_NAME),
    ("gtk-cursor-theme-size", "32"),
)


def _find_gsettings() -> str | None:
    """``gsettings``, if this machine has it and it can be reached.

    ``--version`` is enough to prove both: a gsettings binary with no session
    bus answers on the command line and fails on every ``set``, and finding that
    out here is better than printing a bus error four times in a row.
    """
    found = shutil.which("gsettings")
    if found is None:
        return None
    result = subprocess.run([found, "--version"], check=False,
                            capture_output=True)
    return found if result.returncode == 0 else None


def gsettings_set(log) -> bool:
    """Set the cursor in the settings daemon, for the desktops that use one."""
    gsettings = _find_gsettings()
    if gsettings is None:
        log.detail("gsettings is not installed; the file settings still apply")
        return False
    ok = True
    for key, value in (
        ("org.gnome.desktop.interface cursor-theme", THEME_NAME),
        ("org.gnome.desktop.interface cursor-size", "32"),
    ):
        schema, _, name = key.rpartition(" ")
        result = subprocess.run(
            [gsettings, "set", schema, name, value], check=False,
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            ok = False
    if ok:
        log.detail("selected in the settings daemon with gsettings")
    else:
        log.detail("gsettings refused the change; the file settings still apply")
    return ok


def _gtk_settings_text(existing: str) -> str:
    """An ini file's ``[Settings]`` section with the cursor pair in it.

    GTK's settings file is short and has no comments, so it is rewritten rather
    than merged: the pair is put in the ``[Settings]`` section if it exists,
    and a section is added if the file is empty or has none.
    """
    lines = [line for line in existing.splitlines()]
    wanted = dict(GTK_CURSOR_KEYS)
    out: list[str] = []
    in_settings = False
    seen: set[str] = set()
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_settings:
                # Leaving the section: anything we had to add goes in before it.
                for key, value in GTK_CURSOR_KEYS:
                    if key not in seen:
                        out.append(f"{key}={value}")
                    seen.add(key)
            in_settings = stripped == "[Settings]"
            out.append(line)
            continue
        if in_settings and "=" in stripped:
            key = stripped.split("=", 1)[0].strip()
            if key in wanted:
                out.append(f"{key}={wanted[key]}")
                seen.add(key)
                continue
        out.append(line)
    if in_settings and seen != set(wanted):
        for key, value in GTK_CURSOR_KEYS:
            if key not in seen:
                out.append(f"{key}={value}")
    if "[Settings]" not in "\n".join(out):
        if out and out[-1].strip():
            out.append("")
        out.append("[Settings]")
        out.extend(f"{key}={value}" for key, value in GTK_CURSOR_KEYS)
    text = "\n".join(out).strip("\n")
    return text + "\n" if text else ""


def gtk_set(log, versions: tuple[int, ...] = (3, 4)) -> None:
    """Write the cursor name into each GTK version's own settings file."""
    for version in versions:
        path = gtk_settings_file(version)
        existing = ""
        if path.exists():
            try:
                existing = path.read_text(encoding="utf-8")
            except OSError:
                existing = ""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_gtk_settings_text(existing), encoding="utf-8")
        log.detail(f"wrote {path}")


def xrdb_load(log) -> bool:
    """Make the running X server use the theme now rather than at next login.

    ``xrdb -merge`` is what applies the resource block; without it the change
    waits for the next session, which is the reason half of all cursor themes
    look like they did nothing.
    """
    xrdb = shutil.which("xrdb")
    if xrdb is None or not os.environ.get("DISPLAY"):
        log.detail("no running X server; the settings apply at the next login")
        return False
    result = subprocess.run(
        [xrdb, "-merge", str(Path(os.path.expanduser("~")) / ".Xresources")],
        check=False,
    )
    if result.returncode != 0:
        log.detail("xrdb could not load the resources")
        return False
    log.detail("loaded into the running X server with xrdb -merge")
    return True
