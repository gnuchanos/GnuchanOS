#!/usr/bin/env python3
# =============================================================================
# GnuchanPurple - theme installer
# -----------------------------------------------------------------------------
# Single file installer for the theme in ./gnuchan_theme. No third party
# modules: everything it needs, including the PNG writer xfwm4 requires, is in
# this file.
#
# What it does
# ------------
#   1. copies gnuchan_theme/ into the user theme directories GTK 2, GTK 3,
#      GTK 4, xfwm4, Marco/Metacity and Openbox read;
#   2. renders the xfwm4 decoration images (title bar, borders and every title
#      button) plus the preview that appearance settings show, straight into
#      the installed theme, because xfwm4 composites PNGs and the theme itself
#      ships no binaries;
#   3. installs the GTK 4 / libadwaita overlay as ~/.config/gtk-4.0/gtk.css,
#      which is how libadwaita applications get the palette without GTK_THEME;
#   4. writes a ~/.gtkrc-2.0 that selects the theme for GTK 2 applications;
#   5. copies the terminal / launcher / bar extras to ~/.config/gnuchan-purple.
#
# Existing files are moved aside to <name>.gnuchan-backup instead of being
# overwritten silently, and --uninstall puts them back.
#
#     python theme_install.py              # install
#     python theme_install.py --apply      # install and select it
#     python theme_install.py --dry-run    # show the plan, change nothing
#     python theme_install.py --uninstall  # undo
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path
from typing import Callable

RGBA = tuple[int, int, int, int]

# --- locations ---------------------------------------------------------------

THEME_NAME = "GnuchanPurple"
CONFIG_DIR_NAME = "gnuchan-purple"
BACKUP_SUFFIX = ".gnuchan-backup"
GENERATED_MARKER = "written by theme_install.py"
IGNORED_COPY_NAMES = {"__pycache__", ".DS_Store", "Thumbs.db"}

SCRIPT_DIR = Path(__file__).resolve().parent
SOURCE_THEME_DIR = SCRIPT_DIR / "gnuchan_theme"
SOURCE_THEME_OVERLAY = SOURCE_THEME_DIR / "gtk-4.0" / "user.css"
SOURCE_EXTRAS_DIR = SOURCE_THEME_DIR / "extras"

# --- palette -----------------------------------------------------------------
# Byte for byte the colours used by gtk-2.0/gtkrc, gtk-3.0/gtk.css and
# gtk-4.0/gtk.css, so every toolkit ends up with the same purple.

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
# so these are rendered at install time instead of being shipped in git.

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
    """Move an existing file aside, returning where it went."""
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        backup.unlink()
    shutil.move(str(path), str(backup))
    return backup


def restore_backup(path: Path) -> bool:
    """Put a backup back in place, reporting whether one was found."""
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if not backup.exists():
        return False
    if path.exists():
        path.unlink()
    shutil.move(str(backup), str(path))
    return True


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def is_generated_file(path: Path) -> bool:
    """True when a file on disk was written by this installer."""
    try:
        return GENERATED_MARKER in path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return False


# --- generated configuration -------------------------------------------------


def gtk2_rc_content(theme_dir: Path) -> str:
    """Body of ``~/.gtkrc-2.0``.

    The theme name is what GTK 2 selects on; the explicit include is a fallback
    for setups whose theme search path does not cover the install location.
    """
    included = (theme_dir / "gtk-2.0" / "gtkrc").as_posix()
    return (
        f"# GnuchanPurple - {GENERATED_MARKER}\n"
        "# Remove this file to fall back to the default GTK 2 theme.\n"
        "\n"
        f'gtk-theme-name = "{THEME_NAME}"\n'
        "\n"
        f'include "{included}"\n'
    )


def settings_ini_content() -> str:
    """Body of a GTK 3 or GTK 4 ``settings.ini``."""
    return (
        f"# GnuchanPurple - {GENERATED_MARKER}\n"
        "[Settings]\n"
        f"gtk-theme-name={THEME_NAME}\n"
        "gtk-application-prefer-dark-theme=1\n"
    )


def gtk4_overlay_content() -> str:
    """Body of ``~/.config/gtk-4.0/gtk.css``, the theme plus a provenance header."""
    header = (
        f"/* GnuchanPurple - {GENERATED_MARKER}\n"
        f" * Installed for theme: {THEME_NAME}\n"
        " * Remove this file to fall back to the stock libadwaita palette.\n"
        " */\n\n"
    )
    return header + SOURCE_THEME_OVERLAY.read_text(encoding="utf-8")


# --- logging -----------------------------------------------------------------


class Log:
    """Progress output, honouring --quiet and --dry-run."""

    def __init__(self, quiet: bool = False, dry_run: bool = False) -> None:
        self.quiet = quiet
        self.dry_run = dry_run

    def step(self, message: str) -> None:
        if not self.quiet:
            print(f"==> {message}")

    def detail(self, message: str) -> None:
        if not self.quiet:
            print(f"    {'would ' if self.dry_run else ''}{message}")

    def note(self, message: str) -> None:
        if not self.quiet:
            print(message)

    def warn(self, message: str) -> None:
        print(f"warning: {message}", flush=True)


# --- installing --------------------------------------------------------------


def install_theme(log: Log, theme_dirs: tuple[Path, ...], render_assets: bool) -> list[Path]:
    """Copy the theme into every target directory and render its xfwm4 images."""
    installed: list[Path] = []
    for directory in theme_dirs:
        target = directory / THEME_NAME
        if log.dry_run:
            log.detail(f"copy {SOURCE_THEME_DIR} -> {target}")
        else:
            copied = copy_tree(SOURCE_THEME_DIR, target)
            log.detail(f"copied {copied} files into {target}")
        if render_assets:
            if log.dry_run:
                log.detail(f"render the xfwm4 images and the theme preview into {target}")
            else:
                count = render_xfwm4_assets(target)
                render_thumbnail().save(target / "thumbnail.png")
                log.detail(f"rendered {count} xfwm4 images into {target / 'xfwm4'}")
                log.detail(f"rendered the theme preview into {target / 'thumbnail.png'}")
        installed.append(target)
    return installed


def write_config(log: Log, path: Path, content: str, force: bool) -> None:
    """Write one configuration file, backing up whatever was already there.

    A file this installer did not write is left alone unless --force is given,
    so a hand made configuration is never replaced by surprise.
    """
    if path.exists() and not is_generated_file(path) and not force:
        log.warn(
            f"{path} was not written by this installer; leaving it alone "
            "(pass --force to replace it)"
        )
        return
    if log.dry_run:
        log.detail(f"write {path}")
        return
    backup = backup_file(path)
    write_text(path, content)
    if backup is not None:
        log.detail(f"backed up {path.name} to {backup.name}")
    log.detail(f"wrote {path}")


def install_configuration(log: Log, theme_dir: Path, force: bool, settings_ini: bool) -> None:
    """Install the files GTK 2, GTK 4 and libadwaita read from the home directory."""
    if not SOURCE_THEME_OVERLAY.is_file():
        raise SystemExit(f"error: missing GTK 4 overlay: {SOURCE_THEME_OVERLAY}")
    write_config(log, gtk2_rc_file(), gtk2_rc_content(theme_dir), force)
    write_config(log, gtk4_user_file(), gtk4_overlay_content(), force)
    if settings_ini:
        for path in (gtk3_settings_file(), gtk4_settings_file()):
            write_config(log, path, settings_ini_content(), force)


def install_extras(log: Log) -> None:
    """Install the terminal, launcher and bar colour files."""
    if not SOURCE_EXTRAS_DIR.is_dir():
        raise SystemExit(f"error: missing extras directory: {SOURCE_EXTRAS_DIR}")
    target = extras_dir()
    if log.dry_run:
        log.detail(f"copy {SOURCE_EXTRAS_DIR} -> {target}")
        return
    copied = copy_tree(SOURCE_EXTRAS_DIR, target)
    log.detail(f"installed {copied} extras into {target}")


# --- making the theme active -------------------------------------------------
# Every desktop answers to a different tool, so the list covers the common ones
# and each entry is skipped when its tool is not installed.

APPLY_COMMANDS: tuple[tuple[str, ...], ...] = (
    ("gsettings", "set", "org.gnome.desktop.interface", "gtk-theme", THEME_NAME),
    ("gsettings", "set", "org.gnome.desktop.wm.preferences", "theme", THEME_NAME),
    ("gsettings", "set", "org.mate.interface", "gtk-theme", THEME_NAME),
    ("gsettings", "set", "org.mate.Marco.general", "theme", THEME_NAME),
    ("gsettings", "set", "org.cinnamon.desktop.interface", "gtk-theme", THEME_NAME),
    ("gsettings", "set", "org.cinnamon.theme", "name", THEME_NAME),
    ("xfconf-query", "-c", "xsettings", "-p", "/Net/ThemeName", "-n", "-t", "string", "-s", THEME_NAME),
    ("xfconf-query", "-c", "xfwm4", "-p", "/general/theme", "-n", "-t", "string", "-s", THEME_NAME),
)


def run_command(log: Log, argv: tuple[str, ...]) -> None:
    if shutil.which(argv[0]) is None:
        log.detail(f"skipped, {argv[0]} is not installed")
        return
    completed = subprocess.run(list(argv), capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        output = (completed.stderr or completed.stdout).strip().splitlines()
        log.warn(f"{' '.join(argv)} failed: {output[0] if output else 'unknown error'}")
        return
    log.detail(f"ran {' '.join(argv)}")


# --- uninstalling ------------------------------------------------------------


def uninstall_config(log: Log, path: Path) -> None:
    """Restore a backup if there is one, otherwise delete our own file."""
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if not path.exists() and not backup.exists():
        return
    ours = is_generated_file(path)
    if backup.exists() and (ours or not path.exists()):
        if log.dry_run:
            log.detail(f"restore {path} from {backup.name}")
        else:
            restore_backup(path)
            log.detail(f"restored {path} from its backup")
        return
    if ours:
        if log.dry_run:
            log.detail(f"remove {path}")
        else:
            path.unlink()
            log.detail(f"removed {path}")
        return
    log.warn(f"leaving {path} alone: it was not written by this installer")


def uninstall(log: Log, theme_dirs: tuple[Path, ...]) -> None:
    for directory in theme_dirs:
        target = directory / THEME_NAME
        if not target.exists():
            log.detail(f"not installed: {target}")
            continue
        if log.dry_run:
            log.detail(f"remove {target}")
        else:
            shutil.rmtree(target)
            log.detail(f"removed {target}")

    for path in (gtk2_rc_file(), gtk3_settings_file(), gtk4_settings_file(), gtk4_user_file()):
        uninstall_config(log, path)

    target = extras_dir()
    if target.exists():
        if log.dry_run:
            log.detail(f"remove {target}")
        else:
            shutil.rmtree(target)
            log.detail(f"removed {target}")


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
        "--gtk3-settings",
        action="store_true",
        help="also write the GTK 3 and GTK 4 settings.ini files",
    )
    parser.add_argument(
        "--apply", action="store_true", help="make the theme the active one for this desktop"
    )
    parser.add_argument("--uninstall", action="store_true", help="remove everything it installed")
    parser.add_argument(
        "--force",
        action="store_true",
        help="overwrite configuration files this installer did not write",
    )
    parser.add_argument("--dry-run", action="store_true", help="show the plan, change nothing")
    parser.add_argument("--quiet", action="store_true", help="only print problems")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    log = Log(quiet=args.quiet, dry_run=args.dry_run)

    if args.theme_dir:
        theme_dirs = tuple(Path(value).expanduser() for value in args.theme_dir)
    else:
        theme_dirs = tuple(user_theme_dirs())

    if not SOURCE_THEME_DIR.is_dir():
        print(f"error: theme source is missing: {SOURCE_THEME_DIR}", file=sys.stderr)
        return 1

    if args.uninstall:
        log.step(f"Uninstalling {THEME_NAME}")
        uninstall(log, theme_dirs)
        log.note("")
        log.note("Done. Log out and back in to see the change.")
        return 0

    log.step(f"Installing {THEME_NAME}")
    if not args.no_theme:
        install_theme(log, theme_dirs, render_assets=not args.no_assets)
    if not args.no_config:
        install_configuration(
            log, theme_dirs[0] / THEME_NAME, force=args.force, settings_ini=args.gtk3_settings
        )
    if not args.no_extras:
        install_extras(log)
    if args.apply and not args.dry_run:
        log.step("Applying the theme")
        for command in APPLY_COMMANDS:
            run_command(log, command)

    log.note("")
    if args.dry_run:
        log.note("Dry run: nothing was changed.")
    else:
        log.note(f"{THEME_NAME} installed into:")
        for directory in theme_dirs:
            log.note(f"  {directory / THEME_NAME}")
        log.note("")
        log.note("Log out and back in, or run with --apply, to make it active.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
