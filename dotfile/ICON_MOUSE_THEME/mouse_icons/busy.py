"""The states that turn: wait, progress, busy, watch and their two relatives.

These are the only states in the set that carry more than one frame, and they
are the only ones where a gap matters. A ring with a gap in it reads as
something turning even in a still frame; a closed ring reads as a target. So
what tells the waiting states apart is the gap - how much of the circle is
missing - and never a second shape drawn over the first.

``watch`` is the exception and is an hourglass, which is the shape the X11 core
name has meant since before XRender existed. It carries the pulsing animation
for the same reason: an hourglass does not turn, it runs.
"""

from __future__ import annotations

import math
from typing import Callable

from . import palette, paths
from .canvas import Canvas
from .cursor import CORE, FILL, Geometry, LIGHT, OUTLINE, fade, pulse_of, spin_of
from .draw import arc
from .marks import paint, paint_core
from .states import State

Draw = Callable[[Canvas, Geometry, int, int], None]

#: How much of the circle a full spinner covers, in radians.
FULL = palette.SPINNER_SWEEP * math.tau


def _ring(canvas: Canvas, geometry: Geometry, color, start: float,
          sweep: float) -> None:
    """One arc of a spinner, outline first, at the size the palette asks for.

    The outline is the same arc drawn wider rather than a second arc: widening
    an annulus by ``outline`` on both radii is exactly what a grown edge is, and
    it cannot leave a dark line inside the light one.
    """
    radius, width = geometry.spinner
    centre = geometry.centre
    arc(canvas, centre, centre, radius, width + 2.0 * geometry.outline,
        OUTLINE, start, sweep)
    arc(canvas, centre, centre, radius, width, color, start, sweep)


def _spin(canvas: Canvas, geometry: Geometry, frame: int, frames: int,
          sweep: float, clockwise: bool = True) -> None:
    """The ring, turned to where this frame puts it, with its head lit.

    The head is the leading end of the arc, and it carries the bright core the
    rest of the set uses for its details. It is what makes the ring read as a
    gyro rather than as a broken circle: a light travelling around the rim is
    movement, and a ring of one colour with a gap in it is a ring of one colour.
    """
    angle = spin_of(frame, frames)
    if not clockwise:
        angle = -angle
    _ring(canvas, geometry, LIGHT, angle - sweep, sweep)
    paint_core(
        canvas,
        geometry,
        0.5 + palette.SPINNER_REACH * math.cos(angle),
        0.5 + palette.SPINNER_REACH * math.sin(angle),
        palette.SPINNER_WIDTH * 0.44,
        CORE,
    )


# --- the states ---------------------------------------------------------------


def draw_wait(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """The spinner: one gap, turning clockwise. A program is working."""
    _spin(canvas, geometry, frame, frames, FULL)


def draw_progress(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    """The same ring with a much larger gap, turning the other way.

    A program waiting for the user and a program working are the two things a
    spinner exists to tell apart, and the direction and the length of the arc
    are what say which is which.
    """
    _spin(canvas, geometry, frame, frames, FULL * 0.42, clockwise=False)


def draw_busy(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """Four short arcs: the ring broken into a dashed circle.

    A window that has stopped answering gets the ring with the most gaps in it,
    because at twenty pixels a dashes are the thing that reads as "still going"
    even when the frame the user is looking at happens to be a still one.
    """
    step = math.tau / 4.0
    angle = spin_of(frame, frames)
    for index in range(4):
        _ring(canvas, geometry, LIGHT, angle + index * step, step * 0.55)


def draw_half_busy(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """Two arcs facing each other: the half way point between the two."""
    step = math.pi
    angle = spin_of(frame, frames)
    for index in range(2):
        _ring(canvas, geometry, LIGHT, angle + index * step, step * 0.66)


def draw_watch(canvas: Canvas, geometry: Geometry, frame: int,
               frames: int) -> None:
    """An hourglass, breathing between two thirds and its full colour."""
    paint(canvas, geometry, paths.HOURGLASS,
          fill=fade(FILL, 0.62 + 0.38 * pulse_of(frame, frames)))


def draw_left_pointer_watch(canvas: Canvas, geometry: Geometry, frame: int,
                            frames: int) -> None:
    """The pointer with a small spinner at its lower right.

    This is the one waiting state that is two shapes, and it is two on purpose:
    it is asked for by name beside a window that is working, and the pointer is
    what says where the click went. The spinner sits where a badge sits, which
    is off the arrow's point.
    """
    paint(canvas, geometry, paths.POINTER)
    cx, cy = geometry.pixel(*palette.BADGE_CENTRE)
    outer = palette.BADGE_RADIUS * geometry.size
    radius = outer * 0.60
    width = outer * 0.34
    start = spin_of(frame, frames) - FULL * 0.66
    arc(canvas, cx, cy, radius, width + 2.0 * geometry.outline, OUTLINE,
        start, FULL * 0.66)
    arc(canvas, cx, cy, radius, width, LIGHT, start, FULL * 0.66)


# --- the registry -------------------------------------------------------------

BUSY_STATES: dict[str, State] = {
    "wait": State("wait", draw_wait, palette.FRAMES_SPIN),
    "progress": State("progress", draw_progress, palette.FRAMES_SPIN),
    "busy": State("busy", draw_busy, palette.FRAMES_SPIN),
    "half-busy": State("half-busy", draw_half_busy, palette.FRAMES_SPIN),
    "watch": State("watch", draw_watch, palette.FRAMES),
    "left-pointer-watch": State("left-pointer-watch", draw_left_pointer_watch,
                                palette.FRAMES_SPIN,
                                paths.POINTER_HOTSPOT),
}
