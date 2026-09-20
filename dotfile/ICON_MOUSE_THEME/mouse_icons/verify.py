"""Reading the built theme back, and checking it is what it claims to be.

A cursor theme that is wrong is wrong silently. The X server does not report a
malformed file; it falls back to the default cursor for that one shape, and the
result is a desktop where the pointer is purple and the text caret is a black
I-beam from 1989 - with nothing anywhere saying why. So the files are read back
after they are written and their headers are checked against the format, which is
the only way a build can tell the difference between a theme and a directory of
files that the X server will ignore.

The checks are the ones whose failure is invisible:

    the magic and the version     a file written in the wrong byte order has
                                  the right size and draws nothing
    the table offsets             an offset past the end of the file is a
                                  cursor the X server reads as empty
    the image header              its width and height have to match what the
                                  table says, or the size is picked wrongly
    the hotspot                   outside the image is a cursor that points at
                                  a pixel that does not exist
    the alpha                     a cursor whose every pixel is transparent is
                                  a cursor the user cannot see, and it is the
                                  failure the ARGB byte order produces
"""

from __future__ import annotations

import struct
from pathlib import Path

#: Read from the file, not from :mod:`xcursor`, on purpose: a check that uses
#: the same constants as the writer agrees with the writer about a format they
#: could both have wrong.
MAGIC = 0x72756358
IMAGE_TYPE = 0xFFFD0002
HEADER_BYTES = 16
IMAGE_HEADER_BYTES = 36


def read_cursor(path: Path) -> dict:
    """The contents of one Xcursor file, or a description of why it is not one.

    Returns a dictionary with ``error`` set when the file cannot be read as a
    cursor; the caller turns that into a message. It does not raise, because a
    broken file is one of the things being checked for.
    """
    data = path.read_bytes()
    if len(data) < HEADER_BYTES:
        return {"error": f"only {len(data)} bytes"}

    magic, header_bytes, version, count = struct.unpack_from("<4I", data, 0)
    if magic != MAGIC:
        return {"error": f"magic is {magic:#010x}, not the cursor magic"}
    if header_bytes != HEADER_BYTES:
        return {"error": f"header is {header_bytes} bytes, not {HEADER_BYTES}"}
    if count == 0:
        return {"error": "the table is empty"}
    table_end = HEADER_BYTES + count * 12
    if table_end > len(data):
        return {"error": f"a table of {count} entries does not fit the file"}

    images: list[dict] = []
    for index in range(count):
        entry, subtype, position = struct.unpack_from("<3I", data, HEADER_BYTES + index * 12)
        if entry != IMAGE_TYPE:
            continue
        if position + IMAGE_HEADER_BYTES > len(data):
            return {"error": f"entry {index} points past the end of the file"}
        (
            image_header,
            image_type,
            image_subtype,
            image_version,
            width,
            height,
            xhot,
            yhot,
            delay,
        ) = struct.unpack_from("<9I", data, position)
        if image_header != IMAGE_HEADER_BYTES:
            return {"error": f"entry {index}: image header is {image_header} bytes"}
        if image_type != IMAGE_TYPE or image_subtype != subtype:
            return {"error": f"entry {index}: the table and the image disagree"}
        if delay and image_version != 1:
            return {"error": f"entry {index}: unknown image version {image_version}"}
        if width == 0 or height == 0 or width > 512 or height > 512:
            return {"error": f"entry {index}: implausible size {width}x{height}"}
        if not (0 <= xhot <= width and 0 <= yhot <= height):
            return {"error": f"entry {index}: hotspot {xhot},{yhot} is outside"}
        pixel_bytes = width * height * 4
        if position + IMAGE_HEADER_BYTES + pixel_bytes > len(data):
            return {"error": f"entry {index}: the pixels are cut off"}
        images.append(
            {
                "size": subtype,
                "width": width,
                "height": height,
                "xhot": xhot,
                "yhot": yhot,
                "delay": delay,
                "pixels": data[position + IMAGE_HEADER_BYTES:
                               position + IMAGE_HEADER_BYTES + pixel_bytes],
            }
        )

    if not images:
        return {"error": "no image entries"}

    total_alpha = 0
    for image in images:
        # Every fourth byte is the alpha, because the file stores ARGB and this
        # is read as the bytes it is: B, G, R, A.
        total_alpha += sum(image["pixels"][3::4])
    return {
        "images": images,
        "sizes": sorted({image["size"] for image in images}),
        "frames": len(images),
        "alpha": total_alpha,
        "bytes": len(data),
    }


def check_cursor(path: Path) -> list[str]:
    """Everything wrong with one cursor file, as messages."""
    if not path.exists():
        return [f"{path.name}: missing"]
    if path.is_symlink():
        # A link is not a file to check: following it would check the same
        # cursor once per spelling, which is every spelling but the first.
        target = path.resolve()
        if not target.exists():
            return [f"{path.name}: points at {target.name}, which does not exist"]
        return []
    info = read_cursor(path)
    if "error" in info:
        return [f"{path.name}: {info['error']}"]
    problems = []
    if info["alpha"] == 0:
        problems.append(f"{path.name}: every pixel is transparent")

    # A cursor is allowed to be still: most of this set is, and a still shape
    # written twelve times is twelve times the disk for nothing. What is not
    # allowed is a cursor whose sizes disagree about it - a pointer that pulses
    # at 24 pixels and sits still at 48 is one that changes behaviour when a
    # laptop is plugged into a monitor.
    frames_per_size: dict[int, int] = {}
    for image in info["images"]:
        frames_per_size[image["size"]] = frames_per_size.get(image["size"], 0) + 1
    if len(set(frames_per_size.values())) > 1:
        counts = ", ".join(
            f"{size}px has {count}"
            for size, count in sorted(frames_per_size.items())
        )
        problems.append(
            f"{path.name}: {counts} frame(s), so the animation changes with the "
            "size it is drawn at"
        )
    return problems


def check_theme(root: Path, required: tuple[str, ...], sizes: tuple[int, ...]) -> list[str]:
    """Every problem in a built theme, or an empty list."""
    problems: list[str] = []
    cursors = root / "cursors"
    if not cursors.is_dir():
        return [f"{root} has no cursors directory"]

    for filename in ("index.theme", "cursor.theme"):
        if not (root / filename).is_file():
            problems.append(f"{filename} is missing")

    for name in required:
        problems.extend(check_cursor(cursors / name))

    # The size set is checked once, on the cursor every program has: a theme
    # whose default is missing a size is scaled by the X server, and a theme
    # whose help cursor is missing one is almost never noticed.
    representative = cursors / "default"
    if representative.is_file():
        info = read_cursor(representative)
        if "error" not in info:
            missing = [size for size in sizes if size not in info["sizes"]]
            if missing:
                problems.append(
                    f"default: no {', '.join(str(s) for s in missing)} pixel size"
                )
    return problems


def summary(root: Path) -> str:
    """One line describing what was built, for the installer to print."""
    cursors = root / "cursors"
    if not cursors.is_dir():
        return "nothing"
    files = [
        path for path in cursors.iterdir()
        if path.is_file() and not path.is_symlink()
    ]
    links = [path for path in cursors.iterdir() if path.is_symlink()]
    frames = 0
    total_bytes = 0
    for path in files:
        total_bytes += path.stat().st_size
        info = read_cursor(path)
        if "error" not in info:
            frames += info["frames"]
    kib = total_bytes // 1024
    return (
        f"{len(files)} cursor file(s), {len(links)} linked name(s), "
        f"{frames} frame(s), {kib} KiB"
    )
