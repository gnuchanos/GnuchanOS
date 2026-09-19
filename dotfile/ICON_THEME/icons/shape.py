"""The drawing primitives every GnuchanPurple icon is built from.

An icon is a list of shapes authored in a 100x100 unit box and scaled to
whatever pixel size is being rendered. Each shape knows two things: the SVG
path data for its outline, and a signed distance function in unit space
(negative inside the shape, positive outside). The distance function is what
lets the same artwork be rasterised into PNG at small sizes with proper
antialiasing, without a third party rasteriser or a font engine.

Keeping the outline and the distance function on one class is deliberate: an
icon that draws correctly as SVG but renders differently as PNG is a bug that
is otherwise very hard to see, and this is the one place where both come from
the same numbers.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

Point = tuple[float, float]

BOX = 100.0


def _rotate(px: float, py: float, cx: float, cy: float, degrees: float) -> Point:
    """Rotate a point around a centre, in degrees, screen coordinates (y down)."""
    if not degrees:
        return px, py
    radians = math.radians(degrees)
    cos, sin = math.cos(radians), math.sin(radians)
    dx, dy = px - cx, py - cy
    return cx + dx * cos - dy * sin, cy + dx * sin + dy * cos


def _clamp(value: float, low: float, high: float) -> float:
    return low if value < low else high if value > high else value


def fill_attributes(fill: str) -> str:
    """Turn ``#rrggbb`` or ``#rrggbbaa`` into SVG fill attributes.

    Eight digit hex is a CSS Color 4 extension: librsvg accepts it and older
    SVG consumers do not, so the alpha is split out into ``fill-opacity``,
    which every renderer understands.
    """
    text = fill.strip().lstrip("#")
    if len(text) == 8:
        return f'fill="#{text[:6]}" fill-opacity="{int(text[6:], 16) / 255:g}"'
    return f'fill="#{text}"'


def fmt(value: float) -> str:
    """Shortest reasonable decimal spelling of a coordinate."""
    rounded = round(value, 3)
    if rounded == int(rounded):
        return str(int(rounded))
    return f"{rounded:g}"


@dataclass(frozen=True)
class Shape:
    """A filled area with an optional rotation about a pivot.

    Subclasses provide ``body`` (SVG path data) and ``signed_distance`` (unit
    space). ``svg`` and ``coverage`` are written once here so every primitive
    antialiases and emits markup the same way.
    """

    fill: str
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        """Rotation centre when none was given explicitly."""
        return BOX / 2, BOX / 2

    def body(self) -> str:
        raise NotImplementedError

    def signed_distance(self, x: float, y: float) -> float:
        raise NotImplementedError

    def to_local(self, x: float, y: float) -> Point:
        """Map a sample point in unit space into this shape's own frame."""
        if not self.rotate:
            return x, y
        cx, cy = self.pivot or self.pivot_point()
        return _rotate(x, y, cx, cy, -self.rotate)

    def svg(self) -> str:
        transform = ""
        if self.rotate:
            cx, cy = self.pivot or self.pivot_point()
            transform = f' transform="rotate({fmt(self.rotate)} {fmt(cx)} {fmt(cy)})"'
        return f'<path d="{self.body()}" {fill_attributes(self.fill)}{transform}/>'

    def coverage(self, x: float, y: float, scale: float) -> float:
        """Antialiased coverage of this shape at unit space point ``x, y``.

        ``scale`` is pixels per unit; multiplying the distance by it makes the
        one pixel wide transition band the same width at every icon size.
        """
        local_x, local_y = self.to_local(x, y)
        distance = self.signed_distance(local_x, local_y)
        return _clamp(0.5 - distance * scale, 0.0, 1.0)


def rounded_rect_path(x: float, y: float, width: float, height: float, radius: float) -> str:
    """Path data for a rectangle with equal rounded corners."""
    corner = _clamp(radius, 0.0, min(width, height) / 2.0)
    if corner <= 0.0:
        return (
            f"M{fmt(x)} {fmt(y)}"
            f"L{fmt(x + width)} {fmt(y)}"
            f"L{fmt(x + width)} {fmt(y + height)}"
            f"L{fmt(x)} {fmt(y + height)}Z"
        )
    right = x + width
    bottom = y + height
    return (
        f"M{fmt(x + corner)} {fmt(y)}"
        f"L{fmt(right - corner)} {fmt(y)}"
        f"A{fmt(corner)} {fmt(corner)} 0 0 1 {fmt(right)} {fmt(y + corner)}"
        f"L{fmt(right)} {fmt(bottom - corner)}"
        f"A{fmt(corner)} {fmt(corner)} 0 0 1 {fmt(right - corner)} {fmt(bottom)}"
        f"L{fmt(x + corner)} {fmt(bottom)}"
        f"A{fmt(corner)} {fmt(corner)} 0 0 1 {fmt(x)} {fmt(bottom - corner)}"
        f"L{fmt(x)} {fmt(y + corner)}"
        f"A{fmt(corner)} {fmt(corner)} 0 0 1 {fmt(x + corner)} {fmt(y)}Z"
    )


def rounded_rect_distance(
    px: float, py: float, x: float, y: float, width: float, height: float, radius: float
) -> float:
    """Signed distance to a rounded rectangle, the exact analogue of the path."""
    corner = _clamp(radius, 0.0, min(width, height) / 2.0)
    half_w = width / 2.0
    half_h = height / 2.0
    qx = abs(px - (x + half_w)) - (half_w - corner)
    qy = abs(py - (y + half_h)) - (half_h - corner)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    inside = min(max(qx, qy), 0.0)
    return outside + inside - corner


def ring_distance(px: float, py: float, cx: float, cy: float, radius: float, thickness: float) -> float:
    """Signed distance to a stroked circle of the given stroke width."""
    return abs(math.hypot(px - cx, py - cy) - radius) - thickness / 2.0


def segment_distance(px: float, py: float, x1: float, y1: float, x2: float, y2: float) -> float:
    """Distance from a point to a line segment, with round caps implied."""
    dx, dy = x2 - x1, y2 - y1
    length_squared = dx * dx + dy * dy
    if length_squared <= 0.0:
        return math.hypot(px - x1, py - y1)
    t = _clamp(((px - x1) * dx + (py - y1) * dy) / length_squared, 0.0, 1.0)
    return math.hypot(px - (x1 + dx * t), py - (y1 + dy * t))


def angle_in_sweep(degrees: float, start: float, sweep: float) -> bool:
    """Whether an angle lies inside a clockwise sweep starting at ``start``."""
    if sweep >= 360.0:
        return True
    delta = (degrees - start) % 360.0
    return delta <= sweep
