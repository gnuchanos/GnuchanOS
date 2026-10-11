"""Shape lists as SVG documents.

The scalable half of the theme is written here. An SVG file is the same shape
list the PNG rasteriser consumes, emitted as path data instead of sampled into
pixels, which is the point of keeping the outline and the distance function on
one class in :mod:`icons.primitives`: an icon drawn at 256 pixels and the same
icon drawn at 16 are one geometry read two ways.

One thing is stripped before the markup is written: a shape with no area draws
nothing but costs a file read, and ``alpha=0`` shapes exist in the artwork as
the holes other shapes punch, so they are dropped rather than written out.
"""

from __future__ import annotations

from typing import Sequence

from .shape import BOX, fmt

SVG_NAMESPACE = "http://www.w3.org/2000/svg"
DECLARATION = '<?xml version="1.0" encoding="UTF-8"?>\n'

#: Shapes shorter than this in both axes are not worth writing to a file.
MINIMUM_EXTENT = 1e-6


def _is_visible(shape: object) -> bool:
    """Whether a shape would put any ink on the canvas.

    A fully transparent shape is a no-op in the rasteriser and an empty path in
    SVG, so it is dropped here: the artwork uses ``#00000000`` to mean "nothing"
    and there is no reason for a file to say so.
    """
    fill = getattr(shape, "fill", "")
    if isinstance(fill, str) and fill.strip().lower().lstrip("#").endswith("00"):
        if len(fill.strip().lstrip("#")) == 8:
            return False
    return True


def document(shapes: Sequence[object], size: int, box: float = BOX) -> str:
    """An SVG document of ``size`` units square containing ``shapes``.

    The view box is the authoring box, so every coordinate in the artwork is a
    plain number between zero and a hundred regardless of the pixel size the
    file is rendered at.
    """
    drawn = [shape for shape in shapes if _is_visible(shape)]
    body = "".join(f"  {shape.svg()}\n" for shape in drawn)  # type: ignore[attr-defined]
    return (
        f"{DECLARATION}"
        f'<svg xmlns="{SVG_NAMESPACE}" width="{size}" height="{size}"'
        f' viewBox="0 0 {fmt(box)} {fmt(box)}">\n'
        f"{body}"
        "</svg>\n"
    )


def symbol(shapes: Sequence[object], box: float = BOX) -> str:
    """Just the drawn elements, for embedding in a larger document.

    The contact sheet uses this: it composes many glyphs into one file, and
    putting a whole ``svg`` element inside another is not valid markup.
    """
    drawn = [shape for shape in shapes if _is_visible(shape)]
    return "".join(f"  {shape.svg()}\n" for shape in drawn)  # type: ignore[attr-defined]
