#include "gcl_ide_internal.h"
#include "gcl_settings_panel.h"
#include "raygui.h"
#include <stdio.h>

/* ---------------------------------------------
   Panel özel state — Editor'a YAZILMAZ.
   --------------------------------------------- */
static int g_open = 0;
static int g_tab = 0;               /* 0=Editor, 1=Effects, 2=Theme */
static int g_font_scratch = 14;
static int g_theme_scratch = 0;
static int g_theme_dd_open = 0;

static unsigned int rgc(Color c) { return ColorToInt(c); }

static void sync_raygui_style(const GclIdeTheme *t, int font_sz) {
    GuiSetFont(g_font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, font_sz);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,    rgc(t->text));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   rgc(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,   rgc(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  rgc(t->gutter));
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,     rgc(t->panel_bg));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,  rgc(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, rgc(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, rgc(t->accent));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,    rgc(t->popup_bg));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,   rgc(t->selection));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,   rgc(t->selection));
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL,     rgc(t->popup_bg));
    GuiSetStyle(SLIDER, BASE_COLOR_FOCUSED,    rgc(t->selection));
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED,    rgc(t->selection));
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL,   rgc(t->accent));
    GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED,  rgc(t->accent));
    GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED,  rgc(t->accent));
    GuiSetStyle(CHECKBOX, BASE_COLOR_NORMAL,   rgc(t->popup_bg));
    GuiSetStyle(CHECKBOX, BASE_COLOR_FOCUSED,  rgc(t->selection));
    GuiSetStyle(CHECKBOX, BASE_COLOR_PRESSED,  rgc(t->selection));
    GuiSetStyle(CHECKBOX, BORDER_COLOR_NORMAL, rgc(t->gutter));
    GuiSetStyle(CHECKBOX, BORDER_COLOR_FOCUSED,rgc(t->accent));
    GuiSetStyle(BUTTON,  BASE_COLOR_NORMAL,    rgc(t->popup_bg));
    GuiSetStyle(BUTTON,  BASE_COLOR_FOCUSED,   rgc(t->selection));
    GuiSetStyle(BUTTON,  BASE_COLOR_PRESSED,   rgc(t->selection));
    GuiSetStyle(BUTTON,  BORDER_COLOR_NORMAL,  rgc(t->accent));
    GuiSetStyle(BUTTON,  BORDER_COLOR_FOCUSED, rgc(t->accent));
    GuiSetStyle(BUTTON,  BORDER_COLOR_PRESSED, rgc(t->accent));
}

/* raygui kontrolü çizer; etiket/değer bizim kesin koordinatlarımızla.
   Sabit piksel aralıklar — öğeler birbirine girmez. */

static void check_row(int x, int y, const char *label, int *val,
                      const GclIdeTheme *t, int font_sz) {
    bool bv = *val != 0;
    GuiCheckBox((Rectangle){ (float)x, (float)(y + 2), 18, 18 }, "", &bv);
    *val = bv ? 1 : 0;
    DrawText(label, x + 26, y + 2, font_sz, *val ? t->accent : t->text);
}

/* label üstte, track altta — 46px blok */
static void slider_row(int x, int y, int w, const char *label, int *val,
                       int min, int max, const GclIdeTheme *t, int font_sz) {
    DrawText(label, x, y + 2, font_sz, t->text);
    char num[16];
    snprintf(num, sizeof(num), "%d", *val);
    int nw = MeasureText(num, font_sz);
    DrawText(num, x + w - nw, y + 2, font_sz, t->accent);

    float fv = (float)*val;
    GuiSlider((Rectangle){ (float)x, (float)(y + font_sz + 6), (float)w, 18 }, "", "", &fv, (float)min, (float)max);
    *val = (int)(fv + 0.5f);
    if (*val < min) *val = min;
    if (*val > max) *val = max;
}

/* ---------------------------------------------
   Ayarları Editor'ün GERÇEK çalışma kopyalarına senkronize et.
   IDE, ed->settings.* değil; ed->vhs/crt/editor_font/bloom_strength
   gibi kopyaları kullanır. Bu olmadan paneldeki değişiklikler hiçbir
   efekte yansımaz.
   --------------------------------------------- */
static void sync_editor_effects(Editor *ed) {
    if (!ed) return;
    ed->syntax_highlight = ed->settings.syntax_highlight;
    ed->vhs             = ed->settings.vhs;
    ed->crt             = ed->settings.crt;
    ed->screen_shake    = ed->settings.screen_shake;
    ed->typewriter      = ed->settings.typewriter;
    ed->particles       = ed->settings.particles;
    ed->blink_cursor    = ed->settings.blink_cursor;
    ed->text_bloom      = ed->settings.text_bloom;
    ed->bloom_strength  = ed->settings.bloom_strength;
    ed->shake_strength  = ed->settings.shake_strength;
    ed->particle_strength = ed->settings.particle_strength;
    ed->vhs_strength    = ed->settings.vhs_strength;
    ed->crt_strength    = ed->settings.crt_strength;
    ed->sound           = ed->settings.sound;
    ed->editor_font     = ed->settings.font_size;
    g_line_h = ed->editor_font + 2;
    g_bloom = ed->text_bloom ? ed->bloom_strength : 0;
    ed->theme = gcl_ide_theme_get(ed->settings.theme_index);
}

/* ---------------------------------------------
   Public API
   --------------------------------------------- */

void gcl_settings_panel_open(Editor *ed) {
    if (!ed) return;
    g_font_scratch = ed->settings.font_size;
    g_theme_scratch = ed->settings.theme_index;
    g_tab = 0;
    g_theme_dd_open = 0;
    g_open = 1;
    ed->settings_open = 1;
}

void gcl_settings_panel_close(Editor *ed) {
    g_open = 0;
    g_theme_dd_open = 0;
    if (ed) {
        ed->settings_open = 0;
        ed->active_textbox = -1;
    }
}

int gcl_settings_panel_is_open(void) { return g_open; }

int gcl_settings_panel_input(Editor *ed, int ctrl) {
    if (!g_open) return 0;
    if (ctrl && IsKeyPressed(KEY_S)) {
        ed->settings.font_size = g_font_scratch;
        ed->settings.theme_index = g_theme_scratch;
        gcl_ide_settings_save(&ed->settings);
        sync_editor_effects(ed);
        gcl_settings_panel_close(ed);
        return 1;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        gcl_settings_panel_close(ed);
        return 1;
    }
    return 0;
}

void gcl_settings_panel_draw(Editor *ed, int w, int h, int font_sz, GclIdeTheme *t) {
    if (!ed || !g_open) return;

    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 180 });
    sync_raygui_style(t, font_sz);

    /* SABİT panel boyutu — hiçbir sekmede büyüyüp küçülmez */
    int dw = 660;
    int header_h = 46;
    int footer_h = 52;
    int dh = 560;
    if (dh > h - 20) dh = h - 20;
    if (dw > w - 20) dw = w - 20;

    int dx = (w - dw) / 2, dy = (h - dh) / 2;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;

    draw_panel_frame((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh },
                     "Settings", t, font_sz);

    int content_top = dy + header_h;
    int content_bottom = dy + dh - footer_h;

    /* İçeriği paneline kırp — hiçbir yazı dışarı çıkamaz */
    BeginScissorMode(dx + 6, content_top, dw - 12, content_bottom - content_top);

    int leftw = 116;
    int lx = dx + 14;
    int rx = lx + leftw + 14;
    int rw = dx + dw - 14 - rx;

    /* ---- Sol sekmeler — HER ZAMAN aynı yerde, kaybolmaz ---- */
    const char *tabs[] = { "Editor", "Effects", "Theme" };
    int ty = content_top + 12;
    for (int i = 0; i < 3; i++) {
        Rectangle tb = { (float)lx, (float)ty, (float)leftw, 36 };
        bool sel = (i == g_tab);
        DrawRectangleRec(tb, sel ? t->selection : t->panel_bg);
        DrawRectangleLinesEx(tb, 1.0f, sel ? t->accent : t->gutter);
        int tw = MeasureText(tabs[i], font_sz);
        DrawText(tabs[i], (int)(tb.x + (tb.width - tw) / 2),
                 (int)(tb.y + (tb.height - font_sz) / 2), font_sz, sel ? WHITE : t->text);
        if (!sel && CheckCollisionPointRec(GetMousePosition(), tb) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_tab = i;
            g_theme_dd_open = 0;
        }
        ty += 44;
    }

    /* ---- Sağ içerik ---- */
    int cx = rx;
    int cy = content_top + 14;
    int cw2 = rw;

    if (g_tab == 0) {
        /* Editor */
        slider_row(cx, cy, cw2, "Font Size", &g_font_scratch, 8, 32, t, font_sz);
        int y = cy + 54;
        check_row(cx, y, "Syntax Highlight", &ed->settings.syntax_highlight, t, font_sz); y += 34;
        check_row(cx, y, "Blink Cursor",     &ed->settings.blink_cursor,     t, font_sz); y += 34;
        check_row(cx, y, "Typewriter Sound", &ed->settings.typewriter,       t, font_sz); y += 34;
        check_row(cx, y, "Sound",            &ed->settings.sound,            t, font_sz); y += 34;
    } else if (g_tab == 1) {
        /* Effects — her biri 74px blok, ayrı ayrı */
        int y = cy;
        check_row(cx, y, "VHS",          &ed->settings.vhs,          t, font_sz);         y += 30;
        slider_row(cx, y, cw2, "VHS Strength",      &ed->settings.vhs_strength, 0, 100, t, font_sz); y += 54;
        check_row(cx, y, "CRT",          &ed->settings.crt,          t, font_sz);         y += 30;
        slider_row(cx, y, cw2, "CRT Strength",      &ed->settings.crt_strength, 0, 100, t, font_sz); y += 54;
        check_row(cx, y, "Screen Shake", &ed->settings.screen_shake, t, font_sz);         y += 30;
        slider_row(cx, y, cw2, "Shake Strength",    &ed->settings.shake_strength, 0, 100, t, font_sz); y += 54;
        check_row(cx, y, "Particles",    &ed->settings.particles,    t, font_sz);         y += 30;
        slider_row(cx, y, cw2, "Particle Strength", &ed->settings.particle_strength, 0, 100, t, font_sz); y += 54;
        check_row(cx, y, "Text Bloom",   &ed->settings.text_bloom,   t, font_sz);         y += 30;
        slider_row(cx, y, cw2, "Bloom Strength",    &ed->settings.bloom_strength, 0, 100, t, font_sz); y += 54;
    } else {
        /* Theme */
        const GclIdeThemeEntry *entry = gcl_ide_theme_entry(g_theme_scratch);
        const char *tname = entry ? entry->name : "?";
        Rectangle btn = { (float)cx, (float)cy, (float)cw2, 30 };
        if (GuiButton(btn, tname)) {
            g_theme_dd_open = !g_theme_dd_open;
        }
        int y = cy + 40;

        if (g_theme_dd_open) {
            int n = gcl_ide_theme_count();
            for (int i = 0; i < n; i++) {
                const GclIdeThemeEntry *e = gcl_ide_theme_entry(i);
                if (!e) continue;
                Rectangle it = { (float)cx, (float)y, (float)cw2, 26 };
                bool hover = CheckCollisionPointRec(GetMousePosition(), it);
                if (i == g_theme_scratch) DrawRectangleRec(it, t->selection);
                else if (hover) DrawRectangleRec(it, t->popup_bg);
                DrawText(e->name, (int)(it.x + 8), (int)(it.y + 4), font_sz,
                         (i == g_theme_scratch) ? WHITE : t->text);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    g_theme_scratch = i;
                    g_theme_dd_open = 0;
                    break;
                }
                y += 30;
            }
        }
    }

    EndScissorMode();

    /* ---- Footer butonlar — genişlik metne göre, sağa hizalı ---- */
    const char *apply_lbl = "Apply";
    const char *save_lbl  = "Save";
    const char *close_lbl = "Close";
    int apply_w = MeasureText(apply_lbl, font_sz) + 24;
    int save_w  = MeasureText(save_lbl, font_sz) + 24;
    int close_w = MeasureText(close_lbl, font_sz) + 24;
    int gap = 10;
    int by = dy + dh - footer_h + 11;

    int bx = dx + dw - 14 - close_w;
    if (GuiButton((Rectangle){ (float)bx, (float)by, (float)close_w, 30 }, close_lbl))
        gcl_settings_panel_close(ed);

    bx -= gap + save_w;
    if (GuiButton((Rectangle){ (float)bx, (float)by, (float)save_w, 30 }, save_lbl)) {
        ed->settings.font_size = g_font_scratch;
        ed->settings.theme_index = g_theme_scratch;
        gcl_ide_settings_save(&ed->settings);
        sync_editor_effects(ed);
        gcl_settings_panel_close(ed);
    }

    bx -= gap + apply_w;
    if (GuiButton((Rectangle){ (float)bx, (float)by, (float)apply_w, 30 }, apply_lbl)) {
        ed->settings.font_size = g_font_scratch;
        ed->settings.theme_index = g_theme_scratch;
        gcl_ide_settings_save(&ed->settings);
        sync_editor_effects(ed);
        /* panel açık kalır */
    }
}

/* About diyaloğu — aynı modülde tutulur. */
void draw_about_panel(Editor *ed, int w, int h, int font_sz, GclIdeTheme *t) {
    (void)ed;
    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 180 });
    int dw = 360, dh = 200;
    int dx = (w - dw) / 2, dy = (h - dh) / 2;
    draw_panel_frame((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh },
                     "About", t, font_sz);
    int x = dx + 20;
    int yy = dy + 56;
    DrawText("GCL IDE", x, yy, font_sz + 2, t->accent);
    yy += 32;
    DrawText("GnuchanOS - GCL Language", x, yy, font_sz - 2, t->text);
    yy += 24;
    DrawText("VSCode-style smart completion", x, yy, font_sz - 2, t->gutter);
    yy += 24;
    int ok_w = MeasureText("OK", font_sz) + 24;
    if (GuiButton((Rectangle){ (float)(dx + dw - 14 - ok_w), (float)(dy + dh - 44), (float)ok_w, 30 }, "OK"))
        ed->about = 0;
}
