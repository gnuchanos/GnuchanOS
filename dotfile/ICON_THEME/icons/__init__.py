"""The GnuchanPurple icon theme toolkit.

The package turns a list of glyphs into the two things an icon theme is made of:
PNG files for the fixed sizes and SVG files for the scalable ones, plus the
``index.theme`` that ties them together. Nothing outside the standard library is
used.

The pipeline is five steps, each of which is one module:

``shape``, ``primitives``
    an icon is a list of rectangles, discs, polygons, rings, lines and arcs
    authored in a 100 unit box. Each primitive knows both its outline and a
    signed distance function, which is what lets one geometry be rasterised and
    written as markup without the two drifting apart.

``raster``, ``svg``, ``gif``
    the same shape list as PNG bytes, as an SVG document, or — for the one
    animation in the theme — as an animated GIF.

``glyphs_base``
    the registry a glyph is registered in, and the constructions the artwork
    modules share — a page, a plate, an arrow, a chevron, a gear.

``glyphs_*``
    the artwork itself, one module per family: actions, applications, objects,
    places, status, hardware, mime types, letters, panels, emblems, emotes,
    flags, widget affordances and the few that have no family.
    :func:`glyphs_base.load_all` imports every one of them.

``catalogue``, ``catalogue_files``, ``catalogue_xfce``, ``catalogue_desktops``,
``catalogue_programs``, ``catalogue_extra``
    which icon names exist, which glyph draws each of them, and in which
    context. These are the tables that decide what the theme supports; the
    names are split by subject only so that no single table is unreadable.

``tree``
    writing the generated tree: directories, the spinner, ``index.theme`` and
    the files.

``install``
    placing the generated tree in the directories a desktop reads, selecting it
    in the session, and reporting where it went. ``icon_install.py`` next to the
    package is a three line caller of this module.

``sheet``
    contact sheets, for reviewing the set as a whole rather than one icon at a
    time.
"""

from __future__ import annotations

__all__ = [
    "catalogue",
    "catalogue_desktops",
    "catalogue_extra",
    "catalogue_files",
    "catalogue_programs",
    "catalogue_xfce",
    "gif",
    "install",
    "glyphs_actions",
    "glyphs_apps",
    "glyphs_base",
    "glyphs_emblems",
    "glyphs_emotes",
    "glyphs_extra",
    "glyphs_flags",
    "glyphs_hardware",
    "glyphs_letters",
    "glyphs_mime",
    "glyphs_objects",
    "glyphs_panels",
    "glyphs_places",
    "glyphs_status",
    "glyphs_ui",
    "palette",
    "primitives",
    "raster",
    "shape",
    "sheet",
    "svg",
    "tree",
]
