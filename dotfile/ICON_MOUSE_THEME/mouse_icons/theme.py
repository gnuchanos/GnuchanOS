"""Building the theme directory: cursors, their names, and the two index files.

A cursor theme is a directory with a ``cursors`` subdirectory in it, one file
per name a program might ask for, and a small index naming the theme. That is
the whole format. The interesting part is the names: about two hundred of them
answer to about thirty drawings, so the theme is built by drawing each state
once and then answering the other names with a link to it.

A link and not a copy. Twelve frames at three sizes is about fifteen kilobytes
per name, and the table has a hundred and twenty names; copying would make the
theme two megabytes of the same cursor stored under different spellings. A
symbolic link costs an inode. Where a link cannot be made - a filesystem that
refuses them, or a copy being taken by something that does not follow links - the
bytes are written instead, because a theme that half exists is worse than one
that is slightly larger.

Every name in :mod:`names` is written, and :func:`missing_names` reports the ones
that are not, so a theme that will fall back to the black default for ``help``
says so before it is installed rather than after.
"""

from __future__ import annotations

import os
import shutil
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import build, names, palette
from .busy import BUSY_STATES
from .dialogs import DIALOG_STATES
from .states import STATES, State
from .xcursor import write_cursor

#: The name the cursor theme is installed under. It has to differ from the icon
#: theme's: both are read from `~/.local/share/icons/<name>` and `~/.icons/<name>`,
#: so two themes with one name are one directory - and this installer replaces
#: that directory outright, which is how installing the cursor theme deleted the
#: icon theme's files.
THEME_NAME = "GnuChanMouseIcons"

#: What the theme inherits from when a program asks for a name it does not have.
#: Adwaita is on every GNOME and GTK system and is the one theme that is always
#: installed, so inheriting from it is inheriting from something.
INHERITS = "Adwaita"

COMMENT = "Alien violet hardware: lit armour, hex rings and cold energy lines"

#: How many states are drawn at once. Every state is independent of every other
#: one - a pointer and a caret share no state - so the drawing is handed to a
#: pool sized to the machine: one worker per core, and four on a machine that
#: will not say how many it has.
WORKERS = os.cpu_count() or 4


def all_states() -> dict[str, State]:
    """Every drawing, under the name :mod:`names` refers to it by."""
    combined: dict[str, State] = {}
    combined.update(STATES)
    combined.update(BUSY_STATES)
    combined.update(DIALOG_STATES)
    return combined


def names_by_state() -> dict[str, list[str]]:
    """The cursor names grouped by the state that answers them, sorted.

    Sorted so the run is reproducible: which spelling ends up as the file the
    others link to would otherwise depend on the order the table was written in,
    and a theme that changes its inode layout between two runs of the same
    script is a theme whose diff is noise.
    """
    grouped: dict[str, list[str]] = {}
    for name, state in names.CURSOR_NAMES.items():
        grouped.setdefault(state, []).append(name)
    return {state: sorted(spellings) for state, spellings in grouped.items()}


def missing_names(states: dict[str, State]) -> list[str]:
    """Required names with no drawing, and drawings with no required name."""
    problems = list(names.unshipped())
    for state in sorted(names_by_state()):
        if state not in states:
            problems.append(f"{state}: no drawing for this state")
    return problems


def _link_or_copy(target: Path, source: Path) -> bool:
    """Make ``target`` the same cursor as ``source``. True when it is a link."""
    if target.exists() or target.is_symlink():
        target.unlink()
    try:
        # A relative link, so the theme can be moved or copied into a home
        # directory and still work. An absolute one would break the moment the
        # theme is installed somewhere else than it was built.
        os.symlink(source.name, target)
        return True
    except OSError:
        shutil.copy2(source, target)
        return False


def _write_index(root: Path) -> None:
    """The two files that name the theme.

    Both are written because different programs read different ones: GTK reads
    ``index.theme``, the X cursor library reads ``cursor.theme``, and a theme
    that has only one of them is a theme that is invisible to half the desktop.

    ``Inherits`` names Adwaita and NOT ``default``. That is not a detail: the
    ``default`` theme (written by :mod:`install`) inherits from THIS theme, so a
    theme that inherits back from ``default`` is a cycle - ``default`` ->
    ``GnuChanMouseIcons`` -> ``default`` -> ... The X cursor loader in
    libxcb-cursor does not detect the cycle and recurses until the stack
    overflows, which is a segmentation fault inside ``strstr`` while it reads
    the theme's ``index.theme`` on the way round. It was measured: taking a page
    that makes the toolkit resolve a cursor - YouTube - crashed the whole
    program with SIGSEGV, and removing the ``,default`` below stopped it. Adwaita
    is on every GTK system and is the real fallback, so nothing is lost by
    dropping the pointer to ``default``.
    """
    body = "\n".join(
        [
            "[Icon Theme]",
            f"Name={THEME_NAME}",
            f"Comment={COMMENT}",
            f"Inherits={INHERITS}",
            "Example=default",
            "",
        ]
    )
    for filename in ("index.theme", "cursor.theme"):
        (root / filename).write_text(body, encoding="utf-8")


def is_our_theme(root: Path) -> bool:
    """Whether ``root`` holds this cursor theme, and is safe to replace.

    Two cursor themes cannot share a name, but a name is not the only way to
    end up pointing at a directory that belongs to something else: a theme
    installed by hand under the same name, a link, or a mistake in the names
    above. The check is what stands between a wrong path and an unrecoverable
    ``rm -rf``, and it is not hypothetical - installing this theme with the name
    the icon theme used deleted the icon theme's files, because the replacement
    below was unconditional.
    """
    if (root / "cursors").is_dir():
        return True
    index = root / "index.theme"
    if index.is_file():
        try:
            return f"Name={THEME_NAME}" in index.read_text(encoding="utf-8")
        except OSError:
            return False
    return False


def write_theme(root: Path, report=None) -> dict[str, int]:
    """Write the whole theme under ``root``, returning what was written.

    The directory is emptied first: a name removed from :mod:`names` would
    otherwise stay behind from an earlier run, and a stale cursor is one that is
    still served to whichever program asks for it. It is emptied only when it is
    this theme's - see :func:`is_our_theme` - because the alternative is a
    script that deletes whatever directory it was pointed at.
    """
    states = all_states()
    cursors = root / "cursors"
    if root.exists():
        if not is_our_theme(root):
            raise SystemExit(
                f"error: {root} exists and is not the {THEME_NAME} cursor theme; "
                "refusing to replace it"
            )
        shutil.rmtree(root)
    cursors.mkdir(parents=True)

    grouped = names_by_state()

    def render_one(state):
        """Every frame of one state, at every size, in one list."""
        images = []
        for size in palette.SIZES:
            images.extend(build.render_state(state, size))
        return images

    # The drawing is the slow half and every state's is independent of every
    # other's, so they are drawn in parallel and written afterwards, in the
    # same sorted order the sequential version wrote them in. Writing on this
    # thread is deliberate: the aliases are links to the file just written, so
    # the primary has to be on disk before its aliases are made.
    with ThreadPoolExecutor(max_workers=WORKERS) as pool:
        futures = {
            state_name: pool.submit(render_one, states[state_name])
            for state_name in sorted(grouped)
            if state_name in states
        }
        rendered = {name: future.result() for name, future in futures.items()}

    stats = {"states": 0, "names": 0, "links": 0, "frames": 0}
    for state_name, spellings in sorted(grouped.items()):
        if state_name not in states:
            continue
        primary = cursors / spellings[0]
        images = rendered[state_name]
        write_cursor(str(primary), images)
        stats["states"] += 1
        stats["names"] += 1
        stats["frames"] += len(images)
        for alias in spellings[1:]:
            if _link_or_copy(cursors / alias, primary):
                stats["links"] += 1
            stats["names"] += 1
        if report is not None:
            report(f"    {state_name}: {len(spellings)} name(s), "
                   f"{len(images)} frames")

    _write_index(root)
    return stats
