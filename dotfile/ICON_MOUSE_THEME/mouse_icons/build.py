"""Turning a state into the frames the X server reads.

Two functions, because there are two decisions in it: how many frames a cursor
gets, and where its hotspot is.

The frames are the state's own count. A still cursor carries one frame and an
animation carries as many as it needs, so the theme is only as large as it has
to be - where the set this replaces wrote twelve identical frames of every arrow
and every bar.

The hotspot is the state's, not the image's. It has to be: the hotspot is where
the click lands, and the arrow is not centred on its own tip. A theme that puts
the hotspot in the middle of the image for every state clicks six pixels away
from the point of a twenty four pixel arrow, in every corner of every window on
the screen.
"""

from __future__ import annotations

from . import palette
from .cursor import Geometry
from .states import State
from .xcursor import CursorImage


def render_state(state: State, size: int) -> list[CursorImage]:
    """Every frame of ``state`` at ``size``, ready to be written."""
    geometry = Geometry(size)
    xhot, yhot = state.hotspot_pixels(size)
    return [
        CursorImage(
            size,
            size,
            xhot,
            yhot,
            palette.FRAME_DELAY_MS,
            state.render(geometry, frame).pixels(),
        )
        for frame in range(state.frames)
    ]


def render_state_still(state: State, size: int) -> list[CursorImage]:
    """One frame of ``state`` at ``size``, for the places animation is wrong.

    A cursor shown inside another window - the pointer used as a drag image, or
    the one a screenshot tool draws - is shown as a still, and a still taken from
    a rotation is whichever angle the rotation happened to be at. This takes the
    first frame, and carries no delay, which is what tells the X server the
    cursor does not move.
    """
    geometry = Geometry(size)
    xhot, yhot = state.hotspot_pixels(size)
    return [
        CursorImage(
            size,
            size,
            xhot,
            yhot,
            0,
            state.render(geometry, 0).pixels(),
        )
    ]
