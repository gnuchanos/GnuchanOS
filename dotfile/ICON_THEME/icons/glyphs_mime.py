"""Pictograms for file types.

A file manager shows dozens of these in one directory, so they share their
whole silhouette: one page, one folded corner, and a mark that says what kind
of content is inside. That is what makes a directory listing readable — the eye
learns one shape and only has to read the small mark to tell a text file from a
spreadsheet.

Text types get their extension as a label, drawn with the stroke alphabet;
media types get a pictogram, because a music note is recognised faster than the
letters "MP3" ever would be.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, glyph, page
from .glyphs_letters import letter_shapes
from .glyphs_objects import _note, _picture_marks
from .primitives import Arc, Disc, Line, Poly, Rect
from .shape import BOX


def _sheet(tint: str, ink: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [Poly.of(ink, [(70.0, 74.0), (86.0, 74.0), (86.0, 86.0), (70.0, 86.0)])]


def _labelled(tint: str, label: str, ink: str | None = None) -> ShapeList:
    """A page with the extension written in its lower half."""
    text = ink or tint
    return page(palette.FG_BRIGHT, palette.FG_DIM) + letter_shapes(
        label[0], text, (30.0, 48.0, 40.0, 34.0), 7.0
    )


def _film(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 28.0, 50.0, 44.0, 30.0, 3.0),
        Poly.of(tint, [(36.0, 56.0), (52.0, 65.0), (36.0, 74.0)]),
    ]


def _disc_mark(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Disc(tint, 48.0, 66.0, 14.0),
        Disc(palette.FG_BRIGHT, 48.0, 66.0, 4.0),
    ]


@glyph("text-x-generic")
def text_x_generic(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(tint, 30.0, 52.0, 70.0, 52.0, 5.0),
        Line(tint, 30.0, 63.0, 70.0, 63.0, 5.0),
        Line(tint, 30.0, 74.0, 56.0, 74.0, 5.0),
    ]


@glyph("text-plain")
def text_plain(tint: str) -> ShapeList:
    return text_x_generic(tint)


@glyph("text-richtext")
def text_richtext(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(tint, 30.0, 52.0, 62.0, 52.0, 7.0),
        Line(tint, 30.0, 64.0, 70.0, 64.0, 4.0),
        Line(tint, 30.0, 75.0, 70.0, 75.0, 4.0),
    ]


@glyph("text-html")
def text_html(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(tint, 40.0, 54.0, 28.0, 65.0, 5.0),
        Line(tint, 28.0, 65.0, 40.0, 76.0, 5.0),
        Line(tint, 60.0, 54.0, 72.0, 65.0, 5.0),
        Line(tint, 72.0, 65.0, 60.0, 76.0, 5.0),
    ]


@glyph("text-x-script")
def text_x_script(tint: str) -> ShapeList:
    return text_html(tint)


@glyph("text-x-python")
def text_x_python(tint: str) -> ShapeList:
    return _labelled(tint, "p")


@glyph("text-x-c")
def text_x_c(tint: str) -> ShapeList:
    return _labelled(tint, "c")


@glyph("text-csv")
def text_csv(tint: str) -> ShapeList:
    return document_spreadsheet_marks(tint)


@glyph("text-x-readme")
def text_x_readme(tint: str) -> ShapeList:
    return text_x_generic(tint)


@glyph("application-pdf")
def application_pdf(tint: str) -> ShapeList:
    return _labelled(palette.ERROR, "p")


@glyph("application-x-executable")
def application_x_executable(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 26.0, 58.0, 48.0, 26.0, 4.0),
        Line(palette.FG_BRIGHT, 34.0, 66.0, 44.0, 71.0, 4.0),
        Line(palette.FG_BRIGHT, 34.0, 76.0, 52.0, 76.0, 4.0),
    ]


@glyph("application-x-object")
def application_x_object(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Arc(tint, 50.0, 66.0, 14.0, 0.0, 359.999, 6.0),
        Line(tint, 50.0, 46.0, 50.0, 52.0, 6.0),
        Line(tint, 50.0, 80.0, 50.0, 86.0, 6.0),
        Line(tint, 30.0, 66.0, 36.0, 66.0, 6.0),
        Line(tint, 64.0, 66.0, 70.0, 66.0, 6.0),
    ]


@glyph("application-x-sharedlib")
def application_x_sharedlib(tint: str) -> ShapeList:
    return application_x_object(tint)


@glyph("application-x-archive")
def application_x_archive(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 44.0, 48.0, 12.0, 30.0, 3.0),
        Rect(palette.FG_BRIGHT, 47.0, 54.0, 6.0, 5.0, 1.0),
        Rect(palette.FG_BRIGHT, 47.0, 64.0, 6.0, 5.0, 1.0),
    ]


@glyph("package-x-generic")
def package_x_generic(tint: str) -> ShapeList:
    return application_x_archive(tint)


@glyph("image-x-generic")
def image_x_generic(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 26.0, 50.0, 48.0, 32.0, 3.0),
        Disc(palette.FG_BRIGHT, 38.0, 60.0, 4.0),
        Poly.of(palette.FG_BRIGHT, [(30.0, 80.0), (48.0, 62.0), (70.0, 80.0)]),
    ]


@glyph("audio-x-generic")
def audio_x_generic(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + _note(tint, 50.0, 72.0, 0.85)


@glyph("video-x-generic")
def video_x_generic(tint: str) -> ShapeList:
    return _film(tint)


@glyph("font-x-generic")
def font_x_generic(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + letter_shapes(
        "a", tint, (28.0, 48.0, 44.0, 36.0), 7.0
    )


def document_spreadsheet_marks(tint: str) -> ShapeList:
    """A page with a three by three grid, for CSV and spreadsheets."""
    shapes = page(palette.FG_BRIGHT, palette.FG_DIM)
    for column in range(3):
        for row in range(2):
            shapes.append(
                Rect(tint, 28.0 + column * 16.0, 50.0 + row * 16.0, 13.0, 13.0, 2.0)
            )
    return shapes


@glyph("x-office-document")
def x_office_document(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(tint, 30.0, 52.0, 70.0, 52.0, 6.0),
        Line(tint, 30.0, 64.0, 70.0, 64.0, 4.0),
        Line(tint, 30.0, 75.0, 58.0, 75.0, 4.0),
    ]


@glyph("x-office-spreadsheet")
def x_office_spreadsheet(tint: str) -> ShapeList:
    return document_spreadsheet_marks(tint)


@glyph("x-office-presentation")
def x_office_presentation(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 28.0, 50.0, 44.0, 24.0, 3.0),
        Line(tint, 50.0, 74.0, 50.0, 82.0, 4.0),
    ]


@glyph("x-office-drawing")
def x_office_drawing(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + _picture_marks(tint, top=66.0)


@glyph("x-office-address-book")
def x_office_address_book(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Disc(tint, 50.0, 58.0, 8.0),
        Poly.of(tint, [(36.0, 80.0), (64.0, 80.0), (50.0, 64.0)]),
    ]


@glyph("x-office-calendar")
def x_office_calendar(tint: str) -> ShapeList:
    from .glyphs_objects import calendar

    return calendar(tint)


@glyph("vcalendar")
def vcalendar(tint: str) -> ShapeList:
    return x_office_calendar(tint)


@glyph("application-x-firmware")
def application_x_firmware(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 34.0, 52.0, 32.0, 28.0, 3.0),
        Line(tint, 50.0, 44.0, 50.0, 52.0, 5.0),
    ]


@glyph("application-octet-stream")
def application_octet_stream(tint: str) -> ShapeList:
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Rect(tint, 32.0, 52.0, 36.0, 28.0, 4.0),
        Disc(palette.FG_BRIGHT, 44.0, 62.0, 4.0),
        Disc(palette.FG_BRIGHT, 56.0, 62.0, 4.0),
        Line(palette.FG_BRIGHT, 42.0, 72.0, 58.0, 72.0, 4.0),
    ]


@glyph("application-x-compressed-tar")
def application_x_compressed_tar(tint: str) -> ShapeList:
    return application_x_archive(tint)


@glyph("application-zip")
def application_zip(tint: str) -> ShapeList:
    return application_x_archive(tint)


@glyph("text-x-tex")
def text_x_tex(tint: str) -> ShapeList:
    """A page labelled t, for TeX sources and their auxiliaries.

    TeX files are plain text, and they are drawn as plain text with the letter
    of the tool that reads them, which is the same treatment every other source
    file gets.
    """
    return _labelled(tint, "t")


@glyph("x-office-database")
def x_office_database(tint: str) -> ShapeList:
    """A stacked cylinder: the shape a database has had since the seventies.

    The one office document whose silhouette is not a sheet of paper. Drawn as
    a body between two ellipses with the rim and one shelf line showing, so it
    is told apart from the spreadsheet at sixteen pixels.
    """
    return [
        Disc(tint, 50.0, 72.0, 26.0),
        Rect(tint, 24.0, 30.0, 52.0, 42.0),
        Disc(tint, 50.0, 30.0, 26.0),
        Arc(palette.BG_DARKEST + "99", 50.0, 30.0, 18.0, 0.0, 359.999, 4.0),
        Arc(palette.BG_DARKEST + "77", 50.0, 47.0, 22.0, 15.0, 150.0, 4.0),
    ]


@glyph("unknown")
def unknown(tint: str) -> ShapeList:
    """The icon shown for a file whose type nothing recognises."""
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Poly.of(tint, [(36.0, 78.0), (64.0, 78.0), (50.0, 50.0)]),
        Disc(palette.FG_BRIGHT, 50.0, 74.0, 3.0),
    ]
