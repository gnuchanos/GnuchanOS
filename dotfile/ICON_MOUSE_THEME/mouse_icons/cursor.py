"""The cursor itself: the dot, its glow, and the pulse that animates both.

Every state in the theme is this object, drawn the same way, with something
added on top. That is the whole design: a set where the pointer, the text caret
and the busy ring are recognisably the same cursor is a set the eye learns in one
glance, and it is why this module exists separately from :mod:`states` - the
part that never changes is in one place, and the part that does is in another.

The animation is a pulse, not a spin: the halo brightens and fades over a couple
of seconds, and the core brightens with it. It is deliberately slow. A cursor
that blinks is a cursor that pulls the eye away from what is being typed, and the
point of the glow is to make the cursor easy to find on a busy screen, not to
compete with the screen.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from . import palette
from .canvas import Canvas, mix, parse_color
from .draw import disc, halo, ring

#: The nominal sizes, in pixels. Everything a state draws is a fraction of this,
#: so a state written once draws correctly at every size in :data:`SIZES`.
HALO_COLOR = parse_color(palette.HALO)
RIM_COLOR = parse_color(palette.RIM)
CORE_COLOR = parse_color(palette.CORE)
INNER_COLOR = parse_color(palette.INNER)
SHADOW_COLOR = parse_color(palette.SHADOW)
MARK_COLOR = parse_color(palette.MARK)
MARK_SHADOW_COLOR = parse_color(palette.MARK_SHADOW)
RING_COLOR = parse_color(palette.RING)

#: The colour the core is mixed towards while the pulse is bright. Not pure
#: white: a white core loses the purple entirely at the larger sizes and the
#: cursor stops looking like part of the theme.
BRIGHT = (255, 255, 255)


@dataclass(frozen=True)
class Geometry:
    """Everything a state needs to place its marks, for one nominal size.

    Computed once per size rather than per frame, because none of it depends on
    the frame: the pulse moves brightness and angle, never position, which is
    what keeps an animated cursor from jittering.
    """

    size: int

    @property
    def centre(self) -> float:
        """Half a pixel past the middle, so the dot is centred on a pixel.

        A dot centred on a pixel boundary is a dot drawn half a pixel off at
        every size, and the hotspot would then point between two pixels.
        """
        return (self.size - 1) / 2.0 + 0.5

    @property
    def hotspot(self) -> int:
        """The pixel the cursor actually points at: the middle of the dot."""
        return int(self.centre - 0.5)

    @property
    def radius(self) -> float:
        return self.size * palette.DOT_RADIUS

    @property
    def reach(self) -> float:
        return self.radius * palette.HALO_REACH

    @property
    def rim(self) -> float:
        return self.radius * palette.RIM_WIDTH

    @property
    def shadow(self) -> float:
        return self.radius * palette.SHADOW_WIDTH

    @property
    def mark(self) -> float:
        return max(1.0, self.size * palette.MARK_WIDTH)

    @property
    def mark_radius(self) -> float:
        """Where a mark starts, so it does not sit on top of the core."""
        return self.radius * 0.55


def pulse_of(frame: int, frames: int) -> float:
    """How bright the cursor is on ``frame``, from 0 to 1.

    A cosine rather than a triangle wave: a linear ramp reaches its turning
    point and reverses, which the eye catches as a stutter, while a cosine
    slows down as it turns and reads as breathing.
    """
    if frames <= 1:
        return 1.0
    return 0.5 - 0.5 * math.cos(math.tau * frame / frames)


def spin_of(frame: int, frames: int) -> float:
    """The angle, in radians, of a spinning state on ``frame``."""
    if frames <= 1:
        return 0.0
    return math.tau * frame / frames


def draw_light(canvas: Canvas, geometry: Geometry, brightness: float,
               radius_scale: float = 1.0) -> None:
    """The dot and its glow, at the given brightness.

    The order matters and is the reason this is one function rather than four
    calls from each state: the shadow goes under everything, the halo over it,
    then the rim, the inner ring and the core. Any other order leaves a seam
    where two of them meet.
    """
    centre = geometry.centre
    radius = geometry.radius * radius_scale
    glow = (
        palette.HALO_ALPHA_MIN
        + (palette.HALO_ALPHA_MAX - palette.HALO_ALPHA_MIN) * brightness
    )
    core = mix(CORE_COLOR, BRIGHT, palette.CORE_PULSE * brightness)

    halo(canvas, centre, centre, geometry.reach * radius_scale, HALO_COLOR, glow)
    disc(canvas, centre, centre, radius, SHADOW_COLOR)
    disc(canvas, centre, centre, radius - geometry.shadow, RIM_COLOR)
    ring(canvas, centre, centre, radius - geometry.rim, geometry.rim * 0.55,
         INNER_COLOR)
    disc(canvas, centre, centre, radius * 0.52, core)


def draw_dot(canvas: Canvas, geometry: Geometry, frame: int, frames: int,
             radius_scale: float = 1.0) -> None:
    """The plain cursor: the dot, pulsing. Every other state starts here."""
    draw_light(canvas, geometry, pulse_of(frame, frames), radius_scale)


def clear_hole(canvas: Canvas, geometry: Geometry, radius: float,
               alpha: float = 0.55) -> None:
    """Darken the middle of the dot, for the states that draw a mark there.

    A mark over the bright core has to fight it for contrast; a mark over a
    slightly darkened core reads at half the stroke weight. This is done by
    drawing the shadow colour back over the core rather than by erasing, so the
    cursor keeps a background and does not become a hole in the screen.
    """
    disc(canvas, geometry.centre, geometry.centre, radius, SHADOW_COLOR, alpha)
