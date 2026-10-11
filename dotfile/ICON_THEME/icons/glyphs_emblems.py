"""Overlay badges: the small marks a file manager puts on a file.

An emblem is drawn at the corner of another icon, so it has to say one thing
with no room to say it in. The badge-shaped ones — urgent, success, error, new —
therefore share one disc and differ only in the mark inside it, which is also
what stops them from being confused with each other at 16 px: the silhouette is
identical and only the mark is read.

Most of the names in the ``Emblems`` context are already drawn elsewhere in the
theme — the padlock, the globe, the envelope, the chain link — and the catalogue
points at those rather than at a second copy. What lives here is only the
handful of marks that exist for no other reason than to be a badge.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, arrow, check, cross, glyph, page, star_shape
from .primitives import Disc, Line, Poly, Rect

BADGE_CENTRE = (50.0, 52.0)
BADGE_RADIUS = 32.0


def _badge(fill: str) -> ShapeList:
    """The disc every badge-shaped emblem is drawn on."""
    return [Disc(fill, BADGE_CENTRE[0], BADGE_CENTRE[1], BADGE_RADIUS)]


@glyph("emblem-generic")
def emblem_generic(tint: str) -> ShapeList:
    """A luggage tag: a file with a type the theme does not know."""
    return [
        Poly.of(
            tint,
            [(14.0, 36.0), (58.0, 36.0), (86.0, 50.0), (58.0, 64.0), (14.0, 64.0)],
        ),
        Disc(palette.BG_DARKEST + "99", 28.0, 50.0, 6.0),
    ]


@glyph("emblem-new")
def emblem_new(tint: str) -> ShapeList:
    """A four-point sparkle on a badge: a file that has just appeared."""
    return _badge(tint) + [
        star_shape(palette.BG_DARKEST, 50.0, 52.0, 24.0, 8.0, 4),
        star_shape(palette.BG_DARKEST, 74.0, 32.0, 9.0, 3.0, 4),
    ]


@glyph("emblem-unreadable")
def emblem_unreadable(tint: str) -> ShapeList:
    """A page struck through: a file that cannot be opened."""
    return page(palette.FG_BRIGHT, palette.FG_DIM) + [
        Line(palette.BG_DARKEST + "CC", 20.0, 84.0, 84.0, 20.0, 11.0)
    ]


@glyph("emblem-unlocked")
def emblem_unlocked(tint: str) -> ShapeList:
    """A padlock with the shackle swung open to the right."""
    return [
        Line(tint, 34.0, 46.0, 34.0, 34.0, 8.0),
        Line(tint, 34.0, 34.0, 58.0, 34.0, 8.0),
        Line(tint, 58.0, 34.0, 62.0, 24.0, 8.0),
        Rect(tint, 26.0, 46.0, 48.0, 40.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 62.0, 6.0),
    ]


@glyph("emblem-raised")
def emblem_raised(tint: str) -> ShapeList:
    """An up arrow on a badge: a file ordered above its neighbours."""
    return _badge(tint) + arrow(
        palette.BG_DARKEST, 50.0, 72.0, 50.0, 32.0, 8.0, 15.0
    )


@glyph("emblem-dropbox")
def emblem_dropbox(tint: str) -> ShapeList:
    """An isometric crate: a file synchronised with a cloud service."""
    return [
        Poly.of(
            tint,
            [
                (18.0, 40.0),
                (50.0, 26.0),
                (82.0, 40.0),
                (82.0, 70.0),
                (50.0, 84.0),
                (18.0, 70.0),
            ],
        ),
        Poly.of(
            palette.BG_DARKEST + "66",
            [(18.0, 40.0), (50.0, 54.0), (82.0, 40.0), (50.0, 26.0)],
        ),
        Line(palette.BG_DARKEST + "66", 50.0, 54.0, 50.0, 84.0, 4.0),
    ]


@glyph("emblem-urgent")
def emblem_urgent(tint: str) -> ShapeList:
    """An exclamation mark on a badge: a file that needs attention."""
    return _badge(tint) + [
        Rect(palette.BG_DARKEST, 46.0, 32.0, 8.0, 26.0, 4.0),
        Disc(palette.BG_DARKEST, 50.0, 70.0, 5.0),
    ]


@glyph("emblem-success")
def emblem_success(tint: str) -> ShapeList:
    """A tick on a badge: an operation that completed."""
    return _badge(tint) + check(palette.BG_DARKEST, 50.0, 52.0, 36.0, 9.0)


@glyph("emblem-error")
def emblem_error(tint: str) -> ShapeList:
    """A cross on a badge: a file or an operation that failed."""
    return _badge(tint) + cross(palette.BG_DARKEST, 50.0, 52.0, 21.0, 9.0)


@glyph("emblem-encrypted")
def emblem_encrypted(tint: str) -> ShapeList:
    """A key: an encrypted or signed file."""
    return [
        Disc(tint, 38.0, 38.0, 20.0),
        Disc(palette.BG_DARKEST + "99", 38.0, 38.0, 8.0),
        Line(tint, 52.0, 52.0, 84.0, 84.0, 10.0),
        Line(tint, 68.0, 76.0, 78.0, 66.0, 8.0),
    ]


@glyph("emblem-export")
def emblem_export(tint: str) -> ShapeList:
    """An arrow climbing out of a tray: ``emblem-import``'s mirror image.

    Import and export are the same drawing with the arrow reversed, which is why
    they are one file apart: the pair is what makes the direction legible
    without a second trip to the legend.
    """
    return arrow(tint, 50.0, 60.0, 50.0, 12.0, 9.0, 17.0) + [
        Line(tint, 18.0, 56.0, 18.0, 82.0, 8.0),
        Line(tint, 18.0, 82.0, 82.0, 82.0, 8.0),
        Line(tint, 82.0, 82.0, 82.0, 56.0, 8.0),
    ]


@glyph("emblem-personal")
def emblem_personal(tint: str) -> ShapeList:
    """A head and shoulders on a badge: a file that belongs to one person.

    The counterpart of :func:`emblem_system`, and drawn the same way — the same
    disc, the same dark ink — because the two are read side by side on a list of
    files the user owns and files the system owns.
    """
    return _badge(tint) + [
        Disc(palette.BG_DARKEST, 50.0, 40.0, 11.0),
        Rect(palette.BG_DARKEST, 28.0, 56.0, 44.0, 22.0, 11.0),
    ]
