"""Every cursor name a desktop asks for, and which drawing answers it.

There is no single list. The X11 core names, the freedesktop name that grew over
them, and the names each toolkit added of its own accord together come to a few
hundred spellings of about twenty ideas, and a theme that ships only the modern
names is a theme that leaves the older programs with a black-and-white default
cursor - which is the state this file exists to avoid.

The right-hand side is a state name from one of the three registries. A name
that is not here is not shipped, and the installer reports which ones those are
rather than writing an empty file: a theme with a zero byte cursor in it makes
the X server fall back for that one shape, which is worse than not shipping it.
"""

from __future__ import annotations

#: The names a cursor theme is expected to have, at least. Written out rather
#: than counted, because the installer checks against it and a missing one is a
#: cursor that falls back to the black default in some program.
REQUIRED: tuple[str, ...] = (
    "default",
    "left_ptr",
    "pointer",
    "text",
    "xterm",
    "cross",
    "crosshair",
    "hand1",
    "hand2",
    "watch",
    "wait",
    "progress",
    "left_ptr_watch",
    "move",
    "fleur",
    "not-allowed",
    "help",
    "zoom-in",
    "zoom-out",
    "sb_h_double_arrow",
    "sb_v_double_arrow",
    "size_fdiag",
    "size_bdiag",
    "top_left_corner",
    "top_right_corner",
    "bottom_left_corner",
    "bottom_right_corner",
    "openhand",
    "closedhand",
    "all-scroll",
    "copy",
    "alias",
)

#: name -> state. Grouped by the state, because that is how it is read.
CURSOR_NAMES: dict[str, str] = {}


def _assign(state: str, *names: str) -> None:
    for name in names:
        CURSOR_NAMES[name] = state


# The pointer. Every spelling of "the ordinary arrow" ends up on the dot, which
# is the whole idea of this set: the arrow is not a different cursor, it is the
# dot with nothing added.
_assign(
    "pointer",
    "left_ptr",
    "default",
    "arrow",
    "top_left_arrow",
    "left-arrow",
    "wayland-cursor",
    "x-cursor",
    "draft_large",
    "draft_small",
    "right_ptr",
    "center_ptr",
    "ul_angle",
    "ur_angle",
    "ll_angle",
    "lr_angle",
)

# The precision pointer: a dot with a hairline cross, which is what the old
# `draft` family and the tiled window managers used.
_assign("precise", "precision", "dotbox", "dot_box_mask")

# The text caret, under every name it has ever had. The name `text` is in the
# list as well as being the name of the state: the two are the same word here,
# and leaving it out is how `text` - the one spelling every toolkit uses - ends
# up as the single name in a cursor theme that the X server cannot find.
_assign("text", "text", "xterm", "ibeam", "text_cursor", "vertical-text")

# The crosshair family, and the cell cursor that is a crosshair inside a box.
_assign("crosshair", "cross", "crosshair", "tcross", "color-picker", "diamond_cross")
_assign("cell", "cell", "plus")

# Moving and dragging. Same trap as `text` above: `move` is both the state and
# the name, and both have to be written out.
_assign("move", "move", "fleur", "size_all", "move_cursor", "dnd-move-move")
_assign("all-scroll", "all-scroll", "scroll_all")

# Resizing. The edges are named for where they are, the bars for the shape of
# the arrow, and the corners for the diagonal they sit on.
_assign(
    "resize-horizontal",
    "size_hor",
    "sb_h_double_arrow",
    "h_double_arrow",
    "ew-resize",
    "e-resize",
    "w-resize",
    "col-resize",
    "left_side",
    "right_side",
    "sb_left_arrow",
    "sb_right_arrow",
)
_assign(
    "resize-vertical",
    "size_ver",
    "sb_v_double_arrow",
    "v_double_arrow",
    "ns-resize",
    "n-resize",
    "s-resize",
    "row-resize",
    "top_side",
    "bottom_side",
    "sb_up_arrow",
    "sb_down_arrow",
)
_assign(
    "resize-diagonal-down",
    "size_fdiag",
    "nwse-resize",
    "se-resize",
    "nw-resize",
    "top_left_corner",
    "bottom_right_corner",
    "bd_double_arrow",
)
_assign(
    "resize-diagonal-up",
    "size_bdiag",
    "nesw-resize",
    "ne-resize",
    "sw-resize",
    "top_right_corner",
    "bottom_left_corner",
    "fd_double_arrow",
)

# Single direction arrows, which some programs use for their own scrolling.
_assign("arrow-up", "up-arrow", "sb_up_arrow_bare")
_assign("arrow-down", "down-arrow", "sb_down_arrow_bare")
_assign("arrow-left", "sb_left_arrow_bare")
_assign("arrow-right", "right-arrow")

# The hands. `pointer` in the X11 core names means the hand and not the arrow,
# which is a trap worth naming: `left_ptr` is the arrow.
_assign("open-hand", "openhand", "grab", "hand1", "pointer", "open-hand")
_assign("closed-hand", "closedhand", "grabbing", "hand2", "closed-hand", "dnd-none-move")

# The waiting states.
_assign("wait", "wait", "hourglass", "clock")
_assign("progress", "progress", "left_ptr_watch_progress")
_assign("busy", "busy", "not-responding", "dnd-ask-busy")
_assign("watch", "watch")
_assign("left-pointer-watch", "left_ptr_watch", "left-pointer-watch", "half-busy-left")
# `half-busy` has its own drawing and its own name, and the two were nearly left
# apart: the state existed, was rendered on every run and was never written,
# because no name pointed at it. A drawing with no name is a drawing that is
# built and then thrown away, and nothing reports it - which is why the check
# compares the two lists in both directions.
_assign("half-busy", "half-busy")

# Refusals and questions.
_assign("forbidden", "not-allowed", "forbidden", "no-drop", "crossed_circle",
        "X_cursor", "pirate", "circle")
_assign("help", "help", "question_arrow", "whats_this", "left_ptr_help")

# Zooming, copying and aliasing.
_assign("zoom-in", "zoom-in", "magnifier")
_assign("zoom-out", "zoom-out", "zoom-out-nothing")
_assign("copy", "copy", "dnd-copy-copy")
_assign("alias", "alias", "link", "dnd-link-alias")

# Drag and drop, which is where the badges earn their place: the pointer that
# carries a file has to say what releasing it will do.
_assign("dnd-move", "dnd-move")
_assign("dnd-copy", "dnd-copy")
_assign("dnd-link", "dnd-link")
_assign("dnd-none", "dnd-none")
_assign("dnd-ask", "dnd-ask")


def state_for(name: str) -> str | None:
    """The state that draws ``name``, or None when it is not in the table."""
    return CURSOR_NAMES.get(name)


def unshipped() -> list[str]:
    """The required names that are not in the table.

    Read by the installer's check. It is a list rather than a boolean so the
    report can name them: a theme missing three cursors is a theme someone can
    fix in a minute, and the names are the whole fix.
    """
    return [name for name in REQUIRED if name not in CURSOR_NAMES]
