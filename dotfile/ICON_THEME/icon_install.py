#!/usr/bin/env python3
"""Build the GnuchanPurple icon theme and install it, with no arguments.

    python3 icon_install.py

Run it and the theme is written into the user's icon directories and selected in
the session, so it is offered by name in the appearance dialog (lxappearance, or
whatever the desktop uses) immediately afterwards. Running it again rebuilds and
replaces what is there, so a run after editing the artwork is a run that shows
the edit.

There are deliberately no options: the artwork, the names and the destination
are all decisions the library makes, and a flag that changed one of them would
be a way to install a theme that is not the one in the source.

The work is in the ``icons`` package next to this file, one module per concern:
``glyphs_*`` draw, ``catalogue*`` name, ``tree`` writes and ``install`` places
the result. This file only calls it.
"""

from __future__ import annotations

import sys
from pathlib import Path

# The package sits next to this script, which is what lets the script be run from
# anywhere — by its path, from another directory, or by a desktop file — without
# the current directory having to be the theme's.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from icons.install import main  # noqa: E402  (the path has to be set first)


if __name__ == "__main__":
    raise SystemExit(main(print))
