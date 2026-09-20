"""The navigation states: the pointer, the caret, the crosshair, resizing.

Each state is the same dot with a different arrangement of marks around it, and
each is written once for every size the theme ships: the marks are placed as
fractions of the dot's own radius, which is itself a fraction of the nominal
size, so a state is neither 24 nor 48 pixels wide when it is drawn.

The registry at the end of this module is what :mod:`theme` reads. A state
carries the number of frames it needs, so the ones that spin can ask for more
than the ones that only breathe.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable

from . import palette
from .canvas import Canvas
from .cursor import (
    Geometry,
    MARK_COLOR,
    MARK_SHADOW_COLOR,
    clear_hole,
    draw_dot,
    draw_light,
    pulse_of,
)
from .draw import disc
from .marks import arrow, chevron, line, tick

Draw = Callable[[Canvas, Geometry, int, int], None]


@dataclass(frozen=True)
class State:
    """One cursor: how many frames it has and how to draw one of them."""

    name: str
    draw: Draw
    frames: int

    def render(self, geometry: Geometry, frame: int) -> Canvas:
        canvas = Canvas(geometry.size)
        self.draw(canvas, geometry, frame, self.frames)
        return canvas


# --- the plain states --------------------------------------------------------


def draw_pointer(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    """The arrow, as a dot. The one everything else is a variation of."""
    draw_dot(canvas, geometry, frame, frames)


def draw_precise(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    """The precision pointer: a dot with a hairline cross through it."""
    draw_dot(canvas, geometry, frame, frames)
    centre = geometry.centre
    span = geometry.radius * 1.9
    for angle in (0.0, math.pi / 2.0):
        line(
            canvas,
            centre - math.cos(angle) * span,
            centre - math.sin(angle) * span,
            centre + math.cos(angle) * span,
            centre + math.sin(angle) * span,
            max(1.0, geometry.mark * 0.65),
            MARK_COLOR,
            MARK_SHADOW_COLOR,
        )


def draw_text(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """The caret: a bar with a serif at each end, over a dimmed dot.

    The dot is still drawn, so the caret belongs to the same family, but its
    middle is darkened first - a light bar over the bright core would be a bar
    nobody could see.
    """
    draw_dot(canvas, geometry, frame, frames)
    clear_hole(canvas, geometry, geometry.radius * 0.92)
    centre = geometry.centre
    height = geometry.radius * 1.55
    serif = geometry.radius * 0.42
    width = geometry.mark
    line(canvas, centre, centre - height, centre, centre + height, width,
         MARK_COLOR, MARK_SHADOW_COLOR)
    line(canvas, centre - serif, centre - height, centre + serif, centre - height,
         width, MARK_COLOR, MARK_SHADOW_COLOR)
    line(canvas, centre - serif, centre + height, centre + serif, centre + height,
         width, MARK_COLOR, MARK_SHADOW_COLOR)


def draw_crosshair(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """Four ticks, no dot marks: what a drawing program wants for a corner."""
    draw_light(canvas, geometry, pulse_of(frame, frames), 0.62)
    centre = geometry.centre
    inner = geometry.radius * 1.15
    outer = geometry.radius * 2.05
    for index in range(4):
        tick(canvas, centre, centre, index * math.pi / 2.0, inner, outer,
             geometry.mark, MARK_COLOR, MARK_SHADOW_COLOR)


def draw_cell(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """A crosshair with the dot left whole: for picking a cell in a grid."""
    draw_dot(canvas, geometry, frame, frames)
    centre = geometry.centre
    for index in range(4):
        tick(canvas, centre, centre, index * math.pi / 2.0,
             geometry.radius * 0.72, geometry.radius * 1.25, geometry.mark,
             MARK_COLOR, MARK_SHADOW_COLOR)


# --- the directional states --------------------------------------------------


def _arrows(canvas: Canvas, geometry: Geometry, angles: list[float],
            inner_scale: float = 1.0, outer_scale: float = 1.95) -> None:
    """Arrows out of the dot, one per angle. Shared by every resizing state."""
    centre = geometry.centre
    for angle in angles:
        arrow(canvas, centre, centre, angle, geometry.radius * inner_scale,
              geometry.radius * outer_scale, geometry.mark, MARK_COLOR,
              MARK_SHADOW_COLOR)


def draw_move(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _arrows(canvas, geometry, [0.0, math.pi / 2.0, math.pi, 3 * math.pi / 2.0])


def draw_resize_horizontal(canvas: Canvas, geometry: Geometry, frame: int,
                           frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _arrows(canvas, geometry, [0.0, math.pi])


def draw_resize_vertical(canvas: Canvas, geometry: Geometry, frame: int,
                         frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _arrows(canvas, geometry, [math.pi / 2.0, 3 * math.pi / 2.0])


def draw_resize_diagonal_down(canvas: Canvas, geometry: Geometry, frame: int,
                              frames: int) -> None:
    """The north west to south east diagonal, which is the one that is drawn."""
    draw_dot(canvas, geometry, frame, frames)
    _arrows(canvas, geometry, [math.pi * 0.25, math.pi * 1.25])


def draw_resize_diagonal_up(canvas: Canvas, geometry: Geometry, frame: int,
                            frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _arrows(canvas, geometry, [math.pi * 0.75, math.pi * 1.75])


def _single_chevron(canvas: Canvas, geometry: Geometry, angle: float) -> None:
    centre = geometry.centre
    chevron(canvas, centre, centre, geometry.radius * 1.6, angle,
            geometry.mark, MARK_COLOR, MARK_SHADOW_COLOR)


def draw_arrow_up(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _single_chevron(canvas, geometry, -math.pi / 2.0)


def draw_arrow_down(canvas: Canvas, geometry: Geometry, frame: int,
                    frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _single_chevron(canvas, geometry, math.pi / 2.0)


def draw_arrow_left(canvas: Canvas, geometry: Geometry, frame: int,
                    frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _single_chevron(canvas, geometry, math.pi)


def draw_arrow_right(canvas: Canvas, geometry: Geometry, frame: int,
                     frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _single_chevron(canvas, geometry, 0.0)


def draw_all_scroll(canvas: Canvas, geometry: Geometry, frame: int,
                    frames: int) -> None:
    """Four chevrons at the diagonals: scroll in any direction."""
    draw_dot(canvas, geometry, frame, frames)
    centre = geometry.centre
    for index in range(4):
        angle = math.pi * 0.25 + index * math.pi / 2.0
        chevron(canvas, centre, centre, geometry.radius * 1.55, angle,
                geometry.mark * 0.9, MARK_COLOR, MARK_SHADOW_COLOR)


# --- the hand states ---------------------------------------------------------
# The hand is three small dots and a thumb rather than a drawn hand: at twenty
# pixels a drawn hand is a smudge, while four dots read as fingers immediately,
# and they keep the cursor in the family the rest of the set belongs to.


def _fingers(canvas: Canvas, geometry: Geometry, spread: float,
             reach: float) -> None:
    centre = geometry.centre
    finger = max(1.0, geometry.radius * 0.30)
    for index in range(3):
        angle = -math.pi / 2.0 + (index - 1) * spread
        disc(
            canvas,
            centre + math.cos(angle) * reach,
            centre + math.sin(angle) * reach,
            finger,
            MARK_COLOR,
        )


def draw_open_hand(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _fingers(canvas, geometry, 0.55, geometry.radius * 1.38)


def draw_closed_hand(canvas: Canvas, geometry: Geometry, frame: int,
                     frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _fingers(canvas, geometry, 0.34, geometry.radius * 1.08)
    centre = geometry.centre
    disc(canvas, centre + geometry.radius * 1.15, centre + geometry.radius * 0.35,
         max(1.0, geometry.radius * 0.34), MARK_COLOR)


# --- the registry ------------------------------------------------------------
# The states this module knows, by the name the drawing uses. The mapping from
# the names a desktop asks for to these is in :mod:`theme`, because which
# desktop name means "a hand" is a fact about the desktops and not about the
# drawing.

STATES: dict[str, State] = {
    "pointer": State("pointer", draw_pointer, palette.FRAMES),
    "precise": State("precise", draw_precise, palette.FRAMES),
    "text": State("text", draw_text, palette.FRAMES),
    "crosshair": State("crosshair", draw_crosshair, palette.FRAMES),
    "cell": State("cell", draw_cell, palette.FRAMES),
    "move": State("move", draw_move, palette.FRAMES),
    "resize-horizontal": State("resize-horizontal", draw_resize_horizontal, palette.FRAMES),
    "resize-vertical": State("resize-vertical", draw_resize_vertical, palette.FRAMES),
    "resize-diagonal-down": State("resize-diagonal-down", draw_resize_diagonal_down, palette.FRAMES),
    "resize-diagonal-up": State("resize-diagonal-up", draw_resize_diagonal_up, palette.FRAMES),
    "arrow-up": State("arrow-up", draw_arrow_up, palette.FRAMES),
    "arrow-down": State("arrow-down", draw_arrow_down, palette.FRAMES),
    "arrow-left": State("arrow-left", draw_arrow_left, palette.FRAMES),
    "arrow-right": State("arrow-right", draw_arrow_right, palette.FRAMES),
    "all-scroll": State("all-scroll", draw_all_scroll, palette.FRAMES),
    "open-hand": State("open-hand", draw_open_hand, palette.FRAMES),
    "closed-hand": State("closed-hand", draw_closed_hand, palette.FRAMES),
}
