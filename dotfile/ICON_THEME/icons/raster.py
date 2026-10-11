"""Rasterisation of shapes into PNG bytes, using only the standard library.

PNG is what GTK 2 and older toolkits load, and it is also what every desktop
falls back to when an SVG loader is missing, so the icon theme ships real PNGs
rather than SVG alone. Rendering them needs a rasteriser, and this module is
it: shape coverage comes from the signed distance functions in
:mod:`icons.primitives`, which means an icon drawn as SVG and the same icon
drawn as PNG are one geometry sampled two ways.

Output is truecolour with alpha (colour type 6), written with ``struct`` and
``zlib``, so the icon theme builds on a machine with nothing but CPython.
"""

from __future__ import annotations

import struct
import zlib
from typing import Iterable, Sequence

RGBA = tuple[int, int, int, int]

_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def parse_color(value: str) -> RGBA:
    """Parse ``#rgb``, ``#rrggbb`` or ``#rrggbbaa`` into an RGBA tuple."""
    text = value.strip().lstrip("#")
    if len(text) == 3:
        text = "".join(character * 2 for character in text)
    if len(text) == 6:
        text += "ff"
    if len(text) != 8:
        raise ValueError(f"not a colour: {value!r}")
    return (
        int(text[0:2], 16),
        int(text[2:4], 16),
        int(text[4:6], 16),
        int(text[6:8], 16),
    )


def _chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


class Raster:
    """An RGBA pixel buffer with source-over compositing."""

    def __init__(self, width: int, height: int, fill: RGBA = (0, 0, 0, 0)) -> None:
        if width < 1 or height < 1:
            raise ValueError("a raster needs to be at least 1x1")
        self.width = width
        self.height = height
        self._pixels = bytearray(bytes(fill) * (width * height))

    def blend(self, x: int, y: int, color: RGBA, coverage: float) -> None:
        """Composite ``color`` over the pixel at ``x, y`` with ``coverage``."""
        if coverage <= 0.0 or not (0 <= x < self.width and 0 <= y < self.height):
            return
        source_alpha = (color[3] / 255.0) * min(coverage, 1.0)
        if source_alpha <= 0.0:
            return
        offset = (y * self.width + x) * 4
        pixels = self._pixels
        dest_alpha = pixels[offset + 3] / 255.0
        out_alpha = source_alpha + dest_alpha * (1.0 - source_alpha)
        if out_alpha <= 0.0:
            pixels[offset:offset + 4] = b"\x00\x00\x00\x00"
            return
        keep = dest_alpha * (1.0 - source_alpha)
        for channel in range(3):
            pixels[offset + channel] = round(
                (color[channel] * source_alpha + pixels[offset + channel] * keep) / out_alpha
            )
        pixels[offset + 3] = round(out_alpha * 255.0)

    def rgba(self) -> bytes:
        """The raw RGBA bytes, row by row.

        A copy rather than the live buffer: the GIF writer walks every pixel of
        every frame, and it has no business holding a reference to the pixels
        the next render is about to overwrite.
        """
        return bytes(self._pixels)

    def to_png(self) -> bytes:
        """Encode the buffer as a truecolour-with-alpha PNG."""
        stride = self.width * 4
        raw = bytearray()
        for row in range(self.height):
            raw.append(0)  # filter type 0: no prediction, keeps the encoder tiny
            raw += self._pixels[row * stride:(row + 1) * stride]
        return (
            _PNG_SIGNATURE
            + _chunk(
                b"IHDR",
                struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0),
            )
            + _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _chunk(b"IEND", b"")
        )


def render(shapes: Sequence[object], size: int, unit: float, samples: int = 2) -> Raster:
    """Render ``shapes`` into a ``size`` x ``size`` raster.

    ``unit`` is the size of the authoring box the shapes are expressed in, so
    one icon description works at every pixel size. ``samples`` is the
    supersampling factor per axis; 2 is the default because that is what keeps
    sixteen pixel icons legible.
    """
    raster = Raster(size, size)
    scale = size / unit
    step = 1.0 / samples
    offsets = [(index + 0.5) * step for index in range(samples)]
    subdivisions = samples * samples
    for pixel_y in range(size):
        for pixel_x in range(size):
            base_x = pixel_x / scale
            base_y = pixel_y / scale
            for shape in shapes:
                color = parse_color(shape.fill)  # type: ignore[attr-defined]
                coverage = 0.0
                for offset_y in offsets:
                    for offset_x in offsets:
                        coverage += shape.coverage(  # type: ignore[attr-defined]
                            base_x + offset_x / scale, base_y + offset_y / scale, scale
                        )
                raster.blend(pixel_x, pixel_y, color, coverage / subdivisions)
    return raster


def shape_colors(shapes: Iterable[object]) -> list[RGBA]:
    """Colours used by a shape list, in order, for tests and reports."""
    return [parse_color(shape.fill) for shape in shapes]  # type: ignore[attr-defined]
