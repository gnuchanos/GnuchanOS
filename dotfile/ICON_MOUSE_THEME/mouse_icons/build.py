"""Turning a state into the frames the X server reads.

One function, because there is one decision in it: how many frames a cursor
gets. The state already knows - the spinning ones ask for twice as many as the
rest - so this only has to draw them and wrap each one in the header the format
needs.

The hotspot is the middle of the dot for every state. It has to be: the hotspot
is where the click lands, and a cursor whose badge hangs below and to the right
of the hotspot without the hotspot moving is a cursor that clicks where it
points. Moving it to the centre of the bounding box instead would put the click
half a badge away from the dot, which at twenty four pixels is several pixels of
error in every window on the screen.
"""

from __future__ import annotations

from . import palette
from .cursor import Geometry
from .states import State
from .xcursor import CursorImage


def render_state(state: State, size: int) -> list[CursorImage]:
    """Every frame of ``state`` at ``size``, ready to be written."""
    geometry = Geometry(size)
    hotspot = geometry.hotspot
    return [
        CursorImage(
            size,
            size,
            hotspot,
            hotspot,
            palette.FRAME_DELAY_MS,
            state.render(geometry, frame).pixels(),
        )
        for frame in range(state.frames)
    ]


def render_state_still(state: State, size: int) -> list[CursorImage]:
    """One frame of ``state`` at ``size``, for the places animation is wrong.

    A cursor shown inside another window - the pointer used as a drag image, or
    the one a screenshot tool draws - is shown as a still, and a still taken from
    a pulse is whichever brightness the pulse happened to be at. This takes the
    first frame, which is the dimmest, so a screenshot of a cursor is at least
    the same every time.
    """
    geometry = Geometry(size)
    hotspot = geometry.hotspot
    return [
        CursorImage(
            size,
            size,
            hotspot,
            hotspot,
            0,
            state.render(geometry, 0).pixels(),
        )
    ]
