"""The few glyphs that are not part of any larger family.

Most of the artwork lives in a module named after what it draws: objects,
actions, devices. The handful in here is shared by so many contexts that it has
no single home — a text mark, a grid, a row of bars, a help badge, a printer and
a stop sign — so they live together rather than being filed under a heading that
would be wrong for half of them.

Several are aliases rather than new drawings, and they are aliases on purpose:
``process-stop`` and ``process-working`` must look like the same thing, and a
second copy of the artwork is how they stop doing that.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, bars, cross, glyph
from .glyphs_letters import letter_shapes
from .primitives import Arc, Disc, Line, Poly, Rect


@glyph("text")
def text(tint: str) -> ShapeList:
    """A serif capital T: the mark for anything that is about text itself."""
    return [
        Line(tint, 22.0, 24.0, 78.0, 24.0, 9.0),
        Line(tint, 50.0, 24.0, 50.0, 76.0, 9.0),
        Line(tint, 38.0, 76.0, 62.0, 76.0, 8.0),
    ]


@glyph("grid")
def grid(tint: str) -> ShapeList:
    """Four squares, the mark for a layout, a table or a list style."""
    return [
        Rect(tint, 18.0 + column * 34.0, 18.0 + row * 34.0, 30.0, 30.0, 4.0)
        for row in range(2)
        for column in range(2)
    ]


@glyph("bars")
def bars_glyph(tint: str) -> ShapeList:
    """Three stacked bars: the menu mark and the list mark."""
    return bars(tint, 50.0, 50.0, 56.0, 9.0, 3, 20.0)


@glyph("help")
def help(tint: str) -> ShapeList:
    """A question mark in a ring, for help buttons and about dialogs."""
    return [
        Arc(tint, 50.0, 50.0, 34.0, 0.0, 359.999, 8.0),
        Arc(tint, 50.0, 38.0, 13.0, 150.0, 240.0, 8.0),
        Line(tint, 50.0, 51.0, 50.0, 58.0, 8.0),
        Disc(tint, 50.0, 70.0, 5.0),
    ]


@glyph("print")
def print_glyph(tint: str) -> ShapeList:
    """A printer: the same drawing the action module registers."""
    from .glyphs_actions import printer

    return printer(tint)


@glyph("process-stop")
def process_stop(tint: str) -> ShapeList:
    """A stop sign: the x in a ring, used for cancel and for stopping a task."""
    return [Arc(tint, 50.0, 50.0, 34.0, 0.0, 359.999, 8.0)] + cross(
        tint, 50.0, 50.0, 30.0, 8.0
    )


@glyph("process-working")
def process_working(tint: str) -> ShapeList:
    """A ring with one heavy quarter: the icon a busy window shows."""
    return [
        Arc(tint, 50.0, 50.0, 32.0, 0.0, 359.999, 6.0),
        Arc(tint, 50.0, 50.0, 32.0, -90.0, 90.0, 10.0),
    ]


@glyph("application-x-executable-launch")
def application_x_executable_launch(tint: str) -> ShapeList:
    """The generic program mark: a window with a launch arrow leaving it."""
    return [
        Rect(tint, 14.0, 20.0, 66.0, 52.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 14.0, 20.0, 66.0, 10.0, 6.0),
        Poly.of(palette.FG_BRIGHT, [(50.0, 44.0), (76.0, 56.0), (50.0, 68.0)]),
    ]


@glyph("inode-directory")
def inode_directory(tint: str) -> ShapeList:
    """The generic directory, which is the folder by another name."""
    from .glyphs_objects import folder

    return folder(tint)


@glyph("desktop")
def desktop(tint: str) -> ShapeList:
    from .glyphs_places import user_desktop

    return user_desktop(tint)


@glyph("home")
def home(tint: str) -> ShapeList:
    from .glyphs_places import user_home

    return user_home(tint)


@glyph("trash-empty")
def trash_empty(tint: str) -> ShapeList:
    from .glyphs_places import user_trash

    return user_trash(tint)


@glyph("folder-home")
def folder_home(tint: str) -> ShapeList:
    from .glyphs_places import user_home

    return user_home(tint)


@glyph("media-eject")
def media_eject(tint: str) -> ShapeList:
    """A triangle over a bar: eject, unchanged since it was drawn on a tape deck.

    The one action mark that is not a chevron or an arrow, because ejecting is
    not moving through a list — it is releasing the medium, and the tray symbol
    is what every desktop and every car stereo already uses for it.
    """
    return [
        Poly.of(tint, [(50.0, 18.0), (82.0, 56.0), (18.0, 56.0)]),
        Rect(tint, 18.0, 64.0, 64.0, 14.0, 3.0),
    ]
