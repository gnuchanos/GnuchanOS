"""The navigation states: the pointer, the caret, the crosses, the resizing.

Each state is one shape from :mod:`paths` and one call to :func:`marks.paint`,
plus a bright core where the shape is big enough to hold one. Nothing is drawn
twice and nothing is drawn on top of anything else, which is the whole
difference between this set and the one it replaces - and it is why a state here
is three lines of code instead of a paragraph.

A state also carries its own hotspot. The arrow's is its tip; everything else is
the middle of its own shape. That matters more than it looks: a hotspot in the
middle of the image puts the click half a cursor away from the point of an
arrow, which at twenty four pixels is six pixels of error in every corner of
every window on the screen.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from . import paths
from .canvas import Canvas
from .cursor import RIM, Geometry
from .marks import paint, paint_core

Draw = Callable[[Canvas, Geometry, int, int], None]

#: The middle of the image, which is the hotspot of every state whose shape is
#: centred on it.
MIDDLE = (0.5, 0.5)

#: The radius of a core, in fractions of the nominal size, for the three kinds of
#: shape that hold one: a reticle's middle, a junction, and a small arrow's head.
CORE_RETICLE = 0.072
CORE_JUNCTION = 0.066
CORE_HEAD = 0.046


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


# --- a shape and nothing else -------------------------------------------------


def _shape(points: paths.Path, core: tuple[float, float, float] | None = None,
           thin: bool = False) -> Draw:
    """A state that is one shape from :mod:`paths`.

    Every plain state is this. ``core`` is where a bright dot of light goes, and
    the radius that comes with it; ``thin`` says the shape is too narrow to hold
    the darker inset, which is true of the arrows' shafts - a four pixel bar with
    a two pixel inset in it is a dark bar, not a detailed one.
    """

    def draw(canvas: Canvas, geometry: Geometry, frame: int, frames: int) -> None:
        paint(canvas, geometry, points, inset=None if thin else RIM)
        if core is not None:
            paint_core(canvas, geometry, core[0], core[1], core[2])

    return draw


def _turned(points: paths.Path, turns: float) -> Draw:
    """A single shape, rotated: the four resize bars are one path turned."""
    return _shape(paths.turned(points, turns), thin=True)


# --- the registry -------------------------------------------------------------
# The states this module knows, by the name the rest of the package uses. The
# names a desktop asks for, and which of these answers them, is a fact about the
# desktops and lives in :mod:`names`.

STATES: dict[str, State] = {
    # The pointer and the caret, the two every program on the desktop asks for
    # whether or not it asks for anything else.
    "pointer": State("pointer", _shape(paths.POINTER,
                                       (0.185, 0.245, CORE_HEAD)), 1,
                     paths.POINTER_HOTSPOT),
    "text": State("text", _shape(paths.CARET, thin=True), 1),

    # The precision pointer, the crosshair and the cell, which are three
    # different answers to "pick something out of what is on the screen": a
    # fine cross, a coarse one, and a box around the thing itself.
    "precise": State("precise", _shape(paths.HAIRLINE, thin=True), 1),
    "crosshair": State("crosshair", _shape(paths.CROSS,
                                           (0.5, 0.5, CORE_RETICLE)), 1),
    "cell": State("cell", _shape(paths.FRAME, thin=True), 1),

    # Moving and resizing. One bar and one arrow, turned into every direction
    # the desktop asks for; the four headed move is its own shape because four
    # overlapping arrows are four outlines crossing in the middle, and it is the
    # one resize cursor with a core in it - the middle of it is solid, where the
    # middle of a bar is a shaft.
    "move": State("move", _shape(paths.QUAD, (0.5, 0.5, CORE_JUNCTION)), 1),
    "all-scroll": State("all-scroll", _turned(paths.QUAD, 0.125), 1),
    "resize-horizontal": State("resize-horizontal",
                               _shape(paths.BAR, thin=True), 1),
    "resize-vertical": State("resize-vertical",
                             _turned(paths.BAR, 0.25), 1),
    "resize-diagonal-down": State("resize-diagonal-down",
                                  _turned(paths.BAR, 0.125), 1),
    "resize-diagonal-up": State("resize-diagonal-up",
                                _turned(paths.BAR, -0.125), 1),

    # The single direction arrows, which scroll bars and a few old programs ask
    # for by name.
    "arrow-right": State("arrow-right", _shape(paths.SHAFT, thin=True), 1),
    "arrow-down": State("arrow-down", _turned(paths.SHAFT, 0.25), 1),
    "arrow-left": State("arrow-left", _turned(paths.SHAFT, 0.5), 1),
    "arrow-up": State("arrow-up", _turned(paths.SHAFT, -0.25), 1),

    # The hands, for links and for dragging.
    "open-hand": State("open-hand", _shape(paths.MITTEN), 1),
    "closed-hand": State("closed-hand", _shape(paths.FIST), 1),
}
