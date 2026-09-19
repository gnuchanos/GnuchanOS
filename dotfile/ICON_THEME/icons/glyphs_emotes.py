"""Faces: the emotes a chat client and a mail reader draw inline.

An emote is the one family in this theme that does not take the tint of its
context. That is deliberate: an emote is read by its colour and its shape
together, the way an emoji is, and a purple face reads as a bruise. The disc is
therefore the same yellow everywhere, and the expression is carried by the eyes
and the mouth, which are the two tables below.

The tables exist so that twenty-four expressions are twenty-four short lines
rather than twenty-four drawings: the eyes and the mouth are the only things that
differ, and the shared disc is what makes the set look like one set in a chat
log rather than twenty-four icons that happen to be round.
"""

from __future__ import annotations

from . import palette
from .glyphs_base import ShapeList, glyph
from .primitives import Arc, Disc, Line, Poly, Rect

FACE_CENTRE = (50.0, 50.0)
FACE_RADIUS = 34.0


def _eyes(ink: str, style: str) -> ShapeList:
    """The eyes of a face, by name."""
    if style == "open":
        return [Disc(ink, 38.0, 42.0, 5.0), Disc(ink, 62.0, 42.0, 5.0)]
    if style == "wide":
        return [Disc(ink, 38.0, 42.0, 7.0), Disc(ink, 62.0, 42.0, 7.0)]
    if style == "wink":
        return [Disc(ink, 38.0, 42.0, 5.0), Line(ink, 55.0, 43.0, 68.0, 41.0, 4.0)]
    if style == "closed":
        return [
            Line(ink, 31.0, 41.0, 45.0, 44.0, 4.0),
            Line(ink, 55.0, 44.0, 69.0, 41.0, 4.0),
        ]
    if style == "sad":
        return [
            Line(ink, 31.0, 37.0, 45.0, 44.0, 4.0),
            Line(ink, 69.0, 37.0, 55.0, 44.0, 4.0),
        ]
    if style == "angry":
        return [
            Line(ink, 30.0, 34.0, 45.0, 45.0, 5.0),
            Line(ink, 70.0, 34.0, 55.0, 45.0, 5.0),
        ]
    if style == "tired":
        return [
            Line(ink, 31.0, 44.0, 45.0, 40.0, 4.0),
            Line(ink, 55.0, 40.0, 69.0, 44.0, 4.0),
        ]
    if style == "cool":
        return [
            Rect(ink, 24.0, 36.0, 24.0, 13.0, 4.0),
            Rect(ink, 52.0, 36.0, 24.0, 13.0, 4.0),
            Line(ink, 46.0, 40.0, 54.0, 40.0, 4.0),
        ]
    if style == "glasses":
        return [
            Arc(ink, 37.0, 42.0, 12.0, 0.0, 359.999, 4.0),
            Arc(ink, 63.0, 42.0, 12.0, 0.0, 359.999, 4.0),
            Line(ink, 49.0, 42.0, 51.0, 42.0, 4.0),
        ]
    if style == "masked":
        return [Rect(ink, 22.0, 34.0, 56.0, 15.0, 5.0)]
    raise KeyError(f"no eyes called {style!r}")


def _mouth(ink: str, style: str) -> ShapeList:
    """The mouth of a face, by name."""
    if style == "smile":
        return [Arc(ink, 50.0, 48.0, 16.0, 30.0, 120.0, 5.0)]
    if style == "grin":
        return [Arc(ink, 50.0, 44.0, 20.0, 20.0, 140.0, 6.0)]
    if style == "frown":
        return [Arc(ink, 50.0, 70.0, 16.0, 210.0, 120.0, 5.0)]
    if style == "flat":
        return [Line(ink, 38.0, 64.0, 62.0, 64.0, 5.0)]
    if style == "open":
        return [Disc(ink, 50.0, 64.0, 8.0)]
    if style == "yawn":
        return [
            Disc(ink, 50.0, 64.0, 11.0),
            Disc(palette.EMOTE_TONGUE, 50.0, 69.0, 6.0),
        ]
    if style == "smirk":
        return [Arc(ink, 46.0, 58.0, 14.0, 20.0, 90.0, 5.0)]
    if style == "kiss":
        return [Disc(ink, 50.0, 62.0, 7.0)]
    if style == "wavy":
        return [
            Arc(ink, 42.0, 62.0, 8.0, 200.0, 140.0, 4.0),
            Arc(ink, 58.0, 62.0, 8.0, 200.0, 140.0, 4.0),
        ]
    if style == "uncertain":
        return [
            Arc(ink, 44.0, 62.0, 12.0, 200.0, 100.0, 5.0),
            Line(ink, 58.0, 62.0, 66.0, 60.0, 4.0),
        ]
    if style == "tongue":
        return [
            Line(ink, 38.0, 62.0, 62.0, 62.0, 5.0),
            Disc(palette.EMOTE_TONGUE, 56.0, 70.0, 7.0),
        ]
    raise KeyError(f"no mouth called {style!r}")


def _face(tint: str, eyes: str, mouth: str, ink: str = palette.EMOTE_INK) -> ShapeList:
    """A face with the given eyes and mouth, drawn on the shared disc.

    ``tint`` is accepted and ignored: the glyph registry passes every renderer
    the tint of its context, and this family is the one that overrules it.
    """
    del tint
    return (
        [Disc(palette.EMOTE_FACE, FACE_CENTRE[0], FACE_CENTRE[1], FACE_RADIUS)]
        + _eyes(ink, eyes)
        + _mouth(ink, mouth)
    )


def _blush() -> ShapeList:
    """Two cheeks, shared by the expressions that need them."""
    return [
        Disc(palette.EMOTE_BLUSH, 26.0, 56.0, 7.0),
        Disc(palette.EMOTE_BLUSH, 74.0, 56.0, 7.0),
    ]


def _horns() -> ShapeList:
    """Two horns, drawn before the head so the head covers their roots."""
    return [
        Poly.of(palette.EMOTE_INK, [(30.0, 26.0), (24.0, 4.0), (44.0, 18.0)]),
        Poly.of(palette.EMOTE_INK, [(70.0, 26.0), (76.0, 4.0), (56.0, 18.0)]),
    ]


@glyph("face-smile")
def face_smile(tint: str) -> ShapeList:
    return _face(tint, "open", "smile")


@glyph("face-sad")
def face_sad(tint: str) -> ShapeList:
    return _face(tint, "sad", "frown")


@glyph("face-plain")
def face_plain(tint: str) -> ShapeList:
    return _face(tint, "open", "flat")


@glyph("face-wink")
def face_wink(tint: str) -> ShapeList:
    return _face(tint, "wink", "smile")


@glyph("face-laugh")
def face_laugh(tint: str) -> ShapeList:
    return _face(tint, "closed", "grin") + _blush()


@glyph("face-smile-big")
def face_smile_big(tint: str) -> ShapeList:
    return _face(tint, "open", "grin")


@glyph("face-surprise")
def face_surprise(tint: str) -> ShapeList:
    return _face(tint, "wide", "open")


@glyph("face-uncertain")
def face_uncertain(tint: str) -> ShapeList:
    return _face(tint, "sad", "uncertain")


@glyph("face-angel")
def face_angel(tint: str) -> ShapeList:
    """A smile with a halo, which is drawn first so the head covers its foot."""
    halo = Arc(palette.FG_BRIGHT, 50.0, 18.0, 15.0, 0.0, 359.999, 5.0)
    return [halo] + _face(tint, "open", "smile")


@glyph("face-angry")
def face_angry(tint: str) -> ShapeList:
    return _face(tint, "angry", "frown")


@glyph("face-cool")
def face_cool(tint: str) -> ShapeList:
    return _face(tint, "cool", "smile")


@glyph("face-crying")
def face_crying(tint: str) -> ShapeList:
    return _face(tint, "closed", "frown") + [
        Disc(palette.EMOTE_TONGUE, 30.0, 54.0, 5.0),
        Disc(palette.EMOTE_TONGUE, 70.0, 54.0, 5.0),
    ]


@glyph("face-devilish")
def face_devilish(tint: str) -> ShapeList:
    """A smirk with horns, drawn before the head so the head covers their roots."""
    return _horns() + _face(tint, "angry", "smirk")


@glyph("face-embarrassed")
def face_embarrassed(tint: str) -> ShapeList:
    return _face(tint, "closed", "flat") + _blush()


@glyph("face-kiss")
def face_kiss(tint: str) -> ShapeList:
    return _face(tint, "wink", "kiss")


@glyph("face-raspberry")
def face_raspberry(tint: str) -> ShapeList:
    return _face(tint, "wink", "tongue")


@glyph("face-sick")
def face_sick(tint: str) -> ShapeList:
    return _face(tint, "tired", "wavy") + [
        Disc(palette.EMOTE_TONGUE, 78.0, 30.0, 4.0),
        Disc(palette.EMOTE_TONGUE, 84.0, 42.0, 3.0),
    ]


@glyph("face-smirk")
def face_smirk(tint: str) -> ShapeList:
    return _face(tint, "wink", "smirk")


@glyph("face-tired")
def face_tired(tint: str) -> ShapeList:
    return _face(tint, "tired", "flat")


@glyph("face-worried")
def face_worried(tint: str) -> ShapeList:
    return _face(tint, "sad", "uncertain")


@glyph("face-glasses")
def face_glasses(tint: str) -> ShapeList:
    return _face(tint, "glasses", "smile")


@glyph("face-ninja")
def face_ninja(tint: str) -> ShapeList:
    return _face(tint, "masked", "flat")


@glyph("face-yawn")
def face_yawn(tint: str) -> ShapeList:
    return _face(tint, "closed", "yawn")


@glyph("face-monkey")
def face_monkey(tint: str) -> ShapeList:
    """A monkey: the shared face with ears and a muzzle, which is why it is not
    a row in the tables above — it is a different animal, not a different
    expression."""
    return [
        Disc(palette.EMOTE_FACE, 14.0, 46.0, 13.0),
        Disc(palette.EMOTE_FACE, 86.0, 46.0, 13.0),
        Disc(palette.EMOTE_FACE, 50.0, 50.0, 32.0),
        Disc(palette.FLAG_SAND, 50.0, 62.0, 18.0),
        Disc(palette.EMOTE_INK, 38.0, 40.0, 5.0),
        Disc(palette.EMOTE_INK, 62.0, 40.0, 5.0),
        Disc(palette.EMOTE_INK, 43.0, 58.0, 2.5),
        Disc(palette.EMOTE_INK, 57.0, 58.0, 2.5),
        Arc(palette.EMOTE_INK, 50.0, 60.0, 8.0, 30.0, 120.0, 4.0),
    ]
