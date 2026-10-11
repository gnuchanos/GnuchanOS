#!/usr/bin/env python3
"""Install the GnuchanPurple cursor theme, with no arguments.

    python3 settings_mouse_icon.py

Run it and the theme is drawn, written into the user's icon directories and
selected in the session, so the pointer is the new one immediately and every
program that starts afterwards picks it up. Running it again rebuilds and
replaces what is there, so a run after editing the artwork is a run that shows
the edit.

There are deliberately no options: the shapes, the colours, the sizes, the names
and the destinations are all decisions the library makes, and a flag that changed
one of them would be a way to install a theme that is not the one in the source.

The cursors are the same object in every state - a round dot of light with a
halo, drawn at 24, 32 and 48 pixels - and they are drawn rather than committed
as files, which is what makes the whole set one palette in one place:

    the pointer      the dot, pulsing
    text             the dot, opened out, with a caret over it
    hand, move       the dot with marks around it, so a click is still a click
    wait, progress   the dot with a ring that turns while a program works
    forbidden, help  the dot with a badge, because a symbol needs somewhere to go

On a machine that has LXDE, the cursor name is also written to the files that
desktop reads it from: LXSession's own desktop.conf, GTK 2's gtkrc - because the
panel is a GTK 2 program on the versions people still run - and an autostart
line that loads the X resource database, which nothing on LXDE does by default.
That last one is what makes the window manager and the desktop follow, and not
only the programs that read a GTK setting.

The theme is also copied into a system icon directory, and the machine's default
cursor theme is pointed at it. That is what the display manager's greeter reads
before anyone has logged in, and what a program falls back to when it has no
cursor setting of its own - the reason a cursor theme can be selected in
lxappearance and still be the black arrow on the login screen and over the
desktop. It is the only step that needs root: the script asks for it with sudo
for the commands that need it, rather than re-running itself as root and leaving
root-owned files in a user's home directory.

Everything it writes is described in the package next to this file, one module
per concern. This file only calls it.

License: GPL3
"""

from __future__ import annotations

import sys
from pathlib import Path

# The package sits next to this script, which is what lets the script be run from
# anywhere - by its path, from another directory, or by a desktop file - without
# the current directory having to be the theme's.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from mouse_icons.apply import main  # noqa: E402  (the path has to be set first)


def report(message: str) -> None:
    """Print one line and flush it immediately.

    The build spends most of a second inside a single state's frames, and Python
    block-buffers its output when it is not writing to a terminal. Without the
    flush, a run piped to a file or watched from a logging pane would show
    nothing at all until it had finished - which is the one thing the progress
    lines exist to prevent.
    """
    print(message, flush=True)


if __name__ == "__main__":
    raise SystemExit(main(report))
