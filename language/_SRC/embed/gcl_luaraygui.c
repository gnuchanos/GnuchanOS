/*
 * gcl_luaraygui.c — GCL Embed Lua + Raygui binding.
 *
 * Lua tarafında `raygui` global tablosunu oluşturur:
 *   raygui.GuiStatusBar(raylib.Rectangle(0, 0, 800, 30), "text")
 *
 * Rectangle userdata'sı gcl_luaraylib.c'de oluşturulur; aynı Lua state
 * paylaşıldığı için metatable ismi "__GclRaylibRect" ortaktır.
 *
 * Lua C module: luaopen_LuaRaygui (LuaRaygui.dll|.so olarak yüklenir).
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <raylib.h>
#include <raygui.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#ifdef _WIN32
#define GCL_LUA_EXPORT __declspec(dllexport)
#else
#define GCL_LUA_EXPORT __attribute__((visibility("default")))
#endif

#define RECT_MT "__GclRaylibRect"

typedef struct {
    float x, y, w, h;
} LuaRect;

static LuaRect *check_rect(lua_State *L, int idx) {
    return (LuaRect *)luaL_checkudata(L, idx, RECT_MT);
}

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

static int l_gui_status_bar(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    GuiStatusBar((Rectangle){ r->x, r->y, r->w, r->h }, text);
    return 0;
}

static int l_gui_label(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    GuiLabel((Rectangle){ r->x, r->y, r->w, r->h }, text);
    return 0;
}

static int l_gui_button(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    lua_pushboolean(L, GuiButton((Rectangle){ r->x, r->y, r->w, r->h }, text));
    return 1;
}

static int l_gui_check_box(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    bool checked = lua_toboolean(L, 3);
    GuiCheckBox((Rectangle){ r->x, r->y, r->w, r->h }, text, &checked);
    lua_pushboolean(L, checked);
    return 1;
}

static int l_gui_slider(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *tleft = luaL_checkstring(L, 2);
    const char *tright = luaL_checkstring(L, 3);
    float value = (float)luaL_checknumber(L, 4);
    float minv = (float)luaL_checknumber(L, 5);
    float maxv = (float)luaL_checknumber(L, 6);
    value = GuiSlider((Rectangle){ r->x, r->y, r->w, r->h }, tleft, tright, &value, minv, maxv);
    lua_pushnumber(L, value);
    return 1;
}

static int l_gui_progress_bar(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *tleft = luaL_checkstring(L, 2);
    const char *tright = luaL_checkstring(L, 3);
    float value = (float)luaL_checknumber(L, 4);
    float minv = (float)luaL_checknumber(L, 5);
    float maxv = (float)luaL_checknumber(L, 6);
    value = GuiProgressBar((Rectangle){ r->x, r->y, r->w, r->h }, tleft, tright, &value, minv, maxv);
    lua_pushnumber(L, value);
    return 1;
}

static int l_gui_spinner(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    int value = (int)luaL_checkinteger(L, 3);
    int minv = (int)luaL_checkinteger(L, 4);
    int maxv = (int)luaL_checkinteger(L, 5);
    int edit = (int)luaL_checkinteger(L, 6);
    value = GuiSpinner((Rectangle){ r->x, r->y, r->w, r->h }, text, &value, minv, maxv, edit);
    lua_pushinteger(L, value);
    return 1;
}

static int l_gui_combo_box(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 2);
    int active = (int)luaL_checkinteger(L, 3);
    active = GuiComboBox((Rectangle){ r->x, r->y, r->w, r->h }, text, &active);
    lua_pushinteger(L, active);
    return 1;
}

static int l_gui_panel(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *text = luaL_optstring(L, 2, "");
    GuiPanel((Rectangle){ r->x, r->y, r->w, r->h }, text);
    return 0;
}

static int l_gui_set_style(lua_State *L) {
    int control = (int)luaL_checkinteger(L, 1);
    int property = (int)luaL_checkinteger(L, 2);
    int value = (int)luaL_checkinteger(L, 3);
    GuiSetStyle(control, property, value);
    return 0;
}

static int l_gui_get_style(lua_State *L) {
    int control = (int)luaL_checkinteger(L, 1);
    int property = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, GuiGetStyle(control, property));
    return 1;
}

/* =========================================================================
   FULL RAYGUI LUA WRAPPER
   ========================================================================= */

/* ---- State ---- */
static int l_gui_enable(lua_State *L) { GuiEnable(); return 0; }
static int l_gui_disable(lua_State *L) { GuiDisable(); return 0; }
static int l_gui_lock(lua_State *L) { GuiLock(); return 0; }
static int l_gui_unlock(lua_State *L) { GuiUnlock(); return 0; }
static int l_gui_is_locked(lua_State *L) { lua_pushboolean(L, GuiIsLocked()); return 1; }
static int l_gui_set_alpha(lua_State *L) { GuiSetAlpha((float)luaL_checknumber(L,1)); return 0; }
static int l_gui_set_state(lua_State *L) { GuiSetState((int)luaL_checkinteger(L,1)); return 0; }
static int l_gui_get_state(lua_State *L) { lua_pushinteger(L, GuiGetState()); return 1; }
static int l_gui_set_font(lua_State *L) { Font f; f.baseSize=(int)luaL_checkinteger(L,1); f.texture.id=(unsigned int)luaL_checkinteger(L,2); GuiSetFont(f); return 0; }
static int l_gui_get_font(lua_State *L) { Font f = GuiGetFont(); lua_pushinteger(L, f.baseSize); lua_pushinteger(L, f.texture.id); return 2; }
static int l_gui_load_style(lua_State *L) { GuiLoadStyle(luaL_checkstring(L,1)); return 0; }
static int l_gui_load_style_default(lua_State *L) { GuiLoadStyleDefault(); return 0; }
static int l_gui_enable_tooltip(lua_State *L) { GuiEnableTooltip(); return 0; }
static int l_gui_disable_tooltip(lua_State *L) { GuiDisableTooltip(); return 0; }
static int l_gui_set_tooltip(lua_State *L) { GuiSetTooltip(luaL_checkstring(L,1)); return 0; }
static int l_gui_get_text_width(lua_State *L) { lua_pushinteger(L, GuiGetTextWidth(luaL_checkstring(L,1))); return 1; }
static int l_gui_icon_text(lua_State *L) { lua_pushstring(L, GuiIconText((int)luaL_checkinteger(L,1), luaL_checkstring(L,2))); return 1; }
static int l_gui_set_icon_scale(lua_State *L) { GuiSetIconScale((int)luaL_checkinteger(L,1)); return 0; }
static int l_gui_draw_icon(lua_State *L) { GuiDrawIcon((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),(int)luaL_checkinteger(L,3),(int)luaL_checkinteger(L,4),uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }

/* ---- Container ---- */
static int l_gui_window_box(lua_State *L) { LuaRect *r=check_rect(L,1); lua_pushinteger(L, GuiWindowBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2))); return 1; }
static int l_gui_group_box(lua_State *L) { LuaRect *r=check_rect(L,1); lua_pushinteger(L, GuiGroupBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""))); return 1; }
static int l_gui_line(lua_State *L) { LuaRect *r=check_rect(L,1); lua_pushinteger(L, GuiLine((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""))); return 1; }
static int l_gui_scroll_panel(lua_State *L) {
    LuaRect *r=check_rect(L,1); Rectangle content={r->x,r->y,r->w,r->h};
    if (lua_gettop(L)>=2 && lua_istable(L,2)) {
        lua_rawgeti(L,2,1); lua_rawgeti(L,2,2); lua_rawgeti(L,2,3); lua_rawgeti(L,2,4);
        content.x=(float)luaL_checknumber(L,-4); content.y=(float)luaL_checknumber(L,-3);
        content.width=(float)luaL_checknumber(L,-2); content.height=(float)luaL_checknumber(L,-1);
        lua_pop(L,4);
    }
    Vector2 scroll={0,0}; Rectangle view={0};
    int rv=GuiScrollPanel((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,3,""), content, &scroll, &view);
    lua_pushinteger(L, rv);
    lua_pushnumber(L, scroll.x); lua_pushnumber(L, scroll.y); lua_pushnumber(L, view.x); lua_pushnumber(L, view.y);
    lua_pushnumber(L, view.width); lua_pushnumber(L, view.height); return 7;
}

/* ---- Basic ---- */
static int l_gui_label_button(lua_State *L) { LuaRect *r=check_rect(L,1); lua_pushboolean(L, GuiLabelButton((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2))); return 1; }
static int l_gui_toggle(lua_State *L) { LuaRect *r=check_rect(L,1); bool active=lua_toboolean(L,3); GuiToggle((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &active); lua_pushboolean(L, active); return 1; }
static int l_gui_toggle_group(lua_State *L) { LuaRect *r=check_rect(L,1); int active=(int)luaL_checkinteger(L,3); GuiToggleGroup((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &active); lua_pushinteger(L, active); return 1; }
static int l_gui_toggle_slider(lua_State *L) { LuaRect *r=check_rect(L,1); int active=(int)luaL_checkinteger(L,3); GuiToggleSlider((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &active); lua_pushinteger(L, active); return 1; }
static int l_gui_dropdown_box(lua_State *L) { LuaRect *r=check_rect(L,1); int active=(int)luaL_checkinteger(L,3); bool edit=lua_toboolean(L,4); GuiDropdownBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &active, edit); lua_pushinteger(L, active); return 1; }
static int l_gui_value_box(lua_State *L) { LuaRect *r=check_rect(L,1); int value=(int)luaL_checkinteger(L,3); bool edit=lua_toboolean(L,4); GuiValueBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &value, (int)luaL_checkinteger(L,5), (int)luaL_checkinteger(L,6), edit); lua_pushinteger(L, value); return 1; }
static int l_gui_text_box(lua_State *L) {
    LuaRect *r=check_rect(L,1); const char *text=luaL_checkstring(L,2); int size=(int)luaL_checkinteger(L,3);
    char textBuf[4096]={0}; snprintf(textBuf,sizeof(textBuf),"%s",text);
    bool edit=lua_toboolean(L,4); GuiTextBox((Rectangle){r->x,r->y,r->w,r->h}, textBuf, size, edit);
    lua_pushstring(L, textBuf); return 1;
}
static int l_gui_slider_bar(lua_State *L) { LuaRect *r=check_rect(L,1); float value=(float)luaL_checknumber(L,4); GuiSliderBar((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), luaL_checkstring(L,3), &value, (float)luaL_checknumber(L,5), (float)luaL_checknumber(L,6)); lua_pushnumber(L, value); return 1; }
static int l_gui_dummy_rec(lua_State *L) { LuaRect *r=check_rect(L,1); lua_pushinteger(L, GuiDummyRec((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""))); return 1; }
static int l_gui_grid(lua_State *L) { LuaRect *r=check_rect(L,1); Vector2 cell={0}; int rv=GuiGrid((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), (float)luaL_checknumber(L,3), (int)luaL_checkinteger(L,4), &cell); lua_pushinteger(L, rv); lua_pushnumber(L, cell.x); lua_pushnumber(L, cell.y); return 3; }

/* ---- Advanced ---- */
static int l_gui_list_view(lua_State *L) { LuaRect *r=check_rect(L,1); int scroll=0, active=(int)luaL_checkinteger(L,3); GuiListView((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &scroll, &active); lua_pushinteger(L, active); return 1; }
static int l_gui_list_view_ex(lua_State *L) { LuaRect *r=check_rect(L,1); int scroll=0, active=0, focus=0; GuiListViewEx((Rectangle){r->x,r->y,r->w,r->h}, NULL, 0, &scroll, &active, &focus); lua_pushinteger(L, active); return 1; }
static int l_gui_tab_bar(lua_State *L) { LuaRect *r=check_rect(L,1); int hscroll=0, active=(int)luaL_checkinteger(L,3); GuiTabBar((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), &hscroll, &active); lua_pushinteger(L, active); return 1; }
static int l_gui_message_box(lua_State *L) { LuaRect *r=check_rect(L,1); int btn=0; int rv=GuiMessageBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), luaL_checkstring(L,3), luaL_checkstring(L,4), &btn); lua_pushinteger(L, rv); lua_pushinteger(L, btn); return 2; }
static int l_gui_text_input_box(lua_State *L) {
    LuaRect *r=check_rect(L,1); char textBuf[4096]={0}; snprintf(textBuf,sizeof(textBuf),"%s",luaL_checkstring(L,5));
    int btn=0; bool secret=false;
    int rv=GuiTextInputBox((Rectangle){r->x,r->y,r->w,r->h}, luaL_checkstring(L,2), luaL_checkstring(L,3), textBuf, (int)luaL_checkinteger(L,6), luaL_checkstring(L,4), &btn, &secret);
    lua_pushinteger(L, rv); lua_pushstring(L, textBuf); return 2;
}
static int l_gui_color_picker(lua_State *L) { LuaRect *r=check_rect(L,1); Color c=uint_to_color((unsigned int)luaL_checkinteger(L,3)); GuiColorPicker((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &c); lua_pushinteger(L, color_to_uint(c)); return 1; }
static int l_gui_color_panel(lua_State *L) { LuaRect *r=check_rect(L,1); Color c=uint_to_color((unsigned int)luaL_checkinteger(L,3)); GuiColorPanel((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &c); lua_pushinteger(L, color_to_uint(c)); return 1; }
static int l_gui_color_bar_alpha(lua_State *L) { LuaRect *r=check_rect(L,1); float a=(float)luaL_checknumber(L,3); GuiColorBarAlpha((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &a); lua_pushnumber(L, a); return 1; }
static int l_gui_color_bar_hue(lua_State *L) { LuaRect *r=check_rect(L,1); float v=(float)luaL_checknumber(L,3); GuiColorBarHue((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &v); lua_pushnumber(L, v); return 1; }
static int l_gui_color_picker_hsv(lua_State *L) { LuaRect *r=check_rect(L,1); Vector3 c={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5)}; GuiColorPickerHSV((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &c); lua_pushnumber(L, c.x); lua_pushnumber(L, c.y); lua_pushnumber(L, c.z); return 3; }
static int l_gui_color_panel_hsv(lua_State *L) { LuaRect *r=check_rect(L,1); Vector3 c={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5)}; GuiColorPanelHSV((Rectangle){r->x,r->y,r->w,r->h}, luaL_optstring(L,2,""), &c); lua_pushnumber(L, c.x); lua_pushnumber(L, c.y); lua_pushnumber(L, c.z); return 3; }

/* ---------- Fonksiyon tablosu ---------- */

static const luaL_Reg raygui_funcs[] = {
    {"GuiStatusBar", l_gui_status_bar},
    {"GuiLabel", l_gui_label},
    {"GuiButton", l_gui_button},
    {"GuiCheckBox", l_gui_check_box},
    {"GuiSlider", l_gui_slider},
    {"GuiProgressBar", l_gui_progress_bar},
    {"GuiSpinner", l_gui_spinner},
    {"GuiComboBox", l_gui_combo_box},
    {"GuiPanel", l_gui_panel},
    {"GuiSetStyle", l_gui_set_style},
    {"GuiGetStyle", l_gui_get_style},

    /* State */
    {"GuiEnable", l_gui_enable},
    {"GuiDisable", l_gui_disable},
    {"GuiLock", l_gui_lock},
    {"GuiUnlock", l_gui_unlock},
    {"GuiIsLocked", l_gui_is_locked},
    {"GuiSetAlpha", l_gui_set_alpha},
    {"GuiSetState", l_gui_set_state},
    {"GuiGetState", l_gui_get_state},
    {"GuiSetFont", l_gui_set_font},
    {"GuiGetFont", l_gui_get_font},
    {"GuiLoadStyle", l_gui_load_style},
    {"GuiLoadStyleDefault", l_gui_load_style_default},
    {"GuiEnableTooltip", l_gui_enable_tooltip},
    {"GuiDisableTooltip", l_gui_disable_tooltip},
    {"GuiSetTooltip", l_gui_set_tooltip},
    {"GuiGetTextWidth", l_gui_get_text_width},
    {"GuiIconText", l_gui_icon_text},
    {"GuiSetIconScale", l_gui_set_icon_scale},
    {"GuiDrawIcon", l_gui_draw_icon},

    /* Container */
    {"GuiWindowBox", l_gui_window_box},
    {"GuiGroupBox", l_gui_group_box},
    {"GuiLine", l_gui_line},
    {"GuiScrollPanel", l_gui_scroll_panel},

    /* Basic */
    {"GuiLabelButton", l_gui_label_button},
    {"GuiToggle", l_gui_toggle},
    {"GuiToggleGroup", l_gui_toggle_group},
    {"GuiToggleSlider", l_gui_toggle_slider},
    {"GuiDropdownBox", l_gui_dropdown_box},
    {"GuiValueBox", l_gui_value_box},
    {"GuiTextBox", l_gui_text_box},
    {"GuiSliderBar", l_gui_slider_bar},
    {"GuiDummyRec", l_gui_dummy_rec},
    {"GuiGrid", l_gui_grid},

    /* Advanced */
    {"GuiListView", l_gui_list_view},
    {"GuiListViewEx", l_gui_list_view_ex},
    {"GuiTabBar", l_gui_tab_bar},
    {"GuiMessageBox", l_gui_message_box},
    {"GuiTextInputBox", l_gui_text_input_box},
    {"GuiColorPicker", l_gui_color_picker},
    {"GuiColorPanel", l_gui_color_panel},
    {"GuiColorBarAlpha", l_gui_color_bar_alpha},
    {"GuiColorBarHue", l_gui_color_bar_hue},
    {"GuiColorPickerHSV", l_gui_color_picker_hsv},
    {"GuiColorPanelHSV", l_gui_color_panel_hsv},

    {NULL, NULL}
};

GCL_LUA_EXPORT int luaopen_LuaRaygui(lua_State *L) {
    /* Rectangle metatable — raylib module ile ortak (yoksa oluştur) */
    if (luaL_newmetatable(L, RECT_MT)) {
        lua_pop(L, 1);
    } else {
        lua_pop(L, 1);
    }

    lua_newtable(L);
    for (int i = 0; raygui_funcs[i].name; i++) {
        lua_pushcfunction(L, raygui_funcs[i].func);
        lua_setfield(L, -2, raygui_funcs[i].name);
    }
    lua_setglobal(L, "raygui");

    lua_getglobal(L, "raygui");
    return 1;
}
