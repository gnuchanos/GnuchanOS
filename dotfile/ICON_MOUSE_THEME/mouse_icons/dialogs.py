"""The states that carry a symbol: forbidden, help, zoom, copy and the drags.

Everything here except the forbidden cursor is the pointer with a badge at its
lower right - the part of the screen the point of the arrow is not about to act
on. A badge is the one place this set draws two shapes, and it is the only way
to say "this click will copy" at twenty pixels: there is no room inside the
arrow, and a second arrow crossing the first is the stacked shape the whole set
is meant to avoid. The pointer under the badge is drawn from the same detail as
the pointer everywhere else, so a dialog cursor is the same arrow with something
added rather than a different arrow.

The forbidden cursor is the exception, because it is not about a pointer at all:
it is the hexagon and the bar, which is the shape that sign has had since long
before it was a cursor. Here it is drawn as a lit sign - the ring is a hexagon
with the same cold filament inside it as a spinner has, and the bar carries the
filament too - so a refusal looks like the rest of the set rather than like a
flat red circle with a line through it.
"""

from __future__ import annotations

import math
from typing import Callable

from . import palette, paths
from .canvas import Canvas
from .cursor import ENERGY, GLASS, OUTLINE, PLASMA, SYMBOL, Geometry
from .draw import arc, disc, polygon, poly_ring
from .marks import Detail, paint, paint_badge, paint_symbol
from .states import POINTER_DETAIL, State

Draw = Callable[[Canvas, Geometry, int, int], None]

#: How wide the cold filament in the refusal's ring is, as a fraction of the
#: ring. The same fraction the spinner uses, stated here because the two are
#: the same sign at two different moments.
FILAMENT = 0.38

#: A glyph: something drawn on the badge, in badge coordinates. Every state
#: below is the pointer plus one of these, so the shape of a state is stated
#: once and the glyph is the only thing that differs.
Glyph = Callable[[Canvas, Geometry], None]


def _pointer_and(canvas: Canvas, geometry: Geometry, glyph: Glyph) -> None:
    """The arrow, the badge, and the glyph on it. Every state below but one."""
    paint(canvas, geometry, paths.POINTER, detail=POINTER_DETAIL)
    paint_badge(canvas, geometry)
    glyph(canvas, geometry)


def _glyph(points: paths.Path) -> Glyph:
    """A glyph made of straight lines: one path, placed on the badge."""
    def draw(canvas: Canvas, geometry: Geometry) -> None:
        paint_symbol(canvas, geometry, points)

    return draw


def _plate(canvas: Canvas, geometry: Geometry, pixels: paths.Path) -> None:
    """One placed glyph, drawn as a dark plate with a bright fill over it.

    Split out of :func:`marks.paint_symbol` because the copy badge is two pages
    and the second one has to sit at an offset: a helper that places its own
    points cannot draw a glyph somewhere other than the middle of the badge.
    """
    placed = [geometry.pixel(x, y) for x, y in pixels]
    polygon(canvas, placed, OUTLINE, 0.9, grow=geometry.outline * 0.5)
    polygon(canvas, placed, SYMBOL, 1.0)


def _pages(canvas: Canvas, geometry: Geometry) -> None:
    """Two pages, for the badge of a copy: one behind, one in front.

    Two overlapping squares with a dark edge between them is the shape a copy
    has meant since long before it was a cursor, and it is what tells a copy
    apart from a zoom - the two were the same plus sign before this, which is
    one glyph for two actions that do not resemble each other.
    """
    half = palette.BADGE_RADIUS * palette.BADGE_SYMBOL_SCALE * 0.62
    cx, cy = palette.BADGE_CENTRE
    shift = palette.BADGE_RADIUS * 0.30
    back = paths.placed(paths.SHEET, (cx - shift, cy - shift), half)
    front = paths.placed(paths.SHEET, (cx + shift * 0.45, cy + shift * 0.45),
                         half)
    # The page behind is drawn as a keyline and not as a solid: two solid pages
    # overlapping are one shape with a seam, and the seam is the whole glyph.
    back_pixels = [geometry.pixel(x, y) for x, y in back]
    polygon(canvas, back_pixels, SYMBOL, 0.95)
    polygon(canvas, back_pixels, OUTLINE, 1.0,
            grow=-max(1.0, geometry.outline * 0.9))
    _plate(canvas, geometry, front)


def _question(canvas: Canvas, geometry: Geometry) -> None:
    """A question mark, as the hook, the stem and the dot.

    Drawn from three primitives rather than from a polygon, because a hook is a
    curve and this program has no font: the arc is the only curve available, and
    it is exactly the shape the top of a question mark is.
    """
    cx, cy = geometry.pixel(*palette.BADGE_CENTRE)
    radius = palette.BADGE_RADIUS * geometry.size * palette.BADGE_SYMBOL_SCALE
    width = max(1.0, radius * 0.30)
    hook = radius * 0.36
    arc(canvas, cx, cy - radius * 0.30, hook, width, OUTLINE,
        math.pi * 0.60, math.pi * 1.44)
    arc(canvas, cx, cy - radius * 0.30, hook, width * 0.72, SYMBOL,
        math.pi * 0.60, math.pi * 1.44)
    half = max(0.5, radius * 0.16)
    stem = [
        (cx - half, cy - radius * 0.06),
        (cx + half, cy - radius * 0.06),
        (cx + half, cy + radius * 0.42),
        (cx - half, cy + radius * 0.42),
    ]
    polygon(canvas, stem, OUTLINE, grow=geometry.outline * 0.5)
    polygon(canvas, stem, SYMBOL)
    disc(canvas, cx, cy + radius * 0.74, max(0.8, half * 1.45), OUTLINE)
    disc(canvas, cx, cy + radius * 0.74, max(0.7, half * 1.30), SYMBOL)


# --- the states ---------------------------------------------------------------


def draw_forbidden(canvas: Canvas, geometry: Geometry, frame: int,
                   frames: int) -> None:
    """The bar across a hexagon: the one state that is not about a pointer.

    The bar is drawn first and the ring over it, and the two carry the same
    violet and the same cold filament, so the pair reads as one lit sign rather
    than as a ring with something laid over it - which is the whole difference
    between a prohibition and a decorated circle.
    """
    centre = geometry.centre
    radius, width = geometry.spinner
    slash = paths.turned(paths.SLASH, 0.125)
    spine = paths.turned(paths.SLASH_SPINE, 0.125)
    paint(canvas, geometry, slash, tone=GLASS,
          detail=Detail(spines=(spine,), channel=0.0))
    poly_ring(canvas, centre, centre, radius, width + 2.0 * geometry.outline,
              OUTLINE, 0.0, math.tau, palette.BADGE_SIDES, 0.0)
    poly_ring(canvas, centre, centre, radius, width, PLASMA, 0.0, math.tau,
              palette.BADGE_SIDES, 0.0)
    poly_ring(canvas, centre, centre, radius, width * FILAMENT, ENERGY, 0.0,
              math.tau, palette.BADGE_SIDES, 0.0)


def draw_help(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    """The pointer with a question mark: what is this thing."""
    _pointer_and(canvas, geometry, _question)


def draw_zoom_in(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    _pointer_and(canvas, geometry, _glyph(paths.PLUS))


def draw_zoom_out(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    _pointer_and(canvas, geometry, _glyph(paths.MINUS))


def draw_copy(canvas: Canvas, geometry: Geometry, frame: int,
              frames: int) -> None:
    _pointer_and(canvas, geometry, _pages)


def draw_alias(canvas: Canvas, geometry: Geometry, frame: int,
               frames: int) -> None:
    """The pointer with an arrow: a shortcut points somewhere else."""
    _pointer_and(canvas, geometry, _glyph(paths.ARROW))


def draw_dnd_move(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    _pointer_and(canvas, geometry, _glyph(paths.BLOCK))


def draw_dnd_copy(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    _pointer_and(canvas, geometry, _pages)


def draw_dnd_link(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    _pointer_and(canvas, geometry, _glyph(paths.ARROW))


def draw_dnd_none(canvas: Canvas, geometry: Geometry, frame: int,
                  frames: int) -> None:
    """The pointer with a bar on the badge: nothing can be dropped here.

    A bar and not a cross. A cross at six pixels is an X, and an X on a badge is
    the glyph for a close button everywhere else on the desktop; the bar is the
    one shape that only ever means no.
    """
    _pointer_and(canvas, geometry, _glyph(paths.turned(paths.SLASH, 0.125)))


def draw_dnd_ask(canvas: Canvas, geometry: Geometry, frame: int,
                 frames: int) -> None:
    _pointer_and(canvas, geometry, _question)


# --- the registry -------------------------------------------------------------

DIALOG_STATES: dict[str, State] = {
    "forbidden": State("forbidden", draw_forbidden, 1),
    "help": State("help", draw_help, 1, paths.POINTER_HOTSPOT),
    "zoom-in": State("zoom-in", draw_zoom_in, 1, paths.POINTER_HOTSPOT),
    "zoom-out": State("zoom-out", draw_zoom_out, 1, paths.POINTER_HOTSPOT),
    "copy": State("copy", draw_copy, 1, paths.POINTER_HOTSPOT),
    "alias": State("alias", draw_alias, 1, paths.POINTER_HOTSPOT),
    "dnd-move": State("dnd-move", draw_dnd_move, 1, paths.POINTER_HOTSPOT),
    "dnd-copy": State("dnd-copy", draw_dnd_copy, 1, paths.POINTER_HOTSPOT),
    "dnd-link": State("dnd-link", draw_dnd_link, 1, paths.POINTER_HOTSPOT),
    "dnd-none": State("dnd-none", draw_dnd_none, 1, paths.POINTER_HOTSPOT),
    "dnd-ask": State("dnd-ask", draw_dnd_ask, 1, paths.POINTER_HOTSPOT),
}
