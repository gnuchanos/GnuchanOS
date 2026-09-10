#ifndef GCL_LSP_INTERNAL_H
#define GCL_LSP_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LSP_KIND_UNKNOWN = 0,
    LSP_KIND_FUNC,
    LSP_KIND_VAR,
    LSP_KIND_MACRO,
    LSP_KIND_TYPE,
} LspKind;

typedef enum {
    LSP_VIS_PUBLIC = 0,
    LSP_VIS_PRIVATE = 1,
} LspVis;

typedef struct LspSymbol {
    char *name;
    LspKind kind;
    LspVis vis;
    char *detail;
    char *params;
    char *file;

    struct LspSymbol *members;
    int member_count;
    int member_cap;
} LspSymbol;

/* GCL builtin / primitive types (also used as declaration-context type names).
   lib/.gclib support removed — only GCL + its native modules. */
static const char *gcl_builtin_types[] = {
    "int", "short", "long", "float", "double", "char", "bool", "void",
    "unsigned", "signed", "const", "global", "inline",
    "int8", "int16", "int32", "int64", "int128",
    "uint8", "uint16", "uint32", "uint64", "uint128",
    "float16", "float32", "float64", "float128",
    "gcChar", "size_t",
    NULL
};

/* Python completion disabled for now (user: "lua and python are not needed right now") */
static const char *gcl_python_keywords[] = { NULL };

/* ---- Raylib (GCL native module) ----
   gcl_Raylib_types[] = ALL raylib names (functions + types/constants).
   gcl_Raylib_type_names[] = only TYPE/constant names (for is_type classification).
   Extracted from Modules/gcl_raylib.c g_entries[]. */
static const char *gcl_Raylib_types[] = {
    "InitWindow","CloseWindow","WindowShouldClose","IsWindowReady","IsWindowFullscreen",
    "IsWindowHidden","IsWindowMinimized","IsWindowMaximized","IsWindowFocused","IsWindowResized",
    "IsWindowState","SetWindowState","ClearWindowState","ToggleFullscreen","ToggleBorderlessWindowed",
    "MaximizeWindow","MinimizeWindow","RestoreWindow","SetWindowIcon","SetWindowIcons",
    "SetWindowTitle","SetWindowPosition","SetWindowMonitor","SetWindowMinSize","SetWindowMaxSize",
    "SetWindowSize","SetWindowOpacity","SetWindowFocused","GetWindowHandle","GetScreenWidth",
    "GetScreenHeight","GetRenderWidth","GetRenderHeight","GetMonitorCount","GetCurrentMonitor",
    "GetMonitorPosition","GetMonitorWidth","GetMonitorHeight","GetMonitorPhysicalWidth","GetMonitorPhysicalHeight",
    "GetMonitorRefreshRate","GetWindowPosition","GetWindowScaleDPI","GetMonitorName","SetClipboardText",
    "GetClipboardText","GetClipboardImage","EnableEventWaiting","DisableEventWaiting","ShowCursor",
    "HideCursor","IsCursorHidden","EnableCursor","DisableCursor","IsCursorOnScreen",
    "ClearBackground","BeginDrawing","EndDrawing","BeginMode2D","EndMode2D",
    "BeginMode3D","EndMode3D","BeginTextureMode","EndTextureMode","BeginShaderMode",
    "EndShaderMode","BeginBlendMode","EndBlendMode","BeginScissorMode","EndScissorMode",
    "BeginVrStereoMode","EndVrStereoMode","LoadVrStereoConfig","UnloadVrStereoConfig","LoadShader",
    "LoadShaderFromMemory","IsShaderValid","GetShaderLocation","GetShaderLocationAttrib","SetShaderValue",
    "SetShaderValueV","SetShaderValueMatrix","SetShaderValueTexture","UnloadShader","GetScreenToWorldRay",
    "GetScreenToWorldRayEx","GetWorldToScreen","GetWorldToScreenEx","GetWorldToScreen2D","GetScreenToWorld2D",
    "GetCameraMatrix","GetCameraMatrix2D","SetTargetFPS","GetFrameTime","GetTime",
    "GetFPS","SwapScreenBuffer","PollInputEvents","WaitTime","SetRandomSeed",
    "GetRandomValue","LoadRandomSequence","UnloadRandomSequence","TakeScreenshot","SetConfigFlags",
    "OpenURL","SetTraceLogLevel","TraceLog","SetTraceLogCallback","MemAlloc",
    "MemRealloc","MemFree","LoadFileData","UnloadFileData","SaveFileData",
    "ExportDataAsCode","LoadFileText","UnloadFileText","SaveFileText","SetLoadFileDataCallback",
    "SetSaveFileDataCallback","SetLoadFileTextCallback","SetSaveFileTextCallback","FileRename","FileRemove",
    "FileCopy","FileMove","FileTextReplace","FileTextFindIndex","FileExists",
    "DirectoryExists","IsFileExtension","GetFileLength","GetFileModTime","GetFileExtension",
    "GetFileName","GetFileNameWithoutExt","GetDirectoryPath","GetPrevDirectoryPath","GetWorkingDirectory",
    "GetApplicationDirectory","MakeDirectory","ChangeDirectory","IsPathFile","IsPathDirectory",
    "IsPathAbsolute","IsFileNameValid","LoadDirectoryFiles","LoadDirectoryFilesEx","UnloadDirectoryFiles",
    "IsFileDropped","LoadDroppedFiles","UnloadDroppedFiles","GetDirectoryFileCount","GetDirectoryFileCountEx",
    "CompressData","DecompressData","EncodeDataBase64","DecodeDataBase64","ComputeCRC32",
    "ComputeMD5","ComputeSHA1","ComputeSHA256","LoadAutomationEventList","UnloadAutomationEventList",
    "ExportAutomationEventList","SetAutomationEventList","SetAutomationEventBaseFrame","StartAutomationEventRecording","StopAutomationEventRecording",
    "PlayAutomationEvent","IsKeyPressed","IsKeyPressedRepeat","IsKeyDown","IsKeyReleased",
    "IsKeyUp","GetKeyPressed","GetCharPressed","GetKeyName","SetExitKey",
    "IsGamepadAvailable","GetGamepadName","IsGamepadButtonPressed","IsGamepadButtonDown","IsGamepadButtonReleased",
    "IsGamepadButtonUp","GetGamepadButtonPressed","GetGamepadAxisCount","GetGamepadAxisMovement","SetGamepadMappings",
    "SetGamepadVibration","IsMouseButtonPressed","IsMouseButtonDown","IsMouseButtonReleased","IsMouseButtonUp",
    "GetMouseX","GetMouseY","GetMousePosition","GetMouseDelta","SetMousePosition",
    "SetMouseOffset","SetMouseScale","GetMouseWheelMove","GetMouseWheelMoveV","SetMouseCursor",
    "GetTouchX","GetTouchY","GetTouchPosition","GetTouchPointId","GetTouchPointCount",
    "SetGesturesEnabled","IsGestureDetected","GetGestureDetected","GetGestureHoldDuration","GetGestureDragVector",
    "GetGestureDragAngle","GetGesturePinchVector","GetGesturePinchAngle","UpdateCamera","UpdateCameraPro",
    "SetShapesTexture","GetShapesTexture","GetShapesTextureRectangle","DrawPixel","DrawPixelV",
    "DrawLine","DrawLineV","DrawLineEx","DrawLineStrip","DrawLineBezier",
    "DrawLineDashed","DrawTriangle","DrawTriangleLines","DrawTriangleFan","DrawTriangleStrip",
    "DrawRectangle","DrawRectangleV","DrawRectangleRec","DrawRectanglePro","DrawRectangleGradientV",
    "DrawRectangleGradientH","DrawRectangleGradientEx","DrawRectangleLines","DrawRectangleLinesEx","DrawRectangleRounded",
    "DrawRectangleRoundedLines","DrawRectangleRoundedLinesEx","DrawPoly","DrawPolyLines","DrawPolyLinesEx",
    "DrawCircle","DrawCircleV","DrawCircleGradient","DrawCircleSector","DrawCircleSectorLines",
    "DrawCircleSectorLinesEx","DrawCircleLines","DrawCircleLinesV","DrawCircleLinesEx","DrawEllipse",
    "DrawEllipseV","DrawEllipseLines","DrawEllipseLinesV","DrawEllipseLinesEx","DrawRing",
    "DrawRingLines","DrawRingLinesEx","DrawSplineLinear","DrawSplineBasis","DrawSplineCatmullRom",
    "DrawSplineBezierQuadratic","DrawSplineBezierCubic","DrawSplineSegmentLinear","DrawSplineSegmentBasis","DrawSplineSegmentCatmullRom",
    "DrawSplineSegmentBezierQuadratic","DrawSplineSegmentBezierCubic","GetSplinePointLinear","GetSplinePointBasis","GetSplinePointCatmullRom",
    "GetSplinePointBezierQuadratic","GetSplinePointBezierCubic","CheckCollisionRecs","CheckCollisionCircles","CheckCollisionCircleRec",
    "CheckCollisionCircleLine","CheckCollisionPointRec","CheckCollisionPointCircle","CheckCollisionPointTriangle","CheckCollisionPointLine",
    "CheckCollisionPointPoly","CheckCollisionLines","GetCollisionRec","LoadImage","LoadImageRaw",
    "LoadImageAnim","LoadImageAnimFromMemory","LoadImageFromMemory","LoadImageFromTexture","LoadImageFromScreen",
    "IsImageValid","UnloadImage","ExportImage","ExportImageToMemory","ExportImageAsCode",
    "GenImageColor","GenImageGradientLinear","GenImageGradientRadial","GenImageGradientSquare","GenImageChecked",
    "GenImageWhiteNoise","GenImagePerlinNoise","GenImageCellular","GenImageText","ImageCopy",
    "ImageFromImage","ImageFromChannel","ImageText","ImageTextEx","ImageFormat",
    "ImageToPOT","ImageCrop","ImageAlphaCrop","ImageAlphaClear","ImageAlphaMask",
    "ImageAlphaPremultiply","ImageBlurGaussian","ImageKernelConvolution","ImageResize","ImageResizeNN",
    "ImageResizeCanvas","ImageMipmaps","ImageDither","ImageFlipVertical","ImageFlipHorizontal",
    "ImageRotate","ImageRotateCW","ImageRotateCCW","ImageColorTint","ImageColorInvert",
    "ImageColorGrayscale","ImageColorContrast","ImageColorBrightness","ImageColorReplace","LoadImageColors",
    "LoadImagePalette","UnloadImageColors","UnloadImagePalette","GetImageAlphaBorder","GetImageColor",
    "ImageClearBackground","ImageDrawPixel","ImageDrawPixelV","ImageDrawLine","ImageDrawLineV",
    "ImageDrawLineEx","ImageDrawLineStrip","ImageDrawTriangle","ImageDrawTriangleGradient","ImageDrawTriangleLines",
    "ImageDrawTriangleFan","ImageDrawTriangleStrip","ImageDrawRectangle","ImageDrawRectangleV","ImageDrawRectangleRec",
    "ImageDrawRectanglePro","ImageDrawRectangleLines","ImageDrawRectangleLinesEx","ImageDrawRectangleGradientEx","ImageDrawCircle",
    "ImageDrawCircleV","ImageDrawCircleLines","ImageDrawCircleLinesV","ImageDrawCircleGradient","ImageDrawImage",
    "ImageDrawImageEx","ImageDrawImageRec","ImageDrawImagePro","ImageDrawText","ImageDrawTextEx",
    "ImageDrawTextPro","LoadTexture","LoadTextureFromImage","LoadTextureCubemap","LoadRenderTexture",
    "IsTextureValid","UnloadTexture","IsRenderTextureValid","UnloadRenderTexture","UpdateTexture",
    "UpdateTextureRec","GenTextureMipmaps","SetTextureFilter","SetTextureWrap","DrawTexture",
    "DrawTextureV","DrawTextureEx","DrawTextureRec","DrawTexturePro","DrawTextureNPatch",
    "GetFontDefault","LoadFont","LoadFontEx","LoadFontFromImage","LoadFontFromMemory",
    "IsFontValid","LoadFontData","GenImageFontAtlas","UnloadFontData","UnloadFont",
    "ExportFontAsCode","DrawFPS","DrawText","DrawTextEx","DrawTextPro",
    "DrawTextCodepoint","DrawTextCodepoints","SetTextLineSpacing","MeasureText","MeasureTextEx",
    "MeasureTextCodepoints","GetGlyphIndex","GetGlyphInfo","GetGlyphAtlasRec","LoadUTF8",
    "UnloadUTF8","LoadCodepoints","UnloadCodepoints","GetCodepointCount","GetCodepoint",
    "GetCodepointNext","GetCodepointPrevious","CodepointToUTF8","LoadTextLines","UnloadTextLines",
    "TextCopy","TextIsEqual","TextLength","TextFormat","TextSubtext",
    "TextRemoveSpaces","GetTextBetween","TextReplace","TextReplaceAlloc","TextReplaceBetween",
    "TextReplaceBetweenAlloc","TextInsert","TextInsertAlloc","TextJoin","TextSplit",
    "TextAppend","TextFindIndex","TextToUpper","TextToLower","TextToPascal",
    "TextToSnake","TextToCamel","TextToInteger","TextToFloat","DrawLine3D",
    "DrawPoint3D","DrawCircle3D","DrawTriangle3D","DrawTriangleStrip3D","DrawCube",
    "DrawCubeV","DrawCubeWires","DrawCubeWiresV","DrawSphere","DrawSphereEx",
    "DrawSphereWires","DrawCylinder","DrawCylinderEx","DrawCylinderWires","DrawCylinderWiresEx",
    "DrawCapsule","DrawCapsuleWires","DrawPlane","DrawRay","DrawGrid",
    "LoadModel","LoadModelFromMesh","IsModelValid","UnloadModel","GetModelBoundingBox",
    "DrawModel","DrawModelEx","DrawModelWires","DrawModelWiresEx","DrawBoundingBox",
    "DrawBillboard","DrawBillboardRec","DrawBillboardPro","UploadMesh","UpdateMeshBuffer",
    "UnloadMesh","DrawMesh","DrawMeshInstanced","GetMeshBoundingBox","GenMeshTangents",
    "ExportMesh","ExportMeshAsCode","GenMeshPoly","GenMeshPlane","GenMeshCube",
    "GenMeshSphere","GenMeshHemiSphere","GenMeshCylinder","GenMeshCone","GenMeshTorus",
    "GenMeshKnot","GenMeshHeightmap","GenMeshCubicmap","LoadMaterials","LoadMaterialDefault",
    "IsMaterialValid","UnloadMaterial","SetMaterialTexture","SetModelMeshMaterial","LoadModelAnimations",
    "UpdateModelAnimation","UpdateModelAnimationEx","UnloadModelAnimations","IsModelAnimationValid","CheckCollisionSpheres",
    "CheckCollisionBoxes","CheckCollisionBoxSphere","GetRayCollisionSphere","GetRayCollisionBox","GetRayCollisionMesh",
    "GetRayCollisionTriangle","GetRayCollisionQuad","InitAudioDevice","CloseAudioDevice","IsAudioDeviceReady",
    "SetMasterVolume","GetMasterVolume","LoadWave","LoadWaveFromMemory","IsWaveValid",
    "LoadSound","LoadSoundFromWave","LoadSoundAlias","IsSoundValid","UpdateSound",
    "UnloadWave","UnloadSound","UnloadSoundAlias","ExportWave","ExportWaveAsCode",
    "PlaySound","StopSound","PauseSound","ResumeSound","IsSoundPlaying",
    "SetSoundVolume","SetSoundPitch","SetSoundPan","WaveCopy","WaveCrop",
    "WaveFormat","LoadWaveSamples","UnloadWaveSamples","LoadMusicStream","LoadMusicStreamFromMemory",
    "IsMusicValid","UnloadMusicStream","PlayMusicStream","IsMusicStreamPlaying","UpdateMusicStream",
    "StopMusicStream","PauseMusicStream","ResumeMusicStream","SeekMusicStream","SetMusicVolume",
    "SetMusicPitch","SetMusicPan","GetMusicTimeLength","GetMusicTimePlayed","LoadAudioStream",
    "IsAudioStreamValid","UnloadAudioStream","UpdateAudioStream","IsAudioStreamProcessed","PlayAudioStream",
    "PauseAudioStream","ResumeAudioStream","IsAudioStreamPlaying","StopAudioStream","SetAudioStreamVolume",
    "SetAudioStreamPitch","SetAudioStreamPan","SetAudioStreamBufferSizeDefault","SetAudioStreamCallback","AttachAudioStreamProcessor",
    "DetachAudioStreamProcessor","AttachAudioMixedProcessor","DetachAudioMixedProcessor","Fade","ColorToInt",
    "ColorNormalize","ColorFromNormalized","ColorToHSV","ColorFromHSV","ColorTint",
    "ColorBrightness","ColorContrast","ColorAlpha","ColorAlphaBlend","ColorLerp",
    "GetColor","GetPixelColor","SetPixelColor","GetPixelDataSize","ColorIsEqual",
    "RAYWHITE","LIGHTGRAY","GRAY","DARKGRAY","YELLOW","GOLD","ORANGE","PINK","RED","MAROON",
    "GREEN","LIME","DARKGREEN","SKYBLUE","BLUE","DARKBLUE","PURPLE","VIOLET","DARKPURPLE","BEIGE",
    "BROWN","DARKBROWN","WHITE","BLACK","BLANK","MAGENTA",
    "Rectangle","Vector2","Vector3","Vector4","Matrix","Camera","Camera2D","Ray","BoundingBox",
    "NPatchInfo","GlyphInfo","VrStereoConfig","AutomationEvent",
    NULL
};

static const char *gcl_Raylib_type_names[] = {
    "RAYWHITE","LIGHTGRAY","GRAY","DARKGRAY","YELLOW","GOLD","ORANGE","PINK","RED","MAROON",
    "GREEN","LIME","DARKGREEN","SKYBLUE","BLUE","DARKBLUE","PURPLE","VIOLET","DARKPURPLE","BEIGE",
    "BROWN","DARKBROWN","WHITE","BLACK","BLANK","MAGENTA",
    "Rectangle","Vector2","Vector3","Vector4","Matrix","Camera","Camera2D","Ray","BoundingBox",
    "NPatchInfo","GlyphInfo","VrStereoConfig","AutomationEvent",
    NULL
};

/* ---- Raygui (GCL native module) — all functions, no type/constants ---- */
static const char *gcl_Raygui_types[] = {
    "GuiEnable","GuiDisable","GuiLock","GuiUnlock","GuiIsLocked",
    "GuiSetAlpha","GuiSetState","GuiGetState",
    "GuiSetFont","GuiGetFont",
    "GuiSetStyle","GuiGetStyle",
    "GuiLoadStyle","GuiLoadStyleFromMemory","GuiLoadStyleDefault",
    "GuiEnableTooltip","GuiDisableTooltip","GuiSetTooltip",
    "GuiIconText","GuiSetIconScale","GuiGetIcons","GuiLoadIcons",
    "GuiLoadIconsFromMemory","GuiDrawIcon",
    "GuiGetTextWidth",
    "GuiWindowBox","GuiGroupBox","GuiLine","GuiPanel","GuiScrollPanel",
    "GuiLabel","GuiButton","GuiLabelButton","GuiToggle","GuiToggleGroup",
    "GuiToggleSlider","GuiCheckBox","GuiComboBox","GuiDropdownBox","GuiSpinner",
    "GuiValueBox","GuiValueBoxFloat","GuiTextBox","GuiSlider","GuiSliderBar",
    "GuiProgressBar","GuiStatusBar","GuiDummyRec","GuiGrid",
    "GuiListView","GuiListViewEx","GuiTabBar","GuiTabBarEx",
    "GuiMessageBox","GuiTextInputBox","GuiColorPicker","GuiColorPanel",
    "GuiColorBarAlpha","GuiColorBarHue","GuiColorPickerHSV","GuiColorPanelHSV",
    NULL
};

static const char *gcl_Raygui_type_names[] = { NULL };

/* Python completion disabled for now */
static const char *gcl_python_raylib_names[] = { NULL };

static inline char *gcl_lsp_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static inline char *gcl_lsp_trim(const char *s) {
    if (!s) return NULL;
    const char *a = s; while (*a && isspace((unsigned char)*a)) a++;
    const char *b = s + strlen(s); while (b > a && isspace((unsigned char)*(b-1))) b--;
    size_t n = (size_t)(b - a);
    char *r = (char*)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, a, n);
    r[n] = '\0';
    return r;
}

static inline int gcl_lsp_is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline int gcl_lsp_is_ident_char(char c) {
    return gcl_lsp_is_ident_start(c) || (c >= '0' && c <= '9');
}

/* list helpers (inline so no linker symbols required) */
static inline LspSymbol *lsp_list_add(LspSymbol **out, int *out_count, int *cap,
                                     const char *name, LspKind kind, LspVis vis,
                                     const char *detail, const char *params, const char *file) {
    if (!out || !out_count || !cap) return NULL;
    if (*out_count >= *cap) {
        int ncap = (*cap) ? (*cap * 2) : 8;
        LspSymbol *n = (LspSymbol*)realloc(*out, (size_t)ncap * sizeof(LspSymbol));
        if (!n) return NULL;
        *out = n; *cap = ncap;
    }
    LspSymbol *s = &(*out)[*out_count];
    s->name = gcl_lsp_strdup(name);
    s->kind = kind;
    s->vis = vis;
    s->detail = detail ? gcl_lsp_strdup(detail) : NULL;
    s->params = params ? gcl_lsp_strdup(params) : NULL;
    s->file = file ? gcl_lsp_strdup(file) : NULL;
    s->members = NULL; s->member_count = 0; s->member_cap = 0;
    (*out_count)++;
    return s;
}

static inline void lsp_symbol_free(LspSymbol *s) {
    if (!s) return;
    if (s->name) free(s->name);
    if (s->detail) free(s->detail);
    if (s->params) free(s->params);
    if (s->file) free(s->file);
    if (s->members) {
        for (int i = 0; i < s->member_count; i++) lsp_symbol_free(&s->members[i]);
        free(s->members);
    }
}

static inline void lsp_list_clear(LspSymbol **out, int *out_count) {
    if (!out || !*out) return;
    for (int i = 0; i < *out_count; i++) {
        lsp_symbol_free(&(*out)[i]);
    }
    free(*out);
    *out = NULL; *out_count = 0;
}

#ifdef __cplusplus
}
#endif

#endif
