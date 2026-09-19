#!/usr/bin/env python3
# =============================================================================
# GnuchanPurple - xterm installer
# -----------------------------------------------------------------------------
# Makes xterm behave like a modern terminal: 256 colours and true colour, a
# scalable GNU font with a Unicode fallback, a long scrollback, wheel scrolling
# that works inside less and vim, the usual clipboard shortcuts, bracketed
# paste, sixel graphics and UTF-8 everywhere. Single file, standard library
# only.
#
# What it does
# ------------
#   1. installs the GNU fonts xterm draws with - FreeMono from GNU FreeFont, and
#      GNU Unifont as the fallback for the characters FreeMono does not have -
#      through apt on Debian and Ubuntu, pacman on Arch, dnf or zypper
#      elsewhere, and reports the exact command if it cannot run one;
#   2. writes a marked block into ~/.Xresources that configures xterm, leaving
#      every other line of that file alone: the block is what this script owns
#      and removing it is what --uninstall does;
#   3. makes the session load that file at login, which is the step people
#      miss: editing ~/.Xresources changes nothing until something runs
#      xrdb -merge, and after a reboot the X server starts with no resources
#      at all;
#   4. writes the environment a terminal cannot set for itself
#      (COLORTERM=truecolor, and a UTF-8 locale when there is none) into
#      ~/.config/gnuchan-purple/xterm-env.sh and sources it from ~/.profile;
#   5. adds a launcher so xterm is in the application menu, which is the only
#      way to reach it on a desktop with no terminal keybinding;
#   6. loads the resources now with xrdb and checks the result: the font
#      resolves, the terminfo entry exists, the locale is UTF-8, xterm answers.
#
#     python3 settings_xterm.py              # install and check
#     python3 settings_xterm.py --check      # report problems only
#     python3 settings_xterm.py --uninstall  # remove everything it wrote
#
# The palette is the one the GTK theme and VSCodium use, so the terminal, the
# widgets and the editor agree. dotfile/vscodium_theme/settings.json holds the
# sixteen ANSI slots and is the source of truth: change it there first, then
# copy the values into PALETTE below - see dotfile/GTK_THEME/GnuchanPurple/
# extras/Xresources for the same values in the X11 spelling.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# --- what this script is called and where it leaves its mark -----------------

SCRIPT_NAME = "settings_xterm.py"
MARKER = f"written by {SCRIPT_NAME}"
BACKUP_SUFFIX = ".gnuchan-backup"
CONFIG_DIR_NAME = "gnuchan-purple"

#: The two lines that delimit the part of a file this script owns. Everything
#: between them is replaced on every run and nothing outside them is touched,
#: which is what makes running the script twice harmless and --uninstall
#: possible without knowing what the previous run wrote.
BLOCK_BEGIN = f"! >>> GnuchanPurple xterm - {MARKER}"
BLOCK_END = "! <<< GnuchanPurple xterm"

#: The shell comment form of the same markers, for ~/.profile and ~/.xsessionrc.
SH_BEGIN = f"# >>> GnuchanPurple xterm - {MARKER}"
SH_END = "# <<< GnuchanPurple xterm"

# --- fonts -------------------------------------------------------------------
# GNU FreeFont is the family, FreeMono is the terminal face in it: a monospaced
# TrueType face with a wide repertoire that is packaged everywhere and is what
# "the GNU font" means on a GNU system. It is not, however, complete - FreeMono
# has no CJK and no emoji - so GNU Unifont is named second. xterm takes a
# comma-separated list for faceName and uses the second entry as the fallback
# when the first has no glyph, which is exactly the relationship these two have.

FONT_PRIMARY = "FreeMono"
FONT_FALLBACK = "Unifont"
FONT_SIZE = 12

#: Families to look for, in the order xterm will use them.
FONT_FAMILIES: tuple[str, ...] = (FONT_PRIMARY, FONT_FALLBACK)

#: Font packages by distribution family. The names differ between Debian and
#: Arch, and both are needed: FreeFont for the face, Unifont for the coverage.
FONT_PACKAGES: dict[str, tuple[str, ...]] = {
    "debian": ("fonts-freefont-ttf", "fonts-unifont"),
    "arch": ("gnu-free-fonts", "gnu-unifont"),
    "fedora": ("gnu-free-mono-fonts", "unifont", "unifont-fonts"),
    "suse": ("gnu-free-fonts", "unifont-fonts"),
}

# --- the palette -------------------------------------------------------------
# fg, bg and the sixteen ANSI slots, matching dotfile/vscodium_theme/
# settings.json and dotfile/GTK_THEME/GnuchanPurple/extras/Xresources. The file
# in the GTK theme is the X11 spelling of these same values; if the two ever
# disagree, that file and this table are both wrong and the VSCodium file is
# right.

PALETTE: dict[str, str] = {
    # The window itself
    "foreground": "#ead7ff",
    "background": "#09030d",
    "cursorColor": "#ddb3ff",
    "pointerColorForeground": "#c77dff",
    "pointerColorBackground": "#32143f",
    # Selection: highlightColor is the background of selected text, which is the
    # opposite of what the name suggests; highlightTextColor is its foreground.
    # Both only take effect when highlightColorMode is on.
    "highlightColor": "#542080",
    "highlightTextColor": "#ffffff",
    # Normal black .. white
    "color0": "#170a20",
    "color1": "#c084fc",
    "color2": "#b56cff",
    "color3": "#d8a4ff",
    "color4": "#9d4edd",
    "color5": "#c77dff",
    "color6": "#b76eff",
    "color7": "#ead7ff",
    # Bright black .. white
    "color8": "#70458a",
    "color9": "#d8a4ff",
    "color10": "#c084fc",
    "color11": "#e0aaff",
    "color12": "#b76eff",
    "color13": "#e0aaff",
    "color14": "#d8a4ff",
    "color15": "#ffffff",
}

# --- the environment ---------------------------------------------------------

#: What an application is told about colour support. xterm renders direct
#: colour without being asked, but it does not set COLORTERM, so a program that
#: checks for it - and most modern ones do, because it is how they decide
#: whether to use 24 bit colour - has to be told by the shell.
COLORTERM_VALUE = "truecolor"

#: The locale to fall back to when the session has none, or has one of the
#: historic C or POSIX locales - the two cases where a UTF-8 terminal produces
#: question marks instead of text.
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
    """The X resource database file, which is what `xrdb -merge` reads."""
    return home_dir() / ".Xresources"


def env_file() -> Path:
    """The shell fragment that carries the environment a terminal cannot set."""
    return xdg_config_home() / CONFIG_DIR_NAME / "xterm-env.sh"


def profile_file() -> Path:
    return home_dir() / ".profile"


def xsessionrc_file() -> Path:
    """Debian's per-user X session hook, sourced before the session starts."""
    return home_dir() / ".xsessionrc"


def xprofile_file() -> Path:
    """The hook display managers source, used by Arch and by most others."""
    return home_dir() / ".xprofile"


def launcher_file() -> Path:
    return xdg_data_home() / "applications" / "xterm-gnuchan.desktop"
# --- which distribution this is ---------------------------------------------
# Only the font install needs this: everything else this script writes is read
# by xterm itself and looks the same on every distribution. The family is
# derived from ID_LIKE as well as ID, because Debian derivatives - Ubuntu,
# Mint, Pop, Kali - all report ID_LIKE=debian and all of them use apt.


def os_release() -> dict[str, str]:
    """Parse ``/etc/os-release`` into a dictionary, empty if it is absent."""
    result: dict[str, str] = {}
    try:
        text = Path("/etc/os-release").read_text(encoding="utf-8")
    except OSError:
        return result
    for line in text.splitlines():
        name, separator, value = line.partition("=")
        if not separator:
            continue
        result[name.strip()] = value.strip().strip('"').strip("'")
    return result


def distro_family() -> str:
    """One of ``debian``, ``arch``, ``fedora``, ``suse``, or ``unknown``."""
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


#: The install command per family, without the package names.
PACKAGE_MANAGERS: dict[str, tuple[str, ...]] = {
    "debian": ("apt-get", "install", "-y", "--no-install-recommends"),
    "arch": ("pacman", "-S", "--needed", "--noconfirm"),
    "fedora": ("dnf", "install", "-y"),
    "suse": ("zypper", "--non-interactive", "install"),
}


def distro_description() -> str:
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def package_manager() -> tuple[str, ...] | None:
    """The install command for this distribution, or None if unsupported."""
    family = distro_family()
    command = PACKAGE_MANAGERS.get(family)
    if command is None or shutil.which(command[0]) is None:
        return None
    return command


def root_prefix() -> list[str]:
    """How to run a package manager, given who is running this script.

    ``sudo`` is asked for without a password prompt first: a script that
    installs fonts should not stop in the middle of a run to ask for one, and
    reporting the command that needs a password is more useful than a prompt
    that arrives after part of the work is done.
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


# --- logging -----------------------------------------------------------------


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


# --- file helpers ------------------------------------------------------------


def read_text(path: Path) -> str:
    """Contents of a file, or an empty string when it is not readable."""
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def backup_once(path: Path) -> Path | None:
    """Move an existing file aside, returning where it went.

    The backup is taken once and kept: a second run must not overwrite it with
    this script's own output, because the file worth keeping is the one the user
    had before this script ever touched it.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.move(str(path), str(backup))
    return backup


def backup_before_touching(log: Log, path: Path) -> None:
    """Keep a copy of a file, unless the file is already ours.

    The backup is what --uninstall puts back, so it has to be the file the user
    had and not this script's own previous output: a second run that backed up
    its own first run would leave the backup holding the very block uninstall is
    trying to remove, and restoring it would reinstall what was just removed.
    The marker is what tells the two apart, because every file this script
    writes carries it.
    """
    if not path.exists() or MARKER in read_text(path):
        return
    backup = backup_once(path)
    if backup is not None:
        log.detail(f"backed up {path.name} to {backup.name}")


def write_managed_file(log: Log, path: Path, text: str) -> None:
    """Write a file this script owns, backing up what was there and logging it."""
    backup_before_touching(log, path)
    if text.strip():
        write_text(path, text)
        log.detail(f"wrote {path}")
        return
    if path.exists():
        path.unlink()
        log.detail(f"removed {path}")


def write_merged_file(log: Log, path: Path, begin: str, end: str, block: str) -> None:
    """Put ``block`` between the markers in a file the user also has settings in.

    This is the case the backup rule exists for: ``~/.profile`` and
    ``~/.Xresources`` are the user's files that this script only adds a region
    to, so the copy kept is taken before the first run that touches them and
    never again.
    """
    backup_before_touching(log, path)
    merged = merge_block(read_text(path), begin, end, block)
    if merged.strip():
        write_text(path, merged)
        log.detail(f"wrote {path}")
        return
    if path.exists():
        path.unlink()
        log.detail(f"removed {path}")


def _find_marker(lines: list[str], marker: str, start: int = 0) -> int | None:
    for index in range(start, len(lines)):
        if lines[index].strip() == marker:
            return index
    return None


def merge_block(existing: str, begin: str, end: str, block: str) -> str:
    """Replace the region between two marker lines with ``block``.

    Everything outside the markers is kept, byte for byte: a first run appends
    the block to whatever the user already had, a later run replaces the block
    in place so it does not creep down the file, and an uninstall - an empty
    ``block`` - takes the region back out. A begin marker with no end after it
    is treated as reaching the end of the file, which is the state an
    interrupted previous run leaves behind.
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
# --- fonts -------------------------------------------------------------------


def _font_key(name: str) -> str:
    """A family name reduced to what makes two spellings the same family."""
    return re.sub(r"[^a-z0-9]", "", name.lower())


def font_available(family: str) -> bool:
    """Whether fontconfig resolves ``family`` to itself.

    ``fc-match`` never fails: asked for a family it does not have it answers
    with the substitute it would use instead, so the answer has to be compared
    with the question. The comparison is by containment because the same family
    is spelled differently by different people - fontconfig calls GNU Unifont
    "Unifont", the package calls it "GNU Unifont", and both mean the one font.
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


def missing_fonts() -> list[str]:
    """The families in ``FONT_FAMILIES`` that are not installed."""
    return [family for family in FONT_FAMILIES if not font_available(family)]


def font_install_command() -> list[str] | None:
    """The command that installs both GNU fonts here, or None if unknown."""
    packages = FONT_PACKAGES.get(distro_family())
    manager = package_manager()
    if not packages or manager is None:
        return None
    return [*root_prefix(), *manager, *packages]


def manual_font_hint() -> str:
    """What to tell the user when the script cannot install the fonts itself."""
    family = distro_family()
    packages = FONT_PACKAGES.get(family)
    if not packages:
        return (
            "install the GNU FreeFont and GNU Unifont packages for your "
            f"distribution ({distro_description()}), then run this script again"
        )
    if family == "debian":
        return f"run: sudo apt-get install {' '.join(packages)}"
    if family == "arch":
        return f"run: sudo pacman -S {' '.join(packages)}"
    if family == "fedora":
        return f"run: sudo dnf install {' '.join(packages)}"
    return f"run: sudo zypper install {' '.join(packages)}"


def install_fonts(log: Log, allow_packages: bool = True) -> bool:
    """Make sure the GNU fonts are present, returning whether they are.

    A font that is missing does not stop the install: xterm falls back to the
    XLFD fonts it was built with, so the terminal still runs and only looks
    wrong. That is why a failure here is reported rather than raised.
    """
    missing = missing_fonts()
    if not missing:
        log.detail(
            "the GNU fonts are installed: "
            + ", ".join(FONT_FAMILIES)
        )
        return True

    log.detail("missing: " + ", ".join(missing))
    command = font_install_command()
    if command is None or not allow_packages:
        log.detail(manual_font_hint())
        return False

    if not _is_root() and not root_prefix():
        # Neither root nor a passwordless sudo or doas: running the package
        # manager would fail on the spot, so say what to run instead of
        # printing a permission error the user cannot act on.
        log.detail(manual_font_hint())
        return False

    log.detail("running: " + " ".join(command))
    result = subprocess.run(command, check=False)
    cache = shutil.which("fc-cache")
    if cache is not None:
        # Both package managers run the fontconfig hook, but a font installed
        # with --no-install-recommends on a minimal system can land before the
        # hook exists, and rebuilding the cache is cheap and idempotent.
        subprocess.run([cache, "-f"], check=False, capture_output=True)
    if result.returncode != 0:
        log.detail(manual_font_hint())
        return False
    still_missing = missing_fonts()
    if still_missing:
        log.detail("still missing after installing: " + ", ".join(still_missing))
        return False
    log.detail("installed: " + ", ".join(FONT_FAMILIES))
    return True


# --- the xterm resource block ------------------------------------------------
# Every resource below was checked against the xterm manual for the version
# Debian and Arch ship, and the comment above each group says what it is for.
# Nothing here is set to a value xterm already defaults to unless being explicit
# is the point - a configuration file that repeats defaults is a file whose real
# settings are hard to find.


def _resource_lines() -> list[str]:
    """The ``XTerm*`` resources, in the order they are read."""
    lines: list[str] = []

    lines += [
        "! The terminal name programs read. xterm-256color is a terminfo entry",
        "! that advertises 256 colours; without it every program assumes 8.",
        "XTerm*termName: xterm-256color",
        "",
        "! A login shell, so ~/.profile is read and the environment file this",
        "! script installs is picked up.",
        "XTerm*loginShell: true",
    ]

    lines += [
        "",
        "! --- the font ---------------------------------------------------------",
        "! FreeMono is the monospaced face of GNU FreeFont. The second family is",
        "! the fallback xterm uses for characters the first does not have, which",
        "! is how CJK and the rarer symbols get drawn at all.",
        f"XTerm*faceName: {FONT_PRIMARY}, {FONT_FALLBACK}",
        f"XTerm*faceSize: {FONT_SIZE}",
        "! Double width characters (CJK) are drawn at twice the width, from the",
        "! same family, instead of being stretched to fit.",
        f"XTerm*faceNameDoublesize: {FONT_PRIMARY}",
        "! Use the scalable font rather than the XLFD bitmap one.",
        "XTerm*renderFont: true",
    ]

    lines += [
        "",
        "! --- text and locale --------------------------------------------------",
        "! always: UTF-8 is on and cannot be turned off by an escape sequence.",
        "! locale: follow the locale of the user, converting through luit when",
        "! it is not UTF-8. Together they are what makes accented and non-Latin",
        "! text come out as text rather than as question marks.",
        "XTerm*utf8: always",
        "XTerm*locale: true",
    ]

    lines += [
        "",
        "! --- the window -------------------------------------------------------",
        "! internalBorder is the padding between the text and the window edge.",
        "! A terminal with none looks like a wall of text pressed against glass.",
        "XTerm*internalBorder: 8",
        "! 10000 lines of scrollback, which is the point at which nobody scrolls",
        "! further by hand and the memory is still measured in megabytes.",
        "XTerm*saveLines: 10000",
        "XTerm*cursorBlink: true",
        "XTerm*pointerColorForeground: " + PALETTE["pointerColorForeground"],
        "XTerm*pointerColorBackground: " + PALETTE["pointerColorBackground"],
    ]

    lines += [
        "",
        "! --- selection and clipboard ------------------------------------------",
        "! Selecting text puts it on the clipboard as well as on the primary",
        "! selection, so a select and a paste into a browser both work.",
        "XTerm*selectToClipboard: true",
        "! Do not copy the trailing spaces that a full line of text carries.",
        "XTerm*trimSelection: true",
        "! Use highlightColor and highlightTextColor below instead of reverse",
        "! video, which is what makes a selection readable on a dark theme.",
        "XTerm*highlightColorMode: true",
    ]

    lines += [
        "",
        "! --- keys -------------------------------------------------------------",
        "! Alt sends ESC before the character, which is what readline, Emacs and",
        "! every modern shell expect from Alt.",
        "XTerm*metaSendsEscape: true",
        "! Do not fold Alt and the eighth bit together: that is what puts a",
        "! stray escape character in the middle of a word.",
        "XTerm*eightBitInput: false",
        "! Backspace sends DEL (127), the character modern terminals and every",
        "! default shell configuration expect.",
        "XTerm*backarrowKey: false",
    ]

    lines += [
        "",
        "! --- scrolling --------------------------------------------------------",
        "! The wheel sends cursor up and down when a full screen program is",
        "! running, so it scrolls inside less, vim and htop instead of doing",
        "! nothing; the program is what decides whether that scrolls.",
        "XTerm*alternateScroll: true",
        "! New output does not pull the view back to the bottom while the",
        "! scrollback is being read, which is the difference between reading a",
        "! log and chasing a log.",
        "XTerm*scrollTtyOutput: false",
    ]

    lines += [
        "",
        "! --- what programs are allowed to ask for -----------------------------",
        "! allowWindowOps covers the escapes a terminal uses to put text on the",
        "! clipboard (OSC 52) and to report where it is. It is off by default",
        "! in xterm because a script can use it; it is on here because that is",
        "! what makes copying from a remote shell and from tmux work at all.",
        "! Set it to false if the terminal only ever runs trusted programs -",
        "! the clipboard escapes stop working, and everything else stays.",
        "XTerm*allowWindowOps: true",
        "! Let programs recolour the terminal, which is what a light and dark",
        "! theme switcher, bat and delta all use.",
        "XTerm*dynamicColors: true",
        "! Sixel images scroll with the text instead of being pinned.",
        "XTerm*sixelScrolling: true",
    ]

    lines += [
        "",
        "! --- colours -----------------------------------------------------------",
        "! The GnuchanPurple palette, the same one the GTK theme and VSCodium",
        "! use. See dotfile/vscodium_theme/settings.json for the source values.",
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
        "! --- key bindings ------------------------------------------------------",
        "! #override adds these to the default bindings rather than replacing",
        "! them, so the wheel, the menus and the fullscreen key keep working.",
        "! Ctrl+Shift+C and Ctrl+Shift+V are the pair every other terminal uses;",
        "! Ctrl+plus, Ctrl+minus and Ctrl+0 are the font size, as elsewhere.",
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
        "! The xterm half of the GnuchanPurple desktop. Everything between this",
        "! line and the closing one is written by the script and replaced on",
        "! every run; edit the script, not this block.",
        "!",
        "! Load it with:  xrdb -merge ~/.Xresources",
        "!",
        "! xrdb runs this file through the C preprocessor, which reads an",
        "! apostrophe as the start of a character constant and warns about the",
        "! line. There are none in the comments below for that reason, and there",
        "! should be none in anything added to them.",
    ]
    return "\n".join(header + _resource_lines() + [BLOCK_END])


def environment_snippet() -> str:
    """The shell fragment that sets what a terminal cannot set for itself."""
    return "\n".join(
        [
            SH_BEGIN,
            "# Environment for the GnuchanPurple terminal.",
            "# Sourced from ~/.profile; remove that line to stop using it.",
            "",
            "# xterm renders direct colour but does not announce it, and COLORTERM",
            "# is how a program learns it may use 24 bit colour. It is exported",
            "# only when TERM says xterm, so a session running several terminals",
            "# never claims direct colour in one that cannot do it.",
            'case "${TERM:-}" in',
            f"  xterm*) export COLORTERM={COLORTERM_VALUE} ;;",
            "esac",
            "",
            "# A UTF-8 terminal in a non-UTF-8 locale prints question marks where",
            "# the text should be. The locale is replaced only when there is none",
            "# or when it is a C locale, never when the user has chosen one.",
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
            f'if [ -f "{env_file()}" ]; then',
            f'    . "{env_file()}"',
            "fi",
            SH_END,
        ]
    )


def session_snippet() -> str:
    """The lines that load the resource database when the session starts."""
    return "\n".join(
        [
            SH_BEGIN,
            "# Load the X resource database at login. Without this the settings in",
            "# ~/.Xresources only apply after something runs xrdb by hand.",
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


def _is_root() -> bool:
    """Whether this process can install packages without help.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    do; reading it through ``getattr`` keeps the module importable there so the
    helpers above can be exercised and the file can be linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def install_resources(log: Log) -> None:
    """Merge the xterm block into ``~/.Xresources``."""
    write_merged_file(log, xresources_file(), BLOCK_BEGIN, BLOCK_END, xterm_block())


def install_environment(log: Log) -> None:
    """Write the shell fragment and make ``~/.profile`` source it."""
    write_managed_file(log, env_file(), environment_snippet())
    write_merged_file(log, profile_file(), SH_BEGIN, SH_END, profile_snippet())


def install_session_hooks(log: Log) -> None:
    """Make the session load the resource database when it starts.

    Both hooks are written because they belong to different setups and neither
    is read by the other: Debian's X session sources ``~/.xsessionrc``, while
    the display managers people use on Arch - and most other distributions -
    source ``~/.xprofile``. Writing one and not the other is a configuration
    that works on the machine it was written on.
    """
    for path in (xsessionrc_file(), xprofile_file()):
        write_merged_file(log, path, SH_BEGIN, SH_END, session_snippet())


def install_launcher(log: Log) -> None:
    """Add the application menu entry."""
    write_managed_file(log, launcher_file(), launcher_text())


def load_resources(log: Log) -> bool:
    """Merge ``~/.Xresources`` into the running X server, returning success.

    This is what makes the settings take effect now rather than at the next
    login, and it is also the step that fails quietly when there is no X server
    to talk to - a run over ssh, or from a console - which is why it reports
    what it did rather than assuming.
    """
    xrdb = shutil.which("xrdb")
    if xrdb is None:
        log.detail("xrdb is not installed; the session hook loads the file at login")
        return False
    if not os.environ.get("DISPLAY"):
        log.detail("DISPLAY is not set; the settings apply to the next X session")
        return False
    result = subprocess.run(
        [xrdb, "-merge", str(xresources_file())], check=False
    )
    if result.returncode != 0:
        log.detail("xrdb could not load the resources")
        return False
    log.detail("loaded into the running X server with xrdb -merge")
    return True


# --- checking ----------------------------------------------------------------


def terminfo_available(name: str = "xterm-256color") -> bool:
    """Whether the terminfo entry the termName resource names exists."""
    infocmp = shutil.which("infocmp")
    if infocmp is None:
        # Without infocmp there is no way to ask, and no reason to complain.
        return True
    return subprocess.run(
        [infocmp, name], check=False, capture_output=True
    ).returncode == 0


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


def check_environment(log: Log) -> int:
    """Report what would stop xterm looking the way this script intends.

    Each check covers a failure that is invisible from the outside: a font that
    does not resolve makes xterm draw with its built-in bitmap font, a missing
    terminfo entry makes every program fall back to eight colours, a non-UTF-8
    locale prints question marks, resources that were never loaded leave the
    file on disk doing nothing, and an xterm that is not installed at all is
    the one case where none of the rest matters.
    """
    problems: list[str] = []

    if shutil.which("xterm") is None:
        problems.append("xterm is not installed")

    for family in FONT_FAMILIES:
        if not font_available(family):
            problems.append(f"the font {family} does not resolve ({manual_font_hint()})")

    if not terminfo_available():
        problems.append(
            "the terminfo entry xterm-256color is missing; install ncurses-term "
            "or ncurses, or the terminal will be limited to 8 colours"
        )

    if not locale_is_utf8():
        problems.append(
            "the locale is not UTF-8; non-ASCII text will not be displayed "
            "correctly (see the xterm-env.sh this script installs)"
        )

    text = read_text(xresources_file())
    if BLOCK_BEGIN not in text:
        problems.append(f"{xresources_file()} has no xterm block; run the script")
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

    Files it owns are deleted, files it shares are edited back - the block is
    taken out of ``~/.Xresources``, ``~/.profile`` and the session hooks, which
    leaves whatever the user had in them. A ``.gnuchan-backup`` copy is put
    back where there is one, because that is the file the user had before this
    script ever ran.
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
            # The whole file was this script's, and there is an untouched copy
            # of what was there before it: put that back rather than leaving an
            # empty file behind.
            path.unlink()
            shutil.move(str(backup), str(path))
            log.detail(f"restored {path} from its backup")
            continue
        if merged.strip():
            write_text(path, merged)
            log.detail(f"removed our block from {path}")
        else:
            path.unlink()
            log.detail(f"removed {path}")

    for path in (env_file(), launcher_file()):
        if path.exists():
            path.unlink()
            log.detail(f"removed {path}")
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if backup.exists() and MARKER in read_text(backup):
            # Only a backup that holds our own file can be here: it means an
            # earlier run saved its output before this script learned to
            # recognise it. Removing it is what makes the uninstall complete.
            backup.unlink()
            log.detail(f"removed {backup}")

    config_dir = env_file().parent
    if config_dir.is_dir() and not any(config_dir.iterdir()):
        config_dir.rmdir()
        log.detail(f"removed the empty {config_dir}")

    log.detail("the fonts were left installed; remove them with your package manager")


# --- entry point -------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=SCRIPT_NAME,
        description=(
            "Configure xterm as a modern terminal: 256 colours and true colour, "
            "the GNU fonts, a long scrollback, clipboard shortcuts and UTF-8."
        ),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="report what would stop xterm looking right, then stop",
    )
    parser.add_argument(
        "--uninstall",
        action="store_true",
        help="remove everything this script wrote, then stop",
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
        log.note("Removed. Open a new xterm, or reload the resources with:")
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
    log.note("xterm now uses the GNU fonts, 256 colours and the palette the")
    log.note("rest of the desktop uses. Open a new xterm to see it.")
    log.note("")
    log.note("  Ctrl+Shift+C / Ctrl+Shift+V   copy and paste")
    log.note("  Ctrl+plus / Ctrl+minus / Ctrl+0   font size")
    log.note("  Ctrl+Shift+N                  new window")
    log.note("  Ctrl+click                    the menu")
    log.note("")
    log.note(f"Settings live in {xresources_file()}; remove the block this")
    log.note(f"script wrote there, or run {SCRIPT_NAME} --uninstall.")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
