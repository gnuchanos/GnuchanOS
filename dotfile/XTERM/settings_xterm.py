#!/usr/bin/env python3
"""GnuchanPurple - xterm installer.

Makes xterm a modern terminal: a real programming font with a Unicode
fallback, Xft antialiasing, 256 colour and true colour, a long scrollback with
a visible scrollbar, wheel, drag and keyboard scrolling, clipboard shortcuts
and UTF-8.

    python3 settings_xterm.py              # install and check
    python3 settings_xterm.py --check      # report problems only
    python3 settings_xterm.py --uninstall  # remove everything it wrote

What it writes lives between marker comments, so a second run replaces the
block instead of appending to it, and --uninstall takes it back out:

    ~/.Xresources                          XTerm resources and Xft rendering
    ~/.config/gnuchan-purple/xterm-env.sh  COLORTERM and a UTF-8 locale
    ~/.profile                             sources that fragment
    ~/.xsessionrc, ~/.xprofile             load ~/.Xresources at login
    ~/.local/share/applications/xterm-gnuchan.desktop   menu entry

~/.Xresources changes nothing until something runs `xrdb -merge`, which is why
the login hooks are written as well as the file itself.

The palette is the one in dotfile/vscodium_theme/settings.json, copied here and
into the GTK theme, kitty and Alacritty so the whole desktop agrees.

License: GPL3
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# --- markers and names -------------------------------------------------------

SCRIPT_NAME = "settings_xterm.py"
MARKER = f"written by {SCRIPT_NAME}"
BACKUP_SUFFIX = ".gnuchan-backup"
CONFIG_DIR_NAME = "gnuchan-purple"

BLOCK_BEGIN = f"! >>> GnuchanPurple xterm - {MARKER}"
BLOCK_END = "! <<< GnuchanPurple xterm"
SH_BEGIN = f"# >>> GnuchanPurple xterm - {MARKER}"
SH_END = "# <<< GnuchanPurple xterm"

# --- fonts -------------------------------------------------------------------
# Faces xterm may draw with, best first. Only the families fontconfig actually
# resolves are written to the resource block: fontconfig silently substitutes a
# family it does not have and xterm cannot tell the substitute from the font it
# asked for, so naming an absent face is how a terminal ends up drawn in
# something nobody chose.

FONT_PREFERRED: tuple[str, ...] = (
    "JetBrains Mono",
    "Hack",
    "Cascadia Code",
    "Fira Code",
    "Source Code Pro",
    "Noto Sans Mono",
    "DejaVu Sans Mono",
    "Liberation Mono",
)

#: Appended after the chosen face, for the characters it has no glyph for.
FONT_COVERAGE: tuple[str, ...] = ("Unifont", "Noto Sans Mono", "DejaVu Sans Mono")

FONT_STACK: tuple[str, ...] = FONT_PREFERRED + FONT_COVERAGE
FONT_SIZE = 12

#: How many families go into the faceName list. xterm takes a comma separated
#: list but only a few entries: past its own limit it prints "too many fonts
#: for fNorm, ignoring <name>" at every start and drops the rest, so a longer
#: list buys a warning instead of glyphs. Two is inside the limit of every
#: build and is all that is needed - one face for the cell size, one for the
#: characters it has no glyph for.
FACE_NAME_LIMIT = 2

#: Installed in one transaction. Every name here exists on every installation
#: of that distribution, so the terminal is never left without a scalable
#: monospaced face.
FONT_PACKAGES: dict[str, tuple[str, ...]] = {
    "debian": ("fonts-dejavu-core",),
    "arch": ("ttf-dejavu",),
    "fedora": ("dejavu-sans-mono-fonts",),
    "suse": ("dejavu-fonts",),
}

#: Tried one at a time: package managers abort a whole transaction over a single
#: unknown name, and these are the names that differ most between distributions.
FONT_PACKAGES_OPTIONAL: dict[str, tuple[str, ...]] = {
    "debian": ("fonts-jetbrains-mono", "fonts-hack", "fonts-unifont"),
    "arch": ("ttf-jetbrains-mono", "ttf-hack", "gnu-unifont"),
    "fedora": ("jetbrains-mono-fonts", "unifont"),
    "suse": ("jetbrains-mono-fonts", "unifont-fonts"),
}

# --- the palette -------------------------------------------------------------
# terminal.foreground, the two backgrounds, terminalCursor.foreground and the
# sixteen slots terminal.ansiBlack .. ansiBrightWhite of settings.json.

PALETTE: dict[str, str] = {
    "foreground": "#ead7ff",
    "background": "#09030d",
    "cursorColor": "#ddb3ff",
    "pointerColor": "#c77dff",
    "pointerColorBackground": "#32143f",
    "highlightColor": "#542080",
    "highlightTextColor": "#ffffff",
    "color0": "#170a20",
    "color1": "#c084fc",
    "color2": "#b56cff",
    "color3": "#d8a4ff",
    "color4": "#9d4edd",
    "color5": "#c77dff",
    "color6": "#b76eff",
    "color7": "#ead7ff",
    "color8": "#70458a",
    "color9": "#d8a4ff",
    "color10": "#c084fc",
    "color11": "#e0aaff",
    "color12": "#b76eff",
    "color13": "#e0aaff",
    "color14": "#d8a4ff",
    "color15": "#ffffff",
}

#: The scrollbar trough and thumb, a shade apart from the window background.
SCROLLBAR_TROUGH = "#140620"
SCROLLBAR_THUMB = "#9d4edd"
SCROLLBAR_WIDTH = 14

# --- the environment ---------------------------------------------------------

COLORTERM_VALUE = "truecolor"
FALLBACK_LOCALE = "C.UTF-8"

# --- locations ---------------------------------------------------------------


def home_dir() -> Path:
    return Path(os.path.expanduser("~")).resolve()


def _xdg_dir(variable: str, fallback: Path) -> Path:
    value = os.environ.get(variable, "").strip()
    return Path(value).expanduser() if value else fallback


def xdg_config_home() -> Path:
    return _xdg_dir("XDG_CONFIG_HOME", home_dir() / ".config")


def xdg_data_home() -> Path:
    return _xdg_dir("XDG_DATA_HOME", home_dir() / ".local" / "share")


def xresources_file() -> Path:
    return home_dir() / ".Xresources"


def env_file() -> Path:
    return xdg_config_home() / CONFIG_DIR_NAME / "xterm-env.sh"


def profile_file() -> Path:
    return home_dir() / ".profile"


def xsessionrc_file() -> Path:
    """Debian's per-user X session hook."""
    return home_dir() / ".xsessionrc"


def xprofile_file() -> Path:
    """The hook the display managers on Arch and most other systems source."""
    return home_dir() / ".xprofile"


def launcher_file() -> Path:
    return xdg_data_home() / "applications" / "xterm-gnuchan.desktop"


# --- the distribution --------------------------------------------------------
# Only the font install needs this; everything else is read by xterm itself.


def os_release() -> dict[str, str]:
    result: dict[str, str] = {}
    try:
        text = Path("/etc/os-release").read_text(encoding="utf-8")
    except OSError:
        return result
    for line in text.splitlines():
        name, separator, value = line.partition("=")
        if separator:
            result[name.strip()] = value.strip().strip('"').strip("'")
    return result


def distro_family() -> str:
    """One of debian, arch, fedora, suse, or unknown."""
    release = os_release()
    tokens = " ".join(
        (release.get("ID", ""), release.get("ID_LIKE", ""), release.get("NAME", ""))
    ).lower()
    for family, markers in (
        ("arch", ("arch", "manjaro", "endeavouros", "garuda")),
        ("debian", ("debian", "ubuntu", "mint", "pop", "kali", "raspbian", "devuan")),
        ("fedora", ("fedora", "rhel", "centos", "rocky", "almalinux")),
        ("suse", ("suse", "sles", "opensuse")),
    ):
        if any(marker in tokens for marker in markers):
            return family
    return "unknown"


def distro_description() -> str:
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


PACKAGE_MANAGERS: dict[str, tuple[str, ...]] = {
    "debian": ("apt-get", "install", "-y", "--no-install-recommends"),
    "arch": ("pacman", "-S", "--needed", "--noconfirm"),
    "fedora": ("dnf", "install", "-y"),
    "suse": ("zypper", "--non-interactive", "install"),
}


def package_manager() -> tuple[str, ...] | None:
    family = distro_family()
    command = PACKAGE_MANAGERS.get(family)
    if command is None or shutil.which(command[0]) is None:
        return None
    return command


def _is_root() -> bool:
    """True when this process can install packages itself.

    geteuid does not exist on Windows, where this script has nothing to do;
    reading it through getattr keeps the module importable and testable there.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def root_prefix() -> list[str]:
    """How to run a package manager, without prompt or password.

    A passwordless sudo or doas is used when it is there; when it is not, the
    caller reports the command instead of stopping mid-install to ask.
    """
    if _is_root():
        return []
    sudo = shutil.which("sudo")
    if sudo is None:
        return []
    if subprocess.run([sudo, "-n", "true"], check=False).returncode == 0:
        return [sudo]
    doas = shutil.which("doas")
    if doas is not None and subprocess.run([doas, "true"], check=False).returncode == 0:
        return [doas]
    return []


# --- output ------------------------------------------------------------------


class Log:
    """Progress output, honouring --quiet."""

    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet

    def step(self, message: str) -> None:
        if not self.quiet:
            print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        if not self.quiet:
            print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        if not self.quiet:
            print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- files -------------------------------------------------------------------


def read_text(path: Path) -> str:
    """Contents of a file, or an empty string when it is not readable."""
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def backup_before_touching(log: Log, path: Path) -> None:
    """Keep one copy of a file, taken before the first run that touches it.

    A second run must not back up this script's own output, or --uninstall
    would restore the very block it is removing.
    """
    if not path.exists() or MARKER in read_text(path):
        return
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return
    shutil.move(str(path), str(backup))
    log.detail(f"backed up {path.name} to {backup.name}")


def _find_marker(lines: list[str], marker: str, start: int = 0) -> int | None:
    for index in range(start, len(lines)):
        if lines[index].strip() == marker:
            return index
    return None


def merge_block(existing: str, begin: str, end: str, block: str) -> str:
    """Replace the region between two marker lines with ``block``.

    Lines outside the markers are kept byte for byte: a first run appends the
    block, a later run replaces it where it is, and an empty block takes the
    region out again. A begin marker with no end after it reaches the end of
    the file, which is what an interrupted run leaves behind.
    """
    lines = existing.splitlines()
    start = _find_marker(lines, begin)
    if start is None:
        kept = list(lines)
    else:
        finish = _find_marker(lines, end, start + 1)
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


def write_managed_file(log: Log, path: Path, text: str) -> None:
    """Write a file this script owns, backing up what was there."""
    backup_before_touching(log, path)
    if text.strip():
        write_text(path, text)
        log.detail(f"wrote {path}")
    elif path.exists():
        path.unlink()
        log.detail(f"removed {path}")


def write_merged_file(log: Log, path: Path, begin: str, end: str, block: str) -> None:
    """Put ``block`` between the markers in a file the user also has settings in.

    The file is read before the backup is taken: backing up is a move, and
    merging afterwards would read a file that is no longer there and replace
    the user's settings with nothing but our block.
    """
    existing = read_text(path)
    backup_before_touching(log, path)
    merged = merge_block(existing, begin, end, block)
    if merged.strip():
        write_text(path, merged)
        log.detail(f"wrote {path}")
    elif path.exists():
        path.unlink()
        log.detail(f"removed {path}")


# --- fonts -------------------------------------------------------------------


def _font_key(name: str) -> str:
    """A family name reduced to what makes two spellings the same family."""
    return re.sub(r"[^a-z0-9]", "", name.lower())


def font_available(family: str) -> bool:
    """Whether fontconfig resolves ``family`` to itself and not to a substitute.

    fc-match never fails: asked for a family it does not have it answers with
    the substitute it would use instead, so the answer is compared with the
    question. The comparison is by containment because the same font is spelled
    differently by different people - fontconfig says Unifont, the package says
    GNU Unifont.
    """
    fc_match = shutil.which("fc-match")
    if fc_match is None:
        return False
    result = subprocess.run(
        [fc_match, "-f", "%{family}", family],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return False
    wanted = _font_key(family)
    for name in result.stdout.split(","):
        resolved = _font_key(name)
        if resolved and (resolved in wanted or wanted in resolved):
            return True
    return False


def resolved_families(families: tuple[str, ...]) -> list[str]:
    """The families fontconfig resolves here, in the order given, without repeats."""
    found: list[str] = []
    for family in families:
        if family not in found and font_available(family):
            found.append(family)
    return found


def unresolved_families() -> list[str]:
    """Every family in the stack that fontconfig does not resolve here.

    Without repeats: a face can be listed as both a preferred and a coverage
    font, and the same name twice in the output reads like a bug.
    """
    missing: list[str] = []
    for family in FONT_STACK:
        if family not in missing and not font_available(family):
            missing.append(family)
    return missing


def has_usable_font() -> bool:
    """Whether any scalable monospaced face resolved at all.

    Without one xterm falls back to the XLFD bitmap font it was built with,
    which is exactly the flat, hard-edged text this script exists to replace.
    """
    return bool(resolved_families(FONT_PREFERRED))


def resolve_face_name() -> str:
    """The comma separated faceName to write, or an empty string for none.

    xterm takes the cell size from the first family and draws a character with
    the ones after it when the first has no glyph for it, so the order is the
    whole value of the list: the best installed face first, then the widest
    coverage font. Only faces that resolve are listed, because fontconfig
    answers a request for a family it does not have with a substitute and xterm
    cannot tell the substitute from the font it asked for. The list stops at
    FACE_NAME_LIMIT, past which xterm keeps the first entries and warns about
    the rest. An empty result means nothing resolved and the caller writes no
    faceName at all.
    """
    preferred = resolved_families(FONT_PREFERRED)
    if not preferred:
        return ""
    families = [preferred[0]]
    for name in resolved_families(FONT_COVERAGE):
        if len(families) >= FACE_NAME_LIMIT:
            break
        if name not in families:
            families.append(name)
    return ", ".join(families)


def package_sets() -> tuple[tuple[str, ...], tuple[str, ...]]:
    """The packages to install in one transaction, and the optional ones."""
    family = distro_family()
    return FONT_PACKAGES.get(family, ()), FONT_PACKAGES_OPTIONAL.get(family, ())


def can_install_packages() -> bool:
    """Whether a package manager here could run without asking for a password."""
    return package_manager() is not None and (_is_root() or bool(root_prefix()))


def manual_font_hint() -> str:
    """What to tell the user when this script cannot install the fonts itself."""
    base, extras = package_sets()
    if not base:
        return (
            "install a scalable monospaced font - JetBrains Mono, Hack or DejaVu "
            f"Sans Mono - for your distribution ({distro_description()}), then "
            "run this script again"
        )
    prefix = {
        "debian": "sudo apt-get install",
        "arch": "sudo pacman -S",
        "fedora": "sudo dnf install",
        "suse": "sudo zypper install",
    }[distro_family()]
    return f"run: {prefix} {' '.join((*base, *extras))}"


def _install_packages(log: Log, packages: tuple[str, ...]) -> bool:
    manager = package_manager()
    if manager is None or not packages:
        return False
    command = [*root_prefix(), *manager, *packages]
    log.detail("running: " + " ".join(command))
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        log.detail(f"{packages[0]}: the package manager returned {result.returncode}")
        return False
    return True


def _rebuild_font_cache() -> None:
    """Rebuild the fontconfig cache after installing fonts.

    Both package managers run the hook, but a font can land before the hook
    exists on a minimal system, and rebuilding is cheap and idempotent.
    """
    cache = shutil.which("fc-cache")
    if cache is not None:
        subprocess.run([cache, "-f"], check=False, capture_output=True)


def install_fonts(log: Log, allow_packages: bool = True) -> bool:
    """Make sure a usable font stack is present, returning whether it is.

    A missing font does not stop the install: xterm falls back to its built-in
    bitmap font, so the terminal still runs and only looks wrong, which is why
    a failure here is reported rather than raised.
    """
    if not unresolved_families():
        log.detail("fonts installed: " + ", ".join(FONT_STACK))
        return True

    log.detail("not installed: " + ", ".join(unresolved_families()))
    if not allow_packages or not can_install_packages():
        log.detail(manual_font_hint())
        return has_usable_font()

    base, extras = package_sets()
    if not base:
        log.detail(manual_font_hint())
        return has_usable_font()

    _install_packages(log, base)
    _rebuild_font_cache()

    # The extra faces one at a time: a single unknown name aborts a whole
    # transaction, and losing the rest of them to it is worse than the wait.
    for package in extras:
        if not unresolved_families():
            break
        if _install_packages(log, (package,)):
            _rebuild_font_cache()

    if unresolved_families():
        log.detail("still not installed: " + ", ".join(unresolved_families()))
        log.detail(manual_font_hint())
    if not has_usable_font():
        return False
    log.detail("font in use: " + resolved_families(FONT_PREFERRED)[0])
    return True


# --- the resource block ------------------------------------------------------
# One comment per group, on the line above the settings it explains.


def _resource_lines() -> list[str]:
    """The XTerm resources, in the order xterm reads them."""
    lines: list[str] = []

    face_name = resolve_face_name()
    if face_name:
        # The cell size comes from the first family, the rest draw the
        # characters it has no glyph for.
        lines += [
            f"XTerm*faceName: {face_name}",
            f"XTerm*faceSize: {FONT_SIZE}",
            f"XTerm*faceNameDoublesize: {face_name}",
            "XTerm*renderFont: true",
        ]
    else:
        lines += [
            "! No scalable font resolved, so no font is named above and xterm",
            "! falls back to its built-in bitmap font. Run the script again once",
            "! a font is installed.",
        ]

    lines += [
        "",
        "! terminal type, shell and encoding",
        "XTerm*termName: xterm-256color",
        "XTerm*loginShell: true",
        "XTerm*utf8: always",
        "XTerm*locale: true",
        "",
        "! antialiased text: xterm draws through Xft, which reads these from the",
        "! same resource database, so they need xrdb just as the rest does",
        "Xft.antialias: true",
        "Xft.hinting: true",
        "Xft.hintstyle: hintslight",
        "Xft.rgba: rgb",
        "Xft.lcdfilter: lcddefault",
        "",
        "! window, cursor and pointer",
        "XTerm*internalBorder: 8",
        "XTerm*saveLines: 10000",
        "XTerm*cursorBlink: false",
        f"XTerm*pointerColor: {PALETTE['pointerColor']}",
        f"XTerm*pointerColorBackground: {PALETTE['pointerColorBackground']}",
        "",
        "! selection and clipboard; the charClass makes a double click take a",
        "! whole path or URL instead of one punctuation character of it",
        "XTerm*selectToClipboard: true",
        "XTerm*trimSelection: true",
        "XTerm*charClass: 33:48,37:48,45-47:48,58:48,64:48,126:48",
        "XTerm*highlightColorMode: true",
        "XTerm*allowWindowOps: true",
        "",
        "! keys: Alt is ESC, Backspace is DEL, modified keys are reported",
        "XTerm*altSendsEscape: true",
        "XTerm*eightBitInput: false",
        "XTerm*backarrowKey: false",
        "XTerm*backarrowKeyIsErase: false",
        "XTerm*ptyInitialErase: false",
        "XTerm*modifyOtherKeys: 2",
        "",
        "! scrolling. The wheel works in full screen programs and in the shell,",
        "! Shift+wheel forces the scrollback while a program has the mouse, and",
        "! the scrollbar is drawn on the right so the buffer can be dragged. The",
        "! thumb fills the trough while the scrollback is empty, which is what a",
        "! drag that moves nothing usually means.",
        "XTerm*alternateScroll: true",
        "XTerm*scrollTtyOutput: false",
        "XTerm*scrollKey: true",
        "XTerm*fastScroll: true",
        "XTerm*jumpScroll: true",
        "XTerm*multiScroll: true",
        "XTerm*scrollBar: true",
        "XTerm*rightScrollBar: true",
        f"XTerm*Scrollbar*width: {SCROLLBAR_WIDTH}",
        f"XTerm*Scrollbar*Background: {SCROLLBAR_TROUGH}",
        f"XTerm*Scrollbar*Foreground: {SCROLLBAR_THUMB}",
        "XTerm*Scrollbar*BorderWidth: 0",
        "XTerm*Scrollbar*Cursor: arrow",
        "",
        "! sixel images scroll with the text instead of sticking to the window",
        "XTerm*sixelScrolling: true",
        "",
        "! colours, from dotfile/vscodium_theme/settings.json",
    ]

    for name in (
        "foreground",
        "background",
        "cursorColor",
        "highlightColor",
        "highlightTextColor",
    ):
        lines.append(f"XTerm*{name}: {PALETTE[name]}")
    for index in range(16):
        lines.append(f"XTerm*color{index}: {PALETTE[f'color{index}']}")

    lines += [
        "",
        "! bindings. #override adds these to the defaults instead of replacing",
        "! them, and no scrolling is bound here on purpose: the defaults already",
        "! carry the wheel, Shift+wheel and Shift+PageUp/PageDown, and naming an",
        "! action this build does not have is a warning at every start for no gain.",
        "XTerm*vt100.translations: #override \\n\\",
        "        Ctrl Shift <Key>C: copy-selection(CLIPBOARD) \\n\\",
        "        Ctrl Shift <Key>V: insert-selection(CLIPBOARD) \\n\\",
        "        Ctrl <Key>Insert: copy-selection(CLIPBOARD) \\n\\",
        "        Shift <Key>Insert: insert-selection(PRIMARY,CLIPBOARD) \\n\\",
        "        Ctrl <Key>plus: larger-vt-font() \\n\\",
        "        Ctrl <Key>minus: smaller-vt-font() \\n\\",
        "        Ctrl <Key>0: set-vt-font(d) \\n\\",
        "        Ctrl Shift <Key>N: spawn-new-terminal()",
    ]
    return lines


def xterm_block() -> str:
    """The block this script owns inside ``~/.Xresources``."""
    header = [
        BLOCK_BEGIN,
        "!",
        "! Written by the installer and replaced on every run: edit the script,",
        "! not this block. Load it with:  xrdb -merge ~/.Xresources",
        "!",
        "! xrdb runs this file through the C preprocessor, so no apostrophes.",
    ]
    return "\n".join(header + _resource_lines() + [BLOCK_END])


# --- the environment ---------------------------------------------------------


def environment_snippet() -> str:
    """The environment a terminal cannot set for itself."""
    return "\n".join(
        [
            SH_BEGIN,
            "# Sourced from ~/.profile; remove that line to stop using it.",
            "",
            "# xterm renders direct colour but does not announce it, and COLORTERM",
            "# is how a program learns it may use 24 bit colour. Only set for xterm,",
            "# so no other terminal in the session inherits the claim.",
            'case "${TERM:-}" in',
            f"  xterm*) export COLORTERM={COLORTERM_VALUE} ;;",
            "esac",
            "",
            "# A UTF-8 terminal in a C locale prints question marks instead of text.",
            'case "${LANG:-}" in',
            f'  ""|C|POSIX) export LANG={FALLBACK_LOCALE} ;;',
            "esac",
            SH_END,
        ]
    )


def profile_snippet() -> str:
    """The lines that make ``~/.profile`` pick the environment up."""
    return "\n".join(
        [
            SH_BEGIN,
            f'[ -f "{env_file()}" ] && . "{env_file()}"',
            SH_END,
        ]
    )


def session_snippet() -> str:
    """The lines that load the resource database when the session starts.

    Without them ~/.Xresources takes effect only once something runs xrdb by
    hand, and after a reboot the X server starts with no resources at all.
    """
    return "\n".join(
        [
            SH_BEGIN,
            'if [ -f "$HOME/.Xresources" ] && command -v xrdb >/dev/null 2>&1; then',
            '    xrdb -merge "$HOME/.Xresources"',
            "fi",
            SH_END,
        ]
    )


def launcher_text() -> str:
    """A desktop entry, so xterm is reachable from the application menu."""
    return "\n".join(
        [
            f"# {MARKER}",
            "[Desktop Entry]",
            "Type=Application",
            "Version=1.1",
            "Name=XTerm",
            "GenericName=Terminal",
            "Comment=GnuchanPurple terminal",
            "Exec=xterm -ls",
            "Icon=utilities-terminal",
            "Terminal=false",
            "Categories=System;TerminalEmulator;",
            "Keywords=shell;prompt;command;commandline;terminal;",
            "StartupNotify=true",
            "StartupWMClass=XTerm",
            "",
        ]
    )


# --- installing --------------------------------------------------------------


def install_resources(log: Log) -> None:
    """Merge the xterm block into ``~/.Xresources``."""
    write_merged_file(log, xresources_file(), BLOCK_BEGIN, BLOCK_END, xterm_block())


def install_environment(log: Log) -> None:
    """Write the shell fragment and make ``~/.profile`` source it."""
    write_managed_file(log, env_file(), environment_snippet())
    write_merged_file(log, profile_file(), SH_BEGIN, SH_END, profile_snippet())


def install_session_hooks(log: Log) -> None:
    """Make the session load the resource database when it starts.

    Both hooks are written because neither is read by the other: Debian's X
    session sources ~/.xsessionrc, and the display managers elsewhere source
    ~/.xprofile.
    """
    for path in (xsessionrc_file(), xprofile_file()):
        write_merged_file(log, path, SH_BEGIN, SH_END, session_snippet())


def install_launcher(log: Log) -> None:
    """Add the application menu entry."""
    write_managed_file(log, launcher_file(), launcher_text())


def load_resources(log: Log) -> bool:
    """Merge ``~/.Xresources`` into the running X server, returning success.

    This is what makes the settings apply now rather than at the next login,
    and it is also the step that fails quietly when there is no X server to
    talk to, which is why it reports what it did.
    """
    xrdb = shutil.which("xrdb")
    if xrdb is None:
        log.detail("xrdb is not installed; the session hook loads the file at login")
        return False
    if not os.environ.get("DISPLAY"):
        log.detail("DISPLAY is not set; the settings apply to the next X session")
        return False
    if subprocess.run([xrdb, "-merge", str(xresources_file())], check=False).returncode:
        log.detail("xrdb could not load the resources")
        return False
    log.detail("loaded into the running X server")
    return True


# --- checking ----------------------------------------------------------------


def terminfo_available(name: str = "xterm-256color") -> bool:
    """Whether the terminfo entry the termName resource names exists."""
    infocmp = shutil.which("infocmp")
    if infocmp is None:
        return True
    return subprocess.run([infocmp, name], check=False, capture_output=True).returncode == 0


def locale_is_utf8() -> bool:
    value = os.environ.get("LC_ALL") or os.environ.get("LC_CTYPE") or os.environ.get("LANG", "")
    return "utf" in value.lower()


def resources_loaded() -> bool:
    """Whether the running X server has the xterm resources in its database."""
    xrdb = shutil.which("xrdb")
    if xrdb is None or not os.environ.get("DISPLAY"):
        return True
    result = subprocess.run([xrdb, "-query"], check=False, capture_output=True, text=True)
    if result.returncode != 0:
        return True
    return "xterm*termname" in result.stdout.lower()


def scrollbar_supported() -> bool:
    """Whether this xterm was built with a scrollbar.

    The scrollbar is a build time option and the resources that ask for one are
    ignored without it, which is the one failure of the scrolling setup that no
    amount of configuration can fix. The help text lists the options that were
    compiled in, so -sb or +sb being absent from it is the answer.
    """
    xterm = shutil.which("xterm")
    if xterm is None:
        return True
    result = subprocess.run([xterm, "-help"], check=False, capture_output=True, text=True)
    text = result.stdout or ""
    return True if not text.strip() else bool(re.search(r"(^|\s)[-+]sb(\s|$)", text))


#: Settings the installed block must contain for the terminal to be the one
#: this script promises. The scrollbar is here because it is the setting that
#: an xterm build, a later resource file or an older run of this script can
#: take away without saying so.
REQUIRED_SETTINGS: tuple[str, ...] = (
    "xterm*scrollbar: true",
    "xterm*rightscrollbar: true",
    "xterm*savelines:",
)


def missing_settings() -> list[str]:
    """Which of the required settings are absent from the installed block."""
    text = read_text(xresources_file()).lower()
    return [setting for setting in REQUIRED_SETTINGS if setting not in text]


def check_environment(log: Log) -> int:
    """Report what would stop xterm looking the way this script intends.

    Each check covers a failure that is invisible from the outside: a font that
    does not resolve leaves xterm on its built-in bitmap font, a missing
    terminfo entry limits every program to eight colours, a non-UTF-8 locale
    prints question marks, resources that were never loaded leave the file on
    disk doing nothing, and an xterm is the one case where none of the rest
    matters.
    """
    problems: list[str] = []

    if shutil.which("xterm") is None:
        problems.append("xterm is not installed")
    elif not scrollbar_supported():
        problems.append(
            "this xterm was built without a scrollbar, so the scrollBar "
            "resources do nothing in it; install the distribution's xterm"
        )

    if not has_usable_font():
        problems.append(
            "no scalable monospaced font resolves, so xterm draws with its "
            f"built-in bitmap font ({manual_font_hint()})"
        )
    else:
        log.note(f"font in use: {resolved_families(FONT_PREFERRED)[0]}")
        absent_fonts = unresolved_families()
        if absent_fonts:
            log.note("optional, not installed: " + ", ".join(absent_fonts))

    if not terminfo_available():
        problems.append(
            "the terminfo entry xterm-256color is missing; install ncurses-term, "
            "or the terminal is limited to 8 colours"
        )

    if not locale_is_utf8():
        problems.append(
            "the locale is not UTF-8, so non-ASCII text is not displayed "
            "correctly (see the xterm-env.sh this script installs)"
        )

    if BLOCK_BEGIN not in read_text(xresources_file()):
        problems.append(f"{xresources_file()} has no xterm block; run the script")
    else:
        missing = missing_settings()
        if missing:
            problems.append("the block is missing " + ", ".join(missing) + "; run the script")

    if not resources_loaded():
        problems.append(
            "the X server has no XTerm resources loaded; run "
            f"xrdb -merge {xresources_file()}"
        )

    if problems:
        log.note(f"{len(problems)} problem(s) found:")
        for problem in problems:
            log.note(f"  {problem}")
    else:
        log.note("No problems found.")
    return len(problems)


# --- uninstalling ------------------------------------------------------------


def uninstall(log: Log) -> None:
    """Remove everything this script wrote, and nothing else.

    Shared files keep whatever the user had in them: the block is taken out
    between its markers. A .gnuchan-backup copy is put back only when taking
    the block out would leave the file empty, because otherwise that copy is a
    duplicate of what is already there and the live file is the newer one.
    """
    for path, begin, end in (
        (xresources_file(), BLOCK_BEGIN, BLOCK_END),
        (profile_file(), SH_BEGIN, SH_END),
        (xsessionrc_file(), SH_BEGIN, SH_END),
        (xprofile_file(), SH_BEGIN, SH_END),
    ):
        if not path.exists():
            continue
        merged = merge_block(read_text(path), begin, end, "")
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if backup.exists() and not merged.strip():
            path.unlink()
            shutil.move(str(backup), str(path))
            log.detail(f"restored {path} from its backup")
        elif merged.strip():
            write_text(path, merged)
            log.detail(f"removed our block from {path}")
        else:
            path.unlink()
            log.detail(f"removed {path}")

    for path in (env_file(), launcher_file()):
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if path.exists():
            path.unlink()
            log.detail(f"removed {path}")
        if not backup.exists():
            continue
        if MARKER in read_text(backup):
            # A backup holding our own output has nothing of the user's in it.
            backup.unlink()
            log.detail(f"removed {backup}")
            continue
        shutil.move(str(backup), str(path))
        log.detail(f"restored {path} from its backup")

    config_dir = env_file().parent
    if config_dir.is_dir() and not any(config_dir.iterdir()):
        config_dir.rmdir()
        log.detail(f"removed the empty {config_dir}")

    log.detail("the fonts were left installed; remove them with your package manager")


# --- entry point -------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=SCRIPT_NAME,
        description="Configure xterm as a modern terminal.",
    )
    parser.add_argument(
        "--check", action="store_true", help="report problems, then stop"
    )
    parser.add_argument(
        "--uninstall", action="store_true", help="remove everything this script wrote"
    )
    parser.add_argument(
        "--no-packages",
        action="store_true",
        help="never run a package manager; print the command instead",
    )
    parser.add_argument(
        "--no-load",
        action="store_true",
        help="do not ask the running X server to reload the resources",
    )
    parser.add_argument("--quiet", action="store_true", help="only print problems")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    log = Log(quiet=args.quiet)

    if args.uninstall:
        log.step("Removing the xterm configuration")
        uninstall(log)
        log.note("")
        log.note("Removed. Reload the resources of a running session with:")
        log.note(f"  xrdb -merge {xresources_file()}")
        return 0

    if args.check:
        log.step("Checking the xterm environment")
        return 1 if check_environment(log) else 0

    log.step(f"Configuring xterm ({distro_description()})")
    install_fonts(log, allow_packages=not args.no_packages)
    install_resources(log)
    install_environment(log)
    install_session_hooks(log)
    install_launcher(log)

    if not args.no_load:
        log.step("Loading the resources")
        load_resources(log)

    log.step("Checking the result")
    problems = check_environment(log)

    log.note("")
    log.note("Done. Open a new xterm: a window that is already running keeps")
    log.note("the settings it started with.")
    log.note("")
    log.note("  Ctrl+Shift+C / Ctrl+Shift+V       copy and paste")
    log.note("  Ctrl+plus / Ctrl+minus / Ctrl+0   font size")
    log.note("  Shift+PageUp / Shift+PageDown     one page of scrollback")
    log.note("  wheel, Shift+wheel                scroll, and force it in less/vim")
    log.note("  drag the scrollbar on the right   the scrollback, by hand")
    log.note("")
    log.note(f"Settings: {xresources_file()}")
    log.note(f"Remove them with: {SCRIPT_NAME} --uninstall")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
