"""Every cursor shape, as a list of points in a unit square.

A shape here is one closed polygon and nothing else - the detail that makes the
set read as hardware rather than as clip art is not a second shape drawn on top
of the first. It is three things: the groove the painting code cuts into every
shape, the energy line that runs along the spine named at the bottom of a shape,
and the node that sits where the shape's limbs meet. All three are placed from
the polygon, so a shape here is still one outline, and the pointer cannot come
out with a groove that the resize bar does not have.

The coordinates are fractions of the nominal size, from 0 at the top left to 1
at the bottom right, so one description draws at 24, 32 and 48 pixels. Nothing
goes much past 0.45: the outline and the halo are grown outward from the points,
and a point on the edge of the square would be clipped by the image.

The polygon fill in :mod:`draw` uses the ray crossing rule, so these lists may be
concave - the tail of the pointer, the barb on an arrow head, the slit in the
reticle - as long as the path never crosses itself. The reticle is the one shape
that uses the rule's other consequence: a path that walks around its outside,
slips in through a slit, walks around a hole and slips back out encloses the hole
as empty space, which is how a crosshair is given something to see through.
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

    Used for open polylines as well as for polygons - the energy line down a
    resize bar is turned with the bar it runs inside, so the two cannot end up
    at different angles.
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
    the symbol and the badge cannot drift apart. A symbol that is an open
    polyline - a stroke rather than a filled shape - is scaled the same way,
    which is why this works on both.
    """
    cx, cy = centre
    return [(cx + (x - 0.5) * 2.0 * half, cy + (y - 0.5) * 2.0 * half)
            for x, y in points]


def regular(cx: float, cy: float, radius: float, sides: int,
            turn: float = 0.0) -> Path:
    """A regular polygon as points - the hexagon every machine part is built on.

    A circle is what every desktop already draws. A hexagon is a circle that
    something machined, and using one shape for the badge, the ring of a waiting
    state and the cell selector is what makes those three read as one family.
    ``turn`` is a fraction of the circle: ``1/12`` puts a flat edge at the top.
    """
    if sides < 3:
        return []
    start = turn * math.tau
    return [
        (cx + radius * math.cos(start + math.tau * index / sides),
         cy + radius * math.sin(start + math.tau * index / sides))
        for index in range(sides)
    ]


# --- the arrow family ---------------------------------------------------------
# Every arrow in the set is written from the same five fractions, so a resize
# bar, a scroll chevron and the four headed move cursor are the same shape with
# the same head on it.
#
# The head is not a triangle. Its back edge is cut into by a barb, so the head
# has a swallowtail where a plain arrow has a straight line - which at twenty
# pixels is the difference between an arrow and an arrow that was made by
# something that machines things. ``BARB`` is how far that cut reaches towards
# the tip, and it is deliberately small: a deep barb at 24 pixels is a notch the
# outline closes up, and a head whose notch has been filled in is a triangle
# with a wobble in it.

#: How far an arrow reaches from the middle of the cursor, how wide its shaft is,
#: how far its head sticks out from the shaft, how long its head is, and how far
#: the barb cuts into the back of the head.
REACH = 0.398
SHAFT_HALF = 0.068
HEAD_HALF = 0.202
HEAD_LENGTH = 0.228
BARB = 0.056

#: Where the barb's point sits across the head: halfway down its back edge. Half
#: way between the head's outer corner and the shaft, so the notch is symmetric
#: and the head still reads as a head - a barb that reached the head's own
#: corner would be a straight back edge with a kink in it.
BARB_ACROSS = (HEAD_HALF + SHAFT_HALF) / 2.0


def _head_right(reach: float, shaft: float) -> Path:
    """Half a double headed bar: the right head and its share of the shaft.

    Written as a function because every arrow in this module is this shape
    turned or mirrored, and the barb has to be in all of them. ``reach`` is how
    far the tip is from the middle and ``shaft`` how far the bar runs inwards
    from there. The last two points are the notch: the head's back edge is
    walked out to the barb and back, so the head ends in a swallowtail rather
    than on a straight line.
    """
    tip = 0.5 + reach
    back = 0.5 + reach - HEAD_LENGTH
    return [
        (tip, 0.5),
        (back, 0.5 - HEAD_HALF),
        (back + BARB, 0.5 - BARB_ACROSS),
        (back, 0.5 - shaft),
    ]


#: A double headed bar, pointing left and right. Turned by a quarter for the
#: vertical pair of resize cursors and by an eighth for the diagonals.
BAR: Path = (
    _head_right(REACH, SHAFT_HALF)
    + [
        (0.5 - REACH + HEAD_LENGTH, 0.5 - SHAFT_HALF),
        (0.5 - REACH + HEAD_LENGTH - BARB, 0.5 - BARB_ACROSS),
        (0.5 - REACH + HEAD_LENGTH, 0.5 - HEAD_HALF),
        (0.5 - REACH, 0.5),
        (0.5 - REACH + HEAD_LENGTH, 0.5 + HEAD_HALF),
        (0.5 - REACH + HEAD_LENGTH - BARB, 0.5 + BARB_ACROSS),
        (0.5 - REACH + HEAD_LENGTH, 0.5 + SHAFT_HALF),
        (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
        (0.5 + REACH - HEAD_LENGTH + BARB, 0.5 + BARB_ACROSS),
        (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
    ]
)

#: The energy line down the middle of a bar. Runs a little short of each tip so
#: the cold light sits inside the head rather than on its point.
BAR_SPINE: Path = [
    (0.5 - REACH + HEAD_LENGTH * 0.55, 0.5),
    (0.5 + REACH - HEAD_LENGTH * 0.55, 0.5),
]

#: One arrow pointing right, its tail short of the middle. Turns into the four
#: single direction cursors some programs want for their own scrolling.
SHAFT: Path = (
    _head_right(REACH, SHAFT_HALF)
    + [
        (0.5 - REACH + 0.135, 0.5 - SHAFT_HALF),
        (0.5 - REACH + 0.135, 0.5 + SHAFT_HALF),
        (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
        (0.5 + REACH - HEAD_LENGTH + BARB, 0.5 + BARB_ACROSS),
        (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
    ]
)

SHAFT_SPINE: Path = [(0.5 - REACH + 0.16, 0.5), (0.5 + REACH * 0.72, 0.5)]

#: The right half of the four headed move, from the top tip round to the right
#: flank of the bottom head. The left half is this mirrored, which is why it is
#: written once: four arrows assembled from four copies of :data:`SHAFT` would
#: be four outlines crossing each other in the middle of the cursor.
_QUAD_HALF: Path = [
    (0.5, 0.5 - REACH),                                    # the top tip
    (0.5 + HEAD_HALF, 0.5 - REACH + HEAD_LENGTH),           # its right flank
    (0.5 + BARB_ACROSS, 0.5 - REACH + HEAD_LENGTH + BARB),  # the barb
    (0.5 + SHAFT_HALF, 0.5 - REACH + HEAD_LENGTH),          # into the shaft
    (0.5 + SHAFT_HALF, 0.5 - SHAFT_HALF),                   # the junction
    (0.5 + REACH - HEAD_LENGTH, 0.5 - SHAFT_HALF),
    (0.5 + REACH - HEAD_LENGTH + BARB, 0.5 - BARB_ACROSS),  # the barb
    (0.5 + REACH - HEAD_LENGTH, 0.5 - HEAD_HALF),
    (0.5 + REACH, 0.5),                                     # the right tip
    (0.5 + REACH - HEAD_LENGTH, 0.5 + HEAD_HALF),
    (0.5 + REACH - HEAD_LENGTH + BARB, 0.5 + BARB_ACROSS),  # the barb
    (0.5 + REACH - HEAD_LENGTH, 0.5 + SHAFT_HALF),
    (0.5 + SHAFT_HALF, 0.5 + SHAFT_HALF),                   # the junction
    (0.5 + SHAFT_HALF, 0.5 + REACH - HEAD_LENGTH),
    (0.5 + BARB_ACROSS, 0.5 + REACH - HEAD_LENGTH + BARB),  # the barb
    (0.5 + HEAD_HALF, 0.5 + REACH - HEAD_LENGTH),           # its right flank
]

#: Four heads around a cross, for moving a window and for scrolling in any
#: direction. Walked out as the right half and then as the same half mirrored,
#: so the cross is one polygon that never crosses itself.
QUAD: Path = (
    _QUAD_HALF + [(1.0 - x, y) for x, y in reversed(_QUAD_HALF[1:])]
)

#: One arm of the cross's energy line, from the junction out towards the head,
#: and the same line turned into the other three arms. A junction with no light
#: in it is a lump of violet where four shapes meet.
_QUAD_SPINE_ARM: Path = [
    (0.5, 0.5 - SHAFT_HALF * 1.7),
    (0.5, 0.5 - REACH + HEAD_LENGTH * 0.45),
]

QUAD_SPINES: tuple[Path, ...] = tuple(
    turned(_QUAD_SPINE_ARM, index * 0.25) for index in range(4)
)


# --- the pointer --------------------------------------------------------------
# The classic arrow, sharpened: a tip, a dead straight leading edge, a tail that
# leans away from the tip, and a head whose back edge is the hypotenuse every
# arrow has had since before it was a cursor. It is the one shape in the set that
# is not centred on its own hotspot, which is why it is drawn in the upper left
# of the square.
#
# The detail is what makes it a piece of hardware rather than a triangle: the
# painting code cuts a groove inside the edge and runs the cold energy line down
# the spine below, so the arrow at 24 pixels is an armoured blade with a lit
# seam in it rather than a flat violet arrow.

POINTER: Path = [
    (0.048, 0.034),   # the tip, which is where the click lands
    (0.048, 0.742),   # straight down the leading edge
    (0.208, 0.566),   # in to the wrist where the tail begins
    (0.282, 0.932),   # the tail's outer corner
    (0.430, 0.854),   # its inner corner
    (0.352, 0.546),   # the tail's shoulder
    (0.545, 0.404),   # the fin: the one part of the head that is not an arrow
    (0.616, 0.546),   # the trailing corner of the head
]

#: Where the head's fin sits, and how far it reaches past the head's own back
#: edge. It is the one place the pointer's outline is not the outline of an
#: arrow: a blade with a fin is a blade, and a blade is the shape an instrument
#: has where a pointer has a triangle. It is deliberately shallow - a deep fin
#: at 24 pixels is a bump the outline of the head closes up, and what is left is
#: a triangle with a wobble in it - and it sits two thirds of the way up the
#: back edge, where the head is at its widest.
POINTER_FIN: Point = (0.545, 0.404)

#: The energy line inside the pointer: down the middle of the blade from a
#: little below the tip to the wrist. Placed on the arrow's own medial axis, so
#: at every size the line sits in the middle of the shape rather than near an
#: edge, and it is short enough that its glow never reaches the tip - a tip that
#: has been lit from behind is a tip that looks blunt.
POINTER_SPINE: Path = [
    (0.112, 0.132),
    (0.198, 0.356),
    (0.208, 0.520),
]

#: Where the pointer's tip is. The arrow is the only cursor whose hotspot is not
#: the middle of the image, and moving it is the whole difference between a
#: cursor that clicks where it points and one that clicks half a shape away. It
#: sits a hair inside the tip, where the two leading edges meet.
POINTER_HOTSPOT: Point = (0.076, 0.062)

#: Where the pointer's node sits: the lit dot at the wrist, which is the joint
#: the tail turns on. It is the arrow's one point of warm light, and putting it
#: at the wrist rather than at the tip is deliberate - a cursor is at its most
#: distracting exactly where it is pointing.
POINTER_NODE: Point = (0.214, 0.560)


# --- the caret ----------------------------------------------------------------
# An I-beam with machined serifs: the top and bottom bars are the plates the
# stem runs between, and the stem is narrow enough that the letter behind it is
# still readable. Drawn fat enough to survive being eleven pixels tall, because a
# hairline caret at 24 pixels is one grey pixel the eye loses over a white page.

CARET: Path = [
    (0.295, 0.085), (0.705, 0.085), (0.705, 0.200), (0.582, 0.200),
    (0.582, 0.800), (0.705, 0.800), (0.705, 0.915), (0.295, 0.915),
    (0.295, 0.800), (0.418, 0.800), (0.418, 0.200), (0.295, 0.200),
]

#: The energy line inside the caret: down the stem. It is what tells the two
#: serifs apart from a plain I-beam - a bar, a stem and a bar is the oldest shape
#: on the desktop, and a lit filament running down the middle of it is not.
CARET_SPINE: Path = [(0.5, 0.235), (0.5, 0.765)]


# --- the crosses --------------------------------------------------------------
# The reticle: a cross whose arms stop short of the middle, so the thing being
# aimed at can be seen, with the hole in the centre left as empty space. The hole
# is cut by the same trick :data:`FRAME` uses - the path walks the outside, slips
# in through a slit, walks around the hole and slips back out - so the cross is
# one polygon and the hole is a hole.

#: How far from the middle of the square a reticle's arms reach. Not to the
#: edge: the outline is grown outward from these points and the halo is grown
#: outward from that, so an arm written to 0.97 comes out cut off flat by its
#: own image at 24 pixels - and a crosshair with one arm shorter than the others
#: is the kind of thing that is noticed immediately and explained never.
ARM = 0.905

CROSS: Path = [
    (0.398, 1.0 - ARM), (0.602, 1.0 - ARM),     # the top of the vertical arm
    (0.602, 0.398),                             # down to the right arm
    (ARM, 0.398), (ARM, 0.602),                 # out along it and down its end
    (0.602, 0.602),                             # back in
    (0.602, ARM), (0.398, ARM),                 # down to the bottom and along it
    (0.398, 0.602),                             # back up
    (1.0 - ARM, 0.602), (1.0 - ARM, 0.398),     # the left arm
    (0.398, 0.398),                             # back in
    (0.398, 0.436),                             # down the slit
    (0.436, 0.436),                             # into the hole
    (0.436, 0.564), (0.564, 0.564), (0.564, 0.436),
    (0.436, 0.436),                             # around it
    (0.398, 0.436),                             # and back out
    (0.398, 0.398),
]

#: The four ticks that stick out of the reticle's arms, drawn as cold light:
#: two short strokes on each arm, so the crosshair reads as a sight rather than
#: as a plus sign. Written as one polyline per axis so they can be stroked.
CROSS_TICKS: tuple[Path, ...] = (
    [(1.0 - ARM, 0.5), (0.168, 0.5)],
    [(0.832, 0.5), (ARM, 0.5)],
    [(0.5, 1.0 - ARM), (0.5, 0.168)],
    [(0.5, 0.832), (0.5, ARM)],
)

#: The same cross at half the weight, for the precision pointer. Thin enough
#: that the middle of whatever is being selected is not covered, and with a
#: smaller hole so the arms do not become two separate bars at 24 pixels.
HAIRLINE: Path = [
    (0.462, 1.0 - ARM), (0.538, 1.0 - ARM),
    (0.538, 0.462),
    (ARM, 0.462), (ARM, 0.538),
    (0.538, 0.538),
    (0.538, ARM), (0.462, ARM),
    (0.462, 0.538),
    (1.0 - ARM, 0.538), (1.0 - ARM, 0.462),
    (0.462, 0.462),
    (0.462, 0.478),
    (0.478, 0.478),
    (0.478, 0.522), (0.522, 0.522), (0.522, 0.478),
    (0.478, 0.478),
    (0.462, 0.478),
    (0.462, 0.462),
]

#: A hollow square, for picking one cell of a grid. The wall is one path with a
#: slit in it: the ray crossing rule counts the hole as outside however the path
#: is wound, and a second square drawn inside the first would be exactly the
#: stacked shape this set is meant to have none of.
FRAME: Path = [
    (0.100, 0.100), (0.900, 0.100), (0.900, 0.900), (0.100, 0.900),
    (0.100, 0.760),   # up the leading edge to the slit
    (0.260, 0.760),   # through the wall, into the hole
    (0.260, 0.260), (0.740, 0.260), (0.740, 0.740), (0.260, 0.740),
    (0.100, 0.760),   # and back out through the slit
]

#: The corner ticks of the cell selector: a short stroke cutting each corner off
#: square, which is what turns a box into a bracket.
FRAME_CORNERS: tuple[Path, ...] = (
    [(0.100, 0.240), (0.240, 0.100)],
    [(0.760, 0.100), (0.900, 0.240)],
    [(0.900, 0.760), (0.760, 0.900)],
    [(0.240, 0.900), (0.100, 0.760)],
)


# --- the hands ----------------------------------------------------------------
# A hand is the one cursor that cannot be reduced to a bar and a head, so it is
# written as a silhouette: a palm with three knuckles along it and one finger
# standing out of the top. The knuckles are shallow - a little under a tenth of
# the cursor each - because they have to survive their own outline at twenty four
# pixels: a deep notch between two fingers is a notch the outline fills in, and
# what is left is a solid fork.

#: The pointing hand: an index finger up, the thumb out, and the folded fingers
#: told by the knuckles along the right of the palm.
HAND_OPEN: Path = [
    (0.200, 0.920), (0.800, 0.920),             # the palm, along the bottom
    (0.800, 0.760),                             # up its trailing edge
    (0.880, 0.700), (0.800, 0.620),             # the knuckles of the folded
    (0.880, 0.560), (0.800, 0.480),             # fingers, three shallow bumps
    (0.880, 0.420), (0.780, 0.360),
    (0.600, 0.380),                             # across the top of the fingers
    (0.600, 0.070), (0.420, 0.070),             # the index finger, standing up
    (0.420, 0.380),
    (0.220, 0.380),                             # across the top of the palm
    (0.160, 0.520),                             # down its leading edge
    (0.060, 0.620), (0.060, 0.800), (0.200, 0.860),   # the thumb
]

#: The energy line inside the open hand: up the index finger and into the palm,
#: which is the one place a hand has a spine.
HAND_OPEN_SPINE: Path = [(0.510, 0.130), (0.510, 0.330), (0.500, 0.470), (0.480, 0.700)]

#: The closed hand: a fist with the thumb folded across it. No finger stands up,
#: because that is the whole difference between the two states - and the knuckles
#: are the same three bumps in the same place, so a hand does not change shape
#: when it closes, which is what makes the pair read as one hand.
HAND_CLOSED: Path = [
    (0.180, 0.900), (0.800, 0.900),             # the palm, along the bottom
    (0.800, 0.780),                             # up its trailing edge
    (0.880, 0.720), (0.800, 0.640),             # the same three knuckles,
    (0.880, 0.580), (0.800, 0.500),             # curled over the fingers
    (0.880, 0.440), (0.800, 0.360),
    (0.700, 0.240),                             # the top of the fist
    (0.420, 0.220),
    (0.200, 0.340),
    (0.060, 0.480), (0.060, 0.680), (0.200, 0.760),   # the thumb, folded across
]

HAND_CLOSED_SPINE: Path = [(0.300, 0.760), (0.560, 0.700), (0.740, 0.520)]


# --- waiting ------------------------------------------------------------------
# An hourglass: two cones meeting at a waist that has a throat rather than a
# point, because two cones that meet at a single point are two triangles joined
# by nothing at twenty four pixels. The throat is the sand.

HOURGLASS: Path = [
    (0.190, 0.140), (0.810, 0.140),             # the upper cone
    (0.570, 0.470), (0.570, 0.530),             # the throat
    (0.810, 0.860), (0.190, 0.860),             # the lower cone
    (0.430, 0.530), (0.430, 0.470),
]

#: The two ribs across the hourglass: the plates the cones are clamped between.
#: They are strokes and not part of the polygon, so they can be drawn in the
#: cold light and the hourglass stays one shape.
HOURGLASS_RIBS: tuple[Path, ...] = (
    [(0.175, 0.163), (0.825, 0.163)],
    [(0.175, 0.837), (0.825, 0.837)],
)

#: The energy line down the hourglass's axis, from the upper rib to the lower.
HOURGLASS_SPINE: Path = [(0.500, 0.210), (0.500, 0.790)]


# --- the symbols that go inside a badge ---------------------------------------
# Each is written in its own 0..1 square and placed on the badge by
# :func:`placed`, so a symbol is never bigger than the badge it sits on.

PLUS: Path = [
    (0.360, 0.060), (0.640, 0.060), (0.640, 0.360), (0.940, 0.360),
    (0.940, 0.640), (0.640, 0.640), (0.640, 0.940), (0.360, 0.940),
    (0.360, 0.640), (0.060, 0.640), (0.060, 0.360), (0.360, 0.360),
]

MINUS: Path = [(0.060, 0.400), (0.940, 0.400), (0.940, 0.600), (0.060, 0.600)]

#: A small arrow, for the alias badge: a shortcut is not a copy, and the two
#: states are one glyph apart everywhere else on the desktop too.
ARROW: Path = [
    (0.940, 0.500), (0.640, 0.200), (0.640, 0.390), (0.060, 0.390),
    (0.060, 0.610), (0.640, 0.610), (0.640, 0.800),
]

#: A filled square, for the badge of the drag that moves a thing rather than
#: copying it.
BLOCK: Path = [(0.180, 0.180), (0.820, 0.180), (0.820, 0.820), (0.180, 0.820)]

#: One page, for the badge of a copy: the same square is placed twice, the second
#: time shifted and drawn with an outline, and the dark edge between them is what
#: says there are two of them. A copy and a zoom were the same plus sign before
#: this, which is one glyph for two actions that do not resemble each other.
SHEET: Path = [(0.100, 0.100), (0.900, 0.100), (0.900, 0.900), (0.100, 0.900)]


# --- the refusal --------------------------------------------------------------
# A horizontal bar, turned by an eighth of a circle to make the bar of a
# prohibition sign. Written lying down because that is the shape that is easy to
# read in this file, and turned where it is used so the angle is stated once.

SLASH: Path = [
    (0.060, 0.445), (0.940, 0.445), (0.940, 0.555), (0.060, 0.555),
]

#: The energy line down the bar of the refusal, running out a little past the
#: hexagon it crosses so the light reads as one stroke and not as a lit segment.
SLASH_SPINE: Path = [(0.115, 0.5), (0.885, 0.5)]

#: The ring of the refusal is not a path of its own: it is the same hexagonal
#: ring a spinner turns on, drawn closed and in one colour, so a refusal and a
#: waiting state are visibly the same part of the same machine. See
#: :func:`draw.poly_ring`.
