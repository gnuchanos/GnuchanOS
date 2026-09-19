"""Animated GIF writing, using only the standard library.

Exactly one icon in the theme moves: ``process-working``, the spinner a desktop
draws while it waits. Everything else is a still picture, so this module is
small on purpose — a palette shared by every frame, the mapping of the
antialiased edges onto it, and the LZW coder the format requires.

The rasteriser already produces the frames as RGBA buffers, and a GIF cannot
hold more than 256 colours or partial transparency, so two things are thrown
away on the way out: the alpha channel becomes a single transparent index, and
the colours are reduced to a table chosen from the frames themselves. For a
spinner that is invisible — it is two flat colours with an antialiased edge —
and it keeps the file a few kilobytes rather than a few hundred.

The writer is generic: :func:`encode` takes any list of equal sized
:class:`icons.raster.Raster` frames and returns the bytes of a looping GIF89a.
"""

from __future__ import annotations

import struct
from typing import Sequence

from .raster import Raster

#: A GIF colour table holds 256 entries, one of which is spent on transparency.
MAXIMUM_COLOURS = 256
#: Alpha at or above this is opaque; below it the pixel becomes transparent.
OPAQUE = 128
#: How many high bits of each channel are kept when the palette is counted.
#: Four is 4096 buckets, which is fine enough that two visibly different
#: colours never share one and coarse enough that the edge blends of an
#: antialiased glyph collapse onto the ink they came from.
BUCKET_BITS = 4
BUCKET_SHIFT = 8 - BUCKET_BITS
#: Frames per second is a property of the animation, not of the writer; the
#: caller passes its own delay.
DEFAULT_DELAY = 4

Bucket = tuple[int, int, int]


class _BitWriter:
    """Collects codes of varying width into the byte stream LZW wants."""

    def __init__(self) -> None:
        self._bits = 0
        self._count = 0
        self._data = bytearray()

    def write(self, code: int, width: int) -> None:
        self._bits |= code << self._count
        self._count += width
        while self._count >= 8:
            self._data.append(self._bits & 0xFF)
            self._bits >>= 8
            self._count -= 8

    def flush(self) -> bytes:
        """Write the partial byte, if any, and return everything collected."""
        if self._count > 0:
            self._data.append(self._bits & 0xFF)
            self._bits = 0
            self._count = 0
        return bytes(self._data)


def _bucket(red: int, green: int, blue: int) -> Bucket:
    return (red >> BUCKET_SHIFT, green >> BUCKET_SHIFT, blue >> BUCKET_SHIFT)


def _distance(colour: tuple[int, int, int], other: tuple[int, int, int]) -> int:
    """Squared distance between two colours, for nearest-colour mapping."""
    return (
        (colour[0] - other[0]) ** 2
        + (colour[1] - other[1]) ** 2
        + (colour[2] - other[2]) ** 2
    )


def _nearest(palette: Sequence[tuple[int, int, int]], colour: tuple[int, int, int]) -> int:
    """The index of the palette entry closest to ``colour``.

    Only the buckets that lost the vote reach here, so the loop runs over a
    handful of colours rather than over the whole image.
    """
    best = 0
    best_distance = None
    for index, entry in enumerate(palette):
        distance = _distance(colour, entry)
        if best_distance is None or distance < best_distance:
            best = index
            best_distance = distance
    return best


def _collect(frames: Sequence[Raster]) -> dict[Bucket, list[int]]:
    """Count the opaque pixels of every frame, grouped by their top bits."""
    totals: dict[Bucket, list[int]] = {}
    for frame in frames:
        data = frame.rgba()
        for offset in range(0, len(data), 4):
            if data[offset + 3] < OPAQUE:
                continue
            key = _bucket(data[offset], data[offset + 1], data[offset + 2])
            entry = totals.get(key)
            if entry is None:
                totals[key] = [data[offset], data[offset + 1], data[offset + 2], 1]
            else:
                entry[0] += data[offset]
                entry[1] += data[offset + 1]
                entry[2] += data[offset + 2]
                entry[3] += 1
    return totals


def _palette(frames: Sequence[Raster]) -> tuple[list[tuple[int, int, int]], dict[Bucket, int]]:
    """The colour table for ``frames``, and where each bucket maps onto it.

    The most used buckets take the table and the rest are folded onto the
    nearest of those, so a spinner of two flat colours keeps both of them
    exactly and its antialiased edge lands on the ink it was blended from.
    """
    totals = _collect(frames)
    if not totals:
        raise ValueError("no opaque pixels to build a palette from")
    ranked = sorted(totals.items(), key=lambda item: -item[1][3])
    kept = ranked[: MAXIMUM_COLOURS - 1]
    palette: list[tuple[int, int, int]] = []
    index_of: dict[Bucket, int] = {}
    for key, (red, green, blue, count) in kept:
        index_of[key] = len(palette)
        palette.append((red // count, green // count, blue // count))
    for key, (red, green, blue, count) in ranked[MAXIMUM_COLOURS - 1:]:
        index_of[key] = _nearest(palette, (red // count, green // count, blue // count))
    return palette, index_of


def _indices(frame: Raster, index_of: dict[Bucket, int], transparent: int) -> bytes:
    """One frame as palette indices, with the soft edges made transparent."""
    data = frame.rgba()
    pixels = bytearray(frame.width * frame.height)
    for position in range(len(pixels)):
        offset = position * 4
        if data[offset + 3] < OPAQUE:
            pixels[position] = transparent
            continue
        pixels[position] = index_of[
            _bucket(data[offset], data[offset + 1], data[offset + 2])
        ]
    return bytes(pixels)


def _code_size(entries: int) -> int:
    """The LZW minimum code size a table of ``entries`` indices needs."""
    width = 2
    while (1 << width) < entries:
        width += 1
    return min(width, 8)


def _compress(indices: bytes, minimum: int) -> bytes:
    """LZW compress ``indices``, the way the GIF specification defines it.

    Codes start one bit wider than the palette and grow as the table fills,
    which is what the decoder mirrors: it widens at exactly the point this
    encoder does, so no length has to be written down anywhere.
    """
    clear = 1 << minimum
    end = clear + 1
    width = minimum + 1
    writer = _BitWriter()
    writer.write(clear, width)
    table: dict[tuple[int, ...], int] = {(value,): value for value in range(clear)}
    next_code = end + 1
    if indices:
        current: tuple[int, ...] = (indices[0],)
        for value in indices[1:]:
            candidate = current + (value,)
            if candidate in table:
                current = candidate
                continue
            writer.write(table[current], width)
            if next_code < 4096:
                table[candidate] = next_code
                next_code += 1
                if next_code == (1 << width) and width < 12:
                    width += 1
            else:
                writer.write(clear, width)
                table = {(value_,): value_ for value_ in range(clear)}
                next_code = end + 1
                width = minimum + 1
            current = (value,)
        writer.write(table[current], width)
    writer.write(end, width)
    return writer.flush()


def _sub_blocks(data: bytes) -> bytes:
    """Split compressed data into the length-prefixed blocks a GIF carries."""
    out = bytearray()
    for start in range(0, len(data), 255):
        chunk = data[start:start + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


def _table_size(entries: int) -> int:
    """The power of two a colour table of ``entries`` colours occupies."""
    size = 2
    while size < entries:
        size *= 2
    return min(size, MAXIMUM_COLOURS)


def encode(frames: Sequence[Raster], delay: int = DEFAULT_DELAY, loop: int = 0) -> bytes:
    """A looping GIF89a of ``frames``, ``delay`` hundredths of a second each.

    Every frame is written full size with a transparent background and no
    disposal, so a viewer that draws them in order shows the animation even if
    it ignores the loop extension.
    """
    if not frames:
        raise ValueError("an animation needs at least one frame")
    width, height = frames[0].width, frames[0].height
    for frame in frames:
        if (frame.width, frame.height) != (width, height):
            raise ValueError("every frame of an animation must be the same size")
    palette, index_of = _palette(frames)
    transparent = len(palette)
    table_size = _table_size(len(palette) + 1)
    size_bits = table_size.bit_length() - 2
    minimum = _code_size(len(palette) + 1)

    data = bytearray(b"GIF89a")
    data += struct.pack("<HH", width, height)
    data += bytes([0x80 | 0x70 | size_bits, 0, 0])
    for red, green, blue in palette:
        data += bytes([red, green, blue])
    data += bytes([0, 0, 0]) * (table_size - len(palette))
    # The Netscape application extension is what makes a GIF loop; without it
    # the animation plays once and stops, which is not what a spinner does.
    data += b"\x21\xff\x0bNETSCAPE2.0\x03\x01" + struct.pack("<H", loop) + b"\x00"
    for frame in frames:
        data += (
            b"\x21\xf9\x04"
            + bytes([0x05])
            + struct.pack("<H", delay)
            + bytes([transparent])
            + b"\x00"
        )
        data += b"\x2c" + struct.pack("<HHHH", 0, 0, width, height) + b"\x00"
        data += bytes([minimum])
        data += _sub_blocks(_compress(_indices(frame, index_of, transparent), minimum))
    data += b"\x3b"
    return bytes(data)
