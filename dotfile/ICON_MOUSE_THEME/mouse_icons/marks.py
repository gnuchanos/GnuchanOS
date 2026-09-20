"""The strokes a cursor's state adds on top of the dot: lines and arrows.

A line is drawn from the distance to a segment rather than by stepping along it,
for the same reason a disc comes from a distance to a point: the coverage falls
out of the geometry, so a one pixel arrow shaft and a four pixel one have the
same soft edge and neither needs a second pass to look right.

Every mark is drawn twice - once in the shadow colour, wider, then in the mark
colour - which is what keeps a light chevron legible where it crosses the bright
core of the dot. Drawing it once would leave the part over the core invisible,
and that part is the middle of the arrow.
"""

from __future__ import annotations

import math

from .canvas import Canvas, RGB

#: How much wider the shadow stroke is than the mark it backs.
SHADOW_EXCESS = 1.6


def _segment_distance(px: float, py: float, x0: float, y0: float, x1: float,
                      y1: float) -> float:
    """Distance from a point to the segment, not to the infinite line.

    The difference is the whole of a chevron: with the infinite line, the two
    arms would each extend past the point where they meet and draw a cross.
    """
    dx = x1 - x0
    dy = y1 - y0
    length_squared = dx * dx + dy * dy
    if length_squared <= 0.0:
        return math.hypot(px - x0, py - y0)
    t = ((px - x0) * dx + (py - y0) * dy) / length_squared
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))


def stroke(canvas: Canvas, points: list[tuple[float, float]], width: float,
           color: RGB, alpha: float = 1.0) -> None:
    """A polyline through ``points``, rounded at every joint.

    Rounded joints come for free from the distance test: a pixel within half a
    width of the corner where two segments meet is inside one of them, so no
    separate join is needed and no join can have a notch.
    """
    if width <= 0.0 or alpha <= 0.0 or len(points) < 2:
        return
    half = width / 2.0
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    left = int(min(xs) - half - 1)
    right = int(max(xs) + half + 1)
    top = int(min(ys) - half - 1)
    bottom = int(max(ys) + half + 1)
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            px = x + 0.5
            py = y + 0.5
            nearest = min(
                _segment_distance(px, py, points[index][0], points[index][1],
                                  points[index + 1][0], points[index + 1][1])
                for index in range(len(points) - 1)
            )
            coverage = half + 0.5 - nearest
            if coverage <= 0.0:
                continue
            canvas.blend(x, y, color, min(coverage, 1.0) * alpha)


def line(canvas: Canvas, x0: float, y0: float, x1: float, y1: float,
         width: float, color: RGB, shadow: RGB | None = None,
         alpha: float = 1.0) -> None:
    """One line, with an optional dark backing under it."""
    if shadow is not None:
        stroke(canvas, [(x0, y0), (x1, y1)], width + SHADOW_EXCESS, shadow, alpha)
    stroke(canvas, [(x0, y0), (x1, y1)], width, color, alpha)


def chevron(canvas: Canvas, cx: float, cy: float, size: float, angle: float,
            width: float, color: RGB, shadow: RGB | None = None,
            alpha: float = 1.0) -> None:
    """A V pointing along ``angle``, its tip ``size`` from the centre.

    A chevron and not a filled triangle: at twenty pixels the open V is the
    shape that still reads as a direction when it is two pixels of line, which a
    triangle that small does not.
    """
    tip_x = cx + math.cos(angle) * size
    tip_y = cy + math.sin(angle) * size
    spread = size * 0.72
    points = [
        (tip_x - math.cos(angle + math.pi / 2) * spread,
         tip_y - math.sin(angle + math.pi / 2) * spread),
        (tip_x, tip_y),
        (tip_x + math.cos(angle + math.pi / 2) * spread,
         tip_y + math.sin(angle + math.pi / 2) * spread),
    ]
    if shadow is not None:
        stroke(canvas, points, width + SHADOW_EXCESS, shadow, alpha)
    stroke(canvas, points, width, color, alpha)


def tick(canvas: Canvas, cx: float, cy: float, angle: float, inner: float,
         outer: float, width: float, color: RGB, shadow: RGB | None = None,
         alpha: float = 1.0) -> None:
    """A straight mark along ``angle``, from ``inner`` out to ``outer``.

    One primitive covers three things: the four ticks around a crosshair, the
    arrows of the resize states, and the leaves of the move cursor. They differ
    only in where they start and how far out they reach.
    """
    line(
        canvas,
        cx + math.cos(angle) * inner,
        cy + math.sin(angle) * inner,
        cx + math.cos(angle) * outer,
        cy + math.sin(angle) * outer,
        width,
        color,
        shadow,
        alpha,
    )


def arrow(canvas: Canvas, cx: float, cy: float, angle: float, inner: float,
          outer: float, width: float, color: RGB, shadow: RGB | None = None,
          alpha: float = 1.0) -> None:
    """A shaft with a chevron on its end, pointing along ``angle``."""
    tick(canvas, cx, cy, angle, inner, outer, width, color, shadow, alpha)
    chevron(canvas, cx, cy, outer, angle, width, color, shadow, alpha)


def slash(canvas: Canvas, cx: float, cy: float, size: float, angle: float,
          width: float, color: RGB, shadow: RGB | None = None,
          alpha: float = 1.0) -> None:
    """The bar of the forbidden cursor: a line through the centre.

    Longer than the dot's radius, so its ends stick out past the rim and the
    cursor reads as a prohibition rather than as a decorated dot.
    """
    dx = math.cos(angle) * size
    dy = math.sin(angle) * size
    line(canvas, cx - dx, cy - dy, cx + dx, cy + dy, width, color, shadow, alpha)


def plus(canvas: Canvas, cx: float, cy: float, size: float, width: float,
         color: RGB, vertical: bool = True, shadow: RGB | None = None,
         alpha: float = 1.0) -> None:
    """A plus or a minus, for the zoom states.

    ``vertical`` decides which: a plus is two strokes and a minus is one, and
    the two states have to differ by something that is visible at sixteen
    pixels.
    """
    line(canvas, cx - size, cy, cx + size, cy, width, color, shadow, alpha)
    if vertical:
        line(canvas, cx, cy - size, cx, cy + size, width, color, shadow, alpha)
