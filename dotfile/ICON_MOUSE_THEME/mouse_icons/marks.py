"""Painting a shape: its light, its armour, its groove, its panel and its spine.

Every cursor in the theme is drawn by these functions, and every cursor is
painted in the same order. It is what makes the set look like one set rather
than thirty drawings, and that order is the whole of the design:

    the halo     a wide soft aura and a short bright rim, behind everything, so
                 the shape sits in front of whatever the user was looking at
    the outline  a hairline of void, grown outward from the same points
    the armour   the shell tone, the dark violet the shape is mostly made of
    the groove   a seam of the same void, cut a fixed distance inside the edge
    the panel    the lit tone, sunk inside the groove
    the face     the plasma tone, sunk further and lifted towards the light
    the energy   a cold cyan filament down the shape's own spine, sitting in a
                 dark channel so it reads as something burning inside a machine
    the node     a small lit dot where the shape's limbs meet

Two of those are the difference between this set and the flat one it replaces.
The groove is what gives a cursor three separate depths at twenty four pixels,
so it reads as machined rather than filled in. The energy line is the one thing
on the screen that is not violet - a shape that is violet from edge to centre is
a violet shape, and a shape with a cold line burning along its spine is a piece
of hardware.

The groove and the panel are cut with the same distance field the outline uses,
only with a negative offset: shrinking a polygon along its own normals is not
scaling it about its centre, and scaling is what turns a narrow arrow into a
sliver with a corner sticking out of it. Nothing here is a second shape drawn
over the first.
"""

from __future__ import annotations

from dataclasses import dataclass

from . import palette, paths
from .canvas import Canvas, RGB
from .cursor import (
    BRIGHT,
    CORE,
    ENERGY,
    GLASS,
    GLOW,
    OUTLINE,
    PLASMA,
    PLATE,
    SYMBOL,
    Geometry,
    Tone,
)
from .draw import disc, polygon, sparkle, stroke


@dataclass(frozen=True)
class Detail:
    """The details cut into one shape: its spines and its nodes.

    A spine is a polyline the energy line is drawn along, written in the same
    unit square as the shape it belongs to, so the light cannot drift off the
    middle of the shape at a different size. A node is a point where the shape's
    limbs meet, which gets a lit dot.

    ``channel`` is how many times the energy line's own width the dark channel
    around it is. It is a multiplier rather than a width because a resize bar's
    shaft is narrower than the channel would be at the default: a bar with a
    dark channel twice its own width in it is a black bar, so the thin shapes
    ask for none and let the filament sit directly on the armour.
    """

    spines: tuple[paths.Path, ...] = ()
    nodes: tuple[paths.Point, ...] = ()
    channel: float = 2.4
    core: bool = True


#: A shape with no detail at all: a glyph, a symbol, a shape too small to hold
#: one. The default, so a state that asks for nothing gets nothing.
PLAIN = Detail()


def _shifted(points: list[tuple[float, float]],
             offset: tuple[float, float]) -> list[tuple[float, float]]:
    """``points`` moved by ``offset`` pixels, which is what lights a shape."""
    dx, dy = offset
    if dx == 0.0 and dy == 0.0:
        return points
    return [(x + dx, y + dy) for x, y in points]


def _halo(canvas: Canvas, pixels: list[tuple[float, float]], geometry: Geometry,
          alpha: float) -> None:
    """The two passes of light behind a shape: the aura and the rim.

    The aura is grown well beyond the shape and spread far, so it reads as light
    falling on whatever is behind the cursor. The rim is a short bright band just
    outside the outline, so the silhouette itself looks like it is emitting. The
    pair is the difference between a glow and a bright blob, and it is not a
    subtle one.
    """
    reach, strength = geometry.halo
    polygon(canvas, pixels, GLOW, alpha * strength,
            grow=reach * 0.45, feather=max(0.9, reach * 1.35))
    rim, rim_strength = geometry.rim
    polygon(canvas, pixels, GLOW, alpha * rim_strength,
            grow=rim * 0.35, feather=max(0.8, rim * 1.5))


def _groove(canvas: Canvas, pixels: list[tuple[float, float]], tone: Tone,
            geometry: Geometry, alpha: float, grow: float) -> None:
    """The seam and the lit panel sunk inside it.

    The seam is the outline colour and is cut first, with the whole interior
    filled; the panel is the next tone, filled over the seam at a smaller reach.
    What is left between the two is the seam, and it is exactly as thick as the
    palette says at every size - which is the whole reason the two shapes are
    offsets of one distance field rather than two polygons.

    ``feather`` is tightened to the seam's own width so the band reaches its
    full colour in the middle: a one pixel band feathered over a whole pixel is
    a band that never quite is the colour it was asked to be.
    """
    seam = geometry.groove
    soft = max(0.55, seam * 1.1)
    inset = geometry.groove_inset
    polygon(canvas, pixels, OUTLINE, alpha, grow=grow - inset, feather=soft)
    polygon(canvas, pixels, tone[1], alpha, grow=grow - inset - seam,
            feather=soft)
    if len(tone) < 3:
        return
    polygon(canvas, _shifted(pixels, geometry.lift), tone[2], alpha,
            grow=grow - inset - seam - geometry.inset, feather=soft)
    if len(tone) < 4:
        return
    polygon(canvas, _shifted(pixels, geometry.core_lift), tone[3], alpha,
            grow=grow - inset - seam - geometry.core_inset, feather=soft)


def _spines(canvas: Canvas, geometry: Geometry, detail: Detail,
            alpha: float) -> None:
    """The cold filament down a shape, in its dark channel.

    Three passes: a channel, the filament, and a thin white core inside that. The
    channel is what stops the filament from looking like a scratch in the violet
    - a line of light on a lit surface is a line of light, and a line of light in
    a groove is something running through the object.
    """
    width = geometry.energy
    for spine in detail.spines:
        line = [geometry.pixel(x, y) for x, y in spine]
        if detail.channel > 0.0:
            stroke(canvas, line, width * detail.channel, OUTLINE,
                   alpha * 0.85, feather=max(0.7, width * 0.9))
        stroke(canvas, line, width, ENERGY, alpha,
               feather=max(0.55, width * 1.1))
        if detail.core:
            stroke(canvas, line, width * 0.42, CORE, alpha * 0.85,
                   feather=max(0.5, width * 0.55))


def _nodes(canvas: Canvas, geometry: Geometry, detail: Detail,
           alpha: float) -> None:
    """A lit dot at each joint the shape names.

    Drawn as the same stack as everything else - a bloom, an outline, a lit
    violet and a white centre - so a node reads as a small lit sphere rather
    than as a dot of paint, which is what a single flat disc comes out as.
    """
    reach = geometry.node
    for node in detail.nodes:
        cx, cy = geometry.pixel(*node)
        disc(canvas, cx, cy, reach * palette.NODE_GLOW, GLOW, alpha * 0.5,
             feather=max(0.9, reach * 1.3))
        disc(canvas, cx, cy, reach + geometry.outline * 0.4, OUTLINE, alpha,
             feather=max(0.6, reach * 0.5))
        disc(canvas, cx, cy, reach, PLASMA, alpha,
             feather=max(0.6, reach * 0.5))
        disc(canvas, cx, cy, reach * 0.40, CORE, alpha,
             feather=max(0.5, reach * 0.35))


def paint(canvas: Canvas, geometry: Geometry, points: paths.Path,
          tone: Tone = GLASS, alpha: float = 1.0, grow: float = 0.0,
          outline: bool = True, lit: bool = True,
          detail: Detail | None = None) -> None:
    """Draw one closed shape: its light, its armour, its panel and its detail.

    ``tone`` is the ramp from :mod:`cursor`, outermost first; ``grow`` moves
    every edge of the shape outward without changing its points, which is how a
    shape is asked to be a little bigger than the square it was written in.

    ``outline`` is what a shape drawn on top of something else turns off - the
    symbol on a badge is already on a badge, and a dark ring around a glyph six
    pixels across is a smudge. ``lit`` turns the groove and the panel off, for a
    shape that is too shallow to hold them, which is why a symbol is one flat
    colour and the pointer has a lit middle.
    """
    if alpha <= 0.0 or len(points) < 3:
        return
    pixels = [geometry.pixel(x, y) for x, y in points]
    _halo(canvas, pixels, geometry, alpha)
    if outline:
        polygon(canvas, pixels, OUTLINE, alpha, grow=grow + geometry.outline)
    polygon(canvas, pixels, tone[0], alpha, grow=grow)
    if lit and len(tone) > 1:
        _groove(canvas, pixels, tone, geometry, alpha, grow)
    if detail is not None:
        _spines(canvas, geometry, detail, alpha)
        _nodes(canvas, geometry, detail, alpha)


def paint_spark(canvas: Canvas, geometry: Geometry, ux: float = 0.5,
                uy: float = 0.5, radius: float | None = None,
                color: RGB = CORE, alpha: float = 1.0) -> None:
    """The point of light inside a shape: a four pointed star with a bloom.

    It is drawn last, on a body that is already on the canvas, and it is small.
    A spark the size of the shape it sits in is the stacked look this set is
    written to avoid; a spark a fifth of it is a highlight, and a highlight is
    what makes a flat violet reticle look like an instrument rather than like a
    shape someone filled in.

    The bloom is what does the work at the sizes where the star has degraded into
    a dot: at 24 pixels the four points are a pixel each, and the light around
    them is what is actually seen - which is why the bloom is drawn in the
    spark's own colour rather than in the halo's.
    """
    cx, cy = geometry.pixel(ux, uy)
    size = geometry.spark if radius is None else radius * geometry.size
    if size <= 0.0 or alpha <= 0.0:
        return
    sparkle(canvas, cx, cy, size, color, alpha=alpha, waist=0.17,
            glow=palette.SPARK_GLOW, glow_alpha=0.34)


def paint_core(canvas: Canvas, geometry: Geometry, ux: float, uy: float,
               radius: float, color: RGB = CORE, alpha: float = 1.0) -> None:
    """A lit node at a point of the unit square, sized in fractions of the size.

    The same stack :func:`_nodes` draws, for the states that place a node at a
    point that moves - the head of a spinning ring, which is at a different
    angle on every frame and therefore cannot be a constant in :mod:`paths`.
    """
    if radius <= 0.0 or alpha <= 0.0:
        return
    cx, cy = geometry.pixel(ux, uy)
    reach = radius * geometry.size
    disc(canvas, cx, cy, reach * palette.NODE_GLOW, GLOW, alpha * 0.55,
         feather=max(0.9, reach * 1.3))
    disc(canvas, cx, cy, reach + geometry.outline * 0.4, OUTLINE, alpha,
         feather=max(0.6, reach * 0.5))
    disc(canvas, cx, cy, reach, PLASMA, alpha, feather=max(0.6, reach * 0.5))
    disc(canvas, cx, cy, reach * 0.42, color, alpha,
         feather=max(0.5, reach * 0.35))


def paint_badge(canvas: Canvas, geometry: Geometry) -> None:
    """The plate a badge symbol sits on: a small lit chip at the lower right.

    A badge is the one place a cursor in this set is two shapes rather than one,
    and it is deliberate: a drag and drop cursor has to say what releasing the
    thing will do, and at twenty pixels there is no room to say it inside the
    pointer. The chip is drawn at the lower right of the arrow, over the shaft
    rather than over the tip, so the point of the cursor is never covered.

    It is a hexagon and not a disc. A circle is what every desktop already
    draws; a hexagon is what something machined, and it is the same shape as the
    badge's symbol and as the ring of a refusal, so the three read as one family
    of parts. It carries the same armour, groove and panel as everything else -
    only darker, so a badge is never mistaken for the pointer it sits on.
    """
    outline = paths.regular(0.5, 0.5, palette.BADGE_RADIUS, palette.BADGE_SIDES,
                            1.0 / 12.0)
    placed = paths.placed(outline, palette.BADGE_CENTRE, palette.BADGE_RADIUS)
    paint(canvas, geometry, placed, tone=PLATE, lit=True,
          detail=Detail(nodes=(), spines=(), channel=0.0), outline=True)
    # The highlight: a small bright facet at the upper left of the chip, lit
    # from the same direction as every other shape in the set.
    cx, cy = geometry.pixel(*palette.BADGE_CENTRE)
    radius = palette.BADGE_RADIUS * geometry.size
    highlight = palette.BADGE_HIGHLIGHT * radius
    disc(canvas, cx + palette.BADGE_LIGHT[0] * radius,
         cy + palette.BADGE_LIGHT[1] * radius, highlight, BRIGHT,
         alpha=0.42, feather=max(0.7, highlight * 0.6))


def paint_symbol(canvas: Canvas, geometry: Geometry, points: paths.Path) -> None:
    """One of the small glyphs, centred on the badge.

    Written in its own unit square and placed at the badge's centre at
    :data:`palette.BADGE_SYMBOL_SCALE` of the badge's radius, so a plus and a
    minus are the same weight and neither reaches the badge's own outline.

    It is drawn twice: once as a dark copy grown outward, and once in the bright
    symbol colour. That is a keyline, and it is there because a near-white glyph
    on a lit violet chip has almost no contrast where the two meet - without it a
    plus reads as a soft blob at twenty four pixels, which is one of the things
    this set is being fixed for.
    """
    half = palette.BADGE_RADIUS * palette.BADGE_SYMBOL_SCALE
    placed = paths.placed(points, palette.BADGE_CENTRE, half)
    pixels = [geometry.pixel(x, y) for x, y in placed]
    polygon(canvas, pixels, OUTLINE, 0.85, grow=geometry.outline * 0.55)
    polygon(canvas, pixels, SYMBOL, 1.0)
