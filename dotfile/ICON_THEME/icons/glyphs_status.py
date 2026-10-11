"""Pictograms for state: dialogs, presence, battery, sound and emblems.

Everything here answers a question rather than naming an object: is this
warning important, is the battery nearly flat, is this person online, does this
file need attention. They are the icons a user reads at a glance, so each one is
built around a single mark — a check, a cross, a bar — inside a shape that says
which family it belongs to.

Emblems are the exception: they are laid over another icon, so they are drawn
small and self-contained, with a solid disc behind the mark to keep them legible
against whatever picture they land on.
"""

from __future__ import annotations

from typing import Sequence

from . import palette
from .glyphs_base import (
    ShapeList,
    arrow_head,
    check,
    cloud,
    crescent,
    cross,
    glyph,
    minus,
    point_on,
    ring,
    star_shape,
    sun_shape,
)
from .primitives import Arc, Disc, Line, Poly, Rect

# The disc a dialog or an emblem is drawn on. It fills most of the box because
# both are read at a glance and most of them end up at sixteen pixels.
BADGE_RADIUS = 36.0


def _badge(fill: str) -> ShapeList:
    """A solid disc filling most of the box, the plate an alert sits on."""
    return [Disc(fill, 50.0, 50.0, BADGE_RADIUS)]


def _person(tint: str, cx: float, cy: float, scale: float = 1.0) -> ShapeList:
    """A head and shoulders, the base every presence icon is built from."""
    head = 17.0 * scale
    shoulder_top = cy + head * 0.1
    return [
        Disc(tint, cx, cy - head * 1.05, head),
        Rect(
            tint,
            cx - 24.0 * scale,
            shoulder_top + head * 0.85,
            48.0 * scale,
            26.0 * scale,
            12.0 * scale,
        ),
    ]


def _presence(tint: str, mark: ShapeList) -> ShapeList:
    """The person with a small state badge in the lower right corner.

    The badge sits on a solid disc of the surface colour first, so a check on
    it is not read as part of the shoulder underneath.
    """
    badge_x = 76.0
    badge_y = 76.0
    return _person(tint, 42.0, 40.0, 0.86) + [
        Disc(palette.BG, badge_x, badge_y, 20.0),
        Disc(tint, badge_x, badge_y, 16.0),
    ] + mark


def _battery(tint: str, level: float, bolt: bool = False) -> ShapeList:
    """A battery with its charge drawn as a bar of ``level`` of the cell."""
    left, top, width, height = 12.0, 30.0, 68.0, 40.0
    inset = 5.0
    inner_width = width - inset * 2.0
    inner_height = height - inset * 2.0
    shapes: ShapeList = [
        Rect(tint, left, top, width, height, 7.0),
        Rect(palette.BG_DARKEST + "99", left + inset, top + inset, inner_width, inner_height, 3.0),
        Rect(tint, left + width + 2.0, top + 12.0, 8.0, 16.0, 3.0),
        Rect(tint, left + inset, top + inset, inner_width * level, inner_height, 3.0),
    ]
    if bolt:
        shapes.append(
            Poly.of(
                palette.FG_BRIGHT,
                [
                    (58.0, 34.0),
                    (46.0, 52.0),
                    (54.0, 52.0),
                    (50.0, 68.0),
                    (64.0, 48.0),
                    (55.0, 48.0),
                ],
            )
        )
    return shapes


@glyph("dialog-information")
def dialog_information(tint: str) -> ShapeList:
    """An "i" in a filled disc."""
    return _badge(tint) + [
        Disc(palette.BG_DARKEST, 50.0, 32.0, 5.0),
        Line(palette.BG_DARKEST, 50.0, 44.0, 50.0, 68.0, 11.0),
    ]


@glyph("dialog-question")
def dialog_question(tint: str) -> ShapeList:
    """A question mark in a filled disc."""
    return _badge(tint) + [
        Arc(palette.BG_DARKEST, 50.0, 38.0, 13.0, 150.0, 240.0, 8.0),
        Line(palette.BG_DARKEST, 50.0, 51.0, 50.0, 58.0, 8.0),
        Disc(palette.BG_DARKEST, 50.0, 70.0, 5.0),
    ]


@glyph("dialog-error")
def dialog_error(tint: str) -> ShapeList:
    """A cross in a filled disc, for the failures that stop something."""
    return _badge(tint) + cross(palette.BG_DARKEST, 50.0, 50.0, 21.0, 9.0)


@glyph("dialog-warning")
def dialog_warning(tint: str) -> ShapeList:
    """A triangle with an exclamation in it."""
    return [
        Poly.of(tint, [(50.0, 12.0), (90.0, 84.0), (10.0, 84.0)]),
        Line(palette.BG_DARKEST, 50.0, 38.0, 50.0, 62.0, 8.0),
        Disc(palette.BG_DARKEST, 50.0, 73.0, 5.0),
    ]


@glyph("security-high")
def security_high(tint: str) -> ShapeList:
    """A shield with a tick on it: the channel or the key is sound."""
    return _shield(tint) + check(palette.BG_DARKEST, 50.0, 48.0, 34.0, 9.0)


@glyph("security-medium")
def security_medium(tint: str) -> ShapeList:
    """A shield with a bar on it: secure, but not unconditionally."""
    return _shield(tint) + minus(palette.BG_DARKEST, 50.0, 48.0, 14.0, 9.0)


@glyph("security-low")
def security_low(tint: str) -> ShapeList:
    """A shield with a dot on it: the link is not secured at all."""
    return _shield(tint) + [Disc(palette.BG_DARKEST, 50.0, 48.0, 7.0)]


def _shield(tint: str) -> ShapeList:
    """The shield all three security levels are drawn on."""
    return [
        Poly.of(
            tint,
            [
                (50.0, 10.0),
                (86.0, 24.0),
                (86.0, 50.0),
                (50.0, 90.0),
                (14.0, 50.0),
                (14.0, 24.0),
            ],
        )
    ]


@glyph("user")
def user(tint: str) -> ShapeList:
    """A head and shoulders: the account and avatar fallback."""
    return _person(tint, 50.0, 36.0, 1.0)


@glyph("user-available")
def user_available(tint: str) -> ShapeList:
    """The person with a tick: online and free to talk."""
    return _presence(tint, check(palette.BG_DARKEST, 76.0, 76.0, 18.0, 5.0))


@glyph("user-away")
def user_away(tint: str) -> ShapeList:
    """The person with a clock: online but idle."""
    return _presence(
        tint,
        [
            *ring(palette.BG_DARKEST, 76.0, 76.0, 8.0, 4.0),
            Line(palette.BG_DARKEST, 76.0, 72.0, 76.0, 76.0, 4.0),
            Line(palette.BG_DARKEST, 76.0, 76.0, 81.0, 79.0, 4.0),
        ],
    )


@glyph("user-busy")
def user_busy(tint: str) -> ShapeList:
    """The person with a bar: online and not to be disturbed."""
    return _presence(tint, minus(palette.BG_DARKEST, 76.0, 76.0, 8.0, 6.0))


@glyph("user-offline")
def user_offline(tint: str) -> ShapeList:
    """The person with a cross: signed out."""
    return _presence(tint, cross(palette.BG_DARKEST, 76.0, 76.0, 10.0, 5.0))


@glyph("user-group")
def user_group(tint: str) -> ShapeList:
    """Two people, the front one in the context tint and one behind it."""
    return _person(palette.FG_BRIGHT + "aa", 66.0, 40.0, 0.72) + _person(
        tint, 38.0, 38.0, 0.92
    )


@glyph("battery-full")
def battery_full(tint: str) -> ShapeList:
    """A battery filled to the top."""
    return _battery(tint, 1.0)


@glyph("battery-good")
def battery_good(tint: str) -> ShapeList:
    """A battery a little over half full."""
    return _battery(tint, 0.55)


@glyph("battery-low")
def battery_low(tint: str) -> ShapeList:
    """A battery nearly flat."""
    return _battery(tint, 0.2)


@glyph("battery-charging")
def battery_charging(tint: str) -> ShapeList:
    """A battery with a bolt on it, for the AC adapter."""
    return _battery(tint, 0.5, bolt=True)


@glyph("image-loading")
def image_loading(tint: str) -> ShapeList:
    """A frame with an open ring inside it: a thumbnail that is still coming."""
    return [
        Rect(tint, 12.0, 20.0, 76.0, 60.0, 6.0),
        Arc(palette.FG_BRIGHT, 50.0, 50.0, 20.0, -60.0, 300.0, 8.0),
    ]


@glyph("image-missing")
def image_missing(tint: str) -> ShapeList:
    """A frame with a cross through it, for a picture that will not load."""
    return [
        Rect(tint, 12.0, 20.0, 76.0, 60.0, 6.0),
        Rect(palette.BG_DARKEST + "99", 18.0, 26.0, 64.0, 48.0, 3.0),
        *cross(tint, 50.0, 50.0, 20.0, 8.0),
    ]


@glyph("audio-volume-high")
def audio_volume_high(tint: str) -> ShapeList:
    """A speaker with two waves coming off it."""
    return [
        Poly.of(tint, [(14.0, 40.0), (32.0, 40.0), (52.0, 24.0), (52.0, 76.0), (32.0, 60.0), (14.0, 60.0)]),
        Arc(tint, 54.0, 50.0, 16.0, 300.0, 120.0, 6.0),
        Arc(tint, 54.0, 50.0, 27.0, 300.0, 120.0, 6.0),
    ]


@glyph("audio-volume-low")
def audio_volume_low(tint: str) -> ShapeList:
    """The same speaker with a single wave, for quiet and for mute."""
    return [
        Poly.of(tint, [(14.0, 40.0), (32.0, 40.0), (52.0, 24.0), (52.0, 76.0), (32.0, 60.0), (14.0, 60.0)]),
        Arc(tint, 54.0, 50.0, 16.0, 300.0, 120.0, 6.0),
    ]


@glyph("microphone")
def microphone(tint: str) -> ShapeList:
    """A condenser microphone in its cradle, for recorders and call controls."""
    return [
        Rect(tint, 38.0, 12.0, 24.0, 44.0, 12.0),
        Arc(tint, 50.0, 54.0, 24.0, 0.0, 180.0, 6.0),
        Line(tint, 50.0, 78.0, 50.0, 88.0, 6.0),
        Line(tint, 36.0, 88.0, 64.0, 88.0, 6.0),
    ]


# --- emblems -----------------------------------------------------------------
# An emblem is drawn over another icon, so each one is a mark cut out of a solid
# disc of the context tint. The mark is always the darker surface colour, which
# is what keeps a sixteen pixel badge readable on top of a photo or a folder.


@glyph("emblem-default")
def emblem_default(tint: str) -> ShapeList:
    """The tick, for a file that is the default for its type."""
    return _badge(tint) + check(palette.BG_DARKEST, 50.0, 50.0, 40.0, 10.0)


@glyph("emblem-important")
def emblem_important(tint: str) -> ShapeList:
    """The exclamation, for a file the user has flagged."""
    return _badge(tint) + [
        Line(palette.BG_DARKEST, 50.0, 30.0, 50.0, 56.0, 10.0),
        Disc(palette.BG_DARKEST, 50.0, 68.0, 6.0),
    ]


@glyph("emblem-favorite")
def emblem_favorite(tint: str) -> ShapeList:
    """The star, for a file the user has marked as a favourite."""
    return _badge(tint) + [star_shape(palette.BG_DARKEST, 50.0, 52.0, 26.0, 11.0)]


@glyph("emblem-photos")
def emblem_photos(tint: str) -> ShapeList:
    """A framed landscape, for a picture."""
    return _badge(tint) + [
        Rect(palette.BG_DARKEST, 28.0, 34.0, 44.0, 32.0, 4.0),
        Disc(tint, 38.0, 44.0, 4.5),
        Poly.of(tint, [(32.0, 66.0), (48.0, 50.0), (68.0, 66.0)]),
    ]


@glyph("emblem-documents")
def emblem_documents(tint: str) -> ShapeList:
    """A page with text lines on it, for a document."""
    return _badge(tint) + [
        Poly.of(
            palette.BG_DARKEST,
            [(36.0, 24.0), (60.0, 24.0), (70.0, 34.0), (70.0, 76.0), (36.0, 76.0)],
        ),
        Poly.of(tint, [(60.0, 24.0), (70.0, 34.0), (60.0, 34.0)]),
        Line(tint, 42.0, 52.0, 64.0, 52.0, 5.0),
        Line(tint, 42.0, 63.0, 64.0, 63.0, 4.0),
    ]


@glyph("emblem-music")
def emblem_music(tint: str) -> ShapeList:
    """A single note, for an audio file."""
    from .glyphs_objects import _note

    return _badge(tint) + _note(palette.BG_DARKEST, 44.0, 62.0, 1.1)


@glyph("emblem-videos")
def emblem_videos(tint: str) -> ShapeList:
    """A film frame with a play mark in it."""
    return _badge(tint) + [
        Rect(palette.BG_DARKEST, 30.0, 32.0, 40.0, 34.0, 4.0),
        Poly.of(tint, [(44.0, 39.0), (61.0, 49.0), (44.0, 59.0)]),
    ]


@glyph("emblem-readonly")
def emblem_readonly(tint: str) -> ShapeList:
    """A padlock, for a file that cannot be written to."""
    return _badge(tint) + [
        Line(palette.BG_DARKEST, 40.0, 48.0, 40.0, 40.0, 7.0),
        Line(palette.BG_DARKEST, 40.0, 40.0, 60.0, 40.0, 7.0),
        Line(palette.BG_DARKEST, 60.0, 40.0, 60.0, 48.0, 7.0),
        Rect(palette.BG_DARKEST, 34.0, 48.0, 32.0, 28.0, 5.0),
    ]


@glyph("emblem-synchronizing")
def emblem_synchronizing(tint: str) -> ShapeList:
    """Two arrows chasing each other, for a file that is being synced."""
    radius = 26.0
    arcs: ShapeList = [
        Arc(tint, 50.0, 50.0, radius, -30.0, 140.0, 8.0),
        Arc(tint, 50.0, 50.0, radius, 150.0, 140.0, 8.0),
    ]
    for end in (110.0, 290.0):
        x, y = point_on(50.0, 50.0, radius, end)
        arcs.append(arrow_head(tint, x, y, end + 90.0, 13.0))
    return arcs


@glyph("emblem-shared")
def emblem_shared(tint: str) -> ShapeList:
    """Three linked nodes, for a file shared with other people."""
    return [
        Disc(tint, 38.0, 36.0, 11.0),
        Disc(tint, 38.0, 68.0, 11.0),
        Disc(tint, 68.0, 52.0, 11.0),
        Line(tint, 46.0, 42.0, 60.0, 48.0, 7.0),
        Line(tint, 46.0, 62.0, 60.0, 56.0, 7.0),
    ]


@glyph("emblem-system")
def emblem_system(tint: str) -> ShapeList:
    """A cog, for a file the system owns."""
    from .glyphs_base import gear

    return _badge(tint) + gear(palette.BG_DARKEST, 50.0, 50.0, 18.0, 6, 8.0, 9.0)


def _signal(tint: str, reached: int) -> ShapeList:
    """Four bars of increasing height, the first ``reached`` of them lit.

    Signal strength is a ladder rather than a fan because a ladder stays
    readable at the sixteen pixels a panel gives it, and because "how many bars"
    is the way a user already counts it.
    """
    shapes: ShapeList = []
    for index in range(4):
        height = 16.0 + index * 15.0
        left = 18.0 + index * 18.0
        ink = tint if index < reached else palette.BORDER
        shapes.append(Rect(ink, left, 82.0 - height, 13.0, height, 3.0))
    return shapes


@glyph("network-wireless-signal-none")
def network_wireless_signal_none(tint: str) -> ShapeList:
    """The ladder with nothing lit: no signal at all."""
    return _signal(tint, 0)


@glyph("network-wireless-signal-weak")
def network_wireless_signal_weak(tint: str) -> ShapeList:
    """One bar lit: there is a network, but barely."""
    return _signal(tint, 1)


@glyph("network-wireless-signal-ok")
def network_wireless_signal_ok(tint: str) -> ShapeList:
    """Two bars lit: usable, and not much more."""
    return _signal(tint, 2)


@glyph("network-wireless-signal-good")
def network_wireless_signal_good(tint: str) -> ShapeList:
    """Three bars lit."""
    return _signal(tint, 3)


@glyph("network-wireless-signal-excellent")
def network_wireless_signal_excellent(tint: str) -> ShapeList:
    """Four bars lit: the connection at full strength."""
    return _signal(tint, 4)


def _check_box(tint: str) -> ShapeList:
    """The square every checkbox state is drawn on."""
    return [
        Rect(tint, 18.0, 18.0, 64.0, 64.0, 12.0),
        Rect(palette.BG_DARKEST + "99", 26.0, 26.0, 48.0, 48.0, 7.0),
    ]


@glyph("checkbox-unchecked")
def checkbox_unchecked(tint: str) -> ShapeList:
    """An empty box: the menu item that carries it is off."""
    return _check_box(tint)


@glyph("checkbox-checked")
def checkbox_checked(tint: str) -> ShapeList:
    """A box with a tick: the menu item is on."""
    return _check_box(tint) + check(palette.BG_DARKEST, 50.0, 50.0, 44.0, 11.0)


@glyph("checkbox-mixed")
def checkbox_mixed(tint: str) -> ShapeList:
    """A box with a bar: some of what it covers is selected."""
    return _check_box(tint) + minus(palette.BG_DARKEST, 50.0, 50.0, 18.0, 11.0)


@glyph("radio-unchecked")
def radio_unchecked(tint: str) -> ShapeList:
    """A hollow ring: nothing chosen in this group."""
    return list(ring(tint, 50.0, 50.0, 32.0, 10.0))


@glyph("radio-checked")
def radio_checked(tint: str) -> ShapeList:
    """A ring with a filled centre: this one is chosen."""
    return list(ring(tint, 50.0, 50.0, 32.0, 10.0)) + [Disc(tint, 50.0, 50.0, 16.0)]


@glyph("radio-mixed")
def radio_mixed(tint: str) -> ShapeList:
    """A ring with a small centre: the choice is not exclusive."""
    return list(ring(tint, 50.0, 50.0, 32.0, 10.0)) + [Disc(tint, 50.0, 50.0, 9.0)]


@glyph("weather-clear")
def weather_clear(tint: str) -> ShapeList:
    """A sun: cloudless sky."""
    return sun_shape(tint, 50.0, 50.0, 17.0, 8, 25.0, 20.0, 6.0)


@glyph("weather-clear-night")
def weather_clear_night(tint: str) -> ShapeList:
    """A moon with two stars: cloudless night."""
    return crescent(palette.FG_BRIGHT, palette.BG_DARKEST, 46.0, 48.0, 30.0) + [
        Disc(palette.FG_BRIGHT, 76.0, 26.0, 4.0),
        Disc(palette.FG_BRIGHT, 86.0, 46.0, 3.0),
    ]


@glyph("weather-few-clouds")
def weather_few_clouds(tint: str) -> ShapeList:
    """A sun with one cloud over its corner."""
    return sun_shape(tint, 34.0, 32.0, 12.0, 8, 17.0, 10.0, 5.0) + cloud(
        palette.FG_BRIGHT, 56.0, 62.0, 44.0
    )


@glyph("weather-clouds")
def weather_clouds(tint: str) -> ShapeList:
    """One cloud: overcast, with the sky still visible behind it."""
    return cloud(tint, 50.0, 48.0, 54.0)


@glyph("weather-overcast")
def weather_overcast(tint: str) -> ShapeList:
    """Two clouds, the lower one in front."""
    return cloud(palette.FG_DIM, 36.0, 40.0, 40.0) + cloud(tint, 58.0, 60.0, 46.0)


@glyph("weather-fog")
def weather_fog(tint: str) -> ShapeList:
    """A cloud over three bars: fog, mist and haze."""
    return cloud(tint, 50.0, 42.0, 48.0) + [
        Line(palette.FG_BRIGHT, 20.0, 68.0, 80.0, 68.0, 6.0),
        Line(palette.FG_BRIGHT, 28.0, 79.0, 72.0, 79.0, 6.0),
        Line(palette.FG_BRIGHT, 22.0, 90.0, 66.0, 90.0, 6.0),
    ]


def _drops(tint: str, xs: Sequence[float], top: float = 66.0) -> ShapeList:
    """Rain: one long drop per x, all at the same height."""
    return [
        Poly.of(tint, [(x, top), (x + 7.0, top), (x + 3.5, top + 14.0)]) for x in xs
    ]


@glyph("weather-showers")
def weather_showers(tint: str) -> ShapeList:
    """A cloud with three drops under it."""
    return cloud(tint, 50.0, 40.0, 48.0) + _drops(
        palette.FG_BRIGHT, (30.0, 47.0, 64.0)
    )


@glyph("weather-showers-scattered")
def weather_showers_scattered(tint: str) -> ShapeList:
    """A cloud with two drops: rain in places rather than everywhere."""
    return cloud(tint, 50.0, 40.0, 48.0) + _drops(palette.FG_BRIGHT, (34.0, 58.0))


@glyph("weather-snow")
def weather_snow(tint: str) -> ShapeList:
    """A cloud with three flakes under it."""
    return cloud(tint, 50.0, 40.0, 48.0) + [
        Disc(palette.FG_BRIGHT, x, 74.0 + row * 14.0, 4.0)
        for row in range(2)
        for x in ((32.0, 50.0, 68.0) if row == 0 else (41.0, 59.0))
    ]


@glyph("weather-storm")
def weather_storm(tint: str) -> ShapeList:
    """A cloud with a bolt under it."""
    return cloud(tint, 50.0, 38.0, 48.0) + [
        Poly.of(
            palette.FG_BRIGHT,
            [
                (54.0, 62.0),
                (40.0, 82.0),
                (49.0, 82.0),
                (44.0, 96.0),
                (62.0, 74.0),
                (52.0, 74.0),
            ],
        )
    ]


@glyph("weather-severe-alert")
def weather_severe_alert(tint: str) -> ShapeList:
    """The warning triangle, for the weather that is dangerous rather than wet."""
    return dialog_warning(tint)
