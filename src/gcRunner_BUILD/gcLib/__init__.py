from . import gcLib_GnuLinux as gcLib
from enum import Enum

PI = 3.14159265358979323846
DEG2RAD = (PI/180.0)
RAD2DEG = (180.0/PI)


class gcKeyboard(Enum):
    NULL            = 0        # Key: NULL, used for no key pressed
    # Alphanumeric keys
    APOSTROPHE      = 39       # Key: '
    COMMA           = 44       # Key: ,
    MINUS           = 45       # Key: -
    PERIOD          = 46       # Key: .
    SLASH           = 47       # Key: /
    ZERO            = 48       # Key: 0
    ONE             = 49       # Key: 1
    TWO             = 50       # Key: 2
    THREE           = 51       # Key: 3
    FOUR            = 52       # Key: 4
    FIVE            = 53       # Key: 5
    SIX             = 54       # Key: 6
    SEVEN           = 55       # Key: 7
    EIGHT           = 56       # Key: 8
    NINE            = 57       # Key: 9
    SEMICOLON       = 59       # Key: ;
    EQUAL           = 61       # Key: =
    A               = 65       # Key: A | a
    B               = 66       # Key: B | b
    C               = 67       # Key: C | c
    D               = 68       # Key: D | d
    E               = 69       # Key: E | e
    F               = 70       # Key: F | f
    G               = 71       # Key: G | g
    H               = 72       # Key: H | h
    I               = 73       # Key: I | i
    J               = 74       # Key: J | j
    K               = 75       # Key: K | k
    L               = 76       # Key: L | l
    M               = 77       # Key: M | m
    N               = 78       # Key: N | n
    O               = 79       # Key: O | o
    P               = 80       # Key: P | p
    Q               = 81       # Key: Q | q
    R               = 82       # Key: R | r
    S               = 83       # Key: S | s
    T               = 84       # Key: T | t
    U               = 85       # Key: U | u
    V               = 86       # Key: V | v
    W               = 87       # Key: W | w
    X               = 88       # Key: X | x
    Y               = 89       # Key: Y | y
    Z               = 90       # Key: Z | z
    LEFT_BRACKET    = 91       # Key: [
    BACKSLASH       = 92       # Key: '\'
    RIGHT_BRACKET   = 93       # Key: ]
    GRAVE           = 96       # Key: `
    # Function keys
    SPACE           = 32       # Key: Space
    ESCAPE          = 256      # Key: Esc
    ENTER           = 257      # Key: Enter
    TAB             = 258      # Key: Tab
    BACKSPACE       = 259      # Key: Backspace
    INSERT          = 260      # Key: Ins
    DELETE          = 261      # Key: Del
    RIGHT           = 262      # Key: Cursor right
    LEFT            = 263      # Key: Cursor left
    DOWN            = 264      # Key: Cursor down
    UP              = 265      # Key: Cursor up
    PAGE_UP         = 266      # Key: Page up
    PAGE_DOWN       = 267      # Key: Page down
    HOME            = 268      # Key: Home
    END             = 269      # Key: End
    CAPS_LOCK       = 280      # Key: Caps lock
    SCROLL_LOCK     = 281      # Key: Scroll down
    NUM_LOCK        = 282      # Key: Num lock
    PRINT_SCREEN    = 283      # Key: Print screen
    PAUSE           = 284      # Key: Pause
    F1              = 290      # Key: F1
    F2              = 291      # Key: F2
    F3              = 292      # Key: F3
    F4              = 293      # Key: F4
    F5              = 294      # Key: F5
    F6              = 295      # Key: F6
    F7              = 296      # Key: F7
    F8              = 297      # Key: F8
    F9              = 298      # Key: F9
    F10             = 299      # Key: F10
    F11             = 300      # Key: F11
    F12             = 301      # Key: F12
    LEFT_SHIFT      = 340      # Key: Shift left
    LEFT_CONTROL    = 341      # Key: Control left
    LEFT_ALT        = 342      # Key: Alt left
    LEFT_SUPER      = 343      # Key: Super left
    RIGHT_SHIFT     = 344      # Key: Shift right
    RIGHT_CONTROL   = 345      # Key: Control right
    RIGHT_ALT       = 346      # Key: Alt right
    RIGHT_SUPER     = 347      # Key: Super right
    KB_MENU         = 348      # Key: KB menu
    # Keypad keys
    KP_0            = 320      # Key: Keypad 0
    KP_1            = 321      # Key: Keypad 1
    KP_2            = 322      # Key: Keypad 2
    KP_3            = 323      # Key: Keypad 3
    KP_4            = 324      # Key: Keypad 4
    KP_5            = 325      # Key: Keypad 5
    KP_6            = 326      # Key: Keypad 6
    KP_7            = 327      # Key: Keypad 7
    KP_8            = 328      # Key: Keypad 8
    KP_9            = 329      # Key: Keypad 9
    KP_DECIMAL      = 330      # Key: Keypad .
    KP_DIVIDE       = 331      # Key: Keypad /
    KP_MULTIPLY     = 332      # Key: Keypad *
    KP_SUBTRACT     = 333      # Key: Keypad -
    KP_ADD          = 334      # Key: Keypad +
    KP_ENTER        = 335      # Key: Keypad Enter
    KP_EQUAL        = 336      # Key: Keypad =
    # Android key buttons
    BACK            = 4        # Key: Android back button
    MENU            = 5        # Key: Android menu button
    VOLUME_UP       = 24       # Key: Android volume up button
    VOLUME_DOWN     = 25       # Key: Android volume down button

class gcMouse(Enum):
    BUTTON_LEFT    = 0       # Mouse button left
    BUTTON_RIGHT   = 1       # Mouse button right
    BUTTON_MIDDLE  = 2       # Mouse button middle (pressed wheel)
    BUTTON_SIDE    = 3       # Mouse button side (advanced mouse device)
    BUTTON_EXTRA   = 4       # Mouse button extra (advanced mouse device)
    BUTTON_FORWARD = 5       # Mouse button forward (advanced mouse device)
    BUTTON_BACK    = 6       # Mouse button back (advanced mouse device)

class gcGamepad(Enum):
    AXIS_LEFT_X        = 0     # Gamepad left stick X axis
    AXIS_LEFT_Y        = 1     # Gamepad left stick Y axis
    AXIS_RIGHT_X       = 2     # Gamepad right stick X axis
    AXIS_RIGHT_Y       = 3     # Gamepad right stick Y axis
    AXIS_LEFT_TRIGGER  = 4     # Gamepad back trigger left, pressure level: [1..-1]
    AXIS_RIGHT_TRIGGER = 5     # Gamepad back trigger right, pressure level: [1..-1]

# System
class gcVector2:
    def __init__(self, X: float, Y: float):
        self.X = X
        self.Y = Y

class gcVector3:
    def __init__(self, X: float, Y: float, Z: float):
        self.X = X
        self.Y = Y
        self.Z = Z
    
class gcVector4:
    def __init__(self, X: float, Y: float, Z: float, W: float):
        self.X = X
        self.Y = Y
        self.Z = Z
        self.W = W

class gcRec:
    def __init__(self, X: float, Y: float, Width: float, Height: float):
        self.X      = X
        self.Y      = Y
        self.Width  = Width
        self.Height = Height
    
class gcCircle:
    def __init__(self, X: float, Y: float, Radius: float):
        self.X      = X
        self.Y      = Y
        self.Radius = Radius

class gcRGBA:
    def __init__(self, R: int, G: int, B: int, A: int):
        self.R = R
        self.G = G
        self.B = B
        self.A = A

    @property
    def Get(self):
        return [self.R, self.G, self.B, self.A]




# Extra Variables
class TwoNumber:
    def __init__(self, a, b):
        self.a = a
        self.b = b

    @property
    def Get(self):
        return gcLib.TwoNumber(self.a, self.b)

class gcRandomNumber_Int:
    def __init__(self, Min: int, Max: int):
        self.Min = Min
        self.Max = Max
    
    @property
    def Get(self):
        return gcLib.gc_GetRandomValue_int(self.Min, self.Max)

class gcRandomNumber_Float:
    def __init__(self, Min: int, Max: int):
        self.Min = Min
        self.Max = Max
    
    @property
    def Get(self):
        return gcLib.gc_GetRandomValue_float(self.Min, self.Max)


# Load Things
class gcLoadFont:
    def __init__(self, FontPath: str):
        self.FontPath = FontPath
        self.Font     = None

    @property
    def Load(self):
        return gcLib.gc_LoadFont(self.FontPath)

class gcLoadTexture2D:
    def __init__(self, TexturePath: str):
        self.TexturePath = TexturePath
        #self.Texture     = None

    @property
    def load(self):
        return gcLib.gc_LoadTexture2D(self.TexturePath)

class gcLoadSound:
    def __init__(self, SoundPath: str):
        self.SoundPath = SoundPath

    @property
    def Load(self):
        return gcLib.gc_LoadSound(self.SoundPath)


# Unload Things
class gcUnloadFont:
    def __init__(self, Font):
        self.Font = Font

    def Unload(self):
        gcLib.gc_UnloadFont(self.Font)

class gcUnloadTexture:
    def __init__(self, Texture):
        self.Texture = Texture

    def Unload(self):
        gcLib.gc_UnloadTexture(self.Texture)

class gcUnloadSound:
    def __init__(self, Sound):
        gcLib.gc_UnloadSound(Sound)






# Get Capsule Class Variable
class gcGetFontSize:
    def __init__(self, Font, Text: str, TextSize: float):
        self.Font     = Font
        self.Text     = Text
        self.TextSize = TextSize

    def Get(self):
        return gcLib.gc_GetFontSize(self.Font.Font, self.Text, self.TextSize)

class gcGetTexture2D_Width:
    def __init__(self, Texture):
        self.Texture = Texture

    @property
    def Get(self):
        return gcLib.gc_GetTextureWidth(self.Texture.Texture)

class gcGetTexture2D_Height:
    def __init__(self, Texture):
        self.Texture = Texture

    @property
    def Get(self):
        return gcLib.gc_GetTextureHeight(self.Texture.Texture)

class gcPlaySound:
    def __init__(self, Sound: gcLoadSound):
	    gcLib.gc_PlaySound(Sound)


# Change Capsule Variable
class gcChangeTexture2D_Width:
    def __init__(self, Texture2D, Width):
        self.Width   = Width
        self.Texture = Texture2D
    
    def Change(self):
        gcLib.gc_ChangeTextureWidth(self.Texture.Texture, self.Width)

class gcChangeTexture2D_Height:
    def __init__(self, Texture2D, Height):
        self.Height  = Height
        self.Texture = Texture2D

    def Change(self):
        gcLib.gc_ChangeTextureHeight(self.Texture.Texture, self.Height)




# system settings
class gcSetTargetFPS:
    def __init__(self, FPS: int):
        self.FPS = FPS
    
    def Set(self):
        gcLib.gc_SetTargetFPS(self.FPS)



class gcRayWindow:
    def __init__(self, Width: int, Height: int, Title: str):
        gcLib.gc_InitWindow(Width, Height, Title)
    
    def Close(self):
        gcLib.gc_CloseWindow()


#audo device for sound and music
class gcAudioDevice:
    def __init__(self):
        gcLib.gc_InitAudioDevice()
    def Stop(self):
        gcLib.gc_CloseAudioDevice()

#check if window close stop while loop
class gcWindowShouldClose:
    def IsClose(self):
        return gcLib.gc_WindowShouldClose()

# Simple Draw
class gcBaseDrawing:
    def Begin(self):
        gcLib.gc_BeginDrawing()

    def End(self):
        gcLib.gc_EndDrawing()

class gcCamera2D:
    def __init__(self, Target: gcVector2, Offset: gcVector2, Rotation: float, Zoom: float):
        self.Target   = Target
        self.Offset   = Offset
        self.Rotation = Rotation
        self.Zoom     = Zoom
        
        self.Camera2D = gcLib.gc_Camera2D(self.Offset.X, self.Offset.Y, self.Target.X, self.Target.Y, self.Rotation, self.Zoom)

    def Begin2DMode(self):
        gcLib.gc_Begin2DMode(self.Camera2D)
    
    def End2DMode(self):
        gcLib.gc_End2DMode()


# debug
class gcDrawFPS:
    def __init__(self, X: int, Y: int):
        self.X = X
        self.Y = Y
    
    def Draw(self):
        gcLib.gc_DrawFPS(self.X, self.Y)




# game windows need this
class gcClearBackground:
    def __init__(self, RGBA: gcRGBA):
        gcLib.gc_ClearBackground(RGBA.Get)



# simple text
class gcText:
    def __init__(self, Text: str, Position: gcVector2, Size: int, Color: gcRGBA):
        self.Text     = Text
        self.Position = Position
        self.Size     = Size
        self.Color    = Color

    def DrawSimpleText(self):
        gcLib.gc_DrawText(self.Text, self.Position.X, self.Position.Y, self.Size, self.Color)

    def DrawAdvanceText(self, Font):
        gcLib.gc_DrawTextEx(self.Text, self.Position.X, self.Position.Y, self.Size, Font.Font, self.Color)

# Draw Circles
class gcCircle:
    def __init__(self, Circle: gcCircle, Color: gcRGBA):
        self.Circle  = Circle
        self.Color   = Color

    def DrawSimple(self):
        gcLib.gc_DrawCircle(self.Circle.X, self.Circle.Y, self.Circle.Radius, self.Color)

    def DrawLines(self):
        gcLib.gc_DrawCircleLines(self.Circle.X, self.Circle.Y, self.Circle.Radius, self.Color)

    def DrawGradient(self, Innercolor: gcRGBA, OuterColor: gcRGBA):
        gcLib.gc_DrawCircleGradient(
            self.Circle.X, self.Circle.Y, self.Circle.Radius, Innercolor, OuterColor
        )

# Draw Lines
class gcLine:
    def __init__(self, StartPos: gcVector2, EndPos: gcVector2, Color: gcRGBA):
        self.StartPos = StartPos
        self.EndPos   = EndPos
        self.Color    = Color

    def DrawSimple(self):
        gcLib.gc_DrawLine(self.StartPos.X, self.StartPos.Y, self.EndPos.X, self.EndPos.Y, self.Color)

    def DrawThickLine(self, Thick: float):
        gcLib.gc_DrawLineEx(self.StartPos.X, self.StartPos.Y, self.EndPos.X, self.EndPos.Y, Thick, self.Color)


# Draw Rectangles
class gcRectangle:
    def __init__(self, Rectangle: gcRec, Color: gcRGBA):
        self.Rectangle = Rectangle
        self.Color     = Color

    def DrawSimple(self):
        gcLib.gc_DrawRectangle(self.Rectangle.X, self.Rectangle.Y, self.Rectangle.Width, self.Rectangle.Height, self.Color)

    def DrawLines(self):
        gcLib.gc_DrawRectangleLines(self.Rectangle.X, self.Rectangle.Y, self.Rectangle.Width, self.Rectangle.Height, self.Color)

    def DrawThickLines(self, Thick: float):
        gcLib.gc_DrawRectangleLinesEx(self.Rectangle.X, self.Rectangle.Y, self.Rectangle.Width, self.Rectangle.Height, Thick, self.Color)

    def DrawRoundLines(self, Roundness: float, Segment: int):
        gcLib.gc_DrawRectangleRoundedLines(self.Rectangle.X, self.Rectangle.Y, self.Rectangle.Width, self.Rectangle.Height, Roundness, Segment, self.Color)

    def DrawRoundLinesThick(self, Roundness: float, Segments: int, Thick: float):
        gcLib.gc_DrawRectangleRoundedLinesEx(self.Rectangle.X, self.Rectangle.Y, self.Rectangle.Width, self.Rectangle.Height, Roundness, Segments, Thick, self.Color)

# extra 2D
class gcDrawTriangle:
    def __init__(self, V1: gcVector2, V2: gcVector2, V3: gcVector2, Color: gcRGBA):
        self.V1 = V1
        self.V2 = V2
        self.V3 = V3
        self.Color = Color

    def Draw(self):
        gcLib.gc_DrawTriangle(self.V1.X, self.V1.Y, self.V2.X, self.V2.Y, self.V3.X, self.V3.Y, self.Color)

# Draw Texture2D
class gc2DTexture:
    def __init__(self, Texture, Position: gcVector2, Color: gcRGBA):
        self.Texture = Texture
        self.Position = Position
        self.Color = Color

    def DrawSimple(self):
        gcLib.gc_DrawTexture(self.Texture, self.Position.X, self.Position.Y, self.Color.Get)

    # scale must be max 1
    def DrawEx(self, Rotation: float, Scale: float):
        gcLib.gc_DrawTextureEx(self.Texture, self.Position.X, self.Position.Y, Rotation, Scale, self.Color.Get)

    def DrawRec(self, Rec: gcRec):
        gcLib.gc_DrawTextureRec(self.Texture, Rec.X, Rec.Y, Rec.Width, Rec.Height, self.Position.X, self.Position.Y, self.Color.Get)


# Check Collision 2D
class gcCollision2D:
    @property
    def CheckRectangle(self, Rectangle0: gcRectangle, Rectangle1: gcRectangle):
        return gcLib.gc_CheckCollisionRect(
            Rectangle0.X, Rectangle0.Y, Rectangle0.Width, Rectangle0.Height,
            Rectangle1.X, Rectangle1.Y, Rectangle1.Width, Rectangle1.Height 
        )

    @property
    def CheckCircleRectangle(self, Circle: gcCircle, Rectangle: gcRectangle):
        return gcLib.gc_CheckCollisionCircleRect(
            Circle.X,    Circle.Y,    Circle.Radius, 
            Rectangle.X, Rectangle.Y, Rectangle.Width, Rectangle.Height
        )


# Keyboard Press
class gcIsKeyboard:
    def __init__(self, Key: gcKeyboard):
        self.key = Key.value
   
    @property
    def CheckKeyPressed(self):
        return gcLib.gc_IsKeyPressed(self.key)

    @property
    def CheckKeyRelease(self):
        return gcLib.gc_IsKeyReleased(self.key)

    @property
    def CheckKeyDown(self):
        return gcLib.gc_IsKeyDown(self.key)

    @property
    def CheckKeyUP(self):
        return gcLib.gc_IsKeyUp(self.key)










