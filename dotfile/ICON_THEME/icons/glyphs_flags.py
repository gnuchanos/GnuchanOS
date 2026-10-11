"""Flags for the ``International`` context, generated from a layout table.

A flag is the one icon family where drawing each one by hand would be a mistake:
there are two hundred of them, they are all bands and crosses, and a hundred
hand-placed rectangles is a hundred chances to put a band in the wrong place.
So the flags live in a table of layouts — horizontal bands, vertical bands,
Nordic cross, centred cross, a canton over stripes, a crescent and a star — and
each country is a row naming its layout and its colours.

The flags are approximations on purpose. A flag that carries a coat of arms is
drawn as its bands with the arms left out, because at sixteen pixels the arms
are mud anyway and claiming to have traced them would be a lie. What the icon
has to do is be recognisable in a language menu, and bands do that.

Every flag is drawn over a one unit wider rim in the theme's border colour, so a
white field does not disappear against a white menu.
"""

from __future__ import annotations

from typing import Sequence

from . import palette
from .glyphs_base import GLYPHS, ShapeList, crescent, star_shape
from .primitives import Disc, Poly, Rect

#: The area a flag occupies: 76 by 48 units, which is a flag's 3:2 ratio.
FLAG_BOX = (12.0, 26.0, 76.0, 48.0)

#: How far a later band reaches back under the one before it. Two opaque
#: rectangles that merely touch leave an antialiased seam where their coverages
#: meet; overlapping them hides the boundary inside the upper band.
OVERLAP = 1.0


def _rim() -> ShapeList:
    """The border every flag is drawn on, so white fields stay visible."""
    x, y, width, height = FLAG_BOX
    return [Rect(palette.BORDER, x - 3.0, y - 3.0, width + 6.0, height + 6.0, 4.0)]


def _h_bands(colours: Sequence[str]) -> ShapeList:
    """Equal horizontal bands, top to bottom, for any number of colours."""
    x, y, width, height = FLAG_BOX
    count = len(colours)
    step = height / count
    shapes: ShapeList = []
    for index, colour in enumerate(colours):
        top = y if index == 0 else y + index * step - OVERLAP
        bottom = y + height if index == count - 1 else y + (index + 1) * step
        shapes.append(Rect(colour, x, top, width, bottom - top))
    return shapes


def _v_bands(colours: Sequence[str]) -> ShapeList:
    """Equal vertical bands, left to right, for any number of colours."""
    x, y, width, height = FLAG_BOX
    count = len(colours)
    step = width / count
    shapes: ShapeList = []
    for index, colour in enumerate(colours):
        left = x if index == 0 else x + index * step - OVERLAP
        right = x + width if index == count - 1 else x + (index + 1) * step
        shapes.append(Rect(colour, left, y, right - left, height))
    return shapes


def _field(colour: str) -> ShapeList:
    """A plain field, the base of every flag that is not all bands."""
    x, y, width, height = FLAG_BOX
    return [Rect(colour, x, y, width, height)]


def _nordic(field: str, cross: str) -> ShapeList:
    """A Nordic cross: the upright bar sits towards the hoist, not in the middle.

    That off-centre bar is the whole reason the Scandinavian flags read as a
    family, so it is a layout of its own rather than the centred cross with a
    different argument.
    """
    x, y, width, height = FLAG_BOX
    bar = 12.0
    centre_x = x + width * 0.34
    centre_y = y + height / 2.0
    return _field(field) + [
        Rect(cross, centre_x - bar / 2.0 - OVERLAP, y, bar + 2.0 * OVERLAP, height),
        Rect(cross, x, centre_y - bar / 2.0 - OVERLAP, width, bar + 2.0 * OVERLAP),
    ]


def _cross(field: str, cross: str) -> ShapeList:
    """A centred cross, the Swiss and Georgian construction."""
    x, y, width, height = FLAG_BOX
    bar = 14.0
    centre_x = x + width / 2.0
    centre_y = y + height / 2.0
    return _field(field) + [
        Rect(cross, centre_x - bar / 2.0 - OVERLAP, y, bar + 2.0 * OVERLAP, height),
        Rect(cross, x, centre_y - bar / 2.0 - OVERLAP, width, bar + 2.0 * OVERLAP),
    ]


def _centre() -> tuple[float, float]:
    x, y, width, height = FLAG_BOX
    return x + width / 2.0, y + height / 2.0


def _with_disc(shapes: ShapeList, colour: str, radius: float = 14.0) -> ShapeList:
    """Add a disc at the centre, for the flags that carry a sun or a moon."""
    cx, cy = _centre()
    return shapes + [Disc(colour, cx, cy, radius)]


def _moon(field: str, moon: str, star: str) -> ShapeList:
    """A crescent and a star: the Turkish and Tunisian construction."""
    cx, cy = _centre()
    return [
        *_field(field),
        *crescent(moon, field, cx - 5.0, cy, 13.0, 0.8),
        star_shape(star, cx + 11.0, cy, 7.0, 3.0),
    ]


def _stripes_canton(first: str, second: str, canton: str, mark: str) -> ShapeList:
    """Thirteen stripes under a canton: the United States and its relatives."""
    x, y, width, height = FLAG_BOX
    stripes = [first if index % 2 == 0 else second for index in range(13)]
    canton_width = width * 0.42
    canton_height = height * 7.0 / 13.0
    return _h_bands(stripes) + [
        Rect(canton, x, y, canton_width + OVERLAP, canton_height + OVERLAP),
        Disc(mark, x + canton_width * 0.25, y + canton_height * 0.3, 2.5),
        Disc(mark, x + canton_width * 0.55, y + canton_height * 0.3, 2.5),
        Disc(mark, x + canton_width * 0.4, y + canton_height * 0.68, 2.5),
    ]


def _union(field: str, cross: str, diagonal: str) -> ShapeList:
    """A union flag: the diagonals, then the cross over them.

    Drawn as the diagonals and the two bars rather than traced, which is close
    enough at the sizes an icon is seen and is honest about not being the exact
    counterchanged construction.
    """
    x, y, width, height = FLAG_BOX
    return [
        *_field(field),
        Poly.of(
            diagonal,
            [
                (x, y),
                (x + width * 0.52, y),
                (x + width, y + height * 0.58),
                (x + width, y + height),
                (x + width * 0.48, y + height),
                (x, y + height * 0.42),
            ],
        ),
        Rect(cross, x, y + height / 2.0 - 8.0, width, 16.0),
        Rect(cross, x + width / 2.0 - 8.0, y, 16.0, height),
    ]


def _hoist(triangle: str, colours: Sequence[str]) -> ShapeList:
    """A triangle at the hoist over bands: Czechia and the Philippines."""
    x, y, width, height = FLAG_BOX
    return _h_bands(colours) + [
        Poly.of(
            triangle,
            [(x, y), (x + width * 0.44, y + height / 2.0), (x, y + height)],
        )
    ]


def _diamond(field: str, diamond: str, disc: str) -> ShapeList:
    """A lozenge with a disc inside it, the Brazilian construction."""
    x, y, width, height = FLAG_BOX
    cx, cy = _centre()
    return [
        *_field(field),
        Poly.of(
            diamond,
            [
                (cx, y + 3.0),
                (x + width - 3.0, cy),
                (cx, y + height - 3.0),
                (x + 3.0, cy),
            ],
        ),
        Disc(disc, cx, cy, 12.0),
    ]


def _barred(field: str, bar: str) -> ShapeList:
    """A field with two bars near its edges: Israel's construction."""
    x, y, width, height = FLAG_BOX
    return _field(field) + [
        Rect(bar, x, y + 7.0, width, 6.0),
        Rect(bar, x, y + height - 13.0, width, 6.0),
    ]


def _partitioned(field: str, wedge: str) -> ShapeList:
    """A vertical wedge at the hoist, for the flags whose charge is a field split."""
    x, y, width, height = FLAG_BOX
    return _field(field) + [
        Poly.of(
            wedge,
            [
                (x, y),
                (x + width * 0.36, y + height * 0.34),
                (x + width * 0.36, y + height * 0.66),
                (x, y + height),
            ],
        )
    ]


def _union_of_bands(colours: Sequence[str]) -> ShapeList:
    """Bands with a dark wedge at the hoist, South Africa's construction simplified.

    The real flag carries a green Y and a counterchanged border as well; at the
    size an icon is read, the bands and the wedge are what identify it.
    """
    x, y, width, height = FLAG_BOX
    return _h_bands(colours) + [
        Poly.of(
            palette.FLAG_BLACK,
            [(x, y), (x + width * 0.40, y + height / 2.0), (x, y + height)],
        )
    ]


def _field_star(field: str, star: str) -> ShapeList:
    """A single star on a plain field, for the flags whose charge is one star."""
    cx, cy = _centre()
    return _field(field) + [star_shape(star, cx, cy, 17.0, 7.0)]


# Country code to layout and colours. The first element names the construction
# and the rest are the colours in the order that construction reads them, so a
# row can be checked against the flag it claims to draw without opening a second
# file. Codes are ISO 3166-1 alpha-2, which is what a language or country menu
# asks for, plus ``eu`` and ``un`` for the two non-country flags that appear in
# the same menus.
FLAGS: dict[str, tuple[object, ...]] = {
    # --- Europe --------------------------------------------------------------
    "ad": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_CRIMSON),
    "al": ("field", palette.FLAG_RED),
    "am": ("h", palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_ORANGE),
    "at": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_RED),
    "az": ("h", palette.FLAG_AZURE, palette.FLAG_RED, palette.FLAG_GREEN),
    "ba": ("hoist", palette.FLAG_GOLD, (palette.FLAG_AZURE,)),
    "be": ("v", palette.FLAG_BLACK, palette.FLAG_YELLOW, palette.FLAG_RED),
    "bg": ("h", palette.FLAG_WHITE, palette.FLAG_GREEN, palette.FLAG_RED),
    "by": ("h", palette.FLAG_RED, palette.FLAG_GREEN, palette.FLAG_WHITE),
    "ch": ("cross", palette.FLAG_RED, palette.FLAG_WHITE),
    "cy": ("circle", palette.FLAG_WHITE, palette.FLAG_ORANGE),
    "cz": ("hoist", palette.FLAG_AZURE, (palette.FLAG_WHITE, palette.FLAG_RED)),
    "de": ("h", palette.FLAG_BLACK, palette.FLAG_RED, palette.FLAG_GOLD),
    "dk": ("nordic", palette.FLAG_RED, palette.FLAG_WHITE),
    "ee": ("h", palette.FLAG_AZURE, palette.FLAG_BLACK, palette.FLAG_WHITE),
    "es": ("h", palette.FLAG_RED, palette.FLAG_GOLD, palette.FLAG_RED),
    "eu": ("circle", palette.FLAG_AZURE, palette.FLAG_GOLD),
    "fi": ("nordic", palette.FLAG_WHITE, palette.FLAG_AZURE),
    "fr": ("v", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_RED),
    "gb": ("union", palette.FLAG_NAVY, palette.FLAG_WHITE, palette.FLAG_RED),
    "ge": ("cross", palette.FLAG_WHITE, palette.FLAG_RED),
    "gr": (
        "h",
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
    ),
    "hr": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_NAVY),
    "hu": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_GREEN),
    "ie": ("v", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_ORANGE),
    "is": ("nordic", palette.FLAG_AZURE, palette.FLAG_WHITE),
    "it": ("v", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED),
    "kz": ("circle", palette.FLAG_AZURE, palette.FLAG_GOLD),
    "li": ("h", palette.FLAG_AZURE, palette.FLAG_RED),
    "lt": ("h", palette.FLAG_GOLD, palette.FLAG_GREEN, palette.FLAG_RED),
    "lu": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "lv": ("h", palette.FLAG_MAROON, palette.FLAG_WHITE, palette.FLAG_MAROON),
    "mc": ("h", palette.FLAG_RED, palette.FLAG_WHITE),
    "md": ("v", palette.FLAG_NAVY, palette.FLAG_GOLD, palette.FLAG_CRIMSON),
    "me": ("h", palette.FLAG_MAROON, palette.FLAG_GOLD),
    "mk": ("circle", palette.FLAG_RED, palette.FLAG_GOLD),
    "mt": ("v", palette.FLAG_WHITE, palette.FLAG_RED),
    "nl": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "no": ("nordic", palette.FLAG_RED, palette.FLAG_WHITE),
    "pl": ("h", palette.FLAG_WHITE, palette.FLAG_RED),
    "pt": ("v_disc", palette.FLAG_GREEN, palette.FLAG_RED, palette.FLAG_GOLD),
    "ro": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_CRIMSON),
    "rs": ("h", palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_WHITE),
    "ru": ("h", palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_RED),
    "se": ("nordic", palette.FLAG_AZURE, palette.FLAG_GOLD),
    "si": ("h", palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_RED),
    "sk": ("h", palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_RED),
    "sm": ("h", palette.FLAG_WHITE, palette.FLAG_AZURE),
    "ua": ("h", palette.FLAG_AZURE, palette.FLAG_GOLD),
    "un": ("circle", palette.FLAG_SKY, palette.FLAG_WHITE),
    "va": ("h", palette.FLAG_GOLD, palette.FLAG_WHITE),
    "xk": ("circle", palette.FLAG_AZURE, palette.FLAG_GOLD),
    # --- Africa --------------------------------------------------------------
    "ao": ("h", palette.FLAG_RED, palette.FLAG_BLACK),
    "bf": ("h_star", palette.FLAG_RED, palette.FLAG_GREEN, palette.FLAG_GOLD),
    "bi": ("cross", palette.FLAG_RED, palette.FLAG_WHITE),
    "bj": ("hoist", palette.FLAG_GREEN, (palette.FLAG_GOLD, palette.FLAG_RED)),
    "bw": (
        "h",
        palette.FLAG_SKY,
        palette.FLAG_WHITE,
        palette.FLAG_BLACK,
        palette.FLAG_WHITE,
        palette.FLAG_SKY,
    ),
    "cd": ("circle", palette.FLAG_SKY, palette.FLAG_GOLD),
    "cf": ("h", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_GREEN, palette.FLAG_GOLD),
    "cg": ("h", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_RED),
    "ci": ("v", palette.FLAG_ORANGE, palette.FLAG_WHITE, palette.FLAG_GREEN),
    "cm": ("v_star", palette.FLAG_GREEN, palette.FLAG_RED, palette.FLAG_GOLD, palette.FLAG_GOLD),
    "cv": (
        "h",
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
    ),
    "dj": ("h_star", palette.FLAG_SKY, palette.FLAG_GREEN, palette.FLAG_WHITE),
    "dz": ("moon", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED),
    "eg": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_BLACK),
    "eh": ("h", palette.FLAG_BLACK, palette.FLAG_WHITE, palette.FLAG_GREEN),
    "er": ("hoist", palette.FLAG_RED, (palette.FLAG_GREEN, palette.FLAG_AZURE)),
    "et": ("h_disc", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_RED, palette.FLAG_AZURE),
    "ga": ("h", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_AZURE),
    "gh": ("h_star", palette.FLAG_RED, palette.FLAG_GOLD, palette.FLAG_GREEN, palette.FLAG_BLACK),
    "gm": (
        "h",
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_GREEN,
    ),
    "gn": ("v", palette.FLAG_RED, palette.FLAG_GOLD, palette.FLAG_GREEN),
    "gq": ("hoist", palette.FLAG_AZURE, (palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED)),
    "gw": ("h_star", palette.FLAG_RED, palette.FLAG_GREEN, palette.FLAG_BLACK),
    "ke": ("h", palette.FLAG_BLACK, palette.FLAG_RED, palette.FLAG_GREEN),
    "km": ("h_star", palette.FLAG_GOLD, palette.FLAG_WHITE, palette.FLAG_RED, palette.FLAG_GREEN),
    "lr": ("canton", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_WHITE),
    "ls": ("field_star", palette.FLAG_AZURE, palette.FLAG_WHITE),
    "ly": ("h", palette.FLAG_RED, palette.FLAG_BLACK, palette.FLAG_GREEN),
    "ma": ("field_star", palette.FLAG_RED, palette.FLAG_GREEN),
    "mg": ("v", palette.FLAG_WHITE, palette.FLAG_GREEN),
    "ml": ("v", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_RED),
    "mr": ("moon", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_GOLD),
    "mu": ("h", palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_GREEN),
    "mw": ("h_disc", palette.FLAG_BLACK, palette.FLAG_RED, palette.FLAG_GREEN, palette.FLAG_RED),
    "mz": ("hoist", palette.FLAG_GREEN, (palette.FLAG_TEAL, palette.FLAG_WHITE, palette.FLAG_TEAL)),
    "na": ("partitioned", palette.FLAG_AZURE, palette.FLAG_RED),
    "ne": ("h_disc", palette.FLAG_ORANGE, palette.FLAG_WHITE, palette.FLAG_GREEN, palette.FLAG_ORANGE),
    "ng": ("v", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_GREEN),
    "rw": ("h", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_GREEN),
    "sc": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_RED),
    "sd": ("hoist", palette.FLAG_GREEN, (palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_BLACK)),
    "sl": ("h", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "sn": ("v_star", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_RED, palette.FLAG_GREEN),
    "so": ("field_star", palette.FLAG_SKY, palette.FLAG_WHITE),
    "ss": ("hoist", palette.FLAG_AZURE, (palette.FLAG_BLACK, palette.FLAG_RED, palette.FLAG_GREEN)),
    "st": ("hoist", palette.FLAG_RED, (palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_GREEN)),
    "sz": (
        "h",
        palette.FLAG_AZURE,
        palette.FLAG_GOLD,
        palette.FLAG_CRIMSON,
        palette.FLAG_GOLD,
        palette.FLAG_AZURE,
    ),
    "td": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_RED),
    "tg": (
        "h",
        palette.FLAG_GREEN,
        palette.FLAG_GOLD,
        palette.FLAG_GREEN,
        palette.FLAG_GOLD,
        palette.FLAG_GREEN,
    ),
    "tn": ("moon", palette.FLAG_CRIMSON, palette.FLAG_WHITE, palette.FLAG_WHITE),
    "tz": ("partitioned", palette.FLAG_GREEN, palette.FLAG_AZURE),
    "ug": ("h", palette.FLAG_BLACK, palette.FLAG_GOLD, palette.FLAG_RED),
    "za": (
        "ybands",
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_GREEN,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
    ),
    "zm": ("h", palette.FLAG_GREEN, palette.FLAG_ORANGE, palette.FLAG_BLACK, palette.FLAG_RED),
    "zw": ("hoist", palette.FLAG_BLACK, (palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_RED)),
    # --- the Americas --------------------------------------------------------
    "ar": ("h_disc", palette.FLAG_SKY, palette.FLAG_WHITE, palette.FLAG_SKY, palette.FLAG_GOLD),
    "bb": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_AZURE),
    "bo": ("h", palette.FLAG_RED, palette.FLAG_GOLD, palette.FLAG_GREEN),
    "br": ("diamond", palette.FLAG_GREEN, palette.FLAG_GOLD, palette.FLAG_NAVY),
    "bs": ("hoist", palette.FLAG_AZURE, (palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_AZURE)),
    "bz": ("circle", palette.FLAG_AZURE, palette.FLAG_WHITE),
    "ca": ("v", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_RED),
    "cl": ("canton", palette.FLAG_WHITE, palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_WHITE),
    "co": ("h", palette.FLAG_GOLD, palette.FLAG_AZURE, palette.FLAG_RED),
    "cr": (
        "h",
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_RED,
    ),
    "cu": ("hoist", palette.FLAG_RED, (palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_AZURE)),
    "dm": ("cross", palette.FLAG_GREEN, palette.FLAG_WHITE),
    "do": ("cross", palette.FLAG_AZURE, palette.FLAG_WHITE),
    "ec": ("h", palette.FLAG_GOLD, palette.FLAG_AZURE, palette.FLAG_RED),
    "gd": ("partitioned", palette.FLAG_GREEN, palette.FLAG_RED),
    "gt": ("v", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "gy": ("hoist", palette.FLAG_GOLD, (palette.FLAG_GREEN, palette.FLAG_GREEN)),
    "hn": (
        "h",
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
    ),
    "ht": ("h", palette.FLAG_AZURE, palette.FLAG_RED),
    "jm": ("cross", palette.FLAG_GREEN, palette.FLAG_GOLD),
    "mx": ("v", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED),
    "ni": ("h", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "pa": ("partitioned", palette.FLAG_WHITE, palette.FLAG_AZURE),
    "pe": ("v", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_RED),
    "py": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "sr": (
        "h_star",
        palette.FLAG_GREEN,
        palette.FLAG_WHITE,
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_GOLD,
    ),
    "sv": ("h", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_AZURE),
    "tt": ("partitioned", palette.FLAG_RED, palette.FLAG_BLACK),
    "us": ("canton", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_NAVY, palette.FLAG_WHITE),
    "uy": ("canton", palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_GOLD),
    "vc": ("v", palette.FLAG_AZURE, palette.FLAG_GOLD, palette.FLAG_GREEN),
    "ve": ("h_star", palette.FLAG_GOLD, palette.FLAG_AZURE, palette.FLAG_RED, palette.FLAG_WHITE),
    # --- Asia ----------------------------------------------------------------
    "ae": ("h", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_BLACK),
    "af": ("v", palette.FLAG_BLACK, palette.FLAG_RED, palette.FLAG_GREEN),
    "bd": ("circle", palette.FLAG_GREEN, palette.FLAG_RED),
    "bh": ("v", palette.FLAG_WHITE, palette.FLAG_RED),
    "bn": ("partitioned", palette.FLAG_GOLD, palette.FLAG_BLACK),
    "bt": ("partitioned", palette.FLAG_GOLD, palette.FLAG_ORANGE),
    "cn": ("field_star", palette.FLAG_RED, palette.FLAG_GOLD),
    "id": ("h", palette.FLAG_RED, palette.FLAG_WHITE),
    "il": ("barred", palette.FLAG_WHITE, palette.FLAG_AZURE),
    "in": ("h_disc", palette.FLAG_GOLD, palette.FLAG_WHITE, palette.FLAG_GREEN, palette.FLAG_AZURE),
    "iq": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_BLACK),
    "ir": ("h", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED),
    "jo": ("hoist", palette.FLAG_RED, (palette.FLAG_BLACK, palette.FLAG_WHITE, palette.FLAG_GREEN)),
    "jp": ("circle", palette.FLAG_WHITE, palette.FLAG_CRIMSON),
    "kg": ("circle", palette.FLAG_RED, palette.FLAG_GOLD),
    "kh": ("h", palette.FLAG_AZURE, palette.FLAG_RED, palette.FLAG_AZURE),
    "kp": ("h", palette.FLAG_AZURE, palette.FLAG_WHITE, palette.FLAG_RED),
    "kr": ("circle", palette.FLAG_WHITE, palette.FLAG_CRIMSON),
    "kw": ("h", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_RED),
    "la": ("h_disc", palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_RED, palette.FLAG_WHITE),
    "lb": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_RED),
    "lk": ("partitioned", palette.FLAG_GOLD, palette.FLAG_MAROON),
    "mm": ("h_star", palette.FLAG_GOLD, palette.FLAG_GREEN, palette.FLAG_RED, palette.FLAG_WHITE),
    "mn": ("h", palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_RED),
    "mv": ("circle", palette.FLAG_RED, palette.FLAG_GREEN),
    "my": ("canton", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_NAVY, palette.FLAG_GOLD),
    "np": ("partitioned", palette.FLAG_CRIMSON, palette.FLAG_AZURE),
    "om": ("h", palette.FLAG_WHITE, palette.FLAG_RED, palette.FLAG_GREEN),
    "ph": ("hoist", palette.FLAG_WHITE, (palette.FLAG_AZURE, palette.FLAG_RED)),
    "pk": ("partitioned", palette.FLAG_GREEN, palette.FLAG_WHITE),
    "qa": ("v", palette.FLAG_WHITE, palette.FLAG_MAROON),
    "sa": ("field", palette.FLAG_GREEN),
    "sg": ("h", palette.FLAG_RED, palette.FLAG_WHITE),
    "sy": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_BLACK),
    "th": (
        "h",
        palette.FLAG_RED,
        palette.FLAG_WHITE,
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_RED,
    ),
    "tj": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_GREEN),
    "tl": ("partitioned", palette.FLAG_BLACK, palette.FLAG_GOLD),
    "tm": ("moon", palette.FLAG_GREEN, palette.FLAG_WHITE, palette.FLAG_WHITE),
    "tr": ("moon", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_WHITE),
    "tw": ("canton", palette.FLAG_RED, palette.FLAG_RED, palette.FLAG_AZURE, palette.FLAG_WHITE),
    "uz": (
        "h_star",
        palette.FLAG_AZURE,
        palette.FLAG_WHITE,
        palette.FLAG_GREEN,
        palette.FLAG_WHITE,
    ),
    "vn": ("field_star", palette.FLAG_RED, palette.FLAG_GOLD),
    "ye": ("h", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_BLACK),
    # --- Oceania -------------------------------------------------------------
    "au": ("canton", palette.FLAG_AZURE, palette.FLAG_AZURE, palette.FLAG_NAVY, palette.FLAG_WHITE),
    "fj": ("canton", palette.FLAG_SKY, palette.FLAG_SKY, palette.FLAG_SKY, palette.FLAG_WHITE),
    "fm": ("field_star", palette.FLAG_SKY, palette.FLAG_WHITE),
    "ki": ("h_disc", palette.FLAG_RED, palette.FLAG_WHITE, palette.FLAG_AZURE, palette.FLAG_GOLD),
    "mh": ("field_star", palette.FLAG_AZURE, palette.FLAG_WHITE),
    "nz": ("canton", palette.FLAG_AZURE, palette.FLAG_AZURE, palette.FLAG_NAVY, palette.FLAG_RED),
    "pg": ("partitioned", palette.FLAG_BLACK, palette.FLAG_RED),
    "sb": ("partitioned", palette.FLAG_AZURE, palette.FLAG_GREEN),
    "to": ("partitioned", palette.FLAG_RED, palette.FLAG_WHITE),
    "vu": ("partitioned", palette.FLAG_RED, palette.FLAG_GREEN),
    "ws": ("field_star", palette.FLAG_RED, palette.FLAG_WHITE),
}


def render_flag(spec: tuple[object, ...]) -> ShapeList:
    """Draw one row of :data:`FLAGS`.

    The layouts take their colours in a fixed order, so this is a table of
    signatures rather than a guess: ``h`` and ``v`` take any number of bands,
    ``nordic`` and ``cross`` take a field and a cross, and the rest take the
    colours their construction named above.
    """
    kind = spec[0]
    colours = spec[1:]
    if kind == "h":
        return _rim() + _h_bands(colours)  # type: ignore[arg-type]
    if kind == "v":
        return _rim() + _v_bands(colours)  # type: ignore[arg-type]
    if kind == "field":
        return _rim() + _field(colours[0])  # type: ignore[arg-type]
    if kind == "nordic":
        return _rim() + _nordic(*colours)  # type: ignore[arg-type]
    if kind == "cross":
        return _rim() + _cross(*colours)  # type: ignore[arg-type]
    if kind == "circle":
        return _rim() + _with_disc(_field(colours[0]), colours[1])  # type: ignore[arg-type]
    if kind == "field_star":
        return _rim() + _field_star(*colours)  # type: ignore[arg-type]
    if kind == "h_disc":
        return _rim() + _with_disc(_h_bands(colours[:-1]), colours[-1])  # type: ignore[arg-type]
    if kind == "v_disc":
        return _rim() + _with_disc(_v_bands(colours[:-1]), colours[-1])  # type: ignore[arg-type]
    if kind == "h_star":
        cx, cy = _centre()
        return _rim() + _h_bands(colours[:-1]) + [  # type: ignore[arg-type]
            star_shape(colours[-1], cx, cy, 15.0, 6.0)  # type: ignore[arg-type]
        ]
    if kind == "v_star":
        cx, cy = _centre()
        return _rim() + _v_bands(colours[:-1]) + [  # type: ignore[arg-type]
            star_shape(colours[-1], cx, cy, 15.0, 6.0)  # type: ignore[arg-type]
        ]
    if kind == "moon":
        return _rim() + _moon(*colours)  # type: ignore[arg-type]
    if kind == "canton":
        return _rim() + _stripes_canton(*colours)  # type: ignore[arg-type]
    if kind == "union":
        return _rim() + _union(*colours)  # type: ignore[arg-type]
    if kind == "hoist":
        return _rim() + _hoist(colours[0], colours[1])  # type: ignore[arg-type]
    if kind == "diamond":
        return _rim() + _diamond(*colours)  # type: ignore[arg-type]
    if kind == "barred":
        return _rim() + _barred(*colours)  # type: ignore[arg-type]
    if kind == "partitioned":
        return _rim() + _partitioned(*colours)  # type: ignore[arg-type]
    if kind == "ybands":
        return _rim() + _union_of_bands(colours)  # type: ignore[arg-type]
    raise KeyError(f"no flag layout called {kind!r}")


def _register_flags() -> None:
    """Expose every country as its own ``flag-xx`` glyph.

    Registration is by loop rather than by decorator because the drawing is
    shared: a hundred decorators would be a hundred identical bodies, and the
    table above is the thing a reader needs to check anyway.
    """

    def make(spec: tuple[object, ...]):
        def render(tint: str) -> ShapeList:
            del tint  # a flag has its own colours; the context tint is not one
            return render_flag(spec)

        return render

    for code, spec in FLAGS.items():
        name = f"flag-{code}"
        if name in GLYPHS:
            raise ValueError(f"glyph already defined: {name}")
        GLYPHS[name] = make(spec)


_register_flags()
