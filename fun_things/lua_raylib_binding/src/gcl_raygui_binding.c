/* ============================================================
 * gcl_raygui_binding.c — Lua 5.4 binding for raygui
 *
 * Compiled together with gcl_raylib_binding.c into a single
 * gcl_raylib.dll. Entry point: luaopen_gcl_raygui.
 * ============================================================ */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raygui.h"

/* ============================================================
 * Helper: unpack Rectangle from Lua stack positions
 * ============================================================ */
static Rectangle check_rect(lua_State *L, int idx) {
    Rectangle r;
    r.x      = (float)luaL_checknumber(L, idx);
    r.y      = (float)luaL_checknumber(L, idx+1);
    r.width  = (float)luaL_checknumber(L, idx+2);
    r.height = (float)luaL_checknumber(L, idx+3);
    return r;
}

/* ============================================================
 * GuiControl
 * ============================================================ */
static int lw_GuiWindowBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *title = luaL_checkstring(L, 5);
    lua_pushinteger(L, GuiWindowBox(r, title));
    return 1;
}
static int lw_GuiLabel(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    lua_pushinteger(L, GuiLabel(r, text));
    return 1;
}
static int lw_GuiButton(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    lua_pushinteger(L, GuiButton(r, text));
    return 1;
}
static int lw_GuiToggle(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    bool active = (bool)lua_toboolean(L, 6);
    int result = GuiToggle(r, text, &active);
    lua_pushinteger(L, result);
    lua_pushboolean(L, active);
    return 2;
}
static int lw_GuiCheckBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    bool checked = (bool)lua_toboolean(L, 6);
    int result = GuiCheckBox(r, text, &checked);
    lua_pushinteger(L, result);
    lua_pushboolean(L, checked);
    return 2;
}
static int lw_GuiSlider(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    float value = (float)luaL_checknumber(L, 6);
    float minVal = (float)luaL_checknumber(L, 7);
    float maxVal = (float)luaL_checknumber(L, 8);
    int result = GuiSlider(r, text, NULL, &value, minVal, maxVal);
    lua_pushinteger(L, result);
    lua_pushnumber(L, value);
    return 2;
}
static int lw_GuiSliderBar(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    float value = (float)luaL_checknumber(L, 6);
    float minVal = (float)luaL_checknumber(L, 7);
    float maxVal = (float)luaL_checknumber(L, 8);
    int result = GuiSliderBar(r, text, NULL, &value, minVal, maxVal);
    lua_pushinteger(L, result);
    lua_pushnumber(L, value);
    return 2;
}
static int lw_GuiProgressBar(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    float value = (float)luaL_checknumber(L, 6);
    float minVal = (float)luaL_checknumber(L, 7);
    float maxVal = (float)luaL_checknumber(L, 8);
    int result = GuiProgressBar(r, text, NULL, &value, minVal, maxVal);
    lua_pushinteger(L, result);
    lua_pushnumber(L, value);
    return 2;
}
static int lw_GuiTextBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    size_t textLen;
    const char *text = luaL_checklstring(L, 5, &textLen);
    int maxLen = (int)luaL_checkinteger(L, 6);
    bool editMode = (bool)lua_toboolean(L, 7);
    char *buf = (char*)malloc((maxLen+1)*sizeof(char));
    if (!buf) return luaL_error(L, "out of memory");
    strncpy(buf, text, maxLen);
    buf[maxLen] = '\0';
    int result = GuiTextBox(r, buf, maxLen, editMode);
    lua_pushinteger(L, result);
    lua_pushstring(L, buf);
    free(buf);
    return 2;
}
static int lw_GuiGroupBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    lua_pushinteger(L, GuiGroupBox(r, text));
    return 1;
}
static int lw_GuiValueBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    int value = (int)luaL_checkinteger(L, 6);
    int minVal = (int)luaL_checkinteger(L, 7);
    int maxVal = (int)luaL_checkinteger(L, 8);
    bool editMode = (bool)lua_toboolean(L, 9);
    int result = GuiValueBox(r, text, &value, minVal, maxVal, editMode);
    lua_pushinteger(L, result);
    lua_pushinteger(L, value);
    return 2;
}
static int lw_GuiColorPicker(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    Color c = {(unsigned char)luaL_checkinteger(L, 6),
               (unsigned char)luaL_checkinteger(L, 7),
               (unsigned char)luaL_checkinteger(L, 8),
               (unsigned char)luaL_optinteger(L, 9, 255)};
    int result = GuiColorPicker(r, text, &c);
    lua_pushinteger(L, result);
    lua_createtable(L, 0, 4);
    lua_pushinteger(L, c.r); lua_setfield(L, -2, "r");
    lua_pushinteger(L, c.g); lua_setfield(L, -2, "g");
    lua_pushinteger(L, c.b); lua_setfield(L, -2, "b");
    lua_pushinteger(L, c.a); lua_setfield(L, -2, "a");
    return 2;
}
static int lw_GuiDropdownBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    int active = (int)luaL_checkinteger(L, 6);
    bool editMode = (bool)lua_toboolean(L, 7);
    int result = GuiDropdownBox(r, text, &active, editMode);
    lua_pushinteger(L, result);
    lua_pushinteger(L, active);
    return 2;
}
static int lw_GuiListView(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    int scrollIndex = (int)luaL_checkinteger(L, 6);
    int active = (int)luaL_checkinteger(L, 7);
    int result = GuiListView(r, text, &scrollIndex, &active);
    lua_pushinteger(L, result);
    lua_pushinteger(L, scrollIndex);
    lua_pushinteger(L, active);
    return 3;
}
static int lw_GuiMessageBox(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *title = luaL_checkstring(L, 5);
    const char *msg = luaL_checkstring(L, 6);
    const char *buttons = luaL_checkstring(L, 7);
    lua_pushinteger(L, GuiMessageBox(r, title, msg, buttons));
    return 1;
}
static int lw_GuiToggleGroup(lua_State *L) {
    Rectangle r = check_rect(L, 1);
    const char *text = luaL_checkstring(L, 5);
    int active = (int)luaL_checkinteger(L, 6);
    int result = GuiToggleGroup(r, text, &active);
    lua_pushinteger(L, result);
    lua_pushinteger(L, active);
    return 2;
}
static int lw_GuiSetStyle(lua_State *L) {
    GuiSetStyle((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3));
    return 0;
}
static int lw_GuiGetStyle(lua_State *L) {
    lua_pushinteger(L, GuiGetStyle((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)));
    return 1;
}
static int lw_GuiEnable(lua_State *L) { GuiEnable(); return 0; }
static int lw_GuiDisable(lua_State *L) { GuiDisable(); return 0; }
static int lw_GuiLock(lua_State *L) { GuiLock(); return 0; }
static int lw_GuiUnlock(lua_State *L) { GuiUnlock(); return 0; }
static int lw_GuiIsLocked(lua_State *L) { lua_pushboolean(L, GuiIsLocked()); return 1; }
static int lw_GuiSetAlpha(lua_State *L) { GuiSetAlpha((float)luaL_checknumber(L,1)); return 0; }
static int lw_GuiSetState(lua_State *L) { GuiSetState((int)luaL_checkinteger(L,1)); return 0; }
static int lw_GuiGetState(lua_State *L) { lua_pushinteger(L, GuiGetState()); return 1; }

const struct luaL_Reg raygui_funcs[] = {
    {"GuiWindowBox",   lw_GuiWindowBox},
    {"GuiLabel",       lw_GuiLabel},
    {"GuiButton",      lw_GuiButton},
    {"GuiToggle",      lw_GuiToggle},
    {"GuiCheckBox",    lw_GuiCheckBox},
    {"GuiSlider",      lw_GuiSlider},
    {"GuiSliderBar",   lw_GuiSliderBar},
    {"GuiProgressBar", lw_GuiProgressBar},
    {"GuiTextBox",     lw_GuiTextBox},
    {"GuiGroupBox",    lw_GuiGroupBox},
    {"GuiValueBox",    lw_GuiValueBox},
    {"GuiColorPicker", lw_GuiColorPicker},
    {"GuiDropdownBox", lw_GuiDropdownBox},
    {"GuiListView",    lw_GuiListView},
    {"GuiMessageBox",  lw_GuiMessageBox},
    {"GuiToggleGroup", lw_GuiToggleGroup},
    {"GuiSetStyle",    lw_GuiSetStyle},
    {"GuiGetStyle",    lw_GuiGetStyle},
    {"GuiEnable",      lw_GuiEnable},
    {"GuiDisable",     lw_GuiDisable},
    {"GuiLock",        lw_GuiLock},
    {"GuiUnlock",      lw_GuiUnlock},
    {"GuiIsLocked",    lw_GuiIsLocked},
    {"GuiSetAlpha",    lw_GuiSetAlpha},
    {"GuiSetState",    lw_GuiSetState},
    {"GuiGetState",    lw_GuiGetState},
    {NULL, NULL}
};

void push_raygui_constants(lua_State *L, int table_idx) {
#define PUSH_INT(name) lua_pushinteger(L, name); lua_setfield(L, table_idx, #name)
    PUSH_INT(DEFAULT); PUSH_INT(LABEL); PUSH_INT(BUTTON);
    PUSH_INT(TOGGLE); PUSH_INT(SLIDER); PUSH_INT(PROGRESSBAR);
    PUSH_INT(CHECKBOX); PUSH_INT(COMBOBOX); PUSH_INT(DROPDOWNBOX);
    PUSH_INT(TEXTBOX); PUSH_INT(VALUEBOX); PUSH_INT(TABBAR);
    PUSH_INT(LISTVIEW); PUSH_INT(COLORPICKER); PUSH_INT(SCROLLBAR);
    PUSH_INT(STATUSBAR);
    PUSH_INT(BORDER_COLOR_NORMAL); PUSH_INT(BASE_COLOR_NORMAL);
    PUSH_INT(TEXT_COLOR_NORMAL); PUSH_INT(BORDER_COLOR_FOCUSED);
    PUSH_INT(BASE_COLOR_FOCUSED); PUSH_INT(TEXT_COLOR_FOCUSED);
    PUSH_INT(BORDER_COLOR_PRESSED); PUSH_INT(BASE_COLOR_PRESSED);
    PUSH_INT(TEXT_COLOR_PRESSED); PUSH_INT(BORDER_COLOR_DISABLED);
    PUSH_INT(BASE_COLOR_DISABLED); PUSH_INT(TEXT_COLOR_DISABLED);
    PUSH_INT(TEXT_ALIGN_LEFT); PUSH_INT(TEXT_ALIGN_CENTER);
    PUSH_INT(TEXT_ALIGN_RIGHT);
    PUSH_INT(STATE_NORMAL); PUSH_INT(STATE_FOCUSED);
    PUSH_INT(STATE_PRESSED); PUSH_INT(STATE_DISABLED);
#undef PUSH_INT
}

int __declspec(dllexport) luaopen_gcl_raygui(lua_State *L) {
    luaL_newlib(L, raygui_funcs);
    push_raygui_constants(L, lua_gettop(L));
    return 1;
}
