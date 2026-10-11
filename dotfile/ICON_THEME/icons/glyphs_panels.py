"""Settings-panel and panel-applet pictograms.

The settings of every desktop are the longest name list in the theme and the one
with the least artwork behind it: XFCE's settings manager, GNOME's control
centre and KDE's KCMs are all "a monitor", "a keyboard", "a speaker", "a
printer". Rather than forty near-identical gears, this module draws one
pictogram per *meaning*, and the catalogue points every desktop's name at it, so
``xfce4-display-settings``, ``gnome-display-panel`` and ``kcm_randr`` are one
picture drawn once.

The same module supplies the panel applets — the cpugraph, the weather, the
clipboard, the eyes — because an applet is a settings panel's smaller sibling
and the two want to look related when they sit in the same panel. Where a
picture already exists in another family (a keyboard, a printer, a battery, the
network) the catalogue uses that glyph instead: nothing here redraws what
:mod:`icons.glyphs_hardware` or :mod:`icons.glyphs_status` already draws.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import (
    ShapeList,
    arrow,
    bars,
    chevron,
    cloud,
    crescent,
    gear,
    glyph,
    ring,
    star_shape,
    sun_shape,
)
from .primitives import Arc, Disc, Line, Poly, Rect

# A monitor is the base of half a dozen panels, so it is one helper rather than
# six copies of the same four rectangles.
_MONITOR_X = 8.0
_MONITOR_Y = 16.0
_MONITOR_WIDTH = 84.0
_MONITOR_HEIGHT = 56.0


def _monitor(tint: str, stand: bool = True) -> ShapeList:
    """A screen with a bezel, and optionally its foot."""
    shapes: ShapeList = [
        Rect(tint, _MONITOR_X, _MONITOR_Y, _MONITOR_WIDTH, _MONITOR_HEIGHT, 7.0),
        Rect(
            palette.BG_DARKEST + "99",
            _MONITOR_X + 6.0,
            _MONITOR_Y + 6.0,
            _MONITOR_WIDTH - 12.0,
            _MONITOR_HEIGHT - 12.0,
            4.0,
        ),
    ]
    if stand:
        bottom = _MONITOR_Y + _MONITOR_HEIGHT
        shapes += [
            Rect(tint, 40.0, bottom + 2.0, 20.0, 8.0, 2.0),
            Rect(tint, 28.0, bottom + 10.0, 44.0, 8.0, 3.0),
        ]
    return shapes


def _glass(ink: str) -> ShapeList:
    """The dark inner surface of the monitor, for a mark to sit on."""
    return [
        Rect(
            ink,
            _MONITOR_X + 6.0,
            _MONITOR_Y + 6.0,
            _MONITOR_WIDTH - 12.0,
            _MONITOR_HEIGHT - 12.0,
            4.0,
        )
    ]


def _sliders(tint: str, rows: tuple[tuple[float, float], ...]) -> ShapeList:
    """One track and one knob per row, for every "preferences" pictogram."""
    shapes: ShapeList = []
    for y, knob in rows:
        shapes.append(Line(tint, 18.0, y, 82.0, y, 8.0))
        cx = 18.0 + knob * 64.0
        shapes.append(Disc(tint, cx, y, 12.0))
        shapes.append(Disc(palette.BG_DARKEST + "AA", cx, y, 4.5))
    return shapes


@glyph("panel-manager")
def panel_manager(tint: str) -> ShapeList:
    """Three sliders: the settings manager itself, and every "preferences"."""
    return _sliders(tint, ((28.0, 0.30), (50.0, 0.72), (72.0, 0.45)))


@glyph("panel-display")
def panel_display(tint: str) -> ShapeList:
    """A monitor with a cog on the glass: display and resolution settings."""
    return _monitor(tint) + gear(tint, 50.0, 44.0, 11.0, 8, 6.0, 5.0)


@glyph("panel-wallpaper")
def panel_wallpaper(tint: str) -> ShapeList:
    """A framed landscape: desktop, wallpaper and backdrop settings."""
    return [
        Rect(tint, 10.0, 20.0, 80.0, 60.0, 5.0),
        Disc(palette.BG_DARKEST + "99", 32.0, 38.0, 7.0),
        Poly.of(palette.BG_DARKEST + "99", [(16.0, 74.0), (44.0, 44.0), (70.0, 74.0)]),
        Poly.of(palette.BG_DARKEST + "99", [(54.0, 74.0), (72.0, 52.0), (84.0, 74.0)]),
    ]


@glyph("panel-power")
def panel_power(tint: str) -> ShapeList:
    """A wall socket: power management, suspend and the power applets."""
    return [
        Rect(tint, 20.0, 20.0, 60.0, 60.0, 12.0),
        Disc(palette.BG_DARKEST + "99", 38.0, 40.0, 6.5),
        Disc(palette.BG_DARKEST + "99", 62.0, 40.0, 6.5),
        Arc(palette.BG_DARKEST + "99", 50.0, 58.0, 9.0, 0.0, 359.999, 6.0),
    ]


@glyph("panel-notifications")
def panel_notifications(tint: str) -> ShapeList:
    """A bell: the notification daemon and its preferences."""
    return [
        Poly.of(
            tint,
            [(28.0, 64.0), (33.0, 34.0), (50.0, 22.0), (67.0, 34.0), (72.0, 64.0)],
        ),
        Rect(tint, 22.0, 62.0, 56.0, 9.0, 4.0),
        Disc(tint, 50.0, 78.0, 8.0),
        Disc(tint, 50.0, 18.0, 5.0),
    ]


@glyph("panel-shield")
def panel_shield(tint: str) -> ShapeList:
    """A shield with a keyhole: privacy, security and firewall panels."""
    return [
        Poly.of(
            tint,
            [
                (50.0, 10.0),
                (84.0, 26.0),
                (84.0, 52.0),
                (50.0, 90.0),
                (16.0, 52.0),
                (16.0, 26.0),
            ],
        ),
        Disc(palette.BG_DARKEST + "99", 50.0, 44.0, 9.0),
        Rect(palette.BG_DARKEST + "99", 46.0, 46.0, 8.0, 18.0, 3.0),
    ]


@glyph("panel-accessibility")
def panel_accessibility(tint: str) -> ShapeList:
    """A figure with outstretched arms, the mark for accessibility settings."""
    return [
        Disc(tint, 50.0, 20.0, 11.0),
        Line(tint, 50.0, 32.0, 50.0, 62.0, 9.0),
        Line(tint, 22.0, 42.0, 78.0, 42.0, 9.0),
        Line(tint, 50.0, 62.0, 30.0, 88.0, 9.0),
        Line(tint, 50.0, 62.0, 70.0, 88.0, 9.0),
    ]


@glyph("panel-screensaver")
def panel_screensaver(tint: str) -> ShapeList:
    """A screen showing a moon: screensaver and lock settings."""
    return (
        _monitor(tint)
        + _glass(palette.BG_DARKEST + "99")
        + crescent(palette.FG_BRIGHT, palette.BG_DARKEST, 44.0, 44.0, 14.0)
        + [
            Disc(palette.FG_BRIGHT, 70.0, 34.0, 2.5),
            Disc(palette.FG_BRIGHT, 74.0, 52.0, 2.0),
        ]
    )


@glyph("panel-color")
def panel_color(tint: str) -> ShapeList:
    """A droplet: colour management, colour profiles and the colour chooser."""
    return [
        Poly.of(tint, [(50.0, 10.0), (74.0, 52.0), (26.0, 52.0)]),
        Disc(tint, 50.0, 60.0, 26.0),
        Arc(palette.BG_DARKEST + "88", 50.0, 60.0, 14.0, 200.0, 140.0, 5.0),
    ]


@glyph("panel-stylus")
def panel_stylus(tint: str) -> ShapeList:
    """A stylus: graphics tablets, touchscreens and pen input."""
    return [
        Poly.of(
            tint,
            [(58.0, 14.0), (78.0, 22.0), (44.0, 78.0), (30.0, 86.0), (36.0, 68.0)],
        ),
        Line(palette.BG_DARKEST + "88", 52.0, 30.0, 66.0, 36.0, 4.0),
    ]


@glyph("panel-firewall")
def panel_firewall(tint: str) -> ShapeList:
    """A flame: firewall rules and the network security panels."""
    return [
        Poly.of(
            tint,
            [
                (50.0, 10.0),
                (64.0, 34.0),
                (74.0, 54.0),
                (68.0, 76.0),
                (50.0, 90.0),
                (32.0, 76.0),
                (26.0, 54.0),
                (36.0, 34.0),
            ],
        ),
        Poly.of(
            palette.BG_DARKEST + "99",
            [(50.0, 40.0), (60.0, 60.0), (50.0, 76.0), (40.0, 60.0)],
        ),
    ]


@glyph("panel-login")
def panel_login(tint: str) -> ShapeList:
    """A door with an arrow: login, session and autostart settings."""
    return [
        Rect(tint, 16.0, 14.0, 44.0, 72.0, 5.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 52.0, 5.0),
        Line(tint, 64.0, 50.0, 84.0, 50.0, 8.0),
        Poly.of(tint, [(78.0, 38.0), (92.0, 50.0), (78.0, 62.0)]),
    ]


@glyph("panel-bar")
def panel_bar(tint: str) -> ShapeList:
    """A panel with applets in it: panel, dock and taskbar settings."""
    return [
        Rect(tint, 10.0, 34.0, 80.0, 32.0, 6.0),
        Disc(palette.BG_DARKEST + "99", 24.0, 50.0, 6.0),
        Rect(palette.BG_DARKEST + "99", 38.0, 44.0, 30.0, 12.0, 3.0),
        Disc(palette.BG_DARKEST + "99", 78.0, 50.0, 6.0),
    ]


@glyph("panel-rocket")
def panel_rocket(tint: str) -> ShapeList:
    """A rocket: autostart, startup applications and session startup."""
    return [
        Poly.of(
            tint,
            [(50.0, 10.0), (64.0, 44.0), (64.0, 66.0), (36.0, 66.0), (36.0, 44.0)],
        ),
        Poly.of(tint, [(36.0, 52.0), (20.0, 70.0), (36.0, 66.0)]),
        Poly.of(tint, [(64.0, 52.0), (80.0, 70.0), (64.0, 66.0)]),
        Disc(palette.BG_DARKEST + "99", 50.0, 34.0, 7.0),
        Poly.of(palette.ACCENT_HOVER, [(44.0, 70.0), (56.0, 70.0), (50.0, 90.0)]),
    ]


@glyph("panel-touchpad")
def panel_touchpad(tint: str) -> ShapeList:
    """A touchpad with a fingertip on it: pointer and touchpad settings."""
    return [
        Rect(tint, 14.0, 24.0, 72.0, 56.0, 8.0),
        Rect(palette.BG_DARKEST + "99", 20.0, 30.0, 60.0, 44.0, 5.0),
        Disc(palette.FG_BRIGHT, 50.0, 50.0, 9.0),
        Rect(palette.FG_BRIGHT, 44.0, 54.0, 12.0, 14.0, 5.0),
    ]


@glyph("panel-cursor")
def panel_cursor(tint: str) -> ShapeList:
    """A pointer: cursor theme, mouse and pointer settings."""
    return [
        Poly.of(
            tint,
            [
                (34.0, 14.0),
                (34.0, 74.0),
                (48.0, 60.0),
                (58.0, 84.0),
                (68.0, 78.0),
                (58.0, 56.0),
                (76.0, 56.0),
            ],
        )
    ]


def _eye(fill: str, ink: str, pupil: str, cx: float, cy: float, radius: float) -> ShapeList:
    """One eye: an iris with a dark ring and a bright pupil.

    The xfce4-eyes applet draws two of these and moves the pupils, so the pupil
    is a separate disc a caller could in principle place elsewhere rather than
    being painted into the iris.
    """
    return [
        Disc(fill, cx, cy, radius),
        Disc(ink, cx, cy, radius * 0.58),
        Disc(pupil, cx, cy, radius * 0.28),
    ]


@glyph("panel-update")
def panel_update(tint: str) -> ShapeList:
    """An arrow inside a ring: software updates and package installation."""
    return ring(tint, 50.0, 50.0, 30.0, 7.0) + arrow(
        tint, 50.0, 30.0, 50.0, 70.0, 7.0, 13.0
    )


@glyph("panel-disk")
def panel_disk(tint: str) -> ShapeList:
    """A doughnut chart with a slice out of it: disk usage and free space."""
    return [
        Disc(tint, 50.0, 52.0, 32.0),
        Poly.of(palette.BG_DARKEST + "99", [(50.0, 52.0), (50.0, 20.0), (76.0, 42.0)]),
        Disc(palette.BG_DARKEST + "AA", 50.0, 52.0, 12.0),
    ]


@glyph("panel-thermometer")
def panel_thermometer(tint: str) -> ShapeList:
    """A thermometer: the hardware sensors applet and sensor settings."""
    return [
        Rect(tint, 40.0, 12.0, 20.0, 56.0, 10.0),
        Disc(tint, 50.0, 74.0, 18.0),
        Disc(palette.FG_BRIGHT, 50.0, 74.0, 9.0),
        Line(palette.FG_BRIGHT, 50.0, 34.0, 50.0, 60.0, 7.0),
    ]


@glyph("panel-icons")
def panel_icons(tint: str) -> ShapeList:
    """A plate with a star on it: icon theme and icon settings."""
    return [
        Rect(tint, 14.0, 14.0, 72.0, 72.0, 14.0),
        star_shape(palette.BG_DARKEST + "AA", 50.0, 50.0, 26.0, 11.0),
    ]


@glyph("panel-key")
def panel_key(tint: str) -> ShapeList:
    """A key: keyboard shortcuts, key bindings and the xkb applets."""
    return [
        *ring(tint, 34.0, 34.0, 18.0, 9.0),
        Line(tint, 46.0, 46.0, 80.0, 80.0, 9.0),
        Line(tint, 64.0, 72.0, 74.0, 62.0, 8.0),
        Line(tint, 72.0, 80.0, 82.0, 70.0, 8.0),
    ]


@glyph("panel-weather")
def panel_weather(tint: str) -> ShapeList:
    """A sun behind a cloud: the weather applet and weather settings."""
    return sun_shape(tint, 32.0, 32.0, 13.0, 8, 17.0, 10.0, 5.0) + cloud(
        palette.FG_BRIGHT, 56.0, 62.0, 44.0
    )


@glyph("panel-brightness")
def panel_brightness(tint: str) -> ShapeList:
    """A sun: brightness, backlight and display power settings."""
    return sun_shape(tint, 50.0, 50.0, 17.0, 8, 24.0, 20.0, 6.0)


@glyph("panel-traffic")
def panel_traffic(tint: str) -> ShapeList:
    """An up and a down arrow: netload, wavelan and bandwidth applets."""
    return [
        *arrow(tint, 32.0, 84.0, 32.0, 18.0, 7.0, 14.0),
        *arrow(tint, 68.0, 16.0, 68.0, 82.0, 7.0, 14.0),
    ]


@glyph("panel-clipboard")
def panel_clipboard(tint: str) -> ShapeList:
    """A clipboard: the clipman applet and its history."""
    return [
        Rect(tint, 22.0, 16.0, 56.0, 72.0, 6.0),
        Rect(tint, 34.0, 8.0, 32.0, 14.0, 4.0),
        Line(palette.BG_DARKEST + "99", 32.0, 40.0, 68.0, 40.0, 5.0),
        Line(palette.BG_DARKEST + "99", 32.0, 54.0, 68.0, 54.0, 5.0),
        Line(palette.BG_DARKEST + "99", 32.0, 68.0, 56.0, 68.0, 5.0),
    ]


@glyph("panel-note")
def panel_note(tint: str) -> ShapeList:
    """A sticky note with its corner turned: the notes applet."""
    return [
        Poly.of(
            tint,
            [(18.0, 14.0), (82.0, 14.0), (82.0, 64.0), (62.0, 86.0), (18.0, 86.0)],
        ),
        Poly.of(palette.BG_DARKEST + "99", [(82.0, 64.0), (62.0, 64.0), (62.0, 86.0)]),
        Line(palette.BG_DARKEST + "88", 30.0, 38.0, 68.0, 38.0, 5.0),
        Line(palette.BG_DARKEST + "88", 30.0, 52.0, 56.0, 52.0, 5.0),
    ]


@glyph("panel-eyes")
def panel_eyes(tint: str) -> ShapeList:
    """Two eyes: the eyes applet, and anything that watches."""
    return _eye(
        tint, palette.BG_DARKEST + "99", palette.FG_BRIGHT, 32.0, 40.0, 17.0
    ) + _eye(tint, palette.BG_DARKEST + "99", palette.FG_BRIGHT, 68.0, 60.0, 17.0)


@glyph("panel-timer")
def panel_timer(tint: str) -> ShapeList:
    """A stopwatch: the timer and time-out applets."""
    return [
        *ring(tint, 50.0, 56.0, 28.0, 8.0),
        Line(tint, 50.0, 56.0, 50.0, 36.0, 7.0),
        Line(tint, 50.0, 56.0, 66.0, 66.0, 6.0),
        Rect(tint, 42.0, 12.0, 16.0, 10.0, 3.0),
        Line(tint, 68.0, 24.0, 78.0, 34.0, 7.0),
    ]


@glyph("panel-prompt")
def panel_prompt(tint: str) -> ShapeList:
    """A prompt with a cursor bar: the verve command applet."""
    return [
        *chevron(tint, 30.0, 40.0, 34.0, 10.0),
        Line(tint, 52.0, 62.0, 86.0, 62.0, 10.0),
    ]


@glyph("panel-menu")
def panel_menu(tint: str) -> ShapeList:
    """A menu with a pointer on it: Whisker Menu, app finders, menus in general."""
    return [
        Rect(tint, 14.0, 18.0, 58.0, 44.0, 5.0),
    ] + bars(palette.BG_DARKEST + "99", 43.0, 40.0, 36.0, 5.0, 3, 12.0) + [
        Poly.of(
            palette.FG_BRIGHT,
            [
                (58.0, 52.0),
                (58.0, 88.0),
                (68.0, 78.0),
                (76.0, 92.0),
                (84.0, 88.0),
                (76.0, 74.0),
                (90.0, 74.0),
            ],
        )
    ]


@glyph("panel-plug")
def panel_plug(tint: str) -> ShapeList:
    """A plug on a cord: the mount applet and removable media settings."""
    return [
        Rect(tint, 34.0, 20.0, 32.0, 34.0, 6.0),
        Line(tint, 42.0, 8.0, 42.0, 22.0, 7.0),
        Line(tint, 58.0, 8.0, 58.0, 22.0, 7.0),
        Line(tint, 50.0, 54.0, 50.0, 72.0, 8.0),
        Rect(tint, 36.0, 70.0, 28.0, 18.0, 5.0),
    ]
