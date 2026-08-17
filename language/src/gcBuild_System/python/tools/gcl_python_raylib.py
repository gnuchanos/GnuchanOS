# -*- coding: utf-8 -*-
"""
pyRaylib — Gnuchan gcl raylib Python wrapper (raylib.h karsiligi)

main.py icinde:
    import pyRaylib as rl
    rl.InitWindow(800, 450, "baslik")
    pos = rl.Vector2(400, 200)
    rl.DrawCircleV(pos, 50, rl.RED)

YAPI (dosya icerigi bolumlere ayrilmistir):
    1. Sabitler        (renkler, tuslar, fare, bayraklar, log seviyeleri)
    2. Deger tipleri   (Vector2/3/4, Quaternion, Color, Rectangle, Matrix,
                        Ray, RayCollision, BoundingBox) -> sekans olarak C'ye gider
    3. Kayit tipleri   (Texture, Image, Font, Sound, Music, Wave, Camera2D/3D,
                        NPatchInfo, ...) -> dict olarak C'ye gider
    4. Fonksiyonlar    (Pencere, cizim, metin, texture, kamera, carpisma, ses)
                        -> her biri tam imzasiyla tanimli, govdede gcl_raylib
                        C modulune yonlendirir. Boylece editorde rl. yazinca
                        TUM API otomatik tamamlanir.

Calisma: gcl -pyrun script.py → python.gcDL → gcl_raylib C modulu → bu wrapper
"""

import gcl_raylib as _rl


# ══════════════════════════════════════════════════════════════════════
# 1) SABITLER
# ══════════════════════════════════════════════════════════════════════

# ── Renkler (r, g, b, a) ────────────────────────────────────────────────
LIGHTGRAY = (200, 200, 200, 255)
GRAY = (130, 130, 130, 255)
DARKGRAY = (80, 80, 80, 255)
YELLOW = (253, 249, 0, 255)
GOLD = (255, 203, 0, 255)
ORANGE = (255, 161, 0, 255)
PINK = (255, 109, 194, 255)
RED = (230, 41, 55, 255)
MAROON = (190, 33, 55, 255)
GREEN = (0, 228, 48, 255)
LIME = (0, 158, 47, 255)
DARKGREEN = (0, 117, 44, 255)
SKYBLUE = (102, 191, 255, 255)
BLUE = (0, 121, 241, 255)
DARKBLUE = (0, 82, 172, 255)
PURPLE = (200, 122, 255, 255)
VIOLET = (135, 60, 190, 255)
DARKPURPLE = (112, 31, 126, 255)
BEIGE = (211, 176, 131, 255)
BROWN = (127, 106, 79, 255)
DARKBROWN = (76, 63, 47, 255)
WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
BLANK = (0, 0, 0, 0)
MAGENTA = (255, 0, 255, 255)
RAYWHITE = (245, 245, 245, 255)

# ── Klavye tuslari ──────────────────────────────────────────────────────
KEY_NULL = 0
KEY_SPACE = 32
KEY_ESCAPE = 256
KEY_ENTER = 257
KEY_TAB = 258
KEY_BACKSPACE = 259
KEY_INSERT = 260
KEY_DELETE = 261
KEY_RIGHT = 262
KEY_LEFT = 263
KEY_DOWN = 264
KEY_UP = 265
KEY_PAGE_UP = 266
KEY_PAGE_DOWN = 267
KEY_HOME = 268
KEY_END = 269
KEY_CAPS_LOCK = 280
KEY_SCROLL_LOCK = 281
KEY_NUM_LOCK = 282
KEY_PRINT_SCREEN = 283
KEY_PAUSE = 284
KEY_F1 = 290
KEY_F2 = 291
KEY_F3 = 292
KEY_F4 = 293
KEY_F5 = 294
KEY_F6 = 295
KEY_F7 = 296
KEY_F8 = 297
KEY_F9 = 298
KEY_F10 = 299
KEY_F11 = 300
KEY_F12 = 301
KEY_LEFT_SHIFT = 340
KEY_LEFT_CONTROL = 341
KEY_LEFT_ALT = 342
KEY_LEFT_SUPER = 343
KEY_RIGHT_SHIFT = 344
KEY_RIGHT_CONTROL = 345
KEY_RIGHT_ALT = 346
KEY_RIGHT_SUPER = 347
KEY_KB_MENU = 348
KEY_APOSTROPHE = 39
KEY_COMMA = 44
KEY_MINUS = 45
KEY_PERIOD = 46
KEY_SLASH = 47
KEY_ZERO = 48
KEY_ONE = 49
KEY_TWO = 50
KEY_THREE = 51
KEY_FOUR = 52
KEY_FIVE = 53
KEY_SIX = 54
KEY_SEVEN = 55
KEY_EIGHT = 56
KEY_NINE = 57
KEY_SEMICOLON = 59
KEY_EQUAL = 61
KEY_LEFT_BRACKET = 91
KEY_BACKSLASH = 92
KEY_RIGHT_BRACKET = 93
KEY_GRAVE = 96
KEY_A = 65
KEY_B = 66
KEY_C = 67
KEY_D = 68
KEY_E = 69
KEY_F = 70
KEY_G = 71
KEY_H = 72
KEY_I = 73
KEY_J = 74
KEY_K = 75
KEY_L = 76
KEY_M = 77
KEY_N = 78
KEY_O = 79
KEY_P = 80
KEY_Q = 81
KEY_R = 82
KEY_S = 83
KEY_T = 84
KEY_U = 85
KEY_V = 86
KEY_W = 87
KEY_X = 88
KEY_Y = 89
KEY_Z = 90
KEY_KP_0 = 320
KEY_KP_1 = 321
KEY_KP_2 = 322
KEY_KP_3 = 323
KEY_KP_4 = 324
KEY_KP_5 = 325
KEY_KP_6 = 326
KEY_KP_7 = 327
KEY_KP_8 = 328
KEY_KP_9 = 329
KEY_KP_DECIMAL = 330
KEY_KP_DIVIDE = 331
KEY_KP_MULTIPLY = 332
KEY_KP_SUBTRACT = 333
KEY_KP_ADD = 334
KEY_KP_ENTER = 335
KEY_BACK = 4
KEY_MENU = 5
KEY_VOLUME_UP = 24
KEY_VOLUME_DOWN = 25

# ── Fare ────────────────────────────────────────────────────────────────
MOUSE_BUTTON_LEFT = 0
MOUSE_BUTTON_RIGHT = 1
MOUSE_BUTTON_MIDDLE = 2
MOUSE_BUTTON_SIDE = 3
MOUSE_BUTTON_EXTRA = 4
MOUSE_BUTTON_FORWARD = 5
MOUSE_BUTTON_BACK = 6
MOUSE_CURSOR_DEFAULT = 0
MOUSE_CURSOR_ARROW = 1
MOUSE_CURSOR_IBEAM = 2
MOUSE_CURSOR_CROSSHAIR = 3
MOUSE_CURSOR_POINTING_HAND = 4
MOUSE_CURSOR_RESIZE_EW = 5
MOUSE_CURSOR_RESIZE_NS = 6
MOUSE_CURSOR_RESIZE_NWSE = 7
MOUSE_CURSOR_RESIZE_NESW = 8
MOUSE_CURSOR_RESIZE_ALL = 9
MOUSE_CURSOR_NOT_ALLOWED = 10

# ── Pencere bayraklari ──────────────────────────────────────────────────
FLAG_VSYNC_HINT = 0x40
FLAG_FULLSCREEN_MODE = 0x2
FLAG_WINDOW_RESIZABLE = 0x4
FLAG_WINDOW_UNDECORATED = 0x8
FLAG_WINDOW_HIDDEN = 0x80
FLAG_WINDOW_MINIMIZED = 0x200
FLAG_WINDOW_MAXIMIZED = 0x400
FLAG_WINDOW_UNFOCUSED = 0x800
FLAG_WINDOW_TOPMOST = 0x1000
FLAG_WINDOW_ALWAYS_RUN = 0x100
FLAG_WINDOW_TRANSPARENT = 0x10
FLAG_WINDOW_HIGHDPI = 0x2000
FLAG_WINDOW_MOUSE_PASSTHROUGH = 0x4000
FLAG_BORDERLESS_WINDOWED_MODE = 0x8000
FLAG_MSAA_4X_HINT = 0x20
FLAG_INTERLACED_HINT = 0x10000

# ── Log seviyeleri ──────────────────────────────────────────────────────
LOG_ALL = 0
LOG_TRACE = 1
LOG_DEBUG = 2
LOG_INFO = 3
LOG_WARNING = 4
LOG_ERROR = 5
LOG_FATAL = 6
LOG_NONE = 7

# ── Kamera ──────────────────────────────────────────────────────────────
CAMERA_PERSPECTIVE = 0
CAMERA_ORTHOGRAPHIC = 1

# ── Texture filtre / sarim ──────────────────────────────────────────────
TEXTURE_FILTER_POINT = 0
TEXTURE_FILTER_BILINEAR = 1
TEXTURE_FILTER_TRILINEAR = 2
TEXTURE_FILTER_ANISOTROPIC_4X = 3
TEXTURE_FILTER_ANISOTROPIC_8X = 4
TEXTURE_FILTER_ANISOTROPIC_16X = 5
TEXTURE_WRAP_REPEAT = 0
TEXTURE_WRAP_CLAMP = 1
TEXTURE_WRAP_MIRROR_REPEAT = 2
TEXTURE_WRAP_MIRROR_CLAMP = 3

# ── Surum bilgisi (calisan C modulunden) ────────────────────────────────
raylib_version = getattr(_rl, "raylib_version", "6.1-dev")
RAYLIB_VERSION_MAJOR = getattr(_rl, "RAYLIB_VERSION_MAJOR", 6)
RAYLIB_VERSION_MINOR = getattr(_rl, "RAYLIB_VERSION_MINOR", 1)
RAYLIB_VERSION_PATCH = getattr(_rl, "RAYLIB_VERSION_PATCH", 0)


# ══════════════════════════════════════════════════════════════════════
# 2) DEGER TIPLERI (sekans olarak C'ye gider)
# ══════════════════════════════════════════════════════════════════════

class Vector2:
    """2D vektor (x, y). C fonksiyonlarina (x, y) sekansi olarak gecer."""
    __slots__ = ("x", "y")

    def __init__(self, x=0.0, y=0.0):
        self.x = float(x)
        self.y = float(y)

    def __getitem__(self, index):
        return (self.x, self.y)[index]

    def __len__(self):
        return 2

    def __iter__(self):
        return iter((self.x, self.y))

    def __repr__(self):
        return "Vector2({}, {})".format(self.x, self.y)

    def __eq__(self, other):
        try:
            return len(other) >= 2 and self.x == other[0] and self.y == other[1]
        except TypeError:
            return NotImplemented

    def as_tuple(self):
        return (self.x, self.y)

    def as_list(self):
        return [self.x, self.y]


class Vector3:
    """3D vektor (x, y, z). C fonksiyonlarina (x, y, z) sekansi olarak gecer."""
    __slots__ = ("x", "y", "z")

    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def __getitem__(self, index):
        return (self.x, self.y, self.z)[index]

    def __len__(self):
        return 3

    def __iter__(self):
        return iter((self.x, self.y, self.z))

    def __repr__(self):
        return "Vector3({}, {}, {})".format(self.x, self.y, self.z)

    def __eq__(self, other):
        try:
            return (len(other) >= 3 and self.x == other[0]
                    and self.y == other[1] and self.z == other[2])
        except TypeError:
            return NotImplemented

    def as_tuple(self):
        return (self.x, self.y, self.z)

    def as_list(self):
        return [self.x, self.y, self.z]


class Vector4:
    """4 bilesenli vektor (x, y, z, w)."""
    __slots__ = ("x", "y", "z", "w")

    def __init__(self, x=0.0, y=0.0, z=0.0, w=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
        self.w = float(w)

    def __getitem__(self, index):
        return (self.x, self.y, self.z, self.w)[index]

    def __len__(self):
        return 4

    def __iter__(self):
        return iter((self.x, self.y, self.z, self.w))

    def __repr__(self):
        return "Vector4({}, {}, {}, {})".format(self.x, self.y, self.z, self.w)

    def as_tuple(self):
        return (self.x, self.y, self.z, self.w)

    def as_list(self):
        return [self.x, self.y, self.z, self.w]


class Quaternion(Vector4):
    """Quaternion: Vector4 alias (x, y, z, w)."""


class Color:
    """RGBA renk (r, g, b, a). C fonksiyonlarina [r, g, b, a] sekansi olarak gecer."""
    __slots__ = ("r", "g", "b", "a")

    def __init__(self, r=0, g=0, b=0, a=255):
        self.r = int(r)
        self.g = int(g)
        self.b = int(b)
        self.a = int(a)

    @classmethod
    def from_list(cls, values):
        """[r, g, b, a] (veya [r, g, b]) listesini Color ornegine cevirir."""
        if values is None:
            return cls(0, 0, 0, 255)
        r = values[0]
        g = values[1]
        b = values[2]
        a = values[3] if len(values) > 3 else 255
        return cls(r, g, b, a)

    @classmethod
    def from_name(cls, name):
        """'RED', 'SKYBLUE' gibi sabit adindan Color ornegi uretir (rl.RED gibi)."""
        return cls.from_list(getattr(_rl, name))

    def __getitem__(self, index):
        return (self.r, self.g, self.b, self.a)[index]

    def __len__(self):
        return 4

    def __iter__(self):
        return iter((self.r, self.g, self.b, self.a))

    def __repr__(self):
        return "Color({}, {}, {}, {})".format(self.r, self.g, self.b, self.a)

    def __eq__(self, other):
        try:
            return (len(other) >= 4 and self.r == other[0] and self.g == other[1]
                    and self.b == other[2] and self.a == other[3])
        except TypeError:
            return NotImplemented

    def as_list(self):
        return [self.r, self.g, self.b, self.a]

    def as_tuple(self):
        return (self.r, self.g, self.b, self.a)


class Rectangle:
    """2D dikdortgen (x, y, width, height). C fonksiyonlarina sekans olarak gecer."""
    __slots__ = ("x", "y", "width", "height")

    def __init__(self, x=0.0, y=0.0, width=0.0, height=0.0):
        self.x = float(x)
        self.y = float(y)
        self.width = float(width)
        self.height = float(height)

    def __getitem__(self, index):
        return (self.x, self.y, self.width, self.height)[index]

    def __len__(self):
        return 4

    def __iter__(self):
        return iter((self.x, self.y, self.width, self.height))

    def __repr__(self):
        return "Rectangle({}, {}, {}, {})".format(self.x, self.y, self.width, self.height)

    def __eq__(self, other):
        try:
            return (len(other) >= 4 and self.x == other[0] and self.y == other[1]
                    and self.width == other[2] and self.height == other[3])
        except TypeError:
            return NotImplemented

    def as_tuple(self):
        return (self.x, self.y, self.width, self.height)

    def as_list(self):
        return [self.x, self.y, self.width, self.height]


class Matrix:
    """4x4 matris (m0..m15), column major, OpenGL style."""
    __slots__ = tuple("m{}".format(i) for i in range(16))

    def __init__(self, *values):
        if len(values) > 16:
            raise TypeError("Matrix expects at most 16 values")
        for i in range(16):
            setattr(self, "m{}".format(i), float(values[i]) if i < len(values) else 0.0)

    def __getitem__(self, index):
        return tuple(getattr(self, "m{}".format(i)) for i in range(16))[index]

    def __len__(self):
        return 16

    def __iter__(self):
        return iter(tuple(getattr(self, "m{}".format(i)) for i in range(16)))

    def __repr__(self):
        vals = ", ".join("{:.2f}".format(getattr(self, "m{}".format(i))) for i in range(16))
        return "Matrix({})".format(vals)


class Ray:
    """Isin: position + direction (Vector3)."""
    __slots__ = ("position", "direction")

    def __init__(self, position=None, direction=None):
        self.position = position if position is not None else Vector3()
        self.direction = direction if direction is not None else Vector3()

    def __repr__(self):
        return "Ray({!r}, {!r})".format(self.position, self.direction)


class RayCollision:
    """Isin carpisma bilgisi: hit, distance, point, normal."""
    __slots__ = ("hit", "distance", "point", "normal")

    def __init__(self, hit=False, distance=0.0, point=None, normal=None):
        self.hit = bool(hit)
        self.distance = float(distance)
        self.point = point if point is not None else Vector3()
        self.normal = normal if normal is not None else Vector3()

    def __repr__(self):
        return "RayCollision(hit={}, dist={:.2f})".format(self.hit, self.distance)


class BoundingBox:
    """Sinir kutusu: min + max (Vector3)."""
    __slots__ = ("min", "max")

    def __init__(self, min_=None, max_=None):
        self.min = min_ if min_ is not None else Vector3()
        self.max = max_ if max_ is not None else Vector3()

    def __repr__(self):
        return "BoundingBox({!r}, {!r})".format(self.min, self.max)


# ══════════════════════════════════════════════════════════════════════
# 3) KAYIT (dict) TIPLERI — C tarafi bunlari dict olarak bekler
# ══════════════════════════════════════════════════════════════════════

class Texture(dict):
    """GPU doku: {id, width, height, mipmaps, format}. LoadTexture() bunu dondurur."""
    def __init__(self, id=0, width=0, height=0, mipmaps=1, format=0, **kwargs):
        super().__init__(id=id, width=width, height=height,
                         mipmaps=mipmaps, format=format, **kwargs)


class Texture2D(Texture):
    """Texture2D: Texture alias."""


class RenderTexture(dict):
    """"fbo" icine cizim: {id, texture, depth}."""
    def __init__(self, id=0, texture=None, depth=None, **kwargs):
        super().__init__(id=id,
                         texture=texture if texture is not None else Texture(),
                         depth=depth if depth is not None else Texture(),
                         **kwargs)


class RenderTexture2D(RenderTexture):
    """RenderTexture2D: RenderTexture alias."""


class Image(dict):
    """CPU bellekte goruntu: {slot, width, height, mipmaps, format}."""
    def __init__(self, slot=0, width=0, height=0, mipmaps=1, format=0, data_ok=0, **kwargs):
        super().__init__(slot=slot, width=width, height=height,
                         mipmaps=mipmaps, format=format, data_ok=data_ok, **kwargs)


class Font(dict):
    """Yazi tipi: {slot, baseSize, glyphCount, glyphPadding}. LoadFont() bunu dondurur."""
    def __init__(self, slot=0, baseSize=0, glyphCount=0, glyphPadding=0, **kwargs):
        super().__init__(slot=slot, baseSize=baseSize, glyphCount=glyphCount,
                         glyphPadding=glyphPadding, **kwargs)


class Sound(dict):
    """Ses efekti: {slot, frameCount}. LoadSound() bunu dondurur."""
    def __init__(self, slot=0, frameCount=0, **kwargs):
        super().__init__(slot=slot, frameCount=frameCount, **kwargs)


class Music(dict):
    """Uzun ses akisi: {slot, frameCount}. LoadMusicStream() bunu dondurur."""
    def __init__(self, slot=0, frameCount=0, **kwargs):
        super().__init__(slot=slot, frameCount=frameCount, **kwargs)


class Wave(dict):
    """Ham ses verisi: {slot, sampleRate, sampleSize, channels}. LoadWave() bunu dondurur."""
    def __init__(self, slot=0, sampleRate=0, sampleSize=0, channels=0, **kwargs):
        super().__init__(slot=slot, sampleRate=sampleRate,
                         sampleSize=sampleSize, channels=channels, **kwargs)


class AudioStream(dict):
    """Ozel ses akisi: {sampleRate, sampleSize, channels}."""
    def __init__(self, sampleRate=0, sampleSize=0, channels=0, **kwargs):
        super().__init__(sampleRate=sampleRate, sampleSize=sampleSize,
                         channels=channels, **kwargs)


class Camera2D(dict):
    """2D kamera: {offset, target, rotation, zoom}. BeginMode2D(cam) ile kullan."""
    def __init__(self, offset=None, target=None, rotation=0.0, zoom=1.0, **kwargs):
        super().__init__(
            offset=offset if offset is not None else Vector2(0, 0),
            target=target if target is not None else Vector2(0, 0),
            rotation=rotation, zoom=zoom, **kwargs)

    @property
    def offset(self):
        o = self.get("offset", (0, 0))
        return Vector2(o[0], o[1])

    @property
    def target(self):
        t = self.get("target", (0, 0))
        return Vector2(t[0], t[1])


class Camera3D(dict):
    """3D kamera: {position, target, up, fovy, projection}. BeginMode3D(cam) ile kullan."""
    def __init__(self, position=None, target=None, up=None, fovy=45.0, projection=0, **kwargs):
        super().__init__(
            position=position if position is not None else Vector3(0, 0, 0),
            target=target if target is not None else Vector3(0, 0, 0),
            up=up if up is not None else Vector3(0, 1, 0),
            fovy=fovy, projection=projection, **kwargs)


class Camera(Camera3D):
    """Camera: Camera3D alias."""


class NPatchInfo(dict):
    """9-patch duzen bilgisi: {source, left, top, right, bottom, layout}."""
    def __init__(self, source=None, left=0, top=0, right=0, bottom=0, layout=0, **kwargs):
        super().__init__(
            source=source if source is not None else Rectangle(),
            left=left, top=top, right=right, bottom=bottom, layout=layout, **kwargs)


class MaterialMap(dict):
    """Malzeme haritasi: {texture, color, value}."""
    def __init__(self, texture=None, color=None, value=0.0, **kwargs):
        super().__init__(
            texture=texture if texture is not None else Texture(),
            color=color if color is not None else Color(255, 255, 255, 255),
            value=value, **kwargs)


class GlyphInfo(dict):
    """Font glifi: {value, offsetX, offsetY, advanceX}."""
    def __init__(self, value=0, offsetX=0, offsetY=0, advanceX=0, **kwargs):
        super().__init__(value=value, offsetX=offsetX,
                         offsetY=offsetY, advanceX=advanceX, **kwargs)


class Transform(dict):
    """Vertex donusumu: {translation, rotation, scale}."""
    def __init__(self, translation=None, rotation=None, scale=None, **kwargs):
        super().__init__(
            translation=translation if translation is not None else Vector3(),
            rotation=rotation if rotation is not None else Quaternion(),
            scale=scale if scale is not None else Vector3(1, 1, 1), **kwargs)


class BoneInfo(dict):
    """Iskelet kemigi: {name, parent}."""
    def __init__(self, name="", parent=0, **kwargs):
        super().__init__(name=name, parent=parent, **kwargs)


class FilePathList(dict):
    """Dosya yolu listesi: {count, paths}."""
    def __init__(self, count=0, paths=None, **kwargs):
        super().__init__(count=count, paths=paths if paths is not None else [], **kwargs)


# ══════════════════════════════════════════════════════════════════════
# 4) FONKSIYONLAR — her biri tam imzayla, govde C modulune yonlendirir
# ══════════════════════════════════════════════════════════════════════

# ── Pencere / ekran / timer ────────────────────────────────────────────

def InitWindow(width: int, height: int, title: str) -> None:
    """Pencere olusturur."""
    _rl.InitWindow(width, height, title)


def CloseWindow() -> None:
    """Pencereyi kapatir ve GL context'i yok eder."""
    _rl.CloseWindow()


def WindowShouldClose() -> bool:
    """Pencere kapatilacak mi? (ESC veya X dugmesi)"""
    return bool(_rl.WindowShouldClose())


def IsWindowReady() -> bool:
    """Pencere hazir mi?"""
    return bool(_rl.IsWindowReady())


def IsWindowFullscreen() -> bool:
    return bool(_rl.IsWindowFullscreen())


def IsWindowHidden() -> bool:
    return bool(_rl.IsWindowHidden())


def IsWindowMinimized() -> bool:
    return bool(_rl.IsWindowMinimized())


def IsWindowMaximized() -> bool:
    return bool(_rl.IsWindowMaximized())


def IsWindowFocused() -> bool:
    return bool(_rl.IsWindowFocused())


def IsWindowResized() -> bool:
    return bool(_rl.IsWindowResized())


def IsWindowState(flags: int) -> bool:
    return bool(_rl.IsWindowState(flags))


def SetWindowState(flags: int) -> None:
    _rl.SetWindowState(flags)


def ClearWindowState(flags: int) -> None:
    _rl.ClearWindowState(flags)


def ToggleFullscreen() -> None:
    _rl.ToggleFullscreen()


def ToggleBorderlessWindowed() -> None:
    _rl.ToggleBorderlessWindowed()


def MaximizeWindow() -> None:
    _rl.MaximizeWindow()


def MinimizeWindow() -> None:
    _rl.MinimizeWindow()


def RestoreWindow() -> None:
    _rl.RestoreWindow()


def SetWindowTitle(title: str) -> None:
    _rl.SetWindowTitle(title)


def SetWindowPosition(x: int, y: int) -> None:
    _rl.SetWindowPosition(x, y)


def SetWindowSize(width: int, height: int) -> None:
    _rl.SetWindowSize(width, height)


def SetWindowMinSize(width: int, height: int) -> None:
    _rl.SetWindowMinSize(width, height)


def SetWindowOpacity(opacity: float) -> None:
    _rl.SetWindowOpacity(opacity)


def SetTargetFPS(fps: int) -> None:
    _rl.SetTargetFPS(fps)


def GetScreenWidth() -> int:
    return int(_rl.GetScreenWidth())


def GetScreenHeight() -> int:
    return int(_rl.GetScreenHeight())


def GetRenderWidth() -> int:
    return int(_rl.GetRenderWidth())


def GetRenderHeight() -> int:
    return int(_rl.GetRenderHeight())


def GetMonitorCount() -> int:
    return int(_rl.GetMonitorCount())


def GetCurrentMonitor() -> int:
    return int(_rl.GetCurrentMonitor())


def GetMonitorWidth(monitor: int) -> int:
    return int(_rl.GetMonitorWidth(monitor))


def GetMonitorHeight(monitor: int) -> int:
    return int(_rl.GetMonitorHeight(monitor))


def GetMonitorRefreshRate(monitor: int) -> int:
    return int(_rl.GetMonitorRefreshRate(monitor))


def GetMonitorName(monitor: int) -> str:
    return str(_rl.GetMonitorName(monitor))


def GetFPS() -> int:
    return int(_rl.GetFPS())


def GetFrameTime() -> float:
    return float(_rl.GetFrameTime())


def GetTime() -> float:
    return float(_rl.GetTime())


def SetRandomSeed(seed: int) -> None:
    _rl.SetRandomSeed(seed)


def GetRandomValue(minimum: int, maximum: int) -> int:
    return int(_rl.GetRandomValue(minimum, maximum))


def TakeScreenshot(filename: str) -> None:
    _rl.TakeScreenshot(filename)


def SetConfigFlags(flags: int) -> None:
    _rl.SetConfigFlags(flags)


def SetExitKey(key: int) -> None:
    _rl.SetExitKey(key)


def ShowCursor() -> None:
    _rl.ShowCursor()


def HideCursor() -> None:
    _rl.HideCursor()


def IsCursorHidden() -> bool:
    return bool(_rl.IsCursorHidden())


def EnableCursor() -> None:
    _rl.EnableCursor()


def DisableCursor() -> None:
    _rl.DisableCursor()


def GetClipboardText() -> str:
    return str(_rl.GetClipboardText())


def SetClipboardText(text: str) -> None:
    _rl.SetClipboardText(text)


# ── Klavye + Fare ───────────────────────────────────────────────────────

def IsKeyPressed(key: int) -> bool:
    return bool(_rl.IsKeyPressed(key))


def IsKeyDown(key: int) -> bool:
    return bool(_rl.IsKeyDown(key))


def IsKeyReleased(key: int) -> bool:
    return bool(_rl.IsKeyReleased(key))


def IsKeyUp(key: int) -> bool:
    return bool(_rl.IsKeyUp(key))


def GetKeyPressed() -> int:
    return int(_rl.GetKeyPressed())


def GetCharPressed() -> int:
    return int(_rl.GetCharPressed())


def IsMouseButtonPressed(button: int) -> bool:
    return bool(_rl.IsMouseButtonPressed(button))


def IsMouseButtonDown(button: int) -> bool:
    return bool(_rl.IsMouseButtonDown(button))


def IsMouseButtonReleased(button: int) -> bool:
    return bool(_rl.IsMouseButtonReleased(button))


def IsMouseButtonUp(button: int) -> bool:
    return bool(_rl.IsMouseButtonUp(button))


def GetMouseX() -> int:
    return int(_rl.GetMouseX())


def GetMouseY() -> int:
    return int(_rl.GetMouseY())


def GetMousePosition() -> tuple:
    return tuple(_rl.GetMousePosition())


def GetMouseDelta() -> tuple:
    return tuple(_rl.GetMouseDelta())


def GetMouseWheelMove() -> float:
    return float(_rl.GetMouseWheelMove())


def GetMouseWheelMoveV() -> tuple:
    return tuple(_rl.GetMouseWheelMoveV())


def SetMousePosition(x: int, y: int) -> None:
    _rl.SetMousePosition(x, y)


def SetMouseScale(scaleX: float, scaleY: float) -> None:
    _rl.SetMouseScale(scaleX, scaleY)


def SetMouseCursor(cursor: int) -> None:
    _rl.SetMouseCursor(cursor)


# ── 2D cizim ────────────────────────────────────────────────────────────

def BeginDrawing() -> None:
    _rl.BeginDrawing()


def EndDrawing() -> None:
    _rl.EndDrawing()


def ClearBackground(color) -> None:
    """Ekrani renkle temizler (color: rl.RED, list veya Color ornegi)."""
    _rl.ClearBackground(color)


def DrawPixel(x: int, y: int, color) -> None:
    _rl.DrawPixel(x, y, color)


def DrawLine(x1: int, y1: int, x2: int, y2: int, color) -> None:
    _rl.DrawLine(x1, y1, x2, y2, color)


def DrawLineV(startPos, endPos, color) -> None:
    """Cizgi (Vector2/tuple baslangic ve bitis)."""
    _rl.DrawLineV(startPos, endPos, color)


def DrawLineEx(startPos, endPos, thick: float, color) -> None:
    _rl.DrawLineEx(startPos, endPos, thick, color)


def DrawCircle(centerX: int, centerY: int, radius: float, color) -> None:
    _rl.DrawCircle(centerX, centerY, radius, color)


def DrawCircleV(center, radius: float, color) -> None:
    _rl.DrawCircleV(center, radius, color)


def DrawCircleSector(center, radius: float, startAngle: float, endAngle: float, segments: int, color) -> None:
    _rl.DrawCircleSector(center, radius, startAngle, endAngle, segments, color)


def DrawCircleSectorLines(center, radius: float, startAngle: float, endAngle: float, segments: int, color) -> None:
    _rl.DrawCircleSectorLines(center, radius, startAngle, endAngle, segments, color)


def DrawCircleGradient(center, radius: float, color1, color2) -> None:
    _rl.DrawCircleGradient(center, radius, color1, color2)


def DrawCircleLines(centerX: int, centerY: int, radius: float, color) -> None:
    _rl.DrawCircleLines(centerX, centerY, radius, color)


def DrawEllipse(centerX: int, centerY: int, radiusX: float, radiusY: float, color) -> None:
    _rl.DrawEllipse(centerX, centerY, radiusX, radiusY, color)


def DrawEllipseLines(centerX: int, centerY: int, radiusX: float, radiusY: float, color) -> None:
    _rl.DrawEllipseLines(centerX, centerY, radiusX, radiusY, color)


def DrawRing(center, innerRadius: float, outerRadius: float, startAngle: float, endAngle: float, segments: int, color) -> None:
    _rl.DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)


def DrawRingLines(center, innerRadius: float, outerRadius: float, startAngle: float, endAngle: float, segments: int, color) -> None:
    _rl.DrawRingLines(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)


def DrawRectangle(x: int, y: int, width: int, height: int, color) -> None:
    """Ici dolu dikdortgen cizer."""
    _rl.DrawRectangle(x, y, width, height, color)


def DrawRectangleV(position, size, color) -> None:
    _rl.DrawRectangleV(position, size, color)


def DrawRectangleRec(rec, color) -> None:
    """Rectangle ornegi ile dikdortgen cizer."""
    _rl.DrawRectangleRec(rec, color)


def DrawRectanglePro(rec, origin, rotation: float, color) -> None:
    _rl.DrawRectanglePro(rec, origin, rotation, color)


def DrawRectangleGradientV(x: int, y: int, width: int, height: int, color1, color2) -> None:
    _rl.DrawRectangleGradientV(x, y, width, height, color1, color2)


def DrawRectangleGradientH(x: int, y: int, width: int, height: int, color1, color2) -> None:
    _rl.DrawRectangleGradientH(x, y, width, height, color1, color2)


def DrawRectangleLines(x: int, y: int, width: int, height: int, color) -> None:
    """Kenarlikli dikdortgen cizer."""
    _rl.DrawRectangleLines(x, y, width, height, color)


def DrawRectangleLinesEx(rec, lineThick: float, color) -> None:
    _rl.DrawRectangleLinesEx(rec, lineThick, color)


def DrawRectangleRounded(rec, roundness: float, segments: int, color) -> None:
    _rl.DrawRectangleRounded(rec, roundness, segments, color)


def DrawRectangleRoundedLines(rec, roundness: float, segments: int, color) -> None:
    _rl.DrawRectangleRoundedLines(rec, roundness, segments, color)


def DrawTriangle(v1, v2, v3, color) -> None:
    _rl.DrawTriangle(v1, v2, v3, color)


def DrawTriangleLines(v1, v2, v3, color) -> None:
    _rl.DrawTriangleLines(v1, v2, v3, color)


def DrawPoly(center, sides: int, radius: float, rotation: float, color) -> None:
    _rl.DrawPoly(center, sides, radius, rotation, color)


def DrawPolyLines(center, sides: int, radius: float, rotation: float, color) -> None:
    _rl.DrawPolyLines(center, sides, radius, rotation, color)


def DrawFPS(x: int, y: int) -> None:
    """FPS gostergesi cizer."""
    _rl.DrawFPS(x, y)


def SetTraceLogLevel(logLevel: int) -> None:
    """Log seviyesini ayarlar (rl.LOG_WARNING vb.)."""
    _rl.SetTraceLogLevel(logLevel)


# ── Metin ───────────────────────────────────────────────────────────────

def DrawText(text: str, x: int, y: int, fontSize: int, color) -> None:
    """Varsayilan fontla metin cizer."""
    _rl.DrawText(text, x, y, fontSize, color)


def SetTextLineSpacing(spacing: int) -> None:
    _rl.SetTextLineSpacing(spacing)


def MeasureText(text: str, fontSize: int) -> int:
    return int(_rl.MeasureText(text, fontSize))


def LoadFont(filename: str) -> dict:
    """Font dosyasi yukler, {slot, baseSize, glyphCount, glyphPadding} dondurur."""
    return _rl.LoadFont(filename)


def LoadFontEx(filename: str, fontSize: int, glyphCount: int) -> dict:
    return _rl.LoadFontEx(filename, fontSize, glyphCount)


def UnloadFont(font) -> None:
    _rl.UnloadFont(font)


def IsFontReady(font) -> bool:
    return bool(_rl.IsFontReady(font))


def MeasureTextEx(font, text: str, fontSize: float, spacing: float) -> tuple:
    return tuple(_rl.MeasureTextEx(font, text, fontSize, spacing))


def DrawTextEx(font, text: str, position, fontSize: float, spacing: float, color) -> None:
    _rl.DrawTextEx(font, text, position, fontSize, spacing, color)


def DrawTextPro(font, text: str, position, origin, rotation: float, fontSize: float, spacing: float, color) -> None:
    _rl.DrawTextPro(font, text, position, origin, rotation, fontSize, spacing, color)


# ── Texture / RenderTexture / Image ─────────────────────────────────────

def LoadTexture(filename: str) -> dict:
    """Doku yukler, {id, width, height, mipmaps, format} dondurur."""
    return _rl.LoadTexture(filename)


def LoadTextureFromImage(image) -> dict:
    return _rl.LoadTextureFromImage(image)


def LoadRenderTexture(width: int, height: int) -> dict:
    return _rl.LoadRenderTexture(width, height)


def UnloadTexture(texture) -> None:
    _rl.UnloadTexture(texture)


def UnloadRenderTexture(target) -> None:
    _rl.UnloadRenderTexture(target)


def IsTextureReady(texture) -> bool:
    return bool(_rl.IsTextureReady(texture))


def SetTextureFilter(texture, filterMode: int) -> None:
    _rl.SetTextureFilter(texture, filterMode)


def SetTextureWrap(texture, wrapMode: int) -> None:
    _rl.SetTextureWrap(texture, wrapMode)


def DrawTexture(texture, x: int, y: int, color) -> None:
    _rl.DrawTexture(texture, x, y, color)


def DrawTextureV(texture, position, color) -> None:
    _rl.DrawTextureV(texture, position, color)


def DrawTextureEx(texture, position, rotation: float, scale: float, color) -> None:
    _rl.DrawTextureEx(texture, position, rotation, scale, color)


def DrawTexturePro(texture, source, dest, origin, rotation: float, color) -> None:
    _rl.DrawTexturePro(texture, source, dest, origin, rotation, color)


def DrawTextureRec(texture, source, position, color) -> None:
    _rl.DrawTextureRec(texture, source, position, color)


def UpdateTexture(texture, image) -> None:
    _rl.UpdateTexture(texture, image)


def BeginTextureMode(target) -> None:
    _rl.BeginTextureMode(target)


def EndTextureMode() -> None:
    _rl.EndTextureMode()


def GenImageColor(width: int, height: int, color) -> dict:
    return _rl.GenImageColor(width, height, color)


def LoadImage(filename: str) -> dict:
    return _rl.LoadImage(filename)


def UnloadImage(image) -> None:
    _rl.UnloadImage(image)


# ── Kamera + 3D ─────────────────────────────────────────────────────────

def BeginMode2D(camera) -> None:
    """2D kamera moduna gecer (camera: rl.Camera2D ornegi)."""
    _rl.BeginMode2D(camera)


def EndMode2D() -> None:
    _rl.EndMode2D()


def BeginMode3D(camera) -> None:
    """3D kamera moduna gecer (camera: rl.Camera3D ornegi)."""
    _rl.BeginMode3D(camera)


def EndMode3D() -> None:
    _rl.EndMode3D()


def DrawGrid(slices: int, spacing: float) -> None:
    _rl.DrawGrid(slices, spacing)


def DrawPlane(centerPos, size, color) -> None:
    _rl.DrawPlane(centerPos, size, color)


def DrawCube(position, width: float, height: float, length: float, color) -> None:
    _rl.DrawCube(position, width, height, length, color)


def DrawCubeV(position, size, color) -> None:
    _rl.DrawCubeV(position, size, color)


def DrawCubeWires(position, width: float, height: float, length: float, color) -> None:
    _rl.DrawCubeWires(position, width, height, length, color)


def DrawSphere(centerPos, radius: float, color) -> None:
    _rl.DrawSphere(centerPos, radius, color)


def DrawSphereWires(centerPos, radius: float, rings: int, slices: int, color) -> None:
    _rl.DrawSphereWires(centerPos, radius, rings, slices, color)


# ── Carpisma testleri ───────────────────────────────────────────────────

def CheckCollisionRecs(rec1, rec2) -> bool:
    return bool(_rl.CheckCollisionRecs(rec1, rec2))


def CheckCollisionCircles(center1, radius1: float, center2, radius2: float) -> bool:
    return bool(_rl.CheckCollisionCircles(center1, radius1, center2, radius2))


def CheckCollisionCircleRec(center, radius: float, rec) -> bool:
    return bool(_rl.CheckCollisionCircleRec(center, radius, rec))


def CheckCollisionPointRec(point, rec) -> bool:
    return bool(_rl.CheckCollisionPointRec(point, rec))


def CheckCollisionPointCircle(point, center, radius: float) -> bool:
    return bool(_rl.CheckCollisionPointCircle(point, center, radius))


def CheckCollisionPointTriangle(point, p1, p2, p3) -> bool:
    return bool(_rl.CheckCollisionPointTriangle(point, p1, p2, p3))


def CheckCollisionLines(startPos1, endPos1, startPos2, endPos2) -> tuple:
    return tuple(_rl.CheckCollisionLines(startPos1, endPos1, startPos2, endPos2))


def CheckCollisionPointLine(point, p1, p2, threshold: int) -> bool:
    return bool(_rl.CheckCollisionPointLine(point, p1, p2, threshold))


def GetCollisionRec(rec1, rec2) -> tuple:
    return tuple(_rl.GetCollisionRec(rec1, rec2))


# ── Ses ─────────────────────────────────────────────────────────────────

def InitAudioDevice() -> None:
    _rl.InitAudioDevice()


def CloseAudioDevice() -> None:
    _rl.CloseAudioDevice()


def AudioDeviceReady() -> bool:
    return bool(_rl.AudioDeviceReady())


def SetMasterVolume(volume: float) -> None:
    _rl.SetMasterVolume(volume)


def GetMasterVolume() -> float:
    return float(_rl.GetMasterVolume())


def LoadWave(filename: str) -> dict:
    return _rl.LoadWave(filename)


def LoadSound(filename: str) -> dict:
    """Ses efekti yukler, {slot, frameCount} dondurur."""
    return _rl.LoadSound(filename)


def LoadSoundFromWave(wave) -> dict:
    return _rl.LoadSoundFromWave(wave)


def UnloadSound(sound) -> None:
    _rl.UnloadSound(sound)


def IsSoundReady(sound) -> bool:
    return bool(_rl.IsSoundReady(sound))


def PlaySound(sound) -> None:
    _rl.PlaySound(sound)


def StopSound(sound) -> None:
    _rl.StopSound(sound)


def PauseSound(sound) -> None:
    _rl.PauseSound(sound)


def ResumeSound(sound) -> None:
    _rl.ResumeSound(sound)


def IsSoundPlaying(sound) -> bool:
    return bool(_rl.IsSoundPlaying(sound))


def SetSoundVolume(sound, volume: float) -> None:
    _rl.SetSoundVolume(sound, volume)


def SetSoundPitch(sound, pitch: float) -> None:
    _rl.SetSoundPitch(sound, pitch)


def SetSoundPan(sound, pan: float) -> None:
    _rl.SetSoundPan(sound, pan)


def LoadMusicStream(filename: str) -> dict:
    """Muzik akisi yukler, {slot, frameCount} dondurur."""
    return _rl.LoadMusicStream(filename)


def UnloadMusicStream(music) -> None:
    _rl.UnloadMusicStream(music)


def PlayMusicStream(music) -> None:
    _rl.PlayMusicStream(music)


def UpdateMusicStream(music) -> None:
    _rl.UpdateMusicStream(music)


def StopMusicStream(music) -> None:
    _rl.StopMusicStream(music)


def PauseMusicStream(music) -> None:
    _rl.PauseMusicStream(music)


def ResumeMusicStream(music) -> None:
    _rl.ResumeMusicStream(music)


def IsMusicStreamPlaying(music) -> bool:
    return bool(_rl.IsMusicStreamPlaying(music))


def SetMusicVolume(music, volume: float) -> None:
    _rl.SetMusicVolume(music, volume)


def SetMusicPitch(music, pitch: float) -> None:
    _rl.SetMusicPitch(music, pitch)
