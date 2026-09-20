"""The Xcursor file format, written directly.

A cursor theme is a directory of files, and each file is an Xcursor file: a
small container holding the same cursor drawn at several sizes, and, when it is
animated, several frames per size. There is a tool for making them,
``xcursorgen``, and this module exists instead of it for one reason: xcursorgen
is a separate package, it takes a configuration file per cursor, and it cannot
animate without a directory full of pre-rendered PNGs. A theme that is built by
a Python script is a theme that can be rebuilt by the same script on a machine
with nothing but Python installed, which is what ``settings_mouse_icon.py`` is
for.

The format is documented in the Xcursor library's ``file.c`` and is written
here in the same order it reads:

    header      16 bytes: magic, header length, version, table entries
    table       one entry per image: type, subtype, offset
    images      each one a 36 byte header and the pixels

Everything is little-endian ``uint32``, including the pixels, which are ARGB
with the alpha in the high byte. Written as bytes that is B, G, R, A - the one
detail that is easy to get wrong and produces a cursor that is entirely
transparent, because the blue channel is being read as the alpha.

A theme can hold as many sizes as it likes. Several images with the *same*
subtype are the animation frames of that size, and they are shown in the order
they appear in the table, each for the delay stored in its own header. Several
images with *different* subtypes are different sizes of the same cursor, which is
what keeps a cursor sharp on a HiDPI screen instead of scaling one bitmap up.
"""

from __future__ import annotations

import struct

#: "Xcur" as a little-endian uint32.
XCURSOR_MAGIC = 0x72756358

#: Bytes in the file header. Part of the format, not a choice.
XCURSOR_HEADER_BYTES = 16

#: Version 1.0 of the format: the one every X server since X11R6.9 reads.
XCURSOR_VERSION = 0x00010000

#: Bytes in an image chunk's own header.
XCURSOR_IMAGE_HEADER_BYTES = 36

#: The table entry type that holds an image. The other two types a file may
#: contain are comments and embedded files; a cursor theme needs neither.
XCURSOR_TYPE_IMAGE = 0xFFFD0002

#: The version of the image chunk itself, which is what carries the hotspot and
#: the delay. There is only one.
XCURSOR_IMAGE_VERSION = 1


class CursorImage:
    """One frame of one size, as pixels and a hotspot.

    ``pixels`` is ``width * height`` bytes in RGBA order, which is the order the
    canvas produces and the opposite of the order the file stores; the swap
    happens in :meth:`pack` and nowhere else, so there is exactly one place
    where getting it wrong would matter.
    """

    __slots__ = ("width", "height", "xhot", "yhot", "delay_ms", "pixels")

    def __init__(
        self,
        width: int,
        height: int,
        xhot: int,
        yhot: int,
        delay_ms: int,
        pixels: bytes,
    ) -> None:
        if width <= 0 or height <= 0:
            raise ValueError(f"an image must have a size, not {width}x{height}")
        if len(pixels) != width * height * 4:
            raise ValueError(
                f"expected {width * height * 4} bytes for {width}x{height}, "
                f"got {len(pixels)}"
            )
        # A hotspot outside the image is not a cursor that fails to draw, it is a
        # cursor that points at nothing, so it is refused rather than clamped.
        if not (0 <= xhot <= width and 0 <= yhot <= height):
            raise ValueError(
                f"the hotspot {xhot},{yhot} is outside a {width}x{height} image"
            )
        self.width = width
        self.height = height
        self.xhot = xhot
        self.yhot = yhot
        self.delay_ms = delay_ms
        self.pixels = pixels

    def pack(self) -> bytes:
        """This frame as the file stores it: header, then ARGB pixels."""
        header = struct.pack(
            "<9I",
            XCURSOR_IMAGE_HEADER_BYTES,
            XCURSOR_TYPE_IMAGE,
            self.width,  # the "subtype" of an image is its nominal size
            XCURSOR_IMAGE_VERSION,
            self.width,
            self.height,
            self.xhot,
            self.yhot,
            self.delay_ms,
        )
        return header + _pixels_argb(self.pixels)


def _pixels_argb(rgba: bytes) -> bytes:
    """RGBA bytes to the file's ARGB uint32 order.

    Xcursor stores each pixel as one uint32 in ARGB order, written
    little-endian, so the bytes on disk are B, G, R, A. The canvas works in
    RGBA because that is the order every other image format uses, and this is
    the one conversion between the two.
    """
    out = bytearray(len(rgba))
    out[0::4] = rgba[2::4]
    out[1::4] = rgba[1::4]
    out[2::4] = rgba[0::4]
    out[3::4] = rgba[3::4]
    return bytes(out)


def write_cursor(
    path: str,
    images: list[CursorImage],
    nominal_sizes: list[int] | None = None,
) -> None:
    """Write ``images`` to ``path`` as one Xcursor file.

    ``nominal_sizes`` is the size each image claims to be, in the same order as
    ``images``. It defaults to the image's own width, which is what a theme
    drawing its own bitmaps wants. The two only differ for an image that is
    drawn larger than the size it stands for, and the X server picks the frames
    of one size by this value rather than by the width.
    """
    if not images:
        raise ValueError("a cursor needs at least one image")
    sizes = nominal_sizes if nominal_sizes is not None else [i.width for i in images]
    if len(sizes) != len(images):
        raise ValueError("nominal_sizes has to have one entry per image")

    # The table is written before the images, so its size has to be known before
    # the first offset can be worked out. That is why the file is built in
    # memory as a list of chunks rather than streamed.
    table_bytes = len(images) * 12
    offset = XCURSOR_HEADER_BYTES + table_bytes

    header = struct.pack(
        "<4I",
        XCURSOR_MAGIC,
        XCURSOR_HEADER_BYTES,
        XCURSOR_VERSION,
        len(images),
    )

    table = bytearray()
    body = bytearray()
    for image, size in zip(images, sizes):
        # Each entry points at its image, and the subtypes within one size are
        # written as a set: an X server looking for a 24 pixel cursor reads
        # every entry whose subtype is 24, in order, and treats them as frames.
        table += struct.pack("<3I", XCURSOR_TYPE_IMAGE, int(size), offset)
        packed = image.pack()
        body += packed
        offset += len(packed)

    with open(path, "wb") as handle:
        handle.write(header)
        handle.write(bytes(table))
        handle.write(bytes(body))


def group_by_size(images: list[CursorImage]) -> dict[int, list[CursorImage]]:
    """The images in ``images`` keyed by width, in the order they were given.

    Used by the checks: a cursor whose frames are not all the same size is a
    cursor whose animation jumps, and one whose sizes are missing an entry is
    one that will be scaled by the X server.
    """
    grouped: dict[int, list[CursorImage]] = {}
    for image in images:
        grouped.setdefault(image.width, []).append(image)
    return grouped
