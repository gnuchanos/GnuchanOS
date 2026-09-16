#!/usr/bin/env python3
"""
gen_native_db.py — GENERATES the Raylib/Raygui native API database.

In `language/_SRC/Modules/gcl_raylib.c` and `gcl_raygui.c`:
  * member names + categories from the `g_entries[] = { ... }` table,
  * the RETURN TYPE from assignments such as `g_last_v2=`, `reg_tex(`, `g_str`
    inside the bodies of `fn_<NAME>(...) { ... }`
are extracted and written to `language/_SRC/complete/complete_native_db.c`.

Do not edit by hand: single source (Modules/*.c) → generated table.
Run:  python tools/gen_native_db.py
"""
from __future__ import annotations

import os
import platform
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent          # repo root
MODULES = ROOT / "language" / "_SRC" / "Modules"
OUT = ROOT / "language" / "_SRC" / "complete" / "complete_native_db.c"


def os_name() -> str:
    """Same platform key as language/makefile.py (_temp/<os>/)."""
    name = platform.system().lower()
    if name == "linux":
        return "gnuLinux"
    return name


PLAT_TEMP = ROOT / "_temp" / os_name()
RAYLIB_HEADER = PLAT_TEMP / "Raylib" / "src" / "raylib.h"
RAYGUI_HEADER = PLAT_TEMP / "Raygui" / "src" / "raygui.h"

# raylib 6.1 — modulde kurucusu olan TUM public tipler. Bu kumede olan adlar
# g_entries'te E(...) ile listelenir ve tabloda NF_TYPE olarak isaretlenir;
# boylece IDE `Raylib.` onerisinde tip adlarini (Texture2D, Image, Sound...)
# de gosterir. Liste Modules/gcl_raylib.c'deki "Types" bolumuyle birebir
# ayni tutulmalidir.
TYPE_NAMES = {
    "Rectangle", "Vector2", "Vector3", "Vector4", "Matrix", "Camera",
    "Camera2D", "Ray", "BoundingBox", "NPatchInfo", "GlyphInfo",
    "VrStereoConfig", "AutomationEvent",
    "Quaternion", "Color", "Camera3D", "RayCollision", "Transform", "BoneInfo",
    "Image", "Texture", "Texture2D", "TextureCubemap", "RenderTexture",
    "RenderTexture2D", "Font", "Shader", "Mesh", "MaterialMap", "Material",
    "ModelSkeleton", "Model", "ModelAnimation",
    "Wave", "AudioStream", "Sound", "Music",
    "VrDeviceInfo", "FilePathList", "AutomationEventList",
}

# Every type the runtime can actually chain through (complete_native.c ->
# gcl_native_structs[]). A `ret` naming one of these is a valid chain link;
# any other non-scalar name silently stops completion.
STRUCT_TYPES = {
    "Vector2", "Vector3", "Vector4", "Rectangle", "Color",
    "Camera", "Camera3D", "Camera2D", "Ray", "RayCollision",
    "BoundingBox", "Matrix", "NPatchInfo", "GlyphInfo", "Texture2D",
    "RenderTexture2D", "Font", "Shader", "Image", "AutomationEvent",
    "VrStereoConfig", "Quaternion", "Transform", "Mesh", "MaterialMap",
    "Material", "BoneInfo", "Model", "Wave", "AudioStream", "Sound",
    "Music", "FilePathList", "AutomationEventList",
    "Texture", "TextureCubemap", "RenderTexture", "ModelSkeleton",
    "ModelAnimation", "VrDeviceInfo",
}

# Types that are leaves in GCL terms ('.' suggests nothing meaningful).
SCALAR_TYPES = {
    "void", "int", "float", "float64", "gcChar", "bool", "double",
    "char", "short", "long", "size_t",
}
# A `g_last_<x>` marker only counts when it is the LEFT side of an assignment.
# WHY: these markers are ALSO passed as arguments to void functions, and a bare
# substring match made those functions report the wrong return type:
#   fn_DrawRay        { DrawRay(g_last_ray, ...); }          -> "Ray"      (wrong)
#   fn_DrawMesh       { DrawMesh(..., g_last_mat); }         -> "Matrix"   (wrong)
#   fn_BeginVrStereoMode { BeginVrStereoMode(g_last_vr); }   -> "VrStereoConfig" (wrong)
# The regex in ret_type() requires a single '=' (so '==' does not match).
LAST_ASSIGN_TYPES = {
    "g_last_v2":      "Vector2",
    "g_last_v3":      "Vector3",
    "g_last_v4":      "Vector4",
    "g_last_rect":    "Rectangle",
    "g_last_cam":     "Camera",
    "g_last_cam2d":   "Camera2D",
    "g_last_ray":     "Ray",
    "g_last_raycol":  "RayCollision",
    "g_last_bb":      "BoundingBox",
    "g_last_npatch":  "NPatchInfo",
    "g_last_glyph":   "GlyphInfo",
    "g_last_font":    "Font",
    "g_last_color":   "Color",
    "g_last_mat":     "Matrix",
    "g_last_vr":      "VrStereoConfig",
    "g_last_aevent":  "AutomationEvent",
    # raylib 6.1 tip kuruculari icin ek scratch global'ler
    "g_last_quat":      "Quaternion",
    "g_last_transform": "Transform",
    "g_last_matmap":    "MaterialMap",
    "g_last_skeleton":  "ModelSkeleton",
    "g_last_manim":     "ModelAnimation",
    "g_last_vrdev":     "VrDeviceInfo",
    "g_last_bone":      "BoneInfo",
}

# The module stores loaded objects in TYPED handle tables and returns the handle
# index (an int at the ABI level, but a typed value for completion). Every
# registry must be listed here, otherwise the function falls through to the
# "int" fallback and the chained completion silently stops:
#   Raylib.LoadModel("a.obj").   -> nothing (claimed "int")
#   Raylib.LoadFont("f.ttf").    -> nothing (claimed "int")
REG_MARKERS = [
    ("reg_model(",  "Model"),
    ("reg_mesh(",   "Mesh"),
    ("reg_mat(",    "Material"),
    ("reg_font(",   "Font"),
    ("reg_img(",    "Image"),
    ("reg_tex(",    "Texture2D"),
    ("reg_rt(",     "RenderTexture2D"),
    ("reg_shader(", "Shader"),
    ("reg_snd(",    "Sound"),
    ("reg_mus(",    "Music"),
    ("reg_wave(",   "Wave"),
    ("reg_ast(",    "AudioStream"),
    ("reg_fpl(",    "FilePathList"),
    ("reg_aevl(",   "AutomationEventList"),
]

# A single 'g_last_<name>' followed by '=' (but not '=='). Group 1 is the name.
LAST_ASSIGN_RE = re.compile(r"\b(g_last_[A-Za-z0-9_]+)\s*=\s*([^=]|$)")

# An EXPLICIT value return: `return (double)r;`, `return X ? 1.0 : 0.0;`.
# WHY: several bindings both STORE an out-parameter into a g_last_* scratch
# global and return the control RESULT, e.g.
#     int r = GuiScrollPanel(..., &scroll, &view);
#     g_last_v2 = scroll; g_last_rect = view; return (double)r;
# The marker scan only sees `g_last_v2 =` and therefore claims the member
# returns Vector2, so the IDE chained struct members onto a call that really
# returns an int. The real C header says `int` for every one of these, so when
# the body has a real return statement the header's scalar type is accurate.
# `return 0.0;` (a struct-returning binding: `g_last_v2 = Foo(); return 0.0;`)
# carries no value and is excluded by the lookahead.
EXPLICIT_RETURN_RE = re.compile(r"\breturn\s+(?!0\.0\s*;)")

VOID_PREFIX = (
    "Draw", "Begin", "End", "Show", "Hide", "Enable", "Disable", "Set",
    "Init", "Close", "Clear", "Toggle", "Maximize", "Minimize", "Restore",
    "Pause", "Resume", "Stop", "Play", "Seek", "Attach", "Detach",
    "Unload", "Upload", "Image", "Gen", "WaveCrop", "Poll", "Swap",
    "Wait", "Open", "Save", "Make", "Change", "Update",
)

PARAM_OVERRIDES = {
    "InitWindow": "int width, int height, gcChar title",
    "GetMousePosition": "void",
    "GetMouseX": "void",
    "GetMouseY": "void",
    "Camera": "Vector3 position, Vector3 target, Vector3 up, float fovy, int projection",
    "Vector2": "float x, float y",
    "Vector3": "float x, float y, float z",
    "Vector4": "float x, float y, float z, float w",
    "Rectangle": "float x, float y, float width, float height",
    "DrawText": "gcChar text, int posX, int posY, int fontSize, Color color",
    "DrawTextEx": "Font font, gcChar text, Vector2 position, float fontSize, float spacing, Color tint",
    "DrawFPS": "int posX, int posY",
    "SetWindowTitle": "gcChar title",
    "GetScreenToWorldRay": "Vector2 position, Camera camera",
    "MeasureText": "gcChar text, int fontSize",
    "GetRandomValue": "int min, int max",
    "Color": "int r, int g, int b, int a",
    # --- GCL'e ozgu kopruler: imzalar raylib'den BILINCLI olarak farklidir.
    #     Diziler (int* / Vector3* / Matrix* / char**) GCL'de olmadigi icin
    #     modul ya diziyi metinden kurar ya da "count + duz degerler" bekler.
    #     Hex koprusu: ikili veri hex metin olarak tasinir.
    "LoadUTF8": "gcChar codepointsCsv",
    "LoadCodepoints": "gcChar text",
    "GetCodepoint": "gcChar text, int byteOffset",
    "GetCodepointNext": "gcChar text, int byteOffset",
    "GetCodepointPrevious": "gcChar text, int byteOffset",
    "CodepointToUTF8": "int codepoint",
    "LoadTextLines": "gcChar text",
    "TextSplit": "gcChar text, gcChar delimiter",
    "TextJoin": "gcChar delimiter, gcChar item0, gcChar item1, ...",
    "TextAppend": "gcChar text, gcChar append",
    "TextReplaceAlloc": "gcChar text, gcChar search, gcChar replacement",
    "TextReplaceBetween": "gcChar text, gcChar begin, gcChar end, gcChar replacement",
    "TextReplaceBetweenAlloc": "gcChar text, gcChar begin, gcChar end, gcChar replacement",
    "TextInsertAlloc": "gcChar text, gcChar insert, int position",
    "DrawTextCodepoints": "int font, gcChar text, float x, float y, float fontSize, float spacing, Color tint",
    "MeasureTextCodepoints": "int font, gcChar text, float fontSize, float spacing",
    "LoadFontEx": "gcChar fileName, int fontSize, gcChar codepointsCsv",
    "LoadFontFromImage": "int image, Color key, int firstChar",
    "LoadFontFromMemory": "gcChar fileName, int fontSize",
    "LoadFontData": "gcChar fileName, int fontSize, int type, gcChar codepointsCsv",
    "GenImageFontAtlas": "int glyphSet, int fontSize, int padding, int packMethod",
    "UnloadFontData": "int glyphSet",
    "DrawTriangleStrip3D": "int count, float x0, float y0, float z0, ..., Color color",
    "UploadMesh": "int mesh, int dynamic",
    "UpdateMeshBuffer": "int mesh, int bufferIndex, gcChar hexData, int dataSize, int offset",
    "DrawMeshInstanced": "int mesh, int material, int instances, float m0[16], ...",
    "GenMeshTangents": "int mesh",
    "LoadMaterials": "gcChar fileName",
    "SetMaterialTexture": "int material, int mapType, int texture",
    "SetModelMeshMaterial": "int model, int meshId, int materialId",
    "LoadModelAnimations": "gcChar fileName",
    "UpdateModelAnimation": "int model, int animation, int frame",
    "UpdateModelAnimationEx": "int model, int animationA, float frameA, int animationB, float frameB, float blend",
    "IsModelAnimationValid": "int model, int animation",
    "CheckCollisionBoxes": "float min1x, float min1y, float min1z, float max1x, float max1y, float max1z, float min2x, float min2y, float min2z, float max2x, float max2y, float max2z",
    "CheckCollisionBoxSphere": "float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float centerX, float centerY, float centerZ, float radius",
    "UpdateSound": "int sound, gcChar hexData, int frameCount",
    "WaveCrop": "int wave, int initFrame, int finalFrame",
    "WaveFormat": "int wave, int sampleRate, int sampleSize, int channels",
    "LoadWaveSamples": "int wave",
    "UpdateAudioStream": "int stream, gcChar hexData, int frameCount",
    "SetAudioStreamCallback": "int stream, gcChar callbackName",
    "AttachAudioStreamProcessor": "int stream, gcChar processorName",
    "DetachAudioStreamProcessor": "int stream, gcChar processorName",
    "AttachAudioMixedProcessor": "gcChar processorName",
    "DetachAudioMixedProcessor": "gcChar processorName",
    "LoadImageRaw": "gcChar fileName, int width, int height, int format, int headerSize",
    "LoadImageAnim": "gcChar fileName",
    "LoadImageAnimFromMemory": "gcChar fileName, gcChar fileType",
    "LoadImageFromMemory": "gcChar fileName, gcChar fileType",
    "ExportImageToMemory": "int image, gcChar fileType",
    "LoadImageColors": "int image",
    "LoadImagePalette": "int image, int maxPaletteSize",
    "LoadWaveFromMemory": "gcChar fileName, gcChar fileType",
    "LoadMusicStreamFromMemory": "gcChar fileName, gcChar fileType",
    # --- raylib 6.1 tip kuruculari (params kolonu IDE imzasi icin) ---
    "Quaternion": "float x, float y, float z, float w",
    "Camera3D": "Vector3 position, Vector3 target, Vector3 up, float fovy, int projection",
    "RayCollision": "int hit, float distance, Vector3 point, Vector3 normal",
    "Transform": "Vector3 translation, Vector3 rotation, Vector3 scale",
    "BoneInfo": "gcChar name, int parent",
    "MaterialMap": "Texture2D texture, Color color, float value",
    "Texture": "",
    "Texture2D": "",
    "TextureCubemap": "",
    "RenderTexture": "",
    "RenderTexture2D": "",
    "Font": "",
    "Shader": "",
    "Mesh": "",
    "Material": "",
    "ModelSkeleton": "",
    "Model": "",
    "ModelAnimation": "",
    "Image": "",
    "Wave": "",
    "AudioStream": "",
    "Sound": "",
    "Music": "",
    "VrDeviceInfo": "",
    "FilePathList": "",
    "AutomationEventList": "",
}

# Kurucu fonksiyonun DONUS TIPI marker taramasiyla yanlis cikan adlar.
# Orn: fn_Texture -> reg_tex(...) -> marker "Texture2D" der, ama
# `Raylib.Texture` cagrisi Texture dondurur; fn_Camera3D ise g_last_cam
# (Camera) yazar. Tabloda dogru tip gorunsun diye burada sabitlenir.
CONSTRUCTOR_RET = {
    "Texture":          "Texture",
    "TextureCubemap":   "TextureCubemap",
    "RenderTexture":    "RenderTexture",
    "Camera3D":         "Camera3D",
    "BoneInfo":         "BoneInfo",
    "ModelSkeleton":    "ModelSkeleton",
    "ModelAnimation":   "ModelAnimation",
    "VrDeviceInfo":     "VrDeviceInfo",
}


def slug_category(comment: str) -> str:
    c = re.sub(r"[^0-9a-zA-Z]+", "", comment).lower()
    return c or "misc"


def _normalize_c_return(ret: str) -> str:
    """`const struct Vector2 *` -> `Vector2`; `unsigned int` -> `int`."""
    t = " ".join(ret.replace("*", " ").split())
    t = re.sub(r"\bconst\b", " ", t)
    t = re.sub(r"\bstruct\b", " ", t)
    t = re.sub(r"\benum\b", " ", t)
    t = " ".join(t.split())
    aliases = {
        "unsigned int": "int", "unsigned char": "int", "signed int": "int",
        "unsigned short": "int", "signed char": "int", "unsigned long": "int",
        "long int": "int", "long long": "int", "int64_t": "int",
        "uint32_t": "int", "int32_t": "int", "bool": "int",
        "double": "float64", "size_t": "int", "char": "gcChar",
        "const char": "gcChar",
        "float": "float",
    }
    return aliases.get(t, t.replace(" ", ""))


# Public-API macros. RAYGUIAPI was missing here, so EVERY raygui.h declaration
# was invisible to the generator ("raygui.h=0 decls") - the IDE had no raygui
# return types, no void detection and no doc text.
API_MACROS = ("RLAPI", "RAYLIB_API", "RAYGUIAPI", "RMAPI")

# <MACRO> <ret...> <name>(<params>)
_DECL_RE = re.compile(
    r"^(?:" + "|".join(API_MACROS) + r")\s+(.*?)\s*\(([^()]*)\)$")

# <MACRO> ... ) ;   // doc comment
_DOC_RE = re.compile(
    r"^(?:" + "|".join(API_MACROS) + r")\s+.*?\)\s*;[ \t]*//\s*(.*)$")

# C type -> the GCL-facing token shown in the IDE signature popup.
_SCALAR_PARAM = {
    "int": "int", "unsigned int": "uint", "unsigned char": "uint",
    "short": "int", "unsigned short": "int", "long": "int",
    "unsigned long": "int", "long long": "int", "unsigned long long": "int",
    "size_t": "int", "float": "float", "double": "float64", "bool": "bool",
    "char": "int", "signed char": "int", "int8_t": "int", "uint8_t": "uint",
    "int16_t": "int", "uint16_t": "uint", "int32_t": "int", "uint32_t": "uint",
}

# Value structs are passed FIELD BY FIELD (the module rebuilds them), so the
# IDE has to advertise the expanded argument list, not the C struct.
_STRUCT_PARAM_EXPAND = {
    "Vector2": "float {n}x, float {n}y",
    "Vector3": "float {n}x, float {n}y, float {n}z",
    "Vector4": "float {n}x, float {n}y, float {n}z, float {n}w",
    "Quaternion": "float {n}x, float {n}y, float {n}z, float {n}w",
    "Rectangle": "float {n}x, float {n}y, float {n}width, float {n}height",
    "Camera": "Vector3 position, Vector3 target, Vector3 up, float fovy, int projection",
    "Camera3D": "Vector3 position, Vector3 target, Vector3 up, float fovy, int projection",
    "Camera2D": "Vector2 offset, Vector2 target, float rotation, float zoom",
    "Ray": "Vector3 position, Vector3 direction",
    "BoundingBox": "Vector3 min, Vector3 max",
    "Transform": "Vector3 translation, Vector3 rotation, Vector3 scale",
    "NPatchInfo": "Rectangle source, int left, int top, int right, int bottom, int layout",
    "GlyphInfo": "int value, int offsetX, int offsetY, int advanceX, int image",
    "AutomationEvent": "uint eventFrame, uint type, int p0, int p1, int p2, int p3",
    "VrDeviceInfo": ("int hResolution, int vResolution, float hScreenSize, "
                     "float vScreenSize, float eyeToScreenDistance, "
                     "float lensSeparationDistance, float interpupillaryDistance"),
}

# Objects travel as a registry HANDLE index, never as a raw pointer.
_HANDLE_TYPES = {
    "Image", "Texture", "Texture2D", "TextureCubemap", "RenderTexture",
    "RenderTexture2D", "Font", "Shader", "Mesh", "Material", "MaterialMap",
    "Model", "ModelSkeleton", "ModelAnimation", "Wave", "AudioStream",
    "Sound", "Music", "FilePathList", "AutomationEventList",
    "VrStereoConfig",
}

# `X *pts, int count` -> a count followed by flat values.
_ARRAY_ELEM_FLOATS = {"Vector2": 2, "Vector3": 3, "Vector4": 4, "Quaternion": 4}


def _strip_preproc(src: str) -> str:
    """Remove every `#...` line before the `;` split.

    WHY: a preprocessor line sits between two declarations, so the split glued
    it onto the FOLLOWING declaration and the `^RLAPI` anchor then failed -
    every declaration that followed an `#ifdef`/`#endif` was silently dropped
    from the API map.
    """
    return re.sub(r"^[ \t]*#[^\n]*", "", src, flags=re.M)


def parse_header_docs(path: Path) -> dict:
    """name -> the trailing `// ...` description of its declaration.

    Read from the RAW text: parse_c_api_header() strips all comments, so the doc
    text has to be captured before that happens. It feeds the IDE tooltip.
    """
    docs = {}
    if not path.exists():
        return docs
    raw = path.read_text(encoding="utf-8", errors="replace")
    for line in raw.splitlines():
        s = line.strip()
        m = _DOC_RE.match(s)
        if not m:
            continue
        head = s.split("//", 1)[0]
        nm = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^()]*\)\s*;", head)
        if nm:
            docs[nm.group(1)] = " ".join(m.group(1).split())
    return docs


def _split_param(decl: str):
    """`const Vector2 *points` -> ("Vector2", 1, "points", False)."""
    t = " ".join(decl.replace("\t", " ").split())
    if not t:
        return None
    arr = "[" in t
    t = re.sub(r"\[[^\]]*\]", " ", t)
    t = re.sub(r"\b(const|volatile|struct|enum|union)\b", " ", t)
    t = " ".join(t.split())
    ptrs = t.count("*")
    t = t.replace("*", " ")
    t = " ".join(t.split())
    name = ""
    parts = t.split(" ")
    if len(parts) >= 2 and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", parts[-1]):
        name = parts[-1]
        t = " ".join(parts[:-1])
    return (t.strip(), ptrs, name, arr)


def _gcl_param(decl: str, next_decl: str | None) -> str:
    """Render one C parameter the way the GCL caller must supply it."""
    info = _split_param(decl)
    if info is None:
        return decl
    base, ptrs, name, _arr = info
    label = name or "arg"

    # `X *pts, int count` -> count first, then the flat element list.
    if ptrs and base in _ARRAY_ELEM_FLOATS and next_decl:
        ninfo = _split_param(next_decl)
        if ninfo and ninfo[1] == 0 and ninfo[0] in _SCALAR_PARAM:
            n = ninfo[2] or "count"
            width = _ARRAY_ELEM_FLOATS[base]
            return f"int {n}, float {label}[{n}*{width}]"
    if ptrs and base in _HANDLE_TYPES:
        return f"int {label}"
    if base in _HANDLE_TYPES:
        return f"int {label}"
    if ptrs and base in ("char", "void", ""):
        return f"gcChar {label}"
    if ptrs:
        # Unknown pointee: the ABI cannot carry it, so say so honestly.
        return f"gcChar {label}"
    if base in _STRUCT_PARAM_EXPAND:
        return _STRUCT_PARAM_EXPAND[base].replace("{n}", label if name else "")
    if base in _SCALAR_PARAM:
        return f"{_SCALAR_PARAM[base]} {label}"
    return f"{base} {label}".strip()


def derive_params(params_raw: str) -> str:
    """C parameter list -> the GCL-facing argument list shown by the IDE.

    Before this the IDE showed NOTHING for every member that had no hand-written
    PARAM_OVERRIDES entry (~380 of ~400 members), so the signature popup was
    empty for most of the API.
    """
    t = " ".join(params_raw.split())
    if not t or t == "void" or t.startswith("..."):
        return ""
    chunks, depth, cur = [], 0, ""
    for ch in t:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            chunks.append(cur)
            cur = ""
            continue
        cur += ch
    if cur:
        chunks.append(cur)
    chunks = [c for c in chunks if c.strip()]
    out = []
    for i, c in enumerate(chunks):
        if "(" in c:            # function pointer (callback) - not callable from GCL
            continue
        out.append(_gcl_param(c, chunks[i + 1] if i + 1 < len(chunks) else None))
    return ", ".join(out)


def parse_c_api_header(path: Path) -> dict:
    """name -> (return type, is_pointer, params) from an API header.

    WHY the return type comes from the header: the marker scan in ret_type()
    can only see what the binding body does. When it finds nothing it falls
    back to "int", which is a LEAF type, so chained completion
    (`Raylib.Foo().bar`) dies silently with no diagnostic (todo #7).

    WHY the params come from the header too: the IDE renders member signature
    help as `Name(params)` (ide_complete_ui.c).
    """
    if not path.exists():
        return {}
    src = path.read_text(encoding="utf-8", errors="replace")
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)   # block comments
    src = re.sub(r"//[^\n]*", " ", src)                # line comments
    src = _strip_preproc(src)
    api = {}
    for stmt in src.split(";"):
        st = " ".join(stmt.split())
        if not st:
            continue
        m = _DECL_RE.match(st)
        if not m:
            continue
        decl, params = m.group(1).strip(), " ".join(m.group(2).split())
        # `unsigned char *LoadFileData` -> ret "unsigned char", name "*LoadFileData".
        # The '*' between type and name used to make the whole match fail, so
        # every pointer-returning declaration (LoadFileData, ComputeMD5,
        # LoadImageColors, TextSplit, ...) was absent from the API map.
        parts = decl.rsplit(None, 1)
        if len(parts) != 2:
            continue
        raw_ret, name = parts[0], parts[1]
        is_ptr = name.startswith("*") or "*" in raw_ret
        name = name.lstrip("*").strip()
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            continue
        api[name] = (_normalize_c_return(raw_ret), is_ptr, params)
    return api
def parse_entries(src: str):
    """Extract the (name, category) list from the g_entries[] block."""
    m = re.search(r"g_entries\[\]\s*=\s*\{(.*?)\n\s*\};", src, re.S)
    block = m.group(1) if m else src
    out = []
    category = "misc"
    # Go line by line: /* Category */ comments and E(NAME)/{"NAME"} entries.
    for line in block.splitlines():
        cm = re.search(r"/\*\s*(.+?)\s*\*/", line)
        if cm:
            category = slug_category(cm.group(1))
        for nm in re.finditer(r"E\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", line):
            out.append((nm.group(1), category))
        for nm in re.finditer(r"\{\s*\"([A-Za-z_][A-Za-z0-9_]*)\"\s*,", line):
            out.append((nm.group(1), category))
    # dedup (order preserved)
    seen, uniq = set(), []
    for name, cat in out:
        if name in seen:
            continue
        seen.add(name)
        uniq.append((name, cat))
    return uniq


def fn_body(src: str, name: str) -> str:
    """Return the FULL text of the fn_<name>(...) body (brace-matched).

    WHY not `split("\\n", 1)[0]`: a multi-line body was truncated to its opening
    '{', so every marker inside it was invisible and the return type fell back to
    "int". Any struct-returning binding written across several lines (e.g.
    LoadVrStereoConfig) therefore lost its chain link and `Raylib.Foo().bar`
    silently suggested nothing (todo.md -> "language full bug hunting" #7).
    """
    m = re.search(r"fn_" + re.escape(name) + r"\s*\([^)]*\)\s*\{", src)
    if not m:
        return ""
    depth = 0
    in_str = False
    in_chr = False
    i = m.end() - 1
    n = len(src)
    while i < n:
        c = src[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif in_chr:
            if c == "\\":
                i += 2
                continue
            if c == "'":
                in_chr = False
        elif c == '"':
            in_str = True
        elif c == "'":
            in_chr = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[m.end():i]
        i += 1
    return src[m.end():]


def marker_ret_type(name: str, body: str) -> str:
    """Return type inferred from binding-body markers (may be the "int" guess)."""
    # 1) "g_last_x = value" -> the produced typed value.
    for m in LAST_ASSIGN_RE.finditer(body):
        t = LAST_ASSIGN_TYPES.get(m.group(1))
        if t:
            return t
    # 2) A typed handle registration (reg_*() call).
    for marker, t in REG_MARKERS:
        if marker in body:
            return t
    # 3) String buffer output (g_str'e yazan fonksiyonlar gcChar dondurur).
    if "g_str" in body:
        return "gcChar"
    # Stub bodies: the binding is a no-op that returns 0.0 (unimplemented).
    # Braces AND whitespace must be removed first — the old comparison used
    # brace-less literals, so a one-line stub `{(void)argc;...}` never matched
    # and was reported as "int" (a leaf, which accidentally hid the no-op).
    flat = " ".join(body.replace("{", " ").replace("}", " ").split())
    if flat in ("return 0.0;", "return 0.0 ;", "(void)argc;(void)argv;return 0.0;",
                "(void)argc;(void)argv; return 0.0;", "return 0.0", ""):
        STUB_NAMES.append(name)
        return "void"
    if name.startswith(VOID_PREFIX):
        return "void"
    return "int"


# ABI-packed leaf types: gcl_raylib.c carries a Color as a 32-bit packed uint
# (R | G<<8 | B<<16 | A<<24) — see the file header. A Color value therefore can
# NOT be chained (`.r` is unreachable). The real C header says `Color`, so a
# Color mismatch is EXPECTED and must never be "corrected".
ABI_PACKED_TYPES = {"Color"}

REPORT: list = []
GAPS: list = []
# Binding functions that are no-op stubs (they return 0.0 unconditionally).
STUB_NAMES: list = []


GAP_SUMMARY = ""


def collect_gaps(api: dict | None) -> None:
    """Report the binding stubs (no-op functions) that silently return nothing.

    A stub whose raylib counterpart returns VOID is harmless — "does nothing" is
    exactly what the header promises. Only a stub whose counterpart returns a
    VALUE is a real gap: the caller silently receives 0 instead of the result,
    with no error anywhere. Those are listed individually; the harmless void
    stubs are summarised as one count so the signal is not drowned out.
    """
    global GAP_SUMMARY
    if not api:
        return
    void_stubs = 0
    for name in STUB_NAMES:
        hdr = api.get(name)
        if not hdr:
            continue
        hret, is_ptr = hdr[0], hdr[1]
        if hret == "void" and not is_ptr:
            void_stubs += 1
            continue
        GAPS.append(f"  [gap] {name} -> header returns '{hret}' "
                    f"but the binding is a no-op stub (caller silently gets 0)")
    if void_stubs:
        GAP_SUMMARY = (f"  [gap] {void_stubs} further stub(s) implement VOID "
                       f"functions - no value lost, module work only")


def audit(name: str, binding_ret: str, api: dict | None) -> None:
    """Compare the binding-derived return type with the real C header.

    The header is the authority on the RAYLIB API, but the BINDING is the
    authority on what GCL can do with the value: structs arrive as handles,
    colors as packed uints, unimplemented stubs return 0.0. So the header never
    overrides a binding result — it only raises notes for human review. That is
    how the multi-line `fn_body` truncation above was found: it had silently
    demoted every multi-line struct-returning binding to "int".
    """
    if not api:
        return
    if name in STUB_NAMES:
        return                        # stub: reported once by collect_gaps()
    hdr = api.get(name)
    if not hdr:
        return
    hret, is_ptr = hdr[0], hdr[1]
    if is_ptr:
        return                        # pointer returns are handles/strings, not structs
    if hret in ABI_PACKED_TYPES:
        return                        # packed by design (Color): "int" is correct
    if hret in STRUCT_TYPES:
        if binding_ret == "int":
            REPORT.append(
                f"  [audit] {name}: binding->int, header->{hret} "
                f"(chain link missing unless the binding writes g_last_*)")
        elif binding_ret != hret and binding_ret not in STRUCT_TYPES:
            REPORT.append(
                f"  [audit] {name}: binding->{binding_ret}, header->{hret} (verify)")
        return
    if binding_ret in STRUCT_TYPES and hret in SCALAR_TYPES:
        REPORT.append(
            f"  [audit] {name}: binding->{binding_ret}, header->{hret} (verify binding)")


def header_ret_type(name: str, api: dict | None) -> str:
    """Return type for a binding whose body is NOT in the module source.

    Such a binding is generated by a macro, so the C header is the only
    authority on what it returns; the "int" fallback would be a leaf type and
    would silently kill chained completion (todo #7). Pointer returns ship to
    GCL as `gcChar`, the module's string-output convention.
    """
    hdr = api.get(name) if api else None
    if not hdr:
        return "int"
    hret, is_ptr = hdr[0], hdr[1]
    if is_ptr:
        return "gcChar"
    return hret


def ret_type(name: str, body: str, api: dict | None = None) -> str:
    """Binding-derived return type, arbitrated by the real C header.

    The header decides the ONE thing the markers cannot: whether the function
    returns a value at all. A void raylib function whose binding builds a struct
    into a `g_last_*` scratch global (BeginMode2D/BeginMode3D/UpdateCamera) was
    otherwise reported as returning that struct — the marker scan cannot tell a
    scratch buffer from a return value, the header can.
    """
    # Tip kuruculari: marker taramasi reg_*() uzerinden yanlis tip verebilir
    # (Texture -> reg_tex -> Texture2D). IDE onerisinde dogru tip gorunsun.
    if name in CONSTRUCTOR_RET:
        ret = CONSTRUCTOR_RET[name]
        audit(name, ret, api)
        return ret
    # No body found in the module source: the binding is macro-generated, so
    # the C header is the authority. marker_ret_type() used to treat an empty
    # body as a no-op stub, which both overstated the real stub count
    # (todo.md section 3) and forced the "int" fallback - a leaf type that
    # silently killed chained completion for those members (todo #7).
    if not body.strip():
        ret = header_ret_type(name, api)
        audit(name, ret, api)
        return ret
    ret = marker_ret_type(name, body)
    hdr = api.get(name) if api else None
    if hdr and not hdr[1] and hdr[0] == "void" and ret not in ABI_PACKED_TYPES:
        if ret != "void" and ret != "gcChar":
            ret = "void"
    # A g_last_* store only means "an out-parameter was published"; it does NOT
    # mean the member returns that struct. When the body also has an explicit
    # value return and the header promises a scalar, the scalar is the honest
    # answer (GuiScrollPanel/GuiListViewEx/GuiColorPicker/CheckCollisionLines...).
    if (hdr and not hdr[1] and hdr[0] in SCALAR_TYPES
            and ret in STRUCT_TYPES and EXPLICIT_RETURN_RE.search(body)):
        ret = hdr[0]
    audit(name, ret, api)
    return ret


def flags_of(name: str) -> str:
    if name in TYPE_NAMES:
        return "NF_TYPE"
    if name.isupper() and any(ch.isalpha() for ch in name):
        return "NF_CONST"
    return "NF_FUNC"


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def emit_table(var: str, items, src: str, api: dict | None = None,
               docs: dict | None = None) -> str:
    """Emit one GclNativeMember table.

    `params` is IDE signature help and `doc` its tooltip. Both used to be empty
    for ~95% of the members (only PARAM_OVERRIDES filled params, and doc was
    hard-coded to ""), so the completion popup showed a bare name and the user
    had to guess the argument list.
    """
    lines = [f"const GclNativeMember {var}[] = {{"]
    for name, cat in items:
        body = fn_body(src, name)
        ret = ret_type(name, body, api)
        params = PARAM_OVERRIDES.get(name)
        if params is None:
            hdr = api.get(name) if api else None
            params = derive_params(hdr[2]) if hdr else ""
        doc = (docs or {}).get(name, "")
        fl = flags_of(name)
        lines.append(
            f'    {{"{c_escape(name)}", "{c_escape(ret)}", "{c_escape(params)}", '
            f'"{c_escape(cat)}", "{c_escape(doc)}", {fl}}},'
        )
    lines.append("};")
    lines.append(f"const int {var}_count = (int)(sizeof({var}) / sizeof({var}[0]));")
    return "\n".join(lines)


def main() -> int:
    raylib_src = (MODULES / "gcl_raylib.c").read_text(encoding="utf-8", errors="replace")
    raygui_src = (MODULES / "gcl_raygui.c").read_text(encoding="utf-8", errors="replace")

    raylib = parse_entries(raylib_src)
    raygui = parse_entries(raygui_src)

    raylib_api = parse_c_api_header(RAYLIB_HEADER)
    raygui_api = parse_c_api_header(RAYGUI_HEADER)
    if not raylib_api:
        print(f"[gen_native_db] note: header not found, binding-only mode: "
              f"{RAYLIB_HEADER}", flush=True)
    else:
        print(f"[gen_native_db] headers: raylib.h={len(raylib_api)} decls, "
              f"raygui.h={len(raygui_api)} decls", flush=True)

    raylib_docs = parse_header_docs(RAYLIB_HEADER)
    raygui_docs = parse_header_docs(RAYGUI_HEADER)
    parts = [
        "/* complete_native_db.c — GENERATED FILE (tools/gen_native_db.py). */",
        "/* DO NOT EDIT this file by hand; update the generator script. */",
        '#include "complete_native.h"',
        "",
        emit_table("gcl_native_db_Raylib", raylib, raylib_src, raylib_api,
                   raylib_docs),
        "",
        emit_table("gcl_native_db_Raygui", raygui, raygui_src, raygui_api,
                   raygui_docs),
        "",
    ]
    content = "\n".join(parts)
    collect_gaps(raylib_api)
    for line in GAPS[:20]:
        print(line)
    if len(GAPS) > 20:
        print(f"  ... and {len(GAPS) - 20} more value-returning stub(s)")
    if GAP_SUMMARY:
        print(GAP_SUMMARY)
    if REPORT:
        print(f"[gen_native_db] header audit ({len(REPORT)} note(s)):", flush=True)
        for line in REPORT[:40]:
            print(line)
        if len(REPORT) > 40:
            print(f"  ... and {len(REPORT) - 40} more")
    REPORT.clear()
    GAPS.clear()
    STUB_NAMES.clear()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    old = OUT.read_text(encoding="utf-8") if OUT.exists() else None
    if old == content:
        print(f"[gen_native_db] up to date: {OUT}")
        return 0
    OUT.write_text(content, encoding="utf-8")
    print(f"[gen_native_db] written: {OUT} "
          f"(Raylib={len(raylib)}, Raygui={len(raygui)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
