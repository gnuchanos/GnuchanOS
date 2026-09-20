"""The four shapes every cursor is made of: a glow, a disc, a ring, an arc.

Each one is a loop over the pixels of its own bounding box asking how much of
the shape lands on that pixel, and painting that fraction. Nothing here fills a
polygon or walks a scanline: a cursor is twenty to fifty pixels across, and the
cost of touching every pixel of a tiny box is smaller than the code a faster
fill would need.

Coverage comes out of the geometry, which is the point. A disc's coverage is how
far inside its radius the pixel centre is, feathered over one pixel, so the edge
is smooth at every size without a separate blur pass - and the same number falls
out of a ring's two radii and an arc's angle, so all three have identically soft
edges and a cursor drawn from them looks like one object.
"""

from __future__ import annotations

import math

from .canvas import Canvas, RGB

#: How much of a pixel the edge of a shape is spread over. One pixel: the
#: smallest value that removes the staircase without making a 24 pixel cursor
#: look out of focus.
FEATHER = 1.0


def _clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return low if value < low else high if value > high else value


def cover(inside: float, feather: float = FEATHER) -> float:
    """How much of a pixel is inside a shape, from a signed distance.

    ``inside`` is positive when the pixel is inside. A value of one pixel or
    more outside gives 0, one pixel or more inside gives 1, and the range
    between is the antialiased edge.
    """
    return _clamp(inside / feather + 0.5)


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


def halo(canvas: Canvas, cx: float, cy: float, reach: float, color: RGB,
         alpha_max: float, power: float = 1.8) -> None:
    """The light around the dot, from full brightness at the centre to nothing.

    The falloff is a power curve rather than a linear ramp, because a linear one
    reads as a flat disc with a grey rim - the visible edge the whole glow
    exists to avoid. Inside the dot the halo is at its brightest; the dot is
    drawn over it, so what is left is the part that reaches past it.
    """
    if reach <= 0.0 or alpha_max <= 0.0:
        return
    for y in range(int(cy - reach - FEATHER), int(cy + reach + FEATHER) + 1):
        for x in range(int(cx - reach - FEATHER), int(cx + reach + FEATHER) + 1):
            distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if distance >= reach:
                continue
            falloff = (1.0 - distance / reach) ** power
            canvas.blend(x, y, color, alpha_max * falloff)


def arc(canvas: Canvas, cx: float, cy: float, radius: float, width: float,
        color: RGB, start: float, sweep: float, alpha: float = 1.0) -> None:
    """Part of a ring, from ``start`` radians through ``sweep`` radians.

    Both ends are cut square rather than rounded, which is what makes the busy
    ring read as a comet: the head is where the arc is thickest against the eye
    because it has an edge, and the fade behind it is the pulse.
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


def tapered_arc(canvas: Canvas, cx: float, cy: float, radius: float,
                width: float, color: RGB, start: float, sweep: float,
                alpha: float = 1.0) -> None:
    """An arc that fades from full strength at its head to nothing at its tail.

    The spinning states are what this is for: a ring of constant brightness
    rotating looks like a ring, while one that fades behind its head reads as
    movement even in a still frame, and reads as a direction once it moves.
    """
    steps = max(3, int(sweep * radius))
    step = sweep / steps
    for index in range(steps):
        progress = index / (steps - 1) if steps > 1 else 0.0
        arc(
            canvas,
            cx,
            cy,
            radius,
            width,
            color,
            start + index * step,
            step * 1.35,
            alpha * progress,
        )
