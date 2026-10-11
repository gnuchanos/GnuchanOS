"""Pictograms for hardware: the devices a desktop has to draw.

Drives, media and peripherals are the icons a file manager shows in its device
list and a settings panel shows in its sidebar, so they share one visual family:
a dark body outline with an accent-coloured face. The shapes stay simple
because they are shown at sixteen to twenty four pixels most of the time.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, glyph
from .primitives import Arc, Disc, Line, Poly, Rect


def _screen(fill: str, body: str, width: float, height: float, top: float = 18.0) -> ShapeList:
    """A device body with a lit face inside it."""
    left = 50.0 - width / 2.0
    return [
        Rect(body, left, top, width, height, 8.0),
        Rect(fill, left + 6.0, top + 6.0, width - 12.0, height - 12.0, 2.0),
    ]


@glyph("computer")
def computer(tint: str) -> ShapeList:
    return [
        Rect(tint, 12.0, 16.0, 76.0, 50.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 17.0, 21.0, 66.0, 40.0, 3.0),
        Rect(tint, 40.0, 66.0, 20.0, 10.0, 2.0),
        Rect(tint, 24.0, 76.0, 52.0, 8.0, 3.0),
    ]


@glyph("laptop")
def laptop(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(22.0, 20.0), (78.0, 20.0), (82.0, 62.0), (18.0, 62.0)]),
        Rect(palette.BG_DARKEST + "88", 25.0, 24.0, 50.0, 34.0, 2.0),
        Poly.of(tint, [(10.0, 66.0), (90.0, 66.0), (96.0, 78.0), (4.0, 78.0)]),
    ]


@glyph("phone")
def phone(tint: str) -> ShapeList:
    return [
        Rect(tint, 30.0, 10.0, 40.0, 80.0, 9.0),
        Rect(palette.BG_DARKEST + "88", 35.0, 20.0, 30.0, 54.0, 3.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 82.0, 4.0),
    ]


@glyph("tablet")
def tablet(tint: str) -> ShapeList:
    return [
        Rect(tint, 20.0, 10.0, 60.0, 80.0, 9.0),
        Rect(palette.BG_DARKEST + "88", 26.0, 20.0, 48.0, 58.0, 3.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 84.0, 4.0),
    ]


@glyph("keyboard")
def keyboard(tint: str) -> ShapeList:
    shapes: ShapeList = [
        Rect(tint, 8.0, 26.0, 84.0, 48.0, 6.0),
    ]
    for row in range(3):
        for column in range(4):
            shapes.append(
                Rect(
                    palette.BG_DARKEST + "88",
                    14.0 + column * 20.0,
                    32.0 + row * 12.0,
                    16.0, 8.0, 2.0,
                )
            )
    return shapes


@glyph("mouse")
def mouse(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(30.0, 22.0), (70.0, 22.0), (78.0, 52.0), (70.0, 82.0), (30.0, 82.0), (22.0, 52.0)]),
        Line(palette.BG_DARKEST + "88", 50.0, 22.0, 50.0, 46.0, 4.0),
    ]


@glyph("camera")
def camera(tint: str) -> ShapeList:
    """A compact camera: a body, a viewfinder hump and a lens."""
    return [
        Poly.of(tint, [(34.0, 30.0), (42.0, 20.0), (58.0, 20.0), (66.0, 30.0)]),
        Rect(tint, 10.0, 30.0, 80.0, 48.0, 8.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 54.0, 16.0),
        Disc(tint, 50.0, 54.0, 9.0),
        Disc(palette.BG_DARKEST + "88", 76.0, 38.0, 4.0),
    ]


@glyph("camera-web")
def camera_web(tint: str) -> ShapeList:
    return [
        Disc(tint, 50.0, 46.0, 24.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 46.0, 12.0),
        Rect(tint, 30.0, 70.0, 40.0, 10.0, 3.0),
        Arc(tint, 50.0, 78.0, 16.0, 0.0, 180.0, 5.0),
    ]


@glyph("video-display")
def video_display(tint: str) -> ShapeList:
    return computer(tint)


@glyph("tv")
def tv(tint: str) -> ShapeList:
    return [
        Rect(tint, 8.0, 24.0, 84.0, 52.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 14.0, 30.0, 72.0, 40.0, 3.0),
        Rect(tint, 34.0, 78.0, 32.0, 8.0, 3.0),
    ]


@glyph("audio-headphones")
def audio_headphones(tint: str) -> ShapeList:
    return [
        Arc(tint, 50.0, 50.0, 34.0, 180.0, 180.0, 10.0),
        Rect(tint, 8.0, 48.0, 18.0, 30.0, 5.0),
        Rect(tint, 74.0, 48.0, 18.0, 30.0, 5.0),
    ]


@glyph("audio-speakers")
def audio_speakers(tint: str) -> ShapeList:
    return [
        Rect(tint, 28.0, 12.0, 44.0, 76.0, 8.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 36.0, 10.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 66.0, 16.0),
    ]


@glyph("scanner")
def scanner(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(14.0, 30.0), (86.0, 42.0), (86.0, 54.0), (14.0, 42.0)]),
        Rect(tint, 20.0, 58.0, 60.0, 26.0, 4.0),
        Rect(palette.FG_BRIGHT, 26.0, 64.0, 48.0, 14.0, 2.0),
    ]


@glyph("drive-harddisk")
def drive_harddisk(tint: str) -> ShapeList:
    return [
        Rect(tint, 16.0, 20.0, 68.0, 60.0, 6.0),
        Line(palette.BG_DARKEST + "77", 16.0, 44.0, 84.0, 44.0, 4.0),
        Disc(palette.FG_BRIGHT, 30.0, 32.0, 4.0),
        Rect(palette.FG_BRIGHT, 26.0, 56.0, 34.0, 6.0, 2.0),
    ]


@glyph("drive-ssd")
def drive_ssd(tint: str) -> ShapeList:
    return [
        Rect(tint, 14.0, 26.0, 72.0, 44.0, 5.0),
        Rect(palette.FG_BRIGHT, 22.0, 34.0, 20.0, 12.0, 2.0),
        Rect(palette.FG_BRIGHT, 46.0, 34.0, 32.0, 6.0, 2.0),
        Rect(palette.FG_BRIGHT, 46.0, 44.0, 26.0, 6.0, 2.0),
        Rect(palette.FG_BRIGHT, 22.0, 54.0, 56.0, 6.0, 2.0),
    ]


@glyph("drive-optical")
def drive_optical(tint: str) -> ShapeList:
    return _disc_media(tint)


@glyph("media-optical")
def media_optical(tint: str) -> ShapeList:
    return _disc_media(tint)


def _disc_media(tint: str) -> ShapeList:
    return [
        Disc(tint, 50.0, 50.0, 38.0),
        Disc(palette.BG_DARKEST + "66", 50.0, 50.0, 12.0),
        Arc(palette.FG_BRIGHT, 50.0, 50.0, 26.0, 200.0, 90.0, 5.0),
    ]


@glyph("drive-removable-media")
def drive_removable_media(tint: str) -> ShapeList:
    return [
        Rect(tint, 18.0, 22.0, 64.0, 56.0, 6.0),
        Poly.of(tint, [(18.0, 54.0), (82.0, 54.0), (82.0, 70.0), (18.0, 70.0)]),
        Rect(palette.FG_BRIGHT, 30.0, 32.0, 40.0, 14.0, 2.0),
    ]


@glyph("media-flash")
def media_flash(tint: str) -> ShapeList:
    return [
        Rect(tint, 30.0, 12.0, 40.0, 76.0, 5.0),
        Poly.of(palette.FG_BRIGHT, [(42.0, 24.0), (58.0, 24.0), (52.0, 44.0), (62.0, 44.0), (44.0, 72.0), (50.0, 50.0), (40.0, 50.0)]),
        Line(palette.BG_DARKEST + "77", 30.0, 26.0, 70.0, 26.0, 3.0),
    ]


@glyph("media-floppy")
def media_floppy(tint: str) -> ShapeList:
    return [
        Rect(tint, 16.0, 16.0, 68.0, 68.0, 4.0),
        Rect(palette.FG_BRIGHT, 30.0, 16.0, 34.0, 20.0, 3.0),
        Rect(palette.FG_BRIGHT, 26.0, 52.0, 48.0, 32.0, 3.0),
        Rect(tint, 38.0, 30.0, 12.0, 10.0, 2.0),
    ]


@glyph("usb")
def usb(tint: str) -> ShapeList:
    return [
        Line(tint, 50.0, 84.0, 50.0, 20.0, 6.0),
        Poly.of(tint, [(50.0, 12.0), (60.0, 30.0), (40.0, 30.0)]),
        Disc(tint, 50.0, 84.0, 8.0),
        Disc(tint, 26.0, 54.0, 7.0),
        Line(tint, 26.0, 54.0, 44.0, 54.0, 5.0),
        Poly.of(tint, [(70.0, 62.0), (84.0, 62.0), (84.0, 50.0), (70.0, 50.0)]),
    ]


@glyph("media-removable")
def media_removable(tint: str) -> ShapeList:
    return [
        Poly.of(tint, [(38.0, 12.0), (62.0, 12.0), (70.0, 40.0), (30.0, 40.0)]),
        Rect(tint, 24.0, 40.0, 52.0, 46.0, 5.0),
        Rect(palette.FG_BRIGHT, 32.0, 50.0, 36.0, 26.0, 3.0),
    ]


@glyph("cpu")
def cpu(tint: str) -> ShapeList:
    shapes: ShapeList = [
        Rect(tint, 24.0, 24.0, 52.0, 52.0, 6.0),
        Rect(palette.FG_BRIGHT, 34.0, 34.0, 32.0, 32.0, 3.0),
    ]
    for index in range(3):
        offset = 34.0 + index * 16.0
        shapes.append(Rect(tint, offset, 12.0, 8.0, 12.0, 2.0))
        shapes.append(Rect(tint, offset, 76.0, 8.0, 12.0, 2.0))
        shapes.append(Rect(tint, 12.0, offset, 12.0, 8.0, 2.0))
        shapes.append(Rect(tint, 76.0, offset, 12.0, 8.0, 2.0))
    return shapes


@glyph("memory")
def memory(tint: str) -> ShapeList:
    return [
        Rect(tint, 10.0, 34.0, 80.0, 32.0, 3.0),
        Rect(palette.FG_BRIGHT, 16.0, 40.0, 16.0, 20.0, 2.0),
        Rect(palette.FG_BRIGHT, 38.0, 40.0, 16.0, 20.0, 2.0),
        Rect(palette.FG_BRIGHT, 60.0, 40.0, 16.0, 20.0, 2.0),
        Line(tint, 26.0, 26.0, 26.0, 34.0, 4.0),
        Line(tint, 50.0, 26.0, 50.0, 34.0, 4.0),
        Line(tint, 74.0, 26.0, 74.0, 34.0, 4.0),
    ]


@glyph("input-gaming")
def input_gaming(tint: str) -> ShapeList:
    from .glyphs_apps import applications_games

    return applications_games(tint)


@glyph("input-mouse")
def input_mouse(tint: str) -> ShapeList:
    return mouse(tint)


@glyph("input-keyboard")
def input_keyboard(tint: str) -> ShapeList:
    return keyboard(tint)


@glyph("input-tablet")
def input_tablet(tint: str) -> ShapeList:
    return [
        Rect(tint, 12.0, 60.0, 76.0, 22.0, 4.0),
        Line(tint, 50.0, 12.0, 50.0, 54.0, 7.0),
        Poly.of(tint, [(50.0, 54.0), (42.0, 44.0), (58.0, 44.0)]),
    ]


@glyph("printer-network")
def printer_network(tint: str) -> ShapeList:
    from .glyphs_actions import printer

    return printer(tint) + [
        Arc(tint, 76.0, 24.0, 10.0, 200.0, 140.0, 4.0),
        Disc(tint, 76.0, 28.0, 3.0),
    ]


# --- the devices the specification names that had no picture yet ---------------
# Every one of these is a name in the Icon Naming Specification with no artwork
# behind it, so the theme fell back to hicolor for it. They are drawn in the
# same family as the devices above — a body in the tint, a lit face in
# ``FG_BRIGHT`` — because a device list that mixes two drawing styles reads as
# two device lists.


@glyph("media-tape")
def media_tape(tint: str) -> ShapeList:
    """A cassette: a shell with two reels and a window between them."""
    return [
        Rect(tint, 10.0, 26.0, 80.0, 48.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 32.0, 50.0, 13.0),
        Disc(palette.BG_DARKEST + "88", 68.0, 50.0, 13.0),
        Disc(palette.FG_BRIGHT, 32.0, 50.0, 5.0),
        Disc(palette.FG_BRIGHT, 68.0, 50.0, 5.0),
        Rect(palette.FG_BRIGHT, 24.0, 68.0, 52.0, 6.0, 2.0),
    ]


@glyph("audio-card")
def audio_card(tint: str) -> ShapeList:
    """A sound card: a bracket with a rounded jack and two contacts."""
    return [
        Rect(tint, 12.0, 30.0, 76.0, 40.0, 5.0),
        Disc(palette.BG_DARKEST + "88", 34.0, 50.0, 11.0),
        Rect(palette.FG_BRIGHT, 56.0, 40.0, 22.0, 8.0, 3.0),
        Rect(palette.FG_BRIGHT, 56.0, 54.0, 22.0, 8.0, 3.0),
        Poly.of(tint, [(12.0, 22.0), (20.0, 22.0), (20.0, 30.0), (12.0, 30.0)]),
        Poly.of(tint, [(80.0, 22.0), (88.0, 22.0), (88.0, 30.0), (80.0, 30.0)]),
    ]


@glyph("camera-video")
def camera_video(tint: str) -> ShapeList:
    """A movie camera: a body, a lens and the reels on top."""
    return [
        Disc(tint, 34.0, 24.0, 13.0),
        Disc(tint, 66.0, 24.0, 13.0),
        Disc(palette.BG_DARKEST + "88", 34.0, 24.0, 5.0),
        Disc(palette.BG_DARKEST + "88", 66.0, 24.0, 5.0),
        Rect(tint, 12.0, 40.0, 76.0, 44.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 34.0, 62.0, 14.0),
        Disc(tint, 34.0, 62.0, 7.0),
        Rect(palette.FG_BRIGHT, 56.0, 50.0, 26.0, 8.0, 3.0),
        Rect(palette.FG_BRIGHT, 56.0, 64.0, 18.0, 8.0, 3.0),
    ]


@glyph("video-projector")
def video_projector(tint: str) -> ShapeList:
    """A projector: a body, its lens, and the beam leaving it."""
    return [
        Poly.of(tint + "55", [(58.0, 42.0), (96.0, 20.0), (96.0, 64.0), (58.0, 58.0)]),
        Rect(tint, 8.0, 36.0, 52.0, 40.0, 5.0),
        Disc(palette.BG_DARKEST + "88", 22.0, 56.0, 11.0),
        Disc(tint, 22.0, 56.0, 5.0),
        Disc(palette.FG_BRIGHT, 48.0, 46.0, 4.0),
    ]


@glyph("pda")
def pda(tint: str) -> ShapeList:
    """A palmtop: a body with a screen, and the buttons under it."""
    return [
        Rect(tint, 26.0, 8.0, 48.0, 84.0, 7.0),
        Rect(palette.BG_DARKEST + "88", 32.0, 16.0, 36.0, 46.0, 2.0),
        Disc(palette.BG_DARKEST + "88", 38.0, 74.0, 4.0),
        Disc(palette.BG_DARKEST + "88", 50.0, 74.0, 4.0),
        Disc(palette.BG_DARKEST + "88", 62.0, 74.0, 4.0),
    ]


@glyph("ipod")
def ipod(tint: str) -> ShapeList:
    """A media player: a body, a screen and the wheel under it."""
    return [
        Rect(tint, 28.0, 8.0, 44.0, 84.0, 8.0),
        Rect(palette.BG_DARKEST + "88", 34.0, 14.0, 32.0, 30.0, 2.0),
        Arc(palette.BG_DARKEST + "88", 50.0, 66.0, 15.0, 0.0, 359.999, 6.0),
        Disc(palette.FG_BRIGHT, 50.0, 66.0, 5.0),
    ]


@glyph("input-dialpad")
def input_dialpad(tint: str) -> ShapeList:
    """Twelve keys in a grid: the keypad a dialer or a PIN prompt draws."""
    shapes: ShapeList = []
    for row in range(4):
        for column in range(3):
            shapes.append(
                Rect(
                    tint,
                    22.0 + column * 20.0,
                    14.0 + row * 20.0,
                    16.0, 14.0, 3.0,
                )
            )
    return shapes


@glyph("modem")
def modem(tint: str) -> ShapeList:
    """A modem: a box with status lights and the line coming out of it."""
    return [
        Arc(tint, 76.0, 26.0, 12.0, 200.0, 140.0, 4.0),
        Disc(tint, 76.0, 32.0, 4.0),
        Rect(tint, 10.0, 40.0, 80.0, 40.0, 5.0),
        Disc(palette.FG_BRIGHT, 24.0, 52.0, 5.0),
        Disc(palette.FG_BRIGHT, 40.0, 52.0, 5.0),
        Rect(palette.FG_BRIGHT, 56.0, 48.0, 26.0, 8.0, 3.0),
    ]


@glyph("ups")
def ups(tint: str) -> ShapeList:
    """An uninterruptible supply: a battery box with a plug beside it."""
    return [
        Rect(tint, 8.0, 34.0, 54.0, 40.0, 5.0),
        Poly.of(
            palette.FG_BRIGHT,
            [(28.0, 42.0), (48.0, 42.0), (38.0, 54.0), (50.0, 54.0), (30.0, 70.0), (38.0, 58.0), (26.0, 58.0)],
        ),
        Line(tint, 68.0, 30.0, 68.0, 46.0, 5.0),
        Line(tint, 80.0, 30.0, 80.0, 46.0, 5.0),
        Rect(tint, 64.0, 46.0, 20.0, 10.0, 3.0),
        Line(tint, 74.0, 56.0, 74.0, 72.0, 5.0),
    ]


@glyph("network-router")
def network_router(tint: str) -> ShapeList:
    """A router: a box with two antennas and a row of port lights."""
    return [
        Line(tint, 32.0, 24.0, 24.0, 8.0, 5.0),
        Line(tint, 68.0, 24.0, 76.0, 8.0, 5.0),
        Rect(tint, 10.0, 40.0, 80.0, 34.0, 6.0),
        Disc(palette.FG_BRIGHT, 24.0, 52.0, 4.0),
        Disc(palette.FG_BRIGHT, 36.0, 52.0, 4.0),
        Disc(palette.FG_BRIGHT, 48.0, 52.0, 4.0),
        Rect(palette.FG_BRIGHT, 62.0, 60.0, 20.0, 8.0, 3.0),
    ]


@glyph("smartcard")
def smartcard(tint: str) -> ShapeList:
    """A chip card: a card with the contact pad on it."""
    return [
        Rect(tint, 8.0, 24.0, 84.0, 52.0, 5.0),
        Rect(palette.FG_BRIGHT, 22.0, 38.0, 26.0, 24.0, 3.0),
        Line(tint, 22.0, 50.0, 48.0, 50.0, 3.0),
        Line(tint, 35.0, 38.0, 35.0, 62.0, 3.0),
        Rect(palette.FG_BRIGHT, 58.0, 40.0, 26.0, 6.0, 2.0),
        Rect(palette.FG_BRIGHT, 58.0, 52.0, 18.0, 6.0, 2.0),
    ]


@glyph("usb-drive")
def usb_drive(tint: str) -> ShapeList:
    """A flash stick: a connector, a cap and a body."""
    return [
        Rect(tint, 42.0, 8.0, 30.0, 24.0, 3.0),
        Rect(palette.FG_BRIGHT, 46.0, 12.0, 22.0, 16.0, 2.0),
        Rect(tint, 36.0, 32.0, 42.0, 60.0, 6.0),
        Disc(palette.BG_DARKEST + "88", 57.0, 58.0, 7.0),
    ]


@glyph("usb-hub")
def usb_hub(tint: str) -> ShapeList:
    """A hub: a box with four ports and the line into the machine."""
    return [
        Line(tint, 50.0, 34.0, 50.0, 14.0, 6.0),
        Rect(tint, 20.0, 34.0, 60.0, 46.0, 5.0),
        Rect(palette.FG_BRIGHT, 28.0, 44.0, 14.0, 12.0, 2.0),
        Rect(palette.FG_BRIGHT, 58.0, 44.0, 14.0, 12.0, 2.0),
        Rect(palette.FG_BRIGHT, 28.0, 62.0, 44.0, 8.0, 2.0),
    ]


@glyph("computer-server")
def computer_server(tint: str) -> ShapeList:
    """A rack machine: a tower of slabs, each with its own status light."""
    shapes: ShapeList = []
    for index in range(4):
        top = 10.0 + index * 21.0
        shapes.append(Rect(tint, 18.0, top, 64.0, 17.0, 3.0))
        shapes.append(Disc(palette.FG_BRIGHT, 30.0, top + 8.5, 4.0))
        shapes.append(Rect(palette.FG_BRIGHT, 42.0, top + 5.0, 30.0, 7.0, 2.0))
    return shapes


@glyph("computer-workstation")
def computer_workstation(tint: str) -> ShapeList:
    """A workstation: a display over a wide base."""
    return [
        Rect(tint, 10.0, 12.0, 80.0, 52.0, 6.0),
        Rect(palette.BG_DARKEST + "88", 16.0, 18.0, 68.0, 40.0, 3.0),
        Rect(tint, 30.0, 64.0, 40.0, 8.0, 2.0),
        Rect(tint, 16.0, 72.0, 68.0, 14.0, 3.0),
        Line(palette.BG_DARKEST + "88", 34.0, 79.0, 66.0, 79.0, 3.0),
    ]
