"""Writing the icon theme to disk: directories, index.theme and the files.

The layout follows the Icon Theme Specification: one directory per size and
context, fixed sizes carrying PNGs and the large sizes carrying SVG, plus a
scalable ``symbolic`` directory per context for the monochrome names GTK asks
for when it draws a menu or a toolbar.

Three arrangements are written at once, because three toolkits look in three
different places and none of them falls back to the others:

* ``<size>x<size>/<Context>`` holds the PNGs, which is what GTK 2 and every
  raster-only lookup walks.
* ``<size>x<size>/<Context>/scalable`` holds the SVGs, which is the older
  way of naming a scalable directory.
* ``scalable/<Context>`` holds one SVG per name, which is where GTK 4 and Qt 6
  look first.

Rendering is cached by glyph, tint and size. That is not a micro-optimisation:
forty icon names share the folder artwork and eighty share the generic text
page, so without the cache the theme would rasterise the same picture hundreds
of times. With it, the number of pixels drawn is the number of distinct glyphs.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Callable

# The catalogue modules are imported in this order on purpose, and the order is
# not cosmetic. The pair of context and name may be registered once: a second
# registration wants the same file and one drawing would silently replace the
# other, which is what ``catalogue.duplicate_names`` exists to catch. The tables
# that spell their entries with ``branch`` therefore come first and the tables
# that spell theirs with ``alias`` come after, so that a name two tables both
# want — ``caja`` is a MATE program and a file manager — is drawn by the general
# table and the specific one only adds what is still missing.
from . import (
    catalogue,
    catalogue_files,  # noqa: F401  (the general program and file-type table)
    catalogue_xfce,  # noqa: F401  (the XFCE vocabulary, also spelled with branch)
    catalogue_desktops,  # noqa: F401  (the desktop overlays, spelled with alias)
    catalogue_programs,  # noqa: F401  (the program overlays, spelled with alias)
    catalogue_extra,  # noqa: F401  (the empty contexts and the GTK stock ids)
    gif,
    palette,
    raster,
    svg,
)
from .glyphs_base import build, load_all
from .glyphs_ui import SPINNER_FRAMES, spinner_frame
from .progress import Progress

# Every glyph module is imported for its side effect: the ``@glyph`` decorator
# registers artwork on import, so a module that is never imported contributes
# nothing and does so silently. Loading them at import time — rather than inside
# the build — means the registry is already complete when the catalogue is
# validated against it, which happens before a single file is written.
load_all()

#: The name the icon theme is installed under, and the directory it is read
#: from. It is deliberately not the GTK theme's name or the cursor theme's:
#: an icon theme and a cursor theme are installed into the *same* directories -
#: `~/.local/share/icons/<name>` and `~/.icons/<name>` - so two of them sharing
#: a name are one directory, and installing either one replaces the other.
#: That is exactly what happened when the cursor theme was installed over this
#: one: its `rm -rf` took the icons with it.
THEME_NAME = "GnuChanIcon"
INHERITS = "Adwaita,hicolor"
COMMENT = "Purple icon theme"

# Fixed sizes are PNG so GTK 2 and older toolkits, which cannot read SVG, still
# find an icon; the large sizes are SVG so one file serves every HiDPI scale
# instead of a bitmap per factor. The ladder is the one the specification names
# for toolbars, panels, menus and launchers: a desktop that asks for 8 or for 96
# gets the size it asked for rather than the nearest of five.
PNG_SIZES: tuple[int, ...] = (8, 12, 16, 22, 24, 32, 36, 48)
SVG_SIZES: tuple[int, ...] = (64, 96, 128, 192, 256, 512)

#: The size a symbolic file is authored at. It is scalable, so this is the
#: nominal size the specification wants declared rather than a pixel count.
SYMBOLIC_SIZE = 16

#: The size the root ``scalable/<Context>`` directories declare.
ROOT_SCALABLE_SIZE = 256

#: How long one frame of the spinner is shown, in hundredths of a second.
SPINNER_DELAY = 4

CONTEXT_TINT_KEY: dict[str, str] = {
    catalogue.ACTIONS: "actions",
    catalogue.ANIMATIONS: "animations",
    catalogue.APPLICATIONS: "applications",
    catalogue.CATEGORIES: "categories",
    catalogue.DEVICES: "devices",
    catalogue.EMBLEMS: "emblems",
    catalogue.EMOTES: "emotes",
    catalogue.INTERNATIONAL: "international",
    catalogue.MIMETYPES: "mimetypes",
    catalogue.PLACES: "places",
    catalogue.STATUS: "status",
    catalogue.UI: "ui",
}


#: How many icons are rendered at once. Every icon is independent of every other
#: one, so the work is handed to a pool sized to the machine: one worker per
#: core, and four on a machine that will not say how many it has.
WORKERS = os.cpu_count() or 4

#: The record of what the installed tree was built from, kept inside the theme.
#: It is read before anything is rendered and written after everything is: when
#: it matches, the tree is left exactly as it stands and a second install costs
#: one hash rather than the whole build. It matters most on the machine this
#: theme is meant for - an old laptop, where the build is minutes long.
STAMP_FILE = ".gnuchan-build-stamp"


def build_signature() -> str:
    """A hash of everything a generated tree depends on.

    The whole package's own source is hashed - the glyphs, the palette, the
    catalogue tables, this file's size ladders and helpers - because any of
    them can change a rendered pixel. It is deliberately coarse: a comment in
    an unrelated module also invalidates the cache, which costs one rebuild and
    can never serve a stale icon. The alternative is a hand-kept list of which
    changes matter, and that is the thing that goes out of step.
    """
    digest = hashlib.sha256()
    package_dir = Path(__file__).resolve().parent
    for path in sorted(package_dir.glob("*.py")):
        digest.update(path.name.encode("utf-8"))
        digest.update(path.read_bytes())
    digest.update(
        repr((PNG_SIZES, SVG_SIZES, SYMBOLIC_SIZE, ROOT_SCALABLE_SIZE)).encode("utf-8")
    )
    return digest.hexdigest()


def is_our_theme(root: Path) -> bool:
    """Whether ``root`` holds this icon theme, and is safe to replace.

    The build empties its target before writing, which is what keeps a renamed
    icon from lingering. That makes the target a directory this script deletes,
    so it checks first that the directory is this theme: an icon theme and a
    cursor theme are installed into the same ``icons`` directories, and with one
    name between them the second install deletes the first.
    """
    index = root / "index.theme"
    if not index.is_file():
        return False
    try:
        return f"Name={THEME_NAME}" in index.read_text(encoding="utf-8")
    except OSError:
        return False


def tint_for(context: str) -> str:
    """The colour a glyph is drawn in for a given context."""
    key = CONTEXT_TINT_KEY.get(context, "actions")
    return palette.CONTEXT_TINT.get(key, palette.DEFAULT_TINT)


def samples_for(size: int) -> int:
    """How many samples per axis an icon of ``size`` is rendered with.

    An eight pixel icon is the same artwork as a forty-eight pixel one, so the
    small sizes need the finer sampling more: at eight pixels a single sample
    per axis is the difference between a tick and a blob.
    """
    if size <= 16:
        return 4
    if size <= 32:
        return 3
    return 2


def png_directory(size: int, context: str) -> str:
    return f"{size}x{size}/{context}"


def svg_directory(size: int, context: str) -> str:
    return f"{size}x{size}/{context}/scalable"


def root_scalable_directory(context: str) -> str:
    return f"scalable/{context}"


def symbolic_directory(context: str) -> str:
    return f"symbolic/{context}"


def index_theme(contexts: set[str]) -> str:
    """The ``index.theme`` that ties the directories together."""
    directories: list[str] = []
    lines: list[str] = [
        "[Icon Theme]",
        f"Name={THEME_NAME}",
        f"Comment={COMMENT}",
        f"Inherits={INHERITS}",
    ]
    for size in PNG_SIZES:
        for context in catalogue.CONTEXTS:
            if context not in contexts:
                continue
            directories.append(png_directory(size, context))
            lines += [
                "",
                f"[{png_directory(size, context)}]",
                f"Size={size}",
                "Type=Fixed",
                f"Context={context}",
            ]
    for size in SVG_SIZES:
        for context in catalogue.CONTEXTS:
            if context not in contexts:
                continue
            directories.append(svg_directory(size, context))
            lines += [
                "",
                f"[{svg_directory(size, context)}]",
                f"Size={size}",
                f"MinSize={max(size // 2, 16)}",
                f"MaxSize={size * 2}",
                "Type=Scalable",
                f"Context={context}",
            ]
    for context in catalogue.CONTEXTS:
        if context not in contexts:
            continue
        directories.append(root_scalable_directory(context))
        lines += [
            "",
            f"[{root_scalable_directory(context)}]",
            f"Size={ROOT_SCALABLE_SIZE}",
            "MinSize=16",
            "MaxSize=512",
            "Type=Scalable",
            f"Context={context}",
        ]
    # Only the contexts that carry a symbolic variant get a symbolic directory.
    # Declaring one that holds no files is a lookup that walks into nothing, and
    # the specification's point is that the two agree: every directory listed is
    # a directory written.
    for context in catalogue.CONTEXTS:
        if context not in contexts or context not in catalogue.SYMBOLIC_CONTEXTS:
            continue
        directories.append(symbolic_directory(context))
        lines += [
            "",
            f"[{symbolic_directory(context)}]",
            f"Size={SYMBOLIC_SIZE}",
            "MinSize=8",
            "MaxSize=512",
            "Type=Scalable",
            f"Context={context}",
        ]
    lines.insert(4, "Directories=" + ",".join(directories))
    return "\n".join(lines) + "\n"


class Builder:
    """Renders the catalogue into a directory tree."""

    def __init__(self, root: Path, log: Callable[[str], None] | None = None) -> None:
        self.root = Path(root)
        self._log = log or (lambda message: None)
        self.png_cache: dict[tuple[str, str, int], bytes] = {}
        self.svg_cache: dict[tuple[str, str, int], str] = {}
        self.files_written = 0

    def shapes(self, glyph_name: str, tint: str) -> list[object]:
        return build(glyph_name, tint)

    def png_bytes(self, glyph_name: str, tint: str, size: int) -> bytes:
        key = (glyph_name, tint, size)
        cached = self.png_cache.get(key)
        if cached is None:
            shapes = self.shapes(glyph_name, tint)
            cached = raster.render(
                shapes, size, 100.0, samples=samples_for(size)
            ).to_png()
            self.png_cache[key] = cached
        return cached

    def svg_text(self, glyph_name: str, tint: str, size: int, title: str) -> str:
        key = (glyph_name, tint, size)
        cached = self.svg_cache.get(key)
        if cached is None:
            shapes = self.shapes(glyph_name, tint)
            cached = svg.document(shapes, size)
            self.svg_cache[key] = cached
        if title:
            return cached.replace("<svg", f"<!-- {title} -->\n<svg", 1)
        return cached

    def write(self, relative: str, data: bytes | str) -> None:
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(data, bytes):
            target.write_bytes(data)
        else:
            target.write_text(data, encoding="utf-8")
        self.files_written += 1

    def build(self) -> dict[str, int]:
        """Write every icon, the animation, the index and the theme metadata.

        The work is split into numbered stages and each long one reports a
        percentage, because the whole build runs for minutes and a silent script
        during that time is indistinguishable from a stuck one.
        """
        report = Progress(self._log, stages=6)

        report.stage("reading the catalogue")
        entries = catalogue.entries()
        symbolic = catalogue.symbolic_entries()
        contexts = {entry.context for entry in entries} | {
            entry.context for entry in symbolic
        }
        report.finish(
            f"{len(entries)} icons, {len(symbolic)} symbolic, {len(contexts)} contexts"
        )

        cached = self.cached_counts()
        if cached is not None:
            report.stage("checking the installed tree")
            report.finish("built from these sources already; nothing to redo")
            return cached

        report.stage("clearing the target directory")
        if self.root.exists():
            if not is_our_theme(self.root):
                raise SystemExit(
                    f"error: {self.root} exists and is not the {THEME_NAME} icon "
                    "theme; refusing to replace it"
                )
            shutil.rmtree(self.root)
        self.root.mkdir(parents=True, exist_ok=True)
        report.finish(str(self.root))

        counted = {"png": 0, "svg": 0, "symbolic": 0, "animated": 0}

        report.stage(f"drawing {len(entries)} icons")
        # The distinct pictures are drawn first, in parallel: forty names share
        # one folder and eighty share one text page, so what is actually
        # rendered is a few hundred glyphs and not a few thousand files. The
        # write loops below then find every one of them already in the cache.
        self.warm_caches(entries, symbolic)
        for index, entry in enumerate(entries, start=1):
            tint = tint_for(entry.context)
            for size in PNG_SIZES:
                self.write(
                    f"{png_directory(size, entry.context)}/{entry.name}.png",
                    self.png_bytes(entry.glyph, tint, size),
                )
                counted["png"] += 1
            for size in SVG_SIZES:
                self.write(
                    f"{svg_directory(size, entry.context)}/{entry.name}.svg",
                    self.svg_text(entry.glyph, tint, size, entry.name),
                )
                counted["svg"] += 1
            # The root scalable directory is what GTK 4 and Qt 6 look in first,
            # so one SVG per name is written there as well as in the size
            # ladder above. It is the same document, not a second drawing.
            self.write(
                f"{root_scalable_directory(entry.context)}/{entry.name}.svg",
                self.svg_text(entry.glyph, tint, ROOT_SCALABLE_SIZE, entry.name),
            )
            counted["svg"] += 1
            report.tick(
                index, len(entries), f"{counted['png']} png, {counted['svg']} svg"
            )
        report.finish(f"{counted['png']} PNG and {counted['svg']} SVG")

        report.stage(f"drawing {len(symbolic)} symbolic icons")
        for index, entry in enumerate(symbolic, start=1):
            self.write(
                f"{symbolic_directory(entry.context)}/{entry.name}.svg",
                self.svg_text(entry.glyph, palette.SYMBOLIC, SYMBOLIC_SIZE, entry.name),
            )
            counted["symbolic"] += 1
            report.tick(index, len(symbolic))
        report.finish(f"{counted['symbolic']} symbolic SVG")

        report.stage("drawing the animated spinner")
        counted["animated"] = self.write_spinner()
        report.finish(f"{counted['animated']} animations, {SPINNER_FRAMES} frames each")

        report.stage("writing index.theme and README.md")
        self.write("index.theme", index_theme(contexts))
        self.write("README.md", _readme(entries, symbolic, counted["animated"]))
        counted["glyphs"] = len(self.png_cache)
        report.finish(f"index.theme lists {len(contexts)} contexts")

        self.write_stamp(counted)
        return counted

    # --- the build cache -----------------------------------------------------

    def stamp_path(self) -> Path:
        """Where the record of what this tree was built from lives."""
        return self.root / STAMP_FILE

    def cached_counts(self) -> dict[str, int] | None:
        """The counts of an existing tree that is still current, or None.

        The tree is left where it is only when two things hold: it is really
        this theme - so an unrelated directory is never mistaken for a cache -
        and the sources it was built from have not changed since. Any edit to
        the artwork, the palette, the catalogue or the size ladder changes the
        signature, so a source change always rebuilds.
        """
        if not is_our_theme(self.root):
            return None
        try:
            data = json.loads(self.stamp_path().read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None
        if not isinstance(data, dict):
            return None
        if data.get("signature") != build_signature():
            return None
        counts = data.get("counts")
        if not isinstance(counts, dict):
            return None
        result: dict[str, int] = {}
        for key in ("png", "svg", "symbolic", "animated", "glyphs"):
            value = counts.get(key)
            if not isinstance(value, int):
                return None
            result[key] = value
        return result

    def write_stamp(self, counted: dict[str, int]) -> None:
        """Record what was built, so the next run can skip the whole build."""
        payload = {"signature": build_signature(), "counts": counted}
        self.stamp_path().write_text(
            json.dumps(payload, indent=2) + "\n", encoding="utf-8"
        )

    # --- rendering -----------------------------------------------------------

    def render_png(self, glyph_name: str, tint: str, size: int) -> bytes:
        """One icon as PNG bytes, at one size."""
        return raster.render(
            self.shapes(glyph_name, tint), size, 100.0, samples=samples_for(size)
        ).to_png()

    def render_svg(self, glyph_name: str, tint: str, size: int) -> str:
        """One icon as an SVG document, at one size."""
        return svg.document(self.shapes(glyph_name, tint), size)

    def warm_caches(self, entries, symbolic) -> None:
        """Render every distinct picture once, in parallel.

        The keys are collected before anything is drawn, so each picture is
        rendered exactly once and the pool is given work and not duplicates.
        Results are put into the caches on this thread, so the dictionaries are
        only ever written from one place.
        """
        png_jobs: set[tuple[str, str, int]] = set()
        svg_jobs: set[tuple[str, str, int]] = set()
        for entry in entries:
            tint = tint_for(entry.context)
            for size in PNG_SIZES:
                png_jobs.add((entry.glyph, tint, size))
            for size in SVG_SIZES:
                svg_jobs.add((entry.glyph, tint, size))
            svg_jobs.add((entry.glyph, tint, ROOT_SCALABLE_SIZE))
        for entry in symbolic:
            svg_jobs.add((entry.glyph, palette.SYMBOLIC, SYMBOLIC_SIZE))

        with ThreadPoolExecutor(max_workers=WORKERS) as pool:
            png_results = [
                (key, pool.submit(self.render_png, *key)) for key in png_jobs
            ]
            svg_results = [
                (key, pool.submit(self.render_svg, *key)) for key in svg_jobs
            ]
            for key, future in png_results:
                self.png_cache[key] = future.result()
            for key, future in svg_results:
                self.svg_cache[key] = future.result()

    def write_spinner(self) -> int:
        """Write the animated ``process-working`` GIF into every PNG size.

        The frames come from :mod:`icons.glyphs_ui`, which is also what the
        ``process-working-01`` .. ``process-working-36`` names draw, so the
        animation and its still frames cannot drift apart.
        """
        tint = tint_for(catalogue.ANIMATIONS)
        written = 0
        for size in PNG_SIZES:
            frames = [
                raster.render(
                    list(spinner_frame(step, tint)), size, 100.0, samples=samples_for(size)
                )
                for step in range(SPINNER_FRAMES)
            ]
            self.write(
                f"{png_directory(size, catalogue.ANIMATIONS)}/process-working.gif",
                gif.encode(frames, delay=SPINNER_DELAY),
            )
            written += 1
        return written


def _readme(
    entries: list[catalogue.IconEntry],
    symbolic: list[catalogue.IconEntry],
    animated: int,
) -> str:
    """A short file that explains what was generated and how to install it."""
    return (
        f"# {THEME_NAME}\n\n"
        "Generated by `dotfile/ICON_THEME/icon_install.py`. Edit the tables in\n"
        "`dotfile/ICON_THEME/icons/` and rebuild rather than editing these files:\n"
        "the whole tree is written from the palette and the glyphs each time.\n\n"
        f"- {len(entries)} named icons, each at {len(PNG_SIZES)} PNG sizes "
        f"({', '.join(str(size) for size in PNG_SIZES)}) and {len(SVG_SIZES)} "
        f"SVG sizes ({', '.join(str(size) for size in SVG_SIZES)})\n"
        f"- {len(entries)} scalable SVGs under one root `scalable/<Context>` "
        "directory, which is where GTK 4 and Qt 6 look first\n"
        f"- {len(symbolic)} `-symbolic` names, as scalable SVG\n"
        f"- {animated} animated `process-working` spinners, at "
        f"{', '.join(str(size) for size in PNG_SIZES)} px\n"
        f"- inherited from {INHERITS}, so anything not defined here still resolves\n"
    )
