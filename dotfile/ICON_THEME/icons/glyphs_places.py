"""Pictograms for places: the sidebar of every file manager.

Home, desktop, trash, the network locations and the bookmark ribbon are the
handful of names a file manager asks for before it asks for anything else, and
they are looked at more often than any file type. They are drawn as whole
objects — a house, a bin, a pin — rather than as pages, because a place is a
destination rather than a document.

The artwork here is also what the action vocabulary borrows: ``go-home`` and
``user-home`` are the same house on purpose, since a user who has learnt the
shape in the sidebar should recognise the button in a toolbar.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, glyph, ring
from .primitives import Disc, Line, Poly, Rect


@glyph("user-home")
def user_home(tint: str) -> ShapeList:
    """A house: a roof with eaves, a body and a door."""
    return [
        Poly.of(
            tint,
            [
                (50.0, 14.0),
                (88.0, 46.0),
                (76.0, 46.0),
                (76.0, 82.0),
                (24.0, 82.0),
                (24.0, 46.0),
                (12.0, 46.0),
            ],
        ),
        Rect(palette.BG_DARKEST + "88", 42.0, 58.0, 16.0, 24.0, 2.0),
    ]


@glyph("user-desktop")
def user_desktop(tint: str) -> ShapeList:
    """The desktop itself, drawn as the monitor it is usually seen on."""
    from .glyphs_hardware import computer

    return computer(tint)


@glyph("user-trash")
def user_trash(tint: str) -> ShapeList:
    """A waste bin with a lid, a handle and three ridges."""
    return [
        Poly.of(tint, [(20.0, 30.0), (80.0, 30.0), (74.0, 84.0), (26.0, 84.0)]),
        Rect(tint, 14.0, 20.0, 72.0, 10.0, 4.0),
        Rect(palette.BG_DARKEST + "77", 42.0, 12.0, 16.0, 9.0, 3.0),
        Line(palette.BG_DARKEST + "55", 38.0, 40.0, 40.0, 74.0, 3.0),
        Line(palette.BG_DARKEST + "55", 50.0, 40.0, 50.0, 74.0, 3.0),
        Line(palette.BG_DARKEST + "55", 62.0, 40.0, 60.0, 74.0, 3.0),
    ]


@glyph("network-server")
def network_server(tint: str) -> ShapeList:
    """A rack unit: a boxed chassis with slots and a status light."""
    return [
        Rect(tint, 16.0, 24.0, 68.0, 52.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 22.0, 30.0, 56.0, 12.0, 2.0),
        Rect(palette.BG_DARKEST + "88", 22.0, 48.0, 56.0, 12.0, 2.0),
        Disc(palette.FG_BRIGHT, 30.0, 68.0, 4.0),
        Rect(palette.FG_BRIGHT, 42.0, 64.0, 32.0, 8.0, 2.0),
    ]


@glyph("network-workgroup")
def network_workgroup(tint: str) -> ShapeList:
    """Two screens and the link between them: the workgroup symbol."""
    return [
        Rect(tint, 8.0, 26.0, 36.0, 28.0, 4.0),
        Rect(tint, 56.0, 26.0, 36.0, 28.0, 4.0),
        Rect(palette.BG_DARKEST + "88", 12.0, 30.0, 28.0, 20.0, 2.0),
        Rect(palette.BG_DARKEST + "88", 60.0, 30.0, 28.0, 20.0, 2.0),
        Line(tint, 26.0, 58.0, 26.0, 68.0, 5.0),
        Line(tint, 74.0, 58.0, 74.0, 68.0, 5.0),
        Line(tint, 26.0, 68.0, 74.0, 68.0, 5.0),
        Line(tint, 50.0, 68.0, 50.0, 80.0, 5.0),
    ]


@glyph("network")
def network(tint: str) -> ShapeList:
    """Three nodes joined by links: the generic network mark."""
    return [
        Disc(tint, 50.0, 22.0, 11.0),
        Disc(tint, 22.0, 76.0, 11.0),
        Disc(tint, 78.0, 76.0, 11.0),
        Line(tint, 50.0, 33.0, 27.0, 65.0, 5.0),
        Line(tint, 50.0, 33.0, 73.0, 65.0, 5.0),
        Line(tint, 33.0, 76.0, 67.0, 76.0, 5.0),
    ]


@glyph("bookmark")
def bookmark(tint: str) -> ShapeList:
    """A ribbon with a notched foot, for bookmarks and favourites."""
    return [
        Poly.of(tint, [(28.0, 12.0), (72.0, 12.0), (72.0, 88.0), (50.0, 70.0), (28.0, 88.0)]),
    ]


@glyph("mark-location")
def mark_location(tint: str) -> ShapeList:
    """A map pin with a hole through it, for "show me where this is"."""
    return [
        Poly.of(tint, [(50.0, 88.0), (29.0, 52.0), (71.0, 52.0)]),
        Disc(tint, 50.0, 38.0, 24.0),
        Disc(palette.BG_DARKEST + "99", 50.0, 38.0, 9.0),
    ]


@glyph("search-location")
def search_location(tint: str) -> ShapeList:
    """A magnifier over a pin: find a place rather than a file."""
    return [
        Poly.of(tint, [(46.0, 82.0), (29.0, 52.0), (63.0, 52.0)]),
        Disc(tint, 46.0, 40.0, 20.0),
        Disc(palette.BG_DARKEST + "99", 46.0, 40.0, 8.0),
        Line(palette.FG_BRIGHT, 60.0, 56.0, 78.0, 74.0, 7.0),
    ]


@glyph("folder-remote")
def folder_remote(tint: str) -> ShapeList:
    """A folder with a network node on it, for a mount from another machine."""
    from .glyphs_objects import folder

    return folder(tint) + ring(palette.FG_BRIGHT, 66.0, 60.0, 12.0, 5.0)
