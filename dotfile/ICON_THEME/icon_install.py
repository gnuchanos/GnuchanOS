#!/usr/bin/env python3
# =============================================================================
# GnuchanPurple - icon theme installer
# -----------------------------------------------------------------------------
# Builds and installs the purple icon set in ./GnuchanPurple. Nothing outside
# the standard library is used: the rasteriser, the SVG writer, the palette and
# the artwork all live in ./icons.
#
#     python icon_install.py                 # build and install
#     python icon_install.py --build-only    # write ./GnuchanPurple, install nothing
#     python icon_install.py --check         # report what the environment is missing
#     python icon_install.py --uninstall     # remove what was installed
#     python icon_install.py --list          # every icon name the theme defines
#
# The theme is generated, never hand-edited: GnuchanPurple/ is rebuilt in full
# every run, so the artwork and the names cannot drift apart. What makes the
# theme useful on a machine the author has never seen is that it inherits from
# Adwaita and hicolor, so a name it does not define still resolves to something
# rather than to a blank square.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from icons import catalogue, tree  # noqa: E402  (the path has to be set first)

THEME_NAME = tree.THEME_NAME
BACKUP_SUFFIX = ".gnuchan-backup"
GENERATED_MARKER = "built by icon_install.py"


def home_dir() -> Path:
    return Path(os.path.expanduser("~")).resolve()


def _xdg(variable: str, fallback: Path) -> Path:
    value = os.environ.get(variable, "").strip()
    return Path(value).expanduser() if value else fallback


def theme_search_dirs() -> list[Path]:
    """Where a user level icon theme is looked up, most modern location first."""
    return [
        _xdg("XDG_DATA_HOME", home_dir() / ".local" / "share") / "icons",
        home_dir() / ".icons",
    ]


def gtk_theme_dir() -> Path:
    """The icon theme the GTK theme installs alongside, for reference in --check."""
    return SCRIPT_DIR.parent / "GTK_THEME" / "GnuchanPurple"


def source_dir() -> Path:
    return SCRIPT_DIR / THEME_NAME


# --- building ---------------------------------------------------------------


def build(log, quiet: bool) -> dict[str, int]:
    """Generate the whole theme into ./GnuchanPurple."""
    missing = catalogue.unknown_glyphs()
    if missing:
        raise SystemExit(
            "error: the catalogue names glyphs that do not exist: " + ", ".join(missing)
        )
    unusable = catalogue.unusable_names()
    if unusable:
        raise SystemExit(
            "error: these icon names cannot be written as files: " + ", ".join(unusable)
        )
    duplicates = catalogue.duplicate_names()
    if duplicates and not quiet:
        print(f"    note: {len(duplicates)} icon names are registered twice")
    builder = tree.Builder(source_dir(), log)
    counted = builder.build()
    if not quiet:
        print(f"==> built {THEME_NAME}")
        print(f"    {counted['png']} PNG files")
        print(f"    {counted['svg']} SVG files")
        print(f"    {counted['symbolic']} symbolic SVG files")
        print(f"    {counted['glyphs']} distinct glyph renderings were needed")
    return counted


# --- installing --------------------------------------------------------------


def install_tree(log, quiet: bool) -> list[Path]:
    """Copy the generated tree into every user icon directory."""
    installed: list[Path] = []
    for directory in theme_search_dirs():
        target = directory / THEME_NAME
        if target.exists():
            shutil.rmtree(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source_dir(), target)
        installed.append(target)
        if not quiet:
            print(f"    installed {target}")
    return installed


def apply_theme(log, quiet: bool) -> None:
    """Select the theme in the session, the way the GTK installer does.

    An installed theme that is never selected is never seen: the shell, the
    file manager and every menu keep drawing with the icon theme the session
    already names. xfconf and gsettings own that choice when they exist, and
    ~/.config/gtk-3.0/settings.ini covers the sessions that have neither.
    """
    settings = _xdg("XDG_CONFIG_HOME", home_dir() / ".config") / "gtk-3.0" / "settings.ini"
    write_ini_setting(settings, "gtk-icon-theme-name", THEME_NAME)
    xfconf = shutil.which("xfconf-query")
    if xfconf is not None:
        subprocess.run(
            [xfconf, "-c", "xsettings", "-p", "/Net/IconThemeName", "-s", THEME_NAME],
            check=False,
        )
        if not quiet:
            print("    selected through xfconf")
    gsettings = shutil.which("gsettings")
    if gsettings is not None:
        subprocess.run(
            ["gsettings", "set", "org.gnome.desktop.interface", "icon-theme", THEME_NAME],
            check=False,
        )
        if not quiet:
            print("    selected through gsettings")
    if xfconf is None and gsettings is None and not quiet:
        print("    no settings backend found; pick the theme in your appearance settings")


def write_ini_setting(path: Path, key: str, value: str) -> None:
    """Set one key in a GTK settings.ini without disturbing the rest of it."""
    existing: list[str] = []
    if path.exists():
        existing = path.read_text(encoding="utf-8").splitlines()
    body: list[str] = []
    in_settings = False
    written = False
    for line in existing:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
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


# --- uninstalling ------------------------------------------------------------


def uninstall(log, quiet: bool) -> None:
    for directory in theme_search_dirs():
        target = directory / THEME_NAME
        if not target.exists():
            continue
        shutil.rmtree(target)
        if not quiet:
            print(f"    removed {target}")
    thetheme = _xdg("XDG_CONFIG_HOME", home_dir() / ".config") / "gtk-3.0" / "settings.ini"
    if thetheme.exists():
        text = thetheme.read_text(encoding="utf-8")
        kept = [
            line
            for line in text.splitlines()
            if not line.strip().startswith("gtk-icon-theme-name")
        ]
        thetheme.write_text("\n".join(kept).strip("\n") + "\n", encoding="utf-8")
        if not quiet:
            print(f"    cleared the icon theme from {thetheme}")


# --- checking ----------------------------------------------------------------


def check(log, quiet: bool) -> int:
    """Report anything that would stop the theme from being seen."""
    problems: list[str] = []
    missing = catalogue.unknown_glyphs()
    if missing:
        problems.append("catalogue refers to unknown glyphs: " + ", ".join(missing))
    unusable = catalogue.unusable_names()
    if unusable:
        problems.append(
            "catalogue names icons that cannot be written as files: "
            + ", ".join(unusable)
        )
    duplicates = catalogue.duplicate_names()
    if duplicates:
        problems.append(
            "catalogue registers the same name twice, so one drawing of it is "
            "overwritten: " + ", ".join(duplicates)
        )
    if not source_dir().is_dir():
        problems.append(
            f"{source_dir()} has not been built yet; run this script without arguments"
        )
    else:
        index = source_dir() / "index.theme"
        if not index.is_file():
            problems.append(f"{index} is missing")
        else:
            text = index.read_text(encoding="utf-8")
            if "Name=" not in text:
                problems.append("index.theme has no Name=, so lxappearance cannot list it")
            if "Inherits=" not in text:
                problems.append("index.theme has no Inherits=, so unlisted icons stay blank")
    if not theme_search_dirs()[0].exists() and not theme_search_dirs()[1].exists():
        problems.append(
            "no user icon directory exists yet; run this script to create one"
        )
    if problems:
        print(f"==> Checking the {THEME_NAME} icon environment")
        print(f"{len(problems)} problem(s) found:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("==> Checking the GnuchanPurple icon environment")
    print("no problems found")
    return 0


# --- command line ------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and install the GnuchanPurple icon theme."
    )
    parser.add_argument("--build-only", action="store_true", help="generate the theme, install nothing")
    parser.add_argument("--uninstall", action="store_true", help="remove the installed theme")
    parser.add_argument("--check", action="store_true", help="report environment problems")
    parser.add_argument("--list", action="store_true", help="list every icon name")
    parser.add_argument("--quiet", action="store_true", help="print only errors")
    args = parser.parse_args()

    def log(message: str) -> None:
        if not args.quiet:
            print(f"    {message}")

    if args.list:
        for name in catalogue.names():
            print(name)
        return 0
    if args.check:
        return check(log, args.quiet)
    if args.uninstall:
        uninstall(log, args.quiet)
        return 0
    build(log, args.quiet)
    if args.build_only:
        return 0
    install_tree(log, args.quiet)
    apply_theme(log, args.quiet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
