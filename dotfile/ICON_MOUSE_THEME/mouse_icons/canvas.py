"""A small RGBA canvas, and the only place pixels are written.

The cursor theme is drawn rather than shipped as artwork, so this module is the
whole of its graphics library: a byte buffer, one blending rule, and the two
colour helpers everything else uses. It is deliberately not a general one -
there is no transform, no palette lookup, no file format - because the shapes
that use it need exactly three things: paint a pixel, blend a pixel that is
already there, and mix a colour with another.

Every shape is drawn by asking, for each pixel, how much of the shape covers it,
and painting that fraction. That is why the primitives in :mod:`draw` work from
a distance function and not from a scanline fill: coverage is a number between 0
and 1 that comes out of the geometry itself, which is what gives a twenty pixel
cursor an edge that is smooth instead of stepped, without a separate pass to
blur it.
"""

from __future__ import annotations

RGB = tuple[int, int, int]


def parse_color(text: str) -> RGB:
    """``"#rrggbb"`` to an ``(r, g, b)`` triple.

    The palette is written the way the rest of the desktop writes colours, and
    parsing it here rather than storing tuples keeps the two files looking the
    same. An eight digit value is accepted and its alpha ignored, because a
    colour copied from a stylesheet should not be a syntax error.
    """
    value = text.strip().lstrip("#")
    if len(value) == 8:
        value = value[:6]
    if len(value) != 6:
        raise ValueError(f"not a colour: {text!r}")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16))


def mix(base: RGB, other: RGB, amount: float) -> RGB:
    """``base`` moved ``amount`` of the way towards ``other``.

    Used for the pulse: the core of the dot is mixed towards white when the glow
    is at its brightest, so the whole cursor brightens together instead of the
    halo moving on its own.
    """
    if not 0.0 <= amount <= 1.0:
        raise ValueError(f"amount must be between 0 and 1, not {amount}")
    return (
        int(base[0] + (other[0] - base[0]) * amount + 0.5),
        int(base[1] + (other[1] - base[1]) * amount + 0.5),
        int(base[2] + (other[2] - base[2]) * amount + 0.5),
    )


class Canvas:
    """A square RGBA image, transparent to begin with.

    The buffer is the image: four bytes per pixel in RGBA order, row major, with
    the origin at the top left, which is the same order the Xcursor writer takes
    its input in and the same order every image format uses. Storing anything
    else would mean a conversion somewhere, and a conversion is where the alpha
    and the blue get swapped.
    """

    __slots__ = ("size", "data")

    def __init__(self, size: int) -> None:
        if size <= 0:
            raise ValueError(f"a canvas needs a size, not {size}")
        self.size = size
        self.data = bytearray(size * size * 4)

    # --- painting ------------------------------------------------------------

    def blend(self, x: int, y: int, color: RGB, alpha: float) -> None:
        """Paint one pixel: ``color`` at ``alpha`` over what is already there.

        Source-over, which is the rule every image program uses and the one that
        makes the layers composable: drawing the halo first and the dot on top
        of it gives an edge between them that neither of them describes, because
        the dot's own coverage decides how much of the halo it hides.
        """
        if alpha <= 0.0:
            return
        if x < 0 or y < 0 or x >= self.size or y >= self.size:
            return
        if alpha > 1.0:
            alpha = 1.0

        index = (y * self.size + x) * 4
        data = self.data
        # Un-premultiplied source-over. The destination keeps its own colour
        # where the source is transparent, which is what makes the corners of
        # the image cost nothing.
        dst_alpha = data[index + 3] / 255.0
        out_alpha = alpha + dst_alpha * (1.0 - alpha)
        if out_alpha <= 0.0:
            return
        weight = dst_alpha * (1.0 - alpha)
        for channel in range(3):
            value = (
                color[channel] * alpha
                + data[index + channel] * weight
            ) / out_alpha
            data[index + channel] = int(value + 0.5)
        data[index + 3] = int(out_alpha * 255.0 + 0.5)

    def pixels(self) -> bytes:
        """The image as bytes, RGBA, ready for the Xcursor writer."""
        return bytes(self.data)

    # --- reading, for the checks ---------------------------------------------

    def alpha_at(self, x: int, y: int) -> int:
        """The alpha of one pixel, 0 to 255. Used to prove a cursor is opaque
        where it should be and empty where it should not."""
        if x < 0 or y < 0 or x >= self.size or y >= self.size:
            return 0
        return self.data[(y * self.size + x) * 4 + 3]

    def opaque_bounds(self) -> tuple[int, int, int, int] | None:
        """The box the drawn pixels are in, or None when nothing was drawn.

        The check for a cursor that has drifted out of its own image: the
        hotspot is the centre of a dot that has to be centred, and a dot that is
        off centre is a cursor that points at the wrong pixel.
        """
        left, top, right, bottom = self.size, self.size, -1, -1
        for y in range(self.size):
            row = y * self.size * 4
            for x in range(self.size):
                if self.data[row + x * 4 + 3] > 8:
                    if x < left:
                        left = x
                    if x > right:
                        right = x
                    if y < top:
                        top = y
                    if y > bottom:
                        bottom = y
        if right < 0:
            return None
        return (left, top, right, bottom)
