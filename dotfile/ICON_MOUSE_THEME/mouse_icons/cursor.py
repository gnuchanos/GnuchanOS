"""The frame a cursor is drawn in: its geometry, and the colours it may use.

There is no drawing in this module, and that is the change that matters. What
every state shares is where a shape goes, what light it carries and what
materials it is made of - so a caret and the pointer are drawn by the same code
with a different path, and they cannot come out with different weights or edges.

The colours are a *ramp* rather than a fill and a second fill. A shape is drawn
as a shell with a groove cut into it and a panel sunk inside the groove: the
shell is dark violet, the groove is the outline colour, and the panel is the lit
violet. The tones get brighter as they go in, so the middle of a shape is the
lit part and its edge is the armoured part. Three depths at every size is what
makes a cursor look machined rather than filled in - the version this replaces
had a bright body with a darker tone inset into it, which reads as a matte
sticker.

The light lives here too, as two numbers rather than as code: every shape is
painted over its own light, and how far that light reaches is a property of the
theme and not of the shape. So does the energy line - the cold cyan spine that
runs inside a shape - and the nodes, which are the small lit dots where a
shape's limbs meet.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from . import palette
from .canvas import RGB, parse_color

#: The colours a cursor is made of, parsed once at import: the void of the
#: outline and the grooves, the three steps of armour, the lit panel colours,
#: the near-white of the cores, the halo, the one cold accent, and the fill of
#: the symbols on a badge.
OUTLINE = parse_color(palette.OUTLINE)
SHELL = parse_color(palette.SHELL)
ARMOUR = parse_color(palette.ARMOUR)
BODY = parse_color(palette.BODY)
PLASMA = parse_color(palette.PLASMA)
LIGHT = parse_color(palette.LIGHT)
BRIGHT = parse_color(palette.BRIGHT)
CORE = parse_color(palette.CORE)
GLOW = parse_color(palette.GLOW)
ENERGY = parse_color(palette.ENERGY)
SYMBOL = parse_color(palette.SYMBOL)

#: The flat colour a shape is filled with when it carries no panel at all. Kept
#: as a name because a ring, a bar and a glyph are all "one solid violet" and
#: saying so once is what stops the three of them drifting apart.
FILL = BODY

#: The colours a shape is painted with, outermost first. Each further tone is
#: inset further and lifted further, so the tuple is a description of how deep
#: the light goes rather than a list of unrelated colours.
Tone = tuple[RGB, ...]

#: A shape too thin to hold a second tone: a resize bar, a caret, a chevron. One
#: tone, because a four pixel bar with a lighter stripe in it is a four pixel bar
#: with a stripe in it.
SOLID: Tone = (BODY,)

#: A shape big enough to carry the whole ramp: the pointer, the crosses, the
#: hourglass, the frame. Shell, then body, then the lit panel.
GLASS: Tone = (SHELL, BODY, PLASMA, LIGHT)

#: The ring of a waiting state, which is light all the way through: a spinner is
#: meant to read as a travelling piece of light and not as a purple tube.
BEAM: Tone = (PLASMA,)

#: The armour of a badge or a plate that is meant to read as a machine part:
#: darker than a body, so a badge is told apart from the pointer it sits on.
PLATE: Tone = (ARMOUR, BODY, PLASMA)


def dim(tone: Tone, brightness: float) -> Tone:
    """``tone`` dimmed towards the outline colour by ``1 - brightness``.

    Used by the states that pulse. Dimming towards the outline rather than
    towards black keeps a faded cursor the same hue as a bright one, which is
    what stops the hourglass flickering between two colours as it breathes.
    """
    if brightness >= 1.0:
        return tone
    return tuple(
        (
            int(OUTLINE[0] + (color[0] - OUTLINE[0]) * brightness + 0.5),
            int(OUTLINE[1] + (color[1] - OUTLINE[1]) * brightness + 0.5),
            int(OUTLINE[2] + (color[2] - OUTLINE[2]) * brightness + 0.5),
        )
        for color in tone
    )


def fade(tone: Tone, brightness: float) -> Tone:
    """The same as :func:`dim`, under the name a pulse reads better with."""
    return dim(tone, brightness)


@dataclass(frozen=True)
class Geometry:
    """Everything a shape needs to be placed, for one nominal size.

    ``size`` is the width and height of the image in pixels. A path is written in
    fractions of that, so this is the only place a fraction turns into a pixel
    and every shape is placed the same way. The light is measured in pixels here
    too - a halo that is a fixed number of pixels is a halo that is the same
    width at every size, which is what stops a large cursor carrying a thin aura
    and a small one carrying a fat one.
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

    def detail(self, fraction: float) -> float:
        """A fraction of the size, but never thinner than a pixel.

        The clamp is what makes the set's detail survive at 24 pixels: a groove
        of two and a half hundredths of a 24 pixel cursor is six tenths of a
        pixel, and a groove nobody can see is not a groove. A pixel is the
        thinnest line that still reads as a line at every size the theme ships.
        """
        return max(1.0, fraction * self.size)

    @property
    def outline(self) -> float:
        """How thick the outline is, in pixels. Never thinner than one pixel."""
        return max(1.0, palette.OUTLINE_WIDTH * self.size)

    @property
    def groove(self) -> float:
        """How wide the seam cut into the armour is, in pixels."""
        return self.detail(palette.GROOVE)

    @property
    def groove_inset(self) -> float:
        """How far inside the edge of a shape that seam runs, in pixels."""
        return palette.GROOVE_INSET * self.size

    @property
    def energy(self) -> float:
        """How wide the cold energy line inside a shape is, in pixels."""
        return self.detail(palette.ENERGY_WIDTH)

    @property
    def energy_inset(self) -> float:
        """How far inside the edge of a shape the energy line runs."""
        return palette.ENERGY_INSET * self.size

    @property
    def node(self) -> float:
        """The radius of a node, in pixels: the lit dot at a shape's joint."""
        return max(0.9, palette.NODE_RADIUS * self.size)

    @property
    def halo(self) -> tuple[float, float]:
        """How far the soft aura reaches, and how strong it is."""
        return (palette.GLOW_REACH * self.size, palette.GLOW_ALPHA)

    @property
    def rim(self) -> tuple[float, float]:
        """The bright band of light hugging the shape, reach and strength."""
        return (palette.GLOW_RIM_REACH * self.size, palette.GLOW_RIM_ALPHA)

    @property
    def inset(self) -> float:
        """How far inside the body the lit tone starts, in pixels."""
        return palette.INSET * self.size

    @property
    def lift(self) -> tuple[float, float]:
        """How far the lit tone is shifted towards the light, in pixels."""
        return (palette.LIFT[0] * self.size, palette.LIFT[1] * self.size)

    @property
    def core_inset(self) -> float:
        """How far inside the body the brightest tone starts, in pixels."""
        return palette.CORE_INSET * self.size

    @property
    def core_lift(self) -> tuple[float, float]:
        """How far the brightest tone is shifted towards the light."""
        return (palette.CORE_LIFT[0] * self.size,
                palette.CORE_LIFT[1] * self.size)

    @property
    def spinner(self) -> tuple[float, float]:
        """The radius and the thickness of a waiting state's ring, in pixels."""
        return (palette.SPINNER_REACH * self.size,
                palette.SPINNER_WIDTH * self.size)

    @property
    def spark(self) -> float:
        """The radius of a point of light, in pixels."""
        return palette.SPARK_RADIUS * self.size


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
