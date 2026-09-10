#include "gcl_ide_internal.h"

/* ---------------------------------------------
   Widgets
   --------------------------------------------- */

/* Repeated key handling for hold-to-repeat (key repeat) */
static void textbox_handle_key(int key, char *buf, size_t bufsz, size_t *caret, size_t *sel, bool shift) {
    (void)bufsz;
    size_t n = strlen(buf);
    size_t c = caret ? *caret : n;
    size_t s = sel ? *sel : c;
    size_t a = s < c ? s : c;
    size_t b = s < c ? c : s;
    switch (key) {
        case KEY_BACKSPACE:
            if (a != b) {
                memmove(buf + a, buf + b, n - b + 1);
                if (caret) *caret = a;
                if (sel) *sel = a;
            } else if (c > 0) {
                memmove(buf + c - 1, buf + c, n - c + 1);
                if (caret) (*caret)--;
                if (sel) *sel = *caret;
            }
            break;
        case KEY_DELETE:
            if (a != b) {
                memmove(buf + a, buf + b, n - b + 1);
                if (caret) *caret = a;
                if (sel) *sel = a;
            } else if (c < n) {
                memmove(buf + c, buf + c + 1, n - c);
            }
            break;
        case KEY_LEFT:
            if (caret && *caret > 0) (*caret)--;
            if (!shift && sel) *sel = *caret;
            break;
        case KEY_RIGHT:
            if (caret && *caret < n) (*caret)++;
            if (!shift && sel) *sel = *caret;
            break;
        default: break;
    }
}

void textbox_process(char *buf, size_t bufsz, size_t *caret, size_t *sel) {
    int ch;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int guard = 0;
    while ((ch = GetCharPressed()) != 0 && guard < 64) {
        guard++;
        if (ch == '\r' || ch == '\n') continue;
        size_t n = strlen(buf);
        if (caret && *caret > n) *caret = n;
        if (sel && *sel > n) *sel = n;
        size_t c = caret ? *caret : n;
        size_t s = sel ? *sel : c;
        size_t a = s < c ? s : c;
        size_t b = s < c ? c : s;
        if (a != b) {
            memmove(buf + a, buf + b, n - b + 1);
            if (caret) *caret = a;
            if (sel) *sel = a;
            n = strlen(buf);
            c = a;
        }
        if (n + 1 < bufsz) {
            memmove(buf + c + 1, buf + c, n - c + 1);
            buf[c] = (char)ch;
            if (caret) (*caret)++;
            if (sel) *sel = *caret;
        }
    }
    size_t n = strlen(buf);
    if (caret && *caret > n) *caret = n;
    if (sel && *sel > n) *sel = n;
    if (!caret || !sel) return;

    /* Hold-to-repeat (key repeat) support:
       Process immediately on the first press, wait 0.4s, then repeat every 0.03s. */
    static int rep_key = 0;
    static double rep_t = 0.0;
    double now = GetTime();
    int nav = 0;
    if (IsKeyDown(KEY_BACKSPACE)) nav = KEY_BACKSPACE;
    else if (IsKeyDown(KEY_DELETE)) nav = KEY_DELETE;
    else if (IsKeyDown(KEY_LEFT)) nav = KEY_LEFT;
    else if (IsKeyDown(KEY_RIGHT)) nav = KEY_RIGHT;

    if (nav != rep_key) {
        rep_key = nav;
        rep_t = now;
        if (nav) {
            textbox_handle_key(nav, buf, bufsz, caret, sel, shift);
            rep_t = now + 0.4;
        }
    } else if (nav && now >= rep_t) {
        textbox_handle_key(nav, buf, bufsz, caret, sel, shift);
        rep_t = now + 0.03;
    }

    if (IsKeyPressed(KEY_HOME)) { *caret = 0; if (!shift) *sel = 0; }
    if (IsKeyPressed(KEY_END)) {
        size_t nn = strlen(buf);
        *caret = nn;
        if (!shift) *sel = nn;
    }
}

int ui_button(Rectangle r, const char *label, const GclIdeTheme *t, int font_sz) {
    /* Auto-grow to fit the text width: shift x left so the right edge stays fixed. */
    int tw = MeasureText(label, font_sz);
    float needed = (float)(tw + 16);
    if (r.width < needed) {
        r.x -= (needed - r.width);
        r.width = needed;
    }
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    bool pressed = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color bg = pressed ? t->selection : (hover ? t->accent : t->panel_bg);
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1.0f, t->accent);
    DrawText(label, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - font_sz) / 2), font_sz,
             pressed ? WHITE : (hover ? WHITE : t->text));
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int ui_checkbox(Rectangle r, const char *label, int checked, const GclIdeTheme *t, int font_sz) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    Rectangle box = { r.x, r.y + (r.height - 16) / 2, 16, 16 };
    DrawRectangleRec(box, t->popup_bg);
    DrawRectangleLinesEx(box, 1.0f, hover ? t->accent : t->gutter);
    if (checked) {
        DrawRectangle((int)(box.x + 3), (int)(box.y + 3), (int)(box.width - 6), (int)(box.height - 6), t->accent);
    }
    DrawText(label, (int)(r.x + 22), (int)(r.y + (r.height - font_sz) / 2), font_sz,
             checked ? t->accent : t->text);
    return CheckCollisionPointRec(GetMousePosition(), box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int ui_slider(Rectangle r, const char *label, int *val, int min, int max, const GclIdeTheme *t, int font_sz) {
    /* label + value on the top line, track below and more prominent; label is 14px above the track */
    int ly = (int)r.y - font_sz - 14;
    DrawText(label, (int)r.x, ly, font_sz, t->text);
    char txt[16];
    snprintf(txt, sizeof(txt), "%d", *val);
    DrawText(txt, (int)(r.x + r.width - 38), ly, font_sz, t->accent);

    float track_h = 14.0f;
    Rectangle track = { r.x, r.y, r.width, track_h };
    DrawRectangleRec(track, t->popup_bg);
    DrawRectangleLinesEx(track, 1.0f, t->accent);

    float frac = (float)(*val - min) / (float)(max - min);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    float fill_w = (r.width - 8) * frac;
    DrawRectangle((int)(r.x + 4), (int)(r.y + track_h / 2 - 2), (int)fill_w, 4, t->accent);
    DrawRectangle((int)(r.x + 4 + fill_w - 4), (int)(r.y + 1), 8, (int)(track_h - 2), t->selection);

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){r.x, r.y - 2, r.width, track_h + 4}) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float pos = (GetMousePosition().x - r.x - 4) / (r.width - 8);
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;
        *val = min + (int)((max - min) * pos);
        if (*val < min) *val = min;
        if (*val > max) *val = max;
        return 1;
    }
    return 0;
}

int ui_label_textbox(Rectangle r, char *buf, size_t bufsz, int focused, size_t *caret, size_t *sel, const GclIdeTheme *t, int font_sz) {
    (void)bufsz;
    DrawRectangleRec(r, t->popup_bg);
    DrawRectangleLinesEx(r, focused ? 2.0f : 1.0f, focused ? t->accent : t->gutter);

    size_t n = strlen(buf);
    if (caret && *caret > n) *caret = n;
    if (sel && *sel > n) *sel = n;

    /* Embedded FreeMono monospace: measurement AND drawing use the same font (gcl_measure_text_f/gcl_draw_text_f).
       If raylib's default font is used, DrawText's advance does not match MeasureText
       and the path/input text drifts (the caret appears behind or elsewhere relative to the text).
       gcl_draw_text_f uses a fixed "M" step per character - matching the measurement exactly. */
    float cwf = gcl_measure_text_f("M", font_sz);
    if (cwf < 1.0f) cwf = 1.0f;
    int cw = (int)(cwf + 0.5f);
    if (cw < 1) cw = 1;
    int max_w = (int)r.width - 10;
    if (max_w < cw) max_w = cw;
    int max_chars = max_w / cw;
    if (max_chars < 1) max_chars = 1;

    /* Horizontal scroll: keep the caret always visible. */
    size_t cpos = focused && caret ? *caret : 0;
    if (cpos > n) cpos = n;
    int start = 0;
    if ((int)cpos > max_chars) start = (int)cpos - max_chars;

    /* Selection drawing (within the visible area). */
    if (focused && caret && sel) {
        size_t s0 = *sel < *caret ? *sel : *caret;
        size_t s1 = *sel < *caret ? *caret : *sel;
        if (s0 < s1) {
            int v0 = (int)s0 > start ? (int)s0 - start : 0;
            int v1 = (int)s1 > start ? (int)s1 - start : 0;
            if (v1 > max_chars) v1 = max_chars;
            if (v0 < v1) {
                int sx = (int)(r.x + 6) + v0 * cw;
                int ex = (int)(r.x + 6) + v1 * cw;
                DrawRectangle(sx, (int)(r.y + 4), ex - sx, (int)(r.height - 8), t->selection);
            }
        }
    }

    /* Draw text (from the scrolled start) - embedded FreeMono monospace.
       gcl_draw_text_f draws with a fixed cw step per character; using it instead of
       DrawText makes caret/text alignment match exactly.
       CRITICAL: CLIP the text area with scissor. Otherwise long path/input text
       overflows the right edge of the box - the cause of the "text overflows the box" look. */
    BeginScissorMode((int)(r.x + 6), (int)(r.y + 2), (int)(r.width - 12), (int)(r.height - 4));
    gcl_draw_text_f(buf + start, (float)(r.x + 6), (int)(r.y + (r.height - font_sz) / 2), font_sz, t->text);
    EndScissorMode();

    /* Draw the caret. */
    if (focused && caret) {
        int cc = (int)*caret - start;
        if (cc < 0) cc = 0;
        if (cc > max_chars) cc = max_chars;
        int cx = (int)(r.x + 6) + cc * cw;
        if (cx > (int)(r.x + r.width - 4)) cx = (int)(r.x + r.width - 4);
        DrawRectangle(cx, (int)(r.y + 4), 2, (int)(r.height - 8), t->cursor);
    }

    /* Mouse click: caret relative to the visible area. */
    if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int rel = (int)(GetMousePosition().x - ((float)r.x + 6));
        if (rel < 0) rel = 0;
        size_t col = (size_t)(rel / cw) + (size_t)start;
        if (col > n) col = n;
        if (caret) *caret = col;
        if (sel) *sel = col;
        return 1;
    }
    return 0;
}

void draw_panel_frame(Rectangle r, const char *title, const GclIdeTheme *t, int font_sz) {
    DrawRectangleRec(r, t->panel_bg);
    DrawRectangleLinesEx(r, 1.0f, t->accent);
    DrawRectangleRec((Rectangle){r.x + 1, r.y + 1, r.width - 2, 28}, t->popup_bg);
    DrawText(title, (int)(r.x + 12), (int)(r.y + 6), font_sz, t->accent);
}

/* ---------------------------------------------
   Context menu actions
   --------------------------------------------- */

void run_name_dialog(Editor *ed) {
    if (!ed->name_dialog_input[0]) return;
    char parent[4096];
    int valid_target = ed->ctx_target >= 0 && ed->ctx_target < ed->tree.count;
    int target_is_dir = valid_target && ed->tree.nodes[ed->ctx_target].is_dir;
    const char *target = valid_target ? ed->tree.nodes[ed->ctx_target].path : ed->cwd;
    if (ed->name_dialog_mode == 2) {
        /* rename: always the parent of the target */
        if (valid_target) {
            fs_parent_dir(target, parent, sizeof(parent));
            if (!parent[0]) snprintf(parent, sizeof(parent), "%s", ed->cwd);
        } else {
            snprintf(parent, sizeof(parent), "%s", ed->cwd);
        }
    } else {
        if (valid_target && target_is_dir) snprintf(parent, sizeof(parent), "%s", target);
        else if (valid_target) {
            fs_parent_dir(target, parent, sizeof(parent));
            if (!parent[0]) snprintf(parent, sizeof(parent), "%s", ed->cwd);
        } else {
            snprintf(parent, sizeof(parent), "%s", ed->cwd);
        }
    }
    char full[8192];
    snprintf(full, sizeof(full), "%s/%s", parent, ed->name_dialog_input);
    int ok = 0;
    if (ed->name_dialog_mode == 2) {
        const char *old = valid_target ? ed->tree.nodes[ed->ctx_target].path : NULL;
        if (old && old[0] && strcmp(old, full) != 0) ok = (rename(old, full) == 0);
    } else if (ed->name_dialog_mode == 1) {
        ok = (fs_mkdir(full) == 0);
    } else {
        FILE *f = fopen(full, "wb");
        if (f) { fclose(f); ok = 1; }
    }
    if (ok) {
        snprintf(ed->status_msg, sizeof(ed->status_msg), "%s: %s",
                 ed->name_dialog_mode == 0 ? "Created file" : (ed->name_dialog_mode == 1 ? "Created directory" : "Renamed to"),
                 ed->name_dialog_input);
        ed->status_msg_time = GetTime();
        ed->name_dialog_open = 0;
        ed->ctx_open = 0;
        ed->active_textbox = -1;
        ed->last_tree_scan = 0;  /* tree_rescan on the next frame (while not blocked) */
    } else {
        snprintf(ed->status_msg, sizeof(ed->status_msg), "Error: could not %s '%s'",
                 ed->name_dialog_mode == 0 ? "create file" : (ed->name_dialog_mode == 1 ? "create directory" : "rename"),
                 ed->name_dialog_input);
        ed->status_msg_time = GetTime();
    }
}

void ctx_execute(Editor *ed) {
    const char *target = (ed->ctx_target >= 0 && ed->ctx_target < ed->tree.count) ? ed->tree.nodes[ed->ctx_target].path : ed->cwd;
    if (!target || !target[0]) target = ed->cwd;
    int target_is_dir = ed->ctx_target >= 0 && ed->ctx_target < ed->tree.count && ed->tree.nodes[ed->ctx_target].is_dir;
    const char *base = path_basename(target);

    switch (ed->ctx_item) {
        case CTX_NEW_FILE:
        case CTX_NEW_FOLDER:
        case CTX_RENAME:
            ed->name_dialog_mode = (ed->ctx_item == CTX_NEW_FILE) ? 0 : (ed->ctx_item == CTX_NEW_FOLDER ? 1 : 2);
            if (ed->ctx_item == CTX_RENAME) snprintf(ed->name_dialog_input, sizeof(ed->name_dialog_input), "%.255s", base);
            else ed->name_dialog_input[0] = '\0';
            ed->name_dialog_open = 1;
            ed->active_textbox = 1;
            ed->text_caret_name = strlen(ed->name_dialog_input);
            ed->text_sel_name = ed->text_caret_name;
            ed->ctx_open = 0; /* CLOSE the context menu - so it does not conflict with name_dialog */
            break;
        case CTX_COPY:
            if (ed->ctx_target >= 0 && ed->ctx_target < ed->tree.count) {
                snprintf(ed->clipboard_path, sizeof(ed->clipboard_path), "%s", ed->tree.nodes[ed->ctx_target].path);
                ed->clipboard_has = 1;
            }
            ed->ctx_open = 0;
            break;
        case CTX_PASTE:
            if (ed->clipboard_has && ed->clipboard_path[0]) {
                char parent[4096];
                if (target_is_dir) snprintf(parent, sizeof(parent), "%s", target);
                else {
                    fs_parent_dir(target, parent, sizeof(parent));
                    if (!parent[0]) snprintf(parent, sizeof(parent), "%s", ed->cwd);
                }
                char dst[8192];
                snprintf(dst, sizeof(dst), "%s/%s", parent, path_basename(ed->clipboard_path));
                if (fs_is_dir(ed->clipboard_path)) fs_copy_dir_rec(ed->clipboard_path, dst);
                else fs_copy_file(ed->clipboard_path, dst);
                ed->last_tree_scan = 0;  /* deferred instead of synchronous tree_rescan */
            }
            ed->ctx_open = 0;
            break;
        case CTX_DELETE:
            if (ed->ctx_target >= 0 && ed->ctx_target < ed->tree.count) {
                const char *del = ed->tree.nodes[ed->ctx_target].path;
                const char *bn = path_basename(del);
                /* Do not delete default project folders/files */
                if (strcmp(bn, "scripts") == 0 || strcmp(bn, "assets") == 0 ||
                    strcmp(bn, "include") == 0 || strcmp(bn, "lib") == 0 ||
                    strcmp(bn, "external") == 0 || strcmp(bn, "main.gcsf") == 0 ||
                    strcmp(bn, "project.gcdata") == 0) {
                    snprintf(ed->status_msg, sizeof(ed->status_msg),
                             "Error: cannot delete default project item '%s'", bn);
                    ed->status_msg_time = GetTime();
                    ed->ctx_open = 0;
                    break;
                }
                fs_remove_rec(del);
                ed->last_tree_scan = 0;  /* deferred instead of synchronous tree_rescan */
            }
            ed->ctx_open = 0;
            break;
        default:
            ed->ctx_open = 0;
            break;
    }
}
