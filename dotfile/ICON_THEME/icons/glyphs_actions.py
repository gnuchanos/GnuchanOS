"""The action vocabulary: what every toolbar, menu and context menu asks for.

These are the icons a user does not look at so much as aim at. They are drawn
as a single mark each, without a plate or a page behind them, because they are
placed in a row beside the text they act on and a heavier shape would compete
with it. The set follows the Icon Naming Specification plus the names GTK, Qt
and the common toolkits ask for in practice.

The vocabulary is deliberately built from the shared marks in
:mod:`icons.glyphs_base` — one chevron helper draws all four directions, one
gear draws three different system icons — so a button in a toolbar and the same
concept in a menu are the same picture rather than two similar ones.
"""

from __future__ import annotations

import math

from . import palette
from .glyphs_base import (
    ShapeList,
    arrow,
    arrow_head,
    check,
    chevron,
    cross,
    dots,
    frame,
    gear,
    glyph,
    minus,
    page,
    plus,
    point_on,
    ring,
    star_shape,
)
from .primitives import Arc, Disc, Line, Poly, Rect

POINT = tuple[float, float]


def _pencil(tip: POINT, butt: POINT, width: float, fill: str) -> ShapeList:
    """A pencil whose point is at ``tip`` and whose far end is at ``butt``.

    Drawn from two vectors rather than as a rotated rectangle, so the point is
    exactly on the pixel it is told to touch: ``edit`` aims it at a corner of
    the box, ``save-as`` lays it over a floppy disk, and neither has to guess at
    a rotation.
    """
    tip_x, tip_y = tip
    butt_x, butt_y = butt
    span_x, span_y = butt_x - tip_x, butt_y - tip_y
    length = math.hypot(span_x, span_y)
    if length <= 0.0:
        raise ValueError("a pencil needs two distinct ends")
    along_x, along_y = span_x / length, span_y / length
    side_x, side_y = -along_y, along_x
    half = width / 2.0
    neck_x = tip_x + along_x * length * 0.24
    neck_y = tip_y + along_y * length * 0.24

    def corner(at_x: float, at_y: float, side: float) -> POINT:
        return at_x + side_x * half * side, at_y + side_y * half * side

    return [
        Poly.of(fill, [tip, corner(neck_x, neck_y, 1.0), corner(neck_x, neck_y, -1.0)]),
        Poly.of(
            fill,
            [
                corner(neck_x, neck_y, 1.0),
                corner(butt_x, butt_y, 1.0),
                corner(butt_x, butt_y, -1.0),
                corner(neck_x, neck_y, -1.0),
            ],
        ),
    ]


def _dismiss(tint: str) -> ShapeList:
    """The bold cross that means close or cancel, shared by both names."""
    return cross(tint, 50.0, 50.0, 34.0, 11.0)


def _magnifier(tint: str, mark: ShapeList) -> ShapeList:
    """A magnifier with a ``mark`` inside the lens.

    Search, find and replace and both zooms are the same instrument; only the
    mark inside the lens differs, which is what tells a user at sixteen pixels
    which of the four they are pointing at.
    """
    return (
        ring(tint, 42.0, 42.0, 24.0, 8.0)
        + [Line(tint, 60.0, 60.0, 84.0, 84.0, 10.0)]
        + mark
    )


def _floppy(tint: str) -> ShapeList:
    """The save mark: a disk with a shutter and a label."""
    return [
        Rect(tint, 14.0, 14.0, 72.0, 72.0, 6.0),
        Rect(palette.FG_BRIGHT, 32.0, 16.0, 32.0, 26.0, 2.0),
        Rect(tint, 44.0, 16.0, 12.0, 22.0, 2.0),
        Rect(palette.FG_BRIGHT, 24.0, 54.0, 52.0, 32.0, 3.0),
        Line(palette.BG_DARKEST + "77", 32.0, 64.0, 68.0, 64.0, 4.0),
        Line(palette.BG_DARKEST + "77", 32.0, 74.0, 56.0, 74.0, 4.0),
    ]


@glyph("document-new")
def document_new(tint: str) -> ShapeList:
    """A blank page with a plus on it."""
    return page(palette.FG_BRIGHT, palette.FG_DIM) + plus(tint, 52.0, 62.0, 15.0, 6.0)


@glyph("open")
def document_open(tint: str) -> ShapeList:
    """An open folder: the back of the file drawer and its slanted front."""
    return [
        Poly.of(
            tint,
            [
                (14.0, 32.0),
                (42.0, 32.0),
                (48.0, 40.0),
                (86.0, 40.0),
                (86.0, 58.0),
                (14.0, 58.0),
            ],
        ),
        Poly.of(tint, [(14.0, 58.0), (94.0, 58.0), (78.0, 84.0), (14.0, 84.0)]),
        Line(palette.BG_DARKEST + "55", 14.0, 58.0, 94.0, 58.0, 4.0),
    ]


@glyph("save")
def document_save(tint: str) -> ShapeList:
    """A floppy disk, which is what saving has looked like for forty years."""
    return _floppy(tint)


@glyph("save-as")
def document_save_as(tint: str) -> ShapeList:
    """The same disk with a pencil across it."""
    return _floppy(tint) + _pencil((50.0, 88.0), (76.0, 48.0), 12.0, palette.FG_BRIGHT)


@glyph("printer")
def printer(tint: str) -> ShapeList:
    """A printer with a sheet going in and a sheet coming out."""
    return [
        Rect(palette.FG_BRIGHT, 28.0, 14.0, 44.0, 22.0, 2.0),
        Rect(tint, 18.0, 36.0, 64.0, 28.0, 5.0),
        Disc(palette.BG_DARKEST + "99", 30.0, 50.0, 4.0),
        Rect(palette.FG_BRIGHT, 28.0, 64.0, 44.0, 22.0, 2.0),
        Line(palette.BG_DARKEST + "77", 34.0, 72.0, 66.0, 72.0, 4.0),
        Line(palette.BG_DARKEST + "77", 34.0, 80.0, 56.0, 80.0, 4.0),
    ]


@glyph("refresh")
def refresh(tint: str) -> ShapeList:
    """An open ring with an arrow head, for reload and repeat."""
    radius = 28.0
    return [
        Arc(tint, 50.0, 50.0, radius, 300.0, 280.0, 8.0),
        arrow_head(tint, *point_on(50.0, 50.0, radius, 220.0), 310.0, 14.0),
    ]


@glyph("edit-copy")
def edit_copy(tint: str) -> ShapeList:
    """A sheet behind a sheet, the older one drawn hollow so they separate."""
    return frame(tint, 26.0, 12.0, 62.0, 58.0, 7.0) + [
        Rect(tint, 14.0, 30.0, 58.0, 58.0, 5.0)
    ]


@glyph("edit-cut")
def edit_cut(tint: str) -> ShapeList:
    """Scissors: two crossed blades and two ring handles."""
    return [
        Line(tint, 32.0, 68.0, 62.0, 16.0, 7.0),
        Line(tint, 68.0, 68.0, 38.0, 16.0, 7.0),
        *ring(tint, 26.0, 76.0, 10.0, 6.0),
        *ring(tint, 74.0, 76.0, 10.0, 6.0),
    ]


@glyph("edit-paste")
def edit_paste(tint: str) -> ShapeList:
    """A clipboard with a sheet clipped to it."""
    return [
        Rect(tint, 22.0, 16.0, 56.0, 70.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 28.0, 34.0, 44.0, 48.0, 3.0),
        Rect(palette.FG_BRIGHT, 32.0, 38.0, 36.0, 40.0, 3.0),
        Line(tint, 38.0, 48.0, 62.0, 48.0, 4.0),
        Line(tint, 38.0, 58.0, 62.0, 58.0, 4.0),
        Line(tint, 38.0, 68.0, 54.0, 68.0, 4.0),
        Rect(palette.FG_BRIGHT, 38.0, 10.0, 24.0, 12.0, 3.0),
    ]


@glyph("edit-delete")
def edit_delete(tint: str) -> ShapeList:
    """The waste bin, so that clearing and deleting look like one action."""
    from .glyphs_places import user_trash

    return user_trash(tint)


@glyph("search")
def search(tint: str) -> ShapeList:
    """A plain magnifier, for find and for the search bar."""
    return _magnifier(tint, [])


@glyph("edit-find-replace")
def edit_find_replace(tint: str) -> ShapeList:
    """The magnifier with a plus in the lens: find, then change."""
    return _magnifier(tint, plus(tint, 42.0, 42.0, 12.0, 6.0))


@glyph("edit-undo")
def edit_undo(tint: str) -> ShapeList:
    """An arch with its head turned down at the left end."""
    radius = 26.0
    return [
        Arc(tint, 50.0, 58.0, radius, 180.0, 180.0, 8.0),
        arrow_head(tint, *point_on(50.0, 58.0, radius, 180.0), 90.0, 14.0),
    ]


@glyph("edit-redo")
def edit_redo(tint: str) -> ShapeList:
    """The same arch with its head at the other end."""
    radius = 26.0
    return [
        Arc(tint, 50.0, 58.0, radius, 180.0, 180.0, 8.0),
        arrow_head(tint, *point_on(50.0, 58.0, radius, 360.0), 90.0, 14.0),
    ]


@glyph("edit-select-all")
def edit_select_all(tint: str) -> ShapeList:
    """A marquee with a tick in it: everything."""
    return frame(tint, 12.0, 12.0, 76.0, 76.0, 6.0) + check(tint, 50.0, 50.0, 42.0, 10.0)


@glyph("edit")
def edit(tint: str) -> ShapeList:
    """A pencil, for rename and for anything that edits the thing it names."""
    return _pencil((24.0, 86.0), (74.0, 28.0), 14.0, tint)


@glyph("window-close")
def window_close(tint: str) -> ShapeList:
    """The bold cross a title bar shows."""
    return _dismiss(tint)


@glyph("cancel")
def cancel(tint: str) -> ShapeList:
    """Cancel is the same mark as close, and deliberately so."""
    return _dismiss(tint)


def _corner_arrows(tint: str, outward: bool) -> ShapeList:
    """Four diagonal arrows, pointing away from the centre or back into it.

    Expanding and leaving fullscreen are the same four arrows drawn in opposite
    directions, so they come from one function: nothing is more confusing than
    two icons that differ only by an accident of hand placement.
    """
    near, far = (10.0, 36.0) if outward else (36.0, 12.0)
    shapes: ShapeList = []
    for across in (-1.0, 1.0):
        for down in (-1.0, 1.0):
            shapes += arrow(
                tint,
                50.0 + across * near,
                50.0 + down * near,
                50.0 + across * far,
                50.0 + down * far,
                7.0,
                13.0,
            )
    return shapes


@glyph("view-fullscreen")
def view_fullscreen(tint: str) -> ShapeList:
    """Four arrows leaving the middle, for maximise and fit-to-window."""
    return _corner_arrows(tint, outward=True)


@glyph("view-restore")
def view_restore(tint: str) -> ShapeList:
    """The same four arrows returning to the middle."""
    return _corner_arrows(tint, outward=False)


@glyph("go-next")
def go_next(tint: str) -> ShapeList:
    """A chevron pointing right."""
    return chevron(tint, 50.0, 50.0, 44.0, 11.0, 0.0)


@glyph("go-previous")
def go_previous(tint: str) -> ShapeList:
    """A chevron pointing left."""
    return chevron(tint, 50.0, 50.0, 44.0, 11.0, 180.0)


@glyph("go-down")
def go_down(tint: str) -> ShapeList:
    """A chevron pointing down."""
    return chevron(tint, 50.0, 50.0, 44.0, 11.0, 90.0)


@glyph("go-up")
def go_up(tint: str) -> ShapeList:
    """A chevron pointing up."""
    return chevron(tint, 50.0, 50.0, 44.0, 11.0, 270.0)


@glyph("go-home")
def go_home(tint: str) -> ShapeList:
    """The house from the places context, so both read as the same place."""
    from .glyphs_places import user_home

    return user_home(tint)


@glyph("go-jump")
def go_jump(tint: str) -> ShapeList:
    """A full arrow, for "take me to it" as opposed to "next item"."""
    return arrow(tint, 16.0, 50.0, 86.0, 50.0, 9.0, 18.0)


@glyph("zoom-in")
def zoom_in(tint: str) -> ShapeList:
    """The magnifier with a plus in the lens."""
    return _magnifier(tint, plus(tint, 42.0, 42.0, 12.0, 6.0))


@glyph("zoom-out")
def zoom_out(tint: str) -> ShapeList:
    """The magnifier with a bar in the lens."""
    return _magnifier(tint, minus(tint, 42.0, 42.0, 12.0, 6.0))


@glyph("view-list")
def view_list(tint: str) -> ShapeList:
    """Three bulleted rows, for the list view and for a new tab."""
    shapes: ShapeList = []
    for index in range(3):
        row = 26.0 + index * 22.0
        shapes.append(Disc(tint, 22.0, row, 5.5))
        shapes.append(Line(tint, 38.0, row, 84.0, row, 8.0))
    return shapes


@glyph("view-grid")
def view_grid(tint: str) -> ShapeList:
    """Four squares, for the icon view and the application grid."""
    return [
        Rect(tint, 16.0 + column * 36.0, 16.0 + row * 36.0, 32.0, 32.0, 4.0)
        for row in range(2)
        for column in range(2)
    ]


@glyph("sort")
def view_sort(tint: str) -> ShapeList:
    """Three bars of decreasing length with an upward chevron beside them."""
    return [
        Line(tint, 34.0, 28.0, 86.0, 28.0, 8.0),
        Line(tint, 34.0, 50.0, 70.0, 50.0, 8.0),
        Line(tint, 34.0, 72.0, 56.0, 72.0, 8.0),
    ] + chevron(tint, 18.0, 50.0, 18.0, 7.0, 270.0)


@glyph("view-more")
def view_more(tint: str) -> ShapeList:
    """Three dots, the mark for a menu that opens something else."""
    return dots(tint, 50.0, 50.0, 3, 8.0, 26.0)


@glyph("list-add")
def list_add(tint: str) -> ShapeList:
    """A plus, for adding an entry to whatever list is in front of the user."""
    return plus(tint, 50.0, 50.0, 30.0, 11.0)


@glyph("list-remove")
def list_remove(tint: str) -> ShapeList:
    """A bar, for taking one out again."""
    return minus(tint, 50.0, 50.0, 30.0, 11.0)


@glyph("filter")
def object_filter(tint: str) -> ShapeList:
    """A funnel, which is the only shape a filter has ever needed."""
    return [
        Poly.of(
            tint,
            [
                (12.0, 16.0),
                (88.0, 16.0),
                (60.0, 50.0),
                (60.0, 88.0),
                (40.0, 76.0),
                (40.0, 50.0),
            ],
        )
    ]


@glyph("link")
def link(tint: str) -> ShapeList:
    """Two hoops joined in the middle, for a link or an attachment."""
    return frame(tint, 10.0, 38.0, 48.0, 24.0, 7.0) + frame(
        tint, 42.0, 38.0, 48.0, 24.0, 7.0
    )


@glyph("ok")
def object_select(tint: str) -> ShapeList:
    """A tick, for confirm, select and accepted state."""
    return check(tint, 50.0, 50.0, 58.0, 12.0)


@glyph("share")
def share(tint: str) -> ShapeList:
    """Three linked nodes: the standard mark for passing something on."""
    return [
        Disc(tint, 26.0, 28.0, 12.0),
        Disc(tint, 26.0, 76.0, 12.0),
        Disc(tint, 76.0, 52.0, 12.0),
        Line(tint, 36.0, 34.0, 64.0, 45.0, 7.0),
        Line(tint, 36.0, 70.0, 64.0, 59.0, 7.0),
    ]


@glyph("media-playback-start")
def media_playback_start(tint: str) -> ShapeList:
    """A filled triangle pointing right."""
    return [Poly.of(tint, [(32.0, 18.0), (82.0, 50.0), (32.0, 82.0)])]


@glyph("media-playback-pause")
def media_playback_pause(tint: str) -> ShapeList:
    """Two bars."""
    return [
        Rect(tint, 30.0, 18.0, 15.0, 64.0, 3.0),
        Rect(tint, 55.0, 18.0, 15.0, 64.0, 3.0),
    ]


@glyph("media-playback-stop")
def media_playback_stop(tint: str) -> ShapeList:
    """A filled square."""
    return [Rect(tint, 26.0, 26.0, 48.0, 48.0, 4.0)]


@glyph("media-skip-forward")
def media_skip_forward(tint: str) -> ShapeList:
    """A triangle running into a bar, for the end of a track."""
    return [
        Poly.of(tint, [(22.0, 22.0), (58.0, 50.0), (22.0, 78.0)]),
        Rect(tint, 64.0, 22.0, 14.0, 56.0, 3.0),
    ]


@glyph("media-skip-backward")
def media_skip_backward(tint: str) -> ShapeList:
    """The same the other way round, for the start of a track."""
    return [
        Poly.of(tint, [(78.0, 22.0), (42.0, 50.0), (78.0, 78.0)]),
        Rect(tint, 22.0, 22.0, 14.0, 56.0, 3.0),
    ]


@glyph("star")
def star(tint: str) -> ShapeList:
    """A five pointed star, for ratings and for marking a favourite."""
    return [star_shape(tint, 50.0, 50.0, 42.0, 18.0)]


@glyph("applications-system")
def applications_system(tint: str) -> ShapeList:
    """A cog, for the settings and system category."""
    return gear(tint, 50.0, 50.0, 26.0, 8, 15.0, 12.0)


@glyph("applications-network")
def applications_network(tint: str) -> ShapeList:
    """A globe with an equator and a meridian, for the network category."""
    return [
        *ring(tint, 50.0, 50.0, 32.0, 7.0),
        Line(tint, 19.0, 40.0, 81.0, 40.0, 5.0),
        Line(tint, 19.0, 60.0, 81.0, 60.0, 5.0),
        Line(tint, 50.0, 18.0, 50.0, 82.0, 5.0),
    ]


@glyph("preferences-system-windows")
def preferences_system_windows(tint: str) -> ShapeList:
    """Two overlapping windows, for window manager settings."""
    return [
        Rect(tint, 12.0, 30.0, 54.0, 48.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 12.0, 30.0, 54.0, 11.0, 6.0),
        Rect(tint, 40.0, 18.0, 48.0, 44.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 40.0, 18.0, 48.0, 11.0, 6.0),
    ]
