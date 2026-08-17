/*
 * gcl_python_raylib.c — Python/raylib binding (.gcDL output: python_raylib.gcDL)
 *
 * python.gcDL embed loads this module at run time (LoadLibrary/dlopen) and
 * calls the python_raylib_attach export. Attach installs the "gcl_raylib"
 * module into the Python state.
 *
 * Scope (all major raylib modules, hand-written quality binding):
 *   - Window/config/screen/monitor/timer/random
 *   - Keyboard + Mouse (full state queries)
 *   - 2D drawing: line, circle, sector, ring, ellipse, rectangle (rounded incl.),
 *     triangle, polygon + all collision tests
 *   - Text: DrawText(Ex/Pro), font loading + MeasureText(Ex)
 *   - Texture: LoadTexture(FromImage), DrawTexture(Ex/Pro/Rec), filter/wrap
 *   - Camera: 2D + 3D, grid/plane/cube/sphere
 *   - Audio: device, Sound + Music (load/play/volume/pan)
 *
 * Memory safety: pointer-carrying types (Font, Sound, Music, Image, Wave) are
 * NOT copied into Python; they are stored in bounded slot registries, returned
 * as {slot:...} dicts, and freed by Unload*.
 */

#define _CRT_SECURE_NO_WARNINGS

#if defined(_WIN32)
#define GCL_MODULE_EXPORT __declspec(dllexport)
#else
#define GCL_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#include <Python.h>
#include <string.h>
#include "raylib.h"

/* Helpers: dict/sequence -> raylib types (shared, short) */
static double seq_f(PyObject *o, Py_ssize_t i, double d) {
    PyObject *v = PySequence_Fast_GET_ITEM(o, i);
    return PyFloat_Check(v) ? PyFloat_AsDouble(v) : (PyLong_Check(v) ? (double)PyLong_AsLong(v) : d);
}
static long seq_i(PyObject *o, Py_ssize_t i, long d) {
    PyObject *v = PySequence_Fast_GET_ITEM(o, i);
    return PyLong_Check(v) ? PyLong_AsLong(v) : (PyFloat_Check(v) ? (long)PyFloat_AsDouble(v) : d);
}

static Color py_color(PyObject *o) {
    Color c = { 0, 0, 0, 255 };
    if (o == NULL) return c;
    if (PyDict_Check(o)) {
        PyObject *v;
        if ((v = PyDict_GetItemString(o, "r"))) c.r = (unsigned char)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
        if ((v = PyDict_GetItemString(o, "g"))) c.g = (unsigned char)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
        if ((v = PyDict_GetItemString(o, "b"))) c.b = (unsigned char)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
        if ((v = PyDict_GetItemString(o, "a"))) c.a = (unsigned char)(PyLong_Check(v) ? PyLong_AsLong(v) : 255);
        return c;
    }
    if (PySequence_Check(o)) {
        PyObject *s = PySequence_Fast(o, "color list expected");
        if (s && PySequence_Fast_GET_SIZE(s) >= 4) {
            c.r = (unsigned char)seq_i(s, 0, 0);
            c.g = (unsigned char)seq_i(s, 1, 0);
            c.b = (unsigned char)seq_i(s, 2, 0);
            c.a = (unsigned char)seq_i(s, 3, 255);
        }
        Py_XDECREF(s);
    }
    return c;
}

static Vector2 py_vec2(PyObject *o) {
    Vector2 v = { 0, 0 };
    if (o == NULL) return v;
    if (PyDict_Check(o)) {
        PyObject *x = PyDict_GetItemString(o, "x"), *y = PyDict_GetItemString(o, "y");
        if (x) v.x = (float)(PyLong_Check(x) ? (double)PyLong_AsLong(x) : PyFloat_AsDouble(x));
        if (y) v.y = (float)(PyLong_Check(y) ? (double)PyLong_AsLong(y) : PyFloat_AsDouble(y));
        return v;
    }
    if (PySequence_Check(o)) {
        PyObject *s = PySequence_Fast(o, "vector expected");
        if (s && PySequence_Fast_GET_SIZE(s) >= 2) {
            v.x = (float)seq_f(s, 0, 0);
            v.y = (float)seq_f(s, 1, 0);
        }
        Py_XDECREF(s);
    }
    return v;
}

static Vector3 py_vec3(PyObject *o) {
    Vector3 v = { 0, 0, 0 };
    if (o == NULL) return v;
    if (PySequence_Check(o)) {
        PyObject *s = PySequence_Fast(o, "3-vector expected");
        if (s && PySequence_Fast_GET_SIZE(s) >= 3) {
            v.x = (float)seq_f(s, 0, 0);
            v.y = (float)seq_f(s, 1, 0);
            v.z = (float)seq_f(s, 2, 0);
        }
        Py_XDECREF(s);
    }
    return v;
}

static Rectangle py_rect(PyObject *o) {
    Rectangle r = { 0, 0, 0, 0 };
    if (o == NULL) return r;
    if (PyDict_Check(o)) {
        PyObject *v;
        if ((v = PyDict_GetItemString(o, "x"))) r.x = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
        if ((v = PyDict_GetItemString(o, "y"))) r.y = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
        if ((v = PyDict_GetItemString(o, "width"))) r.width = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
        if ((v = PyDict_GetItemString(o, "height"))) r.height = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
        return r;
    }
    if (PySequence_Check(o)) {
        PyObject *s = PySequence_Fast(o, "4-rect expected");
        if (s && PySequence_Fast_GET_SIZE(s) >= 4) {
            r.x = (float)seq_f(s, 0, 0);
            r.y = (float)seq_f(s, 1, 0);
            r.width = (float)seq_f(s, 2, 0);
            r.height = (float)seq_f(s, 3, 0);
        }
        Py_XDECREF(s);
    }
    return r;
}

/* serializers */
static PyObject *vec2_tuple(Vector2 v) { return Py_BuildValue("(ff)", v.x, v.y); }
static PyObject *rect_tuple(Rectangle r) { return Py_BuildValue("(ffff)", r.x, r.y, r.width, r.height); }

/* Texture (no pointer -> can be carried as dict) */
static PyObject *tex_dict(Texture2D t) {
    return Py_BuildValue("{s:i,s:i,s:i,s:i,s:i}", "id", (int)t.id,
                         "width", t.width, "height", t.height,
                         "mipmaps", t.mipmaps, "format", t.format);
}
static Texture2D py_tex(PyObject *o) {
    Texture2D t = { 0, 0, 0, 0, 0 };
    if (o == NULL || !PyDict_Check(o)) return t;
    PyObject *v;
    if ((v = PyDict_GetItemString(o, "id"))) t.id = (unsigned int)(PyLong_Check(v) ? (unsigned long)PyLong_AsLong(v) : 0u);
    if ((v = PyDict_GetItemString(o, "width"))) t.width = (int)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
    if ((v = PyDict_GetItemString(o, "height"))) t.height = (int)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
    if ((v = PyDict_GetItemString(o, "mipmaps"))) t.mipmaps = (int)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
    if ((v = PyDict_GetItemString(o, "format"))) t.format = (int)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
    return t;
}

/* RenderTexture: dict {id, texture, depth} */
static PyObject *rt_dict(RenderTexture2D rt) {
    return Py_BuildValue("{s:i,s:N,s:N}", "id", (int)rt.id,
                         "texture", tex_dict(rt.texture), "depth", tex_dict(rt.depth));
}
static RenderTexture2D py_rt(PyObject *o) {
    RenderTexture2D rt = { 0 };
    if (o == NULL || !PyDict_Check(o)) return rt;
    PyObject *v = PyDict_GetItemString(o, "id");
    if (v) rt.id = (unsigned int)(PyLong_Check(v) ? (unsigned long)PyLong_AsLong(v) : 0u);
    v = PyDict_GetItemString(o, "texture");
    if (v) rt.texture = py_tex(v);
    v = PyDict_GetItemString(o, "depth");
    if (v) rt.depth = py_tex(v);
    return rt;
}

/* Slot registries: pointer-carrying types (Font/Sound/Music/Image/Wave) */
#define FONT_SLOTS   8
#define SOUND_SLOTS  16
#define MUSIC_SLOTS  4
#define IMAGE_SLOTS  16
#define WAVE_SLOTS   4

static Font  g_fonts[FONT_SLOTS];
static int   g_fonts_used[FONT_SLOTS];
static Sound g_sounds[SOUND_SLOTS];
static int   g_sounds_used[SOUND_SLOTS];
static Music g_musics[MUSIC_SLOTS];
static int   g_musics_used[MUSIC_SLOTS];
static Image g_images[IMAGE_SLOTS];
static int   g_images_used[IMAGE_SLOTS];
static Wave  g_waves[WAVE_SLOTS];
static int   g_waves_used[WAVE_SLOTS];

static int alloc_slot(int *used, int cap, PyObject *what) {
    for (int i = 0; i < cap; i++)
        if (!used[i]) { used[i] = 1; return i; }
    PyErr_Format(PyExc_RuntimeError, "%s: slot registry full", what);
    return -1;
}

static Font *slot_font(PyObject *o) {
    if (o == NULL || !PyDict_Check(o)) return NULL;
    PyObject *v = PyDict_GetItemString(o, "slot");
    if (!v || !PyLong_Check(v)) return NULL;
    long s = PyLong_AsLong(v);
    if (s < 0 || s >= FONT_SLOTS || !g_fonts_used[s]) return NULL;
    return &g_fonts[s];
}
static Sound *slot_sound(PyObject *o) {
    if (o == NULL || !PyDict_Check(o)) return NULL;
    PyObject *v = PyDict_GetItemString(o, "slot");
    if (!v || !PyLong_Check(v)) return NULL;
    long s = PyLong_AsLong(v);
    if (s < 0 || s >= SOUND_SLOTS || !g_sounds_used[s]) return NULL;
    return &g_sounds[s];
}
static Music *slot_music(PyObject *o) {
    if (o == NULL || !PyDict_Check(o)) return NULL;
    PyObject *v = PyDict_GetItemString(o, "slot");
    if (!v || !PyLong_Check(v)) return NULL;
    long s = PyLong_AsLong(v);
    if (s < 0 || s >= MUSIC_SLOTS || !g_musics_used[s]) return NULL;
    return &g_musics[s];
}
static Image *slot_image(PyObject *o) {
    if (o == NULL || !PyDict_Check(o)) return NULL;
    PyObject *v = PyDict_GetItemString(o, "slot");
    if (!v || !PyLong_Check(v)) return NULL;
    long s = PyLong_AsLong(v);
    if (s < 0 || s >= IMAGE_SLOTS || !g_images_used[s]) return NULL;
    return &g_images[s];
}
static Wave *slot_wave(PyObject *o) {
    if (o == NULL || !PyDict_Check(o)) return NULL;
    PyObject *v = PyDict_GetItemString(o, "slot");
    if (!v || !PyLong_Check(v)) return NULL;
    long s = PyLong_AsLong(v);
    if (s < 0 || s >= WAVE_SLOTS || !g_waves_used[s]) return NULL;
    return &g_waves[s];
}

/* Macro wrappers (common 0/1/2-arg patterns) */
#define W0(F) static PyObject *f_##F(PyObject *s, PyObject *a){(void)s;(void)a;F();Py_RETURN_NONE;}
#define W0I(F) static PyObject *f_##F(PyObject *s, PyObject *a){(void)s;(void)a;return PyLong_FromLong((long)F());}
#define W0F(F) static PyObject *f_##F(PyObject *s, PyObject *a){(void)s;(void)a;return PyFloat_FromDouble((double)F());}
#define W0B(F) static PyObject *f_##F(PyObject *s, PyObject *a){(void)s;(void)a;return F()?Py_True:Py_False;}
#define W1i(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0;if(!PyArg_ParseTuple(a,"i",&a0))return NULL;F(a0);Py_RETURN_NONE;}
#define W1iI(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0;if(!PyArg_ParseTuple(a,"i",&a0))return NULL;return PyLong_FromLong((long)F(a0));}
#define W1iB(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0;if(!PyArg_ParseTuple(a,"i",&a0))return NULL;return F(a0)?Py_True:Py_False;}
#define W1iF(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0;if(!PyArg_ParseTuple(a,"i",&a0))return NULL;return PyFloat_FromDouble((double)F(a0));}
#define W1f(F) static PyObject *f_##F(PyObject *s, PyObject *a){float a0;if(!PyArg_ParseTuple(a,"f",&a0))return NULL;F(a0);Py_RETURN_NONE;}
#define W1s(F) static PyObject *f_##F(PyObject *s, PyObject *a){const char *a0;if(!PyArg_ParseTuple(a,"s",&a0))return NULL;F(a0);Py_RETURN_NONE;}
#define W1iS(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0;if(!PyArg_ParseTuple(a,"i",&a0))return NULL;return PyUnicode_FromString(F(a0));}
#define W2ii(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0,a1;if(!PyArg_ParseTuple(a,"ii",&a0,&a1))return NULL;F(a0,a1);Py_RETURN_NONE;}
#define W2iiI(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0,a1;if(!PyArg_ParseTuple(a,"ii",&a0,&a1))return NULL;return PyLong_FromLong((long)F(a0,a1));}
#define W2iiB(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0,a1;if(!PyArg_ParseTuple(a,"ii",&a0,&a1))return NULL;return F(a0,a1)?Py_True:Py_False;}
#define W2iiF(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0,a1;if(!PyArg_ParseTuple(a,"ii",&a0,&a1))return NULL;return PyFloat_FromDouble((double)F(a0,a1));}
#define W2ff(F) static PyObject *f_##F(PyObject *s, PyObject *a){float a0,a1;if(!PyArg_ParseTuple(a,"ff",&a0,&a1))return NULL;F(a0,a1);Py_RETURN_NONE;}
#define W3iii(F) static PyObject *f_##F(PyObject *s, PyObject *a){int a0,a1,a2;if(!PyArg_ParseTuple(a,"iii",&a0,&a1,&a2))return NULL;F(a0,a1,a2);Py_RETURN_NONE;}

/* 1) WINDOW / SCREEN / MONITOR / TIME */
static PyObject *f_InitWindow(PyObject *s, PyObject *a) {
    int w, h; const char *t;
    (void)s;
    if (!PyArg_ParseTuple(a, "iis", &w, &h, &t)) return NULL;
    InitWindow(w, h, t);
    Py_RETURN_NONE;
}
W0(CloseWindow)
W0B(WindowShouldClose)
W0B(IsWindowReady)
W0B(IsWindowFullscreen)
W0B(IsWindowHidden)
W0B(IsWindowMinimized)
W0B(IsWindowMaximized)
W0B(IsWindowFocused)
W0B(IsWindowResized)
W0(ToggleFullscreen)
W0(ToggleBorderlessWindowed)
W0(MaximizeWindow)
W0(MinimizeWindow)
W0(RestoreWindow)
W0(ShowCursor)
W0(HideCursor)
W0B(IsCursorHidden)
W0(EnableCursor)
W0(DisableCursor)
W1s(SetWindowTitle)
W1i(SetWindowState)
W1i(ClearWindowState)
W1iB(IsWindowState)
W2ii(SetWindowSize)
W2ii(SetWindowMinSize)
W2ii(SetWindowPosition)
W1f(SetWindowOpacity)
W1i(SetTargetFPS)
W1i(SetRandomSeed)
W1i(SetExitKey)
W1s(TakeScreenshot)
W1s(SetClipboardText)
W1i(SetConfigFlags)
W0I(GetScreenWidth)
W0I(GetScreenHeight)
W0I(GetRenderWidth)
W0I(GetRenderHeight)
W0I(GetMonitorCount)
W0I(GetCurrentMonitor)
W0I(GetFPS)
W0F(GetFrameTime)
W0F(GetTime)
W1iI(GetMonitorWidth)
W1iI(GetMonitorHeight)
W1iI(GetMonitorRefreshRate)
W1iS(GetMonitorName)
static PyObject *f_GetRandomValue(PyObject *s, PyObject *a) {
    int mn, mx;
    (void)s;
    if (!PyArg_ParseTuple(a, "ii", &mn, &mx)) return NULL;
    return PyLong_FromLong((long)GetRandomValue(mn, mx));
}
static PyObject *f_GetClipboardText(PyObject *s, PyObject *a) {
    (void)s; (void)a;
    return PyUnicode_FromString(GetClipboardText());
}

/* 2) KEYBOARD + MOUSE */
W1iB(IsKeyPressed)
W1iB(IsKeyDown)
W1iB(IsKeyReleased)
W1iB(IsKeyUp)
W0I(GetKeyPressed)
W0I(GetCharPressed)
W1iB(IsMouseButtonPressed)
W1iB(IsMouseButtonDown)
W1iB(IsMouseButtonReleased)
W1iB(IsMouseButtonUp)
W0I(GetMouseX)
W0I(GetMouseY)
W0F(GetMouseWheelMove)
W1i(SetMouseCursor)
W2ii(SetMousePosition)
W2ff(SetMouseScale)
static PyObject *f_GetMousePosition(PyObject *s, PyObject *a) { (void)s;(void)a;return vec2_tuple(GetMousePosition()); }
static PyObject *f_GetMouseDelta(PyObject *s, PyObject *a) { (void)s;(void)a;return vec2_tuple(GetMouseDelta()); }
static PyObject *f_GetMouseWheelMoveV(PyObject *s, PyObject *a) { (void)s;(void)a;return vec2_tuple(GetMouseWheelMoveV()); }

/* 3) 2D DRAWING + SHAPES */
static PyObject *f_DrawPixel(PyObject *s, PyObject *a) {
    int x, y; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiO", &x, &y, &c)) return NULL;
    DrawPixel(x, y, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawLine(PyObject *s, PyObject *a) {
    int x1, y1, x2, y2; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiiiO", &x1, &y1, &x2, &y2, &c)) return NULL;
    DrawLine(x1, y1, x2, y2, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawLineV(PyObject *s, PyObject *a) {
    PyObject *p1, *p2, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOO", &p1, &p2, &c)) return NULL;
    DrawLineV(py_vec2(p1), py_vec2(p2), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawLineEx(PyObject *s, PyObject *a) {
    PyObject *p1, *p2, *c; float th;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOfO", &p1, &p2, &th, &c)) return NULL;
    DrawLineEx(py_vec2(p1), py_vec2(p2), th, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircle(PyObject *s, PyObject *a) {
    int x, y; float r; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iifO", &x, &y, &r, &c)) return NULL;
    DrawCircle(x, y, r, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircleV(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfO", &p, &r, &c)) return NULL;
    DrawCircleV(py_vec2(p), r, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircleSector(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r, a1, a2; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfffiO", &p, &r, &a1, &a2, &seg, &c)) return NULL;
    DrawCircleSector(py_vec2(p), r, a1, a2, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircleSectorLines(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r, a1, a2; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfffiO", &p, &r, &a1, &a2, &seg, &c)) return NULL;
    DrawCircleSectorLines(py_vec2(p), r, a1, a2, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircleGradient(PyObject *s, PyObject *a) {
    PyObject *p, *c1, *c2; float r;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfOO", &p, &r, &c1, &c2)) return NULL;
    DrawCircleGradient(py_vec2(p), r, py_color(c1), py_color(c2));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCircleLines(PyObject *s, PyObject *a) {
    int x, y; float r; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iifO", &x, &y, &r, &c)) return NULL;
    DrawCircleLines(x, y, r, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawEllipse(PyObject *s, PyObject *a) {
    int x, y; float rx, ry; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiffO", &x, &y, &rx, &ry, &c)) return NULL;
    DrawEllipse(x, y, rx, ry, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawEllipseLines(PyObject *s, PyObject *a) {
    int x, y; float rx, ry; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiffO", &x, &y, &rx, &ry, &c)) return NULL;
    DrawEllipseLines(x, y, rx, ry, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRing(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r1, r2, a1, a2; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OffffiO", &p, &r1, &r2, &a1, &a2, &seg, &c)) return NULL;
    DrawRing(py_vec2(p), r1, r2, a1, a2, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRingLines(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r1, r2, a1, a2; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OffffiO", &p, &r1, &r2, &a1, &a2, &seg, &c)) return NULL;
    DrawRingLines(py_vec2(p), r1, r2, a1, a2, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangle(PyObject *s, PyObject *a) {
    int x, y, w, h; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiiiO", &x, &y, &w, &h, &c)) return NULL;
    DrawRectangle(x, y, w, h, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleV(PyObject *s, PyObject *a) {
    PyObject *p, *sz, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOO", &p, &sz, &c)) return NULL;
    DrawRectangleV(py_vec2(p), py_vec2(sz), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleRec(PyObject *s, PyObject *a) {
    PyObject *r, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OO", &r, &c)) return NULL;
    DrawRectangleRec(py_rect(r), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectanglePro(PyObject *s, PyObject *a) {
    PyObject *r, *o, *c; float rot;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOfO", &r, &o, &rot, &c)) return NULL;
    DrawRectanglePro(py_rect(r), py_vec2(o), rot, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleGradientV(PyObject *s, PyObject *a) {
    int x, y, w, h; PyObject *c1, *c2;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiiiOO", &x, &y, &w, &h, &c1, &c2)) return NULL;
    DrawRectangleGradientV(x, y, w, h, py_color(c1), py_color(c2));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleGradientH(PyObject *s, PyObject *a) {
    int x, y, w, h; PyObject *c1, *c2;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiiiOO", &x, &y, &w, &h, &c1, &c2)) return NULL;
    DrawRectangleGradientH(x, y, w, h, py_color(c1), py_color(c2));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleLines(PyObject *s, PyObject *a) {
    int x, y, w, h; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiiiO", &x, &y, &w, &h, &c)) return NULL;
    DrawRectangleLines(x, y, w, h, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleLinesEx(PyObject *s, PyObject *a) {
    PyObject *r, *c; float th;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfO", &r, &th, &c)) return NULL;
    DrawRectangleLinesEx(py_rect(r), th, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleRounded(PyObject *s, PyObject *a) {
    PyObject *r, *c; float rnd; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfiO", &r, &rnd, &seg, &c)) return NULL;
    DrawRectangleRounded(py_rect(r), rnd, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawRectangleRoundedLines(PyObject *s, PyObject *a) {
    PyObject *r, *c; float rnd; int seg;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfiO", &r, &rnd, &seg, &c)) return NULL;
    DrawRectangleRoundedLines(py_rect(r), rnd, seg, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTriangle(PyObject *s, PyObject *a) {
    PyObject *p1, *p2, *p3, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOO", &p1, &p2, &p3, &c)) return NULL;
    DrawTriangle(py_vec2(p1), py_vec2(p2), py_vec2(p3), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTriangleLines(PyObject *s, PyObject *a) {
    PyObject *p1, *p2, *p3, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOO", &p1, &p2, &p3, &c)) return NULL;
    DrawTriangleLines(py_vec2(p1), py_vec2(p2), py_vec2(p3), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawPoly(PyObject *s, PyObject *a) {
    PyObject *p, *c; int sides; float rad, rot;
    (void)s;
    if (!PyArg_ParseTuple(a, "OiffO", &p, &sides, &rad, &rot, &c)) return NULL;
    DrawPoly(py_vec2(p), sides, rad, rot, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawPolyLines(PyObject *s, PyObject *a) {
    PyObject *p, *c; int sides; float rad, rot;
    (void)s;
    if (!PyArg_ParseTuple(a, "OiffO", &p, &sides, &rad, &rot, &c)) return NULL;
    DrawPolyLines(py_vec2(p), sides, rad, rot, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawText(PyObject *s, PyObject *a) {
    const char *t; int x, y, sz; PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "siiiO", &t, &x, &y, &sz, &c)) return NULL;
    DrawText(t, x, y, sz, py_color(c));
    Py_RETURN_NONE;
}
W2ii(DrawFPS)
W1i(SetTextLineSpacing)
static PyObject *f_MeasureText(PyObject *s, PyObject *a) {
    const char *t; int sz;
    (void)s;
    if (!PyArg_ParseTuple(a, "si", &t, &sz)) return NULL;
    return PyLong_FromLong((long)MeasureText(t, sz));
}

/* Draw mode + log level: covers main.py BeginDrawing/EndDrawing and
 * dividing the info area into sections (SetTraceLogLevel). */
W0(BeginDrawing)
W0(EndDrawing)
W1i(SetTraceLogLevel)
static PyObject *f_ClearBackground(PyObject *s, PyObject *a) {
    PyObject *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &c)) return NULL;
    ClearBackground(py_color(c));
    Py_RETURN_NONE;
}

/* 4) TEXTURE + RENDERTEXTURE + IMAGE (slot registries) */
static PyObject *f_LoadTexture(PyObject *s, PyObject *a) {
    const char *p;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    return tex_dict(LoadTexture(p));
}
static PyObject *f_LoadTextureFromImage(PyObject *s, PyObject *a) {
    PyObject *img;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &img)) return NULL;
    return tex_dict(LoadTextureFromImage(*slot_image(img)));
}
static PyObject *f_LoadRenderTexture(PyObject *s, PyObject *a) {
    int w, h;
    (void)s;
    if (!PyArg_ParseTuple(a, "ii", &w, &h)) return NULL;
    return rt_dict(LoadRenderTexture(w, h));
}
static PyObject *f_UnloadTexture(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    UnloadTexture(py_tex(o));
    Py_RETURN_NONE;
}
static PyObject *f_UnloadRenderTexture(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    UnloadRenderTexture(py_rt(o));
    Py_RETURN_NONE;
}
static PyObject *f_IsTextureReady(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    return IsTextureValid(py_tex(o)) ? Py_True : Py_False;
}
static PyObject *f_SetTextureFilter(PyObject *s, PyObject *a) {
    PyObject *o; int m;
    (void)s;
    if (!PyArg_ParseTuple(a, "Oi", &o, &m)) return NULL;
    SetTextureFilter(py_tex(o), m);
    Py_RETURN_NONE;
}
static PyObject *f_SetTextureWrap(PyObject *s, PyObject *a) {
    PyObject *o; int m;
    (void)s;
    if (!PyArg_ParseTuple(a, "Oi", &o, &m)) return NULL;
    SetTextureWrap(py_tex(o), m);
    Py_RETURN_NONE;
}
static PyObject *f_DrawTexture(PyObject *s, PyObject *a) {
    PyObject *o, *c; int x, y;
    (void)s;
    if (!PyArg_ParseTuple(a, "OiiO", &o, &x, &y, &c)) return NULL;
    DrawTexture(py_tex(o), x, y, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTextureV(PyObject *s, PyObject *a) {
    PyObject *o, *p, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOO", &o, &p, &c)) return NULL;
    DrawTextureV(py_tex(o), py_vec2(p), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTextureEx(PyObject *s, PyObject *a) {
    PyObject *o, *p, *c; float rot, sc;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOffO", &o, &p, &rot, &sc, &c)) return NULL;
    DrawTextureEx(py_tex(o), py_vec2(p), rot, sc, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTexturePro(PyObject *s, PyObject *a) {
    PyObject *o, *src, *dst, *orig, *c; float rot;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOOfO", &o, &src, &dst, &orig, &rot, &c)) return NULL;
    DrawTexturePro(py_tex(o), py_rect(src), py_rect(dst), py_vec2(orig), rot, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTextureRec(PyObject *s, PyObject *a) {
    PyObject *o, *src, *p, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOO", &o, &src, &p, &c)) return NULL;
    DrawTextureRec(py_tex(o), py_rect(src), py_vec2(p), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_UpdateTexture(PyObject *s, PyObject *a) {
    PyObject *o, *img;
    (void)s;
    if (!PyArg_ParseTuple(a, "OO", &o, &img)) return NULL;
    UpdateTexture(py_tex(o), slot_image(img)->data);
    Py_RETURN_NONE;
}
static PyObject *f_BeginTextureMode(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    BeginTextureMode(py_rt(o));
    Py_RETURN_NONE;
}
W0(EndTextureMode)

/* Image (slot) */
static PyObject *f_GenImageColor(PyObject *s, PyObject *a) {
    int w, h; PyObject *c; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "iiO", &w, &h, &c)) return NULL;
    slot = alloc_slot(g_images_used, IMAGE_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_images[slot] = GenImageColor(w, h, py_color(c));
    return Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i}", "slot", slot,
                         "width", g_images[slot].width, "height", g_images[slot].height,
                         "mipmaps", g_images[slot].mipmaps, "format", g_images[slot].format, "data_ok", 1);
}
static PyObject *f_LoadImage(PyObject *s, PyObject *a) {
    const char *p; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    slot = alloc_slot(g_images_used, IMAGE_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_images[slot] = LoadImage(p);
    return Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i}", "slot", slot,
                         "width", g_images[slot].width, "height", g_images[slot].height,
                         "mipmaps", g_images[slot].mipmaps, "format", g_images[slot].format, "data_ok", 1);
}
static PyObject *f_UnloadImage(PyObject *s, PyObject *a) {
    PyObject *o; long slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    Image *im = slot_image(o);
    if (!im) { PyErr_SetString(PyExc_ValueError, "invalid image slot"); return NULL; }
    UnloadImage(*im);
    slot = PyLong_AsLong(PyDict_GetItemString(o, "slot"));
    g_images_used[slot] = 0;
    Py_RETURN_NONE;
}

/* 5) FONT + TEXT (from Font slot registry) */
static PyObject *font_dict(int slot) {
    Font *f = &g_fonts[slot];
    return Py_BuildValue("{s:i,s:i,s:i,s:i}", "slot", slot,
                         "baseSize", f->baseSize, "glyphCount", f->glyphCount,
                         "glyphPadding", f->glyphPadding);
}
static PyObject *f_LoadFont(PyObject *s, PyObject *a) {
    const char *p; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    slot = alloc_slot(g_fonts_used, FONT_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_fonts[slot] = LoadFont(p);
    if (!IsFontValid(g_fonts[slot])) {
        g_fonts_used[slot] = 0;
        PyErr_SetString(PyExc_IOError, "font load failed");
        return NULL;
    }
    return font_dict(slot);
}
static PyObject *f_LoadFontEx(PyObject *s, PyObject *a) {
    const char *p; int sz, count, slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "sii", &p, &sz, &count)) return NULL;
    slot = alloc_slot(g_fonts_used, FONT_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_fonts[slot] = LoadFontEx(p, sz, NULL, count);
    if (!IsFontValid(g_fonts[slot])) {
        g_fonts_used[slot] = 0;
        PyErr_SetString(PyExc_IOError, "font load failed");
        return NULL;
    }
    return font_dict(slot);
}
static PyObject *f_UnloadFont(PyObject *s, PyObject *a) {
    PyObject *o; Font *fnt;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    fnt = slot_font(o);
    if (!fnt) { PyErr_SetString(PyExc_ValueError, "invalid font slot"); return NULL; }
    UnloadFont(*fnt);
    g_fonts_used[(int)PyLong_AsLong(PyDict_GetItemString(o, "slot"))] = 0;
    Py_RETURN_NONE;
}
static PyObject *f_IsFontReady(PyObject *s, PyObject *a) {
    PyObject *o; Font *fnt;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    fnt = slot_font(o);
    return fnt && IsFontValid(*fnt) ? Py_True : Py_False;
}
static PyObject *f_MeasureTextEx(PyObject *s, PyObject *a) {
    PyObject *o; const char *t; float sz, sp; Font *fnt;
    (void)s;
    if (!PyArg_ParseTuple(a, "Osff", &o, &t, &sz, &sp)) return NULL;
    fnt = slot_font(o);
    if (!fnt) { PyErr_SetString(PyExc_ValueError, "invalid font slot"); return NULL; }
    return vec2_tuple(MeasureTextEx(*fnt, t, sz, sp));
}
static PyObject *f_DrawTextEx(PyObject *s, PyObject *a) {
    PyObject *o, *p, *c; const char *t; float sz, sp; Font *fnt;
    (void)s;
    if (!PyArg_ParseTuple(a, "OsOffO", &o, &t, &p, &sz, &sp, &c)) return NULL;
    fnt = slot_font(o);
    if (!fnt) { PyErr_SetString(PyExc_ValueError, "invalid font slot"); return NULL; }
    DrawTextEx(*fnt, t, py_vec2(p), sz, sp, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawTextPro(PyObject *s, PyObject *a) {
    PyObject *o, *p, *origin, *c; const char *t;
    float sz, sp, rot; Font *fnt;
    (void)s;
    if (!PyArg_ParseTuple(a, "OsOOfffO", &o, &t, &p, &origin, &rot, &sz, &sp, &c)) return NULL;
    fnt = slot_font(o);
    if (!fnt) { PyErr_SetString(PyExc_ValueError, "invalid font slot"); return NULL; }
    DrawTextPro(*fnt, t, py_vec2(p), py_vec2(origin), rot, sz, sp, py_color(c));
    Py_RETURN_NONE;
}

/* 6) CAMERA (2D/3D) + BASIC 3D */
static Camera2D py_cam2d(PyObject *o) {
    Camera2D c = { { 0, 0 }, { 0, 0 }, 0, 1 };
    if (o == NULL || !PyDict_Check(o)) return c;
    PyObject *v;
    if ((v = PyDict_GetItemString(o, "offset"))) c.offset = py_vec2(v);
    if ((v = PyDict_GetItemString(o, "target"))) c.target = py_vec2(v);
    if ((v = PyDict_GetItemString(o, "rotation"))) c.rotation = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
    if ((v = PyDict_GetItemString(o, "zoom"))) c.zoom = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
    else c.zoom = 1.0f;
    return c;
}
static Camera3D py_cam3d(PyObject *o) {
    Camera3D c = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, 45, 0 };
    if (o == NULL || !PyDict_Check(o)) return c;
    PyObject *v;
    if ((v = PyDict_GetItemString(o, "position"))) c.position = py_vec3(v);
    if ((v = PyDict_GetItemString(o, "target"))) c.target = py_vec3(v);
    if ((v = PyDict_GetItemString(o, "up"))) c.up = py_vec3(v);
    if ((v = PyDict_GetItemString(o, "fovy"))) c.fovy = (float)(PyLong_Check(v) ? (double)PyLong_AsLong(v) : PyFloat_AsDouble(v));
    if ((v = PyDict_GetItemString(o, "projection"))) c.projection = (int)(PyLong_Check(v) ? PyLong_AsLong(v) : 0);
    return c;
}
static PyObject *f_BeginMode2D(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    BeginMode2D(py_cam2d(o));
    Py_RETURN_NONE;
}
W0(EndMode2D)
static PyObject *f_BeginMode3D(PyObject *s, PyObject *a) {
    PyObject *o;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    BeginMode3D(py_cam3d(o));
    Py_RETURN_NONE;
}
W0(EndMode3D)
static PyObject *f_DrawGrid(PyObject *s, PyObject *a) {
    int slices; float sp;
    (void)s;
    if (!PyArg_ParseTuple(a, "if", &slices, &sp)) return NULL;
    DrawGrid(slices, sp);
    Py_RETURN_NONE;
}
static PyObject *f_DrawPlane(PyObject *s, PyObject *a) {
    PyObject *p, *sz, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOO", &p, &sz, &c)) return NULL;
    DrawPlane(py_vec3(p), py_vec2(sz), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCube(PyObject *s, PyObject *a) {
    PyObject *p, *c; float w, h, l;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfffO", &p, &w, &h, &l, &c)) return NULL;
    DrawCube(py_vec3(p), w, h, l, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCubeV(PyObject *s, PyObject *a) {
    PyObject *p, *sz, *c;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOO", &p, &sz, &c)) return NULL;
    DrawCubeV(py_vec3(p), py_vec3(sz), py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawCubeWires(PyObject *s, PyObject *a) {
    PyObject *p, *c; float w, h, l;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfffO", &p, &w, &h, &l, &c)) return NULL;
    DrawCubeWires(py_vec3(p), w, h, l, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawSphere(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfO", &p, &r, &c)) return NULL;
    DrawSphere(py_vec3(p), r, py_color(c));
    Py_RETURN_NONE;
}
static PyObject *f_DrawSphereWires(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r; int rings, slices;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfiiO", &p, &r, &rings, &slices, &c)) return NULL;
    DrawSphereWires(py_vec3(p), r, rings, slices, py_color(c));
    Py_RETURN_NONE;
}

/* 7) COLLISION TESTS */
static PyObject *f_CheckCollisionRecs(PyObject *s, PyObject *a) {
    PyObject *r1, *r2;
    (void)s;
    if (!PyArg_ParseTuple(a, "OO", &r1, &r2)) return NULL;
    return CheckCollisionRecs(py_rect(r1), py_rect(r2)) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionCircles(PyObject *s, PyObject *a) {
    PyObject *p1, *p2; float r1, r2;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfOf", &p1, &r1, &p2, &r2)) return NULL;
    return CheckCollisionCircles(py_vec2(p1), r1, py_vec2(p2), r2) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionCircleRec(PyObject *s, PyObject *a) {
    PyObject *p, *r; float rad;
    (void)s;
    if (!PyArg_ParseTuple(a, "OfO", &p, &rad, &r)) return NULL;
    return CheckCollisionCircleRec(py_vec2(p), rad, py_rect(r)) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionPointRec(PyObject *s, PyObject *a) {
    PyObject *p, *r;
    (void)s;
    if (!PyArg_ParseTuple(a, "OO", &p, &r)) return NULL;
    return CheckCollisionPointRec(py_vec2(p), py_rect(r)) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionPointCircle(PyObject *s, PyObject *a) {
    PyObject *p, *c; float r;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOf", &p, &c, &r)) return NULL;
    return CheckCollisionPointCircle(py_vec2(p), py_vec2(c), r) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionPointTriangle(PyObject *s, PyObject *a) {
    PyObject *p, *a1, *a2, *a3;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOO", &p, &a1, &a2, &a3)) return NULL;
    return CheckCollisionPointTriangle(py_vec2(p), py_vec2(a1), py_vec2(a2), py_vec2(a3)) ? Py_True : Py_False;
}
static PyObject *f_CheckCollisionLines(PyObject *s, PyObject *a) {
    PyObject *s1, *e1, *s2, *e2; Vector2 cp;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOO", &s1, &e1, &s2, &e2)) return NULL;
    bool hit = CheckCollisionLines(py_vec2(s1), py_vec2(e1), py_vec2(s2), py_vec2(e2), &cp);
    return Py_BuildValue("(O(ff))", hit ? Py_True : Py_False, cp.x, cp.y);
}
static PyObject *f_CheckCollisionPointLine(PyObject *s, PyObject *a) {
    PyObject *p, *p1, *p2; int th;
    (void)s;
    if (!PyArg_ParseTuple(a, "OOOi", &p, &p1, &p2, &th)) return NULL;
    return CheckCollisionPointLine(py_vec2(p), py_vec2(p1), py_vec2(p2), th) ? Py_True : Py_False;
}
static PyObject *f_GetCollisionRec(PyObject *s, PyObject *a) {
    PyObject *r1, *r2;
    (void)s;
    if (!PyArg_ParseTuple(a, "OO", &r1, &r2)) return NULL;
    return rect_tuple(GetCollisionRec(py_rect(r1), py_rect(r2)));
}

/* 8) AUDIO (Sound/Music slot registries) */
W0(InitAudioDevice)
W0(CloseAudioDevice)
static PyObject *f_AudioDeviceReady(PyObject *s, PyObject *a) { (void)s;(void)a;return IsAudioDeviceReady()?Py_True:Py_False; }
W1f(SetMasterVolume)
W0F(GetMasterVolume)

static PyObject *f_LoadWave(PyObject *s, PyObject *a) {
    const char *p; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    slot = alloc_slot(g_waves_used, WAVE_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_waves[slot] = LoadWave(p);
    return Py_BuildValue("{s:i,s:i,s:i,s:i}", "slot", slot,
                         "sampleRate", (int)g_waves[slot].sampleRate,
                         "sampleSize", (int)g_waves[slot].sampleSize,
                         "channels", (int)g_waves[slot].channels);
}
static PyObject *f_LoadSound(PyObject *s, PyObject *a) {
    const char *p; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    slot = alloc_slot(g_sounds_used, SOUND_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_sounds[slot] = LoadSound(p);
    if (!IsSoundValid(g_sounds[slot])) {
        g_sounds_used[slot] = 0;
        PyErr_SetString(PyExc_IOError, "sound load failed");
        return NULL;
    }
    return Py_BuildValue("{s:i,s:i}", "slot", slot, "frameCount", (int)g_sounds[slot].frameCount);
}
static PyObject *f_LoadSoundFromWave(PyObject *s, PyObject *a) {
    PyObject *o; int slot; Wave *w;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    w = slot_wave(o);
    if (!w) { PyErr_SetString(PyExc_ValueError, "invalid wave slot"); return NULL; }
    slot = alloc_slot(g_sounds_used, SOUND_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_sounds[slot] = LoadSoundFromWave(*w);
    return Py_BuildValue("{s:i,s:i}", "slot", slot, "frameCount", (int)g_sounds[slot].frameCount);
}
static PyObject *f_UnloadSound(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    UnloadSound(*sd);
    g_sounds_used[(int)PyLong_AsLong(PyDict_GetItemString(o, "slot"))] = 0;
    Py_RETURN_NONE;
}
static PyObject *f_IsSoundReady(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    return sd && IsSoundValid(*sd) ? Py_True : Py_False;
}
static PyObject *f_PlaySound(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    PlaySound(*sd);
    Py_RETURN_NONE;
}
static PyObject *f_StopSound(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    StopSound(*sd);
    Py_RETURN_NONE;
}
static PyObject *f_PauseSound(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    PauseSound(*sd);
    Py_RETURN_NONE;
}
static PyObject *f_ResumeSound(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    ResumeSound(*sd);
    Py_RETURN_NONE;
}
static PyObject *f_IsSoundPlaying(PyObject *s, PyObject *a) {
    PyObject *o; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    sd = slot_sound(o);
    return sd && IsSoundPlaying(*sd) ? Py_True : Py_False;
}
static PyObject *f_SetSoundVolume(PyObject *s, PyObject *a) {
    PyObject *o; float v; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "Of", &o, &v)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    SetSoundVolume(*sd, v);
    Py_RETURN_NONE;
}
static PyObject *f_SetSoundPitch(PyObject *s, PyObject *a) {
    PyObject *o; float p; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "Of", &o, &p)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    SetSoundPitch(*sd, p);
    Py_RETURN_NONE;
}
static PyObject *f_SetSoundPan(PyObject *s, PyObject *a) {
    PyObject *o; float p; Sound *sd;
    (void)s;
    if (!PyArg_ParseTuple(a, "Of", &o, &p)) return NULL;
    sd = slot_sound(o);
    if (!sd) { PyErr_SetString(PyExc_ValueError, "invalid sound slot"); return NULL; }
    SetSoundPan(*sd, p);
    Py_RETURN_NONE;
}
static PyObject *f_LoadMusicStream(PyObject *s, PyObject *a) {
    const char *p; int slot;
    (void)s;
    if (!PyArg_ParseTuple(a, "s", &p)) return NULL;
    slot = alloc_slot(g_musics_used, MUSIC_SLOTS, Py_None);
    if (slot < 0) return NULL;
    g_musics[slot] = LoadMusicStream(p);
    if (!IsMusicValid(g_musics[slot])) {
        g_musics_used[slot] = 0;
        PyErr_SetString(PyExc_IOError, "music load failed");
        return NULL;
    }
    return Py_BuildValue("{s:i,s:i}", "slot", slot, "frameCount", (int)g_musics[slot].frameCount);
}
static PyObject *f_UnloadMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    UnloadMusicStream(*mu);
    g_musics_used[(int)PyLong_AsLong(PyDict_GetItemString(o, "slot"))] = 0;
    Py_RETURN_NONE;
}
static PyObject *f_PlayMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    PlayMusicStream(*mu);
    Py_RETURN_NONE;
}
static PyObject *f_UpdateMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    UpdateMusicStream(*mu);
    Py_RETURN_NONE;
}
static PyObject *f_StopMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    StopMusicStream(*mu);
    Py_RETURN_NONE;
}
static PyObject *f_PauseMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    PauseMusicStream(*mu);
    Py_RETURN_NONE;
}
static PyObject *f_ResumeMusicStream(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    ResumeMusicStream(*mu);
    Py_RETURN_NONE;
}
static PyObject *f_IsMusicStreamPlaying(PyObject *s, PyObject *a) {
    PyObject *o; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "O", &o)) return NULL;
    mu = slot_music(o);
    return mu && IsMusicValid(*mu) && IsMusicStreamPlaying(*mu) ? Py_True : Py_False;
}
static PyObject *f_SetMusicVolume(PyObject *s, PyObject *a) {
    PyObject *o; float v; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "Of", &o, &v)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    SetMusicVolume(*mu, v);
    Py_RETURN_NONE;
}
static PyObject *f_SetMusicPitch(PyObject *s, PyObject *a) {
    PyObject *o; float p; Music *mu;
    (void)s;
    if (!PyArg_ParseTuple(a, "Of", &o, &p)) return NULL;
    mu = slot_music(o);
    if (!mu) { PyErr_SetString(PyExc_ValueError, "invalid music slot"); return NULL; }
    SetMusicPitch(*mu, p);
    Py_RETURN_NONE;
}

/* Method table */
#define M(name, doc) { #name, f_##name, METH_VARARGS, doc }

static PyMethodDef raylib_methods[] = {
    /* window/screen/timer */
    M(InitWindow, "InitWindow(w, h, title)"),
    M(CloseWindow, "CloseWindow()"),
    M(WindowShouldClose, "WindowShouldClose() -> bool"),
    M(IsWindowReady, "IsWindowReady() -> bool"),
    M(IsWindowFullscreen, "IsWindowFullscreen() -> bool"),
    M(IsWindowHidden, "IsWindowHidden() -> bool"),
    M(IsWindowMinimized, "IsWindowMinimized() -> bool"),
    M(IsWindowMaximized, "IsWindowMaximized() -> bool"),
    M(IsWindowFocused, "IsWindowFocused() -> bool"),
    M(IsWindowResized, "IsWindowResized() -> bool"),
    M(IsWindowState, "IsWindowState(flags) -> bool"),
    M(SetWindowState, "SetWindowState(flags)"),
    M(ClearWindowState, "ClearWindowState(flags)"),
    M(ToggleFullscreen, "ToggleFullscreen()"),
    M(ToggleBorderlessWindowed, "ToggleBorderlessWindowed()"),
    M(MaximizeWindow, "MaximizeWindow()"),
    M(MinimizeWindow, "MinimizeWindow()"),
    M(RestoreWindow, "RestoreWindow()"),
    M(SetWindowTitle, "SetWindowTitle(title)"),
    M(SetWindowPosition, "SetWindowPosition(x, y)"),
    M(SetWindowSize, "SetWindowSize(w, h)"),
    M(SetWindowMinSize, "SetWindowMinSize(w, h)"),
    M(SetWindowOpacity, "SetWindowOpacity(opacity)"),
    M(SetTargetFPS, "SetTargetFPS(fps)"),
    M(GetScreenWidth, "GetScreenWidth() -> int"),
    M(GetScreenHeight, "GetScreenHeight() -> int"),
    M(GetRenderWidth, "GetRenderWidth() -> int"),
    M(GetRenderHeight, "GetRenderHeight() -> int"),
    M(GetMonitorCount, "GetMonitorCount() -> int"),
    M(GetCurrentMonitor, "GetCurrentMonitor() -> int"),
    M(GetMonitorWidth, "GetMonitorWidth(idx) -> int"),
    M(GetMonitorHeight, "GetMonitorHeight(idx) -> int"),
    M(GetMonitorRefreshRate, "GetMonitorRefreshRate(idx) -> int"),
    M(GetMonitorName, "GetMonitorName(idx) -> str"),
    M(GetFPS, "GetFPS() -> int"),
    M(GetFrameTime, "GetFrameTime() -> float"),
    M(GetTime, "GetTime() -> float"),
    M(SetRandomSeed, "SetRandomSeed(seed)"),
    M(GetRandomValue, "GetRandomValue(min, max) -> int"),
    M(TakeScreenshot, "TakeScreenshot(fname)"),
    M(SetConfigFlags, "SetConfigFlags(flags)"),
    M(SetExitKey, "SetExitKey(key)"),
    M(ShowCursor, "ShowCursor()"),
    M(HideCursor, "HideCursor()"),
    M(IsCursorHidden, "IsCursorHidden() -> bool"),
    M(EnableCursor, "EnableCursor()"),
    M(DisableCursor, "DisableCursor()"),
    M(GetClipboardText, "GetClipboardText() -> str"),
    M(SetClipboardText, "SetClipboardText(text)"),

    /* keyboard + mouse */
    M(IsKeyPressed, "IsKeyPressed(key) -> bool"),
    M(IsKeyDown, "IsKeyDown(key) -> bool"),
    M(IsKeyReleased, "IsKeyReleased(key) -> bool"),
    M(IsKeyUp, "IsKeyUp(key) -> bool"),
    M(GetKeyPressed, "GetKeyPressed() -> int"),
    M(GetCharPressed, "GetCharPressed() -> int"),
    M(IsMouseButtonPressed, "IsMouseButtonPressed(btn) -> bool"),
    M(IsMouseButtonDown, "IsMouseButtonDown(btn) -> bool"),
    M(IsMouseButtonReleased, "IsMouseButtonReleased(btn) -> bool"),
    M(IsMouseButtonUp, "IsMouseButtonUp(btn) -> bool"),
    M(GetMouseX, "GetMouseX() -> int"),
    M(GetMouseY, "GetMouseY() -> int"),
    M(GetMousePosition, "GetMousePosition() -> (x,y)"),
    M(GetMouseDelta, "GetMouseDelta() -> (x,y)"),
    M(GetMouseWheelMove, "GetMouseWheelMove() -> float"),
    M(GetMouseWheelMoveV, "GetMouseWheelMoveV() -> (x,y)"),
    M(SetMousePosition, "SetMousePosition(x, y)"),
    M(SetMouseScale, "SetMouseScale(sx, sy)"),
    M(SetMouseCursor, "SetMouseCursor(cursor)"),

    /* drawing + shapes */
    M(DrawPixel, "DrawPixel(x, y, color)"),
    M(DrawLine, "DrawLine(x1, y1, x2, y2, color)"),
    M(DrawLineV, "DrawLineV((x,y), (x,y), color)"),
    M(DrawLineEx, "DrawLineEx((x,y), (x,y), thick, color)"),
    M(DrawCircle, "DrawCircle(x, y, r, color)"),
    M(DrawCircleV, "DrawCircleV((x,y), r, color)"),
    M(DrawCircleSector, "DrawCircleSector(center, r, start, end, seg, color)"),
    M(DrawCircleSectorLines, "DrawCircleSectorLines(center, r, start, end, seg, color)"),
    M(DrawCircleGradient, "DrawCircleGradient(x, y, r, c1, c2)"),
    M(DrawCircleLines, "DrawCircleLines(x, y, r, color)"),
    M(DrawEllipse, "DrawEllipse(x, y, rx, ry, color)"),
    M(DrawEllipseLines, "DrawEllipseLines(x, y, rx, ry, color)"),
    M(DrawRing, "DrawRing(center, r1, r2, start, end, seg, color)"),
    M(DrawRingLines, "DrawRingLines(center, r1, r2, start, end, seg, color)"),
    M(DrawRectangle, "DrawRectangle(x, y, w, h, color)"),
    M(DrawRectangleV, "DrawRectangleV(pos, size, color)"),
    M(DrawRectangleRec, "DrawRectangleRec((x,y,w,h), color)"),
    M(DrawRectanglePro, "DrawRectanglePro(rec, origin, rot, color)"),
    M(DrawRectangleGradientV, "DrawRectangleGradientV(x, y, w, h, c1, c2)"),
    M(DrawRectangleGradientH, "DrawRectangleGradientH(x, y, w, h, c1, c2)"),
    M(DrawRectangleLines, "DrawRectangleLines(x, y, w, h, color)"),
    M(DrawRectangleLinesEx, "DrawRectangleLinesEx(rec, thick, color)"),
    M(DrawRectangleRounded, "DrawRectangleRounded(rec, round, seg, color)"),
    M(DrawRectangleRoundedLines, "DrawRectangleRoundedLines(rec, round, seg, color)"),
    M(DrawTriangle, "DrawTriangle(p1, p2, p3, color)"),
    M(DrawTriangleLines, "DrawTriangleLines(p1, p2, p3, color)"),
    M(DrawPoly, "DrawPoly(center, sides, radius, rot, color)"),
    M(DrawPolyLines, "DrawPolyLines(center, sides, radius, rot, color)"),
    M(DrawFPS, "DrawFPS(x, y)"),
    M(BeginDrawing, "BeginDrawing()"),
    M(EndDrawing, "EndDrawing()"),
    M(ClearBackground, "ClearBackground(color)"),
    M(SetTraceLogLevel, "SetTraceLogLevel(level)"),

    /* text */
    M(DrawText, "DrawText(text, x, y, size, color)"),
    M(SetTextLineSpacing, "SetTextLineSpacing(spacing)"),
    M(MeasureText, "MeasureText(text, size) -> int"),
    M(LoadFont, "LoadFont(fname) -> {slot,...}"),
    M(LoadFontEx, "LoadFontEx(fname, size, count) -> {slot,...}"),
    M(UnloadFont, "UnloadFont(font)"),
    M(IsFontReady, "IsFontReady(font) -> bool"),
    M(MeasureTextEx, "MeasureTextEx(font, text, size, spacing) -> (w,h)"),
    M(DrawTextEx, "DrawTextEx(font, text, pos, size, spacing, color)"),
    M(DrawTextPro, "DrawTextPro(font, text, pos, origin, rot, size, spacing, color)"),

    /* texture */
    M(LoadTexture, "LoadTexture(fname) -> {id,w,h,m,f}"),
    M(LoadTextureFromImage, "LoadTextureFromImage(image) -> {id,w,h,m,f}"),
    M(LoadRenderTexture, "LoadRenderTexture(w, h) -> {id,texture,depth}"),
    M(UnloadTexture, "UnloadTexture(tex)"),
    M(UnloadRenderTexture, "UnloadRenderTexture(rt)"),
    M(IsTextureReady, "IsTextureReady(tex) -> bool"),
    M(SetTextureFilter, "SetTextureFilter(tex, filter)"),
    M(SetTextureWrap, "SetTextureWrap(tex, wrap)"),
    M(DrawTexture, "DrawTexture(tex, x, y, color)"),
    M(DrawTextureV, "DrawTextureV(tex, pos, color)"),
    M(DrawTextureEx, "DrawTextureEx(tex, pos, rot, scale, color)"),
    M(DrawTexturePro, "DrawTexturePro(tex, src, dst, origin, rot, color)"),
    M(DrawTextureRec, "DrawTextureRec(tex, src, pos, color)"),
    M(UpdateTexture, "UpdateTexture(tex, image)"),
    M(BeginTextureMode, "BeginTextureMode(rt)"),
    M(EndTextureMode, "EndTextureMode()"),
    M(GenImageColor, "GenImageColor(w, h, color) -> {slot,...}"),
    M(LoadImage, "LoadImage(fname) -> {slot,...}"),
    M(UnloadImage, "UnloadImage(image)"),

    /* camera + 3d */
    M(BeginMode2D, "BeginMode2D(camera)"),
    M(EndMode2D, "EndMode2D()"),
    M(BeginMode3D, "BeginMode3D(camera)"),
    M(EndMode3D, "EndMode3D()"),
    M(DrawGrid, "DrawGrid(slices, spacing)"),
    M(DrawPlane, "DrawPlane(pos3, size2, color)"),
    M(DrawCube, "DrawCube(pos3, w, h, l, color)"),
    M(DrawCubeV, "DrawCubeV(pos3, size3, color)"),
    M(DrawCubeWires, "DrawCubeWires(pos3, w, h, l, color)"),
    M(DrawSphere, "DrawSphere(pos3, r, color)"),
    M(DrawSphereWires, "DrawSphereWires(pos3, r, rings, slices, color)"),

    /* collision */
    M(CheckCollisionRecs, "CheckCollisionRecs(r1, r2) -> bool"),
    M(CheckCollisionCircles, "CheckCollisionCircles(c1, r1, c2, r2) -> bool"),
    M(CheckCollisionCircleRec, "CheckCollisionCircleRec(c, r, rec) -> bool"),
    M(CheckCollisionPointRec, "CheckCollisionPointRec(p, rec) -> bool"),
    M(CheckCollisionPointCircle, "CheckCollisionPointCircle(p, c, r) -> bool"),
    M(CheckCollisionPointTriangle, "CheckCollisionPointTriangle(p, a, b, c) -> bool"),
    M(CheckCollisionLines, "CheckCollisionLines(s1, e1, s2, e2) -> (bool, (x,y))"),
    M(CheckCollisionPointLine, "CheckCollisionPointLine(p, p1, p2, threshold) -> bool"),
    M(GetCollisionRec, "GetCollisionRec(r1, r2) -> (x,y,w,h)"),

    /* audio */
    M(InitAudioDevice, "InitAudioDevice()"),
    M(CloseAudioDevice, "CloseAudioDevice()"),
    M(AudioDeviceReady, "AudioDeviceReady() -> bool"),
    M(SetMasterVolume, "SetMasterVolume(volume)"),
    M(GetMasterVolume, "GetMasterVolume() -> float"),
    M(LoadWave, "LoadWave(fname) -> {slot,...}"),
    M(LoadSound, "LoadSound(fname) -> {slot,...}"),
    M(LoadSoundFromWave, "LoadSoundFromWave(wave) -> {slot,...}"),
    M(UnloadSound, "UnloadSound(sound)"),
    M(IsSoundReady, "IsSoundReady(sound) -> bool"),
    M(PlaySound, "PlaySound(sound)"),
    M(StopSound, "StopSound(sound)"),
    M(PauseSound, "PauseSound(sound)"),
    M(ResumeSound, "ResumeSound(sound)"),
    M(IsSoundPlaying, "IsSoundPlaying(sound) -> bool"),
    M(SetSoundVolume, "SetSoundVolume(sound, volume)"),
    M(SetSoundPitch, "SetSoundPitch(sound, pitch)"),
    M(SetSoundPan, "SetSoundPan(sound, pan)"),
    M(LoadMusicStream, "LoadMusicStream(fname) -> {slot,...}"),
    M(UnloadMusicStream, "UnloadMusicStream(music)"),
    M(PlayMusicStream, "PlayMusicStream(music)"),
    M(UpdateMusicStream, "UpdateMusicStream(music)"),
    M(StopMusicStream, "StopMusicStream(music)"),
    M(PauseMusicStream, "PauseMusicStream(music)"),
    M(ResumeMusicStream, "ResumeMusicStream(music)"),
    M(IsMusicStreamPlaying, "IsMusicStreamPlaying(music) -> bool"),
    M(SetMusicVolume, "SetMusicVolume(music, volume)"),
    M(SetMusicPitch, "SetMusicPitch(music, pitch)"),

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef raylib_module = {
    PyModuleDef_HEAD_INIT,
    "gcl_raylib",
    "Gnuchan gcl raylib binding — comprehensive manual binding",
    -1,
    raylib_methods
};

/* Constants (colors / keys / mouse / flags) */
static void add_colors(PyObject *m) {
    PyObject *c;
#define ADDCOLOR(NAME, r, g, b, a) c = Py_BuildValue("[iiii]", r, g, b, a); PyModule_AddObject(m, NAME, c)
    ADDCOLOR("LIGHTGRAY", 200, 200, 200, 255);
    ADDCOLOR("GRAY", 130, 130, 130, 255);
    ADDCOLOR("DARKGRAY", 80, 80, 80, 255);
    ADDCOLOR("YELLOW", 253, 249, 0, 255);
    ADDCOLOR("GOLD", 255, 203, 0, 255);
    ADDCOLOR("ORANGE", 255, 161, 0, 255);
    ADDCOLOR("PINK", 255, 109, 194, 255);
    ADDCOLOR("RED", 230, 41, 55, 255);
    ADDCOLOR("MAROON", 190, 33, 55, 255);
    ADDCOLOR("GREEN", 0, 228, 48, 255);
    ADDCOLOR("LIME", 0, 158, 47, 255);
    ADDCOLOR("DARKGREEN", 0, 117, 44, 255);
    ADDCOLOR("SKYBLUE", 102, 191, 255, 255);
    ADDCOLOR("BLUE", 0, 121, 241, 255);
    ADDCOLOR("DARKBLUE", 0, 82, 172, 255);
    ADDCOLOR("PURPLE", 200, 122, 255, 255);
    ADDCOLOR("VIOLET", 135, 60, 190, 255);
    ADDCOLOR("DARKPURPLE", 112, 31, 126, 255);
    ADDCOLOR("BEIGE", 211, 176, 131, 255);
    ADDCOLOR("BROWN", 127, 106, 79, 255);
    ADDCOLOR("DARKBROWN", 76, 63, 47, 255);
    ADDCOLOR("WHITE", 255, 255, 255, 255);
    ADDCOLOR("BLACK", 0, 0, 0, 255);
    ADDCOLOR("BLANK", 0, 0, 0, 0);
    ADDCOLOR("MAGENTA", 255, 0, 255, 255);
    ADDCOLOR("RAYWHITE", 245, 245, 245, 255);
#undef ADDCOLOR
}

/* python_raylib_attach: the export python.gcDL calls */
GCL_MODULE_EXPORT PyObject *python_raylib_attach(void) {
    PyObject *m = PyModule_Create(&raylib_module);
    if (m == NULL)
        return NULL;

    PyModule_AddStringConstant(m, "raylib_version", RAYLIB_VERSION);
    PyModule_AddIntConstant(m, "RAYLIB_VERSION_MAJOR", RAYLIB_VERSION_MAJOR);
    PyModule_AddIntConstant(m, "RAYLIB_VERSION_MINOR", RAYLIB_VERSION_MINOR);
    PyModule_AddIntConstant(m, "RAYLIB_VERSION_PATCH", RAYLIB_VERSION_PATCH);

    /* keys (raylib.h KeyboardKey) */
    PyModule_AddIntConstant(m, "KEY_NULL", 0);
    PyModule_AddIntConstant(m, "KEY_SPACE", 32);
    PyModule_AddIntConstant(m, "KEY_APOSTROPHE", 39);
    PyModule_AddIntConstant(m, "KEY_COMMA", 44);
    PyModule_AddIntConstant(m, "KEY_MINUS", 45);
    PyModule_AddIntConstant(m, "KEY_PERIOD", 46);
    PyModule_AddIntConstant(m, "KEY_SLASH", 47);
    PyModule_AddIntConstant(m, "KEY_ZERO", 48);
    PyModule_AddIntConstant(m, "KEY_ONE", 49);
    PyModule_AddIntConstant(m, "KEY_TWO", 50);
    PyModule_AddIntConstant(m, "KEY_THREE", 51);
    PyModule_AddIntConstant(m, "KEY_FOUR", 52);
    PyModule_AddIntConstant(m, "KEY_FIVE", 53);
    PyModule_AddIntConstant(m, "KEY_SIX", 54);
    PyModule_AddIntConstant(m, "KEY_SEVEN", 55);
    PyModule_AddIntConstant(m, "KEY_EIGHT", 56);
    PyModule_AddIntConstant(m, "KEY_NINE", 57);
    PyModule_AddIntConstant(m, "KEY_SEMICOLON", 59);
    PyModule_AddIntConstant(m, "KEY_EQUAL", 61);
    PyModule_AddIntConstant(m, "KEY_A", 65);
    PyModule_AddIntConstant(m, "KEY_B", 66);
    PyModule_AddIntConstant(m, "KEY_C", 67);
    PyModule_AddIntConstant(m, "KEY_D", 68);
    PyModule_AddIntConstant(m, "KEY_E", 69);
    PyModule_AddIntConstant(m, "KEY_F", 70);
    PyModule_AddIntConstant(m, "KEY_G", 71);
    PyModule_AddIntConstant(m, "KEY_H", 72);
    PyModule_AddIntConstant(m, "KEY_I", 73);
    PyModule_AddIntConstant(m, "KEY_J", 74);
    PyModule_AddIntConstant(m, "KEY_K", 75);
    PyModule_AddIntConstant(m, "KEY_L", 76);
    PyModule_AddIntConstant(m, "KEY_M", 77);
    PyModule_AddIntConstant(m, "KEY_N", 78);
    PyModule_AddIntConstant(m, "KEY_O", 79);
    PyModule_AddIntConstant(m, "KEY_P", 80);
    PyModule_AddIntConstant(m, "KEY_Q", 81);
    PyModule_AddIntConstant(m, "KEY_R", 82);
    PyModule_AddIntConstant(m, "KEY_S", 83);
    PyModule_AddIntConstant(m, "KEY_T", 84);
    PyModule_AddIntConstant(m, "KEY_U", 85);
    PyModule_AddIntConstant(m, "KEY_V", 86);
    PyModule_AddIntConstant(m, "KEY_W", 87);
    PyModule_AddIntConstant(m, "KEY_X", 88);
    PyModule_AddIntConstant(m, "KEY_Y", 89);
    PyModule_AddIntConstant(m, "KEY_Z", 90);
    PyModule_AddIntConstant(m, "KEY_LEFT_BRACKET", 91);
    PyModule_AddIntConstant(m, "KEY_BACKSLASH", 92);
    PyModule_AddIntConstant(m, "KEY_RIGHT_BRACKET", 93);
    PyModule_AddIntConstant(m, "KEY_GRAVE", 96);
    PyModule_AddIntConstant(m, "KEY_ESCAPE", 256);
    PyModule_AddIntConstant(m, "KEY_ENTER", 257);
    PyModule_AddIntConstant(m, "KEY_TAB", 258);
    PyModule_AddIntConstant(m, "KEY_BACKSPACE", 259);
    PyModule_AddIntConstant(m, "KEY_INSERT", 260);
    PyModule_AddIntConstant(m, "KEY_DELETE", 261);
    PyModule_AddIntConstant(m, "KEY_RIGHT", 262);
    PyModule_AddIntConstant(m, "KEY_LEFT", 263);
    PyModule_AddIntConstant(m, "KEY_DOWN", 264);
    PyModule_AddIntConstant(m, "KEY_UP", 265);
    PyModule_AddIntConstant(m, "KEY_PAGE_UP", 266);
    PyModule_AddIntConstant(m, "KEY_PAGE_DOWN", 267);
    PyModule_AddIntConstant(m, "KEY_HOME", 268);
    PyModule_AddIntConstant(m, "KEY_END", 269);
    PyModule_AddIntConstant(m, "KEY_CAPS_LOCK", 280);
    PyModule_AddIntConstant(m, "KEY_SCROLL_LOCK", 281);
    PyModule_AddIntConstant(m, "KEY_NUM_LOCK", 282);
    PyModule_AddIntConstant(m, "KEY_PRINT_SCREEN", 283);
    PyModule_AddIntConstant(m, "KEY_PAUSE", 284);
    PyModule_AddIntConstant(m, "KEY_F1", 290);
    PyModule_AddIntConstant(m, "KEY_F2", 291);
    PyModule_AddIntConstant(m, "KEY_F3", 292);
    PyModule_AddIntConstant(m, "KEY_F4", 293);
    PyModule_AddIntConstant(m, "KEY_F5", 294);
    PyModule_AddIntConstant(m, "KEY_F6", 295);
    PyModule_AddIntConstant(m, "KEY_F7", 296);
    PyModule_AddIntConstant(m, "KEY_F8", 297);
    PyModule_AddIntConstant(m, "KEY_F9", 298);
    PyModule_AddIntConstant(m, "KEY_F10", 299);
    PyModule_AddIntConstant(m, "KEY_F11", 300);
    PyModule_AddIntConstant(m, "KEY_F12", 301);
    PyModule_AddIntConstant(m, "KEY_LEFT_SHIFT", 340);
    PyModule_AddIntConstant(m, "KEY_LEFT_CONTROL", 341);
    PyModule_AddIntConstant(m, "KEY_LEFT_ALT", 342);
    PyModule_AddIntConstant(m, "KEY_LEFT_SUPER", 343);
    PyModule_AddIntConstant(m, "KEY_RIGHT_SHIFT", 344);
    PyModule_AddIntConstant(m, "KEY_RIGHT_CONTROL", 345);
    PyModule_AddIntConstant(m, "KEY_RIGHT_ALT", 346);
    PyModule_AddIntConstant(m, "KEY_RIGHT_SUPER", 347);
    PyModule_AddIntConstant(m, "KEY_KB_MENU", 348);
    PyModule_AddIntConstant(m, "KEY_KP_0", 320);
    PyModule_AddIntConstant(m, "KEY_KP_1", 321);
    PyModule_AddIntConstant(m, "KEY_KP_2", 322);
    PyModule_AddIntConstant(m, "KEY_KP_3", 323);
    PyModule_AddIntConstant(m, "KEY_KP_4", 324);
    PyModule_AddIntConstant(m, "KEY_KP_5", 325);
    PyModule_AddIntConstant(m, "KEY_KP_6", 326);
    PyModule_AddIntConstant(m, "KEY_KP_7", 327);
    PyModule_AddIntConstant(m, "KEY_KP_8", 328);
    PyModule_AddIntConstant(m, "KEY_KP_9", 329);
    PyModule_AddIntConstant(m, "KEY_KP_DECIMAL", 330);
    PyModule_AddIntConstant(m, "KEY_KP_DIVIDE", 331);
    PyModule_AddIntConstant(m, "KEY_KP_MULTIPLY", 332);
    PyModule_AddIntConstant(m, "KEY_KP_SUBTRACT", 333);
    PyModule_AddIntConstant(m, "KEY_KP_ADD", 334);
    PyModule_AddIntConstant(m, "KEY_KP_ENTER", 335);
    PyModule_AddIntConstant(m, "KEY_KP_EQUAL", 336);
    PyModule_AddIntConstant(m, "KEY_BACK", 4);
    PyModule_AddIntConstant(m, "KEY_MENU", 5);
    PyModule_AddIntConstant(m, "KEY_VOLUME_UP", 24);
    PyModule_AddIntConstant(m, "KEY_VOLUME_DOWN", 25);

    /* mouse */
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_LEFT", 0);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_RIGHT", 1);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_MIDDLE", 2);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_SIDE", 3);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_EXTRA", 4);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_FORWARD", 5);
    PyModule_AddIntConstant(m, "MOUSE_BUTTON_BACK", 6);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_DEFAULT", 0);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_ARROW", 1);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_IBEAM", 2);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_CROSSHAIR", 3);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_POINTING_HAND", 4);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_RESIZE_EW", 5);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_RESIZE_NS", 6);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_RESIZE_NWSE", 7);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_RESIZE_NESW", 8);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_RESIZE_ALL", 9);
    PyModule_AddIntConstant(m, "MOUSE_CURSOR_NOT_ALLOWED", 10);

    /* window flags */
    PyModule_AddIntConstant(m, "FLAG_VSYNC_HINT", 0x40);
    PyModule_AddIntConstant(m, "FLAG_FULLSCREEN_MODE", 0x2);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_RESIZABLE", 0x4);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_UNDECORATED", 0x8);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_HIDDEN", 0x80);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_MINIMIZED", 0x200);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_MAXIMIZED", 0x400);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_UNFOCUSED", 0x800);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_TOPMOST", 0x1000);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_ALWAYS_RUN", 0x100);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_TRANSPARENT", 0x10);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_HIGHDPI", 0x2000);
    PyModule_AddIntConstant(m, "FLAG_WINDOW_MOUSE_PASSTHROUGH", 0x4000);
    PyModule_AddIntConstant(m, "FLAG_BORDERLESS_WINDOWED_MODE", 0x8000);
    PyModule_AddIntConstant(m, "FLAG_MSAA_4X_HINT", 0x20);
    PyModule_AddIntConstant(m, "FLAG_INTERLACED_HINT", 0x10000);

    /* camera */
    PyModule_AddIntConstant(m, "CAMERA_ORTHOGRAPHIC", 1);
    PyModule_AddIntConstant(m, "CAMERA_PERSPECTIVE", 0);

    /* texture filter/wrap */
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_POINT", 0);
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_BILINEAR", 1);
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_TRILINEAR", 2);
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_ANISOTROPIC_4X", 3);
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_ANISOTROPIC_8X", 4);
    PyModule_AddIntConstant(m, "TEXTURE_FILTER_ANISOTROPIC_16X", 5);
    PyModule_AddIntConstant(m, "TEXTURE_WRAP_REPEAT", 0);
    PyModule_AddIntConstant(m, "TEXTURE_WRAP_CLAMP", 1);
    PyModule_AddIntConstant(m, "TEXTURE_WRAP_MIRROR_REPEAT", 2);
    PyModule_AddIntConstant(m, "TEXTURE_WRAP_MIRROR_CLAMP", 3);

    /* raylib log levels (for SetTraceLogLevel) */
    PyModule_AddIntConstant(m, "LOG_ALL", LOG_ALL);
    PyModule_AddIntConstant(m, "LOG_TRACE", LOG_TRACE);
    PyModule_AddIntConstant(m, "LOG_DEBUG", LOG_DEBUG);
    PyModule_AddIntConstant(m, "LOG_INFO", LOG_INFO);
    PyModule_AddIntConstant(m, "LOG_WARNING", LOG_WARNING);
    PyModule_AddIntConstant(m, "LOG_ERROR", LOG_ERROR);
    PyModule_AddIntConstant(m, "LOG_FATAL", LOG_FATAL);
    PyModule_AddIntConstant(m, "LOG_NONE", LOG_NONE);

    add_colors(m);
    return m;
}
