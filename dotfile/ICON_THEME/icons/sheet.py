"""Contact sheets: every glyph drawn once, for reviewing the set as a whole.

An icon theme is judged as a set rather than one icon at a time. What matters is
that a folder, a page and a drive sit beside each other without one of them
shouting, and that the marks a toolbar is built from still read at sixteen
pixels. Neither is visible in a single icon, and neither is caught by the build:
both are seen only by putting the whole set on one page and looking at it.

The sheet is written as SVG because a glyph is already path data in the
authoring box, so dropping one into a cell is a single transform. That keeps the
module free of a second rasteriser and the file small enough to open in a
browser at any size.

    python -m icons.sheet --size 24 --out sheet-24.svg
    python -m icons.sheet --size 16 --only folder,document,computer
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from . import palette, svg
from .glyphs_base import GLYPHS, build, defined, load_all
from .shape import BOX, fmt

# A cell holds the largest icon on the sheet plus a margin, so the spacing
# between icons is the same whatever size the sheet is drawn at.
MARGIN = 10
CELL = 56
DEFAULT_SIZE = 24
DEFAULT_COLUMNS = 16


def _cell(x: float, y: float, size: int, shapes: list[object]) -> str:
    """One glyph, centred in its cell and scaled out of the authoring box."""
    scale = size / BOX
    offset = (CELL - size) / 2.0
    return (
        f'  <g transform="translate({fmt(x + offset)} {fmt(y + offset)})'
        f' scale({fmt(scale)})">\n'
        f"{svg.symbol(shapes)}"
        "  </g>\n"
    )


def contact_sheet(
    names: list[str],
    size: int = DEFAULT_SIZE,
    columns: int = DEFAULT_COLUMNS,
    tint: str = palette.ACCENT,
) -> str:
    """An SVG document with ``names`` drawn in a grid at ``size`` pixels each."""
    if not names:
        raise ValueError("a contact sheet needs at least one glyph")
    if columns < 1:
        raise ValueError("a contact sheet needs at least one column")
    rows = math.ceil(len(names) / columns)
    step = CELL + MARGIN
    width = columns * step + MARGIN
    height = rows * step + MARGIN
    parts = [
        svg.DECLARATION,
        f'<svg xmlns="{svg.SVG_NAMESPACE}" width="{width}" height="{height}"'
        f' viewBox="0 0 {width} {height}">\n',
        f'  <rect width="{width}" height="{height}" fill="{palette.BG}"/>\n',
    ]
    for index, name in enumerate(names):
        row, column = divmod(index, columns)
        parts.append(
            _cell(
                MARGIN + column * step,
                MARGIN + row * step,
                size,
                build(name, tint),
            )
        )
    parts.append("</svg>\n")
    return "".join(parts)


def write_sheet(
    path: Path,
    names: list[str],
    size: int = DEFAULT_SIZE,
    columns: int = DEFAULT_COLUMNS,
) -> int:
    """Write a sheet to ``path``, returning how many glyphs it holds."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contact_sheet(names, size, columns), encoding="utf-8")
    return len(names)


def select(only: str = "") -> list[str]:
    """The glyphs a sheet should show: the named ones, or all of them, sorted.

    Naming a glyph that has no artwork is an error rather than an empty cell —
    a blank square in a review sheet is exactly the failure the sheet exists to
    catch, so it should not be produced by the sheet itself.
    """
    load_all()
    if only.strip():
        wanted = [name.strip() for name in only.split(",") if name.strip()]
        missing = [name for name in wanted if not defined(name)]
        if missing:
            raise SystemExit("error: no artwork registered for " + ", ".join(missing))
        return wanted
    return sorted(GLYPHS)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Write a contact sheet of the icon theme's glyphs."
    )
    parser.add_argument("--out", default="contact-sheet.svg", help="where to write the sheet")
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE, help="pixels per glyph")
    parser.add_argument("--columns", type=int, default=DEFAULT_COLUMNS, help="glyphs per row")
    parser.add_argument("--only", default="", help="comma separated glyph names to show")
    args = parser.parse_args(argv)

    names = select(args.only)
    written = write_sheet(Path(args.out), names, args.size, args.columns)
    print(f"wrote {args.out} with {written} glyphs at {args.size}px")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
