"""Pictograms for the things a file manager or a launcher has to draw.

A directory is mostly folders, documents, pictures and music, so these are the
shapes a user sees more than any other in the theme. They are also the ones
other modules borrow: the mime module starts every page from a shared helper,
and the action module reuses the folder rather than drawing a second one, which
is what keeps ``folder-new`` recognisably the same object as ``folder``.

The two private helpers, :func:`_note` and :func:`_picture_marks`, are marks
rather than icons. :mod:`icons.glyphs_mime` places them on a page, so they are
told where to start and how large to be instead of filling the box on their own.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, arrow, dots, glyph, page, ring
from .primitives import Disc, Line, Poly, Rect


def _note(fill: str, cx: float, cy: float, scale: float = 1.0) -> ShapeList:
    """A musical note whose head sits at ``cx, cy``.

    ``scale`` is how large the note is drawn: the mime module asks for a small
    one to sit on a page, the audio player for a large one to fill the box.
    Drawing it in one place is what stops the two from disagreeing about which
    way the flag should point.
    """
    head = 9.0 * scale
    stem = 32.0 * scale
    thickness = 5.0 * scale
    stem_x = cx + head * 0.85
    top = cy - stem
    flag_dx = 15.0 * scale
    flag_dy = 12.0 * scale
    return [
        Disc(fill, cx, cy, head),
        Line(fill, stem_x, cy, stem_x, top, thickness),
        Poly.of(
            fill,
            [
                (stem_x - thickness / 2.0, top),
                (stem_x + flag_dx, top + flag_dy),
                (stem_x + flag_dx, top + flag_dy * 1.9),
                (stem_x - thickness / 2.0, top + flag_dy * 1.1),
            ],
        ),
    ]


def _picture_marks(fill: str, top: float = 62.0) -> ShapeList:
    """A framed landscape, from ``top`` down to the page's bottom edge.

    The mime module places this on a page, so it is told where its frame starts
    rather than filling the box. The sun and the hill scale with the frame, and
    the picture ends exactly where the page does.
    """
    height = max(86.0 - top, 16.0)
    bottom = top + height
    return [
        Rect(fill, 26.0, top, 48.0, height, 3.0),
        Disc(palette.FG_BRIGHT, 38.0, top + height * 0.32, 3.5),
        Poly.of(
            palette.FG_BRIGHT,
            [(30.0, bottom), (48.0, top + height * 0.38), (70.0, bottom)],
        ),
    ]


@glyph("folder")
def folder(tint: str) -> ShapeList:
    """The generic directory: a tabbed body with a raised front panel."""
    return [
        Poly.of(
            tint,
            [
                (12.0, 24.0),
                (42.0, 24.0),
                (50.0, 34.0),
                (88.0, 34.0),
                (88.0, 80.0),
                (12.0, 80.0),
            ],
        ),
        Poly.of(
            palette.FG_BRIGHT + "26",
            [(12.0, 44.0), (88.0, 44.0), (88.0, 80.0), (12.0, 80.0)],
        ),
    ]


@glyph("mail")
def mail(tint: str) -> ShapeList:
    """An envelope with its flap drawn in the surface colour."""
    return [
        Rect(tint, 10.0, 26.0, 80.0, 50.0, 6.0),
        Line(palette.BG_DARKEST + "88", 12.0, 30.0, 50.0, 56.0, 5.0),
        Line(palette.BG_DARKEST + "88", 88.0, 30.0, 50.0, 56.0, 5.0),
    ]


@glyph("chat")
def chat(tint: str) -> ShapeList:
    """A speech bubble with three dots, for messaging and presence panels."""
    return [
        Rect(tint, 10.0, 18.0, 80.0, 52.0, 10.0),
        Poly.of(tint, [(26.0, 66.0), (48.0, 66.0), (28.0, 88.0)]),
    ] + dots(palette.BG_DARKEST + "99", 50.0, 44.0, 3, 4.5, 15.0)


@glyph("book")
def book(tint: str) -> ShapeList:
    """An open book: two pages tilted away from a sunken spine."""
    return [
        Poly.of(tint, [(10.0, 24.0), (48.0, 32.0), (48.0, 84.0), (10.0, 76.0)]),
        Poly.of(tint, [(90.0, 24.0), (52.0, 32.0), (52.0, 84.0), (90.0, 76.0)]),
        Line(palette.BG_DARKEST + "66", 50.0, 32.0, 50.0, 84.0, 4.0),
        Line(palette.FG_BRIGHT + "55", 18.0, 41.0, 40.0, 46.0, 4.0),
        Line(palette.FG_BRIGHT + "55", 60.0, 46.0, 82.0, 41.0, 4.0),
    ]


@glyph("calendar")
def calendar(tint: str) -> ShapeList:
    """A wall calendar: a header band, two rings and a grid of days."""
    shapes: ShapeList = [
        Rect(tint, 14.0, 20.0, 72.0, 66.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 14.0, 20.0, 72.0, 14.0, 6.0),
        Rect(palette.FG_BRIGHT, 26.0, 12.0, 8.0, 14.0, 3.0),
        Rect(palette.FG_BRIGHT, 66.0, 12.0, 8.0, 14.0, 3.0),
    ]
    for row in range(2):
        for column in range(4):
            shapes.append(
                Rect(
                    palette.FG_BRIGHT,
                    22.0 + column * 15.0,
                    44.0 + row * 20.0,
                    11.0,
                    13.0,
                    2.0,
                )
            )
    return shapes


@glyph("document")
def document(tint: str) -> ShapeList:
    """A page with text on it, for "document properties" and its relatives."""
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(tint, 30.0, 52.0, 70.0, 52.0, 6.0),
        Line(tint, 30.0, 64.0, 70.0, 64.0, 4.0),
        Line(tint, 30.0, 75.0, 58.0, 75.0, 4.0),
    ]


@glyph("code")
def code(tint: str) -> ShapeList:
    """Angle brackets with a slash between them, the mark for source code."""
    return [
        Line(tint, 40.0, 26.0, 22.0, 50.0, 8.0),
        Line(tint, 22.0, 50.0, 40.0, 74.0, 8.0),
        Line(tint, 60.0, 26.0, 78.0, 50.0, 8.0),
        Line(tint, 78.0, 50.0, 60.0, 74.0, 8.0),
        Line(tint, 56.0, 20.0, 44.0, 80.0, 7.0),
    ]


@glyph("archive")
def archive(tint: str) -> ShapeList:
    """A parcel: a lidded box with the tape down the middle."""
    return [
        Rect(tint, 16.0, 34.0, 68.0, 50.0, 5.0),
        Poly.of(
            palette.FG_BRIGHT,
            [(16.0, 34.0), (50.0, 20.0), (84.0, 34.0), (50.0, 48.0)],
        ),
        Rect(palette.FG_BRIGHT, 44.0, 40.0, 12.0, 44.0, 2.0),
    ]


@glyph("map")
def map(tint: str) -> ShapeList:
    """A folded map: three panels with a marker on the middle one."""
    return [
        Poly.of(
            tint,
            [
                (12.0, 28.0),
                (37.0, 20.0),
                (63.0, 28.0),
                (88.0, 20.0),
                (88.0, 72.0),
                (63.0, 80.0),
                (37.0, 72.0),
                (12.0, 80.0),
            ],
        ),
        Line(palette.BG_DARKEST + "55", 37.0, 20.0, 37.0, 72.0, 4.0),
        Line(palette.BG_DARKEST + "55", 63.0, 28.0, 63.0, 80.0, 4.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 50.0, 8.0),
    ]


@glyph("clock")
def clock(tint: str) -> ShapeList:
    """A clock face with its hands at ten past two."""
    return ring(tint, 50.0, 50.0, 32.0, 7.0) + [
        Line(tint, 50.0, 50.0, 50.0, 28.0, 7.0),
        Line(tint, 50.0, 50.0, 68.0, 58.0, 7.0),
    ]


@glyph("lock")
def lock(tint: str) -> ShapeList:
    """A padlock with a keyhole, for password managers and locked state."""
    return [
        Line(tint, 32.0, 46.0, 32.0, 38.0, 8.0),
        Line(tint, 32.0, 38.0, 68.0, 38.0, 8.0),
        Line(tint, 68.0, 38.0, 68.0, 46.0, 8.0),
        Rect(tint, 26.0, 46.0, 48.0, 40.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 62.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 47.0, 62.0, 6.0, 12.0, 2.0),
    ]


@glyph("download")
def download(tint: str) -> ShapeList:
    """An arrow falling into a tray, for download managers and torrents."""
    return arrow(tint, 50.0, 12.0, 50.0, 60.0, 9.0, 17.0) + [
        Line(tint, 18.0, 56.0, 18.0, 82.0, 8.0),
        Line(tint, 18.0, 82.0, 82.0, 82.0, 8.0),
        Line(tint, 82.0, 82.0, 82.0, 56.0, 8.0),
    ]


# --- the two folder operations the specification names -------------------------
# ``folder-copy`` and ``folder-move`` are names in the specification with no
# artwork behind them. Both are the folder the theme already draws, plus the one
# mark that says which operation it is, so a user reading a context menu sees
# the same object twice and has to read only the small mark.


@glyph("folder-copy")
def folder_copy(tint: str) -> ShapeList:
    """A folder with a second folder behind it: copying a directory."""
    return [
        Poly.of(
            tint + "66",
            [
                (20.0, 12.0),
                (48.0, 12.0),
                (56.0, 22.0),
                (94.0, 22.0),
                (94.0, 68.0),
                (20.0, 68.0),
            ],
        ),
    ] + folder(tint)


@glyph("folder-move")
def folder_move(tint: str) -> ShapeList:
    """A folder with an arrow out of it: moving a directory elsewhere."""
    return folder(tint) + arrow(palette.FG_BRIGHT, 28.0, 60.0, 78.0, 60.0, 7.0, 14.0)
