#include "gcl_ide_internal.h"

static Texture2D s_bg_texture = { 0 };
static Texture2D s_logo_texture = { 0 };

/* GCL_IDE_COMPLETE=1: tamamlama penceresini programatik olarak acan dogrulama
   kancasi (bkz. popup cizim yeri). Varsayilan 0 — normal calismada etkisi yok. */
static int g_complete_hook = 0;

static void ensure_placeholder_textures(void) {
    if (s_bg_texture.id == 0) s_bg_texture = LoadTexture("assets/bg.png");
    if (s_logo_texture.id == 0) s_logo_texture = LoadTexture("assets/logo.png");
}

/* Pane state (editor_pane_empty, editor_pane_tab_index, editor_pane_cur,
   editor_set_split_focus, editor_ensure_valid_split_focus) lives in ide_fs_tree.c
   together with the tab functions: it is pure Editor state with no raylib drawing,
   so it can be exercised headlessly by the pane test (todo bug #23 regression
   guard) instead of only being reachable through a running window. */

static void draw_empty_editor_placeholder(Editor *ed, Rectangle rect, const GclIdeTheme *t) {
    ensure_placeholder_textures();
    if (s_bg_texture.id > 0) {
        Rectangle src = { 0.0f, 0.0f, (float)s_bg_texture.width, (float)s_bg_texture.height };
        Rectangle dst = { rect.x, rect.y, rect.width, rect.height };
        DrawTexturePro(s_bg_texture, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    } else {
        DrawRectangleRec(rect, t->bg);
    }
    DrawRectangleRec(rect, (Color){ 6, 10, 18, 170 });

    float logo_w = 180.0f;
    float logo_h = 180.0f;
    if (s_logo_texture.id > 0) {
        float scale = logo_w / (float)s_logo_texture.width;
        logo_h = (float)s_logo_texture.height * scale;
        Rectangle src = { 0.0f, 0.0f, (float)s_logo_texture.width, (float)s_logo_texture.height };
        Rectangle dst = { rect.x + (rect.width - logo_w) * 0.5f, rect.y + 18.0f, logo_w, logo_h };
        DrawTexturePro(s_logo_texture, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    }

    const char *msg = "HELLO GNUCHAN LANGUAGE EDUCATION PLATFORM";
    int font_sz = ed->editor_font + 18;
    if (font_sz > 34) font_sz = 34;
    int text_w = MeasureText(msg, font_sz);
    int x = (int)(rect.x + (rect.width - text_w) * 0.5f);
    int y = (int)(rect.y + 180.0f + 40.0f);
    if (x < (int)rect.x + 18) x = (int)rect.x + 18;
    DrawText(msg, x, y, font_sz, (Color){ 255, 255, 255, 220 });
}

static int editor_has_any_open_tabs(const Editor *ed) {
    if (!ed->split_enabled) return ed->tab_count > 0;
    return ed->tab_count > 0 || ed->split_right_tab_count > 0;
}

/* Hold-to-repeat for editing keys (Backspace/Delete): fires on the first press,
   then repeats after 'first_delay' seconds at 'rate' intervals while held.
   Uses its own state so it never conflicts with arrow-key navigation repeat. */
static int s_edit_repeat_key = 0;
static double s_edit_repeat_time = 0.0;
static int key_repeat_fire(int key, double first_delay, double rate) {
    double now = GetTime();
    if (IsKeyPressed(key)) {
        s_edit_repeat_key = key;
        s_edit_repeat_time = now + first_delay;
        return 1;
    }
    if (IsKeyDown(key) && s_edit_repeat_key == key && now >= s_edit_repeat_time) {
        s_edit_repeat_time = now + rate;
        return 1;
    }
    return 0;
}

/* Tek bir ok-tuÅŸu adÄ±mÄ±. Åift'siz bir ok tuÅŸu seÃ§im varken basÄ±lÄ±rsa imleÃ§
   seÃ§imin yakÄ±n kenarÄ±na toplanÄ±r (editor_collapse_selection); bÃ¶ylece geride
   hayalet secim kalmaz, sonraki Delete/Backspace yanlis araligini silmez
   (todo #6). Åift basÄ±lÄ±yken seÃ§im geniÅŸletilir (Ã§apa sabit kalÄ±r). */
static void editor_nav_step(Editor *ed, int nav_key, int shift) {
    if (!shift && selection_active(ed)) {
        editor_collapse_selection(ed, nav_key == KEY_LEFT || nav_key == KEY_UP);
        return;
    }
    GclIdeBuffer *b = editor_cur(ed);
    if (nav_key == KEY_LEFT) gcl_ide_buffer_cursor_left(b);
    else if (nav_key == KEY_RIGHT) gcl_ide_buffer_cursor_right(b);
    else if (nav_key == KEY_UP) gcl_ide_buffer_cursor_up(b);
    else if (nav_key == KEY_DOWN) gcl_ide_buffer_cursor_down(b);
    if (!shift) b->sel_anchor = b->cursor;
}

static int split_left_pane_clicked(Rectangle left_box, Rectangle right_box) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    Vector2 mp = GetMousePosition();
    return CheckCollisionPointRec(mp, left_box) && !CheckCollisionPointRec(mp, right_box);
}

static int split_right_pane_clicked(Rectangle right_box, Rectangle left_box) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    Vector2 mp = GetMousePosition();
    return CheckCollisionPointRec(mp, right_box) && !CheckCollisionPointRec(mp, left_box);
}

static void draw_pane_tabbar(Editor *ed, Rectangle bar_rect, int pane, const GclIdeTheme *t, int font_sz, int blocked) {
    Rectangle plus = { bar_rect.x + bar_rect.width - 28.0f, bar_rect.y + 2, 24, (float)(TAB_H - 4) };
    DrawText("+", (int)(plus.x + 6), (int)(plus.y + 6), font_sz, t->text);
    if (CheckCollisionPointRec(GetMousePosition(), plus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !blocked) {
        if (pane == 0) {
            tab_add_pane(ed, 0, NULL);
            ed->split_focus = 0;
        } else {
            tab_add_pane(ed, 1, NULL);
            ed->split_focus = 1;
        }
    }

    int pane_count = (pane == 0) ? ed->tab_count : ed->split_right_tab_count;
    if (pane_count <= 0) {
        if (!blocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bar_rect)) {
            editor_set_split_focus(ed, pane);
        }
        return;
    }

    GclIdeBuffer *pane_tabs = (pane == 0) ? ed->tabs : ed->split_right_tabs;
    float scroll = (pane == 0) ? ed->split_left_tab_scroll : ed->split_right_tab_scroll;
    float content_w = 0.0f;
    for (int i = 0; i < pane_count; i++) {
        const char *name = pane_tabs[i].path ? path_basename(pane_tabs[i].path) : "untitled";
        content_w += (float)(MeasureText(name, font_sz - 2) + 36) + 4.0f;
    }

    float usable_w = bar_rect.width - 42.0f;
    float max_scroll = 0.0f;
    if (content_w > usable_w) max_scroll = content_w - usable_w;
    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0.0f) scroll = 0.0f;
    if (pane == 0) ed->split_left_tab_scroll = scroll; else ed->split_right_tab_scroll = scroll;

    if (CheckCollisionPointRec(GetMousePosition(), bar_rect) && GetMouseWheelMove() != 0.0f && !blocked) {
        float wheel = GetMouseWheelMove();
        if (pane == 0) ed->split_left_tab_scroll += wheel * 30.0f; else ed->split_right_tab_scroll += wheel * 30.0f;
    }

    float tx = bar_rect.x + 6.0f - scroll;
    int active_idx = editor_pane_tab_index(ed, pane);
    if (active_idx < 0 || active_idx >= pane_count) active_idx = 0;

    BeginScissorMode((int)bar_rect.x, (int)bar_rect.y, (int)bar_rect.width, (int)bar_rect.height);
    for (int i = 0; i < pane_count; i++) {
        const char *name = pane_tabs[i].path ? path_basename(pane_tabs[i].path) : "untitled";
        int tw = MeasureText(name, font_sz - 2) + 36;
        Rectangle tr = { tx, bar_rect.y + 2, (float)tw, (float)(TAB_H - 4) };
        bool active = (i == active_idx);

        if (tr.x + tr.width > bar_rect.x - 4.0f && tr.x < bar_rect.x + bar_rect.width) {
            DrawRectangleRec(tr, active ? t->panel_bg : t->menu_bg);
            DrawRectangleLinesEx(tr, 1, active ? t->accent : t->gutter);
            DrawText(name, (int)(tx + 8), (int)(bar_rect.y + 6), font_sz - 2, active ? t->text : t->gutter);
        }

        Rectangle close = { tx + tw - 20, bar_rect.y + 6, 14, 14 };
        if (CheckCollisionPointRec(GetMousePosition(), close)) {
            DrawText("x", (int)(close.x + 2), (int)(close.y), font_sz - 2, t->error);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !blocked) {
                if (i == active_idx) {
                    if (pane == 0) ed->split_left_tab = -1; else ed->split_right_tab = -1;
                }
                if (pane == 0) tab_close(ed, i); else tab_close_pane(ed, pane, i);
                if (pane == 0) {
                    if (ed->split_left_tab < 0 && ed->tab_count > 0) ed->split_left_tab = ed->active_tab;
                } else if (ed->split_right_tab < 0 && ed->split_right_tab_count > 0) {
                    ed->split_right_tab = ed->split_right_tab_count - 1;
                }
                break;
            }
        }

        if (CheckCollisionPointRec(GetMousePosition(), tr) && !blocked && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
            if (!CheckCollisionPointRec(GetMousePosition(), close)) {
                if (pane == 0) {
                    ed->split_left_tab = i;
                    ed->split_focus = 0;
                    tab_activate_pane(ed, pane, i);
                } else {
                    ed->split_right_tab = i;
                    ed->split_focus = 1;
                    tab_activate_pane(ed, pane, i);
                }
                if (ed->split_enabled) editor_cur(ed)->sel_anchor = editor_cur(ed)->cursor;
            }
        }

        tx += tw + 4.0f;
    }
    EndScissorMode();
}

static void draw_single_editor_pane(Editor *ed, Rectangle editor_rect, const GclIdeTheme *t, int blocked, int efont, int pane) {
    GclIdeBuffer *b = editor_pane_cur(ed, pane);
    if (!b) return;
    if (ed->split_enabled) editor_ensure_valid_split_focus(ed);
    bool is_active = ed->split_enabled && ed->split_focus == pane;
    Color border_color = is_active ? (Color){ 212, 170, 255, 255 } : (Color){ 72, 58, 102, 220 };
    Color inner_fill = is_active ? (Color){ 22, 18, 34, 255 } : (Color){ 14, 10, 24, 255 };

    Rectangle pane_box = { editor_rect.x + 2.0f, editor_rect.y + 2.0f, editor_rect.width - 4.0f, editor_rect.height - 4.0f };
    DrawRectangleRec(pane_box, inner_fill);
    /* The pane border is ALWAYS drawn: in split view the focused pane gets the bright
       accent border and the other pane a dim one, so it is visible which pane owns the
       keyboard. Previously the border was skipped whenever split was on, so both panes
       looked identical and the focus highlight never appeared (todo bug #5). */
    DrawRectangleLinesEx(pane_box, 1.5f, border_color);

    if (editor_pane_empty(ed, pane)) {
        if (!blocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), editor_rect)) {
            editor_set_split_focus(ed, pane);
        }
        if (!editor_has_any_open_tabs(ed)) {
            draw_empty_editor_placeholder(ed, editor_rect, t);
            return;
        }
        DrawRectangleRec(editor_rect, (Color){ 20, 14, 44, 255 });
        DrawRectangleRec(editor_rect, (Color){ 8, 6, 20, 180 });
        return;
    }

    float fcw = gcl_measure_text_f("M", efont);
    if (fcw < 1.0f) fcw = 1.0f;
    int cw = (int)(fcw + 0.5f);
    if (cw < 1) cw = 1;
    {
        size_t ccur_line = gcl_ide_buffer_line_of_cursor(b);
        size_t ccur_len = 0;
        const char *ccur_txt = gcl_ide_buffer_line_at(b, ccur_line, &ccur_len);
        size_t ccur_off = gcl_ide_buffer_cursor_in_line(b);
        if (ccur_off > ccur_len) ccur_off = ccur_len;
        char ccur_pre[4096];
        if (ccur_off > sizeof(ccur_pre) - 1) ccur_off = sizeof(ccur_pre) - 1;
        memcpy(ccur_pre, ccur_txt, ccur_off); ccur_pre[ccur_off] = '\0';
        size_t ccol_t = (size_t)gcl_text_cells(ccur_pre, 0);
        int view_chars = ((int)editor_rect.width - GUTTER_W - 8) / cw;
        if (view_chars < 1) view_chars = 1;
        if ((int)ccol_t < (int)b->scroll_x) b->scroll_x = ccol_t;
        else if ((int)ccol_t >= (int)b->scroll_x + view_chars) b->scroll_x = (int)ccol_t - view_chars + 1;
    }
    /* Cursor-follow is PER PANE (ed->last_cursor[pane]): a shared field made each
       pane react to the other pane's caret, which cancelled manual scrolling in
       split view (todo bug #4/#17). */
    if (b->cursor != ed->last_cursor[pane]) {
        size_t cline_t = gcl_ide_buffer_line_of_cursor(b);
        size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
        if (vis < 1) vis = 1;
        if (cline_t < b->scroll_y) b->scroll_y = cline_t;
        else if (cline_t >= b->scroll_y + vis) b->scroll_y = cline_t - vis + 1;
    }
    ed->last_cursor[pane] = b->cursor;
    float sx_f = fcw * (float)b->scroll_x;
    int sx = (int)(sx_f + 0.5f);
    DrawRectangleRec(editor_rect, t->bg);
    size_t total_lines = gcl_ide_buffer_line_count(b);
    size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
    if (vis < 1) vis = 1;
    BeginScissorMode((int)editor_rect.x, (int)editor_rect.y, (int)editor_rect.width, (int)editor_rect.height);
    size_t cur_line = gcl_ide_buffer_line_of_cursor(b);
    if (cur_line >= b->scroll_y) {
        int ly = (int)editor_rect.y + 4 + (int)(cur_line - b->scroll_y) * LINE_H;
        DrawRectangle((int)editor_rect.x, ly, (int)editor_rect.width, LINE_H, t->line_bg);
    }
    if (b->content && b->size > 0) {
        size_t a = b->sel_anchor;
        if (a > b->size) a = b->size;
        size_t c = b->cursor;
        if (c > b->size) c = b->size;
        size_t s0 = a < c ? a : c;
        size_t s1 = a < c ? c : a;
        if (s0 < s1) {
            for (size_t i = b->scroll_y; i < total_lines; i++) {
                int y = (int)editor_rect.y + 4 + (int)(i - b->scroll_y) * LINE_H;
                if (y > (int)(editor_rect.y + editor_rect.height)) break;
                size_t l = 0;
                const char *line = gcl_ide_buffer_line_at(b, i, &l);
                size_t off = (size_t)(line - b->content);
                size_t le = off + l;
                if (s0 <= le && s1 >= off) {
                    size_t cs = s0 > off ? s0 - off : 0;
                    size_t ce = s1 < le ? s1 - off : le - off;
                    if (cs < ce) {
                        char ps[4096], pe[4096];
                        if (cs > sizeof(ps) - 1) cs = sizeof(ps) - 1;
                        if (ce > sizeof(pe) - 1) ce = sizeof(pe) - 1;
                        memcpy(ps, line, cs); ps[cs] = '\0';
                        memcpy(pe, line, ce); pe[ce] = '\0';
                        int x0 = (int)editor_rect.x + GUTTER_W + MeasureText(ps, efont) - sx;
                        int x1 = (int)editor_rect.x + GUTTER_W + MeasureText(pe, efont) - sx;
                        if (x1 > x0) DrawRectangle(x0, y, x1 - x0, LINE_H, t->selection);
                    }
                }
            }
        }
    }
    for (size_t i = b->scroll_y; i < total_lines; i++) {
        int y = (int)editor_rect.y + 4 + (int)(i - b->scroll_y) * LINE_H;
        if (y > (int)(editor_rect.y + editor_rect.height)) break;
        size_t l = 0;
        const char *line = gcl_ide_buffer_line_at(b, i, &l);
        char num[24]; snprintf(num, sizeof(num), "%zu", i + 1);
        DrawText(num, (int)(editor_rect.x + GUTTER_W - 8 - MeasureText(num, efont)), y, efont, t->gutter);
        char linebuf[4096];
        if (l >= sizeof(linebuf)) l = sizeof(linebuf) - 1;
        memcpy(linebuf, line, l); linebuf[l] = '\0';
        if (ed->syntax_highlight) {
            float px = (float)editor_rect.x + GUTTER_W - (float)sx;
            int pcol = 0;
            size_t pos = 0;
            while (pos < l) {
                size_t tl = 0;
                const char *ext = b->path ? strrchr(b->path, '.') : NULL;
                const char *hl_lang = "gcl";
                if (ext && strcmp(ext, ".py") == 0) hl_lang = "python";
                else if (ext && strcmp(ext, ".lua") == 0) hl_lang = "lua";
                int tt = gcl_token_type(linebuf, l, pos, &tl, hl_lang);
                if (tl == 0) tl = 1;
                if (tl > l - pos) tl = l - pos;
                char chunk[4096];
                if (tl > sizeof(chunk) - 1) tl = sizeof(chunk) - 1;
                memcpy(chunk, linebuf + pos, tl); chunk[tl] = '\0';
                if (chunk[0] != '\0') {
                    Color col = gcl_token_color(tt, t);
                    gcl_draw_text_col(chunk, px, y, efont, col, pcol);
                    px += gcl_measure_text_col(chunk, efont, pcol);
                    pcol += gcl_text_cells(chunk, pcol);
                }
                pos += tl;
            }
        } else {
            DrawText(linebuf, (int)(editor_rect.x + GUTTER_W - sx), y, efont, t->text);
        }
    }
    if (cur_line >= b->scroll_y) {
        int cy = (int)editor_rect.y + 4 + (int)(cur_line - b->scroll_y) * LINE_H;
        size_t l = 0;
        const char *cline = gcl_ide_buffer_line_at(b, cur_line, &l);
        size_t cc = gcl_ide_buffer_cursor_in_line(b);
        if (cc > l) cc = l;
        char prefix[4096];
        if (cc > sizeof(prefix) - 1) cc = sizeof(prefix) - 1;
        memcpy(prefix, cline, cc); prefix[cc] = '\0';
        int cx = (int)editor_rect.x + GUTTER_W + MeasureText(prefix, efont) - sx;
        if (ed->blink_cursor) {
            if ((int)(GetTime() * 2.0) % 2 == 0) DrawRectangle(cx, cy, 2, LINE_H, t->cursor);
        } else {
            DrawRectangle(cx, cy, 2, LINE_H, t->cursor);
        }
    }
    EndScissorMode();
    if (!blocked) {
        Vector2 mp = GetMousePosition();
        Rectangle inner = { editor_rect.x + GUTTER_W, editor_rect.y + 4, editor_rect.width - GUTTER_W, editor_rect.height - 8 };
        if (CheckCollisionPointRec(mp, inner)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int line = (int)(b->scroll_y + (int)((mp.y - inner.y) / LINE_H));
                if (line >= 0 && line < (int)total_lines) {
                    size_t l = 0;
                    const char *line_p = gcl_ide_buffer_line_at(b, (size_t)line, &l);
                    size_t off = (size_t)(line_p - b->content);
                    size_t col = 0;
                    size_t boff = 0;
                    int cells = 0;
                    int want = (int)(((mp.x - (inner.x - (float)sx)) / fcw) + 0.5f);
                    if (want < 0) want = 0;
                    while (boff < l) {
                        unsigned char lc = (unsigned char)line_p[boff];
                        int ll = (lc < 0x80) ? 1 : (((lc & 0xE0) == 0xC0) ? 2 :
                                 (((lc & 0xF0) == 0xE0) ? 3 : 4));
                        if (boff + (size_t)ll > l) ll = (int)(l - boff);
                        int adv = (line_p[boff] == '\t')
                                  ? (GCL_TAB_SIZE - (cells % GCL_TAB_SIZE)) : 1;
                        if (cells + adv <= want) {
                            cells += adv;
                            col = boff + (size_t)ll;
                            boff += (size_t)ll;
                        } else {
                            break;
                        }
                    }
                    b->cursor = off + col;
                    if (!ed->split_dragging) b->sel_anchor = b->cursor;
                }
            }
        }
    }
}

int gcl_ide_run(const char *path) {
    Editor ed;
    memset(&ed, 0, sizeof(ed));

    gcl_ide_settings_load(&ed.settings);
    ed.settings_font = ed.settings.font_size;
    ed.settings_theme = ed.settings.theme_index;
    ed.theme = gcl_ide_theme_get(ed.settings.theme_index);

    /* effect/setting copies */
    ed.syntax_highlight = ed.settings.syntax_highlight;
    ed.vhs = ed.settings.vhs;
    ed.crt = ed.settings.crt;
    ed.screen_shake = ed.settings.screen_shake;
    ed.typewriter = ed.settings.typewriter;
    ed.particles = ed.settings.particles;
    ed.blink_cursor = ed.settings.blink_cursor;
    ed.text_bloom = ed.settings.text_bloom;
    ed.bloom_strength = ed.settings.bloom_strength;
    g_bloom = ed.text_bloom ? ed.bloom_strength : 0;
    ed.shake_strength = ed.settings.shake_strength;
    ed.particle_strength = ed.settings.particle_strength;
    ed.vhs_strength = ed.settings.vhs_strength;
    ed.crt_strength = ed.settings.crt_strength;
    ed.sound = ed.settings.sound;
    ed.editor_font = ed.settings.font_size;
    g_line_h = ed.editor_font + 2;

    /* effect initial values (ridiculous_coding: ease-out shake + pitch typewriter) */
    ed.sound_pitch = 1.0f;
    ed.pitch_increase = 0.0f;
    ed.shake_remaining = 0.0f;
    ed.shake_duration = 0.0f;
    ed.shake_intensity = 0.0f;

    ed.about = 0;
    ed.settings_open = 0;
    ed.run_mode = 1;
    ed.current_project[0] = '\0';
    ed.new_project_path[0] = '\0';
    ed.text_caret_projpath = 0;
    ed.text_sel_projpath = 0;
    ed.help_open = 0;
    ed.project_dialog_open = 0;
    ed.project_name_input[0] = '\0';
    ed.text_caret_projname = 0;
    ed.text_sel_projname = 0;
    ed.new_project_open_lua = 1;
    ed.new_project_open_luaraylib = 1;
    ed.new_project_open_python = 1;
    ed.new_project_open_pyraylib = 1;
    ed.nav_repeat_key = 0;
    ed.nav_repeat_time = 0.0;
    ed.last_cursor[0] = 0;
    ed.last_cursor[1] = 0;
    ed.ctx_open = 0;
    ed.ctx_x = 0; ed.ctx_y = 0;
    ed.ctx_item = 0;
    ed.ctx_target = -1;
    ed.name_dialog_open = 0;
    ed.name_dialog_mode = 0;
    ed.name_dialog_input[0] = '\0';
    ed.folder_picker_open = 0;
    ed.folder_picker_path[0] = '\0';
    ed.clipboard_has = 0;
    ed.clipboard_path[0] = '\0';
    ed.expanded_count = 0;
    ed.tree_scroll = 0;
    ed.tree_selected = -1;
    ed.active_textbox = -1;
    ed.theme_dropdown_open = 0;
    ed.sidebar_visible = 1;
    ed.menu_open = 0;
    ed.split_enabled = 0;
    ed.split_ratio = 0.5f;
    ed.split_dragging = 0;
    ed.clip_text = NULL;
    ed.clip_len = 0;
    ed.text_caret_name = 0;
    ed.text_caret_folder = 0;
    ed.text_sel_name = 0;
    ed.text_sel_folder = 0;
    ed.tabs = NULL;
    ed.tab_count = 0;
    ed.active_tab = -1;
    ed.sidebar_width = SIDEBAR_W;
    ed.quit = 0;
    ed.edited_this_frame = 0;
    ed.output[0] = '\0';
    ed.output_len = 0;
    ed.output_visible = 0;
    ed.output_scroll = 0;  /* 0 = bottom (follow); positive = number of lines scrolled up */
    ed.running = 0;

#ifndef _WIN32
    if (getcwd(ed.cwd, sizeof(ed.cwd)) == NULL) snprintf(ed.cwd, sizeof(ed.cwd), ".");
#else
    snprintf(ed.cwd, sizeof(ed.cwd), ".");
#endif

    gcl_ide_buffer_init(&ed.blank_tab);
    if (path && path[0]) {
        /* Doc: gcl -ide D:\path -> directory opens as the workspace, file loads as a tab. */
        if (fs_is_dir(path)) {
            snprintf(ed.cwd, sizeof(ed.cwd), "%s", path);
            ed.tab_count = 0;
            ed.active_tab = -1;
        } else {
            /* A FILE: its own directory becomes the workspace root, so the
               EXPLORER shows the project the file belongs to. The else branch
               used to only add the tab and leave ed.cwd at "." (the process
               CWD): launching `gcl -ide proj/main.gcsf` from the repo root
               therefore listed the REPO ROOT in the explorer while the editor
               showed proj/main.gcsf (todo bug). fs_parent_dir() strips at the
               last '/' or '\' and yields "" for a bare file name, which falls
               back to the current directory as before. */
            fs_parent_dir(path, ed.cwd, sizeof(ed.cwd));
            if (!ed.cwd[0]) snprintf(ed.cwd, sizeof(ed.cwd), ".");
            ed.tab_count = 0;
            ed.active_tab = -1;
            tab_add(&ed, path);
        }
    } else {
        ed.tab_count = 0;
        ed.active_tab = -1;
    }
    tree_rescan(&ed);
    ed.last_tree_scan = GetTime();

    /* UI-verification hook (GCL_IDE_SPLIT=1): start with the split view already on
       so the split layout, the per-pane tab strips and the empty-pane state can be
       captured automatically (see GCL_IDE_SHOT). Off unless the variable is set,
       so normal runs always start in single-pane mode. */
    /* Tamamlama dogrulama kancasi: yalnizca ortam degiskeni verildiginde acilir. */
    g_complete_hook = getenv("GCL_IDE_COMPLETE") ? 1 : 0;

    if (getenv("GCL_IDE_SPLIT")) {
        ed.split_enabled = 1;
        ed.split_left_tab = ed.active_tab;   /* -1 when the IDE was given a directory */
        ed.split_left_tab_scroll = 0.0f;
        ed.split_right_tab = -1;
        ed.split_right_tab_scroll = 0.0f;
        ed.split_focus = (ed.split_left_tab >= 0) ? 0 : 1;
    }

    InitWindow(1200, 760, "GCL IDE");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable default ESC exit

    /* Title bar icon - assets/logo.png */
    Image icon = LoadImage("assets/logo.png");
    if (icon.data != NULL) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    }
    ensure_placeholder_textures();

    /* GNU FreeFont - FreeMono.ttf (monospace, SIL OFL 1.1 license).
       Embedded as a byte array; LoadFontFromMemory eliminates the file path problem
       at its root. Texture filter BILINEAR: on zoom out (16px->8px) text is drawn
       smooth (antialiased) instead of pixelated. The default POINT filter looks
       terrible at small sizes. */
    {
        /* The font texture is generated at HIGH resolution (48px); when scaled to
           8-32px via DrawTextEx(fontSize), the BILINEAR filter makes it smooth on
           zoom out and sharp on zoom in. Scaling down to a small (16px) texture
           would increase pixelation. */
        int tf = 48;
        Font f = LoadFontFromMemory(".ttf", gcl_embed_freemono_ttf,
                                    (int)gcl_embed_freemono_ttf_size, tf, NULL, 0);
        if (f.texture.id > 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            g_font = f;
        }
    }

    InitAudioDevice();
    {
        const unsigned int rate = 22050;
        const unsigned int samples = rate * 60 / 1000;

        /* Normal typewriter click sound (typing a character) */
        short *buf = (short *)malloc(sizeof(short) * samples);
        if (buf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float v = (sinf(2 * 3.14159f * 160.0f * t_) * 0.7f +
                           sinf(2 * 3.14159f * 90.0f * t_) * 0.4f +
                           noise * 0.3f) * env * 0.6f;
                buf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = buf;
            ed.click_snd = LoadSoundFromWave(w);
            free(buf);
        }

        /* Enter: strong low thud (60Hz + sub) */
        short *ebuf = (short *)malloc(sizeof(short) * samples);
        if (ebuf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float v = (sinf(2 * 3.14159f * 60.0f * t_) * 0.8f +
                           sinf(2 * 3.14159f * 45.0f * t_) * 0.5f +
                           sinf(2 * 3.14159f * 130.0f * t_) * 0.3f +
                           noise * 0.4f) * env * 0.7f;
                ebuf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = ebuf;
            ed.enter_snd = LoadSoundFromWave(w);
            free(ebuf);
        }

        /* Delete/Backspace: different, higher-pitched pop (chirp 200->600Hz) */
        short *dbuf = (short *)malloc(sizeof(short) * samples);
        if (dbuf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float freq = 200.0f + 400.0f * env;
                float v = (sinf(2 * 3.14159f * freq * t_) * 0.6f +
                           sinf(2 * 3.14159f * 950.0f * t_) * 0.25f +
                           noise * 0.35f) * env * 0.6f;
                dbuf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = dbuf;
            ed.delete_snd = LoadSoundFromWave(w);
            free(dbuf);
        }
    }

    while (!WindowShouldClose() && !ed.quit) {
        /* Collect output of the externally running process each frame (async run) */
        gcl_ide_proc_poll(&ed);

        /* Poll the build thread — build steps flow without freezing the GUI */
        editor_build_poll(&ed);
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        int font_sz = ed.settings.font_size;
        GclIdeTheme t = ed.theme;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        int content_top = MENU_H;
        Rectangle menu_rect = { 0, 0, (float)w, MENU_H };
        Rectangle sidebar_rect = { 0, (float)content_top, (float)ed.sidebar_width, (float)(h - content_top - STATUS_H) };
        float right_x = ed.sidebar_visible ? (float)ed.sidebar_width : 0;
        float right_w = (float)w - right_x;
        Rectangle tabbar_rect = { right_x, (float)content_top, right_w, TAB_H };
        int output_h = ed.output_visible ? 160 : 0;
        /* panes_rect = the whole editor region. The split layout divides THIS rect into two
           panes, and every pane carries its own tab strip inside its own box.
           editor_rect = the code area of the single (non-split) editor, positioned BELOW the
           single tab strip. Previously editor_rect started at content_top, i.e. exactly
           underneath tabbar_rect, so draw_single_editor_pane() painted the pane over the tab
           strip and every tab was invisible until the user enabled split view
           (todo issue #1). Geometry now matches the split pane layout: TAB_H + 6. */
        Rectangle panes_rect = { right_x, (float)content_top, right_w, (float)(h - content_top - STATUS_H - output_h) };
        Rectangle editor_rect = { panes_rect.x, panes_rect.y + (float)TAB_H + 6.0f,
                                  panes_rect.width, panes_rect.height - (float)TAB_H - 6.0f };
        Rectangle output_rect = { right_x, (float)(h - STATUS_H - output_h), right_w, (float)output_h };
        Rectangle status_rect = { 0, (float)(h - STATUS_H), (float)w, STATUS_H };

        bool blocked = ed.settings_open || ed.name_dialog_open || ed.folder_picker_open || ed.project_dialog_open || ed.about || ed.menu_open || ed.help_open || ed.warning_open;

        /* sidebar resizing (draggable from the right edge) */
        if (ed.sidebar_visible && !blocked) {
            Vector2 mp = GetMousePosition();
            Rectangle handle = { (float)(ed.sidebar_width - 4), (float)content_top, 8, (float)(h - content_top - STATUS_H) };
            if (CheckCollisionPointRec(mp, handle) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                ed.sidebar_width = (int)mp.x;
                if (ed.sidebar_width < 120) ed.sidebar_width = 120;
                if (ed.sidebar_width > w / 2) ed.sidebar_width = w / 2;
            }
        }

        /* periodic explorer refresh — do not run while modal/name_dialog is open (avoid GUI freeze).
           tree_rescan performs synchronous I/O; in a large directory it blocks once per second.
           Calling it while a modal is open (blocked) causes a "nothing can be clicked" state. */
        if (!blocked && (GetTime() - ed.last_tree_scan) > 1.0) {
            ed.last_tree_scan = GetTime();
            tree_rescan(&ed);
            if (ed.tree_selected >= ed.tree.count) ed.tree_selected = -1;
        }

        /* ---- input ---- */
        Rectangle split_left_rect = { 0, 0, 0, 0 };
        Rectangle split_right_rect = { 0, 0, 0, 0 };
        bool mouse_over_split_tabbar = false;
        if (ed.split_enabled) {
            editor_ensure_valid_split_focus(&ed);
            float left_w = panes_rect.width * ed.split_ratio;
            float divider_w = 6.0f;
            split_left_rect = (Rectangle){ panes_rect.x, panes_rect.y, left_w, panes_rect.height };
            split_right_rect = (Rectangle){ panes_rect.x + left_w + divider_w, panes_rect.y,
                                    panes_rect.width - left_w - divider_w, panes_rect.height };
            Rectangle left_box = { panes_rect.x, panes_rect.y, left_w, panes_rect.height };
            Rectangle right_box = { panes_rect.x + left_w + divider_w, panes_rect.y, panes_rect.width - left_w - divider_w, panes_rect.height };
            Rectangle left_tabbar = { left_box.x + 4.0f, left_box.y + 4.0f, left_box.width - 8.0f, (float)TAB_H };
            Rectangle right_tabbar = { right_box.x + 4.0f, right_box.y + 4.0f, right_box.width - 8.0f, (float)TAB_H };
            mouse_over_split_tabbar = CheckCollisionPointRec(GetMousePosition(), left_tabbar) || CheckCollisionPointRec(GetMousePosition(), right_tabbar);

            if (split_left_pane_clicked(left_box, right_box)) {
                editor_set_split_focus(&ed, 0);
            } else if (split_right_pane_clicked(right_box, left_box)) {
                editor_set_split_focus(&ed, 1);
            }
        }

        ed.edited_this_frame = 0;
        if (ed.settings_open && !ed.about) {
            gcl_settings_panel_input(&ed, ctrl);
        } else if (ed.name_dialog_open) {
            if (ed.active_textbox == 1) textbox_process(ed.name_dialog_input, sizeof(ed.name_dialog_input), &ed.text_caret_name, &ed.text_sel_name);
            if (IsKeyPressed(KEY_ESCAPE)) { ed.name_dialog_open = 0; ed.ctx_open = 0; ed.active_textbox = -1; }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) run_name_dialog(&ed);
        } else if (ed.folder_picker_open) {
            if (ed.active_textbox == 2) textbox_process(ed.folder_picker_path, sizeof(ed.folder_picker_path), &ed.text_caret_folder, &ed.text_sel_folder);
            if (IsKeyPressed(KEY_ESCAPE)) { ed.folder_picker_open = 0; ed.active_textbox = -1; }
        } else if (ed.project_dialog_open) {
            ide_new_project_input(&ed, ctrl);
        } else if (ed.help_open) {
            if (IsKeyPressed(KEY_ESCAPE)) ed.help_open = 0;
        } else if (ed.warning_open) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                ed.warning_open = 0;
            }
        } else {
            if (editor_active_pane_has_tab(&ed)) {
                editor_process_typing(&ed, ctrl, shift);
            }

            /* Auto-completion opens ONLY on a deliberate trigger: a typed '.' or
               Ctrl+Space. It never opens by itself while typing ordinary letters. */

            /* Ctrl+MouseWheel zoom (code editor only) */
            if (ctrl) {
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    ed.editor_font += (int)mw;
                    if (ed.editor_font < 8) ed.editor_font = 8;
                    if (ed.editor_font > 32) ed.editor_font = 32;
                    g_line_h = ed.editor_font + 2;
                }
            } else if (shift) {
                /* Shift+MouseWheel horizontal scroll */
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    int step_chars = 8;
                    if (mw > 0) {
                        CUR.scroll_x = CUR.scroll_x > (size_t)step_chars ? CUR.scroll_x - (size_t)step_chars : 0;
                    } else {
                        CUR.scroll_x += (size_t)step_chars;
                    }
                }
            } else {
                /* Normal MouseWheel: vertical scroll (scroll_y changes, not the cursor).
                   In split mode, wheel over the tab strip belongs to the tabbar, not the active code editor. */
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    if (ed.split_enabled && mouse_over_split_tabbar) {
                        /* tab bar handles its own wheel scrolling; editor scroll is intentionally skipped */
                    } else if (ed.output_visible && CheckCollisionPointRec(GetMousePosition(), output_rect)) {
                        int out_line_h = ed.editor_font + 2;
                        int out_vis = (int)((output_rect.height - 28) / out_line_h);
                        if (out_vis < 1) out_vis = 1;
                        int total_out = 0;
                        for (char *s = ed.output; *s; ) {
                            char *nl = strchr(s, '\n');
                            if (!nl) { total_out++; break; }
                            total_out++;
                            s = nl + 1;
                        }
                        int max_out = total_out > out_vis ? total_out - out_vis : 0;
                        if (mw > 0) {
                            if (ed.output_scroll < max_out) ed.output_scroll++;
                        } else {
                            if (ed.output_scroll > 0) ed.output_scroll--;
                        }
                    } else {
                        /* The wheel scrolls the pane UNDER THE POINTER, not the focused
                           pane: over the unfocused pane it used to scroll the focused one
                           instead, so the two panes fought over one event (todo bug #4/#24). */
                        GclIdeBuffer *wheel_buf = &CUR;
                        if (ed.split_enabled) {
                            Vector2 wmp = GetMousePosition();
                            int over_left = CheckCollisionPointRec(wmp, split_left_rect)
                                            && !CheckCollisionPointRec(wmp, split_right_rect);
                            int over_right = CheckCollisionPointRec(wmp, split_right_rect)
                                             && !CheckCollisionPointRec(wmp, split_left_rect);
                            if (over_left) wheel_buf = editor_pane_cur(&ed, 0);
                            else if (over_right) wheel_buf = editor_pane_cur(&ed, 1);
                        }
                        size_t total_lines = gcl_ide_buffer_line_count(wheel_buf);
                        size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
                        if (vis < 1) vis = 1;
                        if (mw > 0) {
                            if (wheel_buf->scroll_y > 0) wheel_buf->scroll_y -= (size_t)mw;
                        } else {
                            size_t max_s = total_lines > vis ? total_lines - vis : 0;
                            if (wheel_buf->scroll_y + (size_t)(-mw) > max_s) wheel_buf->scroll_y = max_s;
                            else wheel_buf->scroll_y += (size_t)(-mw);
                        }
                    }
                }
            }

            if (editor_active_pane_has_tab(&ed) && ed.edited_this_frame) {
                GclIdeBuffer *effect_buf = editor_cur(&ed);
                Rectangle effect_rect = editor_rect;
                if (ed.split_enabled) {
                    effect_rect = (ed.split_focus == 0) ? split_left_rect : split_right_rect;
                    effect_buf = editor_cur(&ed);
                }
                size_t cl = gcl_ide_buffer_line_of_cursor(effect_buf);
                size_t cc = gcl_ide_buffer_cursor_in_line(effect_buf);
                int ppx = (int)effect_rect.x + GUTTER_W;
                int ppy = (int)effect_rect.y + 4 + (int)(cl - effect_buf->scroll_y) * LINE_H + LINE_H / 2;
                if (cc > 0) {
                    size_t cll = 0;
                    const char *cll_str = gcl_ide_buffer_line_at(effect_buf, cl, &cll);
                    size_t cpc = cc;
                    if (cpc > cll) cpc = cll;
                    char cpre[4096];
                    if (cpc > sizeof(cpre)-1) cpc = sizeof(cpre)-1;
                    memcpy(cpre, cll_str, cpc); cpre[cpc] = '\0';
                    ppx += MeasureText(cpre, ed.editor_font);
                }
                if (ed.particles) editor_spawn_particles(&ed, (float)ppx, (float)ppy, ed.particle_strength);
                if (ed.screen_shake) {
                    float inten = (float)ed.shake_strength / 100.0f;
                    if (inten < 0.1f) inten = 0.1f;
                    ed.shake_remaining = 0.15f;
                    ed.shake_duration = 0.15f;
                    ed.shake_intensity = 6.0f * inten;
                }
                ed.edited_this_frame = 0;
            }

            if (ctrl && IsKeyPressed(KEY_N)) tab_add(&ed, NULL);
            if (ctrl && IsKeyPressed(KEY_O)) {
                char dir[2048];
                editor_get_active_dir(&ed, dir, sizeof(dir));
                char *p = gcl_ide_native_dialog(0, dir);
                if (p) { tab_add(&ed, p); free(p); }
            }
            if (ctrl && IsKeyPressed(KEY_S)) {
                GclIdeBuffer *b = &CUR;
                if (b->path) gcl_ide_buffer_save(b);
                else {
                    char dir[2048];
                    editor_get_active_dir(&ed, dir, sizeof(dir));
                    char *p = gcl_ide_native_dialog(1, dir);
                    if (p) { free(b->path); b->path = gcl_strdup(p); gcl_ide_buffer_save(b); free(p); }
                }
            }
            if (ctrl && IsKeyPressed(KEY_C)) editor_copy(&ed);
            if (ctrl && IsKeyPressed(KEY_X)) editor_cut(&ed);
            if (ctrl && IsKeyPressed(KEY_V)) editor_paste(&ed);
            if (ctrl && IsKeyPressed(KEY_A)) editor_select_all(&ed);
            if (ctrl && IsKeyPressed(KEY_J)) ed.output_visible = !ed.output_visible;
            if (ctrl && IsKeyPressed(KEY_E)) {
                gcl_settings_panel_open(&ed);
            }
            if (shift && IsKeyPressed(KEY_SPACE)) ed.sidebar_visible = !ed.sidebar_visible;

            /* Run: F5 or Ctrl+R */
            if (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_R))) editor_run_program(&ed);

            /* Undo: Ctrl+Z  Redo: Ctrl+Y */
            if (ctrl && IsKeyPressed(KEY_Z)) gcl_ide_buffer_undo(&CUR);
            if (ctrl && IsKeyPressed(KEY_Y)) gcl_ide_buffer_redo(&CUR);

            /* Ctrl+Space: FORCE the auto-completion window open (manual) */
            if (ctrl && IsKeyPressed(KEY_SPACE)) {
                ed.completion_dismissed = 0;
                editor_show_completion(&ed, 1);
            }

            /* ---- ESC: tamamlama kaplamalarini kapat ----
               Popup ACIKKEN de, yalnizca seritler gorunurken de calisir.
               Kok neden: imza seridi (complete_draw_signature_help) ve tan seridi
               (complete_draw_diagnostic) popup'tan BAGIMSIZ cizilir; ikisi de
               yalnizca kendi bayraklarina bakar (completion_have_sig /
               completion_have_diag). Bu bayraklari yalnizca bir sonraki
               editor_show_completion() sifirlar.
               Popup bircok yoldan kapatilabiliyor (fonksiyon kabulu, ';',
               aday olmayan Tab) ve bu yollar bayraklari temizlemiyor. Eskiden
               ESC yalnizca "popup acikken" dalinda isleniyordu; popup kapali
               ama serit ekranda kalinca ESC hicbir sey yapmiyordu -> kullanici
               "parametre gosteren pencereyi kapatamiyorum" diyordu.
               Artik tek yerde, her durumda: popup + imza + tani birlikte kapanir
               ve yeni bir bilincli tetikleyene kadar kapali kalir
               (completion_dismissed). Seritler sonraki tetikleyicide yeniden hesaplanir. */
            if (IsKeyPressed(KEY_ESCAPE) &&
                (ed.completion_visible || ed.completion_have_sig || ed.completion_have_diag)) {
                ed.completion_visible = 0;
                ed.completion_no_match = 0;
                ed.completion_message[0] = '\0';
                ed.completion_have_sig = 0;
                ed.completion_sig_label[0] = '\0';
                ed.completion_sig_params[0] = '\0';
                ed.completion_sig_active = 0;
                ed.completion_have_diag = 0;
                ed.completion_diag[0] = '\0';
                ed.completion_dismissed = 1;
            }

            if (editor_active_pane_has_tab(&ed) && !(ed.completion_visible && (ed.completion_count > 0 || ed.completion_no_match))) {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    /* New line + automatic block indentation (Python/GCL):
                       after a line ending with ':' one indent unit is added. */
                    editor_insert_newline_autoindent(&ed);
                    if (ed.typewriter) editor_typewriter_sound(&ed, 1); /* Enter: strong thud */
                    editor_spawn_particles_at_cursor(&ed, ed.split_enabled ? (ed.split_focus == 0 ? split_left_rect : split_right_rect) : editor_rect);
                }
                /* Tab: if there is a selection, indent the selected lines; otherwise it adds indentation at the cursor
                   (uses '\t' when the file uses tabs for indentation, otherwise 4 spaces). Shift+Tab decreases indentation. */
                if (IsKeyPressed(KEY_TAB)) {
                    if (selection_active(&ed)) editor_indent_selection(&ed, shift ? 1 : 0);
                    else if (shift) editor_indent_selection(&ed, 1);
                    else editor_insert_tab(&ed);
                }
                /* Backspace/Delete: hold-to-repeat (no more mashing the key). */
                if (key_repeat_fire(KEY_BACKSPACE, 0.40, 0.03)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); }
                    else { gcl_ide_buffer_backspace(&CUR); }
                    CUR.sel_anchor = CUR.cursor;
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2); /* Backspace: high-pitched pop */
                    editor_spawn_particles_at_cursor(&ed, ed.split_enabled ? (ed.split_focus == 0 ? split_left_rect : split_right_rect) : editor_rect);
                }
                if (key_repeat_fire(KEY_DELETE, 0.40, 0.03)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); }
                    else { gcl_ide_buffer_delete(&CUR); }
                    CUR.sel_anchor = CUR.cursor;
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2); /* Delete: high-pitched pop */
                    editor_spawn_particles_at_cursor(&ed, ed.split_enabled ? (ed.split_focus == 0 ? split_left_rect : split_right_rect) : editor_rect);
                }
                /* Arrow keys: hold-to-repeat support (key repeat). */
                int nav_key = 0;
                if (IsKeyPressed(KEY_LEFT) || IsKeyDown(KEY_LEFT)) nav_key = KEY_LEFT;
                else if (IsKeyPressed(KEY_RIGHT) || IsKeyDown(KEY_RIGHT)) nav_key = KEY_RIGHT;
                else if (IsKeyPressed(KEY_UP) || IsKeyDown(KEY_UP)) nav_key = KEY_UP;
                else if (IsKeyPressed(KEY_DOWN) || IsKeyDown(KEY_DOWN)) nav_key = KEY_DOWN;
                double now_t = GetTime();
                if (nav_key != 0) {
                    bool first_press = IsKeyPressed(nav_key);
                    if (first_press || ed.nav_repeat_key != nav_key) {
                        ed.nav_repeat_key = nav_key;
                        ed.nav_repeat_time = now_t;
                        editor_nav_step(&ed, nav_key, shift);
                        ed.nav_repeat_time = now_t + 0.40;  /* start repeating after 400ms */
                    } else if (now_t >= ed.nav_repeat_time) {
                        editor_nav_step(&ed, nav_key, shift);
                        ed.nav_repeat_time = now_t + 0.03;  /* ~30ms rate */
                    }
                } else {
                    ed.nav_repeat_key = 0;
                    ed.nav_repeat_time = 0.0;
                }
                if (IsKeyPressed(KEY_HOME)) { gcl_ide_buffer_cursor_line_start(&CUR); if (!shift) CUR.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_END)) { gcl_ide_buffer_cursor_line_end(&CUR); if (!shift) CUR.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_PAGE_UP)) { for (int i=0;i<20;i++) gcl_ide_buffer_cursor_up(&CUR); if (!shift) CUR.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_PAGE_DOWN)) { for (int i=0;i<20;i++) gcl_ide_buffer_cursor_down(&CUR); if (!shift) CUR.sel_anchor = CUR.cursor; }
            } else {
                /* auto-completion navigation */
                if (ctrl && IsKeyPressed(KEY_SPACE)) { ed.completion_dismissed = 0; editor_show_completion(&ed, 1); }
                if (IsKeyPressed(KEY_UP)) { if (ed.completion_selected > 0) ed.completion_selected--; }
                if (IsKeyPressed(KEY_DOWN)) { if (ed.completion_selected < ed.completion_count - 1) ed.completion_selected++; }
                /* ESC is handled globally above this if/else — it must also work
                   when the popup is closed but a strip is still on screen. */
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_TAB)) {
                    /* Tab/Enter accepts a real candidate. If there is no candidate
                       (only the "there is no 'X'" warning is shown), Tab is not swallowed — it performs normal indentation (todo #1: "tab not working"). */
                    if (ed.completion_count > 0) {
                        editor_accept_completion(&ed);
                    } else if (IsKeyPressed(KEY_TAB)) {
                        ed.completion_visible = 0;
                        ed.completion_no_match = 0;
                        ed.completion_message[0] = '\0';
                        if (selection_active(&ed)) editor_indent_selection(&ed, shift ? 1 : 0);
                        else if (shift) editor_indent_selection(&ed, 1);
                        else editor_insert_tab(&ed);
                    }
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); }
                    else { gcl_ide_buffer_backspace(&CUR); }
                    CUR.sel_anchor = CUR.cursor;
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2);
                    editor_spawn_particles_at_cursor(&ed, ed.split_enabled ? (ed.split_focus == 0 ? split_left_rect : split_right_rect) : editor_rect);
                    editor_show_completion(&ed, 0);
                }
                if (IsKeyPressed(KEY_DELETE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); }
                    else { gcl_ide_buffer_delete(&CUR); }
                    CUR.sel_anchor = CUR.cursor;
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2);
                    editor_spawn_particles_at_cursor(&ed, ed.split_enabled ? (ed.split_focus == 0 ? split_left_rect : split_right_rect) : editor_rect);
                    editor_show_completion(&ed, 0);
                }
            }

            if (IsFileDropped()) {
                FilePathList fpl = LoadDroppedFiles();
                for (unsigned int i = 0; i < fpl.count; i++) {
                    const char *src = fpl.paths[i];
                    char dst[8192];
                    snprintf(dst, sizeof(dst), "%s/%s", ed.cwd, path_basename(src));
                    if (fs_is_dir(src)) fs_copy_dir_rec(src, dst);
                    else fs_copy_file(src, dst);
                }
                UnloadDroppedFiles(fpl);
                ed.last_tree_scan = 0;  /* deferred rescan */
            }
        }

        /* ---- draw ---- */
        BeginDrawing();

        /* screen shake — ease-out (ridiculous_coding): amplitude decreases with remaining time */
        if (ed.screen_shake && ed.shake_remaining > 0.0f) {
            float t = ed.shake_remaining / (ed.shake_duration > 0.0f ? ed.shake_duration : 1.0f);
            float mag = ed.shake_intensity * t * t; /* ease-out quadratic */
            ed.shake_x = ((float)(rand() % 200) - 100.0f) / 100.0f * mag;
            ed.shake_y = ((float)(rand() % 200) - 100.0f) / 100.0f * mag;
            rlPushMatrix();
            rlTranslatef(ed.shake_x, ed.shake_y, 0.0f);
        }

        /* menu bar */
        DrawRectangleRec(menu_rect, t.menu_bg);
        {
            float mx0 = 8;
            for (int i = 0; g_menu_titles[i]; i++) {
                int tw = MeasureText(g_menu_titles[i], font_sz);
                Rectangle r = { mx0, 2, (float)(tw + 16), (float)(MENU_H - 4) };
                bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                DrawText(g_menu_titles[i], (int)(mx0 + 8), 6, font_sz,
                         (hover || ed.menu_open == i + 1) ? t.accent : t.text);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ed.settings_open && !ed.name_dialog_open && !ed.folder_picker_open && !ed.about) {
                    ed.menu_open = (ed.menu_open == i + 1) ? 0 : (i + 1);
                }
                mx0 += tw + 20;
            }
            /* project name top-right */
            if (ed.current_project[0]) {
                const char *pname = path_basename(ed.current_project);
                int pw = MeasureText(pname, font_sz);
                DrawText(pname, w - pw - 12, 6, font_sz, t.text);
            }
        }

        /* explorer */
        if (ed.sidebar_visible) {
            DrawRectangleRec(sidebar_rect, t.tree_bg);
            const char *hdr = "EXPLORER";
            DrawText(hdr, (int)(sidebar_rect.x + 8), (int)(sidebar_rect.y + 6), font_sz, t.accent);
            DrawRectangle((int)sidebar_rect.x, (int)(sidebar_rect.y + 28), (int)sidebar_rect.width, 1, t.accent);

            int item_h = 18;
            Rectangle list_rect = { sidebar_rect.x + 4, sidebar_rect.y + 32, sidebar_rect.width - 8, sidebar_rect.height - 40 };
            BeginScissorMode((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height);
            int total = ed.tree.count;
            for (int i = ed.tree_scroll; i < total; i++) {
                int y = (int)list_rect.y + (i - ed.tree_scroll) * item_h;
                if (y + item_h > (int)(list_rect.y + list_rect.height)) break;
                TreeNode *node = &ed.tree.nodes[i];
                const char *bn = path_basename(node->path);
                int indent = node->depth * 14;
                if (i == ed.tree_selected) DrawRectangle((int)list_rect.x, y, (int)list_rect.width, item_h, t.selection);
                if (node->is_dir) {
                    if (is_expanded(&ed, node->path)) DrawText("v", (int)(list_rect.x + 4 + indent), y + 2, font_sz - 2, t.text);
                    else DrawText(">", (int)(list_rect.x + 4 + indent), y + 2, font_sz - 2, t.text);
                    DrawText(bn, (int)(list_rect.x + 18 + indent), y + 2, font_sz - 2, t.text);
                } else {
                    DrawRectangle((int)(list_rect.x + 4 + indent), y + 6, 8, 6, t.accent);
                    DrawText(bn, (int)(list_rect.x + 18 + indent), y + 2, font_sz - 2, i == ed.tree_selected ? WHITE : t.text);
                }
            }
            EndScissorMode();

            /* tree click handling - does not run while the context menu is open */
            if (!blocked && !ed.ctx_open) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    Vector2 mp = GetMousePosition();
                    if (CheckCollisionPointRec(mp, list_rect)) {
                        int row = ed.tree_scroll + (int)((mp.y - list_rect.y) / item_h);
                        if (row >= 0 && row < total) {
                            ed.tree_selected = row;
                            TreeNode *node = &ed.tree.nodes[row];
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                if (node->is_dir) {
                                    if (is_expanded(&ed, node->path)) set_expanded(&ed, node->path, 0);
                                    else set_expanded(&ed, node->path, 1);
                                    ed.last_tree_scan = 0;  /* deferred rescan */
                                } else {
                                    tab_add(&ed, node->path);
                                }
                            } else {
                                ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                                ed.ctx_target = row; ed.ctx_item = CTX_NEW_FILE;
                            }
                        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                            ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                            ed.ctx_target = -1; ed.ctx_item = CTX_NEW_FILE;
                        }
                    } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, sidebar_rect)) {
                        ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                        ed.ctx_target = -1; ed.ctx_item = CTX_NEW_FILE;
                    }
                }
            }

            /* explorer context menu */
            if (ed.ctx_open) {
                /* Ordering must match the CtxItem enum in gcl_ide_internal.h exactly:
                   New File, New Directory, Rename, Copy, Paste, Delete,
                   Copy Path, Copy Name (todo #5). */
                const char *items[] = { "New File", "New Directory", "Rename", "Copy", "Paste", "Delete", "Copy Path", "Copy Name", NULL };
                int iw = 160, ih = 24, n = 8;
                int mxm = ed.ctx_x, mym = ed.ctx_y;
                if (mxm + iw > w) mxm = w - iw;
                if (mym + n * ih > h) mym = h - n * ih;
                DrawRectangle(mxm, mym, iw, n * ih, t.popup_bg);
                DrawRectangleLines((float)mxm, (float)mym, (float)iw, (float)(n * ih), t.accent);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 mp = GetMousePosition();
                    if (!CheckCollisionPointRec(mp, (Rectangle){(float)mxm, (float)mym, (float)iw, (float)(n * ih)})) ed.ctx_open = 0;
                    else {
                        int idx = (int)((mp.y - mym) / ih);
                        if (idx >= 0 && idx < n) { ed.ctx_item = idx; ctx_execute(&ed); }
                    }
                }
                for (int i = 0; i < n; i++) {
                    int y = mym + i * ih;
                    Rectangle it = { (float)mxm, (float)y, (float)iw, (float)ih };
                    bool hover = CheckCollisionPointRec(GetMousePosition(), it);
                    if (hover && ed.ctx_item == i) DrawRectangle(mxm, y, iw, ih, t.accent);
                    DrawText(items[i], mxm + 6, y + 4, font_sz - 2, hover ? WHITE : t.text);
                }
            }
        }

        /* tab bar - above the editor area; ignore clicks while a menu popup is open.
           While a modal (settings, build, project, etc.) is open, clicks are swallowed —
           otherwise a click landing on the settings dialog also triggers the tab button underneath. */
        /* Tamamlama popup'inin cizilecegi ALAN. Popup cizimi bu blogun DISINDA
           yapildigi icin, split'te secilen pane'in kod alani buraya tasinir.
           Eskiden popup YALNIZCA split kapaliyken ciziliyordu
           (`if (!ed.split_enabled) ...`), bu yuzden split gorunumunde —
           yani "hangi kod kutusu aktifse orada" beklendigi durumda —
           tamamlama penceresi HIC gorunmuyordu. */
        Rectangle popup_area = editor_rect;
        if (ed.split_enabled) {
            float left_w = panes_rect.width * ed.split_ratio;
            float divider_w = 6.0f;
            Rectangle pane_left = { panes_rect.x, panes_rect.y, left_w, panes_rect.height };
            Rectangle pane_right = { panes_rect.x + left_w + divider_w, panes_rect.y, panes_rect.width - left_w - divider_w, panes_rect.height };
            Rectangle divider = { panes_rect.x + left_w, panes_rect.y, divider_w, panes_rect.height };
            Rectangle left_tabbar = { pane_left.x + 4.0f, pane_left.y + 4.0f, pane_left.width - 8.0f, (float)TAB_H };
            Rectangle right_tabbar = { pane_right.x + 4.0f, pane_right.y + 4.0f, pane_right.width - 8.0f, (float)TAB_H };
            Rectangle left_code = { pane_left.x + 4.0f, pane_left.y + TAB_H + 6.0f, pane_left.width - 8.0f, pane_left.height - TAB_H - 10.0f };
            Rectangle right_code = { pane_right.x + 4.0f, pane_right.y + TAB_H + 6.0f, pane_right.width - 8.0f, pane_right.height - TAB_H - 10.0f };
            /* Popup, ODAKLI pane'in kod alanina hizalanir (kod alani = kendi tab
               seridinin alti). Boylece popup hangi kod kutusu aktifse orada,
               imlecin hemen yanindaki dogru konumda acilir. */
            popup_area = (ed.split_focus == 0) ? left_code : right_code;

            DrawRectangleRec(pane_left, t.bg);
            DrawRectangleRec(pane_right, t.bg);
            DrawRectangleRec(left_tabbar, t.menu_bg);
            DrawRectangleRec(right_tabbar, t.menu_bg);
            if (!ed.menu_open && !blocked) {
                draw_pane_tabbar(&ed, left_tabbar, 0, &t, font_sz, blocked);
                draw_pane_tabbar(&ed, right_tabbar, 1, &t, font_sz, blocked);
            }
            DrawRectangleRec(divider, t.panel_bg);
            DrawRectangleLinesEx(divider, 1.0f, t.accent);
            draw_single_editor_pane(&ed, left_code, &t, blocked, ed.editor_font, 0);
            draw_single_editor_pane(&ed, right_code, &t, blocked, ed.editor_font, 1);
        } else {
            DrawRectangleRec(tabbar_rect, t.menu_bg);
            if (!ed.menu_open && !blocked) {
                float tx = tabbar_rect.x + 6;
                for (int i = 0; i < ed.tab_count; i++) {
                    const char *name = ed.tabs[i].path ? path_basename(ed.tabs[i].path) : "untitled";
                    int tw = MeasureText(name, font_sz - 2) + 36;
                    Rectangle tr = { tx, tabbar_rect.y + 2, (float)tw, (float)(TAB_H - 4) };
                    bool active = (i == ed.active_tab);
                    DrawRectangleRec(tr, active ? t.panel_bg : t.menu_bg);
                    DrawRectangleLinesEx(tr, 1, active ? t.accent : t.gutter);
                    DrawText(name, (int)(tx + 8), (int)(tabbar_rect.y + 6), font_sz - 2, active ? t.text : t.gutter);
                    Rectangle close = { tx + tw - 20, tabbar_rect.y + 6, 14, 14 };
                    if (CheckCollisionPointRec(GetMousePosition(), close)) {
                        DrawText("x", (int)(close.x + 2), (int)(close.y), font_sz - 2, t.error);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { tab_close(&ed, i); break; }
                    }
                    if (CheckCollisionPointRec(GetMousePosition(), tr) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (!CheckCollisionPointRec(GetMousePosition(), close)) tab_activate(&ed, i);
                    }
                    tx += tw + 4;
                }
                Rectangle plus = { tx + 2, tabbar_rect.y + 2, 24, (float)(TAB_H - 4) };
                DrawText("+", (int)(plus.x + 6), (int)(plus.y + 6), font_sz, t.text);
                if (CheckCollisionPointRec(GetMousePosition(), plus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) tab_add(&ed, NULL);
            }
            draw_single_editor_pane(&ed, editor_rect, &t, blocked, ed.editor_font, 0);
        }

        /* completion window + signature help (ide_complete_ui.c, §13) */
        /* Popup cizimi — split ACIKKEN de. Hedef alan, odakli pane'in kod
           alanidir; icerik zaten `editor_cur(ed)` (odakli pane'in secili
           sekmesi) uzerinden hesaplandigi icin cizim pane ile tutarlidir. */
        /* UI dogrulama kancasi (GCL_IDE_COMPLETE=1): tamamlama penceresini
           programatik olarak acar. GCL_IDE_SHOT ile birlikte kullanildiginda
           popup'in GERCEKTEN cizildigini — ozellikle SPLIT gorunumunde, yani
           odakli kod kutusunda — kanitlar (tikleme otomasyonu olmadigi icin,
           bkz. todo OPEN #1). Imlec tamponun SONUNA alinir ki baglam
           belirleyici olsun (dosya "Raylib." ile bitiyorsa uye listesi acilir). */
        if (g_complete_hook) {
            g_complete_hook = 0;
            GclIdeBuffer *hb = editor_cur(&ed);
            if (hb && hb->size > 0) {
                /* Sondaki bosluk/satir sonlarini atla: imlec metnin SON
                   KARAKTERINDEN sonra dursun (dosya "Raylib." ile bitiyorsa
                   bos bir satira degil, dogrudan uye erisimi konumuna duser). */
                size_t pos = hb->size;
                while (pos > 0) {
                    char pc = hb->content[pos - 1];
                    if (pc == '\n' || pc == '\r' || pc == ' ' || pc == '\t') pos--;
                    else break;
                }
                hb->cursor = pos;
                hb->sel_anchor = hb->cursor;
            }
            ed.completion_dismissed = 0;
            editor_show_completion(&ed, 1);
            printf("[gcl] complete-hook: split=%d focus=%d visible=%d count=%d no_match=%d area=%.0f,%.0f %.0fx%.0f\n",
                   ed.split_enabled, ed.split_focus, ed.completion_visible,
                   ed.completion_count, ed.completion_no_match,
                   (double)popup_area.x, (double)popup_area.y,
                   (double)popup_area.width, (double)popup_area.height);
        }
        ide_complete_ui_draw(&ed, popup_area, ed.editor_font, &t);

        /* name dialog (New File / New Directory / Rename) — modal drawing.
           NOTE: input handling exists (lines 299-302) but there was NO drawing code.
           Because blocked=true, tab/editor were also not drawn -> the screen went blank. */
        if (ed.name_dialog_open) {
            /* dim overlay */
            DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 200 });
            int dw = 360, dh = 140;
            int dx = (w - dw) / 2, dy = (h - dh) / 2;
            if (dx < 0) dx = 0;
            if (dy < 0) dy = 0;
            DrawRectangleRec((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh }, t.popup_bg);
            DrawRectangleLinesEx((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh }, 2.0f, t.accent);
            const char *ndt = (ed.name_dialog_mode == 2) ? "Rename" :
                              (ed.name_dialog_mode == 1 ? "New Directory" : "New File");
            DrawText(ndt, dx + 16, dy + 12, font_sz, t.accent);
            Rectangle nd_tb = { (float)(dx + 16), (float)(dy + 44), (float)(dw - 32), 30 };
            ui_label_textbox(nd_tb, ed.name_dialog_input, sizeof(ed.name_dialog_input),
                             ed.active_textbox == 1, &ed.text_caret_name, &ed.text_sel_name, &t, font_sz);
            Rectangle nd_ok = { (float)(dx + dw - 160), (float)(dy + dh - 48), 66, 30 };
            Rectangle nd_cc = { (float)(dx + dw - 86), (float)(dy + dh - 48), 66, 30 };
            if (ui_button(nd_ok, "OK", &t, font_sz)) {
                run_name_dialog(&ed);
            }
            if (ui_button(nd_cc, "Cancel", &t, font_sz)) {
                ed.name_dialog_open = 0;
                ed.ctx_open = 0;
                ed.active_textbox = -1;
            }
        }

        /* settings / about modals — because `blocked=true`, the editor is not drawn;
           without these panels the screen would stay black. */
        if (ed.project_dialog_open) ide_new_project_draw(&ed, w, h, font_sz, &t);
        if (ed.settings_open) gcl_settings_panel_draw(&ed, w, h, font_sz, &t);
        if (ed.about) draw_about_panel(&ed, w, h, font_sz, &t);

        /* output panel — hide while a modal (New Project, etc.) is open: the modal must always stay on top.
           Otherwise the output panel would be drawn on top of the modal, leaving the modal underneath. */
        if (ed.output_visible && !blocked) {
            DrawRectangleRec(output_rect, t.popup_bg);
            DrawRectangleLinesEx(output_rect, 1.0f, t.accent);
            DrawText("Output", (int)(output_rect.x + 8), (int)(output_rect.y + 4), font_sz - 2, t.accent);
            BeginScissorMode((int)output_rect.x, (int)output_rect.y + 22, (int)output_rect.width, (int)output_rect.height - 22);
            int oy = (int)output_rect.y + 26;
            int line_h = font_sz + 2;
            int out_vis = (int)((output_rect.height - 28) / line_h);
            if (out_vis < 1) out_vis = 1;
            int total_out = 0;
            for (char *s = ed.output; *s; ) {
                char *nl = strchr(s, '\n');
                if (!nl) { total_out++; break; }
                total_out++;
                s = nl + 1;
            }
            int max_out = total_out > out_vis ? total_out - out_vis : 0;
            if (ed.output_scroll < 0) ed.output_scroll = 0;
            if (ed.output_scroll > max_out) ed.output_scroll = max_out;
            int start_line = max_out - ed.output_scroll;
            char *line_start = ed.output;
            int skip = start_line;
            while (skip > 0 && *line_start) {
                char *nl = strchr(line_start, '\n');
                if (!nl) break;
                line_start = nl + 1;
                skip--;
            }
            while (*line_start && oy < (int)(output_rect.y + output_rect.height - 6)) {
                char *nl = strchr(line_start, '\n');
                size_t llen = nl ? (size_t)(nl - line_start) : strlen(line_start);
                char linebuf[4096];
                if (llen > sizeof(linebuf) - 1) llen = sizeof(linebuf) - 1;
                memcpy(linebuf, line_start, llen); linebuf[llen] = '\0';
                /* Strip the trailing CR of CRLF output. The embedded mono font has
                   no glyph for '\r', so a leftover CR used to be drawn as '?' at
                   the end of every output line (todo item 11). */
                while (llen > 0 && (linebuf[llen - 1] == '\r' || linebuf[llen - 1] == '\n')) {
                    linebuf[--llen] = '\0';
                }
                DrawText(linebuf, (int)(output_rect.x + 8), oy, font_sz, t.text);
                oy += line_h;
                if (!nl) break;
                line_start = nl + 1;
            }
            EndScissorMode();
            if (max_out > 0) {
                float bar_h = (output_rect.height - 22) * ((float)out_vis / (float)total_out);
                float bar_y = (output_rect.y + 22) + ((output_rect.height - 22) - bar_h) * ((float)(max_out - ed.output_scroll) / (float)max_out);
                DrawRectangle((int)(output_rect.x + output_rect.width - 8), (int)bar_y, 6, (int)bar_h, t.gutter);
            }
        }

        /* status */
        DrawRectangleRec(status_rect, t.status_bg);
        char status[2300];
        if (ed.running) {
            const char *step_msg = ed.status_msg[0] ? ed.status_msg : "Building...";
            DrawText(step_msg, (int)(status_rect.x + 8), (int)(status_rect.y + 6), font_sz - 2, t.accent);
        } else {
            snprintf(status, sizeof(status), "GCL IDE | %s", ed.tab_count > 0 && ed.active_tab >= 0 ? (ed.tabs[ed.active_tab].path ? path_basename(ed.tabs[ed.active_tab].path) : "untitled") : "No file open");
            DrawText(status, (int)(status_rect.x + 8), (int)(status_rect.y + 6), font_sz - 2, t.text);
        }

        /* menu popup on top */
        if (ed.menu_open) {
            int mi = ed.menu_open - 1;
            int n = menu_item_count(mi);
            float menux = 8;
            for (int i = 0; i < mi; i++) menux += MeasureText(g_menu_titles[i], font_sz) + 20;
            int pw = 160;
            for (int i = 0; i < n; i++) {
                int tw = MeasureText(g_menu_items[mi][i], font_sz) + 16;
                if (tw > pw) pw = tw;
            }
            Rectangle pr = { menux, (float)MENU_H, (float)pw, (float)(n * 26 + 4) };
            DrawRectangleRec(pr, t.popup_bg);
            DrawRectangleLinesEx(pr, 1, t.accent);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mp = GetMousePosition();
                if (mp.y > MENU_H && !CheckCollisionPointRec(mp, pr)) ed.menu_open = 0;
            }
            for (int i = 0; i < n; i++) {
                int y = (int)pr.y + 2 + i * 26;
                Rectangle it = { pr.x + 2, (float)y, pr.width - 4, 26 };
                bool hover = CheckCollisionPointRec(GetMousePosition(), it);
                const char *label = g_menu_items[mi][i];
                if (mi == 1 && i == 2) {
                    label = ed.split_enabled ? "Split Off" : "Split On";
                }
                if (hover) DrawRectangleRec(it, t.selection);
                DrawText(label, (int)(it.x + 8), (int)(it.y + 4), font_sz, hover ? WHITE : t.text);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    MenuAction a = g_menu_actions[mi][i];
                    ed.menu_open = 0;
                    menu_action_run(&ed, a);
                }
            }
        }

        /* end the shake */
        if (ed.screen_shake && ed.shake_remaining > 0.0f) {
            rlPopMatrix();
        }

        /* particles — pixel-art square particles */
        {
            float pdt = GetFrameTime();
            editor_update_particles(&ed, pdt);
            for (int i = 0; i < ed.part_count; i++) {
                Particle *p = &ed.parts[i];
                if (p->life > 0) {
                    DrawRectangle((int)(p->x - p->size / 2.0f),
                                  (int)(p->y - p->size / 2.0f),
                                  (int)p->size, (int)p->size, p->color);
                }
            }
        }

        /* effect decay (ridiculous_coding): shake and pitch falloff */
        {
            float pdt = GetFrameTime();
            if (ed.shake_remaining > 0.0f) {
                ed.shake_remaining -= pdt;
                if (ed.shake_remaining < 0.0f) ed.shake_remaining = 0.0f;
            }
            if (ed.pitch_increase > 0.0f) {
                ed.pitch_increase -= 2.0f * pdt;
                if (ed.pitch_increase < 0.0f) ed.pitch_increase = 0.0f;
            }
        }

        /* VHS distortion — a softer version that preserves font readability */
        if (ed.vhs) {
            int base_a = 2 + (ed.vhs_strength * 18 / 100);
            int band_a = 4 + (ed.vhs_strength * 40 / 100);
            for (int i = 0; i < h / 6; i++) {
                int yy = i * 6;
                Color c = { 255, 255, 255, base_a };
                DrawRectangle(0, yy, w, 1, c);
            }
            int band_y = (int)(GetTime() * 60) % (h + 80) - 40;
            DrawRectangle(0, band_y, w, 8, (Color){ 255, 255, 255, band_a });
        }

        /* CRT — lighter scanline + corner darkening (readability first) */
        if (ed.crt) {
            int ca = 8 + (ed.crt_strength * 35 / 100);
            for (int yy = 2; yy < h; yy += 4) {
                DrawRectangle(0, yy, w, 1, (Color){ 0, 0, 0, ca });
            }
            int edge = 3 + (ed.crt_strength * 16 / 100);
            for (int i = 0; i < edge; i++) {
                int a = (int)(20 * (1.0f - (float)i / (float)edge));
                DrawRectangle(0, i, w, 1, (Color){ 0, 0, 0, a });
                DrawRectangle(0, h - 1 - i, w, 1, (Color){ 0, 0, 0, a });
                DrawRectangle(i, 0, 1, h, (Color){ 0, 0, 0, a });
                DrawRectangle(w - 1 - i, 0, 1, h, (Color){ 0, 0, 0, a });
            }
        }

        /* UI capture hook for automated verification: when GCL_IDE_SHOT names a
           .png file, the rendered window is saved once (a few frames in) and the
           IDE exits. Off unless the variable is set, so normal runs are
           unaffected. The renderer grabs its own framebuffer instead of the
           desktop, so the capture works even when the window is not on top. */
        {
            static int shot_frames = 0;
            const char *shot_path = getenv("GCL_IDE_SHOT");
            if (shot_path && shot_path[0] && ++shot_frames == SHOT_AFTER_FRAMES) {
                TakeScreenshot(shot_path);
                ed.quit = 1;
            }
        }
        EndDrawing();
    }

    /* Terminate the still-running external process (Run/Project run) when the IDE closes */
    gcl_ide_proc_kill(&ed);

    free(ed.tabs);
    free(ed.clip_text);
    CloseWindow();
    return 0;
}
