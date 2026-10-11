"""The shapes a cursor is made of: a polygon, a disc, a ring, an arc, a sparkle.

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
         alpha: float = 1.0, feather: float = FEATHER) -> None:
    """A filled circle of ``radius`` centred on ``cx, cy``.

    ``feather`` is how wide the edge is spread over. A shape takes the default
    of one pixel; a halo takes several, which is the whole difference between a
    disc and a glow - they are the same call with a different edge.
    """
    if radius <= 0.0 or alpha <= 0.0:
        return
    if feather <= 0.0:
        feather = 0.01
    outer = radius + feather
    for y in range(int(cy - outer - 1.0), int(cy + outer + 1.0) + 1):
        for x in range(int(cx - outer - 1.0), int(cx + outer + 1.0) + 1):
            distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            canvas.blend(x, y, color, cover(radius - distance, feather) * alpha)


def ring(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
         color: RGB, alpha: float = 1.0, feather: float = FEATHER) -> None:
    """A circle outline: everything ``width`` thick at ``radius``.

    The coverage is the smaller of the two edges' coverage, so a ring narrower
    than a pixel fades out instead of tearing - which is what happens when a
    wide cursor is asked for a hairline and the line is drawn as two edges
    instead of one shape. ``feather`` widens both edges at once, which is what
    turns the same ring into its own halo.
    """
    if width <= 0.0 or alpha <= 0.0:
        return
    if feather <= 0.0:
        feather = 0.01
    half = width / 2.0
    outer = radius + half
    for y in range(int(cy - outer - feather - 1.0),
                   int(cy + outer + feather + 1.0) + 1):
        for x in range(int(cx - outer - feather - 1.0),
                       int(cx + outer + feather + 1.0) + 1):
            distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            outside = cover(outer - distance, feather)
            inside = cover(distance - (radius - half), feather)
            canvas.blend(x, y, color, min(outside, inside) * alpha)


def arc(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
        color: RGB, start: float, sweep: float, alpha: float = 1.0,
        feather: float = FEATHER) -> None:
    """Part of a ring, from ``start`` radians through ``sweep`` radians.

    Both ends are cut square rather than rounded, which is what makes a spinning
    ring read as a ring with a gap in it - and a gap is the whole difference
    between something turning and a target. ``feather`` widens both edges, which
    is how the same arc is drawn as its own halo.
    """
    if width <= 0.0 or sweep <= 0.0 or alpha <= 0.0:
        return
    if feather <= 0.0:
        feather = 0.01
    half = width / 2.0
    outer = radius + half
    # A full circle needs no angle test, and skipping it avoids the seam that a
    # wrapped range leaves at the point where the angles meet.
    full = sweep >= math.tau - 1e-6
    for y in range(int(cy - outer - feather - 1.0),
                   int(cy + outer + feather + 1.0) + 1):
        for x in range(int(cx - outer - feather - 1.0),
                       int(cx + outer + feather + 1.0) + 1):
            px = x + 0.5 - cx
            py = y + 0.5 - cy
            distance = math.hypot(px, py)
            outside = cover(outer - distance, feather)
            if outside <= 0.0:
                continue
            inside = cover(distance - (radius - half), feather)
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


def sparkle(canvas: Canvas, cx: float, cy: float, radius: float, color: RGB,
            alpha: float = 1.0, waist: float = 0.17,
            glow: float = 0.0, glow_alpha: float = 0.0) -> None:
    """A four pointed star: the one shape in the set that is a point of light.

    A disc and a star of the same size read completely differently, and it is
    the star that reads as light: four long points and four short ones is the
    shape a spark has been drawn as since long before it was a cursor, and a dot
    of the same radius is a dot. The waist is how far the middle of each edge is
    pulled towards the centre - at zero the star is a diamond, and the value used
    here is the one where the four short points are still visible at 48 pixels
    and the shape has degraded gracefully into a bright dot at 24.

    ``glow`` is the radius of a soft bloom painted under the star, as a multiple
    of the star's own radius. It is what makes the spark look like it is lighting
    the shape it sits in rather than sitting on top of it.
    """
    if radius <= 0.0 or alpha <= 0.0:
        return
    if glow > 0.0 and glow_alpha > 0.0:
        disc(canvas, cx, cy, radius * glow, color, glow_alpha * alpha,
             feather=max(0.8, radius * glow * 0.75))
    inner = radius * waist
    points = [
        (cx, cy - radius),
        (cx + inner, cy - inner),
        (cx + radius, cy),
        (cx + inner, cy + inner),
        (cx, cy + radius),
        (cx - inner, cy + inner),
        (cx - radius, cy),
        (cx - inner, cy - inner),
    ]
    polygon(canvas, points, color, alpha=alpha,
            feather=max(0.55, radius * 0.28))
def stroke(canvas: Canvas, points: list[tuple[float, float]], width: float,
           color: RGB, alpha: float = 1.0, closed: bool = False,
           feather: float = FEATHER, grow: float = 0.0) -> None:
    """A polyline drawn as a line of ``width`` pixels, not as a filled shape.

    This is the primitive a polygon fill cannot express: the seam between two
    plates of armour, the burning energy line down the spine of a pointer, the
    spokes of a reticle. Drawn as the distance to the nearest segment of the
    line, so the two ends are round caps and the corners are round joins for
    free - which is what a hairlight has to be, because a squared off end on a
    two pixel line is a two pixel line with a mistake at the end of it.

    ``grow`` widens the line on both sides at once, which is how the same call
    draws a line and then its own halo, or a bright core inside a wider groove.
    """
    if width <= 0.0 or alpha <= 0.0 or len(points) < 2:
        return
    half = width / 2.0 + grow
    if half <= 0.0:
        return
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    left = int(math.floor(min(xs) - half - feather))
    right = int(math.ceil(max(xs) + half + feather))
    top = int(math.floor(min(ys) - half - feather))
    bottom = int(math.ceil(max(ys) + half + feather))
    segments = list(zip(points, points[1:]))
    if closed:
        segments.append((points[-1], points[0]))
    for y in range(top, bottom + 1):
        py = y + 0.5
        for x in range(left, right + 1):
            px = x + 0.5
            nearest = float("inf")
            for (x0, y0), (x1, y1) in segments:
                distance = segment_distance(px, py, x0, y0, x1, y1)
                if distance < nearest:
                    nearest = distance
                    if nearest <= 0.0:
                        break
            canvas.blend(x, y, color, cover(half - nearest, feather) * alpha)


def curve(control: list[tuple[float, float]], steps: int = 24) -> list[tuple[float, float]]:
    """A smooth curve through ``control``, as a polyline of ``steps`` points.

    A Catmull-Rom spline: the curve passes through every point it is given, which
    is what makes a shape written by hand stay where it was written. A Bezier
    would pull the curve towards control points instead, so the numbers in
    :mod:`paths` would stop being the outline they look like.
    """
    if len(control) < 2:
        return list(control)
    if len(control) == 2:
        return list(control)
    padded = [control[0]] + list(control) + [control[-1]]
    out: list[tuple[float, float]] = []
    for index in range(len(padded) - 3):
        p0, p1, p2, p3 = padded[index:index + 4]
        for step in range(steps):
            t = step / steps
            t2 = t * t
            t3 = t2 * t
            x = 0.5 * (
                2.0 * p1[0]
                + (-p0[0] + p2[0]) * t
                + (2.0 * p0[0] - 5.0 * p1[0] + 4.0 * p2[0] - p3[0]) * t2
                + (-p0[0] + 3.0 * p1[0] - 3.0 * p2[0] + p3[0]) * t3
            )
            y = 0.5 * (
                2.0 * p1[1]
                + (-p0[1] + p2[1]) * t
                + (2.0 * p0[1] - 5.0 * p1[1] + 4.0 * p2[1] - p3[1]) * t2
                + (-p0[1] + 3.0 * p1[1] - 3.0 * p2[1] + p3[1]) * t3
            )
            out.append((x, y))
    out.append(control[-1])
    return out


def regular(cx: float, cy: float, radius: float, sides: int,
            turn: float = 0.0) -> list[tuple[float, float]]:
    """A regular polygon, as points, starting at ``turn`` of a full circle.

    ``turn`` is a fraction of the circle because every rotation in this package
    is stated that way; a hexagon is turned by ``1/12`` to put a flat edge at
    the top rather than a point.
    """
    if sides < 3:
        return []
    start = turn * math.tau
    return [
        (cx + radius * math.cos(start + math.tau * index / sides),
         cy + radius * math.sin(start + math.tau * index / sides))
        for index in range(sides)
    ]


def star(cx: float, cy: float, radius: float, points: int, inner: float,
         turn: float = 0.0) -> list[tuple[float, float]]:
    """An ``points`` pointed star, ``inner`` being the waist as a fraction.

    The same shape :func:`sparkle` draws, but at any point count and any
    position, which is what the reticle and the badge of a refusal need: four
    points is a spark and eight is a hazard mark, and they are one function.
    """
    if points < 3:
        return []
    out: list[tuple[float, float]] = []
    start = turn * math.tau
    for index in range(points * 2):
        reach = radius if index % 2 == 0 else radius * inner
        angle = start + math.tau * index / (points * 2)
        out.append((cx + reach * math.cos(angle), cy + reach * math.sin(angle)))
    return out
def poly_ring(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
              color: RGB, start: float, sweep: float, sides: int = 6,
              turn: float = 0.0, alpha: float = 1.0,
              feather: float = FEATHER) -> None:
    """Part of a regular polygon's outline - the ring of a waiting state.

    The same shape as :func:`arc`, with one difference that is the whole reason
    it exists: the boundary is a polygon's and not a circle's, so a spinner
    reads as a hex nut turning rather than as a doughnut. ``turn`` rotates the
    polygon itself, which is what lets the whole nut spin rigidly instead of the
    gap sliding around inside a stationary outline.

    It is one pass over the bounding box, exactly like :func:`arc`, because the
    boundary's distance in a given direction comes out of the polygon's own
    support function rather than out of a distance to any of its edges: the
    boundary at angle ``a`` sits at ``inradius / cos(local)``, where ``local`` is
    how far ``a`` is from the middle of the sector it is in. Stroking the outline
    segment by segment would be the same picture at ten times the cost, which is
    ten times the cost on the one kind of state in the set that is animated at
    every size.

    The corners come out very slightly rounded at the sizes this theme ships,
    because the support function is the distance to the *edges* and not to the
    vertices. At twenty pixels that reads as a machined corner, which is what the
    shape is for.
    """
    if width <= 0.0 or sweep <= 0.0 or alpha <= 0.0 or sides < 3:
        return
    if feather <= 0.0:
        feather = 0.01
    half = width / 2.0
    step = math.tau / sides
    inradius = radius * math.cos(math.pi / sides)
    base = turn * math.tau
    outer = radius + half
    full = sweep >= math.tau - 1e-6
    for y in range(int(cy - outer - feather - 1.0),
                   int(cy + outer + feather + 1.0) + 1):
        py = y + 0.5 - cy
        for x in range(int(cx - outer - feather - 1.0),
                       int(cx + outer + feather + 1.0) + 1):
            px = x + 0.5 - cx
            distance = math.hypot(px, py)
            if distance <= 0.0:
                continue
            angle = math.atan2(py, px)
            local = (angle - base) % step - step * 0.5
            edge = inradius / math.cos(local)
            outside = cover(outer - distance, feather)
            if outside <= 0.0:
                continue
            inside = cover(distance - (edge - half), feather)
            if inside <= 0.0:
                continue
            coverage = min(outside, inside)
            if not full:
                offset = (angle - start) % math.tau
                if offset > sweep:
                    continue
                coverage *= cover(min(offset, sweep - offset) * radius)
            canvas.blend(x, y, color, coverage * alpha)
