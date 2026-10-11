#!/usr/bin/env python3
"""Write one PNG showing every cursor in the theme, for looking at.

    python3 preview_sheet.py [path]

Writes a contact sheet of all of the drawings - each state on four frames of its
pulse, on a dark half and a light half - so the set can be checked by looking at
it rather than by trusting the code that drew it. The default destination is
``cursor-sheet.png`` next to this file.

This is a development tool and not part of the install: the installer draws the
cursors into the icon directories and never needs a picture of them. It is here
because a cursor theme is the one kind of artwork that cannot be reviewed as a
file listing, and because the drawing code has to be looked at somehow on a
machine with no display.

License: GPL3
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mouse_icons.sheet import write_sheet  # noqa: E402  (path first)
from mouse_icons.theme import all_states  # noqa: E402


def main(argv: list[str]) -> int:
    destination = Path(argv[1]) if len(argv) > 1 else Path("cursor-sheet.png")
    states = all_states()
    width, height = write_sheet(destination, states)
    names = len(states)
    print(
        f"{names} drawing(s), 4 frames each, on two backgrounds: "
        f"{width}x{height} written to {destination}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
