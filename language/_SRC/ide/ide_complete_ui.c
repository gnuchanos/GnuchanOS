/*
 * ide_complete_ui.c — Tamamlama popup'i + imza yardimi cizimi (todo.md §13).
 *
 * Motor (complete/) ve editor karar verir; bu modul YALNIZCA cizer:
 *   - popup listesi (tur ikonlari: emoji DEGIL, kucuk vektor glifler)
 *   - secili ogenin imza/detay paneli (doc paneli)
 *   - aktif cagrinin imza serididi (signature help, §11.2)
 */
#include "gcl_ide_internal.h"

/* ------------------------------------------------------------------ */
/* Tur ikonlari (vektor glifler — proje kurali: emoji YASAK)           */
/* ------------------------------------------------------------------ */
static void complete_draw_kind_icon(LspKind kind, int x, int y, int sz, Color col) {
    int cx = x + sz / 2;
    int cy = y + sz / 2;
    switch (kind) {
        case LSP_KIND_FUNC:
            DrawCircle(cx, cy, (float)sz / 2.0f, col);
            break;
        case LSP_KIND_VAR:
            DrawRectangle(x, y, sz, sz, col);
            break;
        case LSP_KIND_MACRO:
            DrawTriangle((Vector2){ (float)cx, (float)y },
                         (Vector2){ (float)(x + sz), (float)cy },
                         (Vector2){ (float)cx, (float)(y + sz) }, col);
            break;
        case LSP_KIND_TYPE:
            DrawRectangleLines(x, y, sz, sz, col);
            break;
        default:
            DrawCircle(cx, cy, (float)sz / 3.0f, col);
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Secili ogenin imza/detay paneli (popup altinda, VSCode benzeri)     */
/* ------------------------------------------------------------------ */
static void complete_draw_detail(const LspSymbol *s, int px, int py, int pw,
                                 int efont, const GclIdeTheme *t, int *out_h) {
    if (!s || !s->name) { if (out_h) *out_h = 0; return; }
    char sig[512];
    if (s->params && s->params[0])
        snprintf(sig, sizeof(sig), "%s(%s)", s->name, s->params);
    else
        snprintf(sig, sizeof(sig), "%s", s->name);

    int cw = MeasureText("M", efont);
    if (cw < 1) cw = 1;
    int max_chars = (pw - 16) / cw;
    if (max_chars < 10) max_chars = 10;

    char lines[8][256];
    size_t line_offsets[8];
    int nlines = 0;
    size_t idx = 0;
    size_t slen = strlen(sig);
    while (idx < slen && nlines < 8) {
        int taken = 0;
        while (idx < slen && taken < max_chars) {
            lines[nlines][taken++] = sig[idx++];
        }
        /* kelime ortasindan kesme — ayirac/bosluga kadar geri sar */
        if (idx < slen) {
            while (taken > 1 && lines[nlines][taken - 1] != ' ' &&
                   lines[nlines][taken - 1] != ',' &&
                   lines[nlines][taken - 1] != '(' &&
                   lines[nlines][taken - 1] != ')') {
                taken--; idx--;
            }
        }
        line_offsets[nlines] = idx - (size_t)taken;
        lines[nlines][taken] = '\0';
        nlines++;
    }
    if (nlines == 0) { lines[0][0] = '\0'; nlines = 1; }

    int line_h = efont + 2;
    int h = nlines * line_h + 8;
    if (s->detail && s->detail[0]) h += line_h;

    DrawRectangle(px, py, pw, h, t->popup_bg);
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)pw, (float)h },
                         1.0f, t->accent);

    size_t name_len = strlen(s->name);
    int y = py + 4;
    for (int i = 0; i < nlines; i++) {
        size_t lo = (i < 8) ? line_offsets[i] : 0;
        size_t ll = strlen(lines[i]);
        size_t name_end_in_line = (name_len > lo) ? name_len - lo : 0;
        char part[256];
        if (name_end_in_line < ll) {
            memcpy(part, lines[i], name_end_in_line);
            part[name_end_in_line] = '\0';
            if (part[0]) DrawText(part, px + 8, y, efont - 1, t->accent);
            DrawText(lines[i] + name_end_in_line,
                     px + 8 + MeasureText(part, efont - 1), y, efont - 1, t->text);
        } else {
            DrawText(lines[i], px + 8, y, efont - 1, t->accent);
        }
        y += line_h;
    }
    if (s->detail && s->detail[0]) {
        DrawText(s->detail, px + 8, y, efont - 2, t->gutter);
    }
    if (out_h) *out_h = h;
}

/* ------------------------------------------------------------------ */
/* Signature help (§11.2) — aktif cagrinin imzasi, aktif parametre     */
/* vurgulu. Editor alanindaki "top tarafta ince serit" olarak cizilir. */
/* ------------------------------------------------------------------ */
static int complete_draw_signature_help(Editor *ed, int px, int py, int max_w,
                                        int efont, const GclIdeTheme *t) {
    if (!ed->completion_have_sig) return 0;
    if (!ed->completion_sig_label[0]) return 0;

    char full[600];
    snprintf(full, sizeof(full), "%s(%s)", ed->completion_sig_label,
             ed->completion_sig_params);
    size_t total = strlen(full);
    size_t plen = strlen(ed->completion_sig_label);
    size_t pstart = plen + 1;              /* '(' sonrasi */
    if (pstart > total) pstart = total;

    /* Aktif parametre grubunun [gs, ge) araligini bul (ust-duzey virgul sayimi). */
    size_t gs = pstart;
    size_t ge = total > 0 ? total - 1 : total;   /* ')' haric */
    int found = 0;
    {
        int depth = 0, idx = 0;
        size_t seg = pstart;
        for (size_t i = pstart; i + 1 < total; i++) {
            char c = full[i];
            if (c == '(' || c == '[') depth++;
            else if (c == ')' || c == ']') { if (depth > 0) depth--; }
            else if (c == ',' && depth == 0) {
                if (idx == ed->completion_sig_active) { gs = seg; ge = i; found = 1; break; }
                idx++;
                seg = i + 1;
            }
        }
        if (!found && idx == ed->completion_sig_active) {
            gs = seg;
            ge = total > 0 ? total - 1 : total;
        }
    }
    if (gs > total) gs = total;
    if (ge > total) ge = total;
    if (gs < pstart) gs = pstart;
    if (ge < gs) ge = gs;

    int tw = MeasureText(full, efont - 1);
    int box_w = tw + 16;
    int box_h = efont + 10;
    if (box_w > max_w) box_w = max_w;
    if (box_w < 80) box_w = 80;

    DrawRectangle(px, py, box_w, box_h, t->popup_bg);
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)box_w, (float)box_h },
                         1.0f, t->accent);

    int ty = py + 5;
    int x = px + 8;
    char seg_a[600], seg_b[600], seg_c[600], seg_d[600];
    /* '(' de seg_a'ya dahil edilir: seg_b pstart=plen+1'den baslar, aksi halde
       full[plen]='(' hicbir segmente girmez → "Raylib.InitWindowint..." cikar. */
    size_t n_a = pstart;                       /* isim + '(' */
    size_t n_b = (gs > pstart) ? (gs - pstart) : 0;
    size_t n_c = (ge > gs) ? (ge - gs) : 0;
    size_t n_d = (total > ge) ? (total - ge) : 0;
    if (n_a > sizeof(seg_a) - 1) n_a = sizeof(seg_a) - 1;
    if (n_b > sizeof(seg_b) - 1) n_b = sizeof(seg_b) - 1;
    if (n_c > sizeof(seg_c) - 1) n_c = sizeof(seg_c) - 1;
    if (n_d > sizeof(seg_d) - 1) n_d = sizeof(seg_d) - 1;
    memcpy(seg_a, full, n_a); seg_a[n_a] = '\0';
    memcpy(seg_b, full + pstart, n_b); seg_b[n_b] = '\0';
    memcpy(seg_c, full + gs, n_c); seg_c[n_c] = '\0';
    memcpy(seg_d, full + ge, n_d); seg_d[n_d] = '\0';

    if (seg_a[0]) { DrawText(seg_a, x, ty, efont - 1, t->accent); x += MeasureText(seg_a, efont - 1); }
    if (seg_b[0]) { DrawText(seg_b, x, ty, efont - 1, t->text);   x += MeasureText(seg_b, efont - 1); }
    if (seg_c[0]) { DrawText(seg_c, x, ty, efont - 1, t->accent); x += MeasureText(seg_c, efont - 1); }
    if (seg_d[0]) { DrawText(seg_d, x, ty, efont - 1, t->text); }
    return box_h;
}

/* ------------------------------------------------------------------ */
/* Motor tanisi (§5.4) — or. printf {} yer-tutucu sayisizligi          */
/* ------------------------------------------------------------------ */
static int complete_draw_diagnostic(Editor *ed, int px, int py, int max_w,
                                    int efont, const GclIdeTheme *t) {
    if (!ed->completion_have_diag || !ed->completion_diag[0]) return 0;

    int tw = MeasureText(ed->completion_diag, efont - 1);
    int box_w = tw + 24;
    int box_h = efont + 10;
    if (box_w > max_w) box_w = max_w;
    if (box_w < 80) box_w = 80;

    DrawRectangle(px, py, box_w, box_h, t->popup_bg);
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)box_w, (float)box_h },
                         1.0f, t->error);
    DrawText(ed->completion_diag, px + 8, py + 5, efont - 1, t->error);
    return box_h;
}

/* ------------------------------------------------------------------ */
/* Ana giris: popup + imza serididi                                    */
/* ------------------------------------------------------------------ */
void ide_complete_ui_draw(Editor *ed, Rectangle editor_rect, int efont,
                          const GclIdeTheme *t) {
    if (!ed || !t) return;
    GclIdeBuffer *b = editor_cur(ed);
    if (!b) return;

    int max_w = (int)editor_rect.width - GUTTER_W - 8;
    if (max_w < 120) max_w = 120;

    size_t cur_line = gcl_ide_buffer_line_of_cursor(b);
    int row_line_y = (int)editor_rect.y + 4 +
                     (int)(cur_line - b->scroll_y) * LINE_H;

    int anchor_x = (int)editor_rect.x + GUTTER_W;
    /* Imza seridi/tani, popup YOKSA imleç satirinin hemen altina cizilir. */
    int anchor_y = row_line_y + LINE_H + 4;

    int popup_shown = ed->completion_visible &&
                      (ed->completion_count > 0 || ed->completion_no_match);

    if (popup_shown) {
        int px = anchor_x;
        int py = row_line_y + LINE_H;
        int pw = 280, ph = 28;

        if (ed->completion_no_match) {
            pw = MeasureText(ed->completion_message, efont) + 24;
            if (pw < 240) pw = 240;
        } else {
            int widest = 0;
            for (int i = 0; i < ed->completion_count; i++) {
                const char *lb = ed->completions[i].name ? ed->completions[i].name : "";
                int lw = MeasureText(lb, efont) + 64;
                if (lw > widest) widest = lw;
            }
            pw = widest > 280 ? widest : 280;
            ph = ed->completion_count * 20 + 4;
            if (ph > 200) ph = 200;
        }

        /* Secili ogenin imza/detay paneli popup'in altina eklenir — tasma
           duzeltmesi cizimden ONCE yapilsin diye yuksekligi simdiden ekle. */
        int detail_h = 0;
        int used_detail_h = 0;
        int have_detail = (!ed->completion_no_match &&
                           ed->completion_selected >= 0 &&
                           ed->completion_selected < ed->completion_count);
        if (have_detail) {
            /* yalnizca yukseklik tahmini: satir sayisi + detay satiri */
            const LspSymbol *s = &ed->completions[ed->completion_selected];
            int est_chars = (s->name ? (int)strlen(s->name) : 0) + 32;
            int per_line = (pw - 16) / (MeasureText("M", efont) > 0
                                        ? MeasureText("M", efont) : 1);
            if (per_line < 10) per_line = 10;
            int nlines = (est_chars + per_line - 1) / per_line;
            if (nlines < 1) nlines = 1;
            if (nlines > 8) nlines = 8;
            detail_h = nlines * (efont + 2) + 8;
            if (s->detail && s->detail[0]) detail_h += efont + 2;
        }

        int total_h = ph + detail_h;

        /* X: editor alaninda kal */
        if (px + pw > (int)(editor_rect.x + editor_rect.width))
            px = (int)(editor_rect.x + editor_rect.width) - pw;
        if (px < (int)editor_rect.x) px = (int)editor_rect.x;
        /* Y: once imlecin ALTINDA acilir; sigmazsa USTUNE gecer */
        if (py + total_h > (int)(editor_rect.y + editor_rect.height) - 4) {
            py = row_line_y - total_h - 4;
            if (py < (int)editor_rect.y + 4) py = (int)editor_rect.y + 4;
        }

        DrawRectangle(px, py, pw, ph, t->popup_bg);
        DrawRectangleLines((float)px, (float)py, (float)pw, (float)ph, t->accent);

        if (ed->completion_no_match) {
            DrawText(ed->completion_message, px + 8, py + 4, efont, t->error);
        } else {
            int start = 0;
            if (ed->completion_selected >= 10) start = ed->completion_selected - 9;
            for (int i = start; i < ed->completion_count && i < start + 10; i++) {
                int y = py + 2 + (i - start) * 20;
                bool sel = (i == ed->completion_selected);
                if (sel) DrawRectangle(px + 1, y, pw - 2, 20, t->selection);

                LspKind k = ed->completions[i].kind;
                Color icon_col = sel ? WHITE : t->accent;
                complete_draw_kind_icon(k, px + 5, y + 5, 10, icon_col);

                const char *lb = ed->completions[i].name ? ed->completions[i].name : "";
                const char *dt = ed->completions[i].detail ? ed->completions[i].detail : "";
                DrawText(lb, px + 22, y + 2, efont - 1, sel ? WHITE : t->text);
                if (dt[0]) {
                    int dw = MeasureText(dt, efont - 2);
                    if (px + 22 + MeasureText(lb, efont - 1) + 8 < px + pw - dw - 8)
                        DrawText(dt, px + pw - dw - 8, y + 4, efont - 2, t->gutter);
                }
            }
        }

        /* Secili ogenin imza/detay paneli (popup'in hemen altinda). */
        if (have_detail) {
            int drawn_h = 0;
            complete_draw_detail(&ed->completions[ed->completion_selected],
                                 px, py + ph, (pw > 320 ? pw : 320), efont, t, &drawn_h);
            used_detail_h = drawn_h;
        }

        anchor_x = px;
        anchor_y = py + ph + (have_detail ? used_detail_h : 0) + 4;
    }

    /* Imza seridi ve tani (varsa) DAIMA tamamlamanin HEMEN ALTINDA. */
    anchor_y += complete_draw_signature_help(ed, anchor_x, anchor_y, max_w, efont, t);
    if (ed->completion_have_sig && ed->completion_sig_label[0]) anchor_y += 2;
    complete_draw_diagnostic(ed, anchor_x, anchor_y, max_w, efont, t);
}
