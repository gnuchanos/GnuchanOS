#!/usr/bin/env python3
# =============================================================================
# GnuchanPurple - theme installer
# -----------------------------------------------------------------------------
# Single file installer for the theme in ./GnuchanPurple. No third party
# modules: everything it needs, including the PNG writer xfwm4 requires, is in
# this file.
#
# What it does
# ------------
#   1. copies GnuchanPurple/ into the user theme directories GTK 2, GTK 3,
#      GTK 4, xfwm4, Marco/Metacity and Openbox read;
#   2. renders the xfwm4 decoration images (title bar, borders and every title
#      button) plus the preview that appearance settings show, straight into
#      the installed theme, because xfwm4 composites PNGs. The same renderer
#      produced the copies committed under GnuchanPurple/;
#   3. installs the GTK 4 / libadwaita overlay as ~/.config/gtk-4.0/gtk.css,
#      which is how libadwaita applications get the palette without GTK_THEME;
#   4. writes ~/.gtkrc-2.0 for GTK 2 and the GTK 3 and GTK 4 settings.ini
#      files, which are what actually select the theme on the sessions that
#      have no XSettings daemon (i3, sway, Openbox, plain lxappearance setups);
#   5. copies the terminal / launcher / bar extras to ~/.config/gnuchan-purple.
#
# Existing files are moved aside to <name>.gnuchan-backup instead of being
# overwritten silently, so nothing this installer touches is lost.
#
#     python theme_install.py                  # install
#     python theme_install.py --render-assets  # regenerate the repo PNGs
#
# GnuchanPurple/xfwm4 and GnuchanPurple/thumbnail.png hold the images the
# renderer produces, so a plain "cp -r GnuchanPurple ~/.themes" works without
# running this script at all. --render-assets is how those files are made
# again after a palette change, and the install path regenerates them into the
# installed copy regardless of what the checked in files contain.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path
from typing import Callable

RGBA = tuple[int, int, int, int]

# --- locations ---------------------------------------------------------------

#: The name GTK looks the theme up by, and the directory it is installed as.
#: It is deliberately not the icon theme's name or the cursor theme's: all
#: three are picked in the same dialog, and two of them are installed into the
#: same `icons` directories - an icon theme and a cursor theme that shared a
#: name would be one directory, and whichever was installed second would
#: overwrite the first.
THEME_NAME = "GnuChanTheme"
#: The directory the extras land in under ``~/.config``. Deliberately the same
#: string as the theme name rather than a second spelling of it: one component
#: with two names means the theme directory, the config directory and the name
#: in the settings files can disagree, and a search for either name finds only
#: half of what an install wrote.
CONFIG_DIR_NAME = THEME_NAME
BACKUP_SUFFIX = ".gnuchan-backup"
GENERATED_MARKER = "written by theme_install.py"
IGNORED_COPY_NAMES = {"__pycache__", ".DS_Store", "Thumbs.db"}

SCRIPT_DIR = Path(__file__).resolve().parent
# The source directory carries the theme name on purpose. A user, or a
# distribution packager, who copies this tree into ~/.themes or
# ~/.local/share/themes by hand then gets a directory that the
# "gtk-theme-name=GnuchanPurple" this installer writes can actually find; a
# directory called anything else is silently ignored by every toolkit.
SOURCE_THEME_DIR = SCRIPT_DIR / THEME_NAME
SOURCE_THEME_OVERLAY = SOURCE_THEME_DIR / "gtk-4.0" / "user.css"
SOURCE_THEME_OVERLAY_GTK3 = SOURCE_THEME_DIR / "gtk-3.0" / "user.css"
SOURCE_EXTRAS_DIR = SOURCE_THEME_DIR / "extras"

# --- palette -----------------------------------------------------------------
# The colours the built-in renderer rasterises the xfwm4 images and the theme
# preview from. They are taken from gtk-2.0/gtkrc, gtk-3.0/gtk.css and
# gtk-4.0/gtk.css, but this table is deliberately not a copy of those files: it
# only carries the values the renderer needs, plus the close button tints.
#
# GnuchanPurple/ is the source of truth for the theme palette. Add an entry
# here only when a generated image needs a colour, and keep it in step with the
# stylesheet by hand.

_PALETTE_SOURCE: dict[str, str] = {
    # Surfaces
    "bg_darkest": "#09030d",
    "bg": "#0e0614",
    "bg_alt": "#0f0615",
    "surface": "#12071a",
    "surface_alt": "#180a22",
    "raised": "#1c0b27",
    "hover": "#21102b",
    "raised_hover": "#281036",
    # Lines
    "border_alt": "#24102f",
    "border": "#32143f",
    "border_strong": "#6d28d9",
    # Accents
    "accent": "#a855f7",
    "accent_hover": "#b76eff",
    "accent_active": "#9333ea",
    "accent_light": "#c77dff",
    "accent_pale": "#d8a4ff",
    "selected_bg": "#542080",
    "selected_bg_alt": "#6d28d9",
    # Text
    "fg": "#ead7ff",
    "fg_bright": "#f3e8ff",
    "fg_dim": "#b78ed6",
    "fg_disabled": "#70458a",
    "white": "#ffffff",
    # Close button leans red, as it does in every other file of the theme
    "close": "#e879f9",
    "close_bg": "#86198f",
    "close_bg_pressed": "#a21caf",
    "close_rest": "#2a0b2f",
}


def hex_to_rgba(value: str) -> RGBA:
    """Parse ``#rgb``, ``#rrggbb`` or ``#rrggbbaa`` into an RGBA tuple."""
    text = value.strip().lstrip("#")
    if len(text) == 3:
        text = "".join(ch * 2 for ch in text)
    if len(text) == 6:
        text += "ff"
    if len(text) != 8:
        raise ValueError(f"not a colour: {value!r}")
    return (
        int(text[0:2], 16),
        int(text[2:4], 16),
        int(text[4:6], 16),
        int(text[6:8], 16),
    )


PALETTE: dict[str, RGBA] = {
    name: hex_to_rgba(code) for name, code in _PALETTE_SOURCE.items()
}


# --- signed distance helpers -------------------------------------------------
# Shapes are described as distance fields and rasterised from them, so the
# diagonal glyph strokes and the rounded button corners come out antialiased.


def _clamp(value: float, low: float, high: float) -> float:
    if value < low:
        return low
    if value > high:
        return high
    return value


def _rounded_box_distance(
    px: float, py: float, x: float, y: float, width: float, height: float, radius: float
) -> float:
    half_w = width / 2.0
    half_h = height / 2.0
    corner = _clamp(radius, 0.0, min(half_w, half_h))
    qx = abs(px - (x + half_w)) - (half_w - corner)
    qy = abs(py - (y + half_h)) - (half_h - corner)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    inside = min(max(qx, qy), 0.0)
    return outside + inside - corner


def _segment_distance(
    px: float, py: float, ax: float, ay: float, bx: float, by: float
) -> float:
    dx = bx - ax
    dy = by - ay
    length_sq = dx * dx + dy * dy
    if length_sq <= 0.0:
        return math.hypot(px - ax, py - ay)
    t = _clamp(((px - ax) * dx + (py - ay) * dy) / length_sq, 0.0, 1.0)
    return math.hypot(px - (ax + dx * t), py - (ay + dy * t))


def _disc_distance(px: float, py: float, cx: float, cy: float, radius: float) -> float:
    return math.hypot(px - cx, py - cy) - radius


def _paint_bounds(
    x: float, y: float, width: float, height: float, thickness: float, filled: bool
) -> tuple[int, int, int, int]:
    grow = 0.0 if filled else thickness / 2.0
    return (
        int(math.floor(x - grow - 1.0)),
        int(math.floor(y - grow - 1.0)),
        int(math.ceil(x + width + grow + 1.0)),
        int(math.ceil(y + height + grow + 1.0)),
    )


# --- raster surface ----------------------------------------------------------


class Canvas:
    """Small RGBA surface with the few primitives the window buttons need."""

    def __init__(self, width: int, height: int, fill: RGBA = (0, 0, 0, 0)) -> None:
        if width < 1 or height < 1:
            raise ValueError("a canvas needs to be at least 1x1")
        self.width = width
        self.height = height
        self._pixels = bytearray(width * height * 4)
        self.clear(fill)

    def clear(self, color: RGBA) -> None:
        red, green, blue, alpha = color
        self._pixels[:] = bytes((red, green, blue, alpha)) * (self.width * self.height)

    def _blend(self, x: int, y: int, color: RGBA, coverage: float) -> None:
        if coverage <= 0.0 or not (0 <= x < self.width and 0 <= y < self.height):
            return
        source_alpha = (color[3] / 255.0) * _clamp(coverage, 0.0, 1.0)
        if source_alpha <= 0.0:
            return
        offset = (y * self.width + x) * 4
        pixels = self._pixels
        dst_red = pixels[offset]
        dst_green = pixels[offset + 1]
        dst_blue = pixels[offset + 2]
        dst_alpha = pixels[offset + 3] / 255.0
        out_alpha = source_alpha + dst_alpha * (1.0 - source_alpha)
        if out_alpha <= 0.0:
            pixels[offset:offset + 4] = b"\x00\x00\x00\x00"
            return
        keep = dst_alpha * (1.0 - source_alpha)
        pixels[offset] = round((color[0] * source_alpha + dst_red * keep) / out_alpha)
        pixels[offset + 1] = round((color[1] * source_alpha + dst_green * keep) / out_alpha)
        pixels[offset + 2] = round((color[2] * source_alpha + dst_blue * keep) / out_alpha)
        pixels[offset + 3] = round(out_alpha * 255.0)

    def _shade(
        self,
        distance: Callable[[float, float], float],
        color: RGBA,
        bounds: tuple[int, int, int, int],
        filled: bool,
        thickness: float,
    ) -> None:
        half = thickness / 2.0
        left, top, right, bottom = bounds
        left = max(left, 0)
        top = max(top, 0)
        right = min(right, self.width - 1)
        bottom = min(bottom, self.height - 1)
        for y in range(top, bottom + 1):
            py = y + 0.5
            for x in range(left, right + 1):
                value = distance(x + 0.5, py)
                if filled:
                    coverage = _clamp(0.5 - value, 0.0, 1.0)
                else:
                    coverage = _clamp(0.5 + half - abs(value), 0.0, 1.0)
                if coverage > 0.0:
                    self._blend(x, y, color, coverage)

    def rect(
        self,
        x: float,
        y: float,
        width: float,
        height: float,
        color: RGBA,
        filled: bool = True,
        thickness: float = 1.0,
        radius: float = 0.0,
    ) -> None:
        """Filled or stroked rectangle; a stroke sits just inside the edges."""
        inset = 0.0 if filled else thickness / 2.0

        def distance(px: float, py: float) -> float:
            return _rounded_box_distance(
                px, py, x + inset, y + inset, width - inset * 2, height - inset * 2, radius
            )

        self._shade(
            distance,
            color,
            _paint_bounds(x, y, width, height, thickness, filled),
            filled,
            thickness,
        )

    def line(
        self,
        x0: float,
        y0: float,
        x1: float,
        y1: float,
        color: RGBA,
        thickness: float = 1.0,
    ) -> None:
        def distance(px: float, py: float) -> float:
            return _segment_distance(px, py, x0, y0, x1, y1)

        pad = thickness / 2.0 + 1.0
        bounds = (
            int(math.floor(min(x0, x1) - pad)),
            int(math.floor(min(y0, y1) - pad)),
            int(math.ceil(max(x0, x1) + pad)),
            int(math.ceil(max(y0, y1) + pad)),
        )
        self._shade(distance, color, bounds, False, thickness)

    def disc(self, cx: float, cy: float, radius: float, color: RGBA) -> None:
        def distance(px: float, py: float) -> float:
            return _disc_distance(px, py, cx, cy, radius)

        bounds = (
            int(math.floor(cx - radius - 1.0)),
            int(math.floor(cy - radius - 1.0)),
            int(math.ceil(cx + radius + 1.0)),
            int(math.ceil(cy + radius + 1.0)),
        )
        self._shade(distance, color, bounds, True, 1.0)

    def to_png(self) -> bytes:
        """Encode the surface as a truecolour-with-alpha PNG."""
        stride = self.width * 4
        raw = bytearray()
        for row in range(self.height):
            raw.append(0)  # filter type 0 (none) keeps the encoder tiny
            raw += self._pixels[row * stride:(row + 1) * stride]
        return (
            _PNG_SIGNATURE
            + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
            + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _png_chunk(b"IEND", b"")
        )

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(self.to_png())


_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


# --- xfwm4 decoration images -------------------------------------------------
# xfwm4 composites PNGs for the title bar, the border and every title button,
# so these are rendered rather than drawn by hand. The same images are
# committed under GnuchanPurple/ so that copying the theme directory by hand
# without running this script still produces a complete xfwm4 theme; see
# --render-assets.

BUTTON_SIZE = 24
BUTTON_RADIUS = 5.0
TITLE_HEIGHT = 30
TITLE_CAP_WIDTH = 8

BUTTON_STATES = ("active", "inactive", "prelight", "pressed")

BUTTON_FUNCTIONS = (
    "close",
    "hide",
    "maximize",
    "maximize-toggled",
    "menu",
    "shade",
    "shade-toggled",
    "stick",
    "stick-toggled",
    "unmaximize",
    "unshade",
    "unstick",
)

FRAME_EDGES = (
    "top-left",
    "top",
    "top-right",
    "left",
    "right",
    "bottom-left",
    "bottom",
    "bottom-right",
)


def button_colors(function: str, state: str) -> tuple[RGBA, RGBA, RGBA]:
    """Return ``(fill, border, glyph)`` for one button in one state.

    The four states map onto the four images xfwm4 loads per button:
    ``<name>-active.png``, ``<name>-inactive.png``, ``<name>-prelight.png`` and
    ``<name>-pressed.png``.
    """
    if function == "close":
        # Close leans red, as it does in the GTK and Openbox themes. Only the
        # active window gets the full treatment; an unfocused one stays neutral.
        if state == "active":
            return PALETTE["close_rest"], PALETTE["close"], PALETTE["close"]
        if state == "prelight":
            return PALETTE["close_bg"], PALETTE["close"], PALETTE["white"]
        if state == "pressed":
            return (
                PALETTE["close_bg_pressed"],
                PALETTE["close_bg_pressed"],
                PALETTE["white"],
            )
        return PALETTE["surface"], PALETTE["border_alt"], PALETTE["fg_disabled"]

    if state == "active":
        return PALETTE["raised"], PALETTE["border"], PALETTE["fg"]
    if state == "prelight":
        return PALETTE["raised_hover"], PALETTE["accent"], PALETTE["fg_bright"]
    if state == "pressed":
        return PALETTE["selected_bg"], PALETTE["accent"], PALETTE["white"]
    return PALETTE["surface"], PALETTE["border_alt"], PALETTE["fg_disabled"]


def _draw_close(canvas: Canvas, color: RGBA) -> None:
    canvas.line(7.5, 7.5, 16.5, 16.5, color, 2.0)
    canvas.line(16.5, 7.5, 7.5, 16.5, color, 2.0)


def _draw_hide(canvas: Canvas, color: RGBA) -> None:
    canvas.line(6.5, 15.5, 17.5, 15.5, color, 2.0)


def _draw_maximize(canvas: Canvas, color: RGBA) -> None:
    canvas.rect(6.5, 6.5, 11.0, 11.0, color, filled=False, thickness=2.0, radius=1.5)


def _draw_restore(canvas: Canvas, color: RGBA) -> None:
    # Front window on top, plus only the two back edges that stay visible.
    canvas.rect(5.5, 9.5, 9.0, 9.0, color, filled=False, thickness=1.7, radius=1.5)
    canvas.line(9.0, 5.5, 17.5, 5.5, color, 1.7)
    canvas.line(17.5, 5.5, 17.5, 14.0, color, 1.7)


def _draw_menu(canvas: Canvas, color: RGBA) -> None:
    for y in (8.0, 12.0, 16.0):
        canvas.line(6.5, y, 17.5, y, color, 2.0)


def _draw_shade(canvas: Canvas, color: RGBA) -> None:
    canvas.line(6.5, 15.0, 12.0, 9.0, color, 2.0)
    canvas.line(12.0, 9.0, 17.5, 15.0, color, 2.0)


def _draw_unshade(canvas: Canvas, color: RGBA) -> None:
    canvas.line(6.5, 9.0, 12.0, 15.0, color, 2.0)
    canvas.line(12.0, 15.0, 17.5, 9.0, color, 2.0)


def _draw_stick(canvas: Canvas, color: RGBA) -> None:
    canvas.disc(12.0, 9.0, 3.2, color)
    canvas.line(12.0, 12.2, 12.0, 16.5, color, 2.0)


_BUTTON_GLYPHS: dict[str, Callable[[Canvas, RGBA], None]] = {
    "close": _draw_close,
    "hide": _draw_hide,
    "maximize": _draw_maximize,
    "maximize-toggled": _draw_restore,
    "unmaximize": _draw_restore,
    "menu": _draw_menu,
    "shade": _draw_shade,
    "shade-toggled": _draw_unshade,
    "unshade": _draw_unshade,
    "stick": _draw_stick,
    "stick-toggled": _draw_stick,
    "unstick": _draw_stick,
}


def render_button(function: str, state: str) -> Canvas:
    """Render one title button at its natural size."""
    fill, border, glyph = button_colors(function, state)
    canvas = Canvas(BUTTON_SIZE, BUTTON_SIZE, (0, 0, 0, 0))
    inset = 0.5
    size = BUTTON_SIZE - 1.0
    canvas.rect(inset, inset, size, size, fill, filled=True, radius=BUTTON_RADIUS)
    canvas.rect(
        inset, inset, size, size, border, filled=False, thickness=1.0, radius=BUTTON_RADIUS
    )
    painter = _BUTTON_GLYPHS.get(function)
    if painter is not None:
        painter(canvas, glyph)
    return canvas


def render_title_segment(index: int, active: bool) -> Canvas:
    """Render one of the five title bar segments.

    Segments 2, 3 and 4 are the stretchable fill, so they are a single pixel
    wide; 1 and 5 are the end caps that carry the side border.
    """
    border = PALETTE["border"] if active else PALETTE["border_alt"]
    width = TITLE_CAP_WIDTH if index in (1, 5) else 1
    canvas = Canvas(width, TITLE_HEIGHT, PALETTE["bg_darkest"])
    # The hairline under the title bar mirrors the GTK header bar border.
    canvas.rect(0, TITLE_HEIGHT - 1, width, 1, border, filled=True)
    if index == 1:
        canvas.rect(0, 0, 1, TITLE_HEIGHT, border, filled=True)
    elif index == 5:
        canvas.rect(width - 1, 0, 1, TITLE_HEIGHT, border, filled=True)
    return canvas


def render_frame_edge(active: bool) -> Canvas:
    """Render a 1x1 border tile; xfwm4 tiles these along the edges and corners."""
    return Canvas(1, 1, PALETTE["border"] if active else PALETTE["border_alt"])


def render_xfwm4_assets(theme_dir: Path, report: Callable[[str], None] | None = None) -> int:
    """Render the whole xfwm4 image set into ``<theme_dir>/xfwm4``.

    Returns the number of images written.
    """
    target = Path(theme_dir) / "xfwm4"
    written = 0
    for function in BUTTON_FUNCTIONS:
        for state in BUTTON_STATES:
            render_button(function, state).save(target / f"{function}-{state}.png")
            written += 1
            if report is not None:
                report(f"xfwm4/{function}-{state}.png")
    for index in (1, 2, 3, 4, 5):
        for active, suffix in ((True, "active"), (False, "inactive")):
            render_title_segment(index, active).save(target / f"title-{index}-{suffix}.png")
            written += 1
            if report is not None:
                report(f"xfwm4/title-{index}-{suffix}.png")
    for name in FRAME_EDGES:
        for active, suffix in ((True, "active"), (False, "inactive")):
            render_frame_edge(active).save(target / f"{name}-{suffix}.png")
            written += 1
            if report is not None:
                report(f"xfwm4/{name}-{suffix}.png")
    return written


def render_thumbnail() -> Canvas:
    """Render the 256x192 preview that appearance settings show for a theme.

    Without this file GNOME Tweaks and the Xfce appearance dialog show an empty
    tile for the theme, so it is rendered from the same palette as everything
    else rather than committed as a binary.
    """
    canvas = Canvas(256, 192, PALETTE["bg_darkest"])

    # A window: dark title bar on top, body below, one rounded border around it.
    canvas.rect(20, 20, 216, 152, PALETTE["bg_darkest"], radius=8.0)
    canvas.rect(21, 49, 214, 122, PALETTE["bg"])
    canvas.rect(20, 20, 216, 152, PALETTE["border"], filled=False, thickness=1.0, radius=8.0)

    # Title buttons, right to left: close, maximise, minimise.
    buttons = (
        (PALETTE["close_rest"], PALETTE["close"]),
        (PALETTE["raised"], PALETTE["border"]),
        (PALETTE["raised"], PALETTE["border"]),
    )
    for index, (fill, border) in enumerate(buttons):
        x = 222 - index * 18
        canvas.rect(x - 7, 27, 14, 14, fill, radius=4.0)
        canvas.rect(x - 7, 27, 14, 14, border, filled=False, thickness=1.0, radius=4.0)
        if index == 0:
            canvas.line(x - 3, 31, x + 3, 37, PALETTE["close"], 1.3)
            canvas.line(x + 3, 31, x - 3, 37, PALETTE["close"], 1.3)
        elif index == 1:
            canvas.rect(x - 4, 30, 8, 8, PALETTE["fg_dim"], filled=False, thickness=1.3)
        else:
            canvas.line(x - 4, 37, x + 4, 37, PALETTE["fg_dim"], 1.3)

    # Sidebar with its first row selected.
    canvas.rect(21, 49, 56, 122, PALETTE["surface"])
    for row in range(4):
        color = PALETTE["selected_bg"] if row == 0 else PALETTE["raised"]
        canvas.rect(29, 60 + row * 22, 40, 12, color, radius=3.0)

    # Content rows and a progress bar.
    for row in range(4):
        canvas.rect(85, 60 + row * 22, 130, 12, PALETTE["surface_alt"], radius=3.0)
    canvas.rect(85, 150, 130, 8, PALETTE["bg_darkest"], radius=4.0)
    canvas.rect(85, 150, 78, 8, PALETTE["accent_active"], radius=4.0)

    return canvas


# --- target locations --------------------------------------------------------


def home_dir() -> Path:
    return Path(os.path.expanduser("~")).resolve()


def _xdg_dir(variable: str, fallback: Path) -> Path:
    value = os.environ.get(variable, "").strip()
    return Path(value).expanduser() if value else fallback


def xdg_config_home() -> Path:
    return _xdg_dir("XDG_CONFIG_HOME", home_dir() / ".config")


def xdg_data_home() -> Path:
    return _xdg_dir("XDG_DATA_HOME", home_dir() / ".local" / "share")


def user_theme_dirs() -> list[Path]:
    """Where a user level theme is looked up, most modern location first.

    ``$XDG_DATA_HOME/themes`` is what the current GTK 3 and GTK 4 stacks prefer;
    ``~/.themes`` is the legacy location GTK 2, xfwm4 and most window managers
    still read, so both are filled in.
    """
    return [xdg_data_home() / "themes", home_dir() / ".themes"]


def gtk2_rc_file() -> Path:
    return home_dir() / ".gtkrc-2.0"


def gtk3_settings_file() -> Path:
    return xdg_config_home() / "gtk-3.0" / "settings.ini"


def gtk4_settings_file() -> Path:
    return xdg_config_home() / "gtk-4.0" / "settings.ini"


def gtk3_user_file() -> Path:
    return xdg_config_home() / "gtk-3.0" / "gtk.css"


def gtk4_user_file() -> Path:
    return xdg_config_home() / "gtk-4.0" / "gtk.css"


def extras_dir() -> Path:
    return xdg_config_home() / CONFIG_DIR_NAME


# --- file helpers ------------------------------------------------------------


def copy_tree(source: Path, destination: Path) -> int:
    """Copy everything under ``source`` into ``destination``, returning a count."""
    if not source.is_dir():
        raise SystemExit(f"error: theme source is missing: {source}")
    written = 0
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if any(part in IGNORED_COPY_NAMES for part in relative.parts):
            continue
        target = destination / relative
        if path.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        written += 1
    return written


def backup_file(path: Path) -> Path | None:
    """Move an existing file aside, returning where it went.

    The backup is written once, not once per run: a second run must not
    overwrite it with this installer's own output, because the file worth
    keeping is the one the user had before the script ever touched it. Keeping
    the oldest copy is what makes the promise in the header true however many
    times the script is run.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.move(str(path), str(backup))
    return backup


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


# --- generated configuration -------------------------------------------------


# Keys the installer owns. Anything else in the user's file is left alone.
#
# gtk-theme-name only: no gtk-application-prefer-dark-theme. That key is what
# makes GTK 3 load <theme>/gtk-3.0/gtk-dark.css instead of gtk.css, and this
# theme's gtk-dark.css is a forwarder whose relative @import is one more thing
# that has to resolve before any colour at all applies. The theme has no light
# variant - gtk-3.0/gtk.css is dark, and the GTK 4 overlay defines its colours
# unconditionally - so plain gtk-theme-name is both simpler and one failure
# mode shorter.
SETTINGS_INI_KEYS: dict[str, str] = {
    "gtk-theme-name": THEME_NAME,
}


def read_existing(path: Path) -> str:
    """Contents of a configuration file, or an empty string if there is none."""
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def _generated_header() -> str:
    return f"# GnuchanPurple - {GENERATED_MARKER}\n"


def merge_ini(existing: str, section: str, managed: dict[str, str]) -> str:
    """Set ``managed`` keys inside ``section``, keeping every other line.

    The installer used to rewrite settings.ini with its own two keys and
    nothing else, which silently dropped the user's ``gtk-font-name``,
    ``gtk-icon-theme-name``, ``gtk-cursor-theme-name`` and
    ``gtk-decoration-layout``. Here the file is walked line by line and only
    the managed keys are touched: comments, blank lines, key order and
    unrelated sections survive, missing keys are appended to the section, and
    a section that does not exist yet is created at the end.
    """
    managed_by_lower = {key.lower(): key for key in managed}
    pending = list(managed)
    written: set[str] = set()

    def append_pending(block: list[str]) -> None:
        while pending:
            key = pending.pop(0)
            block.append(f"{key}={managed[key]}")

    body: list[str] = []
    current_section: str | None = None
    section_found = False

    for line in existing.splitlines():
        if GENERATED_MARKER in line:
            continue  # drop the header a previous run wrote
        stripped = line.strip()
        in_our_section = (
            current_section is not None and current_section.lower() == section.lower()
        )
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_our_section:
                append_pending(body)
            current_section = stripped[1:-1].strip()
            if current_section.lower() == section.lower():
                section_found = True
            body.append(line)
            continue
        if in_our_section and "=" in stripped:
            key = stripped.split("=", 1)[0].strip()
            canonical = managed_by_lower.get(key.lower())
            if canonical is not None:
                if canonical in written:
                    continue  # a duplicate of a key we already set
                written.add(canonical)
                body.append(f"{canonical}={managed[canonical]}")
                if canonical in pending:
                    pending.remove(canonical)
                continue
        body.append(line)

    if current_section is not None and current_section.lower() == section.lower():
        append_pending(body)

    if not section_found:
        if body and body[-1].strip():
            body.append("")
        body.append(f"[{section}]")
        append_pending(body)

    text = "\n".join(body).strip("\n")
    return _generated_header() + (text + "\n" if text else "")


def settings_ini_content(path: Path) -> str:
    """Body of a GTK 3 or GTK 4 ``settings.ini``, merged with what was there."""
    return merge_ini(read_existing(path), "Settings", SETTINGS_INI_KEYS)


# The line that tells the user how to undo this file. It has to be recognised
# on the next run like any other line of ours, or every install appends another
# copy of it and the file grows without ever changing meaning.
GTK2_OWNERSHIP_COMMENT = "# Remove the lines below to fall back to the default GTK 2 theme."


def gtk2_rc_content(theme_dir: Path, include_theme: bool = True) -> str:
    """Body of ``~/.gtkrc-2.0``, merged with what was there.

    ``gtk-theme-name`` is what GTK 2 selects on; the explicit include is a
    fallback for setups whose theme search path does not cover the install
    directory. The include is only emitted when the theme tree is actually
    installed, so ``--no-theme`` no longer writes an include pointing at a
    directory that was never created.
    """
    theme_line = f'gtk-theme-name = "{THEME_NAME}"'
    include_line = (
        f'include "{(theme_dir / "gtk-2.0" / "gtkrc").as_posix()}"'
        if include_theme
        else None
    )

    kept: list[str] = []
    for line in read_existing(gtk2_rc_file()).splitlines():
        if GENERATED_MARKER in line:
            continue
        stripped = line.strip()
        if re.match(r"^\s*gtk-theme-name\s*=", stripped):
            continue
        if stripped.startswith("include") and THEME_NAME in stripped:
            continue
        if stripped == GTK2_OWNERSHIP_COMMENT:
            continue
        kept.append(line)

    while kept and not kept[-1].strip():
        kept.pop()
    if kept and kept[-1].strip():
        kept.append("")
    kept.append(GTK2_OWNERSHIP_COMMENT)
    kept.append(theme_line)
    if include_line is not None:
        kept.append(include_line)

    return _generated_header() + "\n".join(kept).strip("\n") + "\n"


def gtk4_overlay_content() -> str:
    """Body of ``~/.config/gtk-4.0/gtk.css``, the theme plus a provenance header."""
    header = (
        f"/* GnuchanPurple - {GENERATED_MARKER}\n"
        f" * Installed for theme: {THEME_NAME}\n"
        " * Remove this file to fall back to the stock libadwaita palette.\n"
        " */\n\n"
    )
    return header + SOURCE_THEME_OVERLAY.read_text(encoding="utf-8")


def gtk3_overlay_content() -> str:
    """Body of ``~/.config/gtk-3.0/gtk.css``, the theme plus a provenance header.

    This is the GTK 3 counterpart of the libadwaita overlay above, and it is
    what makes the palette independent of theme selection: a user stylesheet is
    loaded at user priority by every GTK 3 application, whether or not this
    theme is the one the session selected.
    """
    header = (
        f"/* GnuchanPurple - {GENERATED_MARKER}\n"
        f" * Installed for theme: {THEME_NAME}\n"
        " * Remove this file to fall back to the stock theme's colours.\n"
        " */\n\n"
    )
    return header + SOURCE_THEME_OVERLAY_GTK3.read_text(encoding="utf-8")


# --- logging -----------------------------------------------------------------


class Log:
    """Progress output, honouring --quiet."""

    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet

    def step(self, message: str) -> None:
        if not self.quiet:
            print(f"==> {message}")

    def detail(self, message: str) -> None:
        if not self.quiet:
            print(f"    {message}")

    def note(self, message: str) -> None:
        if not self.quiet:
            print(message)


# --- installing --------------------------------------------------------------


def link_theme_dir(target: Path, primary: Path) -> bool:
    """Make ``target`` point at ``primary``, reporting whether it worked.

    Nothing is destroyed: a real directory that is not one of our own symlinks
    is left in place and the caller falls back to copying into it.
    """
    if target.is_symlink():
        if Path(os.readlink(target)) == primary:
            return True
        target.unlink()
    elif target.exists():
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        target.symlink_to(primary, target_is_directory=True)
    except OSError:
        return False
    return True


def materialise_dark_variants(log: Log, target: Path) -> None:
    """Make each gtk-dark.css a real copy of that toolkit's gtk.css.

    In the source tree gtk-dark.css is a one line ``@import url("gtk.css")``,
    which keeps the two files from drifting apart. In the *installed* tree that
    indirection is a liability: GTK 3.20 and newer load gtk-dark.css whenever an
    application asks for the dark variant (or whenever GTK_THEME ends in
    ":dark"), and if that relative import does not resolve the application gets
    no stylesheet from this theme at all - it keeps the toolkit default, which
    is the grey that makes the theme look as if it had not been installed. GTK
    reports nothing louder than a warning when an import fails.

    Copying the maintained file over the variant removes that failure mode
    completely, and costs one duplicated file per toolkit in the installed tree
    only - the repository keeps the forwarder and its single source of truth.
    """
    for version in ("gtk-3.0", "gtk-4.0"):
        source = target / version / "gtk.css"
        variant = target / version / "gtk-dark.css"
        if not source.is_file():
            continue
        shutil.copy2(source, variant)
        log.detail(f"copied {version}/gtk.css over {version}/gtk-dark.css")


def write_theme_tree(log: Log, target: Path, render_assets: bool) -> None:
    """Copy the theme tree into ``target``, render its images, fill the variants."""
    copied = copy_tree(SOURCE_THEME_DIR, target)
    log.detail(f"copied {copied} files into {target}")
    materialise_dark_variants(log, target)
    if not render_assets:
        return
    count = render_xfwm4_assets(target)
    render_thumbnail().save(target / "thumbnail.png")
    log.detail(f"rendered {count} xfwm4 images into {target / 'xfwm4'}")
    log.detail(f"rendered the theme preview into {target / 'thumbnail.png'}")


def render_source_assets(log: Log) -> int:
    """Render the decoration images into the source tree in this repository.

    This is what fills GnuchanPurple/xfwm4 with the 74 tiles xfwm4 composites
    and GnuchanPurple/thumbnail.png with the preview, so that a user who copies
    the directory into ~/.themes by hand gets a working theme without running
    the installer. It writes only inside the repository.
    """
    count = render_xfwm4_assets(SOURCE_THEME_DIR)
    thumbnail = SOURCE_THEME_DIR / "thumbnail.png"
    render_thumbnail().save(thumbnail)
    log.detail(f"rendered {count} xfwm4 images into {SOURCE_THEME_DIR / 'xfwm4'}")
    log.detail(f"rendered the theme preview into {thumbnail}")
    return count


def install_theme(log: Log, theme_dirs: tuple[Path, ...], render_assets: bool) -> list[Path]:
    """Install the theme and link every directory after the first to it.

    ``user_theme_dirs()`` returns two locations because GTK 4 prefers
    ``$XDG_DATA_HOME/themes`` while GTK 2, xfwm4 and most window managers still
    read ``~/.themes``. Two independent copies drift apart the moment one of
    them is edited, so the second location becomes a symlink to the first;
    where symlinks are unavailable the theme is copied instead.
    """
    installed: list[Path] = []
    primary = (theme_dirs[0] / THEME_NAME) if theme_dirs else None
    for directory in theme_dirs:
        target = directory / THEME_NAME
        if primary is not None and target != primary:
            if link_theme_dir(target, primary):
                log.detail(f"linked {target} -> {primary}")
            else:
                log.detail(f"{target} is already a real directory; filling it instead")
                write_theme_tree(log, target, render_assets)
            installed.append(target)
            continue
        write_theme_tree(log, target, render_assets)
        installed.append(target)
    return installed


def write_config(
    log: Log, path: Path, content: str, merge: bool = False
) -> None:
    """Write one configuration file, backing up whatever was already there.

    There is no mode that refuses to write: the installer is meant to be run,
    and running it has to leave a current configuration behind. What protects a
    file this installer does not own is the merge in the callers - those files
    keep every line the installer does not set - and the ``.gnuchan-backup``
    copy taken here before anything is replaced.
    """
    backup = backup_file(path)
    write_text(path, content)
    if backup is not None:
        log.detail(f"backed up {path.name} to {backup.name}")
    log.detail(f"merged {path}" if merge else f"wrote {path}")


def apply_theme(log: Log) -> None:
    """Ask the session's own settings backend to select this theme.

    Writing ``~/.config/gtk-3.0/settings.ini`` is enough only on a session
    where no settings daemon is running. As soon as one is - xfsettingsd on
    Xfce, gnome-settings-daemon, mate-settings-daemon, lxsettings-daemon,
    xsettingsd - it owns ``gtk-theme-name`` through XSettings and its value
    overrides the file. A theme that is installed but never selected there is
    never loaded: every widget keeps the toolkit's own default, which is the
    grey that makes a finished installation look as if nothing had happened.

    xfconf and gsettings are therefore asked directly when they exist. Both are
    optional: a bare window manager session has neither, and then the
    settings.ini the caller already wrote is what applies.
    """
    xfconf = shutil.which("xfconf-query")
    if xfconf is not None:
        for prop in ("/Net/ThemeName", "/Net/IconThemeName"):
            subprocess.run(
                [xfconf, "-c", "xsettings", "-p", prop, "-s", THEME_NAME],
                check=False,
            )
        log.detail("selected the theme through xfconf (Xfce XSettings)")
    gsettings = shutil.which("gsettings")
    if gsettings is not None:
        for schema, key in (
            ("org.gnome.desktop.interface", "gtk-theme"),
            ("org.gnome.desktop.interface", "icon-theme"),
        ):
            subprocess.run([gsettings, "set", schema, key, THEME_NAME], check=False)
        log.detail("selected the theme through gsettings")
    if xfconf is None and gsettings is None:
        log.detail("no settings backend found; pick the theme in your appearance settings")


def install_configuration(
    log: Log, theme_dir: Path, settings_ini: bool, include_theme: bool
) -> None:
    """Install the files GTK 2, GTK 4 and libadwaita read from the home directory.

    The GTK 2 rc file and the settings.ini files are merged rather than
    replaced, so keys the user already set - fonts, icon theme, cursor theme,
    decoration layout - survive the install untouched.
    """
    if not SOURCE_THEME_OVERLAY.is_file():
        raise SystemExit(f"error: missing GTK 4 overlay: {SOURCE_THEME_OVERLAY}")
    write_config(
        log,
        gtk2_rc_file(),
        gtk2_rc_content(theme_dir, include_theme=include_theme),
        merge=True,
    )
    write_config(log, gtk3_user_file(), gtk3_overlay_content())
    write_config(log, gtk4_user_file(), gtk4_overlay_content())
    if settings_ini:
        for path in (gtk3_settings_file(), gtk4_settings_file()):
            write_config(log, path, settings_ini_content(path), merge=True)
    apply_theme(log)


def install_extras(log: Log) -> None:
    """Install the terminal, launcher and bar colour files."""
    if not SOURCE_EXTRAS_DIR.is_dir():
        raise SystemExit(f"error: missing extras directory: {SOURCE_EXTRAS_DIR}")
    target = extras_dir()
    copied = copy_tree(SOURCE_EXTRAS_DIR, target)
    log.detail(f"installed {copied} extras into {target}")


# --- uninstalling ------------------------------------------------------------
# Everything below undoes what the installer wrote. It deliberately never looks
# at SOURCE_THEME_DIR: a user uninstalls a theme from a machine where this
# repository may no longer exist at all, so requiring the source tree to be
# present would make --uninstall impossible exactly when it is needed.


def looks_like_our_theme(path: Path) -> bool:
    """Whether ``path`` holds this theme, and is therefore safe to delete.

    --uninstall removes a whole directory tree. Before it does, it checks that
    the directory carries one of this theme's own files with the theme name in
    it, so a directory the user has since replaced by hand is left alone and
    reported instead of being deleted.
    """
    if not path.is_dir():
        return False
    for marker in (
        path / "index.theme",
        path / "gtk-3.0" / "gtk.css",
        path / "gtk-2.0" / "gtkrc",
    ):
        if marker.is_file():
            try:
                if THEME_NAME in marker.read_text(encoding="utf-8"):
                    return True
            except OSError:
                continue
    return False


def remove_theme_dirs(log: Log, theme_dirs: tuple[Path, ...]) -> None:
    """Remove the installed theme from every theme directory."""
    for directory in theme_dirs:
        target = directory / THEME_NAME
        if target.is_symlink():
            target.unlink()
            log.detail(f"removed the symlink {target}")
            continue
        if not target.exists():
            log.detail(f"{target} is not installed")
            continue
        if not looks_like_our_theme(target):
            log.detail(f"kept {target}: it does not look like {THEME_NAME}")
            continue
        shutil.rmtree(target)
        log.detail(f"removed {target}")


def restore_backup(path: Path) -> bool:
    """Put the ``.gnuchan-backup`` copy of ``path`` back, if there is one."""
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if not backup.exists():
        return False
    if path.exists():
        path.unlink()
    shutil.move(str(backup), str(path))
    return True


def strip_ini_keys(existing: str, section: str, keys: tuple[str, ...]) -> str:
    """Remove ``keys`` from ``section``, keeping every other line.

    The counterpart of ``merge_ini``: a fresh install that had no backup to
    restore is undone by taking the installer's own keys back out, so the file
    keeps whatever the user has added since.
    """
    wanted = {key.lower() for key in keys}
    body: list[str] = []
    current_section: str | None = None
    for line in existing.splitlines():
        if GENERATED_MARKER in line:
            continue
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            current_section = stripped[1:-1].strip()
            body.append(line)
            continue
        if (
            current_section is not None
            and current_section.lower() == section.lower()
            and "=" in stripped
            and stripped.split("=", 1)[0].strip().lower() in wanted
        ):
            continue
        body.append(line)
    return "\n".join(body).strip("\n")


def strip_gtk2_rc(existing: str) -> str:
    """Remove this installer's lines from a ``~/.gtkrc-2.0``."""
    kept: list[str] = []
    for line in existing.splitlines():
        if GENERATED_MARKER in line:
            continue
        stripped = line.strip()
        if re.match(r"^\s*gtk-theme-name\s*=", stripped):
            continue
        if stripped.startswith("include") and THEME_NAME in stripped:
            continue
        if stripped == GTK2_OWNERSHIP_COMMENT:
            continue
        kept.append(line)
    return "\n".join(kept).strip("\n")


def remove_generated_file(log: Log, path: Path, content: str) -> None:
    """Delete a file this installer wrote, once its own lines are taken out."""
    if not path.exists():
        return
    if content.strip():
        write_text(path, content.rstrip("\n") + "\n")
        log.detail(f"removed our keys from {path}")
        return
    path.unlink()
    log.detail(f"removed {path}")


def uninstall_configuration(log: Log) -> None:
    """Undo ~/.gtkrc-2.0, the settings.ini files and the GTK 4 overlay.

    Where a ``.gnuchan-backup`` copy exists it is put back, which is what
    actually restores the user's own file; otherwise only this installer's
    lines are removed and a file left empty is deleted. The GTK 4 overlay is
    deleted only when it still carries this installer's provenance header, so a
    user stylesheet that was edited by hand is never thrown away.
    """
    for path, section, keys, strip in (
        (
            gtk3_settings_file(),
            "Settings",
            tuple(SETTINGS_INI_KEYS),
            strip_ini_keys,
        ),
        (
            gtk4_settings_file(),
            "Settings",
            tuple(SETTINGS_INI_KEYS),
            strip_ini_keys,
        ),
    ):
        if restore_backup(path):
            log.detail(f"restored {path} from its backup")
            continue
        if path.exists():
            remove_generated_file(log, path, strip(read_existing(path), section, keys))

    rc_file = gtk2_rc_file()
    if restore_backup(rc_file):
        log.detail(f"restored {rc_file} from its backup")
    elif rc_file.exists():
        remove_generated_file(log, rc_file, strip_gtk2_rc(read_existing(rc_file)))

    for overlay in (gtk3_user_file(), gtk4_user_file()):
        if not overlay.exists():
            continue
        if GENERATED_MARKER in read_existing(overlay):
            overlay.unlink()
            log.detail(f"removed {overlay}")
        else:
            log.detail(f"kept {overlay}: it is not the file this installer wrote")


def remove_extras(log: Log) -> None:
    """Remove the terminal and bar colour files, if they are still ours."""
    target = extras_dir()
    if not target.is_dir():
        return
    shutil.rmtree(target)
    log.detail(f"removed {target}")


# --- checking the environment ------------------------------------------------
# Two problems the theme cannot fix from inside its own files, both of them
# diagnosed from the source of the tool that reports them.
#
# 1. lxappearance sorts icon and cursor themes by their localised Name with
#    g_utf8_collate (lxappearance/src/icon-theme.c). A theme whose index.theme
#    exists but has no Name - or is Hidden=true without one - leaves that field
#    NULL, and GLib then prints
#
#        g_utf8_collate: assertion 'str1 != NULL' failed
#
#    once per comparison, four times in the reported session. The offending
#    theme is not this one; --check finds it. It is also why the icon theme
#    GnuchanOS generates writes Name= unconditionally: see
#    dotfile/ICON_THEME/icon_install.py.
#
# 2. GtkFontButton builds a one line stylesheet from the current font
#    description and loads it with gtk_css_provider_load_from_data
#    (gtk/gtkfontbutton.c, pango_font_description_to_css). The family name is
#    inserted without quotes, so an empty or unparsable gtk-font-name makes GTK
#    report
#
#        Theme parsing error: <data>:1:17: Expected a string.
#
#    The installer now merges rather than replaces settings.ini, so it no
#    longer drops that key. Where the key survives in a .gnuchan-backup but not
#    in the live file, an older run did the damage and the backup has to go
#    back; --check says so instead of leaving the user to find it.


def theme_search_dirs() -> list[Path]:
    """Where icon and cursor themes live, user level first.

    Mirrors the two lists lxappearance and GTK both walk: the user locations
    under $HOME, then the system ones from $XDG_DATA_DIRS.
    """
    return [
        home_dir() / ".icons",
        xdg_data_home() / "icons",
        Path("/usr/local/share/icons"),
        Path("/usr/share/icons"),
    ]


def read_ini_key(path: Path, section: str, key: str) -> str | None:
    """Value of ``key`` inside ``section`` of an INI file, or None.

    Deliberately small: the theme files are read here without configparser so
    that a stray duplicate key or an unparseable line in someone else's theme
    cannot raise and stop the check halfway.
    """
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return None
    current: str | None = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith(";"):
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            current = stripped[1:-1].strip()
            continue
        if current is None or current.lower() != section.lower():
            continue
        name, separator, value = stripped.partition("=")
        if separator and name.strip().lower() == key.lower():
            return value.strip()
    return None


def nameless_theme_problems() -> list[str]:
    """Icon and cursor themes whose index.theme carries no Name.

    A directory only counts as a theme for lxappearance when it has a cursors
    subdirectory or an index.theme that lists Directories, so the same test is
    applied here: a stray index.theme without a Name that no list would ever
    show is not worth reporting.
    """
    problems: list[str] = []
    for root in theme_search_dirs():
        if not root.is_dir():
            continue
        try:
            entries = sorted(root.iterdir())
        except OSError:
            continue
        for entry in entries:
            index = entry / "index.theme"
            if not index.is_file():
                continue
            has_directories = bool(read_ini_key(index, "Icon Theme", "Directories"))
            has_cursors = (entry / "cursors").is_dir()
            if not (has_directories or has_cursors):
                continue
            name = read_ini_key(index, "Icon Theme", "Name") or read_ini_key(
                index, "Cursor Theme", "Name"
            )
            if not name:
                problems.append(
                    f"{entry}: index.theme has no Name (lxappearance prints "
                    f"g_utf8_collate assertions for it)"
                )
    return problems


FONT_KEY_RE = re.compile(r"^\s*gtk[-_]font[-_]name\s*=", re.MULTILINE)


def font_name_problems() -> list[str]:
    """Files where a backup holds gtk-font-name but the live file does not."""
    problems: list[str] = []
    for path in (gtk2_rc_file(), gtk3_settings_file(), gtk4_settings_file()):
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        live = read_existing(path)
        old = read_existing(backup)
        if FONT_KEY_RE.search(old) and not FONT_KEY_RE.search(live):
            problems.append(
                f"{path}: gtk-font-name is in {backup.name} but not in the file "
                f"in use; put the backup back (GtkFontButton reports "
                f"'Expected a string' without it)"
            )
    return problems


# The files whose contents decide what the theme looks like on screen.
APPEARANCE_FILES = (
    "gtk-2.0/gtkrc",
    "gtk-3.0/gtk.css",
    "gtk-3.0/gtk-dark.css",
    "gtk-4.0/gtk.css",
    "gtk-4.0/user.css",
    "xfce-notify-4.0/gtk.css",
    "metacity-1/metacity-theme-3.xml",
    "openbox-3/themerc",
)


def _file_crc(path: Path) -> int | None:
    try:
        return zlib.crc32(path.read_bytes()) & 0xFFFFFFFF
    except OSError:
        return None


def stale_copy_problems() -> list[str]:
    """Installed theme copies that no longer match the source tree.

    The installer *copies* GnuchanPurple/ into the theme directories, which is
    what makes the theme work without it, but it also means editing the
    directory in this repository changes nothing on screen: GTK keeps loading
    the installed copy until the installer runs again. A fix that "did not
    work" is very often this - the installed stylesheet is still the old one.
    Comparing the files is therefore the first check worth running.
    """
    problems: list[str] = []
    for name in APPEARANCE_FILES:
        source = SOURCE_THEME_DIR / name
        if not source.is_file():
            continue
        want = _file_crc(source)
        for directory in user_theme_dirs():
            installed = directory / THEME_NAME / name
            if not installed.is_file():
                continue
            if _file_crc(installed) != want:
                problems.append(
                    f"{installed} differs from {source}; run the installer again "
                    f"to update the installed copy"
                )
    return problems


def settings_theme_problems() -> list[str]:
    """Settings files that do not select this theme, or do not exist.

    lxappearance sets the theme through XSettings or through these files, and a
    session without an XSettings daemon reads only the files, so a theme that is
    installed but never named here stays unapplied and everything keeps
    Adwaita's colours.
    """
    problems: list[str] = []
    rc_file = gtk2_rc_file()
    if rc_file.is_file() and THEME_NAME not in read_existing(rc_file):
        problems.append(f"{rc_file} does not name {THEME_NAME}")
    for path in (gtk3_settings_file(), gtk4_settings_file()):
        if not path.is_file():
            problems.append(f"{path} is missing; GTK 3/4 will use the default theme")
            continue
        text = read_existing(path)
        if f"gtk-theme-name={THEME_NAME}" not in text.replace(" ", ""):
            problems.append(f"{path} does not select {THEME_NAME}")
    return problems


def check_environment(log: Log) -> int:
    """Print the environment problems found, returning how many there are."""
    problems = (
        nameless_theme_problems()
        + font_name_problems()
        + stale_copy_problems()
        + settings_theme_problems()
    )
    if not problems:
        log.note("No problems found.")
        return 0
    log.note(f"{len(problems)} problem(s) found:")
    for problem in problems:
        log.note(f"  {problem}")
    return len(problems)


# --- entry point -------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="theme_install.py",
        description="Install the GnuchanPurple theme for GTK 2/3/4 and the window managers.",
    )
    parser.add_argument(
        "--theme-dir",
        action="append",
        metavar="DIR",
        help="themes directory to install into; repeatable "
        "(default: the XDG data directory and ~/.themes)",
    )
    parser.add_argument("--no-theme", action="store_true", help="do not copy the theme tree")
    parser.add_argument(
        "--no-assets", action="store_true", help="do not render the xfwm4 decoration images"
    )
    parser.add_argument(
        "--no-config", action="store_true", help="do not write ~/.gtkrc-2.0 or the GTK 4 overlay"
    )
    parser.add_argument(
        "--no-extras", action="store_true", help="do not install the terminal and bar extras"
    )
    parser.add_argument(
        "--no-settings",
        action="store_true",
        help="do not write the GTK 3 and GTK 4 settings.ini files",
    )
    parser.add_argument(
        "--render-assets",
        action="store_true",
        help="render the xfwm4 images and the theme preview into the theme "
        "source in this repository, then stop",
    )
    parser.add_argument(
        "--uninstall",
        action="store_true",
        help="remove the installed theme, restore the settings files from "
        "their backups and delete the extras, then stop",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="report the environment problems that break the theme's look "
        "(a theme whose index.theme has no Name, a gtk-font-name an older run "
        "dropped), then stop",
    )
    parser.add_argument("--quiet", action="store_true", help="only print problems")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    log = Log(quiet=args.quiet)

    if args.theme_dir:
        theme_dirs = tuple(Path(value).expanduser() for value in args.theme_dir)
    else:
        theme_dirs = tuple(user_theme_dirs())

    # --uninstall and --check run without the theme source on purpose: they are
    # how a user repairs or removes the theme on a machine where this
    # repository is no longer present, so they must not be gated on it.
    if args.uninstall:
        log.step(f"Removing {THEME_NAME}")
        remove_theme_dirs(log, theme_dirs)
        uninstall_configuration(log)
        remove_extras(log)
        log.note("")
        log.note(f"{THEME_NAME} removed. Log out and back in to fall back to")
        log.note("the default theme.")
        return 0

    if args.check:
        log.step(f"Checking the {THEME_NAME} environment")
        return 1 if check_environment(log) else 0

    if not SOURCE_THEME_DIR.is_dir():
        print(f"error: theme source is missing: {SOURCE_THEME_DIR}", file=sys.stderr)
        return 1

    if args.render_assets:
        log.step(f"Rendering the {THEME_NAME} assets")
        render_source_assets(log)
        log.note("")
        log.note(f"Done. {SOURCE_THEME_DIR / 'xfwm4'} and")
        log.note(f"{SOURCE_THEME_DIR / 'thumbnail.png'} now hold the rendered images.")
        return 0

    log.step(f"Installing {THEME_NAME}")
    if not args.no_theme:
        install_theme(log, theme_dirs, render_assets=not args.no_assets)
    if not args.no_config:
        install_configuration(
            log,
            theme_dirs[0] / THEME_NAME,
            settings_ini=not args.no_settings,
            include_theme=not args.no_theme,
        )
    if not args.no_extras:
        install_extras(log)

    log.note("")
    log.note(f"{THEME_NAME} installed into:")
    for directory in theme_dirs:
        log.note(f"  {directory / THEME_NAME}")
    log.note("")
    log.note("Log out and back in to make it active.")
    log.note("")
    log.note("Window managers with no XSettings daemon - i3, sway, Openbox,")
    log.note("bspwm - do not read the settings files above. For those, add")
    log.note("this line to ~/.profile (or ~/.xprofile) and log back in:")
    log.note("")
    log.note(f"  . {extras_dir() / 'gtk-env.sh'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
