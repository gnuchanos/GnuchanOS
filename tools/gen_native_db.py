#!/usr/bin/env python3
"""
gen_native_db.py — Raylib/Raygui native API veritabanını ÜRETİR.

`language/_SRC/Modules/gcl_raylib.c` ve `gcl_raygui.c` içindeki:
  * `g_entries[] = { ... }` tablosundan üye adları + kategoriler,
  * `fn_<NAME>(...) { ... }` gövdelerindeki `g_last_v2=`, `reg_tex(`, `g_str`
    gibi atamalardan DÖNÜŞ TİPİ
çıkarılıp `language/_SRC/complete/complete_native_db.c` yazılır.

Elle düzenlenmez: tek kaynak (Modules/*.c) → üretilen tablo.
Çalıştır:  python tools/gen_native_db.py
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent          # repo kökü
MODULES = ROOT / "language" / "_SRC" / "Modules"
OUT = ROOT / "language" / "_SRC" / "complete" / "complete_native_db.c"

TYPE_NAMES = {
    "Rectangle", "Vector2", "Vector3", "Vector4", "Matrix", "Camera",
    "Camera2D", "Ray", "BoundingBox", "NPatchInfo", "GlyphInfo",
    "VrStereoConfig", "AutomationEvent",
}

# Gövdede bu atamalar varsa dönüş tipi bellidir.
# SIRA ÖNEMLİ: daha özgül işaretçiler önce gelmeli. "g_last_cam" pek çok
# gövdede (g_last_ray / g_last_bb argüman olarak, g_last_cam2d alt-dize)
# geçtiği için yanlış tip veriyordu → cam EN SONA alındı.
RET_MARKERS = [
    ("g_last_v2", "Vector2"),
    ("g_last_v3", "Vector3"),
    ("g_last_v4", "Vector4"),
    ("g_last_rect", "Rectangle"),
    ("g_last_cam2d", "Camera2D"),
    ("g_last_ray", "Ray"),
    ("g_last_bb", "BoundingBox"),
    ("g_last_raycol", "RayCollision"),
    ("g_last_glyph", "GlyphInfo"),
    ("g_last_font", "Font"),
    ("g_last_color", "Color"),
    ("reg_tex(", "Texture2D"),
    ("g_str", "gcChar"),
    ("g_last_cam", "Camera"),   # EN SON: en genel işaretçi
]

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
}


def slug_category(comment: str) -> str:
    c = re.sub(r"[^0-9a-zA-Z]+", "", comment).lower()
    return c or "misc"


def parse_entries(src: str):
    """g_entries[] bloğundan (name, category) listesi çıkar."""
    m = re.search(r"g_entries\[\]\s*=\s*\{(.*?)\n\s*\};", src, re.S)
    block = m.group(1) if m else src
    out = []
    category = "misc"
    # Satır satır ilerle: /* Category */ yorumları ve E(NAME)/{"NAME" girdileri.
    for line in block.splitlines():
        cm = re.search(r"/\*\s*(.+?)\s*\*/", line)
        if cm:
            category = slug_category(cm.group(1))
        for nm in re.finditer(r"E\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", line):
            out.append((nm.group(1), category))
        for nm in re.finditer(r"\{\s*\"([A-Za-z_][A-Za-z0-9_]*)\"\s*,", line):
            out.append((nm.group(1), category))
    # dedup (sıra korunur)
    seen, uniq = set(), []
    for name, cat in out:
        if name in seen:
            continue
        seen.add(name)
        uniq.append((name, cat))
    return uniq


def fn_body(src: str, name: str) -> str:
    """fn_<name>(...) gövdesinin (tek satır varsayımı) metnini döndür."""
    m = re.search(r"fn_" + re.escape(name) + r"\s*\([^)]*\)\s*(.*)", src)
    if not m:
        return ""
    return m.group(1).split("\n", 1)[0]


def ret_type(name: str, body: str) -> str:
    for marker, t in RET_MARKERS:
        if marker in body:
            return t
    if body.strip() in ("return 0.0;", "return 0.0 ;", "(void)argc;(void)argv;return 0.0;",
                        "(void)argc;(void)argv; return 0.0;"):
        return "void"
    if name.startswith(VOID_PREFIX):
        return "void"
    return "int"


def flags_of(name: str) -> str:
    if name in TYPE_NAMES:
        return "NF_TYPE"
    if name.isupper() and any(ch.isalpha() for ch in name):
        return "NF_CONST"
    return "NF_FUNC"


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def emit_table(var: str, items, src: str) -> str:
    lines = [f"const GclNativeMember {var}[] = {{"]
    for name, cat in items:
        body = fn_body(src, name)
        ret = ret_type(name, body)
        params = PARAM_OVERRIDES.get(name, "")
        fl = flags_of(name)
        lines.append(
            f'    {{"{c_escape(name)}", "{c_escape(ret)}", "{c_escape(params)}", '
            f'"{c_escape(cat)}", "", {fl}}},'
        )
    lines.append("};")
    lines.append(f"const int {var}_count = (int)(sizeof({var}) / sizeof({var}[0]));")
    return "\n".join(lines)


def main() -> int:
    raylib_src = (MODULES / "gcl_raylib.c").read_text(encoding="utf-8", errors="replace")
    raygui_src = (MODULES / "gcl_raygui.c").read_text(encoding="utf-8", errors="replace")

    raylib = parse_entries(raylib_src)
    raygui = parse_entries(raygui_src)

    parts = [
        "/* complete_native_db.c — ÜRETİLEN DOSYA (tools/gen_native_db.py). */",
        "/* Bu dosyayı ELLE DÜZENLEMEYİN; üretici script'i güncelleyin. */",
        '#include "complete_native.h"',
        "",
        emit_table("gcl_native_db_Raylib", raylib, raylib_src),
        "",
        emit_table("gcl_native_db_Raygui", raygui, raygui_src),
        "",
    ]
    content = "\n".join(parts)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    old = OUT.read_text(encoding="utf-8") if OUT.exists() else None
    if old == content:
        print(f"[gen_native_db] güncel: {OUT}")
        return 0
    OUT.write_text(content, encoding="utf-8")
    print(f"[gen_native_db] yazıldı: {OUT} "
          f"(Raylib={len(raylib)}, Raygui={len(raygui)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
