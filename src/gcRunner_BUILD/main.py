import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from enum import Enum
from gcLib import *



class GameScene(Enum):
    LogoScreen = 0
    MenuScreen = 1
    GameScreen = 2
    EndScreen  = 3

class GCEngine:
    def __init__(self, Width: int, Height: int, WindowTitle: str):

        self.Width  = Width
        self.Height = Height
        self.Title  = WindowTitle

        self.Window = gcRayWindow(self.Width, self.Height, self.Title)

        # Audio
        self.Audio = gcAudioDevice()

        # Settings
        self.CurrentScene = GameScene.LogoScreen
        self.Key          = gcKeyboard
        self.DefaultFont  = gcLoadFont("./Font/Sans.ttf").Load

        # test Sound
        self.DefaultAudio = gcLoadSound("./Sound/hitSound.wav").Load
        gcPlaySound(self.DefaultAudio)

        # test Texture
        self.DefaultTexture = gcLoadTexture2D("./Texture/logo.png").load
        print(self.DefaultTexture)

        # test Rectangle
        self.Rectangle = gcRec(600, 600, 50, 50)


        self.DefaultCamera = gcCamera2D(gcVector2(self.Width/2, self.Height/2), gcVector2(self.Width/2, self.Height/2), 0, 1)


    def RUN(self):
        while not gcWindowShouldClose().IsClose():
            self.Update()
            # base drawin
            gcBaseDrawing().Begin()
            gcClearBackground(gcRGBA(0, 0, 0, 255))
            
            # camera 2D drawing
            self.DefaultCamera.Begin2DMode()
            
            # camera level layer
            self.Draw_Object()
            self.DefaultCamera.End2DMode()
            
            # ui level layer
            self.Draw_UI()
            gcBaseDrawing().End()

        # unload and close all things yes this is python but c binding must unload
        self.Unload()
        self.Window.Close()
        self.Audio.Stop()


    def Update(self):
        # test update
        if gcIsKeyboard(self.Key.W).CheckKeyDown:
            self.Rectangle.Y -= 0.1
        elif gcIsKeyboard(self.Key.S).CheckKeyDown:
            self.Rectangle.Y += 0.1

        if gcIsKeyboard(self.Key.A).CheckKeyDown:
            self.Rectangle.X -= 0.1
        elif gcIsKeyboard(self.Key.D).CheckKeyDown:
            self.Rectangle.X += 0.1
        
        #test update
        
        if self.CurrentScene == GameScene.LogoScreen:
            pass
        elif self.CurrentScene == GameScene.MenuScreen:
            pass
        elif self.CurrentScene == GameScene.GameScreen:
            pass
        elif self.CurrentScene == GameScene.EndScreen:
            pass

    def Draw_Object(self):
        # test draw   
        gcRectangle(self.Rectangle, gcRGBA(255, 255, 255, 255).Get).DrawSimple()



        if self.CurrentScene == GameScene.LogoScreen:
            pass
        elif self.CurrentScene == GameScene.MenuScreen:
            pass
        elif self.CurrentScene == GameScene.GameScreen:
            pass
        elif self.CurrentScene == GameScene.EndScreen:
            pass

    def Draw_UI(self):
        # test Draw
        gc2DTexture(self.DefaultTexture, gcVector2(100, 100), gcRGBA(255, 255, 255, 255)).DrawSimple()
        gc2DTexture(self.DefaultTexture, gcVector2(100, 500), gcRGBA(255, 255, 255, 255)).DrawEx(0, 1)
        gc2DTexture(self.DefaultTexture, gcVector2(600, 100), gcRGBA(255, 255, 255, 255)).DrawRec(gcRec(10, 10, 50, 100))


        if self.CurrentScene == GameScene.LogoScreen:
            pass
        elif self.CurrentScene == GameScene.MenuScreen:
            pass
        elif self.CurrentScene == GameScene.GameScreen:
            pass
        elif self.CurrentScene == GameScene.EndScreen:
            pass



    def Unload(self):
        gcUnloadSound(self.DefaultAudio)


if __name__ == "__main__":
    gc = GCEngine(1600, 900, "uWu")
    gc.RUN()


print(TwoNumber(10, 20).Get)
import sys
print("Python executable:", sys.executable)
print("Python sys.path:", sys.path)
