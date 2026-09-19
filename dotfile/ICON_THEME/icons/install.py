"""Installing the generated theme into the directories a desktop reads.

This is the part of the library that has nothing to do with drawing: it decides
where on the machine an icon theme is looked up, builds the tree into the first
of those places, and copies it into the rest. Keeping it here rather than in the
top level script means the caller is three lines long and every decision about
*where* the theme goes is in one file with one comment per decision.

Two locations are written, not one, because two generations of desktop read
different ones and neither falls back to the other:

* ``$XDG_DATA_HOME/icons`` — ``~/.local/share/icons`` — which GTK 3 and GTK 4
  and every modern settings dialog read, including lxappearance.
* ``~/.icons`` — which the GTK 2 file pickers and the older panel applets read.

A theme that is only in the first does not appear in the GTK 2 half of an
application; a theme that is only in the second does not appear in the
appearance dialog at all. Both are written on every run.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path
from typing import Callable

from .catalogue import unknown_glyphs, unusable_names
from .tree import Builder, THEME_NAME

#: The message sink a caller may replace; ``None`` silences the progress output.
Logger = Callable[[str], None]


def home_dir() -> Path:
    """The user's home directory, or the best guess at it.

    ``expanduser`` consults ``HOME`` and, on Windows, ``USERPROFILE``; the
    fallback keeps a stripped down session — a service, a container — from
    failing with an empty path and writing the theme to ``/icons``.
    """
    expanded = os.path.expanduser("~")
    if expanded and expanded != "~":
        return Path(expanded).resolve()
    return Path(os.environ.get("USERPROFILE") or os.getcwd()).resolve()


def _xdg(variable: str, fallback: Path) -> Path:
    """An XDG base directory, or ``fallback`` when the variable is unset."""
    value = os.environ.get(variable, "").strip()
    return Path(value).expanduser() if value else fallback


def theme_search_dirs() -> list[Path]:
    """Where a user level icon theme is looked up, modern location first."""
    return [
        _xdg("XDG_DATA_HOME", home_dir() / ".local" / "share") / "icons",
        home_dir() / ".icons",
    ]


def theme_dir() -> Path:
    """The primary location the theme is built into and read from."""
    return theme_search_dirs()[0] / THEME_NAME


def _check_catalogue() -> None:
    """Refuse to build a catalogue that cannot be written to disk.

    Both checks cover the same failure from different sides: a glyph name that
    nothing defines produces an icon that is silently missing, and a name with a
    path separator in it produces a build that stops halfway through with an
    ``OSError``. Either one is cheaper to catch here than to notice on screen.
    """
    missing = unknown_glyphs()
    if missing:
        raise SystemExit(
            "error: the catalogue names glyphs that do not exist: " + ", ".join(missing)
        )
    unusable = unusable_names()
    if unusable:
        raise SystemExit(
            "error: these icon names cannot be written as files: " + ", ".join(unusable)
        )


def build_into(target: Path, log: Logger | None = None) -> dict[str, int]:
    """Write the whole theme into ``target``, replacing what is there.

    Nothing is merged: a theme that has been edited down would otherwise keep the
    files it no longer defines, and a stale icon is invisible until it is the
    wrong picture on screen.
    """
    _check_catalogue()
    counted = Builder(Path(target), log).build()
    return counted


def install(log: Logger | None = None) -> list[Path]:
    """Build the theme once and place it in every search directory.

    The build happens at the primary location and the finished tree is copied to
    the others. Building the artwork once and copying it is the difference
    between one render pass and one per location, which for a theme of this size
    is minutes rather than seconds — and it also guarantees the copies are
    identical rather than merely equivalent.
    """
    emit = log or (lambda message: None)
    primary = theme_dir()
    counted = build_into(primary, log)
    emit(f"==> built {THEME_NAME}")
    emit(f"    {counted['png']} PNG files")
    emit(f"    {counted['svg']} SVG files")
    emit(f"    {counted['symbolic']} symbolic SVG files")
    emit(f"    {counted['glyphs']} distinct glyph renderings")
    emit(f"    {counted['animated']} animated spinners")

    installed: list[Path] = []
    for directory in theme_search_dirs():
        target = directory / THEME_NAME
        if target == primary:
            installed.append(target)
            emit(f"    written to {target}")
            continue
        if target.is_symlink() or target.is_file():
            target.unlink()
        elif target.exists():
            shutil.rmtree(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(primary, target)
        installed.append(target)
        emit(f"    copied to {target}")
    return installed


def write_ini_setting(path: Path, key: str, value: str) -> None:
    """Set one key in a GTK ``settings.ini`` without disturbing the rest of it.

    The file is rewritten in place rather than regenerated: it holds the user's
    font, their cursor theme and every other setting their desktop has written
    there, and replacing it to change one line would throw all of that away.
    """
    existing: list[str] = []
    if path.exists():
        existing = path.read_text(encoding="utf-8").splitlines()
    body: list[str] = []
    in_settings = False
    written = False
    for line in existing:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            # Leaving the [Settings] group without having found the key means it
            # was not there, so it is appended before the next heading.
            if in_settings and not written:
                body.append(f"{key}={value}")
                written = True
            in_settings = stripped[1:-1].strip().lower() == "settings"
            body.append(line)
            continue
        if in_settings and stripped.split("=", 1)[0].strip() == key:
            body.append(f"{key}={value}")
            written = True
            continue
        body.append(line)
    if not written:
        if any(line.strip().lower() == "[settings]" for line in body):
            body.append(f"{key}={value}")
        else:
            body += ["[Settings]", f"{key}={value}"]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(body).strip("\n") + "\n", encoding="utf-8")


def apply_theme(log: Logger | None = None) -> None:
    """Select the theme in the session.

    An installed theme that is never selected is never seen: the shell, the file
    manager and every menu keep drawing with the icon theme the session already
    names. ``xfconf`` owns that choice on XFCE and ``gsettings`` on GNOME, and
    the GTK settings file covers the sessions that have neither — so all three
    are written, and whichever the session actually reads wins.
    """
    emit = log or (lambda message: None)
    settings = _xdg("XDG_CONFIG_HOME", home_dir() / ".config") / "gtk-3.0" / "settings.ini"
    write_ini_setting(settings, "gtk-icon-theme-name", THEME_NAME)
    emit(f"    selected in {settings}")

    xfconf = shutil.which("xfconf-query")
    if xfconf is not None:
        subprocess.run(
            [xfconf, "-c", "xsettings", "-p", "/Net/IconThemeName", "-s", THEME_NAME],
            check=False,
        )
        emit("    selected through xfconf")

    gsettings = shutil.which("gsettings")
    if gsettings is not None:
        subprocess.run(
            ["gsettings", "set", "org.gnome.desktop.interface", "icon-theme", THEME_NAME],
            check=False,
        )
        emit("    selected through gsettings")

    if xfconf is None and gsettings is None:
        emit("    no settings backend found; pick the theme in the appearance settings")


def verify(installed: list[Path], log: Logger | None = None) -> int:
    """Check that what was installed is a theme a desktop can list and read.

    This is the step that says out loud where the theme went. A theme that is
    written but not found is the one failure that looks exactly like success:
    nothing errors, the files are all there, and the appearance dialog simply
    does not mention the theme. Reporting the paths and the counts turns that
    into something visible.
    """
    emit = log or (lambda message: None)
    emit("==> checking the installed theme")
    problems = 0

    for target in installed:
        theme_index = target / "index.theme"
        if not theme_index.is_file():
            emit(f"    PROBLEM: {theme_index} is missing, so the theme cannot be listed")
            problems += 1
            continue
        declared = 0
        for line in theme_index.read_text(encoding="utf-8").splitlines():
            if line.startswith("Directories="):
                declared = len(line.split("=", 1)[1].split(","))
        icons = sum(1 for _ in target.rglob("*.png"))
        icons += sum(1 for _ in target.rglob("*.svg"))
        emit(f"    {target}")
        emit(f"      index.theme declares {declared} directories")
        emit(f"      {icons} icon files are present")
        if declared == 0:
            emit("      PROBLEM: index.theme declares no directories")
            problems += 1
        if icons == 0:
            emit("      PROBLEM: no icon files were copied")
            problems += 1

    emit("    search path: " + os.pathsep.join(str(d) for d in theme_search_dirs()))
    if shutil.which("gtk-update-icon-cache") is None:
        emit("    gtk-update-icon-cache is not installed; it is not needed")
    if problems:
        emit(f"==> {problems} problem(s); the theme may not appear in the appearance dialog")
    else:
        emit("==> the theme is installed; open the appearance dialog to select it")
    return problems


def main(log: Logger | None = None) -> int:
    """Build, install, select and verify, in that order, with no arguments.

    ``log`` is where the progress goes; the shipped script passes ``print`` so a
    user running it can see what happened, and a caller that wants the library
    quiet passes ``None``.
    """
    emit = log or (lambda message: None)
    emit(f"==> installing the {THEME_NAME} icon theme")
    installed = install(log)
    apply_theme(log)
    verify(installed, log)
    emit("")
    emit(f"==> done. Open the appearance settings and pick {THEME_NAME}.")
    return 0
