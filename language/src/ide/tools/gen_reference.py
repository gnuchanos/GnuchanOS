#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_reference.py — .gcReference uretici

GCL kutuphanelerinin icinde neler oldugunu IDE'de gosterebilmek icin her
kutuphanenin yanina bir .gcReference dosyasi uretilir. Format basit:

    ## functions
    - Name(args) -> return
        "one line description"
        ```
        example call
        ```

    ## constants: <category>
    - NAME  (= value)
        "one line description"

Dokumanlar (Library/Lua/lua.doc, Library/Python/py.doc) da buradan
kopyalanir.

Kullanim:
    python gen_reference.py <build_out>
"""

import os
import re
import shutil
import sys

LUA_DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "lua_doc.txt")
PY_DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "python_doc.txt")
GCL_DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gcl_doc.txt")

LUA_RAYLIB_BIND = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "..", "src", "gcBuild_System", "lua", "raylib",
    "gcl_raylib_bind.c",
)
PY_RAYLIB_WRAPPER = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "..", "src", "gcBuild_System", "python", "tools",
    "gcl_python_raylib.py",
)

# ---------------------------------------------------------------------------
# lua_raylib imzali fonksiyon tablosu (gercekten neyi cagirdigi net):
# name -> (signature, aciklama, ornek)
# ---------------------------------------------------------------------------
LUA_RAYLIB_FUNCS = [
    ("InitWindow", "(width:int, height:int, title:str)",
     "Create a window.", "rl.InitWindow(640, 480, \"Game\")"),
    ("CloseWindow", "()", "Close the window.",
     "rl.CloseWindow()"),
    ("WindowShouldClose", "() -> bool",
     "True when the window must close (ESC or X button).",
     "while not rl.WindowShouldClose() do"),
    ("GetScreenWidth", "() -> int", "Current screen width.",
     "local w = rl.GetScreenWidth()"),
    ("GetScreenHeight", "() -> int", "Current screen height.",
     "local h = rl.GetScreenHeight()"),
    ("SetTargetFPS", "(fps:int)", "Set the target frame rate.",
     "rl.SetTargetFPS(60)"),
    ("SetConfigFlags", "(flags:int)", "Set window config flags (FLAG_*).",
     "rl.SetConfigFlags(rl.FLAG_WINDOW_RESIZABLE)"),
    ("SetExitKey", "(key:int)", "Which key closes the window (KEY_*).",
     "rl.SetExitKey(rl.KEY_ESCAPE)"),
    ("ToggleFullscreen", "()", "Switch to/from fullscreen.",
     "rl.ToggleFullscreen()"),
    ("TakeScreenshot", "(filename:str)", "Save a PNG screenshot.",
     "rl.TakeScreenshot(\"shot.png\")"),
    ("GetTime", "() -> number", "Seconds since InitWindow.",
     "local t = rl.GetTime()"),
    ("GetFrameTime", "() -> number", "Seconds since last frame.",
     "local dt = rl.GetFrameTime()"),
    ("GetFPS", "() -> int", "Current FPS.", "local fps = rl.GetFPS()"),
    ("BeginDrawing", "()", "Start drawing (call every frame).",
     "rl.BeginDrawing()"),
    ("EndDrawing", "()", "Swap buffers (end of frame).",
     "rl.EndDrawing()"),
    ("ClearBackground", "(color:Color)", "Fill the screen with a color.",
     "rl.ClearBackground(rl.SKYBLUE)"),
    ("DrawPixel", "(x:int, y:int, color:Color)", "Draw one pixel.",
     "rl.DrawPixel(10, 10, rl.WHITE)"),
    ("DrawLine", "(x1:int, y1:int, x2:int, y2:int, color:Color)",
     "Draw a line.", "rl.DrawLine(0, 0, 100, 100, rl.RED)"),
    ("DrawLineV", "(startPos:Vector2, endPos:Vector2, color:Color)",
     "Draw a line from two vectors.",
     "rl.DrawLineV(rl.Vector2(0,0), rl.Vector2(200,200), rl.RED)"),
    ("DrawCircle", "(centerX:int, centerY:int, radius:number, color:Color)",
     "Draw a filled circle.",
     "rl.DrawCircle(320, 240, 50, rl.GREEN)"),
    ("DrawCircleV", "(center:Vector2, radius:number, color:Color)",
     "Draw a filled circle from a vector.",
     "rl.DrawCircleV(rl.Vector2(320,240), 50, rl.GREEN)"),
    ("DrawCircleLines", "(centerX:int, centerY:int, radius:number, color:Color)",
     "Draw a circle outline.",
     "rl.DrawCircleLines(320, 240, 50, rl.WHITE)"),
    ("DrawCircleGradient", "(center:Vector2, radius:number, color1:Color, color2:Color)",
     "Draw a circle with a radial color gradient.",
     "rl.DrawCircleGradient(rl.Vector2(320,240), 50, rl.PURPLE, rl.BLACK)"),
    ("DrawCircleSector", "(center:Vector2, radius:number, startAngle:number, endAngle:number, segments:int, color:Color)",
     "Draw a filled pie slice (degrees).",
     "rl.DrawCircleSector(rl.Vector2(320,240), 80, 0, 90, 32, rl.GOLD)"),
    ("DrawCircleSectorLines", "(center:Vector2, radius:number, startAngle:number, endAngle:number, segments:int, color:Color)",
     "Draw a pie slice outline.",
     "rl.DrawCircleSectorLines(rl.Vector2(320,240), 80, 0, 90, 32, rl.GOLD)"),
    ("DrawRing", "(center:Vector2, innerRadius:number, outerRadius:number, startAngle:number, endAngle:number, segments:int, color:Color)",
     "Draw a filled ring segment.",
     "rl.DrawRing(rl.Vector2(320,240), 40, 70, 0, 360, 48, rl.ORANGE)"),
    ("DrawRingLines", "(center:Vector2, innerRadius:number, outerRadius:number, startAngle:number, endAngle:number, segments:int, color:Color)",
     "Draw a ring outline.",
     "rl.DrawRingLines(rl.Vector2(320,240), 40, 70, 0, 360, 48, rl.ORANGE)"),
    ("DrawEllipse", "(centerX:int, centerY:int, radiusX:number, radiusY:number, color:Color)",
     "Draw a filled ellipse.",
     "rl.DrawEllipse(320, 240, 120, 60, rl.PINK)"),
    ("DrawRectangle", "(x:int, y:int, width:int, height:int, color:Color)",
     "Draw a filled rectangle.",
     "rl.DrawRectangle(10, 10, 200, 80, rl.BLUE)"),
    ("DrawRectangleV", "(position:Vector2, size:Vector2, color:Color)",
     "Draw a rectangle from two vectors.",
     "rl.DrawRectangleV(rl.Vector2(10,10), rl.Vector2(200,80), rl.BLUE)"),
    ("DrawRectangleRec", "(rec:Rectangle, color:Color)",
     "Draw a rectangle from a Rectangle value.",
     "rl.DrawRectangleRec(rl.Rectangle(10,10,200,80), rl.BLUE)"),
    ("DrawRectangleLines", "(x:int, y:int, width:int, height:int, color:Color)",
     "Draw a rectangle outline.",
     "rl.DrawRectangleLines(10, 10, 200, 80, rl.WHITE)"),
    ("DrawRectangleLinesEx", "(rec:Rectangle, lineThick:number, color:Color)",
     "Draw a rectangle outline with thickness.",
     "rl.DrawRectangleLinesEx(rl.Rectangle(10,10,200,80), 4, rl.WHITE)"),
    ("DrawRectangleRounded", "(rec:Rectangle, roundness:number, segments:int, color:Color)",
     "Draw a filled rounded rectangle.",
     "rl.DrawRectangleRounded(rl.Rectangle(10,10,200,80), 0.3, 16, rl.PURPLE)"),
    ("DrawRectangleGradientV", "(x:int, y:int, width:int, height:int, color1:Color, color2:Color)",
     "Vertical color gradient rectangle.",
     "rl.DrawRectangleGradientV(10,10,200,80, rl.RED, rl.BLUE)"),
    ("DrawRectangleGradientH", "(x:int, y:int, width:int, height:int, color1:Color, color2:Color)",
     "Horizontal color gradient rectangle.",
     "rl.DrawRectangleGradientH(10,10,200,80, rl.RED, rl.BLUE)"),
    ("DrawTriangle", "(v1:Vector2, v2:Vector2, v3:Vector2, color:Color)",
     "Draw a filled triangle.",
     "rl.DrawTriangle(rl.Vector2(0,0), rl.Vector2(200,0), rl.Vector2(100,200), rl.GREEN)"),
    ("DrawPoly", "(center:Vector2, sides:int, radius:number, rotation:number, color:Color)",
     "Draw a filled regular polygon.",
     "rl.DrawPoly(rl.Vector2(320,240), 6, 80, 0, rl.GOLD)"),
    ("DrawPolyLines", "(center:Vector2, sides:int, radius:number, rotation:number, color:Color)",
     "Draw a polygon outline.",
     "rl.DrawPolyLines(rl.Vector2(320,240), 6, 80, 0, rl.GOLD)"),
    ("DrawText", "(text:str, x:int, y:int, fontSize:int, color:Color)",
     "Draw a string with the default font.",
     "rl.DrawText(\"hello\", 10, 10, 20, rl.WHITE)"),
    ("DrawFPS", "(x:int, y:int)", "Draw the current FPS at x,y.",
     "rl.DrawFPS(10, 10)"),
    ("IsKeyDown", "(key:int) -> bool", "Is a key currently held?",
     "if rl.IsKeyDown(rl.KEY_SPACE) then"),
    ("IsKeyPressed", "(key:int) -> bool", "Was a key pressed this frame?",
     "if rl.IsKeyPressed(rl.KEY_ENTER) then"),
    ("IsKeyReleased", "(key:int) -> bool", "Was a key released this frame?",
     "if rl.IsKeyReleased(rl.KEY_SPACE) then"),
    ("IsKeyUp", "(key:int) -> bool", "Is a key currently up?",
     "if rl.IsKeyUp(rl.KEY_ESC) then"),
    ("GetKeyPressed", "() -> int", "Last key pressed (0 if none).",
     "local key = rl.GetKeyPressed()"),
    ("GetCharPressed", "() -> int", "Last char typed.",
     "local ch = rl.GetCharPressed()"),
    ("IsMouseButtonDown", "(button:int) -> bool",
     "Is a mouse button held? (MOUSE_BUTTON_*)",
     "if rl.IsMouseButtonDown(rl.MOUSE_BUTTON_LEFT) then"),
    ("IsMouseButtonPressed", "(button:int) -> bool",
     "Was a mouse button pressed this frame?",
     "if rl.IsMouseButtonPressed(rl.MOUSE_BUTTON_LEFT) then"),
    ("GetMouseX", "() -> int", "Current mouse X.", "local x = rl.GetMouseX()"),
    ("GetMouseY", "() -> int", "Current mouse Y.", "local y = rl.GetMouseY()"),
    ("GetMousePosition", "() -> Vector2", "Current mouse position.",
     "local p = rl.GetMousePosition()"),
    ("GetMouseDelta", "() -> Vector2", "Mouse movement since last frame.",
     "local d = rl.GetMouseDelta()"),
    ("GetMouseWheelMove", "() -> number", "Mouse wheel movement.",
     "local w = rl.GetMouseWheelMove()"),
    ("SetMousePosition", "(x:int, y:int)", "Move the OS mouse cursor.",
     "rl.SetMousePosition(320, 240)"),
    ("SetMouseScale", "(scaleX:number, scaleY:number)",
     "Scale mouse movement.", "rl.SetMouseScale(2.0, 2.0)"),
    ("LoadTexture", "(filename:str) -> Texture2D",
     "Load a texture from a file.", "local tex = rl.LoadTexture(\"img.png\")"),
    ("UnloadTexture", "(texture:Texture2D)", "Free a texture.",
     "rl.UnloadTexture(tex)"),
    ("DrawTexture", "(texture:Texture2D, x:int, y:int, color:Color)",
     "Draw a texture at x,y (WHITE tint = original).",
     "rl.DrawTexture(tex, 0, 0, rl.WHITE)"),
    ("DrawTextureEx", "(texture:Texture2D, position:Vector2, rotation:number, scale:number, color:Color)",
     "Draw a texture with rotation and scale.",
     "rl.DrawTextureEx(tex, rl.Vector2(100,100), 45, 1.5, rl.WHITE)"),
    ("Vector2", "(x:number, y:number) -> Vector2", "Build a 2D vector.",
     "local p = rl.Vector2(100, 100)"),
    ("Vector3", "(x:number, y:number, z:number) -> Vector3",
     "Build a 3D vector.", "local v = rl.Vector3(1, 2, 3)"),
    ("Rectangle", "(x:number, y:number, width:number, height:number) -> Rectangle",
     "Build a rectangle value.",
     "local r = rl.Rectangle(10, 10, 200, 80)"),
    ("Color", "(r:int, g:int, b:int, a:int) -> Color",
     "Build an RGBA color value.", "local c = rl.Color(255, 0, 0, 255)"),
]

# ---------------------------------------------------------------------------
# pyRaylib fonksiyonlari (wrapper'dan otomatik signature cikar):
# (name, signature, docstring, ornek)
# ---------------------------------------------------------------------------
PY_RAYLIB_FUNCS = [
    ("InitWindow", "(width:int, height:int, title:str)", "Create a window.",
     "rl.InitWindow(640, 480, \"Game\")"),
    ("CloseWindow", "()", "Close the window.", "rl.CloseWindow()"),
    ("WindowShouldClose", "() -> bool",
     "True when the window must close.", "while not rl.WindowShouldClose():"),
    ("GetScreenWidth", "() -> int", "Current screen width.",
     "w = rl.GetScreenWidth()"),
    ("GetScreenHeight", "() -> int", "Current screen height.",
     "h = rl.GetScreenHeight()"),
    ("SetTargetFPS", "(fps:int)", "Set the target frame rate.",
     "rl.SetTargetFPS(60)"),
    ("SetConfigFlags", "(flags:int)", "Set window config flags.",
     "rl.SetConfigFlags(rl.FLAG_WINDOW_RESIZABLE)"),
    ("SetExitKey", "(key:int)", "Which key closes the window.",
     "rl.SetExitKey(rl.KEY_ESCAPE)"),
    ("ToggleFullscreen", "()", "Switch to/from fullscreen.",
     "rl.ToggleFullscreen()"),
    ("TakeScreenshot", "(filename:str)", "Save a PNG screenshot.",
     "rl.TakeScreenshot(\"shot.png\")"),
    ("GetTime", "() -> float", "Seconds since InitWindow.",
     "t = rl.GetTime()"),
    ("GetFrameTime", "() -> float", "Seconds since last frame.",
     "dt = rl.GetFrameTime()"),
    ("GetFPS", "() -> int", "Current FPS.", "fps = rl.GetFPS()"),
    ("BeginDrawing", "()", "Start drawing.", "rl.BeginDrawing()"),
    ("EndDrawing", "()", "Swap buffers.", "rl.EndDrawing()"),
    ("ClearBackground", "(color)", "Fill the screen with a color.",
     "rl.ClearBackground((14, 20, 34, 255))"),
    ("DrawPixel", "(x:int, y:int, color)", "Draw one pixel.",
     "rl.DrawPixel(10, 10, rl.WHITE)"),
    ("DrawLine", "(x1:int, y1:int, x2:int, y2:int, color)",
     "Draw a line.", "rl.DrawLine(0, 0, 100, 100, rl.RED)"),
    ("DrawLineV", "(startPos, endPos, color)", "Draw a line from two vectors.",
     "rl.DrawLineV(rl.Vector2(0,0), rl.Vector2(200,200), rl.RED)"),
    ("DrawLineEx", "(startPos, endPos, thick:float, color)",
     "Draw a thick line.", "rl.DrawLineEx(rl.Vector2(0,0), rl.Vector2(200,200), 5, rl.RED)"),
    ("DrawCircle", "(centerX:int, centerY:int, radius:float, color)",
     "Draw a filled circle.", "rl.DrawCircle(320, 240, 50, rl.GREEN)"),
    ("DrawCircleV", "(center, radius:float, color)",
     "Draw a filled circle from a vector.",
     "rl.DrawCircleV(rl.Vector2(320,240), 50, rl.GREEN)"),
    ("DrawCircleLines", "(centerX:int, centerY:int, radius:float, color)",
     "Draw a circle outline.", "rl.DrawCircleLines(320, 240, 50, rl.WHITE)"),
    ("DrawCircleGradient", "(center, radius:float, color1, color2)",
     "Draw a circle with a radial gradient.",
     "rl.DrawCircleGradient(rl.Vector2(320,240), 50, rl.PURPLE, rl.BLACK)"),
    ("DrawCircleSector", "(center, radius:float, startAngle:float, endAngle:float, segments:int, color)",
     "Draw a filled pie slice.",
     "rl.DrawCircleSector(rl.Vector2(320,240), 80, 0, 90, 32, rl.GOLD)"),
    ("DrawCircleSectorLines", "(center, radius:float, startAngle:float, endAngle:float, segments:int, color)",
     "Draw a pie slice outline.",
     "rl.DrawCircleSectorLines(rl.Vector2(320,240), 80, 0, 90, 32, rl.GOLD)"),
    ("DrawRing", "(center, innerRadius:float, outerRadius:float, startAngle:float, endAngle:float, segments:int, color)",
     "Draw a filled ring segment.",
     "rl.DrawRing(rl.Vector2(320,240), 40, 70, 0, 360, 48, rl.ORANGE)"),
    ("DrawRingLines", "(center, innerRadius:float, outerRadius:float, startAngle:float, endAngle:float, segments:int, color)",
     "Draw a ring outline.",
     "rl.DrawRingLines(rl.Vector2(320,240), 40, 70, 0, 360, 48, rl.ORANGE)"),
    ("DrawEllipse", "(centerX:int, centerY:int, radiusX:float, radiusY:float, color)",
     "Draw a filled ellipse.", "rl.DrawEllipse(320, 240, 120, 60, rl.PINK)"),
    ("DrawRectangle", "(x:int, y:int, width:int, height:int, color)",
     "Draw a filled rectangle.",
     "rl.DrawRectangle(10, 10, 200, 80, rl.BLUE)"),
    ("DrawRectangleV", "(position, size, color)",
     "Draw a rectangle from two vectors.",
     "rl.DrawRectangleV(rl.Vector2(10,10), rl.Vector2(200,80), rl.BLUE)"),
    ("DrawRectangleRec", "(rec, color)",
     "Draw a rectangle from a Rectangle value.",
     "rl.DrawRectangleRec(rl.Rectangle(10,10,200,80), rl.BLUE)"),
    ("DrawRectangleLines", "(x:int, y:int, width:int, height:int, color)",
     "Draw a rectangle outline.",
     "rl.DrawRectangleLines(10, 10, 200, 80, rl.WHITE)"),
    ("DrawRectangleLinesEx", "(rec, lineThick:float, color)",
     "Draw a rectangle outline with thickness.",
     "rl.DrawRectangleLinesEx(rl.Rectangle(10,10,200,80), 4, rl.WHITE)"),
    ("DrawRectangleRounded", "(rec, roundness:float, segments:int, color)",
     "Draw a filled rounded rectangle.",
     "rl.DrawRectangleRounded(rl.Rectangle(10,10,200,80), 0.3, 16, rl.PURPLE)"),
    ("DrawRectangleGradientV", "(x:int, y:int, width:int, height:int, color1, color2)",
     "Vertical gradient rectangle.",
     "rl.DrawRectangleGradientV(10,10,200,80, rl.RED, rl.BLUE)"),
    ("DrawRectangleGradientH", "(x:int, y:int, width:int, height:int, color1, color2)",
     "Horizontal gradient rectangle.",
     "rl.DrawRectangleGradientH(10,10,200,80, rl.RED, rl.BLUE)"),
    ("DrawTriangle", "(v1, v2, v3, color)",
     "Draw a filled triangle.",
     "rl.DrawTriangle(rl.Vector2(0,0), rl.Vector2(200,0), rl.Vector2(100,200), rl.GREEN)"),
    ("DrawPoly", "(center, sides:int, radius:float, rotation:float, color)",
     "Draw a filled regular polygon.",
     "rl.DrawPoly(rl.Vector2(320,240), 6, 80, 0, rl.GOLD)"),
    ("DrawPolyLines", "(center, sides:int, radius:float, rotation:float, color)",
     "Draw a polygon outline.",
     "rl.DrawPolyLines(rl.Vector2(320,240), 6, 80, 0, rl.GOLD)"),
    ("DrawText", "(text:str, x:int, y:int, fontSize:int, color)",
     "Draw a string with the default font.",
     "rl.DrawText(\"hello\", 10, 10, 20, rl.WHITE)"),
    ("DrawFPS", "(x:int, y:int)", "Draw the current FPS.",
     "rl.DrawFPS(W - 50, 10)"),
    ("MeasureText", "(text:str, fontSize:int) -> int",
     "Pixel width of a string.", "w = rl.MeasureText(\"hello\", 20)"),
    ("IsKeyDown", "(key:int) -> bool", "Is a key currently held?",
     "if rl.IsKeyDown(rl.KEY_SPACE):"),
    ("IsKeyPressed", "(key:int) -> bool", "Was a key pressed this frame?",
     "if rl.IsKeyPressed(rl.KEY_ENTER):"),
    ("IsKeyReleased", "(key:int) -> bool", "Was a key released this frame?",
     "if rl.IsKeyReleased(rl.KEY_SPACE):"),
    ("IsKeyUp", "(key:int) -> bool", "Is a key currently up?",
     "if rl.IsKeyUp(rl.KEY_ESCAPE):"),
    ("GetKeyPressed", "() -> int", "Last key pressed (0 if none).",
     "key = rl.GetKeyPressed()"),
    ("GetCharPressed", "() -> int", "Last char typed.",
     "ch = rl.GetCharPressed()"),
    ("IsMouseButtonDown", "(button:int) -> bool",
     "Is a mouse button held? (MOUSE_BUTTON_*)",
     "if rl.IsMouseButtonDown(rl.MOUSE_BUTTON_LEFT):"),
    ("IsMouseButtonPressed", "(button:int) -> bool",
     "Was a mouse button pressed this frame?",
     "if rl.IsMouseButtonPressed(rl.MOUSE_BUTTON_LEFT):"),
    ("GetMouseX", "() -> int", "Current mouse X.", "x = rl.GetMouseX()"),
    ("GetMouseY", "() -> int", "Current mouse Y.", "y = rl.GetMouseY()"),
    ("GetMousePosition", "() -> tuple", "Current mouse position.",
     "p = rl.GetMousePosition()"),
    ("GetMouseDelta", "() -> tuple", "Mouse movement since last frame.",
     "d = rl.GetMouseDelta()"),
    ("GetMouseWheelMove", "() -> float", "Mouse wheel movement.",
     "w = rl.GetMouseWheelMove()"),
    ("SetMousePosition", "(x:int, y:int)", "Move the OS mouse cursor.",
     "rl.SetMousePosition(320, 240)"),
    ("SetMouseScale", "(scaleX:float, scaleY:float)",
     "Scale mouse movement.", "rl.SetMouseScale(2.0, 2.0)"),
    ("LoadTexture", "(filename:str) -> dict",
     "Load a texture from a file.", "tex = rl.LoadTexture(\"img.png\")"),
    ("UnloadTexture", "(texture)", "Free a texture.", "rl.UnloadTexture(tex)"),
    ("DrawTexture", "(texture, x:int, y:int, color)",
     "Draw a texture at x,y.", "rl.DrawTexture(tex, 0, 0, rl.WHITE)"),
    ("DrawTextureEx", "(texture, position, rotation:float, scale:float, color)",
     "Draw a texture with rotation and scale.",
     "rl.DrawTextureEx(tex, rl.Vector2(100,100), 45, 1.5, rl.WHITE)"),
    ("ToggleBorderlessWindowed", "()", "Toggle borderless windowed.",
     "rl.ToggleBorderlessWindowed()"),
    ("MaximizeWindow", "()", "Maximize the window.", "rl.MaximizeWindow()"),
    ("MinimizeWindow", "()", "Minimize the window.", "rl.MinimizeWindow()"),
    ("RestoreWindow", "()", "Restore the window.", "rl.RestoreWindow()"),
    ("SetWindowTitle", "(title:str)", "Change the window title.",
     "rl.SetWindowTitle(\"new\")"),
    ("SetWindowSize", "(width:int, height:int)", "Resize the window.",
     "rl.SetWindowSize(800, 600)"),
    ("ClearWindowState", "(flags:int)", "Clear window state flags.",
     "rl.ClearWindowState(rl.FLAG_WINDOW_RESIZABLE)"),
    ("SetWindowState", "(flags:int)", "Set window state flags.",
     "rl.SetWindowState(rl.FLAG_WINDOW_RESIZABLE)"),
    ("IsWindowState", "(flags:int) -> bool", "Is a window state flag set?",
     "if rl.IsWindowState(rl.FLAG_WINDOW_RESIZABLE):"),
    ("LoadRenderTexture", "(width:int, height:int) -> dict",
     "Create an offscreen texture.", "target = rl.LoadRenderTexture(400, 300)"),
    ("BeginTextureMode", "(target)", "Draw into an offscreen texture.",
     "rl.BeginTextureMode(target)"),
    ("EndTextureMode", "()", "Stop drawing into a texture.",
     "rl.EndTextureMode()"),
    ("LoadFont", "(filename:str) -> dict", "Load a font.",
     "font = rl.LoadFont(\"font.ttf\")"),
    ("DrawTextEx", "(font, text:str, position, fontSize:float, spacing:float, color)",
     "Draw text with a custom font.",
     "rl.DrawTextEx(font, \"hi\", rl.Vector2(10,10), 20, 1, rl.WHITE)"),
    ("BeginMode2D", "(camera)", "Enter 2D camera mode.",
     "cam = rl.Camera2D(offset=rl.Vector2(0,0), target=rl.Vector2(0,0))\nrl.BeginMode2D(cam)"),
    ("EndMode2D", "()", "Leave 2D camera mode.", "rl.EndMode2D()"),
    ("CheckCollisionRecs", "(rec1, rec2) -> bool",
     "Do two rectangles overlap?",
     "if rl.CheckCollisionRecs(rl.Rectangle(0,0,10,10), rl.Rectangle(5,5,20,20)):"),
    ("CheckCollisionCircles", "(center1, radius1:float, center2, radius2:float) -> bool",
     "Do two circles overlap?",
     "rl.CheckCollisionCircles(rl.Vector2(0,0), 5, rl.Vector2(3,0), 5)"),
    ("CheckCollisionPointRec", "(point, rec) -> bool",
     "Is a point inside a rectangle?",
     "rl.CheckCollisionPointRec(rl.Vector2(5,5), rl.Rectangle(0,0,10,10))"),
    ("CheckCollisionPointCircle", "(point, center, radius:float) -> bool",
     "Is a point inside a circle?",
     "rl.CheckCollisionPointCircle(rl.Vector2(1,0), rl.Vector2(0,0), 5)"),
    ("CheckCollisionPointLine", "(point, p1, p2, threshold:int) -> bool",
     "Is a point on a line segment?",
     "rl.CheckCollisionPointLine(rl.Vector2(5,5), rl.Vector2(0,0), rl.Vector2(10,10), 1)"),
    ("GetCollisionRec", "(rec1, rec2) -> tuple",
     "Overlapping rectangle area.",
     "overlap = rl.GetCollisionRec(rl.Rectangle(0,0,10,10), rl.Rectangle(5,5,20,20))"),
    ("InitAudioDevice", "()", "Init the audio device.",
     "rl.InitAudioDevice()"),
    ("CloseAudioDevice", "()", "Close the audio device.",
     "rl.CloseAudioDevice()"),
    ("AudioDeviceReady", "() -> bool", "Is audio ready?",
     "if rl.AudioDeviceReady():"),
    ("SetMasterVolume", "(volume:float)", "Master volume 0..1.",
     "rl.SetMasterVolume(0.5)"),
    ("LoadSound", "(filename:str) -> dict", "Load a sound effect.",
     "sound = rl.LoadSound(\"shot.wav\")"),
    ("PlaySound", "(sound)", "Play a sound.", "rl.PlaySound(sound)"),
    ("StopSound", "(sound)", "Stop a sound.", "rl.StopSound(sound)"),
    ("SetSoundVolume", "(sound, volume:float)", "Sound volume 0..1.",
     "rl.SetSoundVolume(sound, 0.8)"),
    ("LoadMusicStream", "(filename:str) -> dict", "Load a music stream.",
     "music = rl.LoadMusicStream(\"loop.mp3\")"),
    ("PlayMusicStream", "(music)", "Play a music stream.", "rl.PlayMusicStream(music)"),
    ("UpdateMusicStream", "(music)", "Feed a music stream.", "rl.UpdateMusicStream(music)"),
    ("StopMusicStream", "(music)", "Stop a music stream.", "rl.StopMusicStream(music)"),
    ("SetMusicVolume", "(music, volume:float)", "Music volume 0..1.",
     "rl.SetMusicVolume(music, 0.5)"),
    ("Vector2", "(x:float, y:float) -> Vector2", "Build a 2D vector.",
     "p = rl.Vector2(100, 100)"),
    ("Vector3", "(x:float, y:float, z:float) -> Vector3",
     "Build a 3D vector.", "v = rl.Vector3(1, 2, 3)"),
    ("Rectangle", "(x:float, y:float, width:float, height:float) -> Rectangle",
     "Build a rectangle value.", "r = rl.Rectangle(10, 10, 200, 80)"),
    ("Color", "(r:int, g:int, b:int, a:int) -> Color",
     "Build an RGBA color value.", "c = rl.Color(255, 0, 0, 255)"),
]

# ---------------------------------------------------------------------------
# Constants (isim, deger, kategori, aciklama)
# ---------------------------------------------------------------------------
CONSTANTS = [
    # colors
    ("RAYWHITE", "(245,245,245,255)", "Colors", "White-ish default background."),
    ("WHITE",    "(255,255,255,255)", "Colors", "Pure white."),
    ("BLACK",    "(0,0,0,255)",       "Colors", "Pure black."),
    ("GRAY",     "(130,130,130,255)", "Colors", "Mid gray."),
    ("DARKGRAY", "(80,80,80,255)",    "Colors", "Dark gray."),
    ("LIGHTGRAY","(200,200,200,255)", "Colors", "Light gray."),
    ("RED",      "(230,41,55,255)",   "Colors", "Standard red."),
    ("GREEN",    "(0,228,48,255)",    "Colors", "Standard green."),
    ("BLUE",     "(0,121,241,255)",   "Colors", "Standard blue."),
    ("SKYBLUE",  "(102,191,255,255)", "Colors", "Sky blue."),
    ("YELLOW",   "(253,249,0,255)",   "Colors", "Yellow."),
    ("GOLD",     "(255,203,0,255)",   "Colors", "Gold."),
    ("ORANGE",   "(255,161,0,255)",   "Colors", "Orange."),
    ("PURPLE",   "(200,122,255,255)", "Colors", "Purple."),
    ("PINK",     "(255,109,194,255)", "Colors", "Pink."),
    ("LIME",     "(0,158,47,255)",    "Colors", "Lime green."),
    ("BROWN",    "(127,106,79,255)",  "Colors", "Brown."),
    ("BEIGE",    "(211,176,131,255)", "Colors", "Beige."),
    ("MAGENTA",  "(255,0,255,255)",   "Colors", "Magenta."),
    ("MAROON",   "(190,33,55,255)",   "Colors", "Maroon."),
    # keyboard
    ("KEY_A", "65", "Keyboard", "Letter key A."),
    ("KEY_B", "66", "Keyboard", "Letter key B."),
    ("KEY_W", "87", "Keyboard", "Letter key W."),
    ("KEY_SPACE", "32", "Keyboard", "Space bar."),
    ("KEY_ENTER", "257", "Keyboard", "Enter key."),
    ("KEY_ESCAPE", "256", "Keyboard", "Escape key (closes window by default)."),
    ("KEY_UP", "265", "Keyboard", "Arrow up."),
    ("KEY_DOWN", "264", "Keyboard", "Arrow down."),
    ("KEY_LEFT", "263", "Keyboard", "Arrow left."),
    ("KEY_RIGHT", "262", "Keyboard", "Arrow right."),
    ("KEY_F1", "290", "Keyboard", "Function key F1."),
    # mouse
    ("MOUSE_BUTTON_LEFT", "0", "Mouse", "Left mouse button."),
    ("MOUSE_BUTTON_RIGHT", "1", "Mouse", "Right mouse button."),
    ("MOUSE_BUTTON_MIDDLE", "2", "Mouse", "Middle mouse button."),
    # window flags
    ("FLAG_VSYNC_HINT", "0x40", "Window flags", "Enable vsync."),
    ("FLAG_FULLSCREEN_MODE", "0x02", "Window flags", "Start fullscreen."),
    ("FLAG_WINDOW_RESIZABLE", "0x04", "Window flags", "Allow resizing."),
    ("FLAG_WINDOW_UNDECORATED", "0x08", "Window flags", "No title bar."),
    ("FLAG_WINDOW_HIDDEN", "0x80", "Window flags", "Start hidden."),
    ("FLAG_WINDOW_MINIMIZED", "0x200", "Window flags", "Start minimized."),
    ("FLAG_WINDOW_MAXIMIZED", "0x400", "Window flags", "Start maximized."),
    ("FLAG_WINDOW_HIGHDPI", "0x2000", "Window flags", "High DPI support."),
    ("FLAG_MSAA_4X_HINT", "0x20", "Window flags", "MSAA 4x hint."),
    ("FLAG_WINDOW_MOUSE_PASSTHROUGH", "0x4000", "Window flags", "Clicks pass through."),
    ("FLAG_BORDERLESS_WINDOWED_MODE", "0x8000", "Window flags", "Borderless windowed."),
    ("FLAG_WINDOW_TOPMOST", "0x1000", "Window flags", "Always on top."),
    ("FLAG_INTERLACED_HINT", "0x10000", "Window flags", "Interlaced video hint."),
    ("FLAG_WINDOW_UNFOCUSED", "0x800", "Window flags", "Start unfocused."),
    # camera / misc
    ("CAMERA_ORTHOGRAPHIC", "1", "Camera", "Orthographic 2D-style camera."),
    ("CAMERA_PERSPECTIVE", "0", "Camera", "Perspective 3D camera."),
]

# ---------------------------------------------------------------------------
# GCL dilinin kendi fonksiyonlari ve on-islemci direktifleri (built-in):
# (name, signature, aciklama, ornek)
# ---------------------------------------------------------------------------
GCL_FUNCS = [
    ("printf", "(fmt:str, args...) -> int",
     "Format output: %d %s %c %f.",
     'printf("ADD: %d\\n", 7)'),
    ("scanf", "(\"%type\", var) -> int",
     "Safe stdin read: %s %d %f %c.",
     'scanf("%d", x)'),
    ("malloc", "(reserve:count) -> int*",
     "Fixed-capacity int list (no auto-grow).",
     'int *Fixed = malloc(reserve=2);'),
    ("gcMalloc", "(reserve:count, extra:n) -> int*",
     "Auto-growing int list (grows when full).",
     'int *List = gcMalloc(reserve=2, extra=3);'),
    ("free", "(ptr) or var.free()",
     "Release a heap value (double-free warns).",
     'free(List);'),
    ("sizeof", "(type) or (variable) -> size",
     "Size in bytes of a type or value.",
     'sizeof(int)'),
    ("#include", "<name> or \"name\"",
     "Merge another .gcsf file into this script.",
     '#include <test_include>'),
    ("#lib", "<name> or \"name\"",
     "Merge a .gclib library file.",
     '#lib <test_library>'),
    ("#extern", "<dll> or \"dll\"",
     "Load a native shared library.",
     '#extern <raylib.dll>'),
    ("#register", "ret name(params);",
     "Declare a native function from #extern.",
     '#register void InitWindow(int w, int h, const char *t);'),
    ("#define", "NAME value",
     "Define a macro constant.",
     '#define MY_MACRO 42'),
    ("#warning", "\"text\"",
     "Print a yellow warning (does not stop the build).",
     '#warning "check this"'),
    ("#error", "\"text\"",
     "Print a red error and stop the build.",
     '#error "raylib.dll is not exist"'),
]

# ---------------------------------------------------------------------------
# yazicilar
# ---------------------------------------------------------------------------

def _startswith_any(name: str, prefixes: tuple[str, ...]) -> bool:
    return any(name.startswith(p) for p in prefixes)


def categorize_funcs(funcs):
    """Duzy fonksiyon listesini ad on ekine gore kategorilere boler.
    Boylece .gcReference icinde her bolum (Window/Drawing/Input/...) kendi
    basligi altinda toplanir — takip ve okuma kolay olur."""
    order = [
        ("Window", ( "InitWindow", "CloseWindow", "WindowShouldClose",
                     "GetScreenWidth", "GetScreenHeight", "SetTargetFPS",
                     "SetConfigFlags", "SetExitKey", "ToggleFullscreen",
                     "TakeScreenshot", "GetTime", "GetFrameTime", "GetFPS",
                     "ToggleBorderlessWindowed", "MaximizeWindow",
                     "MinimizeWindow", "RestoreWindow", "SetWindowTitle",
                     "SetWindowSize", "ClearWindowState", "SetWindowState",
                     "IsWindowState" )),
        ("Drawing", ( "BeginDrawing", "EndDrawing", "ClearBackground", "Draw",
                      "DrawFPS" )),
        ("Input", ( "IsKey", "GetKeyPressed", "GetCharPressed", "IsMouse",
                    "GetMouse", "SetMouse", "SetExitKey" )),
        ("Textures", ( "LoadTexture", "UnloadTexture", "DrawTexture",
                       "LoadRenderTexture", "BeginTextureMode",
                       "EndTextureMode" )),
        ("Fonts & Text", ( "LoadFont", "DrawTextEx", "MeasureText" )),
        ("2D Camera", ( "BeginMode2D", "EndMode2D" )),
        ("Collision", ( "CheckCollision", "GetCollisionRec" )),
        ("Audio", ( "InitAudioDevice", "CloseAudioDevice", "AudioDeviceReady",
                    "SetMasterVolume", "LoadSound", "PlaySound", "StopSound",
                    "SetSoundVolume", "LoadMusicStream", "PlayMusicStream",
                    "UpdateMusicStream", "StopMusicStream",
                    "SetMusicVolume" )),
        ("Value Types", ( "Vector2", "Vector3", "Rectangle", "Color" )),
    ]
    rest: list = []
    out: list = []
    for cat, prefixes in order:
        matched = [f for f in funcs if _startswith_any(f[0], prefixes)]
        if not matched:
            continue
        out.append((cat, matched))
        rest.extend(f for f in matched)
    leftover = [f for f in funcs if f not in rest]
    if leftover:
        out.append(("Other", leftover))
    return out


def write_ref(path, title, intro, func_cats, constants):
    """func_cats: [(kategori_adi, [(name, sig, desc, example), ...]), ...]
    Her kategori kendi basligi altinda toplanir — okunabilirlik ve takip
    icin fonksiyonlar gruplu gosterilir."""
    lines = [f"# GCL Reference: {title}", f"# {intro}", ""]

    lines.append("## functions")
    total = sum(len(items) for _, items in func_cats)
    if total:
        for cat, funcs in func_cats:
            lines.append(f"### {cat}")
            for name, sig, desc, example in funcs:
                lines.append(f"- {name}{sig}")
                lines.append(f'    "{desc}"')
                if example:
                    lines.append("    ```")
                    lines.append(f"    {example}")
                    lines.append("    ```")
                lines.append("")
    else:
        lines.append("(none)")
        lines.append("")

    # constants kategorilere ayrilmis
    lines.append("## constants")
    cats = {}
    for name, value, cat, desc in constants:
        cats.setdefault(cat, []).append((name, value, desc))
    for cat in ["Colors", "Keyboard", "Mouse", "Window flags", "Camera"]:
        items = cats.get(cat, [])
        if not items:
            continue
        lines.append(f"### {cat}")
        for name, value, desc in items:
            lines.append(f"- {name} (= {value})")
            if desc:
                lines.append(f'    "{desc}"')
        lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def write_docs(out):
    """lua.doc, py.doc ve gcl.doc'u Library/ klasorlerine kopyalar."""
    for src, dst_rel in [(LUA_DOC, os.path.join("Lua", "lua.doc")),
                         (PY_DOC, os.path.join("Python", "py.doc")),
                         (GCL_DOC, os.path.join("GCL", "gcl.doc"))]:
        if os.path.isfile(src):
            dst = os.path.join(out, "Library", dst_rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
        else:
            print(f"gen_reference.py: warning: dokuman yok -> {src}")


def main():
    if len(sys.argv) < 2:
        print("usage: gen_reference.py <build_out>")
        return 1
    out = sys.argv[1]
    lua_dir = os.path.join(out, "Library", "Lua")
    py_dir = os.path.join(out, "Library", "Python")
    gcl_dir = os.path.join(out, "Library", "GCL")
    bridge_dir = os.path.join(out, "Library", "bridge")
    os.makedirs(lua_dir, exist_ok=True)
    os.makedirs(py_dir, exist_ok=True)
    os.makedirs(gcl_dir, exist_ok=True)
    os.makedirs(bridge_dir, exist_ok=True)

    write_docs(out)

    # 1) GCL built-in fonksiyonlar + direktifler
    write_ref(os.path.join(gcl_dir, "gcl.gcReference"),
              "GCL built-ins",
              "printf/scanf, malloc/gcMalloc/free, #include/#lib/#extern "
              "and preprocessor directives",
              [("Built-ins", [
                  f for f in GCL_FUNCS
                  if not f[0].startswith("#")
              ]),
               ("Preprocessor", [
                  f for f in GCL_FUNCS
                  if f[0].startswith("#")
              ])],
              [])

    # 2) modul export'lari (sabit)
    write_ref(os.path.join(lua_dir, "lua.gcReference"),
              "Lua Embed (gcdl_loader)", "direct .gcDL exports",
              [("Module", [
                  ("gcdl_lua_version", "() -> str", "Lua release version.",
                   'local v = gcdl_lua_version()'),
                  ("gcdl_lua_run", "(script:str, debug:int, err:str, err_cap:int) -> int",
                   "Run a Lua script through GCL.",
                   'gcdl_lua_run("demo.lua", 1, err, 4096)'),
              ])],
              [])
    write_ref(os.path.join(bridge_dir, "bridge.gcReference"),
              "Cross-language bridge (Lua <-> Python)",
              "shared key/value store in Library/bridge/<session>/*.gcv",
              [("Module", [
                  ("gcdl_bridge_open", "(session:str) -> int",
                   "Select the session folder for all later calls.",
                   'gcdl_bridge_open("ball_war")'),
                  ("gcdl_bridge_set", "(key:str, value:str) -> int",
                   "Atomically write a key.", 'gcdl_bridge_set("ball_py", "x=10")'),
                  ("gcdl_bridge_get", "(key:str, out:str, cap:int) -> int",
                   "Read a key (0 found, 1 missing).",
                   'gcdl_bridge_get("ball_py", buf, sizeof buf)'),
                  ("gcdl_bridge_delete", "(key:str) -> int",
                   "Remove a key.", 'gcdl_bridge_delete("ball_py")'),
                  ("gcdl_bridge_list", "(out:str, cap:int) -> int",
                   "Comma separated keys.",
                   'gcdl_bridge_list(buf, sizeof buf)'),
              ])],
              [])

    # 2) lua_raylib imzali + ornekli
    write_ref(os.path.join(lua_dir, "lua_raylib.gcReference"),
              "gcl.raylib (Lua binding)",
              "window, drawing, colors, keys — call via rl = gcl.raylib",
              categorize_funcs(LUA_RAYLIB_FUNCS), CONSTANTS)

    # 3) python embed modulu (python.gcDL gercek export'lari)
    write_ref(os.path.join(py_dir, "python.gcReference"),
              "Python Embed (gcdl_loader)",
              "direct .gcDL exports of python.gcDL",
              [("Module", [
                  ("gcdl_python_version", "() -> str",
                   "Python release version.",
                   'print(gcdl_python_version())'),
                  ("gcdl_python_run", "(script:str, err:str, err_cap:int) -> int",
                   "Run a Python script through GCL.",
                   'gcdl_python_run("demo.py", err, 4096)'),
                  ("gcdl_python_module", "(modname:str, mod_argc:int, argv:list, err:str, err_cap:int) -> int",
                   "Run a Python module by name (like `python -m <mod>`).",
                   'gcdl_python_module("mypkg.main", 0, None, err, 4096)'),
              ])],
              [])

    # 4) pyRaylib imzali + ornekli (todo #5: python_raylib.gcReference
    #    KALDIRILDI — ayni icerik zaten pyRaylib.gcReference'ta var;
    #    DOCS panelinde iki kez listeleniyordu)
    write_ref(os.path.join(py_dir, "pyRaylib.gcReference"),
              "pyRaylib (Python raylib wrapper)",
              "window, drawing, colors, keys — call via import pyRaylib as rl",
              categorize_funcs(PY_RAYLIB_FUNCS), CONSTANTS)

    print("gen_reference.py: .gcReference files written -> " + out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
