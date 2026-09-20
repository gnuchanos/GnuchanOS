#!/usr/bin/env python3
# =============================================================================
# GnuchanOS - GRUB 2 boot theme installer
# -----------------------------------------------------------------------------
# Installs a purple GRUB theme and makes the machine boot with it. Standard
# library only, and no options: running
#
#     python3 settings_grub.py
#
# is the whole install.
#
# What it does
# ------------
#   1. resolves the images the theme is built from - the wallpaper from
#      assets/bg.png, which is the purple one, and the logo from
#      assets/logo.png - before anything is written, because a theme whose
#      wallpaper is missing is not the theme that was asked for;
#   2. renders the theme: the wallpaper, the logo on a transparent square, the
#      nine slice images of the menu box, the selection box, the scrollbar thumb
#      and the terminal box, and one icon per entry class (debian, gnu-linux,
#      gnu, os, windows, efi, memtest86, recovery). Every one of them is written
#      by the PNG encoder and the few drawing primitives in this file: Pillow is
#      not installed on a fresh system and is not a dependency worth taking for
#      images that are rectangles, lines and discs;
#   3. finds the font. GRUB looks a font up by the name written *inside* the
#      .pf2 file and not by its file name, so the name cannot be guessed here:
#      the fonts the machine has are searched (the unicode.pf2 that grub-common
#      installs, then anything else in the usual directories), the one that is
#      found is copied into the theme so GRUB loads it from the theme itself,
#      its name is read out of it, and the name is substituted into theme.txt in
#      place of @FONT@. A machine with no .pf2 anywhere still gets a working
#      theme: the well known name of Debian's font is written instead, and GRUB
#      answers a name it does not have with the first font it has loaded, and
#      with its built-in glyphs when it has none;
#   4. installs the theme in /usr/share/grub/themes/GnuchanOS, where a packager
#      would put it and where GRUB_THEME is documented to point, and in
#      /boot/grub/themes/GnuchanOS, which is the copy GRUB can actually read.
#      That second copy is not redundant: GRUB reads grub.cfg from the
#      filesystem it was started from, so on a machine whose /boot is a
#      partition of its own a theme under /usr is not reachable at all - and a
#      theme that cannot be read is a theme that is not drawn, however well it
#      is installed. Debian's own 05_debian_theme copies themes into /boot for
#      exactly this reason;
#   5. merges the settings this theme owns into /etc/default/grub - GRUB_THEME
#      pointing at the copy in /boot, a graphics mode that matches the
#      wallpaper, and the keys that would otherwise defeat the theme: a console
#      terminal, which switches gfxterm off and with it every graphical theme,
#      and a background image of the distribution's own, which the theme's
#      desktop-image replaces. Every comment, blank line and unrelated key of
#      the file survives, and the file as it was is kept beside it;
#   6. regenerates grub.cfg with update-grub, or with grub-mkconfig on a machine
#      that has no such wrapper, and then checks what was written: every file
#      the theme names is looked for, every generated image is decoded again,
#      the font named in theme.txt is compared with the name inside the font
#      that was shipped, /etc/default/grub is read back, and grub.cfg is
#      searched for the loadfont and set theme lines that make any of it take
#      effect - because a theme that is installed and never loaded looks exactly
#      like one that was never installed, and that is what this last step exists
#      to tell apart.
#
# Nothing here reboots the machine and nothing here runs grub-install: the boot
# loader is already where it is, and this only changes what it draws. The theme
# is visible from the next boot, which the script says in its last line.
#
# The wallpaper is 1920x1080 and GRUB crops it to whatever mode the screen is,
# so it fills any resolution rather than being stretched. The mode itself is
# asked for with the panel's own first, then 1920x1080, then "auto": a mode the
# panel cannot show whole is a mode drawn past the edges of the screen, and the
# bottom of the layout and the edges of the wallpaper are what goes missing.
#
# Undo
# ----
# There are no flags, so undoing is by hand and always possible:
#
#     rm -rf /usr/share/grub/themes/GnuchanOS
#     rm -rf /boot/grub/themes/GnuchanOS
#     cp /etc/default/grub.gnuchan-backup /etc/default/grub
#     update-grub
#
# The third line is the one that matters: GRUB_THEME is what makes the theme
# load, and putting the configuration back and regenerating grub.cfg is what
# takes it away again.
#
# The palette is the one the GTK theme, the icon theme, the cursor theme and
# VSCodium use. dotfile/vscodium_theme/settings.json is the source of truth for
# it; change it there first, then the table below and theme.txt next to this
# script.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path
from typing import Callable

RGBA = tuple[int, int, int, int]

SCRIPT_PATH = Path(__file__).resolve()
SCRIPT_DIR = SCRIPT_PATH.parent

#: The theme name, which is the directory name in both places it is installed
#: and the name this script prints.
THEME_NAME = "GnuchanOS"

#: The theme as it is committed next to this script: theme.txt with @FONT@ in
#: it, and nothing else - every image is generated.
SOURCE_THEME_DIR = SCRIPT_DIR / THEME_NAME
THEME_FILE = "theme.txt"

#: Replaced in theme.txt with the name of the font found on this machine.
FONT_PLACEHOLDER = "@FONT@"

#: The name written when the machine has no .pf2 font to read a name from. It is
#: the name of the font Debian's grub-common installs as
#: /usr/share/grub/unicode.pf2, so it is the name a Debian machine has loaded
#: whether or not this script found the file - grub.cfg loads that font itself.
FALLBACK_FONT_NAME = "Unifont Regular 16"

#: Where a theme is installed. The first is the documented location and what a
#: package would ship; the second is the one GRUB reads, and is the theme
#: directory of whichever of /boot/grub and /boot/grub2 this machine has.
GRUB_SHARE_DIR = Path("/usr/share/grub")
GRUB_SHARE_THEMES = GRUB_SHARE_DIR / "themes"

#: The boot directory candidates, in the order worth trying. Debian and every
#: distribution derived from it uses /boot/grub; the RPM family uses /boot/grub2.
GRUB_BOOT_DIRS = (Path("/boot/grub"), Path("/boot/grub2"))

#: The theme images this script generates, by the names the theme refers to.
BACKGROUND_FILE = "background.png"
LOGO_FILE = "logo.png"
TERMINAL_BOX_STYLE = "terminal_box_*.png"
MENU_BOX_STYLE = "menu_*.png"
SELECT_BOX_STYLE = "select_*.png"
SCROLLBAR_THUMB_STYLE = "scrollbar_thumb_*.png"

#: The names in a GRUB box style, which is what the '*' in the styles above is
#: replaced by: the four corners, the four sides and the centre. They are fixed
#: by grub-core/gfxmenu/widget-box.c and cannot be renamed.
BOX_NAMES = ("nw", "n", "ne", "w", "c", "e", "sw", "s", "se")

#: Everything copied from the theme source directory into the installed theme.
THEME_TEXT_FILES = (THEME_FILE,)

#: The names those assets have in the repository. The wallpaper is the purple
#: one; the logo is only drawn when it is there.
BACKGROUND_ASSET = "bg.png"
LOGO_ASSET = "logo.png"

#: The sizes the generated images are drawn at. The wallpaper is used at its own
#: size, the logo is fitted into the square the theme draws it in, the icons are
#: drawn larger than the 28 pixels the menu asks for so they stay sharp on a
#: scaled screen, and the box slices are the corner size of the boxes.
LOGO_SIZE = 132
ICON_SIZE = 64
SLICE_SIZE = 12
SLICE_RADIUS = 9
SLICE_BORDER = 1

#: The same two numbers for the box drawn around one menu entry and around a
#: scrollbar thumb. A box style is not only its corners: GRUB insets the box by
#: the size of the slices it is built from, so the twelve pixel slices of a
#: panel make the box around a 34 pixel entry 58 pixels tall while the entries
#: sit 44 pixels apart, and the highlight climbs over the entry above and the
#: one below it. These are small enough to stay inside the entry they are drawn
#: around.
ITEM_SLICE_SIZE = 5
ITEM_SLICE_RADIUS = 3

#: How much of the wallpaper a panel lets through, as the alpha of the fill.
BOX_ALPHA = 190
SELECT_ALPHA = 205

#: The icons that are written into icons/, one per menu entry class. GRUB tries
#: the classes of an entry in order and uses the first icon file it finds, so
#: os.png is the one every entry without a class of its own falls back to, and a
#: name that is not in this list is simply not drawn.
ICON_NAMES = (
    "debian",
    "gnu-linux",
    "gnu",
    "os",
    "windows",
    "efi",
    "memtest86",
    "recovery",
)

#: The palette, from dotfile/vscodium_theme/settings.json. The wallpaper's own
#: background colour is measured from assets/bg.png and is the one the panels
#: are made of, so a panel looks like a lighter part of the wallpaper rather than
#: a rectangle laid on top of it.
_PALETTE: dict[str, str] = {
    "wallpaper": "#1c0532",
    "bg_darkest": "#09030d",
    "surface_alt": "#180a22",
    "raised": "#1c0b27",
    "border": "#32143f",
    "border_strong": "#6d28d9",
    "accent": "#a855f7",
    "accent_active": "#9333ea",
    "accent_light": "#c77dff",
    "accent_pale": "#d8a4ff",
    "selected_bg": "#542080",
    "fg": "#ead7ff",
    "fg_bright": "#f3e8ff",
    "fg_dim": "#b78ed6",
    "white": "#ffffff",
}


def hex_to_rgba(value: str, alpha: int = 255) -> RGBA:
    """Parse ``#rgb`` or ``#rrggbb`` into an RGBA tuple."""
    text = value.strip().lstrip("#")
    if len(text) == 3:
        text = "".join(character * 2 for character in text)
    if len(text) != 6:
        raise ValueError(f"not a colour: {value!r}")
    return (
        int(text[0:2], 16),
        int(text[2:4], 16),
        int(text[4:6], 16),
        alpha,
    )


PALETTE: dict[str, RGBA] = {
    name: hex_to_rgba(value) for name, value in _PALETTE.items()
}
#: The environment variable that stops a sudo which fails to change the user
#: from re-running the script for ever.
ELEVATED_VARIABLE = "GNUGHAN_GRUB_ELEVATED"

#: The suffix every file this script rewrites is kept under, once.
BACKUP_SUFFIX = ".gnuchan-backup"


# --- logging ------------------------------------------------------------------


class Log:
    """Progress output. There is no quiet mode: the script has nothing to
    configure, so every run prints the same steps."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- running things -----------------------------------------------------------


def run(
    command: list[str],
    capture: bool = False,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run a command, with its output kept only when it is asked for.

    Nothing goes through a shell: every command is a list, so a path with a
    space in it cannot turn into two arguments. Keeping the output is for the
    short commands whose text is only wanted when they fail - update-grub is not
    one of them: its output is the only sign of what it did, and swallowing it
    turns a slow run into a run that looks hung.
    """
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True, env=environment
        )
    return subprocess.run(command, check=False, text=True, env=environment)


def is_root() -> bool:
    """Whether this process can write /etc and /boot.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    install; reading it through ``getattr`` keeps the module importable there so
    the image code can be exercised and the file linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """Be root, re-running the script through sudo when possible.

    Everything this installer writes - /boot, /etc/default/grub, grub.cfg - is
    root's, so there is no useful partial run as a normal user. sudo is used
    when it exists and the fact is announced rather than done silently; the
    environment variable stops a sudo that fails to change the user from
    looping.
    """
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit(
            "error: still not root after sudo; run the script as root "
            "(su -c 'python3 settings_grub.py')"
        )
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: installing a boot theme writes to /boot and /etc; "
            "run this script as root"
        )
    log.step("This install is system wide; re-running it through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(SCRIPT_PATH), *sys.argv[1:]], environment)


# --- where this machine keeps GRUB --------------------------------------------


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


def distro_description() -> str:
    """What this distribution calls itself, for the first line of output."""
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def grub_boot_dir() -> Path | None:
    """The GRUB directory this machine boots from, or None.

    /boot/grub is Debian's and everyone derived from it; /boot/grub2 is the RPM
    family's. The one holding a grub.cfg is the one in use, and that is what is
    returned; a directory without one is still better than nothing, because it
    is where the next ``update-grub`` will write.
    """
    for directory in GRUB_BOOT_DIRS:
        if (directory / "grub.cfg").is_file():
            return directory
    for directory in GRUB_BOOT_DIRS:
        if directory.is_dir():
            return directory
    return None


def grub_config_file(boot_dir: Path) -> Path:
    """The configuration GRUB reads the theme and the entries from."""
    return boot_dir / "grub.cfg"


def grub_mkconfig_command(boot_dir: Path) -> list[str] | None:
    """The command that regenerates grub.cfg here, or None.

    ``update-grub`` is Debian's wrapper and does the same thing with the right
    output path, so it is preferred where it exists; grub-mkconfig is told which
    file to write, which is what makes this work on a distribution that has no
    wrapper at all.
    """
    update_grub = shutil.which("update-grub")
    if update_grub is not None:
        return [update_grub]
    mkconfig = shutil.which("grub-mkconfig") or shutil.which("grub2-mkconfig")
    if mkconfig is None:
        return None
    return [mkconfig, "-o", str(grub_config_file(boot_dir))]


def grub_present() -> bool:
    """Whether GRUB is installed here.

    The regeneration command is what matters: a theme can be written without it,
    but nothing would load it, and a machine whose boot loader is something else
    entirely - systemd-boot, rEFInd, LILO - has no GRUB to theme.
    """
    return shutil.which("update-grub") is not None or shutil.which(
        "grub-mkconfig"
    ) is not None or shutil.which("grub2-mkconfig") is not None


# --- the assets the theme is built from ---------------------------------------


def assets_dirs() -> list[Path]:
    """Every place the repository's images may be, most likely first.

    The script lives in dotfile/GRUB_THEME, so the assets are two directories up
    from it; the rest are where a packager would put them on an installed
    system, which is what makes this usable when the repository is not on the
    machine.
    """
    found: list[Path] = []
    for candidate in (SCRIPT_DIR, *SCRIPT_DIR.parents):
        assets = candidate / "assets"
        if assets not in found:
            found.append(assets)
    for assets in (
        Path("/usr/share/gnuchanos/assets"),
        Path("/usr/local/share/gnuchanos/assets"),
        Path("/usr/share/gnuchanos"),
    ):
        if assets not in found:
            found.append(assets)
    return found


def find_asset(name: str) -> Path | None:
    """The first copy of an asset, or None."""
    for directory in assets_dirs():
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def require_background() -> Path:
    """The wallpaper, or a message saying where it was looked for.

    It is resolved before anything is written: a theme with no wallpaper is not
    the theme that was asked for, and finding that out after /etc/default/grub
    has been rewritten would leave the machine in a worse state than the one it
    was found in.
    """
    background = find_asset(BACKGROUND_ASSET)
    if background is not None:
        return background
    looked = "\n".join(f"  {directory / BACKGROUND_ASSET}" for directory in assets_dirs())
    raise SystemExit(
        "error: the wallpaper this theme is built from was not found:\n" + looked
    )


def optional_logo() -> Path | None:
    """The logo, when the repository has one.

    It is optional on purpose. The theme draws it in a corner, and a theme
    without it is still a complete theme - so a checkout that carries only the
    wallpaper is installed rather than refused.
    """
    return find_asset(LOGO_ASSET)


# --- file helpers --------------------------------------------------------------


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
    """Copy an existing file aside, returning where the copy went.

    The copy is taken once and kept: a second run must not overwrite it with
    this script's own output, because the file worth keeping is the one the
    machine had before the script ever touched it. It is copied rather than
    moved for the same reason - the caller merges into the file afterwards, and
    a moved file would be merged into nothing.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.copy2(path, backup)
    return backup


def copy_file(log: Log, source: Path, target: Path) -> None:
    """Copy one file, creating its directory, and make it world readable.

    GRUB reads these files before any user exists and has no permission model of
    its own to speak of, but the theme is also read by tools running as a normal
    user - the configuration check in this script, for one - so the modes are
    set explicitly rather than left to whatever umask this run inherited.
    """
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    target.chmod(0o644)
    log.detail(f"wrote {target}")


def copy_tree(log: Log, source: Path, target: Path) -> None:
    """Copy a directory tree, with the modes the theme needs."""
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        destination = target / relative
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        destination.chmod(0o644)
    for path in sorted(target.rglob("*")):
        if path.is_dir():
            path.chmod(0o755)
    log.detail(f"copied the theme into {target}")
# --- PNG images ----------------------------------------------------------------
# Every file this theme needs except the wallpaper is generated here, and Pillow
# is not installed on a fresh system and is not a dependency worth taking for
# images that are rectangles, lines and discs. So the code below reads PNG well
# enough for the repository's own assets - 1, 2, 4, 8 and 16 bit grey, grey with
# alpha, RGB, RGBA and palette images, interlaced or not - and writes 8 bit RGBA
# images, which is what GRUB's png module reads. An image that cannot be read is
# reported rather than guessed at: the wallpaper has to be a PNG for GRUB to
# draw it, and a theme with a wallpaper that failed silently is a theme that
# comes up black.


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

#: Channels per pixel, by the colour type in the header: 0 grey, 2 RGB,
#: 3 palette, 4 grey with alpha, 6 RGBA.
PNG_CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}

#: Adam7: the seven passes of an interlaced image, as
#: (x_start, y_start, x_step, y_step).
ADAM7_PASSES = (
    (0, 0, 8, 8),
    (4, 0, 8, 8),
    (0, 4, 4, 8),
    (2, 0, 4, 4),
    (0, 2, 2, 4),
    (1, 0, 2, 2),
    (0, 1, 1, 2),
)


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    """One PNG chunk: length, tag, body, CRC over the tag and the body."""
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def _samples(row: bytearray, count: int, depth: int) -> list[int] | None:
    """The first ``count`` samples of a scanline, widened to 0-255.

    A sample of one, two or four bits is scaled by the largest value it can
    hold, so a one bit image is black and white rather than dark grey and light
    grey; a sixteen bit one keeps its high byte, which is what every display
    does with it anyway.
    """
    if depth == 8:
        if len(row) < count:
            return None
        return list(row[:count])
    if depth == 16:
        if len(row) < count * 2:
            return None
        return [row[index * 2] for index in range(count)]
    per_byte = 8 // depth
    mask = (1 << depth) - 1
    scale = 255 // mask
    if len(row) * per_byte < count:
        return None
    values: list[int] = []
    for index in range(count):
        byte = row[index // per_byte]
        shift = 8 - depth * (index % per_byte + 1)
        values.append(((byte >> shift) & mask) * scale)
    return values


def _unfilter(
    raw: bytes, start: int, width: int, height: int, bits_per_pixel: int
) -> tuple[list[bytearray] | None, int]:
    """Undo the per scanline filters of one image or one Adam7 pass.

    Returns the scanlines and the offset the next pass starts at, or None when
    the data runs out or names a filter that does not exist.
    """
    stride = (width * bits_per_pixel + 7) // 8
    # The filters look back by one pixel, which is one byte for everything under
    # eight bits per pixel.
    step = max(1, bits_per_pixel // 8)
    rows: list[bytearray] = []
    previous = bytearray(stride)
    offset = start
    for _ in range(height):
        if offset + 1 + stride > len(raw):
            return None, start
        filter_type = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        if filter_type == 0:
            pass
        elif filter_type == 1:
            for index in range(step, stride):
                row[index] = (row[index] + row[index - step]) & 0xFF
        elif filter_type == 2:
            for index in range(stride):
                row[index] = (row[index] + previous[index]) & 0xFF
        elif filter_type == 3:
            for index in range(stride):
                left = row[index - step] if index >= step else 0
                row[index] = (row[index] + ((left + previous[index]) >> 1)) & 0xFF
        elif filter_type == 4:
            for index in range(stride):
                left = row[index - step] if index >= step else 0
                above = previous[index]
                corner = previous[index - step] if index >= step else 0
                estimate = left + above - corner
                distance_left = abs(estimate - left)
                distance_above = abs(estimate - above)
                distance_corner = abs(estimate - corner)
                if distance_left <= distance_above and distance_left <= distance_corner:
                    predictor = left
                elif distance_above <= distance_corner:
                    predictor = above
                else:
                    predictor = corner
                row[index] = (row[index] + predictor) & 0xFF
        else:
            return None, start
        rows.append(row)
        previous = row
    return rows, offset


def _row_to_rgba(
    row: bytearray,
    width: int,
    depth: int,
    colour_type: int,
    palette: bytes,
    transparency: bytes,
) -> bytearray | None:
    """One scanline as RGBA bytes."""
    channels = PNG_CHANNELS[colour_type]
    samples = _samples(row, width * channels, depth)
    if samples is None:
        return None

    # Colour key transparency: one value of a greyscale or RGB image that is
    # meant to be see through, which is how logos older than the alpha channel
    # carry their transparency.
    key: tuple[int, ...] | None = None
    if transparency and colour_type == 0 and len(transparency) >= 2:
        key = (transparency[0],)
    elif transparency and colour_type == 2 and len(transparency) >= 6:
        key = (transparency[1], transparency[3], transparency[5])

    rgba = bytearray(width * 4)
    for index in range(width):
        base = index * channels
        alpha = 255
        if colour_type == 0:
            red = green = blue = samples[base]
            if key is not None and samples[base] == key[0]:
                alpha = 0
        elif colour_type == 4:
            red = green = blue = samples[base]
            alpha = samples[base + 1]
        elif colour_type == 2:
            red, green, blue = samples[base], samples[base + 1], samples[base + 2]
            if key is not None and (red, green, blue) == key:
                alpha = 0
        elif colour_type == 6:
            red, green, blue = samples[base], samples[base + 1], samples[base + 2]
            alpha = samples[base + 3]
        else:
            entry = samples[base] * 3
            if entry + 3 > len(palette):
                return None
            red, green, blue = palette[entry], palette[entry + 1], palette[entry + 2]
            if samples[base] < len(transparency):
                alpha = transparency[samples[base]]
        target = index * 4
        rgba[target] = red
        rgba[target + 1] = green
        rgba[target + 2] = blue
        rgba[target + 3] = alpha
    return rgba


def _inside_rounded(
    px: float, py: float, x: float, y: float, width: float, height: float, radius: float
) -> bool:
    """Whether a point is inside a rounded rectangle."""
    if px < x or py < y or px > x + width or py > y + height:
        return False
    corner = min(radius, min(width, height) / 2.0)
    if corner <= 0.0:
        return True
    left = x + corner
    right = x + width - corner
    top = y + corner
    bottom = y + height - corner
    if left <= px <= right or top <= py <= bottom:
        return True
    centre_x = left if px < left else right
    centre_y = top if py < top else bottom
    return (px - centre_x) ** 2 + (py - centre_y) ** 2 <= corner * corner


def _segment_distance(
    px: float, py: float, ax: float, ay: float, bx: float, by: float
) -> float:
    """Distance from a point to a line segment."""
    dx = bx - ax
    dy = by - ay
    length_squared = dx * dx + dy * dy
    if length_squared <= 0.0:
        return math.hypot(px - ax, py - ay)
    t = ((px - ax) * dx + (py - ay) * dy) / length_squared
    t = min(1.0, max(0.0, t))
    return math.hypot(px - (ax + dx * t), py - (ay + dy * t))


#: Sub-samples per axis when a shape is drawn. Three is enough to make a curve
#: look round at the sizes used here without turning the icons into a slow job.
SAMPLES = 3


class Raster:
    """An RGBA image in memory, the few shapes this theme is drawn from, and
    the PNG reader and writer that move it to and from disk."""

    __slots__ = ("width", "height", "pixels")

    def __init__(self, width: int, height: int, pixels: bytearray | None = None) -> None:
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 4) if pixels is None else pixels

    @classmethod
    def solid(cls, width: int, height: int, colour: RGBA = (0, 0, 0, 0)) -> "Raster":
        """A rectangle of one colour."""
        return cls(width, height, bytearray(bytes(colour) * (width * height)))

    # --- drawing ----------------------------------------------------------

    def blend(self, x: int, y: int, colour: RGBA, coverage: float) -> None:
        """Put one pixel of ``colour`` over the image, in source-over order.

        The colours are composited with the alpha of what is already there, so
        two translucent shapes over one another do not multiply their colours
        together the way a plain assignment would.
        """
        if coverage <= 0.0 or not (0 <= x < self.width and 0 <= y < self.height):
            return
        source_alpha = (colour[3] / 255.0) * min(1.0, coverage)
        if source_alpha <= 0.0:
            return
        offset = (y * self.width + x) * 4
        pixels = self.pixels
        destination_alpha = pixels[offset + 3] / 255.0
        out_alpha = source_alpha + destination_alpha * (1.0 - source_alpha)
        if out_alpha <= 0.0:
            pixels[offset:offset + 4] = b"\x00\x00\x00\x00"
            return
        keep = destination_alpha * (1.0 - source_alpha)
        pixels[offset] = round(
            (colour[0] * source_alpha + pixels[offset] * keep) / out_alpha
        )
        pixels[offset + 1] = round(
            (colour[1] * source_alpha + pixels[offset + 1] * keep) / out_alpha
        )
        pixels[offset + 2] = round(
            (colour[2] * source_alpha + pixels[offset + 2] * keep) / out_alpha
        )
        pixels[offset + 3] = round(out_alpha * 255.0)

    def rounded_rect(
        self,
        x: float,
        y: float,
        width: float,
        height: float,
        colour: RGBA,
        radius: float = 0.0,
        filled: bool = True,
        thickness: float = 1.0,
    ) -> None:
        """A filled or stroked rounded rectangle, antialiased by supersampling.

        A stroke sits just inside the edge; the two shapes a box style is made
        of - the panel and its hairline border - are this function twice.
        """
        inset = 0.0 if filled else thickness / 2.0
        inner_x = x + thickness
        inner_y = y + thickness
        inner_width = width - 2 * thickness
        inner_height = height - 2 * thickness
        inner_radius = max(0.0, radius - thickness)
        step = 1.0 / SAMPLES
        for row in range(int(math.floor(y)), int(math.ceil(y + height))):
            for column in range(int(math.floor(x)), int(math.ceil(x + width))):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if not _inside_rounded(px, py, x, y, width, height, radius):
                            continue
                        if filled:
                            hits += 1
                            continue
                        if inset > 0.0 and inner_width > 0.0 and inner_height > 0.0:
                            if _inside_rounded(
                                px, py, inner_x, inner_y, inner_width, inner_height, inner_radius
                            ):
                                continue
                        hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def line(
        self,
        x0: float,
        y0: float,
        x1: float,
        y1: float,
        colour: RGBA,
        thickness: float = 1.0,
    ) -> None:
        """A line segment of the given width, antialiased."""
        half = thickness / 2.0
        left = int(math.floor(min(x0, x1) - half - 1))
        right = int(math.ceil(max(x0, x1) + half + 1))
        top = int(math.floor(min(y0, y1) - half - 1))
        bottom = int(math.ceil(max(y0, y1) + half + 1))
        step = 1.0 / SAMPLES
        for row in range(top, bottom + 1):
            for column in range(left, right + 1):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if _segment_distance(px, py, x0, y0, x1, y1) <= half:
                            hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def disc(self, centre_x: float, centre_y: float, radius: float, colour: RGBA) -> None:
        """A filled circle, antialiased."""
        self.ring(centre_x, centre_y, radius / 2.0, radius, colour)

    def ring(
        self,
        centre_x: float,
        centre_y: float,
        radius: float,
        thickness: float,
        colour: RGBA,
    ) -> None:
        """A circle outline, drawn as a band of the given width."""
        half = thickness / 2.0
        outer = radius + half
        left = int(math.floor(centre_x - outer - 1))
        right = int(math.ceil(centre_x + outer + 1))
        top = int(math.floor(centre_y - outer - 1))
        bottom = int(math.ceil(centre_y + outer + 1))
        step = 1.0 / SAMPLES
        for row in range(top, bottom + 1):
            for column in range(left, right + 1):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if abs(math.hypot(px - centre_x, py - centre_y) - radius) <= half:
                            hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def arc(
        self,
        centre_x: float,
        centre_y: float,
        radius: float,
        thickness: float,
        colour: RGBA,
        start_degrees: float,
        end_degrees: float,
    ) -> None:
        """Part of a ring, from one angle to another, going clockwise.

        Used for the spiral of the Debian icon and the arrow of the recovery
        one: a whole ring drawn as segments, which is what these shapes are at
        the size they are drawn.
        """
        step = 6.0
        angle = start_degrees
        previous = None
        while angle <= end_degrees:
            radians = math.radians(angle)
            point = (
                centre_x + radius * math.cos(radians),
                centre_y + radius * math.sin(radians),
            )
            if previous is not None:
                self.line(previous[0], previous[1], point[0], point[1], colour, thickness)
            previous = point
            angle += step
        if previous is not None:
            radians = math.radians(end_degrees)
            self.line(
                previous[0],
                previous[1],
                centre_x + radius * math.cos(radians),
                centre_y + radius * math.sin(radians),
                colour,
                thickness,
            )

    # --- scaling and files ------------------------------------------------

    def scaled_to(self, width: int, height: int) -> "Raster":
        """The image at another size, averaging the pixels it shrinks away.

        Averaging is done on colours premultiplied by their alpha and divided
        back out afterwards: mixing the colours of transparent and opaque pixels
        first is what puts a dark fringe around a logo drawn on nothing.
        """
        target = Raster.solid(width, height)
        source = self.pixels
        for target_y in range(height):
            first_y = target_y * self.height // height
            last_y = min(max(first_y + 1, (target_y + 1) * self.height // height), self.height)
            for target_x in range(width):
                first_x = target_x * self.width // width
                last_x = min(max(first_x + 1, (target_x + 1) * self.width // width), self.width)
                red = green = blue = alpha_sum = 0
                count = (last_x - first_x) * (last_y - first_y)
                for source_y in range(first_y, last_y):
                    row = (source_y * self.width + first_x) * 4
                    for column in range(last_x - first_x):
                        offset = row + column * 4
                        alpha = source[offset + 3]
                        red += source[offset] * alpha
                        green += source[offset + 1] * alpha
                        blue += source[offset + 2] * alpha
                        alpha_sum += alpha
                if alpha_sum == 0:
                    continue  # every pixel behind this one was transparent
                offset = (target_y * width + target_x) * 4
                target.pixels[offset] = min(255, red // alpha_sum)
                target.pixels[offset + 1] = min(255, green // alpha_sum)
                target.pixels[offset + 2] = min(255, blue // alpha_sum)
                target.pixels[offset + 3] = min(255, alpha_sum // count)
        return target

    def fitted(self, size: int) -> "Raster":
        """The image scaled to fit a transparent square of ``size``.

        The proportions are kept, so a logo taller than it is wide is not
        squashed into a square, and the square is what the theme draws: the
        component that shows it has a width and a height and no way to keep an
        aspect ratio, so the shape has to be right in the file.
        """
        scale = min(size / self.width, size / self.height)
        target_width = max(1, min(size, round(self.width * scale)))
        target_height = max(1, min(size, round(self.height * scale)))
        scaled = self.scaled_to(target_width, target_height)
        canvas = Raster.solid(size, size)
        left = (size - target_width) // 2
        top = (size - target_height) // 2
        for y in range(target_height):
            source = y * target_width * 4
            target = ((y + top) * size + left) * 4
            canvas.pixels[target:target + target_width * 4] = scaled.pixels[
                source:source + target_width * 4
            ]
        return canvas

    def write(self, path: Path) -> None:
        """Write the image as an 8 bit RGBA PNG.

        Filter type 0 on every scanline: the point of the encoder is to be short
        and obviously correct, and these images are small.
        """
        stride = self.width * 4
        raw = bytearray()
        for row in range(self.height):
            raw.append(0)
            raw += self.pixels[row * stride:(row + 1) * stride]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(
            PNG_SIGNATURE
            + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
            + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _png_chunk(b"IEND", b"")
        )

    def write_readable(self, path: Path) -> None:
        """Write the image and make it world readable, as the theme expects."""
        self.write(path)
        path.chmod(0o644)


def read_png(path: Path) -> Raster | None:
    """Read a PNG, or return None when it is not one this can handle."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return None
    if not data.startswith(PNG_SIGNATURE):
        return None

    header: tuple[int, int, int, int, int, int, int] | None = None
    palette = b""
    transparency = b""
    compressed = bytearray()
    offset = len(PNG_SIGNATURE)
    while offset + 8 <= len(data):
        length, tag = struct.unpack(">I4s", data[offset:offset + 8])
        body = data[offset + 8:offset + 8 + length]
        if len(body) != length:
            return None
        offset += 12 + length
        if tag == b"IHDR":
            if length != 13:
                return None
            header = struct.unpack(">IIBBBBB", body)
        elif tag == b"PLTE":
            palette = body
        elif tag == b"tRNS":
            transparency = body
        elif tag == b"IDAT":
            compressed += body
        elif tag == b"IEND":
            break
    if header is None:
        return None

    width, height, depth, colour_type, compression, filter_method, interlace = header
    if compression != 0 or filter_method != 0 or width == 0 or height == 0:
        return None
    if colour_type not in PNG_CHANNELS or depth not in (1, 2, 4, 8, 16):
        return None
    if depth < 8 and colour_type not in (0, 3):
        return None
    if colour_type == 3 and not palette:
        return None
    try:
        raw = zlib.decompress(bytes(compressed))
    except zlib.error:
        return None

    bits_per_pixel = PNG_CHANNELS[colour_type] * depth
    pixels = bytearray(width * height * 4)
    if interlace == 0:
        rows, _ = _unfilter(raw, 0, width, height, bits_per_pixel)
        if rows is None:
            return None
        for row_index, row in enumerate(rows):
            rgba = _row_to_rgba(row, width, depth, colour_type, palette, transparency)
            if rgba is None:
                return None
            pixels[row_index * width * 4:(row_index + 1) * width * 4] = rgba
    elif interlace == 1:
        position = 0
        for x_start, y_start, x_step, y_step in ADAM7_PASSES:
            pass_width = (width - x_start + x_step - 1) // x_step if width > x_start else 0
            pass_height = (height - y_start + y_step - 1) // y_step if height > y_start else 0
            if pass_width == 0 or pass_height == 0:
                continue
            rows, position = _unfilter(raw, position, pass_width, pass_height, bits_per_pixel)
            if rows is None:
                return None
            for row_index, row in enumerate(rows):
                rgba = _row_to_rgba(row, pass_width, depth, colour_type, palette, transparency)
                if rgba is None:
                    return None
                y = y_start + row_index * y_step
                for column in range(pass_width):
                    x = x_start + column * x_step
                    target = (y * width + x) * 4
                    source = column * 4
                    pixels[target:target + 4] = rgba[source:source + 4]
    else:
        return None
    return Raster(width, height, pixels)
# --- the images the theme is drawn from -----------------------------------------


def crop(image: Raster, x: int, y: int, width: int, height: int) -> Raster:
    """The part of an image inside a rectangle, as a new image."""
    piece = Raster.solid(width, height)
    for row in range(height):
        source = ((y + row) * image.width + x) * 4
        target = row * width * 4
        piece.pixels[target:target + width * 4] = image.pixels[source:source + width * 4]
    return piece


def render_panel(
    width: int, height: int, fill: RGBA, border: RGBA, radius: float
) -> Raster:
    """A rounded panel: the fill, then the hairline border just inside it."""
    panel = Raster.solid(width, height)
    panel.rounded_rect(0.0, 0.0, float(width), float(height), fill, radius=radius)
    panel.rounded_rect(
        0.0,
        0.0,
        float(width),
        float(height),
        border,
        radius=radius,
        filled=False,
        thickness=float(SLICE_BORDER),
    )
    return panel


def box_slices(
    fill: RGBA, border: RGBA, radius: float, size: int = SLICE_SIZE
) -> dict[str, Raster]:
    """The nine images of a GRUB box style, sliced from one drawn panel.

    A GRUB box is drawn by stretching the four sides and the centre and leaving
    the four corners alone, so a corner has to line up with the side next to it
    exactly, and a side has to be the same all the way along or stretching it
    shows a seam. The way to guarantee both is to draw the whole panel once and
    cut it up, with the slices taken from a panel three corners across: the two
    outer slices are the corners with the curve on them, the three middle ones
    are the straight parts, and the straight parts are the ones that stretch.

    Drawing it at twice the corner size instead - which is the obvious thing to
    do - leaves no straight part at all, because the corners meet in the middle,
    and every edge then smears a piece of the curve along its length.
    """
    panel = render_panel(size * 3, size * 3, fill, border, radius)
    step = size
    return {
        "nw": crop(panel, 0, 0, step, step),
        "n": crop(panel, step, 0, step, step),
        "ne": crop(panel, step * 2, 0, step, step),
        "w": crop(panel, 0, step, step, step),
        "c": crop(panel, step, step, step, step),
        "e": crop(panel, step * 2, step, step, step),
        "sw": crop(panel, 0, step * 2, step, step),
        "s": crop(panel, step, step * 2, step, step),
        "se": crop(panel, step * 2, step * 2, step, step),
    }
def render_icon(name: str) -> Raster:
    """One menu icon, drawn from the same few shapes as everything else.

    The shapes are the palette's violet on nothing: GRUB draws them at 28x28 in
    the menu, and they are drawn at 64 so the scaling GRUB does has something to
    work with. A class this does not know is not written at all - GRUB simply
    draws no icon for it, which looks deliberate, where a generic square would
    look unfinished.
    """
    glyph = PALETTE["accent_pale"]
    accent = PALETTE["accent"]
    icon = Raster.solid(ICON_SIZE, ICON_SIZE)
    centre = ICON_SIZE / 2.0

    if name == "debian":
        # The swirl: an arc that curls inwards, and the dot it curls around.
        icon.arc(centre, centre, 18.0, 5.0, accent, 200.0, 470.0)
        icon.arc(centre + 3.0, centre - 3.0, 8.0, 4.5, glyph, 300.0, 560.0)
        icon.disc(centre + 7.0, centre - 7.0, 3.5, glyph)
    elif name in ("gnu-linux", "gnu"):
        # A prompt: the chevron of a shell and the cursor under it.
        icon.line(17.0, 17.0, 29.0, 32.0, accent, 5.0)
        icon.line(29.0, 32.0, 17.0, 47.0, accent, 5.0)
        icon.line(32.0, 45.0, 47.0, 45.0, glyph, 5.0)
    elif name == "os":
        # The theme's own mark, which is also the cursor theme's: a ring with a
        # disc in the middle.
        icon.ring(centre, centre, 16.0, 5.0, accent)
        icon.disc(centre, centre, 6.0, glyph)
    elif name == "windows":
        for x, y in ((9.0, 9.0), (34.0, 9.0), (9.0, 34.0), (34.0, 34.0)):
            icon.rounded_rect(x, y, 21.0, 21.0, accent, radius=3.0)
    elif name == "efi":
        # A chip: a square with a core and pins.
        icon.rounded_rect(13.0, 13.0, 38.0, 38.0, accent, radius=6.0, filled=False, thickness=4.0)
        icon.disc(centre, centre, 7.0, glyph)
        for offset in (centre - 8.0, centre + 8.0):
            icon.line(offset, 6.0, offset, 13.0, accent, 4.0)
            icon.line(offset, 51.0, offset, 58.0, accent, 4.0)
            icon.line(6.0, offset, 13.0, offset, accent, 4.0)
            icon.line(51.0, offset, 58.0, offset, accent, 4.0)
    elif name == "memtest86":
        # A memory module: the board, the chips on it, and the notch.
        icon.rounded_rect(6.0, 20.0, 52.0, 24.0, accent, radius=4.0, filled=False, thickness=4.0)
        for x in (18.0, 28.0, 38.0, 48.0):
            icon.line(x, 26.0, x, 38.0, glyph, 3.0)
        icon.rounded_rect(24.0, 16.0, 16.0, 6.0, accent, radius=2.0)
    elif name == "recovery":
        # A cross, which is what a rescue entry is drawn as everywhere else.
        icon.rounded_rect(26.0, 12.0, 12.0, 40.0, glyph, radius=4.0)
        icon.rounded_rect(12.0, 26.0, 40.0, 12.0, glyph, radius=4.0)
    return icon


def write_box_style(
    log: Log,
    directory: Path,
    prefix: str,
    fill: RGBA,
    border: RGBA,
    radius: float,
    size: int = SLICE_SIZE,
) -> None:
    """Write the nine images of one box style into the theme directory."""
    for name, image in box_slices(fill, border, radius, size).items():
        image.write_readable(directory / f"{prefix}_{name}.png")
    log.detail(f"wrote the {prefix}_*.png box style ({len(BOX_NAMES)} images)")


def write_icons(log: Log, directory: Path) -> None:
    """Write one icon per entry class, under icons/."""
    icons = directory / "icons"
    icons.mkdir(parents=True, exist_ok=True)
    for name in ICON_NAMES:
        render_icon(name).write_readable(icons / f"{name}.png")
    log.detail(f"wrote {len(ICON_NAMES)} entry icons into {icons}")


def write_theme_images(
    log: Log, directory: Path, background: Path, logo: Path | None
) -> None:
    """Write every image the theme is made of into ``directory``.

    The wallpaper is copied byte for byte: it is already 1920x1080 and GRUB
    scales and crops it to whatever mode it ended up in, so decoding and
    re-encoding four megabytes of photograph would cost time and lose nothing.
    Everything else is drawn, and the panel colours are the wallpaper's own
    background colour with the accent violet on the edges, so the boxes read as
    part of the wallpaper rather than as rectangles laid on it.
    """
    copy_file(log, background, directory / BACKGROUND_FILE)

    if logo is not None:
        image = read_png(logo)
        if image is None:
            log.warn(f"{logo} is not a PNG this script can read; the logo is left out")
        else:
            image.fitted(LOGO_SIZE).write_readable(directory / LOGO_FILE)
            log.detail(f"wrote {directory / LOGO_FILE} ({LOGO_SIZE}x{LOGO_SIZE})")
    else:
        log.detail("no logo in this checkout; the theme is installed without one")

    panel_fill = PALETTE["wallpaper"] + (BOX_ALPHA,)
    panel_border = PALETTE["border_strong"] + (150,)
    select_fill = PALETTE["selected_bg"] + (SELECT_ALPHA,)
    select_border = PALETTE["accent"] + (215,)
    terminal_fill = PALETTE["bg_darkest"] + (232,)
    terminal_border = PALETTE["accent"] + (180,)
    thumb_fill = PALETTE["accent"] + (200,)
    thumb_border = PALETTE["accent_light"] + (235,)

    write_box_style(log, directory, "menu", panel_fill, panel_border, float(SLICE_RADIUS))
    write_box_style(
        log,
        directory,
        "select",
        select_fill,
        select_border,
        float(ITEM_SLICE_RADIUS),
        ITEM_SLICE_SIZE,
    )
    write_box_style(
        log, directory, "terminal_box", terminal_fill, terminal_border, float(SLICE_RADIUS)
    )
    write_box_style(
        log,
        directory,
        "scrollbar_thumb",
        thumb_fill,
        thumb_border,
        float(ITEM_SLICE_RADIUS),
        ITEM_SLICE_SIZE,
    )
    write_icons(log, directory)
# --- the font -------------------------------------------------------------------
# GRUB looks a font up by the name written *inside* the .pf2 file and not by the
# file name, and the theme loader only ever *selects* a font that grub.cfg has
# already loaded: title-font and message-font name a font, they do not read one.
# So the name cannot be guessed here. The font is found, its name is read out of
# it, that name replaces @FONT@ in theme.txt, and the file is copied into the
# theme directory - which is exactly where util/grub.d/00_header looks for one,
# because for every *.pf2 in the theme directory, and in its f/ subdirectory, it
# emits a `loadfont` line of its own. A font beside theme.txt is therefore
# loaded with nothing else asked of anyone.
#
# The layout of a .pf2 is in grub-core/font/font.c: a four byte section name and
# a four byte big endian length, repeatedly - FILE first, holding the four byte
# format magic, then NAME, whose body is the font's own name, NUL terminated.
# Only NAME is read here, because it is the only section the theme needs.

#: Where a .pf2 font is looked for, most likely first. grub-common installs the
#: first of these, and grub.cfg on a Debian machine already loads it.
FONT_DIRS = (
    Path("/usr/share/grub"),
    Path("/usr/share/grub2"),
    Path("/boot/grub/fonts"),
    Path("/boot/grub2/fonts"),
    Path("/usr/share/fonts/grub"),
    Path("/usr/local/share/grub"),
)

#: The fonts a machine is most likely to have, best first. unicode.pf2 is the
#: one grub-common ships and the one whose name GRUB already knows.
FONT_NAMES = ("unicode.pf2", "ascii.pf2", "eurlatgr.pf2", "cyberbit.pf2")

#: The extension a GRUB font has.
FONT_SUFFIX = ".pf2"

#: What the font is called inside the theme. 00_header loads every .pf2 it finds
#: in the theme directory, so the file name is only a label - and this is the
#: label Debian's own themes use, which makes this theme's directory read like
#: every other GRUB theme on the machine.
FONT_FILE = "unicode.pf2"

#: The section names of the .pf2 format, from grub-core/font/font.c.
FONT_FILE_SECTION = b"FILE"
FONT_NAME_SECTION = b"NAME"

#: How many bytes of the name to accept. A font name is a line of text; the
#: ceiling is here so that a corrupt length cannot ask for a gigabyte.
FONT_NAME_LIMIT = 256


def read_font_name(path: Path) -> str | None:
    """The name written inside a .pf2 file, or None when it cannot be read.

    The sections are walked rather than assumed to be in a particular order, so
    a font whose NAME is not the second section is still read correctly, and a
    length that runs past the end of the file ends the walk instead of raising.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if not data.startswith(FONT_FILE_SECTION):
        return None

    offset = 0
    while offset + 8 <= len(data):
        section = data[offset:offset + 4]
        length = int.from_bytes(data[offset + 4:offset + 8], "big")
        if length > len(data) - offset - 8:
            return None
        body = data[offset + 8:offset + 8 + length]
        if section == FONT_NAME_SECTION:
            name = body.split(b"\x00", 1)[0][:FONT_NAME_LIMIT]
            text = name.decode("utf-8", "replace").strip()
            return text or None
        offset += 8 + length
    return None


def find_font_file() -> Path | None:
    """A .pf2 font on this machine, or None.

    The well known file names are tried first, in each directory, and only then
    anything else ending in .pf2: a distribution that ships its own font under
    a name of its own is still usable, but the one every Debian machine has is
    preferred because it is the one grub.cfg is most likely to have loaded
    already.
    """
    for name in FONT_NAMES:
        for directory in FONT_DIRS:
            candidate = directory / name
            if candidate.is_file():
                return candidate
    for directory in FONT_DIRS:
        try:
            entries = sorted(directory.iterdir())
        except OSError:
            continue
        for candidate in entries:
            if candidate.is_file() and candidate.suffix.lower() == FONT_SUFFIX:
                return candidate
    return None


def choose_font(log: Log) -> tuple[Path | None, str]:
    """The font to ship and the name to write into theme.txt.

    A machine with no .pf2 gets the name of Debian's own rather than nothing:
    grub.cfg loads ``unicode`` itself on such a machine and GRUB answers a font
    name it does not have with the first font it has loaded, so the theme still
    draws its text. That is a fallback and it is reported as one.
    """
    font_file = find_font_file()
    if font_file is None:
        log.detail(
            "no .pf2 font found; theme.txt will name "
            f"{FALLBACK_FONT_NAME!r}, which grub.cfg loads by itself on Debian"
        )
        return None, FALLBACK_FONT_NAME
    name = read_font_name(font_file)
    if name is None:
        log.detail(f"{font_file} is not a font this script can read its name from")
        return None, FALLBACK_FONT_NAME
    log.detail(f"font: {font_file} (named {name!r})")
    return font_file, name


# --- where the theme is installed ----------------------------------------------


def installed_theme_dir(directory: Path) -> Path:
    """The theme directory under a GRUB theme root."""
    return directory / THEME_NAME


def boot_theme_dir(boot_dir: Path) -> Path:
    """The copy of the theme GRUB itself can read."""
    return boot_dir / "themes" / THEME_NAME


def boot_theme_file(boot_dir: Path) -> Path:
    """The theme.txt that GRUB_THEME is set to.

    It is the copy under /boot and not the one under /usr/share, because GRUB
    reads grub.cfg from the filesystem it was started from: on a machine whose
    /boot is a partition of its own, /usr/share is on a filesystem the boot
    loader cannot open at all. 00_header turns this system path into one
    relative to the root of the boot filesystem, which is why the path written
    into /etc/default/grub may be the ordinary one.
    """
    return boot_theme_dir(boot_dir) / THEME_FILE


# --- building and installing the theme -----------------------------------------


def theme_image_names() -> list[str]:
    """Every image file the installed theme is expected to have.

    theme.txt names these, and GRUB fails to load the whole theme when one of
    them cannot be opened - a missing image is not a missing picture, it is a
    theme that is not drawn at all - so the list is also the checklist the
    verification walks.
    """
    names = [BACKGROUND_FILE, LOGO_FILE]
    for prefix in ("menu", "select", "terminal_box", "scrollbar_thumb"):
        names.extend(f"{prefix}_{box}.png" for box in BOX_NAMES)
    names.extend(f"icons/{icon}.png" for icon in ICON_NAMES)
    return names


def build_theme(
    log: Log, background: Path, logo: Path | None, font_file: Path | None, font_name: str
) -> Path:
    """Render the whole theme into a fresh temporary directory.

    It is built once and copied to both places it is installed, so the two
    copies cannot differ: rendering into each would write the same pixels twice
    and would let a run that failed half way leave two themes that are not the
    same theme.

    A checkout with no logo still gets a logo.png - a fully transparent square.
    That is not decoration: theme.txt draws an image from that file, and GRUB
    abandons the whole theme when an image it names cannot be loaded, so an
    absent file would cost the wallpaper, the menu and the icons as well.
    """
    theme = read_text(SOURCE_THEME_DIR / THEME_FILE)
    if not theme:
        raise SystemExit(
            f"error: {SOURCE_THEME_DIR / THEME_FILE} is missing or empty; "
            "it is the theme this script installs"
        )
    if FONT_PLACEHOLDER not in theme:
        raise SystemExit(
            f"error: {SOURCE_THEME_DIR / THEME_FILE} does not name "
            f"{FONT_PLACEHOLDER}, so the font could not be substituted into it"
        )

    build = Path(tempfile.mkdtemp(prefix="gnuchan-grub-theme-"))
    write_text(build / THEME_FILE, theme.replace(FONT_PLACEHOLDER, font_name))
    write_theme_images(log, build, background, logo)
    if logo is None:
        Raster.solid(LOGO_SIZE, LOGO_SIZE).write_readable(build / LOGO_FILE)
        log.detail(f"wrote {build / LOGO_FILE} (empty, so the theme still loads)")
    if font_file is not None:
        copy_file(log, font_file, build / FONT_FILE)
    return build


def install_theme(log: Log, build: Path, destinations: tuple[Path, ...]) -> list[Path]:
    """Copy the built theme into every destination, returning the ones written.

    An existing theme directory is removed first rather than merged over: a
    slice image that a later version of this script no longer generates would
    otherwise stay behind and keep being served, and a theme directory is this
    script's own by construction.
    """
    written: list[Path] = []
    for destination in destinations:
        try:
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                shutil.rmtree(destination)
            copy_tree(log, build, destination)
        except OSError as error:
            log.warn(f"could not write the theme into {destination}: {error}")
            continue
        written.append(destination)
    return written


# --- /etc/default/grub ----------------------------------------------------------
# /etc/default/grub is a shell fragment that /etc/grub.d/00_header sources, and
# the two keys this theme owns are read from it: GRUB_THEME, which the header
# turns into `set theme=` and, for every *.pf2 in the theme directory, a
# `loadfont`; and GRUB_GFXMODE, which is the mode load_video and gfxterm are
# given. Everything else in the file is the machine's own - the timeout, the
# kernel command line, the distribution's background - and is left exactly as it
# was found, comments and key order included.
#
# Two of those keys do have to be dealt with rather than left, because they
# defeat the theme on their own:
#
#   GRUB_TERMINAL=console   makes the header emit `terminal_output console` and
#                           skip `insmod gfxterm` altogether. A theme is drawn
#                           by gfxterm; without it GRUB shows the plain text
#                           menu however well the theme is installed.
#   GRUB_BACKGROUND         names the distribution's own image. The header only
#                           reads it in the branch that runs when GRUB_THEME is
#                           *not* set, so it cannot override the theme - but it
#                           is a key that says "draw a picture" while the theme
#                           draws another one, and the theme's desktop-image is
#                           what replaces it.
#
# Both are commented out rather than deleted, so the line and its value are
# still in the file and putting it back is one edit.

#: The file the header reads these keys from.
GRUB_DEFAULT_FILE = Path("/etc/default/grub")

#: The comment this script writes above the keys it owns, so a later run knows
#: which lines are its own and where they start.
MANAGED_MARKER = "# GnuchanOS theme settings, written by settings_grub.py"

#: The modes GRUB is asked for after the panel's own, in the order it tries
#: them. 1920x1080 is the wallpaper's own size, for a machine whose panel is
#: that size and whose kernel reported nothing; "auto" is what lets GRUB pick
#: the best mode it has instead of failing to start the video at all.
FALLBACK_GFX_MODES = ("1920x1080", "auto")

#: Where the kernel lists the modes an output supports, most preferred first.
#: The first line is the panel's own mode on every driver that fills the file
#: in, and the status file beside it says whether anything is plugged in.
DRM_MODES_GLOB = "card*-*/modes"


def preferred_mode() -> str | None:
    """The mode the kernel reports first for a connected output, or None.

    This is the panel's own resolution, and it is the mode GRUB should be asked
    for. Asking for a fixed 1920x1080 instead - which is what this used to do -
    is how a theme ends up drawn on a framebuffer wider and taller than the
    screen: GRUB sets the mode it was asked for, the panel has no scaler for it,
    and everything past the edge is never seen. The bottom line of the theme and
    the edges of the wallpaper are the parts that go first, which is exactly how
    the machine this was fixed on looked.
    """
    root = Path("/sys/class/drm")
    if not root.is_dir():
        return None
    for modes in sorted(root.glob(DRM_MODES_GLOB)):
        try:
            if (modes.parent / "status").read_text(encoding="utf-8").strip() != "connected":
                continue
            reported = modes.read_text(encoding="utf-8").splitlines()
        except OSError:
            continue
        if reported and reported[0].strip():
            return reported[0].strip()
    return None


def gfx_mode() -> str:
    """The value written to GRUB_GFXMODE.

    GRUB takes the first mode in the list it can set, so the order is the whole
    of it: the panel's own mode, then the wallpaper's size, then "auto". Nothing
    here is invented when the panel cannot be asked - the two fallbacks are
    always there, and "auto" is why a machine that can do none of the named
    modes still starts its video rather than coming up in text.
    """
    modes = [mode for mode in (preferred_mode(), *FALLBACK_GFX_MODES) if mode]
    ordered: list[str] = []
    for mode in modes:
        if mode not in ordered:
            ordered.append(mode)
    return ",".join(ordered)

#: The keys that defeat the theme, with the test for whether a value of that key
#: is one of them and the reason written above the line that is commented out. A
#: key is only touched when its value really is the defeating one, so a machine
#: that has already been configured sensibly is not rewritten.
DEFEATED_GRUB_KEYS: dict[str, tuple[Callable[[str], bool], str]] = {
    "GRUB_TERMINAL": (
        lambda value: value.strip().strip('"').strip("'") == "console",
        "GRUB_TERMINAL=console switches gfxterm off, and gfxterm is what draws a theme",
    ),
    "GRUB_BACKGROUND": (
        lambda value: bool(value.strip().strip('"').strip("'")),
        "the theme's own desktop-image replaces the distribution's background",
    ),
}


def managed_grub_keys(boot_dir: Path) -> dict[str, str]:
    """The keys this run owns, and the values it writes.

    GRUB_THEME is quoted because that is how Debian's own configuration writes
    it and because the value is a path: the header sources this file, so an
    unquoted path with a space in it would be word split.
    """
    return {
        "GRUB_THEME": f'"{boot_theme_file(boot_dir).as_posix()}"',
        "GRUB_GFXMODE": f'"{gfx_mode()}"',
    }


def assignment(line: str) -> tuple[str, str] | None:
    """The name and value of a shell assignment, or None.

    Only the exact shape ``NAME=value``, with an optional leading ``export``, is
    recognised, and a line that starts with a comment is not one. That is what
    makes the merge idempotent: a key this script commented out is, on the next
    run, a line it no longer sees an assignment in, so it is not commented out
    twice and no second note is written above it.
    """
    text = line.strip()
    if not text or text.startswith("#"):
        return None
    if text.startswith("export "):
        text = text[len("export "):].strip()
    name, separator, value = text.partition("=")
    name = name.strip()
    if not separator or not name:
        return None
    if not all(character.isalnum() or character == "_" for character in name):
        return None
    return name, value


def merge_grub_default(existing: str, managed: dict[str, str]) -> str:
    """Set the managed keys in /etc/default/grub, keeping every other line.

    A managed key is replaced where it already stands and appended if it is
    missing; a duplicate of one is dropped, because two GRUB_THEME lines are a
    file whose meaning depends on which one the header reads last. The marker
    line is placed just above the first managed key, which is what makes a
    second run produce the file the first run produced.
    """
    written: set[str] = set()
    body: list[str] = []

    for line in existing.splitlines():
        if line.strip() == MANAGED_MARKER:
            continue
        entry = assignment(line)
        if entry is None:
            body.append(line)
            continue
        name, value = entry
        if name in managed:
            if name in written:
                continue
            written.add(name)
            body.append(f"{name}={managed[name]}")
            continue
        defeated = DEFEATED_GRUB_KEYS.get(name)
        if defeated is not None and defeated[0](value):
            if body and body[-1].strip():
                body.append("")
            body.append(f"# GnuchanOS: {defeated[1]}.")
            body.append(f"#{name}={value}")
            continue
        body.append(line)

    pending = [(name, value) for name, value in managed.items() if name not in written]
    if pending:
        if body and body[-1].strip():
            body.append("")
        body.extend(f"{name}={value}" for name, value in pending)

    for position, line in enumerate(body):
        entry = assignment(line)
        if entry is not None and entry[0] in managed:
            body.insert(position, MANAGED_MARKER)
            break

    text = "\n".join(body).strip("\n")
    return text + "\n" if text else ""


def read_grub_key(text: str, key: str) -> str | None:
    """The value of a key in /etc/default/grub, unquoted, or None.

    Read with the same parser the merge writes with, so the verification can
    only see what the merge would have produced.
    """
    for line in text.splitlines():
        entry = assignment(line)
        if entry is not None and entry[0] == key:
            return entry[1].strip().strip('"').strip("'")
    return None


def install_grub_default(log: Log, boot_dir: Path) -> None:
    """Merge the theme's keys into /etc/default/grub.

    The file as it was is copied aside once, before anything is merged into it,
    because that copy is the whole undo: putting it back and running update-grub
    is what takes the theme off the machine again.
    """
    existing = read_text(GRUB_DEFAULT_FILE)
    if not existing:
        log.detail(f"{GRUB_DEFAULT_FILE} does not exist; it will be created")
    backup = backup_once(GRUB_DEFAULT_FILE)
    if backup is not None:
        log.detail(f"backed up {GRUB_DEFAULT_FILE.name} to {backup.name}")
    managed = managed_grub_keys(boot_dir)
    write_text(GRUB_DEFAULT_FILE, merge_grub_default(existing, managed))
    for name, value in managed.items():
        log.detail(f"{name}={value}")


# --- regenerating grub.cfg -----------------------------------------------------


def regenerate_grub_config(log: Log, boot_dir: Path) -> bool:
    """Run update-grub, returning whether grub.cfg was rewritten.

    Nothing is regenerated by hand: grub.cfg is generated, and one written by
    this script would be overwritten by the next package upgrade and would miss
    whichever entries the machine's own /etc/grub.d adds - the kernel list, the
    memtest entry, os-prober's Windows entry. The output is left on the terminal
    because it is the only sign of what the command did, and a run that printed
    nothing for a minute would look like a run that had stopped.
    """
    command = grub_mkconfig_command(boot_dir)
    if command is None:
        log.warn(
            "neither update-grub nor grub-mkconfig is installed, so grub.cfg was "
            "not regenerated and the theme will not be loaded until one of them is run"
        )
        return False
    log.detail("running: " + " ".join(command))
    result = run(command)
    if result.returncode != 0:
        log.warn(f"{command[0]} failed; grub.cfg was not regenerated")
        return False
    log.detail(f"regenerated {grub_config_file(boot_dir)}")
    return True


# --- checking the result -------------------------------------------------------
# Everything here is read back from what was written rather than trusted. A theme
# is loaded by GRUB or it is not, and every way it can fail - a missing image, a
# font whose name does not match the one the font really carries, a
# GRUB_TERMINAL left set to console, a grub.cfg that never had update-grub run
# over it - ends in the same place: the plain text menu. This is the step that
# exists to tell those apart before a reboot tells the user instead.


# GRUB does not ignore a property it does not know, or a value it cannot use: it
# fails to load the theme and draws the plain text menu instead. "middle" where
# "center" belongs costs the whole theme - the wallpaper, the menu and the icons
# with it - which is why the properties are checked against the ones GRUB really
# has rather than trusted. The names and the sets of words are the ones in
# grub-core/gfxmenu/view.c for the global properties and in gui_list.c,
# gui_label.c, gui_image.c and gui_progress_bar.c for the components.

#: The properties a theme file may set, by the block they are in: the empty
#: string is the global block. A block that is not named here - one of the
#: component types this theme does not use - has its properties left unchecked.
THEME_PROPERTIES: dict[str, frozenset[str]] = {
    "": frozenset(
        {
            "title-text",
            "title-font",
            "title-color",
            "message-font",
            "message-color",
            "message-bg-color",
            "desktop-image",
            "desktop-image-scale-method",
            "desktop-image-h-align",
            "desktop-image-v-align",
            "desktop-color",
            "terminal-font",
            "terminal-box",
            "terminal-border",
            "terminal-left",
            "terminal-top",
            "terminal-width",
            "terminal-height",
        }
    ),
    "boot_menu": frozenset(
        {
            "item_font",
            "item_color",
            "selected_item_color",
            "item_height",
            "item_padding",
            "item_spacing",
            "item_icon_space",
            "icon_width",
            "icon_height",
            "menu_pixmap_style",
            "selected_item_font",
            "selected_item_pixmap_style",
            "scrollbar",
            "scrollbar_frame",
            "scrollbar_thumb",
            "scrollbar_width",
        }
    ),
    "label": frozenset({"text", "font", "color", "align"}),
    "image": frozenset({"file"}),
    "progress_bar": frozenset(
        {
            "id",
            "font",
            "text",
            "text_color",
            "fg_color",
            "bg_color",
            "border_color",
            "bar_style",
            "highlight_style",
            "show_text",
            "align",
        }
    ),
}

#: The properties every component has, whatever its type.
COMPONENT_GEOMETRY = frozenset({"left", "top", "width", "height"})

#: The component types GRUB has. A block that is not one of them is a theme GRUB
#: cannot load, whatever is written inside it.
COMPONENT_TYPES = frozenset(
    {
        "boot_menu",
        "label",
        "image",
        "progress_bar",
        "circular_progress",
        "vbox",
        "hbox",
        "canvas",
    }
)

#: The properties whose value is one of a fixed set of words, and those words.
#: A value that is not among them is a theme GRUB refuses to load. Everything
#: else a theme sets - a colour, a size, a file name - is free, and is checked
#: by the file and image checks instead.
THEME_VALUES: dict[tuple[str, str], frozenset[str]] = {
    ("", "desktop-image-scale-method"): frozenset({"stretch", "crop", "padding"}),
    ("", "desktop-image-h-align"): frozenset({"left", "center", "right"}),
    ("", "desktop-image-v-align"): frozenset({"top", "center", "bottom"}),
    ("label", "align"): frozenset({"left", "center", "right"}),
}


def theme_properties(text: str) -> list[tuple[str, str, str, int]]:
    """Every property of a theme file, as (block, name, value, line number).

    The format is GRUB's own and looser than it looks: a global property is
    ``name: "value"``, a component is ``+ type { ... }`` with ``name = value``
    inside it, a component may be written on one line and may hold another
    component, a comment starts with # and may follow a value on the same line,
    and a closing brace is a line of its own or the tail of one. The two rules
    that make all of that readable are the ones GRUB reads by: a quoted value
    runs to its closing quote and may hold spaces, and an unquoted one stops at
    the first space or brace - which is what lets several properties share a
    line. The block a property belongs to is the innermost component around it,
    which is what the tables above are keyed by, and is empty outside every
    component.
    """
    found: list[tuple[str, str, str, int]] = []
    stack: list[str] = []
    for number, line in enumerate(text.splitlines(), start=1):
        rest = line
        while True:
            trimmed = rest.lstrip()
            if not trimmed or trimmed.startswith("#"):
                break
            if trimmed.startswith("}"):
                if stack:
                    stack.pop()
                rest = trimmed[1:]
                continue
            if trimmed.startswith("+"):
                name, brace, after = trimmed[1:].partition("{")
                if not brace:
                    break
                stack.append(name.strip())
                rest = after
                continue
            # The name runs to whichever separator comes first: a global
            # property is written `name: value`, a component property
            # `name = value`, and a value may hold either character.
            stop = len(trimmed)
            for candidate in ("=", ":"):
                position = trimmed.find(candidate)
                if 0 <= position < stop:
                    stop = position
            if stop == len(trimmed):
                break
            name = trimmed[:stop]
            value = trimmed[stop + 1:].lstrip()
            if value.startswith('"'):
                end = value.find('"', 1)
                if end < 0:
                    break
                rest = value[end + 1:]
                value = value[:end + 1]
            else:
                end = 0
                while end < len(value) and not value[end].isspace() and value[end] != "}":
                    end += 1
                rest = value[end:]
                value = value[:end]
            found.append(
                (
                    stack[-1] if stack else "",
                    name.strip(),
                    value.strip().strip('"'),
                    number,
                )
            )
            if not rest.strip():
                break
    return found


def check_theme_properties(text: str) -> list[str]:
    """Problems GRUB would refuse the theme over, in the properties it sets.

    A property that is not one GRUB has and a value outside the set a property
    accepts are the same failure seen twice: the theme is not drawn at all, and
    the machine boots the plain text menu with every file in place.
    """
    problems: list[str] = []
    for block, name, value, line in theme_properties(text):
        if block and block not in COMPONENT_TYPES:
            problems.append(
                f"line {line}: + {block} is not a component GRUB has, so the "
                "theme fails to load and the plain text menu is drawn"
            )
            continue
        allowed = THEME_PROPERTIES.get(block)
        if allowed is not None and name not in allowed and name not in COMPONENT_GEOMETRY:
            problems.append(
                f"line {line}: {block or 'the theme'} has no property {name!r}, "
                "and GRUB refuses the whole theme over a property it does not know"
            )
            continue
        words = THEME_VALUES.get((block, name))
        if words is not None and value not in words:
            problems.append(
                f"line {line}: {name} is {value!r}, and GRUB accepts only "
                + ", ".join(sorted(words))
            )
    return problems


def theme_font_name(text: str) -> str | None:
    """The font theme.txt names in title-font, or None."""
    for line in text.splitlines():
        name, separator, value = line.partition(":")
        if separator and name.strip() == "title-font":
            return value.strip().strip('"')
    return None


def check_theme_dir(directory: Path, font_name: str) -> list[str]:
    """Problems with one copy of the installed theme.

    Every image is decoded rather than merely looked for. GRUB abandons a theme
    it cannot load a piece of, so a zero byte slice costs the wallpaper and the
    menu as well as the slice - and a half written file is exactly what a run
    interrupted in the middle leaves behind.

    The font is checked the way GRUB looks one up: the name in theme.txt is
    compared with the name inside the .pf2 that was shipped, because those are
    two different strings and only the second one is what GRUB matches on. When
    no font was shipped the fallback name is written and grub.cfg's own loadfont
    is what answers it, which is not a problem and is not reported as one.

    The properties are read as GRUB reads them, because a name GRUB does not
    know or a value it cannot use is not something it ignores: the theme fails
    to load and the plain text menu is drawn instead.
    """
    problems: list[str] = []
    theme_file = directory / THEME_FILE
    text = read_text(theme_file)
    if not text:
        return [f"{theme_file} is missing or empty"]

    if FONT_PLACEHOLDER in text:
        problems.append(f"{theme_file} still names {FONT_PLACEHOLDER}")
    named = theme_font_name(text)
    if not named:
        problems.append(f"{theme_file} names no title-font")
    elif named != font_name:
        problems.append(
            f"{theme_file} asks for the font {named!r}, but {font_name!r} is the "
            "name of the font that was found"
        )

    font_path = directory / FONT_FILE
    if font_path.is_file():
        inside = read_font_name(font_path)
        if inside is None:
            problems.append(f"{font_path} is not a .pf2 font this script can read")
        elif inside != font_name:
            problems.append(
                f"{theme_file} asks for the font {font_name!r}, but {font_path} "
                f"carries the name {inside!r}, which is what GRUB matches on"
            )

    for name in theme_image_names():
        path = directory / name
        if not path.is_file():
            problems.append(f"{path} is missing")
        elif path.stat().st_size == 0:
            problems.append(f"{path} is empty")
        elif read_png(path) is None:
            problems.append(f"{path} is not a PNG GRUB can read")

    problems.extend(check_theme_properties(text))
    return problems


def check_grub_default(boot_dir: Path) -> list[str]:
    """Problems with what /etc/default/grub now tells the header to do.

    Read back with the same parser the merge writes with, so a key this script
    commented out is really gone from what the header will see, and a key it
    wrote is really there.
    """
    problems: list[str] = []
    text = read_text(GRUB_DEFAULT_FILE)
    if not text:
        return [f"{GRUB_DEFAULT_FILE} is missing or empty"]

    wanted = boot_theme_file(boot_dir).as_posix()
    theme = read_grub_key(text, "GRUB_THEME")
    if theme is None:
        problems.append(
            f"{GRUB_DEFAULT_FILE} does not set GRUB_THEME, so the header writes no "
            "`set theme=` line and nothing this script installed is used"
        )
    elif theme != wanted:
        problems.append(
            f"{GRUB_DEFAULT_FILE} sets GRUB_THEME to {theme} instead of {wanted}"
        )
    elif not Path(theme).is_file():
        problems.append(f"{GRUB_DEFAULT_FILE} names {theme}, which is not there")

    mode = read_grub_key(text, "GRUB_GFXMODE")
    panel = preferred_mode()
    if mode is None:
        problems.append(
            f"{GRUB_DEFAULT_FILE} does not set GRUB_GFXMODE, so GRUB asks for the "
            "mode its own default names and the wallpaper is cropped to that"
        )
    elif panel is not None and mode.split(",")[0].strip() not in ("auto", panel):
        problems.append(
            f"{GRUB_DEFAULT_FILE} asks GRUB for {mode.split(',')[0].strip()}, and "
            f"this machine's panel reports {panel}: a mode the panel cannot show "
            "whole is drawn past the edges of the screen, and the bottom of the "
            "theme and the edges of the wallpaper are what is lost"
        )

    terminal = read_grub_key(text, "GRUB_TERMINAL")
    if terminal == "console":
        problems.append(
            f"{GRUB_DEFAULT_FILE} still sets GRUB_TERMINAL=console, which switches "
            "gfxterm off, and a theme is drawn by gfxterm"
        )
    if read_grub_key(text, "GRUB_BACKGROUND"):
        problems.append(
            f"{GRUB_DEFAULT_FILE} still sets GRUB_BACKGROUND; the theme's own "
            "desktop-image is what replaces it"
        )
    return problems


def check_grub_cfg(boot_dir: Path, font_shipped: bool) -> list[str]:
    """Problems with the grub.cfg that was generated from all of that.

    This is the check the others exist for. Everything above can be right while
    grub.cfg still has no theme in it, because grub.cfg is generated and does not
    change until update-grub is run over it - and a machine whose grub.cfg was
    never regenerated boots exactly as it did before, whatever else was
    installed.

    The lines looked for are the ones util/grub.d/00_header writes for a theme:
    `insmod gfxmenu`, `insmod png` when the theme directory holds PNGs (this one
    always does), a `loadfont` per .pf2 in the theme directory and in its f/
    subdirectory, and `set theme=`.
    """
    problems: list[str] = []
    grub_cfg = grub_config_file(boot_dir)
    if not grub_cfg.is_file():
        return [f"{grub_cfg} is missing"]
    text = read_text(grub_cfg)
    if not text:
        return [f"{grub_cfg} is empty"]

    if "set theme=" not in text:
        problems.append(
            f"{grub_cfg} has no `set theme=` line, so GRUB loads no theme at all: "
            "run update-grub, and check that /etc/default/grub is the file it reads"
        )
    elif boot_theme_dir(boot_dir).name not in text:
        problems.append(
            f"{grub_cfg} sets a theme that is not the {THEME_NAME} one, so "
            "something else is overriding /etc/default/grub"
        )

    if "insmod gfxmenu" not in text:
        problems.append(f"{grub_cfg} does not `insmod gfxmenu`, so it cannot load a theme")
    if "insmod png" not in text:
        problems.append(
            f"{grub_cfg} does not `insmod png`, and every image this theme is drawn "
            "from is a PNG"
        )
    if font_shipped and "loadfont" not in text:
        problems.append(
            f"{grub_cfg} has no `loadfont` line, so the font theme.txt names is "
            "never loaded and GRUB falls back to the first font it has"
        )
    return problems


def check_result(log: Log, boot_dir: Path, directories: tuple[Path, ...], font_name: str) -> int:
    """Report what is wrong with the install, returning how many things are.

    The copies of the theme come first because a problem there is a problem
    twice, once for each destination, and the reader should see the one that
    matters before the list of its consequences.
    """
    font_shipped = any(
        (directory / FONT_FILE).is_file() for directory in directories
    )
    problems: list[str] = []
    for directory in directories:
        problems.extend(check_theme_dir(directory, font_name))
    problems.extend(check_grub_default(boot_dir))
    problems.extend(check_grub_cfg(boot_dir, font_shipped))

    if not problems:
        log.note(
            "No problems found in "
            + ", ".join(str(directory) for directory in directories)
            + "."
        )
        return 0
    log.note(f"{len(problems)} problem(s) found:")
    for problem in problems:
        log.note(f"  {problem}")
    return len(problems)


# --- entry point ---------------------------------------------------------------


def cleanup(log: Log, build: Path) -> None:
    """Remove the temporary directory the theme was rendered into.

    It is removed whether the run succeeded or failed: what it holds has been
    copied to both places the theme is installed, and a directory of a few
    hundred kilobytes of PNGs left behind under /tmp every time the script runs
    is litter that nobody goes looking for.
    """
    try:
        shutil.rmtree(build)
    except OSError as error:
        log.detail(f"could not remove the build directory {build}: {error}")


def main() -> int:
    """Install the GnuchanOS GRUB theme, with no options to pass."""
    log = Log()
    ensure_root(log)

    log.step(f"Installing the GnuchanOS GRUB theme ({distro_description()})")

    # The images are resolved before anything is written, so a machine that
    # does not have them is left exactly as it was found.
    background = require_background()
    logo = optional_logo()
    log.detail(f"wallpaper: {background}")
    log.detail(f"logo: {logo}" if logo is not None else "logo: none in this checkout")

    if not grub_present():
        raise SystemExit(
            "error: neither update-grub nor grub-mkconfig is installed, so this "
            "machine does not start GRUB and there is no menu to theme"
        )
    boot_dir = grub_boot_dir()
    if boot_dir is None:
        raise SystemExit(
            "error: neither /boot/grub nor /boot/grub2 exists, so there is no "
            "GRUB installation to put a theme into"
        )
    log.detail(f"GRUB: {boot_dir}")

    font_file, font_name = choose_font(log)

    log.step("Building the theme")
    build = build_theme(log, background, logo, font_file, font_name)

    try:
        log.step("Installing the theme")
        written = install_theme(
            log,
            build,
            (installed_theme_dir(GRUB_SHARE_THEMES), boot_theme_dir(boot_dir)),
        )
        if not written:
            raise SystemExit("error: the theme could not be written anywhere")

        log.step("Configuring GRUB")
        install_grub_default(log, boot_dir)

        log.step("Regenerating grub.cfg")
        regenerate_grub_config(log, boot_dir)

        log.step("Checking the result")
        problems = check_result(log, boot_dir, tuple(written), font_name)
    finally:
        cleanup(log, build)

    log.note("")
    log.note("The GnuchanOS GRUB theme is installed:")
    log.note(f"  theme      {boot_theme_dir(boot_dir)}")
    log.note(f"  settings   {GRUB_DEFAULT_FILE}")
    log.note(f"  wallpaper  {boot_theme_dir(boot_dir) / BACKGROUND_FILE}")
    if font_file is not None:
        log.note(f"  font       {boot_theme_dir(boot_dir) / FONT_FILE} ({font_name})")
    else:
        log.note(f"  font       {font_name} (loaded by grub.cfg itself)")
    log.note("")
    if problems:
        log.note("The problems listed above have to be fixed before the theme will be")
        log.note("drawn; until then the machine boots GRUB's plain text menu.")
    else:
        log.note("The theme is drawn from the next boot, and not before: GRUB reads")
        log.note("grub.cfg once, when it starts.")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
