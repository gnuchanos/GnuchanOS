"""Colours and sizes for the GnuchanPurple cursor theme.

Every cursor in the theme is the same object seen in a different state: a round
dot of light with a halo around it. That is why the colours live here rather
than in the shape that draws them - the whole point of the set is that the dot,
the halo and the rim are the same three colours everywhere, and a shape that
picked its own would be a shape that looks like a different cursor.

The values are the accents of the rest of the desktop, taken from
``dotfile/ICON_THEME/icons/palette.py``: a cursor drawn from the same palette as
the icons it points at is a cursor that belongs to the theme.

Nothing is drawn in a colour that only reads on one background. A light purple
cursor disappears on a white page and a dark one disappears on a dark editor, so
every cursor is drawn in four layers, innermost first:

    the shadow   a dark ring, so the cursor is visible on a light background
    the halo     the soft light around the dot, which is the glow
    the rim      the saturated purple edge
    the core     near white, what the eye actually follows

The shadow is what makes the set usable on a light background; without it the
halo would dissolve into the page.
"""

from __future__ import annotations

# --- the four layers ---------------------------------------------------------

#: Dark ring drawn first, under everything. Not black: a black ring on a light
#: page looks like a printing artefact, while a very dark purple looks like the
#: same cursor with a shadow.
SHADOW = "#1b0729"

#: The outer glow. It is drawn as a radial falloff, so only its colour matters;
#: its alpha is computed per pixel from the distance to the centre.
HALO = "#c77dff"

#: The saturated edge of the dot itself.
RIM = "#a855f7"

#: The bright centre. Light enough that the dot reads as a light source, which
#: is what makes the pulse visible at twenty pixels across.
CORE = "#f3e0ff"

#: The thin ring between rim and core, which is what stops the core looking
#: like a blown-out highlight at the larger sizes.
INNER = "#ddb3ff"

# --- the parts a state adds on top -------------------------------------------

#: Arrows, ticks, chevrons and the slash are drawn in this colour: light enough
#: to read on the dark part of the cursor, which is where they always are.
MARK = "#f3e0ff"

#: The dark backing under a mark, so a chevron drawn over the bright core is
#: still legible. Same reasoning as SHADOW.
MARK_SHADOW = "#2a0d3d"

#: The busy ring and the forbidden slash: the two things that have to read as a
#: warning rather than as part of the dot.
RING = "#ddb3ff"

# --- geometry ----------------------------------------------------------------
# All of it in fractions of the nominal size or of the dot radius, so one
# description draws correctly at every size the theme ships.

#: Dot radius as a fraction of the nominal size. A quarter of the size leaves a
#: quarter on each side for the halo, which is the room it needs to fade out
#: before the edge of the image.
DOT_RADIUS = 0.235

#: How far the halo reaches, as a multiple of the dot radius. The falloff is
#: computed over this distance and reaches zero at it, so the image never shows
#: a square edge.
HALO_REACH = 2.05

#: The rim thickness, as a fraction of the dot radius.
RIM_WIDTH = 0.20

#: The dark ring under the dot, as a fraction of the dot radius. Thin: it is
#: there to separate the cursor from a white page, not to be an outline.
SHADOW_WIDTH = 0.16

#: The width of a mark line (arrow shafts, ticks, the slash), as a fraction of
#: the nominal size.
MARK_WIDTH = 0.075

#: How bright the halo is at its strongest, before the falloff. The pulse moves
#: between these two.
HALO_ALPHA_MIN = 0.30
HALO_ALPHA_MAX = 0.62

#: How much the dot's own brightness moves during the pulse. Small on purpose: a
#: cursor that blinks like a warning light is hard to look at while typing.
CORE_PULSE = 0.16

# --- sizes -------------------------------------------------------------------
# The nominal sizes written into the theme. A cursor is read at the size the
# desktop asks for, and a theme with only one size gets scaled by the X server,
# which resamples a 24 pixel cursor up to 48 and draws a blurred dot. Three
# sizes cover a normal screen, a HiDPI one and a large-cursor setting.

SIZES: tuple[int, ...] = (24, 32, 48)

# --- animation ---------------------------------------------------------------

#: Frames per cursor. Twelve at one frame every FRAME_DELAY_MS is a little over
#: two seconds of pulse: slow enough to read as breathing rather than blinking,
#: which is what makes the halo look like light instead of an outline.
FRAMES = 12

#: How long each frame is shown, in milliseconds. Xcursor stores this per image.
FRAME_DELAY_MS = 210

#: Frames in the states that spin. A rotation that comes back to where it began
#: after FRAMES_SPIN frames reads as continuous; one tied to the pulse does not.
FRAMES_SPIN = 24
