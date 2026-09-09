/*
 * gcl_pyraylib.c — GCL Embed Python + Raylib binding (FULL).
 *
 * Python tarafında `import raylib` ile kullanılır:
 *   import raylib
 *   raylib.InitWindow(800, 600, "title")
 *   raylib.Rectangle(0, 0, 800, 30)  → (x, y, w, h) tuple
 *   raylib.RAYWHITE / raylib.RED ... → packed 32-bit renk integer
 *
 * Python C extension module: PyInit_raylib (raylib.pyd|.so).
 */

#include <Python.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#define GCL_PY_EXPORT __declspec(dllexport)
#else
#define GCL_PY_EXPORT __attribute__((visibility("default")))
#endif

/* ---------- Renk yardımcıları ---------- */

static unsigned int color_to_uint(Color c) {
    return ((unsigned int)c.r) |
           (((unsigned int)c.g) << 8) |
           (((unsigned int)c.b) << 16) |
           (((unsigned int)c.a) << 24);
}

static Color uint_to_color(unsigned int v) {
    Color c;
    c.r = (unsigned char)(v & 0xFF);
    c.g = (unsigned char)((v >> 8) & 0xFF);
    c.b = (unsigned char)((v >> 16) & 0xFF);
    c.a = (unsigned char)((v >> 24) & 0xFF);
    return c;
}

static int py_get_uint(PyObject *o, unsigned int *out) {
    if (!o) return 0;
    *out = (unsigned int)PyLong_AsUnsignedLong(o);
    return !PyErr_Occurred();
}

/* ---------- Window ---------- */

static PyObject *py_init_window(PyObject *self, PyObject *args) {
    (void)self;
    int w, h;
    const char *title;
    if (!PyArg_ParseTuple(args, "iis", &w, &h, &title)) return NULL;
    InitWindow(w, h, title);
    Py_RETURN_NONE;
}

static PyObject *py_window_should_close(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    if (WindowShouldClose()) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *py_close_window(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    CloseWindow();
    Py_RETURN_NONE;
}

static PyObject *py_set_target_fps(PyObject *self, PyObject *args) {
    (void)self;
    int fps;
    if (!PyArg_ParseTuple(args, "i", &fps)) return NULL;
    SetTargetFPS(fps);
    Py_RETURN_NONE;
}

static PyObject *py_set_window_size(PyObject *self, PyObject *args) {
    (void)self;
    int w, h;
    if (!PyArg_ParseTuple(args, "ii", &w, &h)) return NULL;
    SetWindowSize(w, h);
    Py_RETURN_NONE;
}

static PyObject *py_get_screen_width(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    return PyLong_FromLong(GetScreenWidth());
}

static PyObject *py_get_screen_height(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    return PyLong_FromLong(GetScreenHeight());
}

/* ---------- Drawing ---------- */

static PyObject *py_begin_drawing(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    BeginDrawing();
    Py_RETURN_NONE;
}

static PyObject *py_end_drawing(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    EndDrawing();
    Py_RETURN_NONE;
}

static PyObject *py_clear_background(PyObject *self, PyObject *args) {
    (void)self;
    unsigned int c;
    if (!PyArg_ParseTuple(args, "I", &c)) return NULL;
    ClearBackground(uint_to_color(c));
    Py_RETURN_NONE;
}

static PyObject *py_draw_rectangle(PyObject *self, PyObject *args) {
    (void)self;
    int x, y, w, h;
    unsigned int c;
    if (!PyArg_ParseTuple(args, "iiiiI", &x, &y, &w, &h, &c)) return NULL;
    DrawRectangle(x, y, w, h, uint_to_color(c));
    Py_RETURN_NONE;
}

static PyObject *py_draw_text(PyObject *self, PyObject *args) {
    (void)self;
    const char *text;
    int x, y, size;
    unsigned int c;
    if (!PyArg_ParseTuple(args, "siiiI", &text, &x, &y, &size, &c)) return NULL;
    DrawText(text, x, y, size, uint_to_color(c));
    Py_RETURN_NONE;
}

static PyObject *py_draw_circle(PyObject *self, PyObject *args) {
    (void)self;
    int x, y;
    float r;
    unsigned int c;
    if (!PyArg_ParseTuple(args, "iifI", &x, &y, &r, &c)) return NULL;
    DrawCircle(x, y, r, uint_to_color(c));
    Py_RETURN_NONE;
}

static PyObject *py_draw_line(PyObject *self, PyObject *args) {
    (void)self;
    int x1, y1, x2, y2;
    unsigned int c;
    if (!PyArg_ParseTuple(args, "iiiiI", &x1, &y1, &x2, &y2, &c)) return NULL;
    DrawLine(x1, y1, x2, y2, uint_to_color(c));
    Py_RETURN_NONE;
}

static PyObject *py_draw_fps(PyObject *self, PyObject *args) {
    (void)self;
    int x, y;
    if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;
    DrawFPS(x, y);
    Py_RETURN_NONE;
}

/* ---------- Input ---------- */

static PyObject *py_is_key_pressed(PyObject *self, PyObject *args) {
    (void)self;
    int key;
    if (!PyArg_ParseTuple(args, "i", &key)) return NULL;
    if (IsKeyPressed(key)) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *py_is_key_down(PyObject *self, PyObject *args) {
    (void)self;
    int key;
    if (!PyArg_ParseTuple(args, "i", &key)) return NULL;
    if (IsKeyDown(key)) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *py_get_mouse_x(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    return PyLong_FromLong(GetMouseX());
}

static PyObject *py_get_mouse_y(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    return PyLong_FromLong(GetMouseY());
}

/* ---------- Rectangle / Vector2 ---------- */

static PyObject *py_rectangle(PyObject *self, PyObject *args) {
    (void)self;
    float x, y, w, h;
    if (!PyArg_ParseTuple(args, "ffff", &x, &y, &w, &h)) return NULL;
    return Py_BuildValue("(ffff)", x, y, w, h);
}

static PyObject *py_vector2(PyObject *self, PyObject *args) {
    (void)self;
    float x, y;
    if (!PyArg_ParseTuple(args, "ff", &x, &y)) return NULL;
    return Py_BuildValue("(ff)", x, y);
}

static PyObject *py_vector3(PyObject *self, PyObject *args) {
    (void)self;
    float x, y, z;
    if (!PyArg_ParseTuple(args, "fff", &x, &y, &z)) return NULL;
    return Py_BuildValue("(fff)", x, y, z);
}

/* =========================================================================
   FULL PYTHON RAYLIB WRAPPER
   ========================================================================= */

/* ---- Window / Core ---- */
static PyObject *py_is_window_ready(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowReady())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_fullscreen(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowFullscreen())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_hidden(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowHidden())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_minimized(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowMinimized())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_maximized(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowMaximized())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_focused(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowFocused())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_window_resized(PyObject *self, PyObject *args) { (void)self;(void)args; if(IsWindowResized())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_toggle_fullscreen(PyObject *self, PyObject *args) { (void)self;(void)args;ToggleFullscreen();Py_RETURN_NONE; }
static PyObject *py_toggle_borderless_windowed(PyObject *self, PyObject *args) { (void)self;(void)args;ToggleBorderlessWindowed();Py_RETURN_NONE; }
static PyObject *py_maximize_window(PyObject *self, PyObject *args) { (void)self;(void)args;MaximizeWindow();Py_RETURN_NONE; }
static PyObject *py_minimize_window(PyObject *self, PyObject *args) { (void)self;(void)args;MinimizeWindow();Py_RETURN_NONE; }
static PyObject *py_restore_window(PyObject *self, PyObject *args) { (void)self;(void)args;RestoreWindow();Py_RETURN_NONE; }
static PyObject *py_set_window_title(PyObject *self, PyObject *args) { (void)self; const char *t; if(!PyArg_ParseTuple(args,"s",&t))return NULL; SetWindowTitle(t);Py_RETURN_NONE; }
static PyObject *py_set_window_position(PyObject *self, PyObject *args) { (void)self; int x,y; if(!PyArg_ParseTuple(args,"ii",&x,&y))return NULL; SetWindowPosition(x,y);Py_RETURN_NONE; }
static PyObject *py_set_window_monitor(PyObject *self, PyObject *args) { (void)self; int m; if(!PyArg_ParseTuple(args,"i",&m))return NULL; SetWindowMonitor(m);Py_RETURN_NONE; }
static PyObject *py_set_window_min_size(PyObject *self, PyObject *args) { (void)self; int w,h; if(!PyArg_ParseTuple(args,"ii",&w,&h))return NULL; SetWindowMinSize(w,h);Py_RETURN_NONE; }
static PyObject *py_set_window_max_size(PyObject *self, PyObject *args) { (void)self; int w,h; if(!PyArg_ParseTuple(args,"ii",&w,&h))return NULL; SetWindowMaxSize(w,h);Py_RETURN_NONE; }
static PyObject *py_set_window_opacity(PyObject *self, PyObject *args) { (void)self; float o; if(!PyArg_ParseTuple(args,"f",&o))return NULL; SetWindowOpacity(o);Py_RETURN_NONE; }
static PyObject *py_set_window_focused(PyObject *self, PyObject *args) { (void)self;(void)args;SetWindowFocused();Py_RETURN_NONE; }
static PyObject *py_get_render_width(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetRenderWidth()); }
static PyObject *py_get_render_height(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetRenderHeight()); }
static PyObject *py_get_monitor_count(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetMonitorCount()); }
static PyObject *py_get_current_monitor(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetCurrentMonitor()); }
static PyObject *py_get_monitor_width(PyObject *self, PyObject *args) { (void)self;int m;if(!PyArg_ParseTuple(args,"i",&m))return NULL;return PyLong_FromLong(GetMonitorWidth(m)); }
static PyObject *py_get_monitor_height(PyObject *self, PyObject *args) { (void)self;int m;if(!PyArg_ParseTuple(args,"i",&m))return NULL;return PyLong_FromLong(GetMonitorHeight(m)); }
static PyObject *py_get_monitor_refresh_rate(PyObject *self, PyObject *args) { (void)self;int m;if(!PyArg_ParseTuple(args,"i",&m))return NULL;return PyLong_FromLong(GetMonitorRefreshRate(m)); }
static PyObject *py_get_window_position(PyObject *self, PyObject *args) { (void)self;(void)args;Vector2 v=GetWindowPosition();return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_get_window_scale_dpi(PyObject *self, PyObject *args) { (void)self;(void)args;Vector2 v=GetWindowScaleDPI();return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_set_clipboard_text(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;SetClipboardText(t);Py_RETURN_NONE; }
static PyObject *py_get_clipboard_text(PyObject *self, PyObject *args) { (void)self;(void)args;return PyUnicode_FromString(GetClipboardText()); }
static PyObject *py_enable_event_waiting(PyObject *self, PyObject *args) { (void)self;(void)args;EnableEventWaiting();Py_RETURN_NONE; }
static PyObject *py_disable_event_waiting(PyObject *self, PyObject *args) { (void)self;(void)args;DisableEventWaiting();Py_RETURN_NONE; }
static PyObject *py_set_exit_key(PyObject *self, PyObject *args) { (void)self;int k;if(!PyArg_ParseTuple(args,"i",&k))return NULL;SetExitKey(k);Py_RETURN_NONE; }
static PyObject *py_set_config_flags(PyObject *self, PyObject *args) { (void)self;unsigned int f;if(!PyArg_ParseTuple(args,"I",&f))return NULL;SetConfigFlags(f);Py_RETURN_NONE; }

/* ---- Cursor ---- */
static PyObject *py_show_cursor(PyObject *self, PyObject *args) { (void)self;(void)args;ShowCursor();Py_RETURN_NONE; }
static PyObject *py_hide_cursor(PyObject *self, PyObject *args) { (void)self;(void)args;HideCursor();Py_RETURN_NONE; }
static PyObject *py_is_cursor_hidden(PyObject *self, PyObject *args) { (void)self;(void)args;if(IsCursorHidden())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_enable_cursor(PyObject *self, PyObject *args) { (void)self;(void)args;EnableCursor();Py_RETURN_NONE; }
static PyObject *py_disable_cursor(PyObject *self, PyObject *args) { (void)self;(void)args;DisableCursor();Py_RETURN_NONE; }
static PyObject *py_is_cursor_on_screen(PyObject *self, PyObject *args) { (void)self;(void)args;if(IsCursorOnScreen())Py_RETURN_TRUE;Py_RETURN_FALSE; }

/* ---- Drawing modes ---- */
static float py_dict_f(PyObject *d, const char *k) {
    PyObject *v = d ? PyDict_GetItemString(d, k) : NULL;
    return v ? (float)PyFloat_AsDouble(v) : 0.0f;
}
static int py_dict_i(PyObject *d, const char *k) {
    PyObject *v = d ? PyDict_GetItemString(d, k) : NULL;
    return v ? (int)PyLong_AsLong(v) : 0;
}
static void py_read_vec3_dict(PyObject *d, Vector3 *out) {
    if (!d || !PyDict_Check(d)) return;
    PyObject *vx = PyDict_GetItemString(d, "x"); if (vx) out->x = (float)PyFloat_AsDouble(vx);
    PyObject *vy = PyDict_GetItemString(d, "y"); if (vy) out->y = (float)PyFloat_AsDouble(vy);
    PyObject *vz = PyDict_GetItemString(d, "z"); if (vz) out->z = (float)PyFloat_AsDouble(vz);
}
static PyObject *py_camera2d(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    PyObject *d = PyDict_New();
    PyObject *offset = PyDict_New(); PyDict_SetItemString(offset, "x", PyFloat_FromDouble(0.0)); PyDict_SetItemString(offset, "y", PyFloat_FromDouble(0.0)); PyDict_SetItemString(d, "offset", offset); Py_DECREF(offset);
    PyObject *target = PyDict_New(); PyDict_SetItemString(target, "x", PyFloat_FromDouble(0.0)); PyDict_SetItemString(target, "y", PyFloat_FromDouble(0.0)); PyDict_SetItemString(d, "target", target); Py_DECREF(target);
    PyDict_SetItemString(d, "rotation", PyFloat_FromDouble(0.0));
    PyDict_SetItemString(d, "zoom", PyFloat_FromDouble(1.0));
    return d;
}
static PyObject *py_camera3d(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    PyObject *d = PyDict_New();
    PyObject *pos = PyDict_New(); PyDict_SetItemString(pos, "x", PyFloat_FromDouble(0.0)); PyDict_SetItemString(pos, "y", PyFloat_FromDouble(0.0)); PyDict_SetItemString(pos, "z", PyFloat_FromDouble(0.0)); PyDict_SetItemString(d, "position", pos); Py_DECREF(pos);
    PyObject *tgt = PyDict_New(); PyDict_SetItemString(tgt, "x", PyFloat_FromDouble(0.0)); PyDict_SetItemString(tgt, "y", PyFloat_FromDouble(0.0)); PyDict_SetItemString(tgt, "z", PyFloat_FromDouble(0.0)); PyDict_SetItemString(d, "target", tgt); Py_DECREF(tgt);
    PyObject *up = PyDict_New(); PyDict_SetItemString(up, "x", PyFloat_FromDouble(0.0)); PyDict_SetItemString(up, "y", PyFloat_FromDouble(1.0)); PyDict_SetItemString(up, "z", PyFloat_FromDouble(0.0)); PyDict_SetItemString(d, "up", up); Py_DECREF(up);
    PyDict_SetItemString(d, "fovy", PyFloat_FromDouble(45.0));
    PyDict_SetItemString(d, "projection", PyLong_FromLong(CAMERA_PERSPECTIVE));
    return d;
}
static PyObject *py_begin_mode_2d(PyObject *self, PyObject *args) {
    (void)self;
    Camera2D cam = {0};
    if (PyTuple_GET_SIZE(args) == 1 && PyDict_Check(PyTuple_GET_ITEM(args, 0))) {
        PyObject *d = PyTuple_GET_ITEM(args, 0);
        PyObject *off = PyDict_GetItemString(d, "offset");
        if (off && PyDict_Check(off)) { cam.offset.x = py_dict_f(off, "x"); cam.offset.y = py_dict_f(off, "y"); }
        PyObject *tgt = PyDict_GetItemString(d, "target");
        if (tgt && PyDict_Check(tgt)) { cam.target.x = py_dict_f(tgt, "x"); cam.target.y = py_dict_f(tgt, "y"); }
        cam.rotation = py_dict_f(d, "rotation");
        cam.zoom = py_dict_f(d, "zoom");
    } else {
        float ox,oy,tx,ty,rot,zoom;
        if(!PyArg_ParseTuple(args,"ffffff",&ox,&oy,&tx,&ty,&rot,&zoom))return NULL;
        cam.offset=(Vector2){ox,oy}; cam.target=(Vector2){tx,ty}; cam.rotation=rot; cam.zoom=zoom;
    }
    BeginMode2D(cam); Py_RETURN_NONE;
}
static PyObject *py_end_mode_2d(PyObject *self, PyObject *args) { (void)self;(void)args;EndMode2D();Py_RETURN_NONE; }
/* Son kamera — UpdateCamera için kalıcı state */
static Camera3D g_py_cam = {0};
static PyObject *py_begin_mode_3d(PyObject *self, PyObject *args) {
    (void)self;
    if (PyTuple_GET_SIZE(args) == 1 && PyDict_Check(PyTuple_GET_ITEM(args, 0))) {
        PyObject *d = PyTuple_GET_ITEM(args, 0);
        PyObject *pos = PyDict_GetItemString(d, "position"); if (pos) py_read_vec3_dict(pos, &g_py_cam.position);
        PyObject *tgt = PyDict_GetItemString(d, "target"); if (tgt) py_read_vec3_dict(tgt, &g_py_cam.target);
        PyObject *up = PyDict_GetItemString(d, "up"); if (up) py_read_vec3_dict(up, &g_py_cam.up);
        g_py_cam.fovy = py_dict_f(d, "fovy");
        g_py_cam.projection = py_dict_i(d, "projection");
    } else {
        float px,py,pz,tx,ty,tz,ux,uy,uz,fovy; int proj;
        if(!PyArg_ParseTuple(args,"fffffffffi",&px,&py,&pz,&tx,&ty,&tz,&ux,&uy,&uz,&fovy,&proj))return NULL;
        g_py_cam.position=(Vector3){px,py,pz}; g_py_cam.target=(Vector3){tx,ty,tz}; g_py_cam.up=(Vector3){ux,uy,uz}; g_py_cam.fovy=fovy; g_py_cam.projection=proj;
    }
    BeginMode3D(g_py_cam); Py_RETURN_NONE;
}
static PyObject *py_end_mode_3d(PyObject *self, PyObject *args) { (void)self;(void)args;EndMode3D();Py_RETURN_NONE; }
/* UpdateCamera(camera, CAMERA_FREE) veya UpdateCamera(mode) */
static PyObject *py_update_camera(PyObject *self, PyObject *args) {
    (void)self; int mode;
    if (PyTuple_GET_SIZE(args) >= 2 && PyDict_Check(PyTuple_GET_ITEM(args, 0))) {
        PyObject *d = PyTuple_GET_ITEM(args, 0);
        PyObject *pos = PyDict_GetItemString(d, "position"); if (pos) py_read_vec3_dict(pos, &g_py_cam.position);
        PyObject *tgt = PyDict_GetItemString(d, "target"); if (tgt) py_read_vec3_dict(tgt, &g_py_cam.target);
        PyObject *up = PyDict_GetItemString(d, "up"); if (up) py_read_vec3_dict(up, &g_py_cam.up);
        g_py_cam.fovy = py_dict_f(d, "fovy");
        g_py_cam.projection = py_dict_i(d, "projection");
        mode = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 1));
        UpdateCamera(&g_py_cam, mode);
    } else if (!PyArg_ParseTuple(args, "i", &mode)) {
        return NULL;
    } else {
        UpdateCamera(&g_py_cam, mode);
    }
    Py_RETURN_NONE;
}
static PyObject *py_begin_texture_mode(PyObject *self, PyObject *args) { (void)self; unsigned int id,tid,did; int w,h; if(!PyArg_ParseTuple(args,"IIiII",&id,&tid,&w,&h,&did))return NULL; RenderTexture2D rt; rt.id=id; rt.texture.id=tid; rt.texture.width=w; rt.texture.height=h; rt.depth.id=did; BeginTextureMode(rt);Py_RETURN_NONE; }
static PyObject *py_end_texture_mode(PyObject *self, PyObject *args) { (void)self;(void)args;EndTextureMode();Py_RETURN_NONE; }
static PyObject *py_begin_shader_mode(PyObject *self, PyObject *args) { (void)self; unsigned int id; if(!PyArg_ParseTuple(args,"I",&id))return NULL; Shader s; s.id=id; BeginShaderMode(s);Py_RETURN_NONE; }
static PyObject *py_end_shader_mode(PyObject *self, PyObject *args) { (void)self;(void)args;EndShaderMode();Py_RETURN_NONE; }
static PyObject *py_begin_blend_mode(PyObject *self, PyObject *args) { (void)self;int m;if(!PyArg_ParseTuple(args,"i",&m))return NULL;BeginBlendMode(m);Py_RETURN_NONE; }
static PyObject *py_end_blend_mode(PyObject *self, PyObject *args) { (void)self;(void)args;EndBlendMode();Py_RETURN_NONE; }
static PyObject *py_begin_scissor_mode(PyObject *self, PyObject *args) { (void)self;int x,y,w,h;if(!PyArg_ParseTuple(args,"iiii",&x,&y,&w,&h))return NULL;BeginScissorMode(x,y,w,h);Py_RETURN_NONE; }
static PyObject *py_end_scissor_mode(PyObject *self, PyObject *args) { (void)self;(void)args;EndScissorMode();Py_RETURN_NONE; }

/* ---- Timing ---- */
static PyObject *py_get_frame_time(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetFrameTime()); }
static PyObject *py_get_time(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetTime()); }
static PyObject *py_get_fps(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetFPS()); }
static PyObject *py_wait_time(PyObject *self, PyObject *args) { (void)self;double s;if(!PyArg_ParseTuple(args,"d",&s))return NULL;WaitTime(s);Py_RETURN_NONE; }

/* ---- Shader ---- */
static PyObject *py_load_shader(PyObject *self, PyObject *args) { (void)self;const char*v,*f;if(!PyArg_ParseTuple(args,"ss",&v,&f))return NULL;Shader s=LoadShader(v,f);return PyLong_FromUnsignedLong(s.id); }
static PyObject *py_load_shader_from_memory(PyObject *self, PyObject *args) { (void)self;const char*v,*f;if(!PyArg_ParseTuple(args,"ss",&v,&f))return NULL;Shader s=LoadShaderFromMemory(v,f);return PyLong_FromUnsignedLong(s.id); }
static PyObject *py_is_shader_valid(PyObject *self, PyObject *args) { (void)self;unsigned int id;if(!PyArg_ParseTuple(args,"I",&id))return NULL;Shader s; s.id=id;if(IsShaderValid(s))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_shader_location(PyObject *self, PyObject *args) { (void)self;unsigned int id;const char*n;if(!PyArg_ParseTuple(args,"Is",&id,&n))return NULL;Shader s; s.id=id;return PyLong_FromLong(GetShaderLocation(s,n)); }
static PyObject *py_get_shader_location_attrib(PyObject *self, PyObject *args) { (void)self;unsigned int id;const char*n;if(!PyArg_ParseTuple(args,"Is",&id,&n))return NULL;Shader s;s.id=id;return PyLong_FromLong(GetShaderLocationAttrib(s,n)); }
static PyObject *py_unload_shader(PyObject *self, PyObject *args) { (void)self;unsigned int id;if(!PyArg_ParseTuple(args,"I",&id))return NULL;Shader s;s.id=id;UnloadShader(s);Py_RETURN_NONE; }

/* ---- File system ---- */
static PyObject *py_file_exists(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;if(FileExists(f))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_directory_exists(PyObject *self, PyObject *args) { (void)self;const char*d;if(!PyArg_ParseTuple(args,"s",&d))return NULL;if(DirectoryExists(d))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_load_file_text(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;char*t=LoadFileText(f);if(!t)Py_RETURN_NONE;PyObject*r=PyUnicode_FromString(t);UnloadFileText(t);return r; }
static PyObject *py_save_file_text(PyObject *self, PyObject *args) { (void)self;const char*f,*t;if(!PyArg_ParseTuple(args,"ss",&f,&t))return NULL;if(SaveFileText(f,t))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_file_extension(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;return PyUnicode_FromString(GetFileExtension(f)); }
static PyObject *py_get_file_name(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;return PyUnicode_FromString(GetFileName(f)); }
static PyObject *py_get_file_name_without_ext(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;return PyUnicode_FromString(GetFileNameWithoutExt(f)); }
static PyObject *py_get_directory_path(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;return PyUnicode_FromString(GetDirectoryPath(f)); }
static PyObject *py_get_working_directory(PyObject *self, PyObject *args) { (void)self;(void)args;return PyUnicode_FromString(GetWorkingDirectory()); }
static PyObject *py_get_application_directory(PyObject *self, PyObject *args) { (void)self;(void)args;return PyUnicode_FromString(GetApplicationDirectory()); }
static PyObject *py_make_directory(PyObject *self, PyObject *args) { (void)self;const char*d;if(!PyArg_ParseTuple(args,"s",&d))return NULL;return PyLong_FromLong(MakeDirectory(d)); }

/* ---- Random ---- */
static PyObject *py_set_random_seed(PyObject *self, PyObject *args) { (void)self;unsigned int s;if(!PyArg_ParseTuple(args,"I",&s))return NULL;SetRandomSeed(s);Py_RETURN_NONE; }
static PyObject *py_get_random_value(PyObject *self, PyObject *args) { (void)self;int mn,mx;if(!PyArg_ParseTuple(args,"ii",&mn,&mx))return NULL;return PyLong_FromLong(GetRandomValue(mn,mx)); }

/* ---- Misc ---- */
static PyObject *py_take_screenshot(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;TakeScreenshot(f);Py_RETURN_NONE; }
static PyObject *py_open_url(PyObject *self, PyObject *args) { (void)self;const char*u;if(!PyArg_ParseTuple(args,"s",&u))return NULL;OpenURL(u);Py_RETURN_NONE; }

/* ---- Input: keyboard ---- */
static PyObject *py_is_key_pressed_repeat(PyObject *self, PyObject *args) { (void)self;int k;if(!PyArg_ParseTuple(args,"i",&k))return NULL;if(IsKeyPressedRepeat(k))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_key_released(PyObject *self, PyObject *args) { (void)self;int k;if(!PyArg_ParseTuple(args,"i",&k))return NULL;if(IsKeyReleased(k))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_key_up(PyObject *self, PyObject *args) { (void)self;int k;if(!PyArg_ParseTuple(args,"i",&k))return NULL;if(IsKeyUp(k))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_key_pressed(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetKeyPressed()); }
static PyObject *py_get_char_pressed(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetCharPressed()); }
static PyObject *py_get_key_name(PyObject *self, PyObject *args) { (void)self;int k;if(!PyArg_ParseTuple(args,"i",&k))return NULL;return PyUnicode_FromString(GetKeyName(k)); }

/* ---- Input: gamepad ---- */
static PyObject *py_is_gamepad_available(PyObject *self, PyObject *args) { (void)self;int g;if(!PyArg_ParseTuple(args,"i",&g))return NULL;if(IsGamepadAvailable(g))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_gamepad_name(PyObject *self, PyObject *args) { (void)self;int g;if(!PyArg_ParseTuple(args,"i",&g))return NULL;return PyUnicode_FromString(GetGamepadName(g)); }
static PyObject *py_is_gamepad_button_pressed(PyObject *self, PyObject *args) { (void)self;int g,b;if(!PyArg_ParseTuple(args,"ii",&g,&b))return NULL;if(IsGamepadButtonPressed(g,b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_gamepad_button_down(PyObject *self, PyObject *args) { (void)self;int g,b;if(!PyArg_ParseTuple(args,"ii",&g,&b))return NULL;if(IsGamepadButtonDown(g,b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_gamepad_button_released(PyObject *self, PyObject *args) { (void)self;int g,b;if(!PyArg_ParseTuple(args,"ii",&g,&b))return NULL;if(IsGamepadButtonReleased(g,b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_gamepad_button_up(PyObject *self, PyObject *args) { (void)self;int g,b;if(!PyArg_ParseTuple(args,"ii",&g,&b))return NULL;if(IsGamepadButtonUp(g,b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_gamepad_button_pressed(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetGamepadButtonPressed()); }
static PyObject *py_get_gamepad_axis_count(PyObject *self, PyObject *args) { (void)self;int g;if(!PyArg_ParseTuple(args,"i",&g))return NULL;return PyLong_FromLong(GetGamepadAxisCount(g)); }
static PyObject *py_get_gamepad_axis_movement(PyObject *self, PyObject *args) { (void)self;int g,a;if(!PyArg_ParseTuple(args,"ii",&g,&a))return NULL;return PyFloat_FromDouble(GetGamepadAxisMovement(g,a)); }
static PyObject *py_set_gamepad_mappings(PyObject *self, PyObject *args) { (void)self;const char*m;if(!PyArg_ParseTuple(args,"s",&m))return NULL;return PyLong_FromLong(SetGamepadMappings(m)); }
static PyObject *py_set_gamepad_vibration(PyObject *self, PyObject *args) { (void)self;int g;float lm,rm,d;if(!PyArg_ParseTuple(args,"ifff",&g,&lm,&rm,&d))return NULL;SetGamepadVibration(g,lm,rm,d);Py_RETURN_NONE; }

/* ---- Input: mouse ---- */
static PyObject *py_is_mouse_button_pressed(PyObject *self, PyObject *args) { (void)self;int b;if(!PyArg_ParseTuple(args,"i",&b))return NULL;if(IsMouseButtonPressed(b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_mouse_button_down(PyObject *self, PyObject *args) { (void)self;int b;if(!PyArg_ParseTuple(args,"i",&b))return NULL;if(IsMouseButtonDown(b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_mouse_button_released(PyObject *self, PyObject *args) { (void)self;int b;if(!PyArg_ParseTuple(args,"i",&b))return NULL;if(IsMouseButtonReleased(b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_is_mouse_button_up(PyObject *self, PyObject *args) { (void)self;int b;if(!PyArg_ParseTuple(args,"i",&b))return NULL;if(IsMouseButtonUp(b))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_mouse_position(PyObject *self, PyObject *args) { (void)self;(void)args;Vector2 v=GetMousePosition();return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_get_mouse_delta(PyObject *self, PyObject *args) { (void)self;(void)args;Vector2 v=GetMouseDelta();return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_set_mouse_position(PyObject *self, PyObject *args) { (void)self;int x,y;if(!PyArg_ParseTuple(args,"ii",&x,&y))return NULL;SetMousePosition(x,y);Py_RETURN_NONE; }
static PyObject *py_set_mouse_offset(PyObject *self, PyObject *args) { (void)self;int x,y;if(!PyArg_ParseTuple(args,"ii",&x,&y))return NULL;SetMouseOffset(x,y);Py_RETURN_NONE; }
static PyObject *py_set_mouse_scale(PyObject *self, PyObject *args) { (void)self;float x,y;if(!PyArg_ParseTuple(args,"ff",&x,&y))return NULL;SetMouseScale(x,y);Py_RETURN_NONE; }
static PyObject *py_get_mouse_wheel_move(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetMouseWheelMove()); }
static PyObject *py_set_mouse_cursor(PyObject *self, PyObject *args) { (void)self;int c;if(!PyArg_ParseTuple(args,"i",&c))return NULL;SetMouseCursor(c);Py_RETURN_NONE; }

/* ---- Input: touch ---- */
static PyObject *py_get_touch_x(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetTouchX()); }
static PyObject *py_get_touch_y(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetTouchY()); }
static PyObject *py_get_touch_position(PyObject *self, PyObject *args) { (void)self;int idx;if(!PyArg_ParseTuple(args,"i",&idx))return NULL;Vector2 v=GetTouchPosition(idx);return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_get_touch_point_id(PyObject *self, PyObject *args) { (void)self;int idx;if(!PyArg_ParseTuple(args,"i",&idx))return NULL;return PyLong_FromLong(GetTouchPointId(idx)); }
static PyObject *py_get_touch_point_count(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetTouchPointCount()); }

/* ---- Gestures ---- */
static PyObject *py_set_gestures_enabled(PyObject *self, PyObject *args) { (void)self;unsigned int g;if(!PyArg_ParseTuple(args,"I",&g))return NULL;SetGesturesEnabled(g);Py_RETURN_NONE; }
static PyObject *py_is_gesture_detected(PyObject *self, PyObject *args) { (void)self;unsigned int g;if(!PyArg_ParseTuple(args,"I",&g))return NULL;if(IsGestureDetected(g))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_get_gesture_detected(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GetGestureDetected()); }
static PyObject *py_get_gesture_hold_duration(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetGestureHoldDuration()); }
static PyObject *py_get_gesture_drag_vector(PyObject *self, PyObject *args) { (void)self;(void)args;Vector2 v=GetGestureDragVector();return Py_BuildValue("(ff)",v.x,v.y); }
static PyObject *py_get_gesture_drag_angle(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetGestureDragAngle()); }

/* ---- Shapes ---- */
static PyObject *py_draw_pixel(PyObject *self, PyObject *args) { (void)self;int x,y;unsigned int c;if(!PyArg_ParseTuple(args,"iiI",&x,&y,&c))return NULL;DrawPixel(x,y,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_line_ex(PyObject *self, PyObject *args) { (void)self;float x1,y1,x2,y2,t;unsigned int c;if(!PyArg_ParseTuple(args,"fffffI",&x1,&y1,&x2,&y2,&t,&c))return NULL;DrawLineEx((Vector2){x1,y1},(Vector2){x2,y2},t,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_triangle(PyObject *self, PyObject *args) { (void)self;float x1,y1,x2,y2,x3,y3;unsigned int c;if(!PyArg_ParseTuple(args,"ffffffI",&x1,&y1,&x2,&y2,&x3,&y3,&c))return NULL;DrawTriangle((Vector2){x1,y1},(Vector2){x2,y2},(Vector2){x3,y3},uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_rectangle_v(PyObject *self, PyObject *args) { (void)self;float x,y,w,h;unsigned int c;if(!PyArg_ParseTuple(args,"ffffI",&x,&y,&w,&h,&c))return NULL;DrawRectangleV((Vector2){x,y},(Vector2){w,h},uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_rectangle_rec(PyObject *self, PyObject *args) { (void)self;float x,y,w,h;unsigned int c;if(!PyArg_ParseTuple(args,"ffffI",&x,&y,&w,&h,&c))return NULL;DrawRectangleRec((Rectangle){x,y,w,h},uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_rectangle_lines(PyObject *self, PyObject *args) { (void)self;int x,y,w,h;unsigned int c;if(!PyArg_ParseTuple(args,"iiiiI",&x,&y,&w,&h,&c))return NULL;DrawRectangleLines(x,y,w,h,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_rectangle_lines_ex(PyObject *self, PyObject *args) { (void)self;float x,y,w,h,t;unsigned int c;if(!PyArg_ParseTuple(args,"fffffI",&x,&y,&w,&h,&t,&c))return NULL;DrawRectangleLinesEx((Rectangle){x,y,w,h},t,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_rectangle_rounded(PyObject *self, PyObject *args) { (void)self;float x,y,w,h,r;int seg;unsigned int c;if(!PyArg_ParseTuple(args,"fffffiI",&x,&y,&w,&h,&r,&seg,&c))return NULL;DrawRectangleRounded((Rectangle){x,y,w,h},r,seg,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_circle_v(PyObject *self, PyObject *args) { (void)self;float x,y,r;unsigned int c;if(!PyArg_ParseTuple(args,"fffI",&x,&y,&r,&c))return NULL;DrawCircleV((Vector2){x,y},r,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_circle_gradient(PyObject *self, PyObject *args) { (void)self;float x,y,r;unsigned int c1,c2;if(!PyArg_ParseTuple(args,"fffII",&x,&y,&r,&c1,&c2))return NULL;DrawCircleGradient((Vector2){x,y},r,uint_to_color(c1),uint_to_color(c2));Py_RETURN_NONE; }
static PyObject *py_draw_circle_lines(PyObject *self, PyObject *args) { (void)self;int x,y;float r;unsigned int c;if(!PyArg_ParseTuple(args,"iifI",&x,&y,&r,&c))return NULL;DrawCircleLines(x,y,r,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_ellipse(PyObject *self, PyObject *args) { (void)self;int x,y;float rh,rv;unsigned int c;if(!PyArg_ParseTuple(args,"iiffI",&x,&y,&rh,&rv,&c))return NULL;DrawEllipse(x,y,rh,rv,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_poly(PyObject *self, PyObject *args) { (void)self;float x,y,rad,rot;int sides;unsigned int c;if(!PyArg_ParseTuple(args,"ffifiI",&x,&y,&sides,&rad,&rot,&c))return NULL;DrawPoly((Vector2){x,y},sides,rad,rot,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_poly_lines(PyObject *self, PyObject *args) { (void)self;float x,y,rad,rot;int sides;unsigned int c;if(!PyArg_ParseTuple(args,"ffifiI",&x,&y,&sides,&rad,&rot,&c))return NULL;DrawPolyLines((Vector2){x,y},sides,rad,rot,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_grid(PyObject *self, PyObject *args) { (void)self;int slices;float spacing;if(!PyArg_ParseTuple(args,"if",&slices,&spacing))return NULL;DrawGrid(slices,spacing);Py_RETURN_NONE; }

/* ---- Textures / Images ---- */
static PyObject *py_load_texture(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;Texture2D t=LoadTexture(f);return PyLong_FromUnsignedLong(t.id); }
static PyObject *py_unload_texture(PyObject *self, PyObject *args) { (void)self;unsigned int id;if(!PyArg_ParseTuple(args,"I",&id))return NULL;Texture2D t;t.id=id;UnloadTexture(t);Py_RETURN_NONE; }
static PyObject *py_is_texture_valid(PyObject *self, PyObject *args) { (void)self;unsigned int id;if(!PyArg_ParseTuple(args,"I",&id))return NULL;Texture2D t;t.id=id;if(IsTextureValid(t))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_set_texture_filter(PyObject *self, PyObject *args) { (void)self;unsigned int id;int f;if(!PyArg_ParseTuple(args,"Ii",&id,&f))return NULL;Texture2D t;t.id=id;SetTextureFilter(t,f);Py_RETURN_NONE; }
static PyObject *py_set_texture_wrap(PyObject *self, PyObject *args) { (void)self;unsigned int id;int w;if(!PyArg_ParseTuple(args,"Ii",&id,&w))return NULL;Texture2D t;t.id=id;SetTextureWrap(t,w);Py_RETURN_NONE; }
static PyObject *py_draw_texture(PyObject *self, PyObject *args) { (void)self;unsigned int id;int x,y;unsigned int c;if(!PyArg_ParseTuple(args,"IiiI",&id,&x,&y,&c))return NULL;Texture2D t;t.id=id;DrawTexture(t,x,y,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_texture_ex(PyObject *self, PyObject *args) { (void)self;unsigned int id;float x,y,rot,scale;unsigned int c;if(!PyArg_ParseTuple(args,"IffffI",&id,&x,&y,&rot,&scale,&c))return NULL;Texture2D t;t.id=id;DrawTextureEx(t,(Vector2){x,y},rot,scale,uint_to_color(c));Py_RETURN_NONE; }

/* ---- Color helpers ---- */
static PyObject *py_fade(PyObject *self, PyObject *args) { (void)self;unsigned int c;float a;if(!PyArg_ParseTuple(args,"If",&c,&a))return NULL;return PyLong_FromUnsignedLong(color_to_uint(Fade(uint_to_color(c),a))); }
static PyObject *py_color_to_int(PyObject *self, PyObject *args) { (void)self;unsigned int c;if(!PyArg_ParseTuple(args,"I",&c))return NULL;return PyLong_FromLong(ColorToInt(uint_to_color(c))); }
static PyObject *py_color_from_hsv(PyObject *self, PyObject *args) { (void)self;float h,s,v;if(!PyArg_ParseTuple(args,"fff",&h,&s,&v))return NULL;return PyLong_FromUnsignedLong(color_to_uint(ColorFromHSV(h,s,v))); }
static PyObject *py_color_alpha(PyObject *self, PyObject *args) { (void)self;unsigned int c;float a;if(!PyArg_ParseTuple(args,"If",&c,&a))return NULL;return PyLong_FromUnsignedLong(color_to_uint(ColorAlpha(uint_to_color(c),a))); }
static PyObject *py_color_tint(PyObject *self, PyObject *args) { (void)self;unsigned int a,b;if(!PyArg_ParseTuple(args,"II",&a,&b))return NULL;return PyLong_FromUnsignedLong(color_to_uint(ColorTint(uint_to_color(a),uint_to_color(b)))); }
static PyObject *py_color_brightness(PyObject *self, PyObject *args) { (void)self;unsigned int c;float f;if(!PyArg_ParseTuple(args,"If",&c,&f))return NULL;return PyLong_FromUnsignedLong(color_to_uint(ColorBrightness(uint_to_color(c),f))); }
static PyObject *py_color_contrast(PyObject *self, PyObject *args) { (void)self;unsigned int c;float f;if(!PyArg_ParseTuple(args,"If",&c,&f))return NULL;return PyLong_FromUnsignedLong(color_to_uint(ColorContrast(uint_to_color(c),f))); }
static PyObject *py_get_color(PyObject *self, PyObject *args) { (void)self;unsigned int v;if(!PyArg_ParseTuple(args,"I",&v))return NULL;return PyLong_FromUnsignedLong(color_to_uint(GetColor(v))); }

/* ---- Text / Font ---- */
static PyObject *py_get_font_default(PyObject *self, PyObject *args) { (void)self;(void)args;Font f=GetFontDefault();return Py_BuildValue("iI",f.baseSize,f.texture.id); }
static PyObject *py_load_font(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;Font fo=LoadFont(f);return Py_BuildValue("iI",fo.baseSize,fo.texture.id); }
static PyObject *py_load_font_ex(PyObject *self, PyObject *args) { (void)self;const char*f;int sz;if(!PyArg_ParseTuple(args,"si",&f,&sz))return NULL;Font fo=LoadFontEx(f,sz,NULL,0);return Py_BuildValue("iI",fo.baseSize,fo.texture.id); }
static PyObject *py_unload_font(PyObject *self, PyObject *args) { (void)self;int bs;unsigned int id;if(!PyArg_ParseTuple(args,"iI",&bs,&id))return NULL;Font f;f.baseSize=bs;f.texture.id=id;UnloadFont(f);Py_RETURN_NONE; }
static PyObject *py_draw_text_ex(PyObject *self, PyObject *args) { (void)self;int bs;unsigned int id;const char*t;float x,y,sz,sp;unsigned int c;if(!PyArg_ParseTuple(args,"iIsfffI",&bs,&id,&t,&x,&y,&sz,&sp,&c))return NULL;Font f;f.baseSize=bs;f.texture.id=id;DrawTextEx(f,t,(Vector2){x,y},sz,sp,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_measure_text(PyObject *self, PyObject *args) { (void)self;const char*t;int sz;if(!PyArg_ParseTuple(args,"si",&t,&sz))return NULL;return PyLong_FromLong(MeasureText(t,sz)); }
static PyObject *py_text_to_upper(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;char*r=TextToUpper(t);PyObject*out=PyUnicode_FromString(r);return out; }
static PyObject *py_text_to_lower(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;char*r=TextToLower(t);PyObject*out=PyUnicode_FromString(r);return out; }
static PyObject *py_text_to_integer(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;return PyLong_FromLong(TextToInteger(t)); }
static PyObject *py_text_to_float(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;return PyFloat_FromDouble(TextToFloat(t)); }
static PyObject *py_text_length(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;return PyLong_FromUnsignedLong(TextLength(t)); }

/* ---- 3D ---- */
static PyObject *py_draw_cube(PyObject *self, PyObject *args) {
    (void)self; Vector3 pos = {0}; float w,h,l; unsigned int c;
    if (PyTuple_GET_SIZE(args) >= 7) {
        pos.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,0)); pos.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,1)); pos.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));
        w=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3)); h=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,4)); l=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,5));
        c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,6));
    } else if (PyTuple_GET_SIZE(args) >= 5) {
        PyObject *vec = PyTuple_GET_ITEM(args,0);
        if (vec && PyTuple_Check(vec) && PyTuple_GET_SIZE(vec) >= 3) {
            pos.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,0)); pos.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,1)); pos.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,2));
        } else if (vec && PyDict_Check(vec)) { py_read_vec3_dict(vec, &pos); }
        w=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,1)); h=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2)); l=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));
        c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,4));
    } else return NULL;
    if (PyErr_Occurred()) return NULL;
    DrawCube(pos,w,h,l,uint_to_color(c)); Py_RETURN_NONE;
}
static PyObject *py_draw_cube_wires(PyObject *self, PyObject *args) {
    (void)self; Vector3 pos = {0}; float w,h,l; unsigned int c;
    if (PyTuple_GET_SIZE(args) >= 7) {
        pos.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,0)); pos.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,1)); pos.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));
        w=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3)); h=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,4)); l=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,5));
        c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,6));
    } else if (PyTuple_GET_SIZE(args) >= 5) {
        PyObject *vec = PyTuple_GET_ITEM(args,0);
        if (vec && PyTuple_Check(vec) && PyTuple_GET_SIZE(vec) >= 3) {
            pos.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,0)); pos.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,1)); pos.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(vec,2));
        } else if (vec && PyDict_Check(vec)) { py_read_vec3_dict(vec, &pos); }
        w=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,1)); h=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2)); l=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));
        c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,4));
    } else return NULL;
    if (PyErr_Occurred()) return NULL;
    DrawCubeWires(pos,w,h,l,uint_to_color(c)); Py_RETURN_NONE;
}
static PyObject *py_draw_sphere(PyObject *self, PyObject *args) { (void)self;float x,y,z,r;unsigned int c;if(!PyArg_ParseTuple(args,"ffffI",&x,&y,&z,&r,&c))return NULL;DrawSphere((Vector3){x,y,z},r,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_cylinder(PyObject *self, PyObject *args) { (void)self;float x,y,z,rt,rb,h;int sides;unsigned int c;if(!PyArg_ParseTuple(args,"fffffiI",&x,&y,&z,&rt,&rb,&h,&sides,&c))return NULL;DrawCylinder((Vector3){x,y,z},rt,rb,h,sides,uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_plane(PyObject *self, PyObject *args) { (void)self;float x,y,z,w,h;unsigned int c;if(!PyArg_ParseTuple(args,"fffffI",&x,&y,&z,&w,&h,&c))return NULL;DrawPlane((Vector3){x,y,z},(Vector2){w,h},uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_line_3d(PyObject *self, PyObject *args) { (void)self;float x1,y1,z1,x2,y2,z2;unsigned int c;if(!PyArg_ParseTuple(args,"ffffffI",&x1,&y1,&z1,&x2,&y2,&z2,&c))return NULL;DrawLine3D((Vector3){x1,y1,z1},(Vector3){x2,y2,z2},uint_to_color(c));Py_RETURN_NONE; }
static PyObject *py_draw_point_3d(PyObject *self, PyObject *args) { (void)self;float x,y,z;unsigned int c;if(!PyArg_ParseTuple(args,"fffI",&x,&y,&z,&c))return NULL;DrawPoint3D((Vector3){x,y,z},uint_to_color(c));Py_RETURN_NONE; }

/* ---- Audio ---- */
static PyObject *py_init_audio_device(PyObject *self, PyObject *args) { (void)self;(void)args;InitAudioDevice();Py_RETURN_NONE; }
static PyObject *py_close_audio_device(PyObject *self, PyObject *args) { (void)self;(void)args;CloseAudioDevice();Py_RETURN_NONE; }
static PyObject *py_is_audio_device_ready(PyObject *self, PyObject *args) { (void)self;(void)args;if(IsAudioDeviceReady())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_set_master_volume(PyObject *self, PyObject *args) { (void)self;float v;if(!PyArg_ParseTuple(args,"f",&v))return NULL;SetMasterVolume(v);Py_RETURN_NONE; }
static PyObject *py_get_master_volume(PyObject *self, PyObject *args) { (void)self;(void)args;return PyFloat_FromDouble(GetMasterVolume()); }
static PyObject *py_load_sound(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;Sound s=LoadSound(f);return PyLong_FromUnsignedLong(s.stream.sampleRate); }
static PyObject *py_unload_sound(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;UnloadSound(s);Py_RETURN_NONE; }
static PyObject *py_play_sound(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;PlaySound(s);Py_RETURN_NONE; }
static PyObject *py_stop_sound(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;StopSound(s);Py_RETURN_NONE; }
static PyObject *py_pause_sound(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;PauseSound(s);Py_RETURN_NONE; }
static PyObject *py_resume_sound(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;ResumeSound(s);Py_RETURN_NONE; }
static PyObject *py_is_sound_playing(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Sound s;s.stream.sampleRate=rate;if(IsSoundPlaying(s))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_load_music_stream(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;Music m=LoadMusicStream(f);return PyLong_FromUnsignedLong(m.stream.sampleRate); }
static PyObject *py_play_music_stream(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Music m;m.stream.sampleRate=rate;PlayMusicStream(m);Py_RETURN_NONE; }
static PyObject *py_update_music_stream(PyObject *self, PyObject *args) { (void)self;unsigned int rate;if(!PyArg_ParseTuple(args,"I",&rate))return NULL;Music m;m.stream.sampleRate=rate;UpdateMusicStream(m);Py_RETURN_NONE; }

/* ---------- Metot tablosu ---------- */

static PyMethodDef raylib_methods[] = {
    {"InitWindow", py_init_window, METH_VARARGS, "Initialize window"},
    {"WindowShouldClose", py_window_should_close, METH_NOARGS, "Check window close"},
    {"CloseWindow", py_close_window, METH_NOARGS, "Close window"},
    {"SetTargetFPS", py_set_target_fps, METH_VARARGS, "Set target FPS"},
    {"SetWindowSize", py_set_window_size, METH_VARARGS, "Set window size"},
    {"GetScreenWidth", py_get_screen_width, METH_NOARGS, "Get screen width"},
    {"GetScreenHeight", py_get_screen_height, METH_NOARGS, "Get screen height"},
    {"BeginDrawing", py_begin_drawing, METH_NOARGS, "Begin drawing"},
    {"EndDrawing", py_end_drawing, METH_NOARGS, "End drawing"},
    {"ClearBackground", py_clear_background, METH_VARARGS, "Clear background"},
    {"DrawRectangle", py_draw_rectangle, METH_VARARGS, "Draw rectangle"},
    {"DrawText", py_draw_text, METH_VARARGS, "Draw text"},
    {"DrawCircle", py_draw_circle, METH_VARARGS, "Draw circle"},
    {"DrawLine", py_draw_line, METH_VARARGS, "Draw line"},
    {"DrawFPS", py_draw_fps, METH_VARARGS, "Draw FPS"},
    {"IsKeyPressed", py_is_key_pressed, METH_VARARGS, "Check key pressed"},
    {"IsKeyDown", py_is_key_down, METH_VARARGS, "Check key down"},
    {"GetMouseX", py_get_mouse_x, METH_NOARGS, "Get mouse X"},
    {"GetMouseY", py_get_mouse_y, METH_NOARGS, "Get mouse Y"},
    {"Rectangle", py_rectangle, METH_VARARGS, "Create Rectangle tuple"},
    {"Vector2", py_vector2, METH_VARARGS, "Create Vector2 tuple"},
    {"Vector3", py_vector3, METH_VARARGS, "Create Vector3 tuple"},
    {"Camera2D", py_camera2d, METH_NOARGS, "Create Camera2D dict"},
    {"Camera3D", py_camera3d, METH_NOARGS, "Create Camera3D dict"},
    {"IsWindowReady", py_is_window_ready, METH_NOARGS, NULL},
    {"IsWindowFullscreen", py_is_window_fullscreen, METH_NOARGS, NULL},
    {"IsWindowHidden", py_is_window_hidden, METH_NOARGS, NULL},
    {"IsWindowMinimized", py_is_window_minimized, METH_NOARGS, NULL},
    {"IsWindowMaximized", py_is_window_maximized, METH_NOARGS, NULL},
    {"IsWindowFocused", py_is_window_focused, METH_NOARGS, NULL},
    {"IsWindowResized", py_is_window_resized, METH_NOARGS, NULL},
    {"ToggleFullscreen", py_toggle_fullscreen, METH_NOARGS, NULL},
    {"ToggleBorderlessWindowed", py_toggle_borderless_windowed, METH_NOARGS, NULL},
    {"MaximizeWindow", py_maximize_window, METH_NOARGS, NULL},
    {"MinimizeWindow", py_minimize_window, METH_NOARGS, NULL},
    {"RestoreWindow", py_restore_window, METH_NOARGS, NULL},
    {"SetWindowTitle", py_set_window_title, METH_VARARGS, NULL},
    {"SetWindowPosition", py_set_window_position, METH_VARARGS, NULL},
    {"SetWindowMonitor", py_set_window_monitor, METH_VARARGS, NULL},
    {"SetWindowMinSize", py_set_window_min_size, METH_VARARGS, NULL},
    {"SetWindowMaxSize", py_set_window_max_size, METH_VARARGS, NULL},
    {"SetWindowOpacity", py_set_window_opacity, METH_VARARGS, NULL},
    {"SetWindowFocused", py_set_window_focused, METH_NOARGS, NULL},
    {"GetRenderWidth", py_get_render_width, METH_NOARGS, NULL},
    {"GetRenderHeight", py_get_render_height, METH_NOARGS, NULL},
    {"GetMonitorCount", py_get_monitor_count, METH_NOARGS, NULL},
    {"GetCurrentMonitor", py_get_current_monitor, METH_NOARGS, NULL},
    {"GetMonitorWidth", py_get_monitor_width, METH_VARARGS, NULL},
    {"GetMonitorHeight", py_get_monitor_height, METH_VARARGS, NULL},
    {"GetMonitorRefreshRate", py_get_monitor_refresh_rate, METH_VARARGS, NULL},
    {"GetWindowPosition", py_get_window_position, METH_NOARGS, NULL},
    {"GetWindowScaleDPI", py_get_window_scale_dpi, METH_NOARGS, NULL},
    {"SetClipboardText", py_set_clipboard_text, METH_VARARGS, NULL},
    {"GetClipboardText", py_get_clipboard_text, METH_NOARGS, NULL},
    {"EnableEventWaiting", py_enable_event_waiting, METH_NOARGS, NULL},
    {"DisableEventWaiting", py_disable_event_waiting, METH_NOARGS, NULL},
    {"SetExitKey", py_set_exit_key, METH_VARARGS, NULL},
    {"SetConfigFlags", py_set_config_flags, METH_VARARGS, NULL},
    {"ShowCursor", py_show_cursor, METH_NOARGS, NULL},
    {"HideCursor", py_hide_cursor, METH_NOARGS, NULL},
    {"IsCursorHidden", py_is_cursor_hidden, METH_NOARGS, NULL},
    {"EnableCursor", py_enable_cursor, METH_NOARGS, NULL},
    {"DisableCursor", py_disable_cursor, METH_NOARGS, NULL},
    {"IsCursorOnScreen", py_is_cursor_on_screen, METH_NOARGS, NULL},
    {"BeginMode2D", py_begin_mode_2d, METH_VARARGS, NULL},
    {"EndMode2D", py_end_mode_2d, METH_NOARGS, NULL},
    {"BeginMode3D", py_begin_mode_3d, METH_VARARGS, NULL},
    {"EndMode3D", py_end_mode_3d, METH_NOARGS, NULL},
    {"UpdateCamera", py_update_camera, METH_VARARGS, NULL},
    {"BeginTextureMode", py_begin_texture_mode, METH_VARARGS, NULL},
    {"EndTextureMode", py_end_texture_mode, METH_NOARGS, NULL},
    {"BeginShaderMode", py_begin_shader_mode, METH_VARARGS, NULL},
    {"EndShaderMode", py_end_shader_mode, METH_NOARGS, NULL},
    {"BeginBlendMode", py_begin_blend_mode, METH_VARARGS, NULL},
    {"EndBlendMode", py_end_blend_mode, METH_NOARGS, NULL},
    {"BeginScissorMode", py_begin_scissor_mode, METH_VARARGS, NULL},
    {"EndScissorMode", py_end_scissor_mode, METH_NOARGS, NULL},
    {"GetFrameTime", py_get_frame_time, METH_NOARGS, NULL},
    {"GetTime", py_get_time, METH_NOARGS, NULL},
    {"GetFPS", py_get_fps, METH_NOARGS, NULL},
    {"WaitTime", py_wait_time, METH_VARARGS, NULL},
    {"LoadShader", py_load_shader, METH_VARARGS, NULL},
    {"LoadShaderFromMemory", py_load_shader_from_memory, METH_VARARGS, NULL},
    {"IsShaderValid", py_is_shader_valid, METH_VARARGS, NULL},
    {"GetShaderLocation", py_get_shader_location, METH_VARARGS, NULL},
    {"GetShaderLocationAttrib", py_get_shader_location_attrib, METH_VARARGS, NULL},
    {"UnloadShader", py_unload_shader, METH_VARARGS, NULL},
    {"FileExists", py_file_exists, METH_VARARGS, NULL},
    {"DirectoryExists", py_directory_exists, METH_VARARGS, NULL},
    {"LoadFileText", py_load_file_text, METH_VARARGS, NULL},
    {"SaveFileText", py_save_file_text, METH_VARARGS, NULL},
    {"GetFileExtension", py_get_file_extension, METH_VARARGS, NULL},
    {"GetFileName", py_get_file_name, METH_VARARGS, NULL},
    {"GetFileNameWithoutExt", py_get_file_name_without_ext, METH_VARARGS, NULL},
    {"GetDirectoryPath", py_get_directory_path, METH_VARARGS, NULL},
    {"GetWorkingDirectory", py_get_working_directory, METH_NOARGS, NULL},
    {"GetApplicationDirectory", py_get_application_directory, METH_NOARGS, NULL},
    {"MakeDirectory", py_make_directory, METH_VARARGS, NULL},
    {"SetRandomSeed", py_set_random_seed, METH_VARARGS, NULL},
    {"GetRandomValue", py_get_random_value, METH_VARARGS, NULL},
    {"TakeScreenshot", py_take_screenshot, METH_VARARGS, NULL},
    {"OpenURL", py_open_url, METH_VARARGS, NULL},
    {"IsKeyPressedRepeat", py_is_key_pressed_repeat, METH_VARARGS, NULL},
    {"IsKeyReleased", py_is_key_released, METH_VARARGS, NULL},
    {"IsKeyUp", py_is_key_up, METH_VARARGS, NULL},
    {"GetKeyPressed", py_get_key_pressed, METH_NOARGS, NULL},
    {"GetCharPressed", py_get_char_pressed, METH_NOARGS, NULL},
    {"GetKeyName", py_get_key_name, METH_VARARGS, NULL},
    {"IsGamepadAvailable", py_is_gamepad_available, METH_VARARGS, NULL},
    {"GetGamepadName", py_get_gamepad_name, METH_VARARGS, NULL},
    {"IsGamepadButtonPressed", py_is_gamepad_button_pressed, METH_VARARGS, NULL},
    {"IsGamepadButtonDown", py_is_gamepad_button_down, METH_VARARGS, NULL},
    {"IsGamepadButtonReleased", py_is_gamepad_button_released, METH_VARARGS, NULL},
    {"IsGamepadButtonUp", py_is_gamepad_button_up, METH_VARARGS, NULL},
    {"GetGamepadButtonPressed", py_get_gamepad_button_pressed, METH_NOARGS, NULL},
    {"GetGamepadAxisCount", py_get_gamepad_axis_count, METH_VARARGS, NULL},
    {"GetGamepadAxisMovement", py_get_gamepad_axis_movement, METH_VARARGS, NULL},
    {"SetGamepadMappings", py_set_gamepad_mappings, METH_VARARGS, NULL},
    {"SetGamepadVibration", py_set_gamepad_vibration, METH_VARARGS, NULL},
    {"IsMouseButtonPressed", py_is_mouse_button_pressed, METH_VARARGS, NULL},
    {"IsMouseButtonDown", py_is_mouse_button_down, METH_VARARGS, NULL},
    {"IsMouseButtonReleased", py_is_mouse_button_released, METH_VARARGS, NULL},
    {"IsMouseButtonUp", py_is_mouse_button_up, METH_VARARGS, NULL},
    {"GetMousePosition", py_get_mouse_position, METH_NOARGS, NULL},
    {"GetMouseDelta", py_get_mouse_delta, METH_NOARGS, NULL},
    {"SetMousePosition", py_set_mouse_position, METH_VARARGS, NULL},
    {"SetMouseOffset", py_set_mouse_offset, METH_VARARGS, NULL},
    {"SetMouseScale", py_set_mouse_scale, METH_VARARGS, NULL},
    {"GetMouseWheelMove", py_get_mouse_wheel_move, METH_NOARGS, NULL},
    {"SetMouseCursor", py_set_mouse_cursor, METH_VARARGS, NULL},
    {"GetTouchX", py_get_touch_x, METH_NOARGS, NULL},
    {"GetTouchY", py_get_touch_y, METH_NOARGS, NULL},
    {"GetTouchPosition", py_get_touch_position, METH_VARARGS, NULL},
    {"GetTouchPointId", py_get_touch_point_id, METH_VARARGS, NULL},
    {"GetTouchPointCount", py_get_touch_point_count, METH_NOARGS, NULL},
    {"SetGesturesEnabled", py_set_gestures_enabled, METH_VARARGS, NULL},
    {"IsGestureDetected", py_is_gesture_detected, METH_VARARGS, NULL},
    {"GetGestureDetected", py_get_gesture_detected, METH_NOARGS, NULL},
    {"GetGestureHoldDuration", py_get_gesture_hold_duration, METH_NOARGS, NULL},
    {"GetGestureDragVector", py_get_gesture_drag_vector, METH_NOARGS, NULL},
    {"GetGestureDragAngle", py_get_gesture_drag_angle, METH_NOARGS, NULL},
    {"DrawPixel", py_draw_pixel, METH_VARARGS, NULL},
    {"DrawLineEx", py_draw_line_ex, METH_VARARGS, NULL},
    {"DrawTriangle", py_draw_triangle, METH_VARARGS, NULL},
    {"DrawRectangleV", py_draw_rectangle_v, METH_VARARGS, NULL},
    {"DrawRectangleRec", py_draw_rectangle_rec, METH_VARARGS, NULL},
    {"DrawRectangleLines", py_draw_rectangle_lines, METH_VARARGS, NULL},
    {"DrawRectangleLinesEx", py_draw_rectangle_lines_ex, METH_VARARGS, NULL},
    {"DrawRectangleRounded", py_draw_rectangle_rounded, METH_VARARGS, NULL},
    {"DrawCircleV", py_draw_circle_v, METH_VARARGS, NULL},
    {"DrawCircleGradient", py_draw_circle_gradient, METH_VARARGS, NULL},
    {"DrawCircleLines", py_draw_circle_lines, METH_VARARGS, NULL},
    {"DrawEllipse", py_draw_ellipse, METH_VARARGS, NULL},
    {"DrawPoly", py_draw_poly, METH_VARARGS, NULL},
    {"DrawPolyLines", py_draw_poly_lines, METH_VARARGS, NULL},
    {"DrawGrid", py_draw_grid, METH_VARARGS, NULL},
    {"LoadTexture", py_load_texture, METH_VARARGS, NULL},
    {"UnloadTexture", py_unload_texture, METH_VARARGS, NULL},
    {"IsTextureValid", py_is_texture_valid, METH_VARARGS, NULL},
    {"SetTextureFilter", py_set_texture_filter, METH_VARARGS, NULL},
    {"SetTextureWrap", py_set_texture_wrap, METH_VARARGS, NULL},
    {"DrawTexture", py_draw_texture, METH_VARARGS, NULL},
    {"DrawTextureEx", py_draw_texture_ex, METH_VARARGS, NULL},
    {"Fade", py_fade, METH_VARARGS, NULL},
    {"ColorToInt", py_color_to_int, METH_VARARGS, NULL},
    {"ColorFromHSV", py_color_from_hsv, METH_VARARGS, NULL},
    {"ColorAlpha", py_color_alpha, METH_VARARGS, NULL},
    {"ColorTint", py_color_tint, METH_VARARGS, NULL},
    {"ColorBrightness", py_color_brightness, METH_VARARGS, NULL},
    {"ColorContrast", py_color_contrast, METH_VARARGS, NULL},
    {"GetColor", py_get_color, METH_VARARGS, NULL},
    {"GetFontDefault", py_get_font_default, METH_NOARGS, NULL},
    {"LoadFont", py_load_font, METH_VARARGS, NULL},
    {"LoadFontEx", py_load_font_ex, METH_VARARGS, NULL},
    {"UnloadFont", py_unload_font, METH_VARARGS, NULL},
    {"DrawTextEx", py_draw_text_ex, METH_VARARGS, NULL},
    {"MeasureText", py_measure_text, METH_VARARGS, NULL},
    {"TextToUpper", py_text_to_upper, METH_VARARGS, NULL},
    {"TextToLower", py_text_to_lower, METH_VARARGS, NULL},
    {"TextToInteger", py_text_to_integer, METH_VARARGS, NULL},
    {"TextToFloat", py_text_to_float, METH_VARARGS, NULL},
    {"TextLength", py_text_length, METH_VARARGS, NULL},
    {"DrawCube", py_draw_cube, METH_VARARGS, NULL},
    {"DrawCubeWires", py_draw_cube_wires, METH_VARARGS, NULL},
    {"DrawSphere", py_draw_sphere, METH_VARARGS, NULL},
    {"DrawCylinder", py_draw_cylinder, METH_VARARGS, NULL},
    {"DrawPlane", py_draw_plane, METH_VARARGS, NULL},
    {"DrawLine3D", py_draw_line_3d, METH_VARARGS, NULL},
    {"DrawPoint3D", py_draw_point_3d, METH_VARARGS, NULL},
    {"InitAudioDevice", py_init_audio_device, METH_NOARGS, NULL},
    {"CloseAudioDevice", py_close_audio_device, METH_NOARGS, NULL},
    {"IsAudioDeviceReady", py_is_audio_device_ready, METH_NOARGS, NULL},
    {"SetMasterVolume", py_set_master_volume, METH_VARARGS, NULL},
    {"GetMasterVolume", py_get_master_volume, METH_NOARGS, NULL},
    {"LoadSound", py_load_sound, METH_VARARGS, NULL},
    {"UnloadSound", py_unload_sound, METH_VARARGS, NULL},
    {"PlaySound", py_play_sound, METH_VARARGS, NULL},
    {"StopSound", py_stop_sound, METH_VARARGS, NULL},
    {"PauseSound", py_pause_sound, METH_VARARGS, NULL},
    {"ResumeSound", py_resume_sound, METH_VARARGS, NULL},
    {"IsSoundPlaying", py_is_sound_playing, METH_VARARGS, NULL},
    {"LoadMusicStream", py_load_music_stream, METH_VARARGS, NULL},
    {"PlayMusicStream", py_play_music_stream, METH_VARARGS, NULL},
    {"UpdateMusicStream", py_update_music_stream, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef raylib_module = {
    PyModuleDef_HEAD_INIT,
    "raylib",
    "GCL Raylib binding",
    -1,
    raylib_methods
};

/* Renk sabitlerini modüle ekle */
static void add_color_constants(PyObject *mod) {
    PyModule_AddObject(mod, "RAYWHITE", PyLong_FromUnsignedLong(color_to_uint(RAYWHITE)));
    PyModule_AddObject(mod, "LIGHTGRAY", PyLong_FromUnsignedLong(color_to_uint(LIGHTGRAY)));
    PyModule_AddObject(mod, "GRAY", PyLong_FromUnsignedLong(color_to_uint(GRAY)));
    PyModule_AddObject(mod, "DARKGRAY", PyLong_FromUnsignedLong(color_to_uint(DARKGRAY)));
    PyModule_AddObject(mod, "YELLOW", PyLong_FromUnsignedLong(color_to_uint(YELLOW)));
    PyModule_AddObject(mod, "GOLD", PyLong_FromUnsignedLong(color_to_uint(GOLD)));
    PyModule_AddObject(mod, "ORANGE", PyLong_FromUnsignedLong(color_to_uint(ORANGE)));
    PyModule_AddObject(mod, "PINK", PyLong_FromUnsignedLong(color_to_uint(PINK)));
    PyModule_AddObject(mod, "RED", PyLong_FromUnsignedLong(color_to_uint(RED)));
    PyModule_AddObject(mod, "MAROON", PyLong_FromUnsignedLong(color_to_uint(MAROON)));
    PyModule_AddObject(mod, "GREEN", PyLong_FromUnsignedLong(color_to_uint(GREEN)));
    PyModule_AddObject(mod, "LIME", PyLong_FromUnsignedLong(color_to_uint(LIME)));
    PyModule_AddObject(mod, "DARKGREEN", PyLong_FromUnsignedLong(color_to_uint(DARKGREEN)));
    PyModule_AddObject(mod, "SKYBLUE", PyLong_FromUnsignedLong(color_to_uint(SKYBLUE)));
    PyModule_AddObject(mod, "BLUE", PyLong_FromUnsignedLong(color_to_uint(BLUE)));
    PyModule_AddObject(mod, "DARKBLUE", PyLong_FromUnsignedLong(color_to_uint(DARKBLUE)));
    PyModule_AddObject(mod, "PURPLE", PyLong_FromUnsignedLong(color_to_uint(PURPLE)));
    PyModule_AddObject(mod, "VIOLET", PyLong_FromUnsignedLong(color_to_uint(VIOLET)));
    PyModule_AddObject(mod, "DARKPURPLE", PyLong_FromUnsignedLong(color_to_uint(DARKPURPLE)));
    PyModule_AddObject(mod, "BEIGE", PyLong_FromUnsignedLong(color_to_uint(BEIGE)));
    PyModule_AddObject(mod, "BROWN", PyLong_FromUnsignedLong(color_to_uint(BROWN)));
    PyModule_AddObject(mod, "DARKBROWN", PyLong_FromUnsignedLong(color_to_uint(DARKBROWN)));
    PyModule_AddObject(mod, "WHITE", PyLong_FromUnsignedLong(color_to_uint(WHITE)));
    PyModule_AddObject(mod, "BLACK", PyLong_FromUnsignedLong(color_to_uint(BLACK)));
    PyModule_AddObject(mod, "BLANK", PyLong_FromUnsignedLong(color_to_uint(BLANK)));
    PyModule_AddObject(mod, "MAGENTA", PyLong_FromUnsignedLong(color_to_uint(MAGENTA)));
    /* Enum sabitleri: CAMERA_*, KEY_*, MOUSE_* */
    PyModule_AddIntConstant(mod, "CAMERA_FREE", CAMERA_FREE);
    PyModule_AddIntConstant(mod, "CAMERA_ORBITAL", CAMERA_ORBITAL);
    PyModule_AddIntConstant(mod, "CAMERA_FIRST_PERSON", CAMERA_FIRST_PERSON);
    PyModule_AddIntConstant(mod, "CAMERA_THIRD_PERSON", CAMERA_THIRD_PERSON);
    PyModule_AddIntConstant(mod, "CAMERA_PERSPECTIVE", CAMERA_PERSPECTIVE);
    PyModule_AddIntConstant(mod, "CAMERA_ORTHOGRAPHIC", CAMERA_ORTHOGRAPHIC);
    PyModule_AddIntConstant(mod, "KEY_Z", KEY_Z);
    PyModule_AddIntConstant(mod, "KEY_X", KEY_X);
    PyModule_AddIntConstant(mod, "KEY_C", KEY_C);
    PyModule_AddIntConstant(mod, "KEY_V", KEY_V);
    PyModule_AddIntConstant(mod, "KEY_ESCAPE", KEY_ESCAPE);
    PyModule_AddIntConstant(mod, "KEY_SPACE", KEY_SPACE);
    PyModule_AddIntConstant(mod, "KEY_TAB", KEY_TAB);
    PyModule_AddIntConstant(mod, "KEY_LEFT", KEY_LEFT);
    PyModule_AddIntConstant(mod, "KEY_RIGHT", KEY_RIGHT);
    PyModule_AddIntConstant(mod, "KEY_UP", KEY_UP);
    PyModule_AddIntConstant(mod, "KEY_DOWN", KEY_DOWN);
    PyModule_AddIntConstant(mod, "KEY_W", KEY_W);
    PyModule_AddIntConstant(mod, "KEY_A", KEY_A);
    PyModule_AddIntConstant(mod, "KEY_S", KEY_S);
    PyModule_AddIntConstant(mod, "KEY_D", KEY_D);
    PyModule_AddIntConstant(mod, "MOUSE_BUTTON_LEFT", MOUSE_BUTTON_LEFT);
    PyModule_AddIntConstant(mod, "MOUSE_BUTTON_RIGHT", MOUSE_BUTTON_RIGHT);
    PyModule_AddIntConstant(mod, "MOUSE_BUTTON_MIDDLE", MOUSE_BUTTON_MIDDLE);
}

GCL_PY_EXPORT PyObject *PyInit_raylib(void) {
    PyObject *mod = PyModule_Create(&raylib_module);
    if (!mod) return NULL;
    add_color_constants(mod);
    return mod;
}
