"""The glyph registry, and the drawing helpers every artwork module shares.

An icon here is a function that takes the tint its context asked for and returns
a list of primitives. Those functions are registered by name in :data:`GLYPHS`.
Registration is by decorator rather than by a table so the name and the drawing
live in the same place, and so the build can ask "is there artwork for this
name?" without importing anything that might not exist: a catalogue entry that
names a glyph nothing defines is caught by :func:`build` and by
``icon_install.py --check`` instead of silently becoming a blank square.

The helpers are the shared vocabulary the artwork is written in, and they exist
because the same few constructions recur in every module: a document page, an
application plate, a stroke drawn along a table of unit space segments, an
arrow, a chevron. Writing them once is what keeps the folder a file manager
draws and the folder an action draws from drifting apart.
"""

from __future__ import annotations

import importlib
import math
from typing import Callable, List, Sequence, Tuple, Union

from .primitives import Arc, Disc, Line, Poly, Rect, Ring

Shape = Union[Rect, Disc, Poly, Ring, Line, Arc]
ShapeList = List[Shape]
GlyphRenderer = Callable[[str], ShapeList]

Box = Tuple[float, float, float, float]

#: Every registered glyph, by name. Filled by :func:`glyph` at import time.
GLYPHS: dict[str, GlyphRenderer] = {}


# --- registration ------------------------------------------------------------


def glyph(name: str) -> Callable[[GlyphRenderer], GlyphRenderer]:
    """Register the decorated function as the artwork for ``name``.

    A duplicate name is an error rather than a silent overwrite: two modules
    drawing the same icon means one of them is dead code, and the picture the
    theme ships would depend on the import order in :mod:`icons.tree`.
    """

    def register(render: GlyphRenderer) -> GlyphRenderer:
        if name in GLYPHS:
            raise ValueError(f"glyph already defined: {name}")
        GLYPHS[name] = render
        return render

    return register


def build(glyph_name: str, tint: str) -> ShapeList:
    """Draw ``glyph_name`` in ``tint``, or explain what is missing.

    The returned list is a copy, so a caller that appends to it — the mime
    module builds pages by extending a shared page — cannot corrupt the
    registry for the next caller.
    """
    render = GLYPHS.get(glyph_name)
    if render is None:
        known = ", ".join(sorted(GLYPHS)[:8])
        raise KeyError(f"no artwork registered for {glyph_name!r} (have: {known}, ...)")
    return list(render(tint))


def defined(glyph_name: str) -> bool:
    """Whether artwork for ``glyph_name`` has been registered."""
    return glyph_name in GLYPHS


# The artwork modules, in the order they are loaded. Each one registers its
# glyphs as a side effect of being imported, so a module nobody imports
# contributes nothing and does so silently. The list lives here, beside the
# registry, so there is exactly one place that can fall out of step with the
# files on disk — and so a second consumer, like the contact sheet, does not
# have to keep a copy of it.
ARTWORK_MODULES: tuple[str, ...] = (
    "glyphs_actions",
    "glyphs_apps",
    "glyphs_emblems",
    "glyphs_emotes",
    "glyphs_extra",
    "glyphs_flags",
    "glyphs_hardware",
    "glyphs_letters",
    "glyphs_mime",
    "glyphs_objects",
    "glyphs_panels",
    "glyphs_places",
    "glyphs_status",
    "glyphs_ui",
)


def load_all() -> None:
    """Import every artwork module, so :data:`GLYPHS` holds the whole set."""
    for module in ARTWORK_MODULES:
        importlib.import_module(f"{__package__}.{module}")


# --- shared constructions ----------------------------------------------------


def page(fill: str, fold: str) -> ShapeList:
    """A document page with its top right corner turned down.

    Every file type in :mod:`icons.glyphs_mime` starts with this, which is what
    makes a directory listing read as one family: the eye learns the silhouette
    once and only has to read the small mark in the lower half.
    """
    return [
        Poly.of(
            fill,
            [
                (24.0, 12.0),
                (64.0, 12.0),
                (80.0, 28.0),
                (80.0, 86.0),
                (24.0, 86.0),
            ],
        ),
        Poly.of(fold, [(64.0, 12.0), (80.0, 28.0), (64.0, 28.0)]),
    ]


PLATE_BOX: Box = (14.0, 14.0, 72.0, 72.0)
PLATE_RADIUS = 13.0


def plate(color: str, edge: str | None = None, box: Box = PLATE_BOX, radius: float = PLATE_RADIUS) -> ShapeList:
    """The tile a generic application icon sits on.

    A program with no artwork of its own still needs *some* recognisable shape,
    and a plate with a mark on it is the convention every desktop already reads
    as "an application". ``edge`` draws the rim the plate is raised against.
    """
    x, y, width, height = box
    shapes: ShapeList = []
    if edge is not None:
        shapes.append(Rect(edge, x - 3.0, y - 3.0, width + 6.0, height + 6.0, radius + 3.0))
    shapes.append(Rect(color, x, y, width, height, radius))
    return shapes


def strokes(
    segments: Sequence[tuple[float, float, float, float]],
    fill: str,
    thickness: float,
    box: Box,
) -> ShapeList:
    """Draw a table of unit space segments inside ``box``.

    Segments are ``(x1, y1, x2, y2)`` in a unit square, y running downwards, so
    a letterform is a short table of numbers that can be corrected by eye. This
    is what keeps the stroke alphabet and the application plates in step.
    """
    x, y, width, height = box
    return [
        Line(
            fill,
            x + x1 * width,
            y + y1 * height,
            x + x2 * width,
            y + y2 * height,
            thickness,
        )
        for x1, y1, x2, y2 in segments
    ]


def bars(
    fill: str,
    cx: float,
    cy: float,
    width: float,
    thickness: float,
    count: int,
    gap: float,
) -> ShapeList:
    """``count`` stacked horizontal strokes, centred on ``cx, cy``."""
    left = cx - width / 2.0
    top = cy - (count - 1) * gap / 2.0
    return [
        Line(fill, left, top + index * gap, left + width, top + index * gap, thickness)
        for index in range(count)
    ]


def cross(fill: str, cx: float, cy: float, reach: float, thickness: float) -> ShapeList:
    """An x whose diagonals reach ``reach`` from the centre.

    ``reach`` is the distance from the centre to a tip, so a cross inscribed in
    a ring of inner radius ``reach`` touches it instead of overflowing.
    """
    offset = reach / math.sqrt(2.0)
    return [
        Line(fill, cx - offset, cy - offset, cx + offset, cy + offset, thickness),
        Line(fill, cx + offset, cy - offset, cx - offset, cy + offset, thickness),
    ]


def plus(fill: str, cx: float, cy: float, reach: float, thickness: float) -> ShapeList:
    """A ``+`` mark, the addition and "new item" symbol."""
    return [
        Line(fill, cx - reach, cy, cx + reach, cy, thickness),
        Line(fill, cx, cy - reach, cx, cy + reach, thickness),
    ]


def minus(fill: str, cx: float, cy: float, reach: float, thickness: float) -> ShapeList:
    """A ``-`` mark, the removal and "collapse" symbol."""
    return [Line(fill, cx - reach, cy, cx + reach, cy, thickness)]


def check(fill: str, cx: float, cy: float, size: float, thickness: float) -> ShapeList:
    """A tick mark, for confirm, select and enabled state."""
    half = size / 2.0
    corner_x = cx - half * 0.15
    corner_y = cy + half * 0.75
    return [
        Line(fill, cx - half * 0.8, cy + half * 0.05, corner_x, corner_y, thickness),
        Line(fill, corner_x, corner_y, cx + half * 0.85, cy - half * 0.75, thickness),
    ]


def chevron(
    fill: str,
    cx: float,
    cy: float,
    size: float,
    thickness: float,
    degrees: float = 0.0,
) -> ShapeList:
    """A ``>`` mark centred on ``cx, cy``, turned clockwise by ``degrees``.

    One helper draws every directional arrow in the theme: 0 points right, 90
    down, 180 left, 270 up. The turn is applied by the primitives, so the same
    two strokes serve every direction and every one of them antialiases the
    same way.
    """
    half = size / 2.0
    return [
        Line(
            fill,
            cx - half,
            cy - half,
            cx + half,
            cy,
            thickness,
            rotate=degrees,
            pivot=(cx, cy),
        ),
        Line(
            fill,
            cx + half,
            cy,
            cx - half,
            cy + half,
            thickness,
            rotate=degrees,
            pivot=(cx, cy),
        ),
    ]


def arrow(
    fill: str,
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    thickness: float = 6.0,
    head: float = 15.0,
) -> ShapeList:
    """A stroked shaft from ``x1, y1`` ending in a filled head at ``x2, y2``."""
    angle = math.atan2(y2 - y1, x2 - x1)
    base = math.pi - math.pi / 6.0
    shaft_x = x2 - math.cos(angle) * head * 0.6
    shaft_y = y2 - math.sin(angle) * head * 0.6
    return [
        Line(fill, x1, y1, shaft_x, shaft_y, thickness),
        Poly.of(
            fill,
            [
                (x2, y2),
                (
                    x2 + math.cos(angle + base) * head,
                    y2 + math.sin(angle + base) * head,
                ),
                (
                    x2 + math.cos(angle - base) * head,
                    y2 + math.sin(angle - base) * head,
                ),
            ],
        ),
    ]


def ring(fill: str, cx: float, cy: float, radius: float, thickness: float) -> ShapeList:
    """A full stroked circle, the base of every badge and dial in the theme."""
    return [Arc(fill, cx, cy, radius, 0.0, 359.999, thickness)]


def dots(fill: str, cx: float, cy: float, count: int, radius: float, gap: float) -> ShapeList:
    """``count`` discs in a horizontal row, centred on ``cx, cy``."""
    left = cx - (count - 1) * gap / 2.0
    return [Disc(fill, left + index * gap, cy, radius) for index in range(count)]


def gear(
    fill: str,
    cx: float,
    cy: float,
    radius: float = 26.0,
    teeth: int = 8,
    tooth: float = 13.0,
    thickness: float = 12.0,
) -> ShapeList:
    """A cog: an annulus with ``teeth`` square teeth around its outer edge.

    The teeth are squares placed on the outer edge and turned about the centre,
    which is why this is a helper rather than a dozen hand-placed rectangles.
    The same assembly is the system category, the applications menu and the
    emblem that marks a file as belonging to the system, so all three read as
    one idea instead of three similar ones.
    """
    if teeth < 3:
        raise ValueError("a gear needs at least three teeth")
    half = tooth / 2.0
    centre_radius = radius + thickness / 2.0
    shapes: ShapeList = list(ring(fill, cx, cy, radius, thickness))
    for index in range(teeth):
        shapes.append(
            Rect(
                fill,
                cx - half,
                cy - centre_radius - half,
                tooth,
                tooth,
                tooth * 0.25,
                rotate=index * 360.0 / teeth,
                pivot=(cx, cy),
            )
        )
    return shapes


def frame(
    fill: str,
    x: float,
    y: float,
    width: float,
    height: float,
    thickness: float = 7.0,
) -> ShapeList:
    """A hollow rectangle drawn as four strokes with round corners.

    There is no stroked rectangle primitive, and one is needed constantly — for
    a copy of a sheet, a chain link, a marquee — so it is assembled here from
    four lines rather than being drawn again in each module.
    """
    half = thickness / 2.0
    right = x + width - half
    bottom = y + height - half
    left = x + half
    top = y + half
    return [
        Line(fill, left, top, right, top, thickness),
        Line(fill, right, top, right, bottom, thickness),
        Line(fill, right, bottom, left, bottom, thickness),
        Line(fill, left, bottom, left, top, thickness),
    ]


def star_shape(
    fill: str,
    cx: float,
    cy: float,
    outer: float,
    inner: float,
    points: int = 5,
) -> Poly:
    """A star with its first point straight up.

    ``outer`` is the radius of the points and ``inner`` the radius of the
    valleys, which is what makes a star look like a star rather than like a
    cog: the ratio between the two is the whole character of the shape.
    """
    if points < 3:
        raise ValueError("a star needs at least three points")
    vertices: list[tuple[float, float]] = []
    for index in range(points * 2):
        radius = outer if index % 2 == 0 else inner
        angle = math.radians(-90.0 + index * 180.0 / points)
        vertices.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle)))
    return Poly.of(fill, vertices)


def point_on(cx: float, cy: float, radius: float, degrees: float) -> tuple[float, float]:
    """The point ``radius`` away from ``cx, cy`` at ``degrees`` (y downwards)."""
    radians = math.radians(degrees)
    return cx + radius * math.cos(radians), cy + radius * math.sin(radians)


def arrow_head(
    fill: str,
    x: float,
    y: float,
    degrees: float,
    size: float = 12.0,
) -> Poly:
    """A filled triangle pointing along ``degrees``, with its tip at ``x, y``.

    Rotating marks — refresh, repeat, synchronise — end in one of these. Placing
    it with :func:`point_on` and telling it the direction of travel is what
    keeps the head on the end of the arc instead of being eyeballed at eight
    different angles.
    """
    angle = math.radians(degrees)
    spread = math.pi - math.pi / 6.0
    return Poly.of(
        fill,
        [
            (x, y),
            (
                x + math.cos(angle + spread) * size,
                y + math.sin(angle + spread) * size,
            ),
            (
                x + math.cos(angle - spread) * size,
                y + math.sin(angle - spread) * size,
            ),
        ],
    )


def sun_shape(
    fill: str,
    cx: float,
    cy: float,
    radius: float = 18.0,
    rays: int = 8,
    ray_inner: float = 24.0,
    ray_length: float = 20.0,
    thickness: float = 6.0,
) -> ShapeList:
    """A disc with ``rays`` strokes around it, the sun of every weather icon.

    ``ray_inner`` is where a ray starts and ``ray_length`` how far it reaches,
    so the whole mark is guaranteed to stay inside the authoring box however it
    is used: the two are passed together rather than being derived from the
    radius, because a weather icon and a brightness icon want a different gap
    between the disc and the rays.
    """
    shapes: ShapeList = [Disc(fill, cx, cy, radius)]
    for index in range(rays):
        degrees = index * 360.0 / rays
        x1, y1 = point_on(cx, cy, ray_inner, degrees)
        x2, y2 = point_on(cx, cy, ray_inner + ray_length, degrees)
        shapes.append(Line(fill, x1, y1, x2, y2, thickness))
    return shapes


def crescent(
    fill: str,
    cut: str,
    cx: float,
    cy: float,
    radius: float,
    offset: float = 0.85,
) -> ShapeList:
    """A moon: a disc with a second disc taken out of its upper right.

    The subtraction is a disc in the colour of whatever is behind the moon
    rather than a path with a hole, because the hole has to be filled with the
    surface the moon sits on and every caller knows that surface. ``offset`` is
    the bite as a fraction of the radius.
    """
    return [
        Disc(fill, cx, cy, radius),
        Disc(cut, cx + radius * offset, cy - radius * offset * 0.5, radius * 0.92),
    ]


def cloud(fill: str, cx: float, cy: float, width: float = 44.0) -> ShapeList:
    """A cloud: three discs sharing a rounded base.

    Scaling from one number — ``width`` — keeps the cloud the same shape whether
    it is the whole icon or the small cloud behind the sun in ``weather-few-``
    ``clouds``, which is why it takes a width and not a radius per disc.
    """
    unit = width / 44.0
    return [
        Disc(fill, cx - 12.0 * unit, cy, 12.0 * unit),
        Disc(fill, cx + 10.0 * unit, cy + unit, 13.0 * unit),
        Disc(fill, cx, cy - 8.0 * unit, 15.0 * unit),
        Rect(fill, cx - 14.0 * unit, cy, 28.0 * unit, 14.0 * unit, 5.0 * unit),
    ]
