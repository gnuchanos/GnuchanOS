"""The GnuchanPurple cursor theme, drawn rather than shipped as artwork.

The package is one module per concern, the same shape the icon theme next to it
uses:

    palette       the colours and the proportions everything is drawn from
    canvas        the pixel buffer and the blending rule
    draw          the soft shapes: glow, disc, ring, arc
    marks         the strokes: lines, chevrons, arrows
    cursor        the dot and its pulse, shared by every state
    states        the navigation states: pointer, caret, crosshair, resizing
    busy          the states that spin: wait, progress, busy
    dialogs       the states with a symbol: forbidden, help, zoom, copy
    names         every cursor name a desktop asks for, mapped to a state
    build         one state, at one size, as the frames the server reads
    xcursor       the Xcursor file format, written directly
    theme         the theme directory: cursors, links and the index files
    verify        reading the result back and checking it
    install       where things go, and the files that are the user's
    select        telling GTK, the settings daemon and the X server
    profiles      the LXDE files: LXSession, GTK 2, and the autostart hook
    system        the theme outside the home directory, and the machine's
                  default - what makes the display manager's greeter, the
                  window manager and the desktop use it too
    sheet         every cursor drawn into one PNG, to be looked at
    apply         the order the steps go in

The point of drawing the cursors instead of committing a directory of PNGs is
that there is nothing to keep in step: change the accent colour or the dot's
radius in ``palette`` and every cursor at every size is rebuilt from it. What
would otherwise be a few hundred files is a few hundred lines.
"""

from __future__ import annotations

from .apply import check, install, main, uninstall
from .theme import THEME_NAME

__all__ = ["THEME_NAME", "check", "install", "main", "uninstall"]
