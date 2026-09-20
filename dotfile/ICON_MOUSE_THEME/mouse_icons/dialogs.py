"""The states that carry a symbol: forbidden, help, zoom, copy and drag.

These are the states a window uses to say what a click will do before it is
made: no, look at this, closer, further, copy it, move it. They share a shape
because they share a problem - the dot already fills the cursor, so the symbol
has to go somewhere - and the answer is a badge: a smaller disc at the lower
right, drawn over the dot, carrying the symbol.

That is the one place this set breaks its own rule that the dot is always the
whole cursor. It does so because the alternative is worse: an arrow with a
chevron crossing its shaft is unreadable at twenty pixels, while a badge is a
shape the eye already knows from every icon theme on the desktop.
"""

from __future__ import annotations

import math

from . import palette
from .canvas import Canvas
from .cursor import (
    Geometry,
    MARK_COLOR,
    MARK_SHADOW_COLOR,
    RING_COLOR,
    SHADOW_COLOR,
    clear_hole,
    draw_dot,
)
from .draw import arc, disc, ring
from .marks import line, plus, slash

#: Where the badge sits, as a multiple of the dot radius, and how big it is.
BADGE_OFFSET = 0.92
BADGE_RADIUS = 0.66


def _badge_centre(geometry: Geometry) -> tuple[float, float]:
    """The lower right of the dot, where a badge goes.

    Lower right and not upper left: a cursor's hotspot is its middle, and what
    is drawn below and to the right of it covers the part of the screen the
    pointer is least likely to be about to act on.
    """
    return (
        geometry.centre + geometry.radius * BADGE_OFFSET,
        geometry.centre + geometry.radius * BADGE_OFFSET,
    )


def _badge_backing(canvas: Canvas, geometry: Geometry) -> tuple[float, float, float]:
    """Draw the badge disc and return where it is, so a symbol can be drawn on it."""
    cx, cy = _badge_centre(geometry)
    radius = geometry.radius * BADGE_RADIUS
    disc(canvas, cx, cy, radius + geometry.radius * 0.14, SHADOW_COLOR)
    disc(canvas, cx, cy, radius, RING_COLOR)
    return cx, cy, radius


# --- the symbols -------------------------------------------------------------


def _draw_cross_mark(canvas: Canvas, geometry: Geometry) -> None:
    """The X of a refusal: two strokes across the badge."""
    cx, cy, radius = _badge_backing(canvas, geometry)
    width = max(1.0, geometry.mark * 0.8)
    reach = radius * 0.62
    for angle in (math.pi * 0.25, math.pi * 0.75):
        line(
            canvas,
            cx - math.cos(angle) * reach,
            cy - math.sin(angle) * reach,
            cx + math.cos(angle) * reach,
            cy + math.sin(angle) * reach,
            width,
            MARK_COLOR,
            MARK_SHADOW_COLOR,
        )


def _draw_question(canvas: Canvas, geometry: Geometry) -> None:
    """A question mark: the hook, the stem, the dot.

    Drawn as three primitives rather than as a glyph, because there is no font
    in this program and a question mark is small enough to be drawn from an arc
    and two marks.
    """
    cx, cy, radius = _badge_backing(canvas, geometry)
    width = max(1.0, geometry.mark * 0.7)
    # The hook, counter-clockwise from the left so it opens at the bottom.
    arc(canvas, cx, cy - radius * 0.26, radius * 0.34, width, MARK_COLOR,
        math.pi * 0.62, math.pi * 1.3)
    line(canvas, cx, cy - radius * 0.02, cx, cy + radius * 0.26, width,
         MARK_COLOR, MARK_SHADOW_COLOR)
    disc(canvas, cx, cy + radius * 0.58, max(0.8, width * 0.62), MARK_COLOR)


def _draw_plus(canvas: Canvas, geometry: Geometry, vertical: bool) -> None:
    cx, cy, radius = _badge_backing(canvas, geometry)
    plus(canvas, cx, cy, radius * 0.62, max(1.0, geometry.mark * 0.85),
         MARK_COLOR, vertical, MARK_SHADOW_COLOR)


def _draw_link(canvas: Canvas, geometry: Geometry) -> None:
    """A chain link, as two interlocking arcs."""
    cx, cy, radius = _badge_backing(canvas, geometry)
    width = max(1.0, geometry.mark * 0.75)
    for offset in (-1.0, 1.0):
        ring(canvas, cx + radius * 0.3 * offset, cy + radius * 0.3 * offset,
             radius * 0.34, width, MARK_COLOR)


def _draw_move_badge(canvas: Canvas, geometry: Geometry) -> None:
    """Two chevrons pointing opposite ways: the drag and drop move state."""
    cx, cy, radius = _badge_backing(canvas, geometry)
    width = max(1.0, geometry.mark * 0.75)
    for index in range(2):
        angle = math.pi * 0.25 + index * math.pi
        reach = radius * 0.6
        tip_x = cx + math.cos(angle) * reach
        tip_y = cy + math.sin(angle) * reach
        spread = reach * 0.7
        line(
            canvas,
            tip_x - math.cos(angle + math.pi / 2) * spread,
            tip_y - math.sin(angle + math.pi / 2) * spread,
            tip_x,
            tip_y,
            width,
            MARK_COLOR,
            MARK_SHADOW_COLOR,
        )
        line(
            canvas,
            tip_x,
            tip_y,
            tip_x + math.cos(angle + math.pi / 2) * spread,
            tip_y + math.sin(angle + math.pi / 2) * spread,
            width,
            MARK_COLOR,
            MARK_SHADOW_COLOR,
        )


# --- the states --------------------------------------------------------------


def draw_forbidden(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """The bar itself, across the whole cursor.

    This is the one state that does not use a badge: the forbidden cursor is
    recognised by a bar through it and by nothing else, and moving that bar into
    a corner would leave a cursor that reads as "here" rather than as "no".
    """
    clear_hole(canvas, geometry, geometry.radius * 0.98, 0.75)
    ring(canvas, geometry.centre, geometry.centre, geometry.radius,
         max(1.0, geometry.mark), RING_COLOR)
    slash(canvas, geometry.centre, geometry.centre, geometry.radius * 1.42,
          math.pi * 0.25, max(1.0, geometry.mark * 1.15), MARK_COLOR,
          MARK_SHADOW_COLOR)


def draw_help(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_question(canvas, geometry)


def draw_zoom_in(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_plus(canvas, geometry, vertical=True)


def draw_zoom_out(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_plus(canvas, geometry, vertical=False)


def draw_copy(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_plus(canvas, geometry, vertical=True)


def draw_alias(canvas: Canvas, geometry: Geometry, frame: int,
               frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_link(canvas, geometry)


def draw_dnd_move(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_move_badge(canvas, geometry)


def draw_dnd_copy(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_plus(canvas, geometry, vertical=True)


def draw_dnd_link(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_link(canvas, geometry)


def draw_dnd_none(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_cross_mark(canvas, geometry)


def draw_dnd_ask(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    draw_dot(canvas, geometry, frame, frames)
    _draw_question(canvas, geometry)


DIALOG_STATES: dict[str, "object"] = {}


def _register() -> dict[str, object]:
    """Build the registry, importing State here to avoid a circular import.

    :mod:`states` does not know about this module and this module needs the type
    it defines, so the import happens inside the function rather than at the top
    of the file. The alternative - moving State into this module - would put a
    dataclass in a file about drawing symbols.
    """
    from .states import State

    return {
        "forbidden": State("forbidden", draw_forbidden, palette.FRAMES),
        "help": State("help", draw_help, palette.FRAMES),
        "zoom-in": State("zoom-in", draw_zoom_in, palette.FRAMES),
        "zoom-out": State("zoom-out", draw_zoom_out, palette.FRAMES),
        "copy": State("copy", draw_copy, palette.FRAMES),
        "alias": State("alias", draw_alias, palette.FRAMES),
        "dnd-move": State("dnd-move", draw_dnd_move, palette.FRAMES),
        "dnd-copy": State("dnd-copy", draw_dnd_copy, palette.FRAMES),
        "dnd-link": State("dnd-link", draw_dnd_link, palette.FRAMES),
        "dnd-none": State("dnd-none", draw_dnd_none, palette.FRAMES),
        "dnd-ask": State("dnd-ask", draw_dnd_ask, palette.FRAMES),
    }


DIALOG_STATES = _register()
