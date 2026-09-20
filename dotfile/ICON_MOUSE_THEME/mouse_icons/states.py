"""The navigation states: the pointer, the caret, the crosses, the resizing.

Each state is one shape from :mod:`paths`, its detail from the same module, and
one call to :func:`marks.paint` - with the groove and the panel lit, or with a
single flat tone where the shape is too thin to hold a groove, plus the cold
energy line down whatever spine the shape names. Nothing is drawn twice and
nothing is drawn on top of anything else, which is the whole difference between
this set and the one it replaces.

A state also carries its own hotspot. The arrow's is its tip; everything else is
the middle of its own shape. That matters more than it looks: a hotspot in the
middle of the image puts the click half a cursor away from the point of an arrow,
which at twenty four pixels is six pixels of error in every corner of every
window on the screen.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from . import paths
from .canvas import Canvas
from .cursor import GLASS, SOLID, Geometry, Tone
from .marks import PLAIN, Detail, paint, paint_spark

Draw = Callable[[Canvas, Geometry, int, int], None]

#: The middle of the image, which is the hotspot of every state whose shape is
#: centred on it.
MIDDLE = (0.5, 0.5)

#: How wide the dark channel around an energy line is, on a shape with room for
#: one. Two and a half times the filament's own width is the point where the
#: channel reads as a groove the light is sitting in rather than as a dark line
#: drawn beside it.
CHANNEL = 2.4

#: The same, on a shape whose walls are only a couple of pixels thick: enough to
#: separate the filament from the armour, not enough to eat the shape.
THIN_CHANNEL = 1.5

#: The radius of the spark in the middle of the cell selector, in fractions of
#: the nominal size. It sits in a hole ten pixels across at twenty four, so it
#: can afford to be a star rather than a dot.
SPARK_CELL = 0.070


@dataclass(frozen=True)
class State:
    """One cursor: what to draw, how many frames, and where the click lands."""

    name: str
    draw: Draw
    frames: int
    hotspot: tuple[float, float] = MIDDLE

    def render(self, geometry: Geometry, frame: int) -> Canvas:
        canvas = Canvas(geometry.size)
        self.draw(canvas, geometry, frame, self.frames)
        return canvas

    def hotspot_pixels(self, size: int) -> tuple[int, int]:
        """The hotspot of this state at a given image size, in pixels."""
        return Geometry(size).hotspot(*self.hotspot)


# --- turning a detail with the shape it belongs to ----------------------------


def turned_detail(detail: Detail, turns: float) -> Detail:
    """``detail`` rotated by ``turns``, spines and nodes together.

    A detail is written in the same square as the shape it belongs to, so a
    shape that is drawn turned has to have its energy line turned with it - a
    spine that stays where it was written while the bar around it rotates is a
    filament running out of the side of the cursor.
    """
    if turns == 0.0:
        return detail
    return Detail(
        spines=tuple(paths.turned(spine, turns) for spine in detail.spines),
        nodes=tuple(paths.turned([node], turns)[0] for node in detail.nodes),
        channel=detail.channel,
        core=detail.core,
    )


# --- a shape and nothing else -------------------------------------------------


def _shape(points: paths.Path, tone: Tone = GLASS,
           detail: Detail | None = None) -> Draw:
    """A state that is one shape from :mod:`paths`.

    Every plain state is this. ``tone`` says how deep the shape is allowed to
    be: the whole armour-and-panel ramp where there is room, and a single flat
    tone where there is not, because a four pixel bar with a groove in it is a
    four pixel bar with a dark line down it.
    """

    def draw(canvas: Canvas, geometry: Geometry, frame: int, frames: int) -> None:
        paint(canvas, geometry, points, tone=tone,
              detail=detail if detail is not None else PLAIN)

    return draw


def _solid(points: paths.Path, spines: tuple[paths.Path, ...] = (),
           nodes: tuple[paths.Point, ...] = (),
           spark: float | None = None) -> Draw:
    """A shape too shallow for a groove: flat armour and a lit spine.

    A resize bar, a caret and a chevron are all this. They keep the cold
    filament down the middle - that is what stops a flat bar from being a flat
    bar - but no groove, because a groove on a three pixel shaft is the shaft.
    """
    detail = Detail(spines=spines, nodes=nodes, channel=THIN_CHANNEL)

    def draw(canvas: Canvas, geometry: Geometry, frame: int, frames: int) -> None:
        paint(canvas, geometry, points, tone=SOLID, detail=detail)
        if spark is not None:
            paint_spark(canvas, geometry, 0.5, 0.5, spark)

    return draw


# --- the registry -------------------------------------------------------------
# The states this module knows, by the name the rest of the package uses. The
# names a desktop asks for, and which of these answers them, is a fact about the
# desktops and lives in :mod:`names`.

#: The pointer's own detail: the spine down the blade and the node at the wrist.
#: Public because the states that carry a badge draw the same pointer, and a
#: dialog cursor whose arrow had no spine in it would be a different cursor from
#: the one the desktop shows everywhere else.
POINTER_DETAIL = Detail(
    spines=(paths.POINTER_SPINE,),
    nodes=(paths.POINTER_NODE,),
    channel=CHANNEL,
)

#: The resize bar's filament and the scroll arrow's, turned into the eight
#: directions they are used in.
BAR_DETAIL = Detail(spines=(paths.BAR_SPINE,), channel=THIN_CHANNEL)
SHAFT_DETAIL = Detail(spines=(paths.SHAFT_SPINE,), channel=THIN_CHANNEL)

STATES: dict[str, State] = {
    # The pointer and the caret, the two every program on the desktop asks for
    # whether or not it asks for anything else.
    "pointer": State("pointer", _shape(paths.POINTER, GLASS, POINTER_DETAIL),
                     1, paths.POINTER_HOTSPOT),
    "text": State("text", _solid(paths.CARET, (paths.CARET_SPINE,)), 1),

    # The precision pointer, the reticle and the cell, which are three different
    # answers to "pick something out of what is on the screen": a fine cross with
    # no light in it at all, a coarse one with ticks and a lit core, and a box
    # around the thing itself with a spark in the middle of it.
    "precise": State("precise", _solid(paths.HAIRLINE), 1),
    "crosshair": State(
        "crosshair",
        _shape(paths.CROSS, GLASS,
               Detail(spines=paths.CROSS_TICKS, nodes=(MIDDLE,),
                      channel=0.0)),
        1,
    ),
    "cell": State("cell", _solid(paths.FRAME, paths.FRAME_CORNERS, (),
                                 spark=SPARK_CELL), 1),

    # Moving and resizing. One bar and one arrow, turned into every direction
    # the desktop asks for; the four headed move is its own shape because four
    # overlapping arrows are four outlines crossing in the middle, and it is the
    # one resize cursor with a lit hub in it - a junction with no light in it is
    # a lump of violet where four shapes meet.
    "move": State(
        "move",
        _shape(paths.QUAD, GLASS,
               Detail(spines=paths.QUAD_SPINES, nodes=(MIDDLE,),
                      channel=CHANNEL)),
        1,
    ),
    "all-scroll": State(
        "all-scroll",
        _shape(paths.turned(paths.QUAD, 0.125), GLASS,
               Detail(spines=tuple(paths.turned(spine, 0.125)
                                   for spine in paths.QUAD_SPINES),
                      nodes=(MIDDLE,), channel=CHANNEL)),
        1,
    ),
    "resize-horizontal": State(
        "resize-horizontal", _shape(paths.BAR, SOLID, BAR_DETAIL), 1),
    "resize-vertical": State(
        "resize-vertical",
        _shape(paths.turned(paths.BAR, 0.25), SOLID,
               turned_detail(BAR_DETAIL, 0.25)),
        1,
    ),
    "resize-diagonal-down": State(
        "resize-diagonal-down",
        _shape(paths.turned(paths.BAR, 0.125), SOLID,
               turned_detail(BAR_DETAIL, 0.125)),
        1,
    ),
    "resize-diagonal-up": State(
        "resize-diagonal-up",
        _shape(paths.turned(paths.BAR, -0.125), SOLID,
               turned_detail(BAR_DETAIL, -0.125)),
        1,
    ),

    # The single direction arrows, which scroll bars and a few old programs ask
    # for by name.
    "arrow-right": State("arrow-right",
                         _shape(paths.SHAFT, SOLID, SHAFT_DETAIL), 1),
    "arrow-down": State(
        "arrow-down",
        _shape(paths.turned(paths.SHAFT, 0.25), SOLID,
               turned_detail(SHAFT_DETAIL, 0.25)),
        1,
    ),
    "arrow-left": State(
        "arrow-left",
        _shape(paths.turned(paths.SHAFT, 0.5), SOLID,
               turned_detail(SHAFT_DETAIL, 0.5)),
        1,
    ),
    "arrow-up": State(
        "arrow-up",
        _shape(paths.turned(paths.SHAFT, -0.25), SOLID,
               turned_detail(SHAFT_DETAIL, -0.25)),
        1,
    ),

    # The hands, for links and for dragging. Both are one silhouette with the
    # knuckles drawn into it, which is what stops a hand from arriving as two
    # shapes - the fork the last version of this pair came out as was one
    # outline per finger, and at twenty four pixels their outlines met.
    "open-hand": State(
        "open-hand",
        _shape(paths.HAND_OPEN, GLASS,
               Detail(spines=(paths.HAND_OPEN_SPINE,), channel=THIN_CHANNEL)),
        1,
    ),
    "closed-hand": State(
        "closed-hand",
        _shape(paths.HAND_CLOSED, GLASS,
               Detail(spines=(paths.HAND_CLOSED_SPINE,), channel=THIN_CHANNEL)),
        1,
    ),
}
