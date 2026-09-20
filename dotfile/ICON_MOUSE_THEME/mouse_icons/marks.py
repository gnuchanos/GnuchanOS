"""Painting a shape: the rim, the body, the darker inset, and the bright core.

Every cursor in the theme is drawn by these three functions, and every cursor is
drawn the same four passes. It is what makes the set look like one set rather
than twenty drawings: a shape is a rim, a body of accent purple, a second purple
inset far enough inside the body to be seen at twenty four pixels, and - where
the shape is big enough to hold one - a single bright core. That is the whole
vocabulary. A caret and a resize bar are painted by the same lines of code, so
they cannot come out with different edges or different weights.

The inset is where the detail comes from, and it is the safest kind of detail
there is: it is the same path as the body, drawn with a negative ``grow``, so it
cannot stick out of the shape it is inside, cannot add a corner, and cannot turn
a cursor into two shapes. The set this replaces got that wrong in the other
direction - every state was a dot with marks on top of it, which at twenty four
pixels is two cursors sitting on each other.

The core is the one mark in the set that is not a shade of purple. It is placed
inside a body that is already there rather than on top of a shape, so a reticle
is still one object: the middle of a crosshair, the head of a spinner.
"""

from __future__ import annotations

from . import palette, paths
from .canvas import Canvas, RGB
from .cursor import CORE, FILL, OUTLINE, RIM, SYMBOL, Geometry
from .draw import disc, polygon


def paint(canvas: Canvas, geometry: Geometry, points: paths.Path,
          fill: RGB = FILL, outline: RGB | None = OUTLINE,
          inset: RGB | None = RIM, alpha: float = 1.0,
          grow: float = 0.0) -> None:
    """Draw one closed shape: its outline, its body, and the tone inside it.

    ``grow`` moves every edge of the shape outward without changing its points,
    which is how a shape is asked to be a little bigger than the square it was
    written in. ``inset`` may be None for a shape too thin to hold a second tone,
    and ``outline`` may be None for a mark that is already drawn on something.
    """
    pixels = [geometry.pixel(x, y) for x, y in points]
    if outline is not None:
        polygon(canvas, pixels, outline, alpha, grow=grow + geometry.outline)
    polygon(canvas, pixels, fill, alpha, grow=grow)
    if inset is not None:
        polygon(canvas, pixels, inset, alpha,
                grow=grow - max(1.0, palette.INSET * geometry.size))


def paint_core(canvas: Canvas, geometry: Geometry, ux: float = 0.5,
               uy: float = 0.5, radius: float = 0.075,
               color: RGB = CORE, ring: bool = True) -> None:
    """The bright core: the one detail that is not a shade of purple.

    It is drawn last, inside a body that is already on the canvas, and it is
    deliberately small. A core the size of the shape it sits in is the stacked
    look this set is written to avoid; a core a third of it is a highlight, and
    a highlight is what makes a flat purple reticle look like an instrument.
    """
    cx, cy = geometry.pixel(ux, uy)
    size = radius * geometry.size
    if ring:
        # A dark edge of its own, so a white dot on a purple body still has an
        # edge where the two meet - without it the core dissolves into the body
        # at the larger sizes and the cursor looks blurred rather than lit.
        disc(canvas, cx, cy, size + max(0.6, geometry.outline * 0.6), OUTLINE)
    disc(canvas, cx, cy, size, color)


def paint_badge(canvas: Canvas, geometry: Geometry) -> None:
    """The disc a badge symbol sits on.

    A badge is the one place a cursor in this set is two shapes rather than one,
    and it is deliberate: a drag and drop cursor has to say what releasing the
    thing will do, and at twenty pixels there is no room to say it inside the
    pointer. The disc is drawn at the lower right of the arrow, over the shaft
    rather than over the tip, so the point of the cursor is never covered.

    It carries the same two tones as everything else, so it reads as part of the
    cursor rather than as a sticker on it, and its glyph is drawn in the bright
    core colour, which is the one colour that reads on top of either purple.
    """
    cx, cy = geometry.pixel(*palette.BADGE_CENTRE)
    radius = palette.BADGE_RADIUS * geometry.size
    disc(canvas, cx, cy, radius + geometry.outline, OUTLINE)
    disc(canvas, cx, cy, radius, FILL)
    disc(canvas, cx, cy, radius * 0.68, RIM)


def paint_symbol(canvas: Canvas, geometry: Geometry, points: paths.Path) -> None:
    """One of the small glyphs, centred on the badge.

    Written in its own unit square and placed at the badge's centre at
    :data:`palette.BADGE_SYMBOL_SCALE` of the badge's radius, so a plus and a
    minus are the same weight and neither reaches the badge's own outline. No
    outline is drawn around a symbol: it is already on a badge, and a dark ring
    around a glyph six pixels across is a smudge.
    """
    half = palette.BADGE_RADIUS * palette.BADGE_SYMBOL_SCALE
    paint(canvas, geometry, paths.placed(points, palette.BADGE_CENTRE, half),
          fill=SYMBOL, outline=None, inset=None)
