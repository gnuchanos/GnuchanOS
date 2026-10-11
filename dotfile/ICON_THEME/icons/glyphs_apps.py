"""Application icons: category pictograms and the generic app tile.

This is the module that answers "does the theme support every program". It does
that the way the icon naming specification intends: programs that ship their own
icon keep it, and everything else is matched by name, by ``GenericName`` or by
``Categories`` against the pictograms below. A package manager on a machine the
theme has never seen asks for ``applications-system`` or ``system-software-
install`` and gets a themed gear or box, not a blank square.

Plates are used for the generic tile, because a tool with no logo needs *some*
recognisable shape, and a lettered plate is the convention every desktop
already understands. Category icons are drawn as pictograms instead, since they
are shown in menus where a letter would say nothing.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import GLYPHS, ShapeList, glyph, plate
from .glyphs_letters import LETTER_SEGMENTS, letter_shapes
from .glyphs_objects import _note
from .primitives import Arc, Disc, Line, Poly, Rect


def _with_plate(inner: ShapeList) -> ShapeList:
    return plate(palette.APP_PLATE, edge=palette.APP_PLATE_EDGE) + inner


TILE_BOX = (28.0, 26.0, 44.0, 46.0)


@glyph("app")
def app(tint: str) -> ShapeList:
    """The generic application tile: a plate with a window drawn on it."""
    return _with_plate(
        [
            Rect(palette.APP_GLYPH, 30.0, 34.0, 40.0, 32.0, 4.0),
            Rect(palette.APP_PLATE, 30.0, 34.0, 40.0, 8.0, 4.0),
            Disc(palette.APP_GLYPH, 36.0, 38.0, 2.0),
            Disc(palette.APP_PLATE, 42.0, 38.0, 2.0),
        ]
    )


def _register_tiles() -> None:
    """One plate glyph per character, for programs with no artwork at all."""

    def make(character: str):
        def render(tint: str) -> ShapeList:
            return _with_plate(letter_shapes(character, palette.APP_GLYPH, TILE_BOX, 7.0))

        return render

    for character in LETTER_SEGMENTS:
        prefix = "digit" if character.isdigit() else "letter"
        GLYPHS[f"app-{prefix}-{character}"] = make(character)


_register_tiles()


@glyph("applications-internet")
def applications_internet(tint: str) -> ShapeList:
    return [
        Arc(tint, 50.0, 50.0, 32.0, 0.0, 359.999, 7.0),
        Line(tint, 50.0, 18.0, 50.0, 82.0, 6.0),
        Line(tint, 18.0, 50.0, 82.0, 50.0, 6.0),
        Arc(tint, 50.0, 50.0, 17.0, 0.0, 359.999, 6.0),
    ]


@glyph("web-browser")
def web_browser(tint: str) -> ShapeList:
    return applications_internet(tint)


@glyph("applications-graphics")
def applications_graphics(tint: str) -> ShapeList:
    """A painter's palette with three paint wells and one brush."""
    dot = palette.BG_DARKEST
    return [
        Poly.of(
            tint,
            [
                (50.0, 14.0),
                (78.0, 26.0),
                (86.0, 52.0),
                (74.0, 78.0),
                (50.0, 86.0),
                (26.0, 78.0),
                (14.0, 52.0),
                (22.0, 26.0),
            ],
        ),
        Disc(dot, 32.0, 40.0, 6.0),
        Disc(dot, 50.0, 32.0, 6.0),
        Disc(dot, 66.0, 44.0, 6.0),
        Disc(dot, 56.0, 60.0, 6.0),
    ]


@glyph("applications-multimedia")
def applications_multimedia(tint: str) -> ShapeList:
    return [
        Rect(tint, 14.0, 24.0, 72.0, 52.0, 6.0),
        Poly.of(tint, [(40.0, 38.0), (64.0, 50.0), (40.0, 62.0)]),
        Poly.of(palette.BG_DARKEST + "00", [(40.0, 38.0), (40.0, 38.0), (40.0, 38.0)]),
    ]


@glyph("applications-office")
def applications_office(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(22.0, 16.0), (62.0, 16.0), (78.0, 32.0), (78.0, 84.0), (22.0, 84.0)]),
        Poly.of(palette.FG_BRIGHT, [(62.0, 16.0), (78.0, 32.0), (62.0, 32.0)]),
        Line(palette.FG_BRIGHT, 32.0, 48.0, 68.0, 48.0, 5.0),
        Line(palette.FG_BRIGHT, 32.0, 60.0, 68.0, 60.0, 5.0),
        Line(palette.FG_BRIGHT, 32.0, 72.0, 54.0, 72.0, 5.0),
    ]


@glyph("applications-science")
def applications_science(tint: str) -> ShapeList:
    """A conical flask with liquid in it."""
    return [
        Poly.of(tint, [(38.0, 14.0), (62.0, 14.0), (62.0, 40.0), (84.0, 82.0), (16.0, 82.0), (38.0, 40.0)]),
        Poly.of(palette.FG_BRIGHT, [(24.0, 68.0), (76.0, 68.0), (84.0, 82.0), (16.0, 82.0)]),
        Line(palette.FG_BRIGHT, 36.0, 20.0, 64.0, 20.0, 5.0),
    ]


@glyph("applications-engineering")
def applications_engineering(tint: str) -> ShapeList:
    """A pair of dividers, the mark engineers and CAD tools use."""
    return [
        Disc(tint, 50.0, 18.0, 10.0),
        Line(tint, 44.0, 24.0, 22.0, 84.0, 8.0),
        Line(tint, 56.0, 24.0, 78.0, 84.0, 8.0),
        Arc(tint, 50.0, 62.0, 20.0, 40.0, 100.0, 5.0),
    ]


@glyph("applications-utilities")
def applications_utilities(tint: str) -> ShapeList:
    """A spanner crossed with a screwdriver."""
    return [
        Arc(tint, 32.0, 30.0, 14.0, 200.0, 300.0, 10.0),
        Rect(tint, 26.0, 40.0, 12.0, 44.0, 4.0, rotate=35.0, pivot=(32.0, 62.0)),
        Rect(tint, 60.0, 26.0, 12.0, 48.0, 4.0, rotate=-30.0, pivot=(66.0, 50.0)),
        Poly.of(tint, [(54.0, 20.0), (76.0, 20.0), (65.0, 38.0)]),
    ]


@glyph("applications-development")
def applications_development(tint: str) -> ShapeList:
    return [
        Line(tint, 38.0, 26.0, 16.0, 50.0, 9.0),
        Line(tint, 16.0, 50.0, 38.0, 74.0, 9.0),
        Line(tint, 62.0, 26.0, 84.0, 50.0, 9.0),
        Line(tint, 84.0, 50.0, 62.0, 74.0, 9.0),
        Line(tint, 56.0, 22.0, 44.0, 78.0, 8.0),
    ]


@glyph("applications-games")
def applications_games(tint: str) -> ShapeList:
    """A game controller: a wide body with a cross and two buttons."""
    return [
        Poly.of(tint, [(22.0, 40.0), (78.0, 40.0), (90.0, 62.0), (78.0, 78.0), (22.0, 78.0), (10.0, 62.0)]),
        Line(palette.BG_DARKEST + "99", 30.0, 58.0, 42.0, 58.0, 6.0),
        Line(palette.BG_DARKEST + "99", 36.0, 52.0, 36.0, 64.0, 6.0),
        Disc(palette.BG_DARKEST + "99", 64.0, 54.0, 5.0),
        Disc(palette.BG_DARKEST + "99", 72.0, 62.0, 5.0),
    ]


@glyph("applications-education")
def applications_education(tint: str) -> ShapeList:
    """A graduation cap with a tassel."""
    return [
        Poly.of(tint, [(50.0, 22.0), (92.0, 40.0), (50.0, 58.0), (8.0, 40.0)]),
        Poly.of(tint, [(28.0, 48.0), (72.0, 48.0), (72.0, 68.0), (50.0, 80.0), (28.0, 68.0)]),
        Line(tint, 86.0, 42.0, 86.0, 70.0, 4.0),
        Disc(tint, 86.0, 74.0, 5.0),
    ]


@glyph("applications-accessories")
def applications_accessories(tint: str) -> ShapeList:
    """A jigsaw piece, for the catch-all accessories category."""
    return [
        Poly.of(
            tint,
            [
                (22.0, 22.0),
                (46.0, 22.0),
                (46.0, 14.0),
                (62.0, 14.0),
                (62.0, 22.0),
                (78.0, 22.0),
                (78.0, 78.0),
                (22.0, 78.0),
            ],
        ),
        Arc(palette.BG_DARKEST + "00", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        Disc(palette.APP_PLATE, 34.0, 50.0, 10.0),
    ]


@glyph("preferences-desktop")
def preferences_desktop(tint: str) -> ShapeList:
    return [
        Rect(tint, 12.0, 20.0, 76.0, 46.0, 6.0),
        Rect(palette.FG_BRIGHT, 18.0, 26.0, 64.0, 34.0, 3.0),
        Rect(tint, 40.0, 66.0, 20.0, 8.0, 2.0),
        Rect(tint, 26.0, 74.0, 48.0, 8.0, 3.0),
    ]


@glyph("preferences-desktop-theme")
def preferences_desktop_theme(tint: str) -> ShapeList:
    return applications_graphics(tint)


@glyph("preferences-desktop-font")
def preferences_desktop_font(tint: str) -> ShapeList:
    return letter_shapes("a", tint, (16.0, 16.0, 68.0, 68.0), 9.0)


@glyph("system-software-install")
def system_software_install(tint: str) -> ShapeList:
    """A package box with a download arrow on its lid."""
    return [
        Rect(tint, 18.0, 34.0, 64.0, 48.0, 5.0),
        Poly.of(palette.FG_BRIGHT, [(18.0, 34.0), (50.0, 22.0), (82.0, 34.0), (50.0, 46.0)]),
        Rect(palette.FG_BRIGHT, 44.0, 40.0, 12.0, 30.0, 3.0),
        Poly.of(palette.FG_BRIGHT, [(38.0, 62.0), (62.0, 62.0), (50.0, 76.0)]),
    ]


@glyph("software-centre")
def software_centre(tint: str) -> ShapeList:
    """A shopping bag, the shape software centres use on their icon."""
    return [
        Poly.of(tint, [(18.0, 34.0), (82.0, 34.0), (74.0, 86.0), (26.0, 86.0)]),
        Arc(tint, 50.0, 34.0, 16.0, 180.0, 180.0, 7.0),
        Line(palette.BG_DARKEST + "55", 30.0, 44.0, 70.0, 44.0, 4.0),
    ]


@glyph("system-run")
def system_run(tint: str) -> ShapeList:
    return [
        Line(tint, 40.0, 30.0, 20.0, 50.0, 8.0),
        Line(tint, 20.0, 50.0, 40.0, 70.0, 8.0),
        Line(tint, 58.0, 70.0, 82.0, 70.0, 8.0),
    ]


@glyph("system-shutdown")
def system_shutdown(tint: str) -> ShapeList:
    """The power symbol: a ring broken at the top with a bar through it."""
    return [
        Arc(tint, 50.0, 54.0, 26.0, 120.0, 300.0, 9.0),
        Line(tint, 50.0, 16.0, 50.0, 46.0, 9.0),
    ]


@glyph("system-log-out")
def system_log_out(tint: str) -> ShapeList:
    return [
        Rect(tint, 16.0, 20.0, 34.0, 60.0, 5.0),
        Line(tint, 56.0, 50.0, 86.0, 50.0, 8.0),
        Poly.of(tint, [(78.0, 38.0), (92.0, 50.0), (78.0, 62.0)]),
    ]


@glyph("system-lock-screen")
def system_lock_screen(tint: str) -> ShapeList:
    return [
        Arc(tint, 50.0, 38.0, 18.0, 180.0, 180.0, 8.0),
        Rect(tint, 26.0, 42.0, 48.0, 42.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 60.0, 6.0),
    ]


@glyph("system-monitor")
def system_monitor(tint: str) -> ShapeList:
    """A line chart, for system monitors and usage reports."""
    return [
        Rect(tint, 14.0, 20.0, 72.0, 60.0, 6.0),
        Line(palette.FG_BRIGHT, 24.0, 68.0, 40.0, 48.0, 6.0),
        Line(palette.FG_BRIGHT, 40.0, 48.0, 54.0, 60.0, 6.0),
        Line(palette.FG_BRIGHT, 54.0, 60.0, 76.0, 32.0, 6.0),
    ]


@glyph("system-file-manager")
def system_file_manager(tint: str) -> ShapeList:
    from .glyphs_objects import folder

    return folder(tint)


@glyph("system-help")
def system_help(tint: str) -> ShapeList:
    from .glyphs_status import dialog_question

    return dialog_question(tint)


@glyph("accessories-text-editor")
def accessories_text_editor(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(20.0, 16.0), (60.0, 16.0), (76.0, 32.0), (76.0, 84.0), (20.0, 84.0)]),
        Poly.of(palette.FG_BRIGHT, [(60.0, 16.0), (76.0, 32.0), (60.0, 32.0)]),
        Line(palette.FG_BRIGHT, 30.0, 46.0, 64.0, 46.0, 4.0),
        Line(palette.FG_BRIGHT, 30.0, 58.0, 64.0, 58.0, 4.0),
        Line(palette.FG_BRIGHT, 30.0, 70.0, 50.0, 70.0, 4.0),
    ]


@glyph("accessories-calculator")
def accessories_calculator(tint: str) -> ShapeList:
    return [
        Rect(tint, 22.0, 14.0, 56.0, 72.0, 8.0),
        Rect(palette.FG_BRIGHT, 30.0, 22.0, 40.0, 14.0, 3.0),
        Rect(palette.FG_BRIGHT, 30.0, 44.0, 12.0, 12.0, 3.0),
        Rect(palette.FG_BRIGHT, 46.0, 44.0, 12.0, 12.0, 3.0),
        Rect(palette.FG_BRIGHT, 62.0, 44.0, 8.0, 30.0, 3.0),
        Rect(palette.FG_BRIGHT, 30.0, 62.0, 12.0, 12.0, 3.0),
        Rect(palette.FG_BRIGHT, 46.0, 62.0, 12.0, 12.0, 3.0),
    ]


@glyph("accessories-character-map")
def accessories_character_map(tint: str) -> ShapeList:
    return letter_shapes("a", tint, (14.0, 14.0, 72.0, 72.0), 9.0)


@glyph("utilities-terminal")
def utilities_terminal(tint: str) -> ShapeList:
    return [
        Rect(tint, 10.0, 20.0, 80.0, 60.0, 8.0),
        Rect(palette.BG_DARKEST + "88", 10.0, 20.0, 80.0, 12.0, 8.0),
        Line(palette.FG_BRIGHT, 24.0, 48.0, 36.0, 58.0, 5.0),
        Line(palette.FG_BRIGHT, 36.0, 58.0, 24.0, 68.0, 5.0),
        Line(palette.FG_BRIGHT, 46.0, 68.0, 64.0, 68.0, 5.0),
    ]


@glyph("internet-mail")
def internet_mail(tint: str) -> ShapeList:
    from .glyphs_objects import mail

    return mail(tint)


@glyph("internet-chat")
def internet_chat(tint: str) -> ShapeList:
    from .glyphs_objects import chat

    return chat(tint)


@glyph("multimedia-player")
def multimedia_player(tint: str) -> ShapeList:
    return [
        Rect(tint, 20.0, 12.0, 60.0, 76.0, 8.0),
        Rect(palette.FG_BRIGHT, 26.0, 20.0, 48.0, 36.0, 4.0),
        Disc(tint, 50.0, 72.0, 12.0),
        Disc(palette.FG_BRIGHT, 50.0, 72.0, 5.0),
    ]


@glyph("audio-player")
def audio_player(tint: str) -> ShapeList:
    return _note(tint, 44.0, 66.0, 1.5) + [
        Line(tint, 64.0, 26.0, 86.0, 20.0, 6.0),
        Line(tint, 64.0, 38.0, 86.0, 32.0, 6.0),
    ]


@glyph("video-player")
def video_player(tint: str) -> ShapeList:
    return [
        Rect(tint, 10.0, 24.0, 80.0, 52.0, 8.0),
        Poly.of(palette.FG_BRIGHT, [(40.0, 38.0), (66.0, 50.0), (40.0, 62.0)]),
    ]


@glyph("image-editor")
def image_editor(tint: str) -> ShapeList:
    return [
        Rect(tint, 10.0, 22.0, 80.0, 56.0, 6.0),
        Disc(palette.FG_BRIGHT, 30.0, 40.0, 6.0),
        Poly.of(palette.FG_BRIGHT, [(16.0, 72.0), (40.0, 48.0), (66.0, 72.0)]),
        Poly.of(palette.FG_BRIGHT, [(52.0, 72.0), (70.0, 56.0), (86.0, 72.0)]),
    ]


@glyph("screenshot-tool")
def screenshot_tool(tint: str) -> ShapeList:
    return [
        Rect(tint, 12.0, 20.0, 76.0, 60.0, 6.0),
        Arc(palette.FG_BRIGHT, 50.0, 50.0, 15.0, 0.0, 359.999, 5.0),
        Line(palette.FG_BRIGHT, 50.0, 28.0, 50.0, 72.0, 4.0),
        Line(palette.FG_BRIGHT, 28.0, 50.0, 72.0, 50.0, 4.0),
    ]


@glyph("gparted")
def gparted(tint: str) -> ShapeList:
    return [
        Rect(tint, 12.0, 26.0, 76.0, 22.0, 4.0),
        Rect(tint, 12.0, 56.0, 40.0, 22.0, 4.0),
        Rect(palette.FG_BRIGHT, 18.0, 32.0, 20.0, 10.0, 2.0),
        Rect(palette.FG_BRIGHT, 56.0, 62.0, 26.0, 10.0, 2.0),
    ]


@glyph("font-manager")
def font_manager(tint: str) -> ShapeList:
    from .glyphs_mime import font_x_generic

    return font_x_generic(tint)


@glyph("archive-manager")
def archive_manager(tint: str) -> ShapeList:
    from .glyphs_mime import application_x_archive

    return application_x_archive(tint)


@glyph("translation")
def translation(tint: str) -> ShapeList:
    return letter_shapes("a", tint, (10.0, 20.0, 42.0, 42.0), 7.0) + letter_shapes(
        "n", tint, (48.0, 38.0, 42.0, 42.0), 7.0
    )


@glyph("accessories-dictionary")
def accessories_dictionary(tint: str) -> ShapeList:
    from .glyphs_objects import book

    return book(tint)


@glyph("preferences-system-network")
def preferences_system_network(tint: str) -> ShapeList:
    return applications_internet(tint)


@glyph("network-wireless")
def network_wireless(tint: str) -> ShapeList:
    return [
        Arc(tint, 50.0, 76.0, 34.0, 200.0, 140.0, 8.0),
        Arc(tint, 50.0, 76.0, 22.0, 200.0, 140.0, 8.0),
        Disc(tint, 50.0, 76.0, 6.0),
    ]


@glyph("network-wired")
def network_wired(tint: str) -> ShapeList:
    return [
        Rect(tint, 32.0, 12.0, 36.0, 30.0, 4.0),
        Poly.of(tint, [(38.0, 42.0), (62.0, 42.0), (70.0, 74.0), (30.0, 74.0)]),
        Rect(tint, 20.0, 74.0, 60.0, 12.0, 3.0),
    ]


@glyph("bluetooth")
def bluetooth(tint: str) -> ShapeList:
    return [
        Line(tint, 42.0, 22.0, 64.0, 44.0, 7.0),
        Line(tint, 64.0, 44.0, 42.0, 64.0, 7.0),
        Line(tint, 42.0, 36.0, 64.0, 36.0, 0.0),
        Line(tint, 50.0, 14.0, 50.0, 86.0, 7.0),
        Line(tint, 50.0, 14.0, 68.0, 30.0, 7.0),
        Line(tint, 68.0, 30.0, 42.0, 58.0, 7.0),
        Line(tint, 42.0, 58.0, 64.0, 74.0, 7.0),
        Line(tint, 64.0, 74.0, 50.0, 86.0, 7.0),
    ]
