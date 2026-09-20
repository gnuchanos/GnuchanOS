"""The states that spin: wait, progress and busy.

These are the three states where the cursor has to say something has not
finished, and they are the only ones that are timed rather than merely animated.
A pulse says "here I am"; a rotation says "still working", and it is the
rotation, not the colour, that carries that - which is why the ring turns through
a full circle in one cycle while the dot underneath keeps the same slow pulse as
every other state in the set.

The ring fades behind its head. A ring of one brightness turning looks like a
ring that is being rotated by hand; a ring that is bright at the head and fades
to nothing behind it reads as movement even in a still frame, and reads as a
direction as soon as it moves.
"""

from __future__ import annotations

import math

from . import palette
from .canvas import Canvas
from .cursor import (
    Geometry,
    MARK_COLOR,
    RING_COLOR,
    clear_hole,
    draw_dot,
    spin_of,
)
from .draw import ring, tapered_arc
from .marks import line
from .states import State, draw_pointer

#: How far out the ring sits, as a multiple of the dot radius. Just outside the
#: rim, so the two do not overlap and the ring reads as separate from the dot.
RING_REACH = 1.42

#: How much of the circle the head covers, in radians. A little over a third:
#: short enough that the gap reads as a gap, long enough to look deliberate.
RING_SWEEP = math.tau * 0.72


def _spin_ring(canvas: Canvas, geometry: Geometry, frame: int, frames: int,
               clockwise: bool = True) -> None:
    """The rotating arc, drawn outside the dot."""
    angle = spin_of(frame, frames)
    if not clockwise:
        # The counter-clockwise pair is what tells a program waiting for input
        # apart from one that is working; the two are drawn as mirrors.
        angle = -angle
    tapered_arc(
        canvas,
        geometry.centre,
        geometry.centre,
        geometry.radius * RING_REACH,
        max(1.0, geometry.mark * 0.85),
        RING_COLOR,
        angle - RING_SWEEP,
        RING_SWEEP,
        0.95,
    )


def draw_wait(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """The busy ring around a dot: the cursor for a program that is working."""
    draw_dot(canvas, geometry, frame, frames)
    _spin_ring(canvas, geometry, frame, frames)


def draw_progress(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    """The ring, counter-clockwise, over a dot with its middle opened out.

    A program that is waiting for input rather than working gets the mirror of
    the ring, and that contrast is the whole difference between the two states.
    """
    draw_dot(canvas, geometry, frame, frames)
    clear_hole(canvas, geometry, geometry.radius * 0.62, 0.35)
    _spin_ring(canvas, geometry, frame, frames, clockwise=False)


def draw_busy(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """The ring plus a steady bar: "not now", for a window that cannot answer.

    The bar is what distinguishes it from the wait state at a glance, and it is
    drawn instead of the bright core rather than over it, so the two states do
    not look like the same cursor on two frames of one animation.
    """
    draw_dot(canvas, geometry, frame, frames)
    clear_hole(canvas, geometry, geometry.radius * 0.86)
    centre = geometry.centre
    bar = geometry.radius * 0.76
    line(canvas, centre - bar, centre, centre + bar, centre,
         max(1.0, geometry.mark), MARK_COLOR, None)
    _spin_ring(canvas, geometry, frame, frames)


def draw_watch(canvas: Canvas, geometry: Geometry, frame: int,
               frames: int) -> None:
    """The same rotation drawn as a full ring with a travelling highlight.

    Some desktops ask for ``watch`` and mean the plain hourglass shape; drawing
    the dot with a complete ring around it is that shape in this set's language,
    and the highlight is what keeps it animated.
    """
    draw_dot(canvas, geometry, frame, frames)
    ring(canvas, geometry.centre, geometry.centre, geometry.radius * RING_REACH,
         max(1.0, geometry.mark * 0.7), RING_COLOR, 0.22)
    _spin_ring(canvas, geometry, frame, frames)


def draw_left_pointer_watch(canvas: Canvas, geometry: Geometry, frame: int,
                            frames: int) -> None:
    """The pointer with the ring: what a desktop shows beside a busy window.

    Drawn as the pointer and not as a bare ring because this one is asked for by
    name next to a window that is working, and the pointer is what says where the
    click went.
    """
    draw_pointer(canvas, geometry, frame, palette.FRAMES)
    _spin_ring(canvas, geometry, frame, frames)


def draw_half_busy(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """A ring of two halves: the state some toolkits use for a partial wait."""
    draw_dot(canvas, geometry, frame, frames)
    angle = spin_of(frame, frames)
    for half in range(2):
        start = angle + half * math.pi
        tapered_arc(
            canvas,
            geometry.centre,
            geometry.centre,
            geometry.radius * RING_REACH,
            max(1.0, geometry.mark * 0.8),
            RING_COLOR,
            start,
            math.pi * 0.72,
            0.9,
        )


BUSY_STATES: dict[str, State] = {
    "wait": State("wait", draw_wait, palette.FRAMES_SPIN),
    "progress": State("progress", draw_progress, palette.FRAMES_SPIN),
    "busy": State("busy", draw_busy, palette.FRAMES_SPIN),
    "watch": State("watch", draw_watch, palette.FRAMES_SPIN),
    "left-pointer-watch": State("left-pointer-watch", draw_left_pointer_watch,
                                palette.FRAMES_SPIN),
    "half-busy": State("half-busy", draw_half_busy, palette.FRAMES_SPIN),
}
