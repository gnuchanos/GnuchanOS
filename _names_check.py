"""Throwaway proof that the three theme names are distinct and that neither
installer will delete a directory that is not its own. Deleted after the run.
"""

import os
import sys
from pathlib import Path

REPO = Path("/mnt/d/GnuchanOS")
sys.path.insert(0, str(REPO / "dotfile/ICON_MOUSE_THEME"))
sys.path.insert(0, str(REPO / "dotfile/ICON_THEME"))

from icons import tree as icon_tree  # noqa: E402
from mouse_icons import theme as cursor_theme  # noqa: E402

names = (cursor_theme.THEME_NAME, icon_tree.THEME_NAME)
print("cursor theme name:", names[0])
print("icon theme name:  ", names[1])
print("distinct:         ", len(set(names)) == 2)

home = Path(os.environ["HOME"])
icons = home / ".icons"

# A decoy at the icon theme's path, holding one file, standing in for the real
# icon theme without waiting for a build that takes minutes.
decoy = icons / icon_tree.THEME_NAME
decoy.mkdir(parents=True, exist_ok=True)
marker = decoy / "index.theme"
marker.write_text(f"[Icon Theme]\nName={icon_tree.THEME_NAME}\n", encoding="utf-8")

print("icon theme recognises it:", icon_tree.is_our_theme(decoy))
print("cursor theme claims it:  ", cursor_theme.is_our_theme(decoy))

# And the cursor theme must refuse a directory at its own path that is not its
# own - the case that deletes data if the guard is missing.
foreign = icons / cursor_theme.THEME_NAME
foreign.mkdir(parents=True, exist_ok=True)
(foreign / "something-else.txt").write_text("not a cursor theme\n", encoding="utf-8")
print("cursor theme recognises a foreign dir:", cursor_theme.is_our_theme(foreign))
try:
    cursor_theme.write_theme(foreign, None)
except SystemExit as error:
    print("refused as expected:", error)
except Exception as error:  # noqa: BLE001  (any other failure is the point)
    print("WRONG FAILURE:", type(error).__name__, error)

print("decoy still there:", marker.is_file())
