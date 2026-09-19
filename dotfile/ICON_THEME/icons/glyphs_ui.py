"""Widget affordances, and the spinner.

The ``UI`` context in the specification is for the names that belong to a widget
rather than to a document: the arrow on an expander, the grip on a draggable row,
the handles on a paned window, the spinner a toolkit shows while it waits. A
theme that only writes these into ``Actions`` is the reason a GTK 4 header bar
sometimes falls back to an untinted icon, so they are written twice, in both
contexts, and this module draws the ones that are new.

The spinner is here rather than in its own module because it is the same kind of
thing — a mark no document owns — and because the frames and the animated file
have to come from one place or the GIF and the SVGs drift apart. The animation
itself is written by :mod:`icons.tree`, which asks this module for the frames.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import GLYPHS, ShapeList, bars, check, chevron, frame, glyph, plus, ring
from .glyphs_letters import letter_shapes
from .glyphs_panels import panel_color
from .primitives import Arc, Disc, Line, Poly, Rect

#: How many frames a full turn of the spinner is divided into. Thirty-six is
#: ten degrees a frame, which is smooth enough for a busy indicator and few
#: enough that stepping through the static frames by hand still looks right.
SPINNER_FRAMES = 36

#: How much of the ring is bright in each frame.
SPINNER_ARC = 90.0


def spinner_frame(step: int, tint: str = palette.ACCENT) -> ShapeList:
    """One frame of the spinner: a faint ring with the bright quarter turned.

    The faint ring is the whole circle and the bright quarter is drawn over it,
    so a frame is complete on its own and the animation reads even if a toolkit
    shows only one frame.
    """
    degrees = 360.0 * step / SPINNER_FRAMES
    return [
        *ring(palette.BORDER_STRONG, 50.0, 50.0, 30.0, 8.0),
        Arc(tint, 50.0, 50.0, 30.0, degrees, SPINNER_ARC, 8.0),
    ]


def _register_spinner_frames() -> None:
    """Expose every frame as ``process-working-01`` .. ``process-working-36``.

    A toolkit that cannot animate a GIF can walk these instead, which is why
    they exist as names rather than only as bytes inside the animation.
    """

    def make(step: int):
        def render(tint: str) -> ShapeList:
            return spinner_frame(step, tint)

        return render

    for step in range(SPINNER_FRAMES):
        name = f"process-working-{step + 1:02d}"
        if name in GLYPHS:
            raise ValueError(f"glyph already defined: {name}")
        GLYPHS[name] = make(step)


_register_spinner_frames()


@glyph("pan-down")
def pan_down(tint: str) -> ShapeList:
    """A downward chevron: an expander, a dropdown, a combo box arrow."""
    return chevron(tint, 50.0, 50.0, 44.0, 10.0, 90.0)


@glyph("pan-up")
def pan_up(tint: str) -> ShapeList:
    """An upward chevron, for the same widgets pointing the other way."""
    return chevron(tint, 50.0, 50.0, 44.0, 10.0, 270.0)


@glyph("pan-start")
def pan_start(tint: str) -> ShapeList:
    """A left chevron, in left-to-right layouts."""
    return chevron(tint, 50.0, 50.0, 44.0, 10.0, 180.0)


@glyph("pan-end")
def pan_end(tint: str) -> ShapeList:
    """A right chevron, in left-to-right layouts."""
    return chevron(tint, 50.0, 50.0, 44.0, 10.0, 0.0)


@glyph("open-menu")
def open_menu(tint: str) -> ShapeList:
    """The menu button of a header bar: three bars, wider than the list mark.

    It is deliberately not ``bars``: that glyph is the list-style mark and sits
    beside text, while this one is a button and has to hold its own at the edge
    of a title bar.
    """
    return bars(tint, 50.0, 50.0, 68.0, 11.0, 3, 22.0)


@glyph("list-drag-handle")
def list_drag_handle(tint: str) -> ShapeList:
    """Six dots: the grip on a row that can be dragged, and on a splitter."""
    return [
        Disc(tint, 40.0 + column * 20.0, 36.0 + row * 14.0, 5.0)
        for row in range(3)
        for column in range(2)
    ]


def _sidebar(tint: str, side: str, reveal: bool) -> ShapeList:
    """A frame with one side filled, and an arrow showing which way it moves."""
    panel = Rect(tint, 16.0, 22.0, 24.0, 56.0, 3.0)
    if side == "right":
        panel = Rect(tint, 60.0, 22.0, 24.0, 56.0, 3.0)
    degrees = 0.0 if reveal else 180.0
    if side == "right":
        degrees = 180.0 if reveal else 0.0
    return frame(tint, 12.0, 18.0, 76.0, 64.0, 7.0) + [panel] + chevron(
        tint, 50.0, 50.0, 26.0, 8.0, degrees
    )


@glyph("sidebar-show")
def sidebar_show(tint: str) -> ShapeList:
    """A frame with its left side filled and an arrow pointing into it."""
    return _sidebar(tint, "left", True)


@glyph("sidebar-hide")
def sidebar_hide(tint: str) -> ShapeList:
    """The reverse of :func:`sidebar_show`: the arrow points out of the frame."""
    return _sidebar(tint, "left", False)


@glyph("sidebar-show-right")
def sidebar_show_right(tint: str) -> ShapeList:
    """A frame with its right side filled, for right-to-left layouts."""
    return _sidebar(tint, "right", True)


@glyph("sidebar-hide-right")
def sidebar_hide_right(tint: str) -> ShapeList:
    """The reverse of :func:`sidebar_show_right`."""
    return _sidebar(tint, "right", False)


@glyph("slider-handle")
def slider_handle(tint: str) -> ShapeList:
    """The knob of a slider, drawn on its own for toolkits that compose it."""
    return [Rect(tint, 40.0, 22.0, 20.0, 56.0, 9.0)]


@glyph("selection-mode")
def selection_mode(tint: str) -> ShapeList:
    """A tick in a ring: the button that switches a view into selection mode."""
    return ring(tint, 50.0, 50.0, 32.0, 7.0) + check(tint, 50.0, 50.0, 36.0, 9.0)


@glyph("font-selector")
def font_selector(tint: str) -> ShapeList:
    """The letter a with a dropdown arrow: the font chooser button."""
    return letter_shapes("a", tint, (10.0, 18.0, 56.0, 56.0), 8.0) + chevron(
        tint, 80.0, 50.0, 22.0, 7.0, 90.0
    )


@glyph("color-selector")
def color_selector(tint: str) -> ShapeList:
    """The colour droplet with a dropdown arrow: the colour chooser button."""
    return panel_color(tint) + chevron(tint, 78.0, 80.0, 20.0, 7.0, 90.0)


@glyph("window-new")
def window_new(tint: str) -> ShapeList:
    """A window with a plus in it: new window, and new document alongside it."""
    return [
        Rect(tint, 12.0, 18.0, 76.0, 64.0, 7.0),
        Rect(palette.BG_DARKEST + "88", 12.0, 18.0, 76.0, 14.0, 7.0),
    ] + plus(palette.FG_BRIGHT, 50.0, 60.0, 16.0, 8.0)


@glyph("tab-drag")
def tab_drag(tint: str) -> ShapeList:
    """A tab above a grip: the handle a tab strip is dragged or reordered by.

    It is deliberately not ``tab-new``: that is a list mark, and this is the
    grip a user pulls, so it carries the same dot pattern as the row handle
    below it and reads as the same gesture.
    """
    return [
        Rect(tint, 14.0, 14.0, 72.0, 20.0, 5.0),
        Rect(tint, 14.0, 40.0, 46.0, 14.0, 5.0),
    ] + [
        Disc(tint, 40.0 + column * 20.0, 70.0 + row * 14.0, 5.0)
        for row in range(2)
        for column in range(2)
    ]


def _lens(tint: str) -> ShapeList:
    """An eye: a lens with an iris and a pupil in it.

    Drawn from a stadium rather than from two arcs, because a stadium is the
    same shape at every size while two arcs have to be retuned whenever the
    thickness changes.
    """
    return [
        Rect(tint, 10.0, 36.0, 80.0, 28.0, 14.0),
        Disc(palette.BG_DARKEST + "AA", 50.0, 50.0, 11.0),
        Disc(palette.FG_BRIGHT, 50.0, 50.0, 5.0),
    ]


@glyph("view-reveal")
def view_reveal(tint: str) -> ShapeList:
    """An open eye: a password or a hidden field being shown."""
    return _lens(tint)


@glyph("view-conceal")
def view_conceal(tint: str) -> ShapeList:
    """The same eye struck through: the field is hidden again."""
    return _lens(tint) + [Line(palette.BG_DARKEST + "CC", 16.0, 80.0, 84.0, 20.0, 9.0)]


# --- the object and selection marks the specification names --------------------
# Four names in the UI context with no artwork behind them. The two group marks
# are the marquee idea: a frame with two things inside it, and the same frame
# with the things leaving it. The two selection marks are the text cursor with
# the direction it extends in, which is how every editor draws them.


def _grouped(tint: str, inside: bool) -> ShapeList:
    """A marquee, with two blocks either fenced in or leaving it."""
    block_left = (24.0, 34.0, 20.0, 32.0)
    block_right = (56.0, 34.0, 20.0, 32.0)
    shapes: ShapeList = []
    if not inside:
        shapes += [
            Rect(tint, *block_left, 3.0),
            Rect(tint, *block_right, 3.0),
        ]
    shapes += frame(tint, 10.0, 18.0, 80.0, 64.0, 6.0)
    if inside:
        shapes += [
            Rect(tint, *block_left, 3.0),
            Rect(tint, *block_right, 3.0),
        ]
    return shapes


@glyph("object-group")
def object_group(tint: str) -> ShapeList:
    """Two blocks inside a marquee: grouping the selected objects."""
    return _grouped(tint, True)


@glyph("object-ungroup")
def object_ungroup(tint: str) -> ShapeList:
    """The same blocks outside the marquee: breaking the group apart."""
    return _grouped(tint, False)


@glyph("selection-start")
def selection_start(tint: str) -> ShapeList:
    """The caret at the start of a selection, with its arrow pointing back."""
    return [
        Line(tint, 62.0, 12.0, 62.0, 88.0, 9.0),
        Poly.of(tint, [(62.0, 30.0), (62.0, 12.0), (48.0, 21.0)]),
    ]


@glyph("selection-end")
def selection_end(tint: str) -> ShapeList:
    """The caret at the end of a selection, with its arrow pointing on."""
    return [
        Line(tint, 38.0, 12.0, 38.0, 88.0, 9.0),
        Poly.of(tint, [(38.0, 30.0), (38.0, 12.0), (52.0, 21.0)]),
    ]
