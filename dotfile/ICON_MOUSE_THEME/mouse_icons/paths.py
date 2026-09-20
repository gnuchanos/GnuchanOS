"""Every cursor shape, as a list of points in a unit square.

A shape here is one closed polygon and nothing else. There is no shape in this
module that is built by drawing a second shape on top of the first, because that
is exactly what made the set this replaces unreadable: a round dot with a caret
over it is two cursors, and at twenty four pixels the eye reads the pair as a
blob rather than as a caret.

The coordinates are fractions of the nominal size, from 0 at the top left to 1
at the bottom right, so one description draws at 24, 32 and 48 pixels. Nothing
goes much past 0.45: the outline is grown outward from the points, and a point
on the edge of the square would be clipped by the image.

The polygon fill in :mod:`draw` uses the ray crossing rule, so these lists may be
concave - the tail of the pointer, the waist of the hourglass, the slit in the
frame - as long as the path never crosses itself.
"""

from __future__ import annotations

import math

Point = tuple[float, float]
Path = list[Point]


# --- transforms ---------------------------------------------------------------


def turned(points: Path, turns: float, centre: Point = (0.5, 0.5)) -> Path:
    """``points`` rotated by ``turns`` of a full circle about ``centre``.

    Turns rather than radians because every rotation this module asks for is a
    quarter or an eighth of a circle, and ``0.25`` says that where ``math.pi /
    2`` only computes it. The screen's y axis points down, so a positive turn is
    clockwise on screen, which is the direction a reader expects.
    """
    if turns == 0.0:
        return list(points)
    angle = turns * math.tau
    cos = math.cos(angle)
    sin = math.sin(angle)
    cx, cy = centre
    out: Path = []
    for x, y in points:
        dx = x - cx
        dy = y - cy
        out.append((cx + dx * cos - dy * sin, cy + dx * sin + dy * cos))
    return out


def placed(points: Path, centre: Point, half: float) -> Path:
    """The unit square ``points`` are written in, mapped onto a box.

    Used for the symbols that go inside a badge: each one is written in its own
    0..1 square and then placed at the badge's centre at the badge's size, so
    the symbol and the badge cannot drift apart.
    """
    cx, cy = centre
    return [(cx + (x - 0.5) * 2.0 * half, cy + (y - 0.5) * 2.0 * half)
            for x, y in points]


# --- the arrow family ---------------------------------------------------------
# Every arrow in the set is written from the same five fractions, so a resize
# bar, a scroll chevron and the four headed move cursor are the same shape with
# the same head on it. The head is a triangle whose base is a third of the
# cursor wide, which is what makes a direction readable when the shaft is one
# pixel.

#: How far an arrow reaches from the middle of the cursor, how wide its shaft is,
#: how far its head sticks out from the shaft, and how long its head is.
REACH = 0.45
SHAFT_HALF = 0.085
HEAD_HALF = 0.235
HEAD_LENGTH = 0.28

#: A double headed bar, pointing left and right. Turned by a quarter for the
#: vertical pair of resize cursors and by an eighth for the diagonals.
BAR: Path = [
    (0.5 + REACH, 0.5),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 - REACH, 0.5),
    (0.5 - REACH + HEAD_LENGTH, 0.5 + HEAD_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
]

#: One arrow pointing right, its tail short of the middle. Turns into the four
#: single direction cursors some programs want for their own scrolling.
SHAFT: Path = [
    (0.5 + REACH, 0.5),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 - REACH + 0.13, 0.5 - SHAFT_HALF),
    (0.5 - REACH + 0.13, 0.5 + SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
]

#: Four heads around a cross, for moving a window and for scrolling in any
#: direction. Written out rather than assembled from four copies of
#: :data:`SHAFT`, because four overlapping arrows are four outlines crossing
#: each other in the middle of the cursor.
QUAD: Path = [
    (0.5, 0.5 - REACH),
    (0.5 + HEAD_HALF, 0.5 - REACH + HEAD_LENGTH),
    (0.5 + SHAFT_HALF, 0.5 - REACH + HEAD_LENGTH),
    (0.5 + SHAFT_HALF, 0.5 - SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 + REACH, 0.5),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
    (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 + SHAFT_HALF, 0.5 + SHAFT_HALF),
    (0.5 + SHAFT_HALF, 0.5 + REACH - HEAD_LENGTH),
    (0.5 + HEAD_HALF, 0.5 + REACH - HEAD_LENGTH),
    (0.5, 0.5 + REACH),
    (0.5 - HEAD_HALF, 0.5 + REACH - HEAD_LENGTH),
    (0.5 - SHAFT_HALF, 0.5 + REACH - HEAD_LENGTH),
    (0.5 - SHAFT_HALF, 0.5 + SHAFT_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 + HEAD_HALF),
    (0.5 - REACH, 0.5),
    (0.5 - REACH + HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 - REACH + HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 - SHAFT_HALF, 0.5 - SHAFT_HALF),
    (0.5 - SHAFT_HALF, 0.5 - REACH + HEAD_LENGTH),
    (0.5 - HEAD_HALF, 0.5 - REACH + HEAD_LENGTH),
]


# --- the pointer --------------------------------------------------------------
# The classic arrow: a tip, a wide head and a tail leaning away from the tip. It
# is the one shape in the set that is not centred on its own hotspot, which is
# why it is drawn in the upper left of the square.

POINTER: Path = [
    (0.042, 0.042),   # the tip, which is where the click lands
    (0.042, 0.788),   # straight down the leading edge
    (0.228, 0.602),   # in to the notch where the tail begins
    (0.368, 0.928),   # the tail's outer corner
    (0.508, 0.868),   # its inner corner
    (0.368, 0.542),   # the tail's shoulder
    (0.602, 0.542),   # the trailing corner of the head
]

#: Where the pointer's tip is. The arrow is the only cursor whose hotspot is not
#: the middle of the image, and moving it is the whole difference between a
#: cursor that clicks where it points and one that clicks half a shape away.
POINTER_HOTSPOT: Point = (0.07, 0.07)


# --- the caret ----------------------------------------------------------------
# An I-beam and nothing else, drawn fat enough to survive being eleven pixels
# tall: a hairline caret at 24 pixels is one grey pixel the eye loses over a
# white page.

CARET: Path = [
    (0.30, 0.09), (0.70, 0.09), (0.70, 0.195), (0.575, 0.195),
    (0.575, 0.805), (0.70, 0.805), (0.70, 0.91), (0.30, 0.91),
    (0.30, 0.805), (0.425, 0.805), (0.425, 0.195), (0.30, 0.195),
]


# --- the crosses --------------------------------------------------------------

#: A plus with arms to the edge of the square: the crosshair a drawing program
#: and a colour picker ask for.
CROSS: Path = [
    (0.415, 0.05), (0.585, 0.05), (0.585, 0.415), (0.95, 0.415),
    (0.95, 0.585), (0.585, 0.585), (0.585, 0.95), (0.415, 0.95),
    (0.415, 0.585), (0.05, 0.585), (0.05, 0.415), (0.415, 0.415),
]

#: The same cross at half the weight, for the precision pointer. Thin enough
#: that the middle of whatever is being selected is not covered.
HAIRLINE: Path = [
    (0.465, 0.03), (0.535, 0.03), (0.535, 0.465), (0.97, 0.465),
    (0.97, 0.535), (0.535, 0.535), (0.535, 0.97), (0.465, 0.97),
    (0.465, 0.535), (0.03, 0.535), (0.03, 0.465), (0.465, 0.465),
]

#: A hollow square, for picking one cell of a grid. The wall is one path with a
#: slit in it: the ray crossing rule counts the hole as outside however the path
#: is wound, and a second square drawn inside the first would be exactly the
#: stacked shape this set is meant to have none of.
FRAME: Path = [
    (0.10, 0.10), (0.90, 0.10), (0.90, 0.90), (0.10, 0.90),
    (0.10, 0.72),   # up the leading edge to the slit
    (0.28, 0.72),   # through the wall, into the hole
    (0.28, 0.28), (0.72, 0.28), (0.72, 0.72), (0.28, 0.72),
    (0.10, 0.72),   # and back out through the slit
]


# --- the hands ----------------------------------------------------------------
# A hand is the one cursor that cannot be reduced to a bar and a head, so it is
# written as a silhouette: a palm with three fingers over it, in one outline.

#: The open hand, drawn as the pointing one: a fist with the index finger up and
#: the thumb out to the side. Three separate fingers were tried first and are not
#: drawn here any more - at twenty four pixels the two gaps between them are
#: filled in by their own outlines and the hand comes out as a solid fork, which
#: is the one shape a hand must not be.
MITTEN: Path = [
    (0.20, 0.92), (0.80, 0.92), (0.80, 0.44),   # the palm, along the bottom
    (0.62, 0.44),                               # across the top of the fingers
    (0.60, 0.10), (0.44, 0.10), (0.42, 0.44),   # the index finger, standing up
    (0.20, 0.44),                               # across the top of the palm
    (0.20, 0.56),                               # down its leading edge
    (0.08, 0.62), (0.08, 0.78), (0.20, 0.82),   # the thumb
]

#: The closed hand: a fist with the thumb across the bottom of the palm. No
#: fingers, because closed is the whole difference between the two states and a
#: fist with the fingers still drawn on it reads as the other one. Squatter than
#: the open hand on purpose, so the two are told apart at a glance and not by
#: counting the fingers.
FIST: Path = [
    (0.22, 0.90), (0.80, 0.90), (0.80, 0.44), (0.70, 0.26),
    (0.34, 0.26), (0.24, 0.44), (0.10, 0.52), (0.10, 0.70), (0.22, 0.76),
]


# --- waiting ------------------------------------------------------------------
# An hourglass: two triangles meeting at a waist. It is the shape every desktop
# that predates a spinner used for "wait", and it is still legible at sixteen
# pixels, where a ring with a gap in it is a ring with a gap in it.

HOURGLASS: Path = [
    (0.20, 0.10), (0.80, 0.10), (0.50, 0.50),
    (0.80, 0.90), (0.20, 0.90), (0.50, 0.50),
]


# --- the symbols that go inside a badge ---------------------------------------
# Each is written in its own 0..1 square and placed on the badge by
# :func:`placed`, so a symbol is never bigger than the badge it sits on.

PLUS: Path = [
    (0.36, 0.06), (0.64, 0.06), (0.64, 0.36), (0.94, 0.36),
    (0.94, 0.64), (0.64, 0.64), (0.64, 0.94), (0.36, 0.94),
    (0.36, 0.64), (0.06, 0.64), (0.06, 0.36), (0.36, 0.36),
]

MINUS: Path = [(0.06, 0.40), (0.94, 0.40), (0.94, 0.60), (0.06, 0.60)]

#: A small arrow, for the alias badge: a shortcut is not a copy, and the two
#: states are one glyph apart everywhere else on the desktop too.
ARROW: Path = [
    (0.94, 0.5), (0.64, 0.20), (0.64, 0.39), (0.06, 0.39),
    (0.06, 0.61), (0.64, 0.61), (0.64, 0.80),
]

#: A filled square, for the badge of the drag that moves a thing rather than
#: copying it.
BLOCK: Path = [(0.20, 0.20), (0.80, 0.20), (0.80, 0.80), (0.20, 0.80)]


# --- the refusal --------------------------------------------------------------
# A horizontal bar, turned by an eighth of a circle to make the bar of a
# prohibition sign. Written lying down because that is the shape that is easy to
# read in this file, and turned where it is used so the angle is stated once.

SLASH: Path = [
    (0.06, 0.445), (0.94, 0.445), (0.94, 0.555), (0.06, 0.555),
]
