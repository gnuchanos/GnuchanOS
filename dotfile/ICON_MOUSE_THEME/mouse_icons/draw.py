"""The shapes a cursor is made of: a polygon, a disc, a ring, an arc.

Every one of them is drawn the same way - by asking, for each pixel of its own
bounding box, how much of the shape lands on that pixel, and painting that
fraction. The coverage comes out of a signed distance, so a disc's edge and a
polygon's edge are feathered identically and a cursor built from both looks like
one object instead of two drawings with different edges.

There is no scanline fill and no scan conversion anywhere, which is the whole
reason this is short. A cursor is twenty to fifty pixels across; the cost of
touching every pixel of a tiny box is smaller than the code a faster fill would
need, and a distance-based fill antialiases itself for free.
"""

from __future__ import annotations

import math

from .canvas import Canvas, RGB

#: How much of a pixel a shape's edge is spread over. One pixel: the smallest
#: value that removes the staircase without making a 24 pixel cursor look out of
#: focus.
FEATHER = 1.0


def _clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return low if value < low else high if value > high else value


def cover(inside: float, feather: float = FEATHER) -> float:
    """How much of a pixel is inside a shape, from a signed distance.

    ``inside`` is positive when the pixel is inside. A value of half a feather
    or more outside gives 0, the same inside gives 1, and the range between is
    the antialiased edge.
    """
    if feather <= 0.0:
        return 1.0 if inside >= 0.0 else 0.0
    return _clamp(inside / feather + 0.5)


def segment_distance(px: float, py: float, x0: float, y0: float, x1: float,
                     y1: float) -> float:
    """Distance from a point to the segment, not to the infinite line.

    The difference is the whole of an arrow head: with the infinite line, the
    two edges of a head would each extend past the point where they meet.
    """
    dx = x1 - x0
    dy = y1 - y0
    length_squared = dx * dx + dy * dy
    if length_squared <= 0.0:
        return math.hypot(px - x0, py - y0)
    t = ((px - x0) * dx + (py - y0) * dy) / length_squared
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))


def polygon_distance(px: float, py: float,
                     points: list[tuple[float, float]]) -> float:
    """Signed distance from a point to a closed polygon: positive inside.

    The sign is decided by the ray crossing rule and the magnitude by the
    nearest edge, which is what lets one function fill a shape as awkward as the
    hole in the middle of a hollow square - a shape whose inside is not
    connected.
    """
    nearest = float("inf")
    inside = False
    count = len(points)
    for index in range(count):
        x0, y0 = points[index]
        x1, y1 = points[(index + 1) % count]
        distance = segment_distance(px, py, x0, y0, x1, y1)
        if distance < nearest:
            nearest = distance
        if (y0 > py) != (y1 > py):
            crossing = x0 + (py - y0) * (x1 - x0) / (y1 - y0)
            if px < crossing:
                inside = not inside
    if nearest == float("inf"):
        return -1.0
    return nearest if inside else -nearest


def polygon(canvas: Canvas, points: list[tuple[float, float]], color: RGB,
            alpha: float = 1.0, grow: float = 0.0,
            feather: float = FEATHER) -> None:
    """A closed polygon of ``points``, grown outward by ``grow`` pixels.

    Growing is how an outline is drawn: the same polygon is painted once in the
    outline colour with a positive ``grow`` and then in the fill colour with
    none, so the two shapes are the same shape and the outline cannot have a
    corner the fill does not have. Offsetting the edges of a polygon properly
    would need a miter join per corner; moving the edge outward along its own
    normal is exactly what a distance test does for free.
    """
    if len(points) < 3 or alpha <= 0.0:
        return
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    left = int(math.floor(min(xs) - grow - feather))
    right = int(math.ceil(max(xs) + grow + feather))
    top = int(math.floor(min(ys) - grow - feather))
    bottom = int(math.ceil(max(ys) + grow + feather))
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            inside = polygon_distance(x + 0.5, y + 0.5, points) - grow
            canvas.blend(x, y, color, cover(inside, feather) * alpha)


def disc(canvas: Canvas, cx: float, cy: float, radius: float, color: RGB,
         alpha: float = 1.0) -> None:
    """A filled circle of ``radius`` centred on ``cx, cy``."""
    if radius <= 0.0 or alpha <= 0.0:
        return
    for y in range(int(cy - radius - FEATHER), int(cy + radius + FEATHER) + 1):
        for x in range(int(cx - radius - FEATHER), int(cx + radius + FEATHER) + 1):
            distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            canvas.blend(x, y, color, cover(radius - distance) * alpha)


def ring(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
         color: RGB, alpha: float = 1.0) -> None:
    """A circle outline: everything ``width`` thick at ``radius``.

    The coverage is the smaller of the two edges' coverage, so a ring narrower
    than a pixel fades out instead of tearing - which is what happens when a
    wide cursor is asked for a hairline and the line is drawn as two edges
    instead of one shape.
    """
    if width <= 0.0 or alpha <= 0.0:
        return
    half = width / 2.0
    outer = radius + half
    for y in range(int(cy - outer - FEATHER), int(cy + outer + FEATHER) + 1):
        for x in range(int(cx - outer - FEATHER), int(cx + outer + FEATHER) + 1):
            distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            outside = cover(outer - distance)
            inside = cover(distance - (radius - half))
            canvas.blend(x, y, color, min(outside, inside) * alpha)


def arc(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
        color: RGB, start: float, sweep: float, alpha: float = 1.0) -> None:
    """Part of a ring, from ``start`` radians through ``sweep`` radians.

    Both ends are cut square rather than rounded, which is what makes a spinning
    ring read as a ring with a gap in it - and a gap is the whole difference
    between something turning and a target.
    """
    if width <= 0.0 or sweep <= 0.0 or alpha <= 0.0:
        return
    half = width / 2.0
    outer = radius + half
    # A full circle needs no angle test, and skipping it avoids the seam that a
    # wrapped range leaves at the point where the angles meet.
    full = sweep >= math.tau - 1e-6
    for y in range(int(cy - outer - FEATHER), int(cy + outer + FEATHER) + 1):
        for x in range(int(cx - outer - FEATHER), int(cx + outer + FEATHER) + 1):
            px = x + 0.5 - cx
            py = y + 0.5 - cy
            distance = math.hypot(px, py)
            outside = cover(outer - distance)
            if outside <= 0.0:
                continue
            inside = cover(distance - (radius - half))
            if inside <= 0.0:
                continue
            coverage = min(outside, inside)
            if not full:
                offset = (math.atan2(py, px) - start) % math.tau
                if offset > sweep:
                    continue
                # The two ends are feathered the same way the radii are, so the
                # arc does not end on a hard radial line.
                coverage *= cover(min(offset, sweep - offset) * radius)
            canvas.blend(x, y, color, coverage * alpha)
