"""The states that turn: wait, progress, busy, watch and their two relatives.

These are the only states in the set that carry more than one frame, and they
are the only ones where a gap matters. A ring with a gap in it reads as
something turning even in a still frame; a closed ring reads as a target. So
what tells the waiting states apart is the gap - how much of the circle is
missing - and never a second shape drawn over the first.

The ring is a hexagon rather than a circle, and the hexagon spins rigidly with
its own gap. That one decision is most of what makes the waiting states read as
alien hardware instead of as a desktop doughnut: a circle is the shape every
toolkit already draws, and a six sided nut turning inside a cursor is not. It is
also why the ring is drawn with :func:`draw.poly_ring` and not by stroking a
hexagon's edges - the support function costs one pass over the bounding box,
which is what :func:`draw.arc` costs, where stroking a polygon costs one pass
per segment.

Inside the violet tube there is a colder filament, and at the leading end of the
arc there is a lit node. Those two are what make the ring read as a beam of
something being driven round a track rather than as a length of violet tube.

``watch`` is the exception and is an hourglass, which is the shape the X11 core
name has meant since before XRender existed. It carries the pulsing animation
for the same reason: an hourglass does not turn, it runs.
"""

from __future__ import annotations

import math
from typing import Callable

from . import palette, paths
from .canvas import Canvas
from .cursor import CORE, ENERGY, GLASS, OUTLINE, PLASMA, Geometry, dim, pulse_of, spin_of
from .draw import poly_ring, stroke
from .marks import Detail, paint, paint_core
from .states import CHANNEL, POINTER_DETAIL, State

Draw = Callable[[Canvas, Geometry, int, int], None]

#: How much of the hexagon a full spinner covers, in radians.
FULL = palette.SPINNER_SWEEP * math.tau

#: How many sides the ring has. The same number the badge has, so the parts of
#: the set that are meant to look machined are all the same kind of machined.
SIDES = palette.BADGE_SIDES

#: How wide the cold filament inside the ring is, as a fraction of the ring's
#: own thickness. Just over a third: wide enough to survive being one pixel at
#: twenty four, narrow enough that the violet is still the ring and the filament
#: is still a filament.
FILAMENT = 0.38


def _hex_reach(radius: float, angle: float, turn: float) -> float:
    """How far the hexagon's boundary is in the direction ``angle``.

    The polygon's support function. A hexagon is not a circle: its boundary is
    at the circumradius in the direction of a vertex and at the inradius in the
    direction of the middle of an edge, and the head of the spinner has to sit on
    the boundary rather than near it - a node floating a pixel off the end of the
    arc is the detail that reads as broken.
    """
    step = math.tau / SIDES
    local = (angle - turn * math.tau) % step - step * 0.5
    return radius * math.cos(math.pi / SIDES) / math.cos(local)


def _ring(canvas: Canvas, geometry: Geometry, start: float, sweep: float,
          turn: float) -> None:
    """One arc of the spinner: outline, violet tube, cold filament.

    The outline is the same arc drawn wider rather than a second arc drawn
    behind it: widening an annulus by ``outline`` on both radii is exactly what
    a grown edge is, and it cannot leave a dark line inside the light one.
    """
    radius, width = geometry.spinner
    centre = geometry.centre
    poly_ring(canvas, centre, centre, radius, width + 2.0 * geometry.outline,
              OUTLINE, start, sweep, SIDES, turn)
    poly_ring(canvas, centre, centre, radius, width, PLASMA, start, sweep,
              SIDES, turn)
    poly_ring(canvas, centre, centre, radius, width * FILAMENT, ENERGY,
              start, sweep, SIDES, turn)


def _head(canvas: Canvas, geometry: Geometry, angle: float,
          turn: float) -> None:
    """The lit node at the leading end of the arc.

    It is what makes the ring read as a gyro rather than as a broken nut: a
    light travelling around the rim is movement, and a ring of one colour with a
    gap in it is a ring of one colour. It sits on the hexagon's boundary rather
    than on the circumradius, so it stays welded to the end of the arc as the
    nut turns.
    """
    radius, width = geometry.spinner
    reach = _hex_reach(radius / geometry.size, angle, turn)
    paint_core(canvas, geometry,
               0.5 + reach * math.cos(angle),
               0.5 + reach * math.sin(angle),
               width / geometry.size * 0.42, CORE)


def _spin(canvas: Canvas, geometry: Geometry, frame: int, frames: int,
          sweep: float, clockwise: bool = True) -> None:
    """The ring, turned to where this frame puts it, with its head lit."""
    angle = spin_of(frame, frames)
    if not clockwise:
        angle = -angle
    turn = angle / math.tau
    _ring(canvas, geometry, angle - sweep, sweep, turn)
    _head(canvas, geometry, angle, turn)


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
    """Four short arcs: the ring broken into a dashed hexagon.

    A window that has stopped answering gets the ring with the most gaps in it,
    because at twenty pixels dashes are the thing that reads as "still going"
    even when the frame the user is looking at happens to be a still one.
    """
    step = math.tau / 4.0
    angle = spin_of(frame, frames)
    turn = angle / math.tau
    for index in range(4):
        _ring(canvas, geometry, angle + index * step, step * 0.55, turn)


def draw_half_busy(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """Two arcs facing each other: the half way point between the two."""
    step = math.pi
    angle = spin_of(frame, frames)
    turn = angle / math.tau
    for index in range(2):
        _ring(canvas, geometry, angle + index * step, step * 0.66, turn)


def draw_watch(canvas: Canvas, geometry: Geometry, frame: int,
               frames: int) -> None:
    """An hourglass with cold ribs, breathing between two thirds and full.

    The ribs are two strokes and not part of the polygon, so the cones stay one
    shape and the plates they are clamped between can be drawn in the cold
    light. ``dim`` rather than an alpha, because a pulse that fades the outline
    as well as the fill makes the whole shape vanish at the bottom of its
    breath, and an hourglass flickering out of existence is not an hourglass.
    """
    level = 0.62 + 0.38 * pulse_of(frame, frames)
    paint(canvas, geometry, paths.HOURGLASS, tone=dim(GLASS, level),
          detail=Detail(spines=(paths.HOURGLASS_SPINE,), channel=CHANNEL))
    for rib in paths.HOURGLASS_RIBS:
        line = [geometry.pixel(x, y) for x, y in rib]
        stroke(canvas, line, geometry.energy * 1.8, ENERGY, level,
               feather=max(0.6, geometry.energy))


def draw_left_pointer_watch(canvas: Canvas, geometry: Geometry, frame: int,
                            frames: int) -> None:
    """The pointer with a small turning hexagon at its lower right.

    This is the one waiting state that is two shapes, and it is two on purpose:
    it is asked for by name beside a window that is working, and the pointer is
    what says where the click went. The spinner sits where a badge sits, which
    is off the arrow's point.
    """
    paint(canvas, geometry, paths.POINTER, detail=POINTER_DETAIL)
    cx, cy = geometry.pixel(*palette.BADGE_CENTRE)
    outer = palette.BADGE_RADIUS * geometry.size
    radius = outer * 0.62
    width = outer * 0.32
    angle = spin_of(frame, frames)
    turn = angle / math.tau
    start = angle - FULL * 0.62
    poly_ring(canvas, cx, cy, radius, width + 2.0 * geometry.outline, OUTLINE,
              start, FULL * 0.62, SIDES, turn)
    poly_ring(canvas, cx, cy, radius, width, PLASMA, start, FULL * 0.62,
              SIDES, turn)
    poly_ring(canvas, cx, cy, radius, width * FILAMENT, ENERGY, start,
              FULL * 0.62, SIDES, turn)


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
