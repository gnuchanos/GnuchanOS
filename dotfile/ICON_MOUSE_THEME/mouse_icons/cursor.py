"""The frame a cursor is drawn in: its geometry, and the colours it may use.

There is no drawing in this module any more, and that is the change that matters.
The set this one replaces had a shared dot here that every state painted first
and then covered with marks, which is why the caret, the hand and the resize
arrows all ended up looking like one purple ball with something crossing it. A
shape is now a path in :mod:`paths`, and what every state shares is only where
the shape goes and what colour it is.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from . import palette
from .canvas import RGB, parse_color

#: The colours a cursor is made of, parsed once at import: the fill of a shape
#: and the darker tone inset from it, the outline around both, the lighter fill
#: of a spinner, and the bright core that is the one detail in the set which is
#: not a shade of purple.
FILL = parse_color(palette.FILL)
RIM = parse_color(palette.RIM)
OUTLINE = parse_color(palette.OUTLINE)
LIGHT = parse_color(palette.LIGHT)
SYMBOL = parse_color(palette.SYMBOL)
CORE = parse_color(palette.CORE)


@dataclass(frozen=True)
class Geometry:
    """Everything a shape needs to be placed, for one nominal size.

    ``size`` is the width and height of the image in pixels. A path is written in
    fractions of that, so this is the only place the fraction turns into a pixel
    and every shape is placed the same way.
    """

    size: int

    @property
    def centre(self) -> float:
        """Half a pixel past the middle, so a shape is centred on a pixel.

        A shape centred on a pixel boundary is one drawn half a pixel off at
        every size, and the hotspot of a centred cursor would then point between
        two pixels.
        """
        return (self.size - 1) / 2.0 + 0.5

    def pixel(self, ux: float, uy: float) -> tuple[float, float]:
        """A point of the unit square, in pixels."""
        return (ux * self.size, uy * self.size)

    def hotspot(self, ux: float, uy: float) -> tuple[int, int]:
        """A point of the unit square as an integer hotspot.

        Clamped rather than rounded and hoped for: a path may name a hotspot on
        the edge of the square, and an image whose hotspot is one pixel outside
        itself is one the Xcursor writer refuses.
        """
        x = min(self.size - 1, max(0, int(round(ux * self.size))))
        y = min(self.size - 1, max(0, int(round(uy * self.size))))
        return (x, y)

    @property
    def outline(self) -> float:
        """How thick the outline is, in pixels. Never thinner than one pixel."""
        return max(1.0, palette.OUTLINE_WIDTH * self.size)

    @property
    def spinner(self) -> tuple[float, float]:
        """The radius and the thickness of a waiting state's ring, in pixels."""
        return (palette.SPINNER_REACH * self.size,
                palette.SPINNER_WIDTH * self.size)


def pulse_of(frame: int, frames: int) -> float:
    """How bright the cursor is on ``frame``, from 0 to 1.

    A cosine rather than a triangle wave: a linear ramp reaches its turning
    point and reverses, which the eye catches as a stutter, while a cosine slows
    down as it turns and reads as breathing.
    """
    if frames <= 1:
        return 1.0
    return 0.5 - 0.5 * math.cos(math.tau * frame / frames)


def spin_of(frame: int, frames: int) -> float:
    """The angle, in radians, of a spinning state on ``frame``."""
    if frames <= 1:
        return 0.0
    return math.tau * frame / frames


def fade(color: RGB, brightness: float) -> RGB:
    """``color`` dimmed towards the outline colour by ``1 - brightness``.

    Used by the states that pulse. Dimming towards the outline rather than
    towards black keeps a faded cursor the same hue as a bright one, which is
    what stops the hourglass flickering between two colours as it breathes.
    """
    if brightness >= 1.0:
        return color
    return (
        int(OUTLINE[0] + (color[0] - OUTLINE[0]) * brightness + 0.5),
        int(OUTLINE[1] + (color[1] - OUTLINE[1]) * brightness + 0.5),
        int(OUTLINE[2] + (color[2] - OUTLINE[2]) * brightness + 0.5),
    )
