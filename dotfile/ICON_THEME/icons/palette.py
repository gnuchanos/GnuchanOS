"""Colours for the GnuchanPurple icon theme.

The values are the same ones ``dotfile/GTK_THEME/GnuchanPurple`` uses, so an
icon and the widget it sits in are drawn from one palette. Icons are seen on
both dark and light backgrounds, so the module also exposes the few tints that
read well on either: the accents are light enough for a dark panel, and the
outlines use the dark surfaces when an icon needs contrast against a light one.

Nothing here is a literal used twice: every colour has exactly one spelling,
and the layouts in :mod:`icons.glyphs` refer to names, not to hex strings.
"""

from __future__ import annotations

# --- surfaces, shared with the GTK theme -------------------------------------

BG_DARKEST = "#09030d"
BG = "#0e0614"
BG_ALT = "#0f0615"
SURFACE = "#12071a"
SURFACE_ALT = "#180a22"
RAISED = "#1c0b27"
RAISED_HOVER = "#281036"

# --- lines -------------------------------------------------------------------

BORDER_ALT = "#24102f"
BORDER = "#32143f"
BORDER_STRONG = "#6d28d9"

# --- accents -----------------------------------------------------------------

ACCENT = "#a855f7"
ACCENT_HOVER = "#b76eff"
ACCENT_ACTIVE = "#9333ea"
ACCENT_LIGHT = "#c77dff"
ACCENT_PALE = "#d8a4ff"
SELECTED_BG = "#542080"

# --- text --------------------------------------------------------------------

FG = "#ead7ff"
FG_BRIGHT = "#f3e8ff"
FG_DIM = "#b78ed6"
FG_DISABLED = "#70458a"
WHITE = "#ffffff"

# --- state -------------------------------------------------------------------

ERROR = "#e879f9"
ERROR_BG = "#86198f"
WARNING = "#c77dff"
WARNING_BG = "#9333ea"
SUCCESS = "#d8a4ff"
SUCCESS_BG = "#6d28d9"
INFO = "#c77dff"
INFO_BG = "#180a22"

# Fully transparent, used for the holes an icon needs to punch.
CLEAR = "#00000000"

# --- per context tints -------------------------------------------------------
# One entry per icon context, so every folder, device or document in the set is
# recognisably part of the family without being the same colour as its
# neighbours. Keys are the freedesktop context names used by the catalogue.

CONTEXT_TINT: dict[str, str] = {
    "apps": ACCENT,
    "applications": ACCENT,
    "categories": ACCENT_LIGHT,
    "actions": ACCENT_PALE,
    "devices": BORDER_STRONG,
    "mimetypes": ACCENT_LIGHT,
    "places": ACCENT,
    "status": SUCCESS,
    "emblems": WARNING,
    "emotes": WARNING,
    "international": ACCENT_HOVER,
    "ui": ACCENT_LIGHT,
    "animations": ACCENT,
    "legacy": FG_DIM,
}

DEFAULT_TINT = ACCENT

# The plate an application icon sits on, and the glyph drawn on top of it.
APP_PLATE = SELECTED_BG
APP_PLATE_EDGE = ACCENT_ACTIVE
APP_GLYPH = FG_BRIGHT

# Symbolic icons are single colour glyphs that follow the text colour of
# whatever draws them; the fallback below is only used when an icon is
# rendered outside a GTK style context.
SYMBOLIC = FG
# --- flags -------------------------------------------------------------------
# Flag colours are named by the colour they are, not by the country that owns
# them: twenty flags share a red, and a table that respells the hex once per
# country is a table that drifts. They are also slightly muted, so a contact
# sheet of forty flags does not read as forty different reds.

FLAG_RED = "#d32f2f"
FLAG_CRIMSON = "#b21f36"
FLAG_MAROON = "#8a1538"
FLAG_BURGUNDY = "#a02040"
FLAG_ROSE = "#e0556b"
FLAG_BLUE = "#1f5fbf"
FLAG_AZURE = "#2f8fdf"
FLAG_SKY = "#79c0ff"
FLAG_NAVY = "#1b2a6b"
FLAG_TEAL = "#1f7a8c"
FLAG_CYAN = "#3fc1d0"
FLAG_GREEN = "#2e8b57"
FLAG_EMERALD = "#1f8a4c"
FLAG_LIME = "#7ec850"
FLAG_OLIVE = "#6b7a2f"
FLAG_YELLOW = "#f2c73d"
FLAG_GOLD = "#e0a020"
FLAG_ORANGE = "#e07a1f"
FLAG_BROWN = "#7a4a1f"
FLAG_SAND = "#e8d9a0"
FLAG_IVORY = "#f4f1e4"
FLAG_WHITE = WHITE
FLAG_BLACK = "#161021"
FLAG_GREY = "#9aa0a6"
FLAG_STEEL = "#5b6470"
FLAG_PURPLE = ACCENT_ACTIVE
FLAG_VIOLET = ACCENT

# --- emotes ------------------------------------------------------------------
# A face is drawn from the same handful of colours everywhere, so the family
# reads as one set: the disc, the features, and the two accents the expressions
# that need a tongue or a blush share.

EMOTE_FACE = FLAG_YELLOW
EMOTE_INK = BG_DARKEST
EMOTE_TONGUE = FLAG_ROSE
EMOTE_BLUSH = FLAG_ROSE
