/*
 * gcl_pyraygui.c — GCL Embed Python + Raygui binding (FULL).
 *
 * Python tarafında `import raygui` ile kullanılır:
 *   import raygui
 *   raygui.GuiStatusBar((0, 0, 800, 30), "text")
 *
 * Rectangle, gcl_pyraylib.c'de tuple olarak döner; burada aynı tuple alınır.
 * Color, packed 32-bit unsigned int (R|G<<8|B<<16|A<<24).
 *
 * Python C extension module: PyInit_raygui (raygui.pyd|.so).
 */

#include <Python.h>
#include <raylib.h>
#include <raygui.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#ifdef _WIN32
#define GCL_PY_EXPORT __declspec(dllexport)
#else
#define GCL_PY_EXPORT __attribute__((visibility("default")))
#endif

/* ---------- Yardımcılar ---------- */

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

static int parse_rect(PyObject *rect, Rectangle *rec) {
    if (!rect) return 0;
    PyObject *tuple = NULL;
    if (PyTuple_Check(rect)) {
        if (PyTuple_GET_SIZE(rect) < 4) return 0;
        tuple = rect;
    } else {
        tuple = PySequence_Tuple(rect);
        if (!tuple) return 0;
    }
    rec->x = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(tuple, 0));
    rec->y = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(tuple, 1));
    rec->width = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(tuple, 2));
    rec->height = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(tuple, 3));
    if (tuple != rect) Py_DECREF(tuple);
    return 1;
}

static int parse_args(PyObject *args, Rectangle *rec, const char **text) {
    if (PyTuple_GET_SIZE(args) < 2) return 0;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), rec)) return 0;
    *text = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    return *text ? 1 : 0;
}

/* ---------- Temel (mevcut) ---------- */

static PyObject *py_gui_status_bar(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    if (!parse_args(args, &rec, &text)) return NULL;
    GuiStatusBar(rec, text);
    Py_RETURN_NONE;
}

static PyObject *py_gui_label(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    if (!parse_args(args, &rec, &text)) return NULL;
    GuiLabel(rec, text);
    Py_RETURN_NONE;
}

static PyObject *py_gui_button(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    if (!parse_args(args, &rec, &text)) return NULL;
    if (GuiButton(rec, text)) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *py_gui_check_box(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    bool checked;
    if (!parse_args(args, &rec, &text)) return NULL;
    if (PyTuple_GET_SIZE(args) < 3) return NULL;
    checked = PyObject_IsTrue(PyTuple_GET_ITEM(args, 2));
    GuiCheckBox(rec, text, &checked);
    if (checked) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *py_gui_slider(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *tleft, *tright;
    float value, minv, maxv;
    if (PyTuple_GET_SIZE(args) < 6) return NULL;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), &rec)) return NULL;
    tleft = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    tright = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 2));
    if (!tleft || !tright) return NULL;
    value = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 3));
    minv = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 4));
    maxv = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 5));
    value = GuiSlider(rec, tleft, tright, &value, minv, maxv);
    return PyFloat_FromDouble((double)value);
}

static PyObject *py_gui_progress_bar(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *tleft, *tright;
    float value, minv, maxv;
    if (PyTuple_GET_SIZE(args) < 6) return NULL;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), &rec)) return NULL;
    tleft = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    tright = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 2));
    if (!tleft || !tright) return NULL;
    value = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 3));
    minv = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 4));
    maxv = (float)PyFloat_AsDouble(PyTuple_GET_ITEM(args, 5));
    value = GuiProgressBar(rec, tleft, tright, &value, minv, maxv);
    return PyFloat_FromDouble((double)value);
}

static PyObject *py_gui_spinner(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    int value, minv, maxv, edit;
    if (PyTuple_GET_SIZE(args) < 6) return NULL;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), &rec)) return NULL;
    text = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    if (!text) return NULL;
    value = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 2));
    minv = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 3));
    maxv = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 4));
    edit = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 5));
    value = GuiSpinner(rec, text, &value, minv, maxv, edit);
    return PyLong_FromLong(value);
}

static PyObject *py_gui_combo_box(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    int active;
    if (PyTuple_GET_SIZE(args) < 3) return NULL;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), &rec)) return NULL;
    text = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    if (!text) return NULL;
    active = (int)PyLong_AsLong(PyTuple_GET_ITEM(args, 2));
    active = GuiComboBox(rec, text, &active);
    return PyLong_FromLong(active);
}

static PyObject *py_gui_panel(PyObject *self, PyObject *args) {
    (void)self;
    Rectangle rec;
    const char *text;
    if (PyTuple_GET_SIZE(args) < 1) return NULL;
    if (!parse_rect(PyTuple_GET_ITEM(args, 0), &rec)) return NULL;
    if (PyTuple_GET_SIZE(args) >= 2 && PyTuple_GET_ITEM(args, 1) != Py_None) {
        text = PyUnicode_AsUTF8(PyTuple_GET_ITEM(args, 1));
    } else {
        text = "";
    }
    GuiPanel(rec, text);
    Py_RETURN_NONE;
}

static PyObject *py_gui_set_style(PyObject *self, PyObject *args) {
    (void)self;
    int control, property, value;
    if (!PyArg_ParseTuple(args, "iii", &control, &property, &value)) return NULL;
    GuiSetStyle(control, property, value);
    Py_RETURN_NONE;
}

static PyObject *py_gui_get_style(PyObject *self, PyObject *args) {
    (void)self;
    int control, property;
    if (!PyArg_ParseTuple(args, "ii", &control, &property)) return NULL;
    return PyLong_FromLong(GuiGetStyle(control, property));
}

/* =========================================================================
   FULL RAYGUI PYTHON WRAPPER
   ========================================================================= */

/* ---- State ---- */
static PyObject *py_gui_enable(PyObject *self, PyObject *args) { (void)self;(void)args;GuiEnable();Py_RETURN_NONE; }
static PyObject *py_gui_disable(PyObject *self, PyObject *args) { (void)self;(void)args;GuiDisable();Py_RETURN_NONE; }
static PyObject *py_gui_lock(PyObject *self, PyObject *args) { (void)self;(void)args;GuiLock();Py_RETURN_NONE; }
static PyObject *py_gui_unlock(PyObject *self, PyObject *args) { (void)self;(void)args;GuiUnlock();Py_RETURN_NONE; }
static PyObject *py_gui_is_locked(PyObject *self, PyObject *args) { (void)self;(void)args;if(GuiIsLocked())Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_gui_set_alpha(PyObject *self, PyObject *args) { (void)self;float a;if(!PyArg_ParseTuple(args,"f",&a))return NULL;GuiSetAlpha(a);Py_RETURN_NONE; }
static PyObject *py_gui_set_state(PyObject *self, PyObject *args) { (void)self;int s;if(!PyArg_ParseTuple(args,"i",&s))return NULL;GuiSetState(s);Py_RETURN_NONE; }
static PyObject *py_gui_get_state(PyObject *self, PyObject *args) { (void)self;(void)args;return PyLong_FromLong(GuiGetState()); }
static PyObject *py_gui_set_font(PyObject *self, PyObject *args) { (void)self;int bs;unsigned int id;if(!PyArg_ParseTuple(args,"iI",&bs,&id))return NULL;Font f;f.baseSize=bs;f.texture.id=id;GuiSetFont(f);Py_RETURN_NONE; }
static PyObject *py_gui_get_font(PyObject *self, PyObject *args) { (void)self;(void)args;Font f=GuiGetFont();return Py_BuildValue("iI",f.baseSize,f.texture.id); }
static PyObject *py_gui_load_style(PyObject *self, PyObject *args) { (void)self;const char*f;if(!PyArg_ParseTuple(args,"s",&f))return NULL;GuiLoadStyle(f);Py_RETURN_NONE; }
static PyObject *py_gui_load_style_default(PyObject *self, PyObject *args) { (void)self;(void)args;GuiLoadStyleDefault();Py_RETURN_NONE; }
static PyObject *py_gui_enable_tooltip(PyObject *self, PyObject *args) { (void)self;(void)args;GuiEnableTooltip();Py_RETURN_NONE; }
static PyObject *py_gui_disable_tooltip(PyObject *self, PyObject *args) { (void)self;(void)args;GuiDisableTooltip();Py_RETURN_NONE; }
static PyObject *py_gui_set_tooltip(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;GuiSetTooltip(t);Py_RETURN_NONE; }
static PyObject *py_gui_get_text_width(PyObject *self, PyObject *args) { (void)self;const char*t;if(!PyArg_ParseTuple(args,"s",&t))return NULL;return PyLong_FromLong(GuiGetTextWidth(t)); }
static PyObject *py_gui_icon_text(PyObject *self, PyObject *args) { (void)self;int id;const char*t;if(!PyArg_ParseTuple(args,"is",&id,&t))return NULL;return PyUnicode_FromString(GuiIconText(id,t)); }
static PyObject *py_gui_set_icon_scale(PyObject *self, PyObject *args) { (void)self;int s;if(!PyArg_ParseTuple(args,"i",&s))return NULL;GuiSetIconScale(s);Py_RETURN_NONE; }
static PyObject *py_gui_draw_icon(PyObject *self, PyObject *args) { (void)self;int id,x,y,ps;unsigned int c;if(!PyArg_ParseTuple(args,"iiiiI",&id,&x,&y,&ps,&c))return NULL;GuiDrawIcon(id,x,y,ps,uint_to_color(c));Py_RETURN_NONE; }

/* ---- Container ---- */
static PyObject *py_gui_window_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;if(PyTuple_GET_SIZE(args)<2)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));return PyLong_FromLong(GuiWindowBox(r,t)); }
static PyObject *py_gui_group_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";if(PyTuple_GET_SIZE(args)<1)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));return PyLong_FromLong(GuiGroupBox(r,t)); }
static PyObject *py_gui_line(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";if(PyTuple_GET_SIZE(args)<1)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));return PyLong_FromLong(GuiLine(r,t)); }
static PyObject *py_gui_scroll_panel(PyObject *self, PyObject *args) {
    (void)self;Rectangle r,content,view;Vector2 scroll;const char*t="";
    if(PyTuple_GET_SIZE(args)<1)return NULL;
    if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;
    if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));
    content=(Rectangle){r.x,r.y,r.width,r.height};
    if(PyTuple_GET_SIZE(args)>=3&&PyTuple_GET_ITEM(args,2)!=Py_None)parse_rect(PyTuple_GET_ITEM(args,2),&content);
    scroll=(Vector2){0,0};view=(Rectangle){0,0,0,0};
    int rv=GuiScrollPanel(r,t,content,&scroll,&view);
    return Py_BuildValue("i(ff)(ffff)",rv,scroll.x,scroll.y,view.x,view.y,view.width,view.height);
}

/* ---- Basic ---- */
static PyObject *py_gui_label_button(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;if(PyTuple_GET_SIZE(args)<2)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));if(GuiLabelButton(r,t))Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_gui_toggle(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;bool active=false;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=PyObject_IsTrue(PyTuple_GET_ITEM(args,2));GuiToggle(r,t,&active);if(active)Py_RETURN_TRUE;Py_RETURN_FALSE; }
static PyObject *py_gui_toggle_group(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int active;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));GuiToggleGroup(r,t,&active);return PyLong_FromLong(active); }
static PyObject *py_gui_toggle_slider(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int active;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));GuiToggleSlider(r,t,&active);return PyLong_FromLong(active); }
static PyObject *py_gui_dropdown_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int active;bool edit;if(PyTuple_GET_SIZE(args)<4)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));edit=PyObject_IsTrue(PyTuple_GET_ITEM(args,3));GuiDropdownBox(r,t,&active,edit);return PyLong_FromLong(active); }
static PyObject *py_gui_value_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";int value,minv,maxv;bool edit;if(PyTuple_GET_SIZE(args)<6)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));value=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));minv=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,3));maxv=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,4));edit=PyObject_IsTrue(PyTuple_GET_ITEM(args,5));GuiValueBox(r,t,&value,minv,maxv,edit);return PyLong_FromLong(value); }
static PyObject *py_gui_value_box_float(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="",*textValue;float value;bool edit;char textBuf[64]={0};if(PyTuple_GET_SIZE(args)<7)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));textValue=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,2));snprintf(textBuf,sizeof(textBuf),"%s",textValue?textValue:"");value=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));edit=PyObject_IsTrue(PyTuple_GET_ITEM(args,4));GuiValueBoxFloat(r,t,textBuf,&value,edit);return Py_BuildValue("(sf)",textBuf,value); }
static PyObject *py_gui_text_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int sz;bool edit;char textBuf[4096]={0};if(PyTuple_GET_SIZE(args)<4)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));snprintf(textBuf,sizeof(textBuf),"%s",t?t:"");sz=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));edit=PyObject_IsTrue(PyTuple_GET_ITEM(args,3));GuiTextBox(r,textBuf,sz,edit);return PyUnicode_FromString(textBuf); }
static PyObject *py_gui_slider_bar(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*tl,*tr;float value,minv,maxv;if(PyTuple_GET_SIZE(args)<6)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;tl=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));tr=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,2));value=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));minv=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,4));maxv=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,5));GuiSliderBar(r,tl,tr,&value,minv,maxv);return PyFloat_FromDouble(value); }
static PyObject *py_gui_dummy_rec(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";if(PyTuple_GET_SIZE(args)<1)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));return PyLong_FromLong(GuiDummyRec(r,t)); }
static PyObject *py_gui_grid(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";float spacing;int subdivs;Vector2 cell;int rv;if(PyTuple_GET_SIZE(args)<4)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));spacing=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));subdivs=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,3));rv=GuiGrid(r,t,spacing,subdivs,&cell);return Py_BuildValue("i(ff)",rv,cell.x,cell.y); }

/* ---- Advanced ---- */
static PyObject *py_gui_list_view(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int scroll=0,active;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));GuiListView(r,t,&scroll,&active);return PyLong_FromLong(active); }
static PyObject *py_gui_list_view_ex(PyObject *self, PyObject *args) { (void)self;Rectangle r;int count;int scroll=0,active=0,focus=0;if(PyTuple_GET_SIZE(args)<2)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;count=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,1));GuiListViewEx(r,NULL,count,&scroll,&active,&focus);return Py_BuildValue("(iii)",active,focus,scroll); }
static PyObject *py_gui_tab_bar(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t;int hscroll=0,active;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));active=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,2));GuiTabBar(r,t,&hscroll,&active);return PyLong_FromLong(active); }
static PyObject *py_gui_tab_bar_ex(PyObject *self, PyObject *args) { (void)self;Rectangle r;int count;int hscroll=0,active=0,focus=0;if(PyTuple_GET_SIZE(args)<2)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;count=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,1));GuiTabBarEx(r,NULL,count,&hscroll,&active,&focus);return Py_BuildValue("(iii)",active,focus,hscroll); }
static PyObject *py_gui_message_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*title,*msg,*btnText;int btn=0,rv;if(PyTuple_GET_SIZE(args)<4)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;title=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));msg=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,2));btnText=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,3));rv=GuiMessageBox(r,title,msg,btnText,&btn);return Py_BuildValue("(ii)",rv,btn); }
static PyObject *py_gui_text_input_box(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*title,*msg,*btnText;char textBuf[4096]={0};int textSize,rv,btn=0;bool secret=false;if(PyTuple_GET_SIZE(args)<6)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;title=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));msg=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,2));btnText=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,3));snprintf(textBuf,sizeof(textBuf),"%s",PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,4)));textSize=(int)PyLong_AsLong(PyTuple_GET_ITEM(args,5));rv=GuiTextInputBox(r,title,msg,textBuf,textSize,btnText,&btn,&secret);return Py_BuildValue("(isb)",rv,textBuf,secret); }
static PyObject *py_gui_color_picker(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";unsigned int c;Color col;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,2));col=uint_to_color(c);GuiColorPicker(r,t,&col);return PyLong_FromUnsignedLong(color_to_uint(col)); }
static PyObject *py_gui_color_panel(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";unsigned int c;Color col;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));c=(unsigned int)PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args,2));col=uint_to_color(c);GuiColorPanel(r,t,&col);return PyLong_FromUnsignedLong(color_to_uint(col)); }
static PyObject *py_gui_color_bar_alpha(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";float a;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));a=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));GuiColorBarAlpha(r,t,&a);return PyFloat_FromDouble(a); }
static PyObject *py_gui_color_bar_hue(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";float v;if(PyTuple_GET_SIZE(args)<3)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));v=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));GuiColorBarHue(r,t,&v);return PyFloat_FromDouble(v); }
static PyObject *py_gui_color_picker_hsv(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";Vector3 c;if(PyTuple_GET_SIZE(args)<5)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));c.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));c.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));c.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,4));GuiColorPickerHSV(r,t,&c);return Py_BuildValue("(fff)",c.x,c.y,c.z); }
static PyObject *py_gui_color_panel_hsv(PyObject *self, PyObject *args) { (void)self;Rectangle r;const char*t="";Vector3 c;if(PyTuple_GET_SIZE(args)<5)return NULL;if(!parse_rect(PyTuple_GET_ITEM(args,0),&r))return NULL;if(PyTuple_GET_SIZE(args)>=2&&PyTuple_GET_ITEM(args,1)!=Py_None)t=PyUnicode_AsUTF8(PyTuple_GET_ITEM(args,1));c.x=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));c.y=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,3));c.z=(float)PyFloat_AsDouble(PyTuple_GET_ITEM(args,4));GuiColorPanelHSV(r,t,&c);return Py_BuildValue("(fff)",c.x,c.y,c.z); }

/* ---------- Metot tablosu ---------- */

static PyMethodDef raygui_methods[] = {
    {"GuiStatusBar", py_gui_status_bar, METH_VARARGS, "Status bar"},
    {"GuiLabel", py_gui_label, METH_VARARGS, "Label"},
    {"GuiButton", py_gui_button, METH_VARARGS, "Button"},
    {"GuiCheckBox", py_gui_check_box, METH_VARARGS, "Check box"},
    {"GuiSlider", py_gui_slider, METH_VARARGS, "Slider"},
    {"GuiProgressBar", py_gui_progress_bar, METH_VARARGS, "Progress bar"},
    {"GuiSpinner", py_gui_spinner, METH_VARARGS, "Spinner"},
    {"GuiComboBox", py_gui_combo_box, METH_VARARGS, "Combo box"},
    {"GuiPanel", py_gui_panel, METH_VARARGS, "Panel"},
    {"GuiSetStyle", py_gui_set_style, METH_VARARGS, "Set style"},
    {"GuiGetStyle", py_gui_get_style, METH_VARARGS, "Get style"},
    {"GuiEnable", py_gui_enable, METH_NOARGS, NULL},
    {"GuiDisable", py_gui_disable, METH_NOARGS, NULL},
    {"GuiLock", py_gui_lock, METH_NOARGS, NULL},
    {"GuiUnlock", py_gui_unlock, METH_NOARGS, NULL},
    {"GuiIsLocked", py_gui_is_locked, METH_NOARGS, NULL},
    {"GuiSetAlpha", py_gui_set_alpha, METH_VARARGS, NULL},
    {"GuiSetState", py_gui_set_state, METH_VARARGS, NULL},
    {"GuiGetState", py_gui_get_state, METH_NOARGS, NULL},
    {"GuiSetFont", py_gui_set_font, METH_VARARGS, NULL},
    {"GuiGetFont", py_gui_get_font, METH_NOARGS, NULL},
    {"GuiLoadStyle", py_gui_load_style, METH_VARARGS, NULL},
    {"GuiLoadStyleDefault", py_gui_load_style_default, METH_NOARGS, NULL},
    {"GuiEnableTooltip", py_gui_enable_tooltip, METH_NOARGS, NULL},
    {"GuiDisableTooltip", py_gui_disable_tooltip, METH_NOARGS, NULL},
    {"GuiSetTooltip", py_gui_set_tooltip, METH_VARARGS, NULL},
    {"GuiGetTextWidth", py_gui_get_text_width, METH_VARARGS, NULL},
    {"GuiIconText", py_gui_icon_text, METH_VARARGS, NULL},
    {"GuiSetIconScale", py_gui_set_icon_scale, METH_VARARGS, NULL},
    {"GuiDrawIcon", py_gui_draw_icon, METH_VARARGS, NULL},
    {"GuiWindowBox", py_gui_window_box, METH_VARARGS, NULL},
    {"GuiGroupBox", py_gui_group_box, METH_VARARGS, NULL},
    {"GuiLine", py_gui_line, METH_VARARGS, NULL},
    {"GuiScrollPanel", py_gui_scroll_panel, METH_VARARGS, NULL},
    {"GuiLabelButton", py_gui_label_button, METH_VARARGS, NULL},
    {"GuiToggle", py_gui_toggle, METH_VARARGS, NULL},
    {"GuiToggleGroup", py_gui_toggle_group, METH_VARARGS, NULL},
    {"GuiToggleSlider", py_gui_toggle_slider, METH_VARARGS, NULL},
    {"GuiDropdownBox", py_gui_dropdown_box, METH_VARARGS, NULL},
    {"GuiValueBox", py_gui_value_box, METH_VARARGS, NULL},
    {"GuiValueBoxFloat", py_gui_value_box_float, METH_VARARGS, NULL},
    {"GuiTextBox", py_gui_text_box, METH_VARARGS, NULL},
    {"GuiSliderBar", py_gui_slider_bar, METH_VARARGS, NULL},
    {"GuiDummyRec", py_gui_dummy_rec, METH_VARARGS, NULL},
    {"GuiGrid", py_gui_grid, METH_VARARGS, NULL},
    {"GuiListView", py_gui_list_view, METH_VARARGS, NULL},
    {"GuiListViewEx", py_gui_list_view_ex, METH_VARARGS, NULL},
    {"GuiTabBar", py_gui_tab_bar, METH_VARARGS, NULL},
    {"GuiTabBarEx", py_gui_tab_bar_ex, METH_VARARGS, NULL},
    {"GuiMessageBox", py_gui_message_box, METH_VARARGS, NULL},
    {"GuiTextInputBox", py_gui_text_input_box, METH_VARARGS, NULL},
    {"GuiColorPicker", py_gui_color_picker, METH_VARARGS, NULL},
    {"GuiColorPanel", py_gui_color_panel, METH_VARARGS, NULL},
    {"GuiColorBarAlpha", py_gui_color_bar_alpha, METH_VARARGS, NULL},
    {"GuiColorBarHue", py_gui_color_bar_hue, METH_VARARGS, NULL},
    {"GuiColorPickerHSV", py_gui_color_picker_hsv, METH_VARARGS, NULL},
    {"GuiColorPanelHSV", py_gui_color_panel_hsv, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef raygui_module = {
    PyModuleDef_HEAD_INIT,
    "raygui",
    "GCL Raygui binding",
    -1,
    raygui_methods
};

GCL_PY_EXPORT PyObject *PyInit_raygui(void) {
    return PyModule_Create(&raygui_module);
}
