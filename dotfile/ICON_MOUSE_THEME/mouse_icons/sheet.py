"""A contact sheet of every cursor, written as a PNG.

A theme is thirty three drawings and a hundred and twenty names, and a person
checking it cannot see any of them: they are binary files the X server reads.
This module draws them all into one picture - every state, four frames of its
animation, on both a dark and a light background - so the set can be looked at
in one glance instead of trusted.

Both backgrounds are in the sheet, and that is not decoration. A light purple
cursor disappears on a white page, which is why every state is drawn with a dark
ring under it; the only way to know that the ring is doing its job is to see the
same cursor on both.

The PNG writer is deliberately minimal - truecolour, no interlacing, deflate -
because it is a diagnostic path and not an image library. zlib is in the standard
library, so nothing here needs installing.
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

from .cursor import Geometry
from .states import State

#: The size the sheet is drawn at, and how much it is enlarged by. Cursors are
#: drawn at 48 and shown at twice that, so the edges are visible; a 24 pixel
#: cursor shown at its own size in a contact sheet is a dot with no detail.
TILE = 48
SCALE = 2

#: Frames shown per state, and states per row of the sheet.
FRAMES = 4
COLUMNS = 5

#: The two backgrounds, dark first. They are the two cases a cursor has to work
#: on and the reason the palette carries a shadow colour at all.
DARK = bytes((0x14, 0x10, 0x18))
LIGHT = bytes((0xF4, 0xF1, 0xF7))


def _composite(tile: bytes, size: int, scale: int,
               background: bytes) -> list[bytes]:
    """A tile over a background, enlarged by ``scale``, as rows of RGB."""
    out: list[bytes] = []
    for y in range(size):
        row = bytearray()
        for x in range(size):
            index = (y * size + x) * 4
            alpha = tile[index + 3] / 255.0
            pixel = bytes(
                int(tile[index + channel] * alpha
                    + background[channel] * (1.0 - alpha) + 0.5)
                for channel in range(3)
            )
            row += pixel * scale
        out.extend([bytes(row)] * scale)
    return out


def _sheet_image(states: dict[str, State], background: bytes) -> tuple[int, int, bytes]:
    """One half of the sheet: every state, on one background."""
    names = sorted(states)
    cell_width = TILE * SCALE * FRAMES
    cell_height = TILE * SCALE
    width = COLUMNS * cell_width
    rows = (len(names) + COLUMNS - 1) // COLUMNS
    height = rows * cell_height

    canvas = bytearray(background * (width * height))
    geometry = Geometry(TILE)
    for index, name in enumerate(names):
        state = states[name]
        column = index % COLUMNS
        row = index // COLUMNS
        origin_x = column * cell_width
        origin_y = row * cell_height
        for shown in range(FRAMES):
            frame = (shown * state.frames) // FRAMES
            tile = state.render(geometry, frame).pixels()
            lines = _composite(tile, TILE, SCALE, background)
            tile_x = origin_x + shown * TILE * SCALE
            for line_index, line in enumerate(lines):
                start = ((origin_y + line_index) * width + tile_x) * 3
                canvas[start:start + len(line)] = line
    return width, height, bytes(canvas)


def write_png(path: str | Path, width: int, height: int, rgb: bytes) -> None:
    """A truecolour PNG, written by hand.

    Stored with a real deflate stream from zlib rather than the stored blocks
    the greeter's diagnostic writer uses, because a contact sheet of a hundred
    cursors is a few megabytes of flat colour and compresses to almost nothing.
    """
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)  # filter type 0: no prediction
        raw += rgb[y * stride:(y + 1) * stride]
    compressed = zlib.compress(bytes(raw), 9)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", compressed)
        + chunk(b"IEND", b"")
    )
    Path(path).write_bytes(payload)


def write_sheet(path: str | Path, states: dict[str, State]) -> tuple[int, int]:
    """Every cursor in ``states``, on both backgrounds, in one PNG.

    The light half is written below the dark half rather than into a second
    file, because the point of the sheet is to be looked at in one glance and two
    files is two glances.
    """
    dark_width, dark_height, dark = _sheet_image(states, DARK)
    light_width, light_height, light = _sheet_image(states, LIGHT)
    if dark_width != light_width:
        raise ValueError("the two halves have to be the same width")

    width = dark_width
    height = dark_height + light_height
    combined = bytearray(dark) + bytearray(light)
    write_png(path, width, height, bytes(combined))
    return width, height
