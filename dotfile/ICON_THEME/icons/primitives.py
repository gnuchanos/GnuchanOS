"""The six shapes the whole GnuchanPurple icon set is drawn from.

Every icon in this theme is a list of ``Rect``, ``Disc``, ``Poly``, ``Ring``,
``Line`` and ``Arc`` objects. That is a deliberately small vocabulary: it is
enough for documents, folders, drives, arrows, gears, locks, batteries and
every widget glyph, and it keeps the shape code short enough to reason about.

The mixin holds the two rendering paths (SVG markup and pixel coverage) so no
primitive can implement one and forget the other.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Iterable, Sequence

from .shape import (
    Point,
    angle_in_sweep,
    fill_attributes,
    fmt,
    ring_distance,
    rounded_rect_distance,
    rounded_rect_path,
    segment_distance,
)


class _Renderable:
    """Shared SVG and coverage behaviour for every primitive.

    Attributes are declared for readability only; each primitive is a frozen
    dataclass that sets them, which is what keeps icons hashable and safe to
    share between the PNG and SVG renderers.
    """

    fill: str
    rotate: float
    pivot: Point | None

    def pivot_point(self) -> Point:
        raise NotImplementedError

    def body(self) -> str:
        raise NotImplementedError

    def attrs(self) -> str:
        return fill_attributes(self.fill)

    def signed_distance(self, x: float, y: float) -> float:
        raise NotImplementedError

    def to_local(self, x: float, y: float) -> Point:
        if not self.rotate:
            return x, y
        from .shape import _rotate

        cx, cy = self.pivot or self.pivot_point()
        return _rotate(x, y, cx, cy, -self.rotate)

    def svg(self) -> str:
        transform = ""
        if self.rotate:
            cx, cy = self.pivot or self.pivot_point()
            transform = f' transform="rotate({fmt(self.rotate)} {fmt(cx)} {fmt(cy)})"'
        return f'<path d="{self.body()}" {self.attrs()}{transform}/>'

    def coverage(self, x: float, y: float, scale: float) -> float:
        local_x, local_y = self.to_local(x, y)
        distance = self.signed_distance(local_x, local_y)
        return min(max(0.5 - distance * scale, 0.0), 1.0)


@dataclass(frozen=True)
class Rect(_Renderable):
    """A rectangle, optionally with equal rounded corners."""

    fill: str
    x: float
    y: float
    width: float
    height: float
    radius: float = 0.0
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        return self.x + self.width / 2.0, self.y + self.height / 2.0

    def body(self) -> str:
        return rounded_rect_path(self.x, self.y, self.width, self.height, self.radius)

    def signed_distance(self, x: float, y: float) -> float:
        return rounded_rect_distance(
            x, y, self.x, self.y, self.width, self.height, self.radius
        )


@dataclass(frozen=True)
class Disc(_Renderable):
    """A filled circle."""

    fill: str
    cx: float
    cy: float
    radius: float
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        return self.cx, self.cy

    def body(self) -> str:
        r = self.radius
        return (
            f"M{fmt(self.cx - r)} {fmt(self.cy)}"
            f"a{fmt(r)} {fmt(r)} 0 1 0 {fmt(2 * r)} 0"
            f"a{fmt(r)} {fmt(r)} 0 1 0 {fmt(-2 * r)} 0Z"
        )

    def signed_distance(self, x: float, y: float) -> float:
        return math.hypot(x - self.cx, y - self.cy) - self.radius


@dataclass(frozen=True)
class Poly(_Renderable):
    """A closed polygon, filled by the even-odd rule."""

    fill: str
    points: tuple[Point, ...]
    rotate: float = 0.0
    pivot: Point | None = None

    @staticmethod
    def of(fill: str, points: Sequence[Point], **kwargs: object) -> "Poly":
        return Poly(fill, tuple((float(px), float(py)) for px, py in points), **kwargs)

    def pivot_point(self) -> Point:
        xs = [p[0] for p in self.points]
        ys = [p[1] for p in self.points]
        return (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0

    def body(self) -> str:
        head = f"M{fmt(self.points[0][0])} {fmt(self.points[0][1])}"
        rest = "".join(f"L{fmt(px)} {fmt(py)}" for px, py in self.points[1:])
        return f"{head}{rest}Z"

    def signed_distance(self, x: float, y: float) -> float:
        inside = False
        nearest = math.inf
        count = len(self.points)
        for index in range(count):
            x1, y1 = self.points[index]
            x2, y2 = self.points[(index + 1) % count]
            nearest = min(nearest, segment_distance(x, y, x1, y1, x2, y2))
            if (y1 > y) != (y2 > y):
                crossing = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
                if x < crossing:
                    inside = not inside
        return -nearest if inside else nearest

    def svg(self) -> str:
        transform = ""
        if self.rotate:
            cx, cy = self.pivot or self.pivot_point()
            transform = f' transform="rotate({fmt(self.rotate)} {fmt(cx)} {fmt(cy)})"'
        return f'<path d="{self.body()}" {fill_attributes(self.fill)} fill-rule="evenodd"{transform}/>'


@dataclass(frozen=True)
class Ring(_Renderable):
    """A stroked circle: an annulus drawn as one even-odd path."""

    fill: str
    cx: float
    cy: float
    radius: float
    thickness: float
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        return self.cx, self.cy

    def _circle(self, radius: float) -> str:
        return (
            f"M{fmt(self.cx - radius)} {fmt(self.cy)}"
            f"a{fmt(radius)} {fmt(radius)} 0 1 0 {fmt(2 * radius)} 0"
            f"a{fmt(radius)} {fmt(radius)} 0 1 0 {fmt(-2 * radius)} 0Z"
        )

    def body(self) -> str:
        outer = self.radius + self.thickness / 2.0
        inner = max(self.radius - self.thickness / 2.0, 0.0)
        return self._circle(outer) + self._circle(inner)

    def signed_distance(self, x: float, y: float) -> float:
        return ring_distance(x, y, self.cx, self.cy, self.radius, self.thickness)

    def svg(self) -> str:
        transform = ""
        if self.rotate:
            cx, cy = self.pivot or self.pivot_point()
            transform = f' transform="rotate({fmt(self.rotate)} {fmt(cx)} {fmt(cy)})"'
        return (
            f'<path d="{self.body()}" {fill_attributes(self.fill)}'
            f' fill-rule="evenodd"{transform}/>'
        )


@dataclass(frozen=True)
class Line(_Renderable):
    """A straight stroke with round caps."""

    fill: str
    x1: float
    y1: float
    x2: float
    y2: float
    thickness: float = 6.0
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        return (self.x1 + self.x2) / 2.0, (self.y1 + self.y2) / 2.0

    def body(self) -> str:
        return (
            f"M{fmt(self.x1)} {fmt(self.y1)}L{fmt(self.x2)} {fmt(self.y2)}"
        )

    def attrs(self) -> str:
        return (
            f'fill="none" stroke="{self.fill}" stroke-width="{fmt(self.thickness)}"'
            ' stroke-linecap="round" stroke-linejoin="round"'
        )

    def signed_distance(self, x: float, y: float) -> float:
        return (
            segment_distance(x, y, self.x1, self.y1, self.x2, self.y2)
            - self.thickness / 2.0
        )


@dataclass(frozen=True)
class Arc(_Renderable):
    """A stroked circular arc, clockwise from ``start`` degrees over ``sweep``."""

    fill: str
    cx: float
    cy: float
    radius: float
    start: float
    sweep: float
    thickness: float = 6.0
    rotate: float = 0.0
    pivot: Point | None = None

    def pivot_point(self) -> Point:
        return self.cx, self.cy

    def _endpoint(self, degrees: float) -> Point:
        radians = math.radians(degrees)
        return (
            self.cx + self.radius * math.cos(radians),
            self.cy + self.radius * math.sin(radians),
        )

    def body(self) -> str:
        start_x, start_y = self._endpoint(self.start)
        end_x, end_y = self._endpoint(self.start + min(self.sweep, 359.999))
        large = 1 if self.sweep > 180.0 else 0
        return (
            f"M{fmt(start_x)} {fmt(start_y)}"
            f"A{fmt(self.radius)} {fmt(self.radius)} 0 {large} 1"
            f" {fmt(end_x)} {fmt(end_y)}"
        )

    def attrs(self) -> str:
        return (
            f'fill="none" stroke="{self.fill}" stroke-width="{fmt(self.thickness)}"'
            ' stroke-linecap="round"'
        )

    def signed_distance(self, x: float, y: float) -> float:
        dx, dy = x - self.cx, y - self.cy
        degrees = math.degrees(math.atan2(dy, dx))
        along = abs(math.hypot(dx, dy) - self.radius) - self.thickness / 2.0
        cap = math.inf
        for angle in (self.start, self.start + self.sweep):
            ex, ey = self._endpoint(angle)
            cap = min(cap, math.hypot(x - ex, y - ey) - self.thickness / 2.0)
        if angle_in_sweep(degrees, self.start, self.sweep):
            return min(along, cap)
        return cap


def bounds(shapes: Iterable[_Renderable]) -> tuple[float, float, float, float]:
    """Rough bounding box of a shape list, used to keep artwork inside the box."""
    left = top = math.inf
    right = bottom = -math.inf
    for shape in shapes:
        if isinstance(shape, Rect):
            left = min(left, shape.x)
            top = min(top, shape.y)
            right = max(right, shape.x + shape.width)
            bottom = max(bottom, shape.y + shape.height)
        elif isinstance(shape, (Disc, Ring, Arc)):
            reach = shape.radius + getattr(shape, "thickness", 0.0) / 2.0
            left = min(left, shape.cx - reach)
            top = min(top, shape.cy - reach)
            right = max(right, shape.cx + reach)
            bottom = max(bottom, shape.cy + reach)
        elif isinstance(shape, Poly):
            xs = [p[0] for p in shape.points]
            ys = [p[1] for p in shape.points]
            left, top = min(left, min(xs)), min(top, min(ys))
            right, bottom = max(right, max(xs)), max(bottom, max(ys))
        elif isinstance(shape, Line):
            left = min(left, min(shape.x1, shape.x2))
            top = min(top, min(shape.y1, shape.y2))
            right = max(right, max(shape.x1, shape.x2))
            bottom = max(bottom, max(shape.y1, shape.y2))
    return left, top, right, bottom
