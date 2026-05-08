from pyray import *
from .Settings import *



class GText:
    def __init__(self, Text: str, TFont: Font, TFontSize: int, TPosition: GVector2, TColor: Color):
        self.Text      = Text
        self.TFont     = TFont
        self.TFontSize = TFontSize
        self.Tposition = TPosition
        self.TColor    = TColor

    def Update(self, NewText: str):
        self.Text = NewText

    def CenterXPosition(self, ScreenWidth: int):
        self.ScoreTextScale = measure_text_ex(self.TFont, self.Text, self.TFontSize, 0)
        self.Tposition = GVector2(ScreenWidth/2-self.ScoreTextScale.x/2, self.Tposition.Y)

    def CenterYPosition(self, ScreenHeight: int):
        self.ScoreTextScale = measure_text_ex(self.TFont, self.Text, self.TFontSize, 0)
        self.Tposition = GVector2(self.Tposition.X, ScreenHeight/2-self.ScoreTextScale.y/2)

    def Draw(self):
        draw_text_ex(self.TFont, self.Text, Vector2(self.Tposition.X, self.Tposition.Y), self.TFontSize, 0, self.TColor)


class GButton:
    def __init__(self, Text: str, TFont: Font, TextSize: int, TPosition: GVector2, TColor: Color, NColor: Color, HColor: Color, PColor: Color):
        self.BText      = Text
        self.BTextSize  = TextSize
        self.BTextPosition = TPosition
        self.BTextFont  = TFont
        self.BFontSize  = measure_text_ex(self.BTextFont, Text, TextSize, 0)
        self.ButtonSize = Rectangle(TPosition.X, TPosition.Y, self.BFontSize.x+20, self.BFontSize.y+10)

        self.BTextColor    = TColor
        self.BCurrentColor = NColor
        self.BNormalColor  = NColor
        self.BHoverColor   = HColor
        self.BPressColor   = PColor

        self.BPressWaitTime = 1
        self.BPressCheck    = False

    def Update(self, MouseCursor: GVector2):
        if (check_collision_point_rec(MouseCursor, self.ButtonSize)):
            self.BCurrentColor = self.BHoverColor
            if (is_mouse_button_down(GMouse.LEFT)):
                self.BCurrentColor = self.BPressColor
                self.BPressCheck   = True
        else:
            self.BCurrentColor = self.BNormalColor

        if (self.BPressCheck):
            if (self.BPressWaitTime > 0):
                self.BPressWaitTime -= get_frame_time()
            else:
                self.BPressWaitTime = 1
                self.BPressCheck    = False
                return True

        return False

    @property
    def Draw(self):
        draw_rectangle_rounded(self.ButtonSize, 3, 8, self.BCurrentColor)
        draw_text_ex(self.BTextFont, self.BText, [self.BTextPosition.X+10, self.BTextPosition.Y+5], self.BTextSize, 0, self.BTextColor)

class GProgressBar:
    def __init__(self):
        pass

class GCheckBox:
    def __init__(self):
        pass



