"""Colours, sizes and proportions for the GnuChanMouseIcons cursor theme.

The theme is xenotech: a cursor that looks like a piece of alien hardware that
was found in a crashed ship and plugged into a desktop. It is not a flat shape
with a gradient on it. Three things make that read, and all three live here.

The first is the colour range. The set is violet in every tone it has - a void
black-violet for the grooves, three armoured violets for the shell, a plasma
violet for the lit panels and a near white for the cores - plus exactly one
foreign hue, a cold cyan that is used only for the energy lines. That one
colour is what makes the violet look lit rather than painted: a shape that is
violet from edge to centre is a violet shape, and a shape with a cyan line
burning along its spine is a machine.

The second is the material. Every shape is a shell with a groove cut into it and
a panel sunk inside the groove. The shell is dark violet, the groove is the
outline colour, and the panel is the lit violet - so a cursor at any size has
three separate depths in it and reads as machined rather than as filled in.

The third is scale. Nothing here is a pixel value: everything is a fraction of
the nominal size, so one description draws correctly at 24, 32 and 48 pixels,
and the same shape keeps its weights at all three. The details are the one
exception, and they are clamped to a pixel so a groove never disappears.

The palette is the accent purple of ``dotfile/ICON_THEME/icons/palette.py`` and
``dotfile/GRUB_THEME``, extended downward into the void rather than upward into
pink, so the cursor is the same violet as the rest of the desktop's artwork.
"""

from __future__ import annotations

# --- the colours -------------------------------------------------------------

#: The outline and the grooves. A cursor is seen over whatever the user was
#: looking at, so the shape carries its own edge rather than relying on contrast
#: with a background it cannot know. Near black and violet rather than neutral
#: grey, so it reads as the shadowed side of the same light the body is made of.
OUTLINE = "#08030f"

#: The shell: the dark armour under every lit panel, and the colour a shape is
#: mostly made of. The step below the body, and the one that gives a cursor its
#: depth - a shape drawn only in :data:`BODY` and :data:`PLASMA` is a sticker.
SHELL = "#2b0a52"

#: The mid tone between the shell and the body, used for the plates that sit on
#: the shell in a large shape.
ARMOUR = "#4b1391"

#: The body: the theme's accent purple, and the colour every shape is mostly
#: made of.
BODY = "#7c2ff2"

#: The lit violet, inset from the body and shifted towards the light. This is
#: the colour that makes the set look like it is lit from within; a darker inset
#: is what makes a cursor read as flat clip art.
PLASMA = "#a855f7"

#: The pale end of the range: the lit face of a panel, and the colour of the
#: highlight along the edge nearest the light.
LIGHT = "#c9a2ff"

#: The brightest violet a body carries, used along the top edge of a lit panel
#: and for the head of a spinning ring.
BRIGHT = "#efe4ff"

#: The near-white of the marks that are meant to be a point of light: the core
#: of a reticle, the node at the tip of the pointer, the head of a spinner. The
#: one colour in the set that is not a shade of violet, and it is used in tiny
#: amounts for that reason.
CORE = "#ffffff"

#: The colour the halo around every shape is drawn in. Lighter than the body and
#: slightly desaturated, which is what emitted light looks like: the aura of a
#: violet object is a paler violet, not a second object.
GLOW = "#9d4bff"

#: The cold accent. The one hue in the set that is not violet, and it is spent
#: only on the energy lines - the thin burning spine inside a shape - which is
#: what makes the violet shell read as something being lit rather than as
#: something painted. It is deliberately a thin line and never a fill: a cyan
#: shape beside a violet one is two cursors.
ENERGY = "#5ff0ff"

#: The fill of the symbols on a badge. Near white, because a glyph six pixels
#: across is legible by contrast and by nothing else.
SYMBOL = "#f7efff"

# --- proportions --------------------------------------------------------------
# Written as fractions of the nominal size. A value near 0.5 is the edge of the
# image, so nothing wants to be much above 0.40: the outline and the halo are
# grown outward from the points, and a shape on the edge of the square is one
# whose light gets cut off flat.

#: The hairline around every shape: dark, thin, and the same on a bar as on the
#: pointer.
OUTLINE_WIDTH = 0.034

#: The groove cut into the shell, and how far inside the edge it runs. The groove
#: is the same dark as the outline, which is what makes a shape look like two
#: pieces of armour with a seam between them rather than one plate with a line
#: drawn on it.
GROOVE = 0.026
GROOVE_INSET = 0.088

#: The halo. ``GLOW_REACH`` is how far the soft aura is grown outward and how
#: far it fades; the rim is a much shorter, much brighter band, which is what
#: makes the edge of a shape look like the edge of something lit rather than the
#: edge of something painted.
GLOW_REACH = 0.062
GLOW_ALPHA = 0.26
GLOW_RIM_REACH = 0.013
GLOW_RIM_ALPHA = 0.55

#: How far inside the body its lit tone starts, and how far that tone is shifted
#: towards the upper left. The shift is small - barely two hundredths of the
#: cursor - but it is the difference between a flat fill and a lit one.
INSET = 0.052
LIFT = (-0.014, -0.018)

#: How far the brightest tone is inset, and how far it is lifted, inside a shape
#: large enough to carry three tones.
CORE_INSET = 0.108
CORE_LIFT = (-0.024, -0.030)

#: The energy line: how wide it is, and how far inside the edge of a shape it
#: runs. Clamped to a pixel where it is drawn, because a line four hundredths of
#: nothing is a line nobody sees, and a cursor whose only detail is invisible at
#: 24 pixels is a cursor with no detail.
ENERGY_WIDTH = 0.019
ENERGY_INSET = 0.150

#: How far the nodes - the small lit dots at the joints of a shape - reach, and
#: how far the bloom around them reaches as a multiple of that.
NODE_RADIUS = 0.052
NODE_GLOW = 2.1

#: How far the ring of a waiting state reaches from the centre, and how thick
#: the ring itself is. The reach leaves room for the width, the outline and the
#: halo inside the square: a ring drawn at four tenths of the cursor is a ring
#: whose outer edge is clipped flat by its own image at 24 pixels, which is the
#: one size where the clipping is visible and the one size most desktops use.
SPINNER_REACH = 0.336
SPINNER_WIDTH = 0.096

#: How much of the circle a waiting state's ring covers when it has a gap, and
#: how much of that arc is the comet's tail. A ring of one width and one colour
#: with a gap in it is a broken circle; a ring that is thick and bright at its
#: head and thins away along its tail is something moving.
SPINNER_SWEEP = 0.72
SPINNER_TAIL = 0.62

#: Where a badge sits, and how big it is. The lower right of the cursor, because
#: the hotspot is the arrow's tip at the upper left and a badge has to stay off
#: the part of the screen the click is about to land on.
#:
#: Two numbers decide both: the badge has to clear the arrow's tail, or the two
#: shapes touch and the cursor reads as one blob, and it has to stay inside the
#: image once its halo is grown outward, or one side comes out cut flat.
BADGE_CENTRE: tuple[float, float] = (0.722, 0.718)
BADGE_RADIUS = 0.176

#: How much of the badge a symbol fills. A symbol at the badge's own radius
#: would have its outline sitting on the badge's outline.
BADGE_SYMBOL_SCALE = 0.80

#: Where the light on a badge sits, as a fraction of its radius, and how big the
#: highlight is. The upper left, matching :data:`LIFT`: a badge lit from below
#: while every other cursor is lit from above is the kind of detail that reads as
#: wrong without anyone being able to say why.
BADGE_LIGHT: tuple[float, float] = (-0.30, -0.34)
BADGE_HIGHLIGHT = 0.42

#: How many sides the badge and the ring of a waiting state are given. Six: a
#: circle is what every desktop already draws, and a hexagon is a circle that was
#: made by something that machines things.
BADGE_SIDES = 6

#: The dark side of a badge and of every body: how much of the outline colour is
#: mixed into the lit tone at the edge away from the light. Small, because the
#: outline already separates the shape from its halo.
SHADE = 0.22

#: The radius of the spark at the tip of the pointer and in the middle of a
#: reticle, and how far below that radius it stops being a sparkle and becomes a
#: dot. A four pointed star two pixels across is a dot with corners nobody can
#: see, so the shape is only used where there is room for it.
SPARK_RADIUS = 0.082
SPARK_GLOW = 1.95

# --- sizes -------------------------------------------------------------------
# A cursor is read at the size the desktop asks for. A theme that ships one size
# gets scaled by the X server, which resamples a 24 pixel cursor up to 48 and
# draws it blurred; three sizes cover a normal screen, a HiDPI one and the large
# cursor setting.
SIZES: tuple[int, ...] = (24, 32, 48)

# --- animation ----------------------------------------------------------------
# Only the states that really move carry more than one frame. Every other state
# in the set is a still shape, and a still shape written twelve times is twelve
# times the disk for nothing.
#
#: Frames in a state that pulses - the hourglass of a window that cannot answer.
FRAMES = 12

#: How long each frame is shown, in milliseconds.
FRAME_DELAY_MS = 150

#: Frames in the states that spin: a rotation that returns to where it started
#: after this many frames reads as continuous.
FRAMES_SPIN = 24
