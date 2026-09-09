#include "gcl_ide_internal.h"
/* rayGUI gövdesi (implementation) tek TU'da üretilir — gcl_settings_panel.c. */
#ifdef RAYGUI_IMPLEMENTATION
#undef RAYGUI_IMPLEMENTATION
#endif
#include "raygui.h"

/* ---------------------------------------------------------------------------
   New Project panel — English-only, compact, theme-aware.
   Panel auto-sizes to fit content; no text overflows the panel.
   GCL core is always present; Lua & Python are independent toggles.
   ------------------------------------------------------------------------- */

#define NPD_PAD 16

/* RayGUI'yi tema ile senkronize et */
static void npd_sync_raygui_style(const GclIdeTheme *t, int font_sz) {
    GuiSetFont(g_font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, font_sz);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,    ColorToInt(t->text));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   ColorToInt(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,   ColorToInt(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  ColorToInt(t->gutter));
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,     ColorToInt(t->popup_bg));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,  ColorToInt(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(t->accent));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,    ColorToInt(t->panel_bg));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,   ColorToInt(t->selection));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,   ColorToInt(t->selection));

    GuiSetStyle(BUTTON,  BORDER_COLOR_NORMAL,  ColorToInt(t->accent));
    GuiSetStyle(BUTTON,  BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
    GuiSetStyle(BUTTON,  BORDER_COLOR_PRESSED, ColorToInt(t->accent));
    GuiSetStyle(BUTTON,  BASE_COLOR_NORMAL,    ColorToInt(t->panel_bg));
    GuiSetStyle(BUTTON,  BASE_COLOR_FOCUSED,   ColorToInt(t->selection));
    GuiSetStyle(BUTTON,  BASE_COLOR_PRESSED,   ColorToInt(t->accent));
    GuiSetStyle(BUTTON,  TEXT_COLOR_NORMAL,    ColorToInt(t->text));
    GuiSetStyle(BUTTON,  TEXT_COLOR_FOCUSED,   ColorToInt(WHITE));

    GuiSetStyle(CHECKBOX, BORDER_COLOR_NORMAL,  ColorToInt(t->accent));
    GuiSetStyle(CHECKBOX, BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
    GuiSetStyle(CHECKBOX, BASE_COLOR_NORMAL,    ColorToInt(t->panel_bg));
    GuiSetStyle(CHECKBOX, BASE_COLOR_FOCUSED,   ColorToInt(t->selection));
    GuiSetStyle(CHECKBOX, BASE_COLOR_PRESSED,   ColorToInt(t->accent));
    GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL,    ColorToInt(t->text));

    GuiSetStyle(TOGGLE, BORDER_COLOR_NORMAL,  ColorToInt(t->accent));
    GuiSetStyle(TOGGLE, BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
    GuiSetStyle(TOGGLE, BASE_COLOR_NORMAL,    ColorToInt(t->panel_bg));
    GuiSetStyle(TOGGLE, BASE_COLOR_FOCUSED,   ColorToInt(t->selection));
    GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED,   ColorToInt(t->accent));
    GuiSetStyle(TOGGLE, TEXT_COLOR_NORMAL,    ColorToInt(t->text));
    GuiSetStyle(TOGGLE, TEXT_COLOR_FOCUSED,   ColorToInt(WHITE));

    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL,    ColorToInt(t->text));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED,   ColorToInt(t->accent));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL,  ColorToInt(t->gutter));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL,    ColorToInt(t->popup_bg));
}

void ide_new_project_open(Editor *ed) {
    ed->project_dialog_open = 1;
    ed->project_name_input[0] = '\0';
    ed->new_project_gcl_scene = 0;
    ed->new_project_gcl_raylib = 0;
    ed->new_project_lua_scene = 0;
    ed->new_project_python_scene = 0;
    ed->new_project_scroll = 0;
    ed->new_project_open_lua = 0;
    ed->new_project_open_luaraylib = 0;
    ed->new_project_open_python = 0;
    ed->new_project_open_pyraylib = 0;
    if (!ed->new_project_path[0]) {
        if (ed->current_project[0]) snprintf(ed->new_project_path, sizeof(ed->new_project_path), "%s", ed->current_project);
        else snprintf(ed->new_project_path, sizeof(ed->new_project_path), "%s", ed->cwd);
    }
    ed->active_textbox = 3;
    ed->text_caret_projname = 0;
    ed->text_sel_projname = 0;
    ed->text_caret_projpath = strlen(ed->new_project_path);
    ed->text_sel_projpath = ed->text_caret_projpath;
}

void ide_new_project_input(Editor *ed, int ctrl) {
    (void)ctrl;
    if (!ed->project_dialog_open) return;
    if (ed->active_textbox == 3)
        textbox_process(ed->project_name_input, sizeof(ed->project_name_input),
                        &ed->text_caret_projname, &ed->text_sel_projname);
    if (ed->active_textbox == 4)
        textbox_process(ed->new_project_path, sizeof(ed->new_project_path),
                        &ed->text_caret_projpath, &ed->text_sel_projpath);
    if (IsKeyPressed(KEY_ESCAPE)) {
        ed->project_dialog_open = 0;
        ed->active_textbox = -1;
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        run_project_dialog(ed);
        return;
    }
}

/* Scene butonları çiz — genişlik yazıya göre.
   GuiButton global rayGUI state'ine bağlı olduğu için bu panelde tıklamaları
   kaçırabiliyor (3D seçilemiyor, hep Empty üretiliyor). Projenin kendi ui_button'u
   saf CheckCollisionPointRec ile çalışır ve OK/Cancel'de kanıtlanmıştır. */
static void npd_scene_buttons(Editor *ed, int x, int y, int *scene, const GclIdeTheme *t, int fs) {
    const char *names[] = { "Empty", "2D", "3D" };
    int bx = x;
    for (int i = 0; i < 3; i++) {
        int sw = MeasureText(names[i], fs) + 22;
        Rectangle br = { (float)bx, (float)y, (float)sw, 28 };
        bool sel = (*scene == i);
        /* Seçili butonun zeminini accent ile doldur, diğerleri panel bg */
        DrawRectangleRec(br, sel ? t->accent : t->panel_bg);
        DrawRectangleLinesEx(br, 1.0f, t->accent);
        int tw = MeasureText(names[i], fs);
        DrawText(names[i], (int)(br.x + (br.width - tw) / 2), (int)(br.y + (br.height - fs) / 2), fs,
                 sel ? WHITE : t->text);
        /* Tıklama: ui_button ile aynı saf yöntem (rayGUI state'inden bağımsız) */
        if (CheckCollisionPointRec(GetMousePosition(), br) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            *scene = i;
        }
        bx += sw + 6;
    }
    (void)ed;
}

void ide_new_project_draw(Editor *ed, int w, int h, int font_sz, const GclIdeTheme *t) {
    if (!ed->project_dialog_open) return;

    npd_sync_raygui_style(t, font_sz);

    /* karartma */
    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 200 });

    int fs = font_sz;
    int lbl_h = fs + 4;
    int tb_h = 30;
    int box_h = 90;

    /* --- Gerçek içerik yüksekliği (font'a bağlı) ---
       Title 40 + (Name lbl+6+tb+14) + (Location lbl+6+tb+14)
       + 3 group box (box_h + aralık) + buton satırı. */
    int content_h = 40
        + (lbl_h + 6 + tb_h + 14)     /* Name */
        + (lbl_h + 6 + tb_h + 14)     /* Location */
        + (box_h + 8)                 /* GCL Core */
        + (box_h + 8)                 /* Lua */
        + (box_h + 12)                /* Python */
        + 8 + 30 + 8;                 /* OK/Cancel */

    int NPD_W = 460;
    int NPD_H = content_h;
    int max_panel_h = h - 40;
    if (max_panel_h < 200) max_panel_h = 200;
    if (NPD_H > max_panel_h) NPD_H = max_panel_h;

    int dx = (w - NPD_W) / 2;
    int dy = (h - NPD_H) / 2;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;

    GuiPanel((Rectangle){ (float)dx, (float)dy, (float)NPD_W, (float)NPD_H }, "New Project");

    int cx = dx + NPD_PAD;
    int cw = NPD_W - NPD_PAD * 2;

    /* --- Scroll: içerik sığmıyorsa scrollbar --- */
    int content_top = dy + 40;
    int content_bottom = dy + NPD_H - 40; /* OK/Cancel alanı */
    int avail_h = content_bottom - content_top;
    int total_content = content_h - 40 - 8 - 30 - 8;
    int max_scroll = total_content > avail_h ? total_content - avail_h : 0;
    if (ed->new_project_scroll > max_scroll) ed->new_project_scroll = max_scroll;
    if (ed->new_project_scroll < 0) ed->new_project_scroll = 0;

    /* Fare tekerleği ile kaydır */
    Rectangle scroll_area = { (float)cx, (float)content_top, (float)cw, (float)avail_h };
    if (CheckCollisionPointRec(GetMousePosition(), scroll_area)) {
        float mw = GetMouseWheelMove();
        if (mw != 0) {
            ed->new_project_scroll -= (int)mw * 24;
            if (ed->new_project_scroll > max_scroll) ed->new_project_scroll = max_scroll;
            if (ed->new_project_scroll < 0) ed->new_project_scroll = 0;
        }
    }

    /* İçerik alanı — scissor ile kırp, y offset ile kaydır */
    BeginScissorMode(cx, content_top, cw, avail_h);
    int y = content_top - ed->new_project_scroll;

    /* ---- Name ---- */
    GuiLabel((Rectangle){ (float)cx, (float)y, (float)cw, (float)lbl_h }, "Name");
    y += lbl_h + 6;
    Rectangle name_tb = { (float)cx, (float)y, (float)cw, (float)tb_h };
    if (ui_label_textbox(name_tb, ed->project_name_input, sizeof(ed->project_name_input),
                         ed->active_textbox == 3, &ed->text_caret_projname,
                         &ed->text_sel_projname, t, fs)) {
        ed->active_textbox = 3;
    }
    y += tb_h + 14;

    /* ---- Location + Browse ---- */
    GuiLabel((Rectangle){ (float)cx, (float)y, (float)cw, (float)lbl_h }, "Location");
    y += lbl_h + 6;
    int browse_w = MeasureText("Browse", fs) + 22;
    int gap = 8;
    int path_w = cw - browse_w - gap;
    Rectangle path_tb = { (float)cx, (float)y, (float)path_w, (float)tb_h };
    if (ui_label_textbox(path_tb, ed->new_project_path, sizeof(ed->new_project_path),
                         ed->active_textbox == 4, &ed->text_caret_projpath,
                         &ed->text_sel_projpath, t, fs)) {
        ed->active_textbox = 4;
    }
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(t->panel_bg));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(t->text));
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(t->accent));
    if (GuiButton((Rectangle){ (float)(cx + path_w + gap), (float)y, (float)browse_w, (float)tb_h }, "Browse")) {
        char *p = gcl_ide_native_dialog_folder(ed->new_project_path, sizeof(ed->new_project_path));
        (void)p;
        ed->text_caret_projpath = strlen(ed->new_project_path);
        ed->text_sel_projpath = ed->text_caret_projpath;
        ed->active_textbox = 4;
    }
    y += tb_h + 14;

    /* ---- GCL Core ---- */
    GuiGroupBox((Rectangle){ (float)cx, (float)y, (float)cw, (float)box_h }, "GCL Core");
    {
        int gx = cx + 10;
        int gy = y + 22;
        bool rl_on = (ed->new_project_gcl_raylib != 0);
        bool new_on = rl_on;
        int tw = MeasureText("Enable", fs) + 20;
        if (GuiToggle((Rectangle){ (float)gx, (float)gy, (float)tw, 28 }, "Enable", &new_on)) {
            ed->new_project_gcl_raylib = new_on ? 1 : 0;
        }
        rl_on = ed->new_project_gcl_raylib;
        const char *state = rl_on ? "Enabled" : "Disabled";
        DrawText(state, gx + tw + 8, gy + 6, fs, rl_on ? t->accent : t->gutter);
        if (rl_on) {
            npd_scene_buttons(ed, gx + tw + 90, gy, &ed->new_project_gcl_scene, t, fs);
        }
    }
    y += box_h + 8;

    /* ---- Embedded Lua ---- */
    GuiGroupBox((Rectangle){ (float)cx, (float)y, (float)cw, (float)box_h }, "Embedded Lua");
    {
        int gx = cx + 10;
        int gy = y + 22;
        bool lua_on = (ed->new_project_open_lua != 0);
        bool new_on = lua_on;
        int tw = MeasureText("Enable", fs) + 20;
        if (GuiToggle((Rectangle){ (float)gx, (float)gy, (float)tw, 28 }, "Enable", &new_on)) {
            ed->new_project_open_lua = new_on ? 1 : 0;
        }
        lua_on = ed->new_project_open_lua;
        const char *state = lua_on ? "Enabled" : "Disabled";
        DrawText(state, gx + tw + 8, gy + 6, fs, lua_on ? t->accent : t->gutter);
        if (lua_on) {
            npd_scene_buttons(ed, gx + tw + 90, gy, &ed->new_project_lua_scene, t, fs);
        }
    }
    y += box_h + 8;

    /* ---- Embedded Python ---- */
    GuiGroupBox((Rectangle){ (float)cx, (float)y, (float)cw, (float)box_h }, "Embedded Python");
    {
        int gx = cx + 10;
        int gy = y + 22;
        bool py_on = (ed->new_project_open_python != 0);
        bool new_on = py_on;
        int tw = MeasureText("Enable", fs) + 20;
        if (GuiToggle((Rectangle){ (float)gx, (float)gy, (float)tw, 28 }, "Enable", &new_on)) {
            ed->new_project_open_python = new_on ? 1 : 0;
        }
        py_on = ed->new_project_open_python;
        const char *state = py_on ? "Enabled" : "Disabled";
        DrawText(state, gx + tw + 8, gy + 6, fs, py_on ? t->accent : t->gutter);
        if (py_on) {
            npd_scene_buttons(ed, gx + tw + 90, gy, &ed->new_project_python_scene, t, fs);
        }
    }
    y += box_h + 12;
    EndScissorMode();

    /* Scrollbar çiz */
    if (max_scroll > 0) {
        int sbw = 6;
        int sby = content_top;
        int sbh = avail_h;
        float frac = (float)avail_h / (float)total_content;
        float bar_h = frac * sbh;
        if (bar_h < 20) bar_h = 20;
        float bar_y = sby + (sbh - bar_h) * ((float)ed->new_project_scroll / (float)max_scroll);
        DrawRectangle(cx + cw - sbw, sby, sbw, sbh, t->popup_bg);
        DrawRectangle(cx + cw - sbw, (int)bar_y, sbw, (int)bar_h, t->accent);
    }

    /* ---- OK / Cancel — her zaman görünür, panel altına sabit.
       NOT: rayGUI'nin GuiButton'u yerine projenin kendi ui_button'u kullanılır.
       ui_button saf raylib CheckCollisionPointRec ile tıklamayı algılar;
       GuiButton global rayGUI state'ine bağlıdır ve bu panelde tıklamayı
       kaçırabiliyordu. ui_button ile OK kesin tetiklenir. ---- */
    int ok_w = MeasureText("OK", fs) + 24;
    int cc_w = MeasureText("Cancel", fs) + 24;
    int gap2 = 8;
    int bby = dy + NPD_H - 40;
    Rectangle ok = { (float)(dx + NPD_W - NPD_PAD - cc_w - gap2 - ok_w), (float)bby, (float)ok_w, 30 };
    Rectangle cc = { (float)(dx + NPD_W - NPD_PAD - cc_w), (float)bby, (float)cc_w, 30 };
    if (ui_button(ok, "OK", t, fs)) {
        run_project_dialog(ed);
    }
    if (ui_button(cc, "Cancel", t, fs)) {
        ed->project_dialog_open = 0;
        ed->active_textbox = -1;
    }
}
