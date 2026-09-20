"""Colours, sizes and proportions for the GnuchanPurple cursor theme.

Every cursor in the theme is one flat shape with an outline around it. That is a
deliberate change from the set this one replaces, which drew every state as a
round dot of light with marks stacked on top of it: at twenty four pixels that
reads as two cursors sitting on each other, and the caret, the resize arrows and
the hand all turned into the same purple blob with something crossing it.

So there are two colours and no gradient. The fill is the theme's accent purple,
taken from ``dotfile/ICON_THEME/icons/palette.py`` like the rest of the desktop's
artwork, and the outline is a very dark purple that separates the shape from a
white page without reading as a second colour. A shape drawn in one flat colour
with one outline is a shape that is still legible when it is eleven pixels of
arrow, which is the only test a cursor has to pass.

Nothing here is a pixel value. Everything is a fraction of the nominal size, so
the same description draws correctly at 24, 32 and 48 pixels - and a cursor that
is drawn at the size it is asked for is one the X server never has to scale.
"""

from __future__ import annotations

# --- the colours -------------------------------------------------------------

#: The fill of every cursor: the theme's accent purple.
FILL = "#a855f7"

#: The second purple, painted inside the fill and inset from it. Every cursor in
#: the set is two flat tones of the same hue plus a rim, which is what makes a
#: shape read as a panel rather than as a sticker: one flat colour at twenty four
#: pixels is a silhouette, and a silhouette with a darker core is an object.
RIM = "#6b21c8"

#: How far the darker tone is inset from the edge, as a fraction of the nominal
#: size. Small enough that a resize bar four pixels wide still has its lighter
#: edge, large enough to be seen at all at 24 pixels.
INSET = 0.045

#: The bright core: the one near-white mark in the set, and the only detail that
#: is not a shade of purple. It sits inside a shape rather than on top of it - in
#: the middle of a reticle, at the head of a gyro - so a cursor is still one
#: object with one silhouette, which is the thing the stacked shapes it replaces
#: got wrong.
CORE = "#f3e0ff"

#: The outline drawn around the fill. A cursor is seen over whatever the user
#: was looking at, so the shape carries its own edge rather than relying on
#: contrast with a background it cannot know.
OUTLINE = "#1b0729"

#: The fill of the donut a waiting state is drawn as, and of the marks that
#: stand for something inside a badge. Lighter than :data:`FILL` so a spinner
#: reads as a ring of light and a symbol reads against the badge it sits on.
LIGHT = "#ddb3ff"

#: The near-white the symbols inside badges are drawn in - the plus of a zoom,
#: the question mark of a help cursor. Only ever used on top of :data:`FILL`.
SYMBOL = "#f3e0ff"

# --- proportions --------------------------------------------------------------
# Written as fractions of the nominal size. A value near 0.5 is the edge of the
# image, so nothing wants to be much above 0.45.

#: How thick the outline around every shape is, at each side.
OUTLINE_WIDTH = 0.052

#: How far the donut of a waiting state reaches from the centre, and how thick
#: the ring itself is.
SPINNER_REACH = 0.395
SPINNER_WIDTH = 0.135

#: How much of the circle a waiting state's ring covers when it has a gap: a gap
#: is what makes a ring read as something turning rather than as a target.
SPINNER_SWEEP = 0.74

#: Where a badge sits, and how big it is. The lower right of the cursor, because
#: the hotspot is the arrow's tip at the upper left and a badge has to stay off
#: the part of the screen the click is about to land on.
#:
#: Two numbers decide both: the badge has to clear the arrow's tail, or the two
#: shapes touch and the cursor reads as a disc with a wedge cut out of it, and it
#: has to stay inside the image once its outline is grown outward, or one side
#: comes out cut flat. 0.735 - 0.18 - 0.052 is inside, and the arrow's tail ends
#: 0.26 from the badge's centre, which is outside 0.18 + 0.052.
BADGE_CENTRE: tuple[float, float] = (0.735, 0.735)
BADGE_RADIUS = 0.18

#: How much of the badge a symbol fills. A symbol at the badge's own radius
#: would have its outline sitting on the badge's outline.
BADGE_SYMBOL_SCALE = 0.72

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
