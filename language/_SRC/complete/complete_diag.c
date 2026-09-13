/*
 * complete_diag.c — Motor tanıları (§5.4).
 *
 * GCL'de printf yer-tutucusu `{}`'tir; `%s`/`%d` YOKTUR. `printf("a={} b={}",
 * x, y)` gibi. Kalıp string'i kapanmış bir printf çağrısında `{}` sayısı ile
 * takip eden argüman sayısı uyuşmuyorsa kullanıcıya uyarı verilir.
 *
 * Denetim yalnızca kalıp string'i TAMAMLANMIŞSA yapılır: yazım sürerken
 * (kapanış tırnağı yoksa) argüman sayısı bilinemez, uyarı üretilmez.
 */
#include "complete_diag.h"
#include <stdio.h>

static int diag_is_blank(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Aktif çağrının '(' konumunu bul; yoksa -1. String/yorum içindeki parantezler
   SAYILMAZ.

   ÖNEMLİ: kullanıcı çağrıyı çoktan bitirmiş olabilir — `printf("x={}", a);`.
   Bu yüzden sondaki boşluklar, ';' ve BİR kapanış ')' işaretini taramadan
   önce yok sayarız; tanı, çağrı yazılıp bittikten sonra da görünür kalır. */
static long diag_find_call_paren(const char *text, size_t cursor) {
    size_t eff = cursor;
    for (;;) {
        while (eff > 0 && diag_is_blank(text[eff - 1])) eff--;
        if (eff > 0 && text[eff - 1] == ';') { eff--; continue; }
        break;
    }
    if (eff > 0 && text[eff - 1] == ')') eff--;   /* kapanışı yok say */

    long stack[64];
    int sp = 0, block = 0, line = 0;
    char quote = 0;
    for (size_t i = 0; i < eff; i++) {
        char c = text[i];
        if (line) { if (c == '\n') line = 0; continue; }
        if (block == 1) {
            if (c == '*' && i + 1 < eff && text[i + 1] == '/') { block = 0; i++; }
            continue;
        }
        if (block == 2) {
            if (c == '|' && i + 1 < eff && text[i + 1] == '#') { block = 0; i++; }
            continue;
        }
        if (quote) {
            if (c == '\\' && i + 1 < eff) { i++; continue; }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '/' && i + 1 < eff && text[i + 1] == '/') { line = 1; i++; continue; }
        if (c == '/' && i + 1 < eff && text[i + 1] == '*') { block = 1; i++; continue; }
        if (c == '#' && i + 1 < eff && text[i + 1] == '|') { block = 2; i++; continue; }
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '(') { if (sp < 64) stack[sp++] = (long)i; }
        else if (c == ')') { if (sp > 0) sp--; }
    }
    return (sp > 0) ? stack[sp - 1] : -1;
}

/* '(' öncesindeki çağrı adının SON bileşenini yaz ("Stdio.printf" → "printf"). */
static int diag_callee_is_printf(const char *text, size_t paren) {
    size_t e = paren;
    while (e > 0 && (text[e - 1] == ' ' || text[e - 1] == '\t')) e--;
    size_t s = e;
    while (s > 0 && gclc_ident_char(text[s - 1])) s--;
    if (s == e) return 0;
    size_t n = e - s;
    return (n == 6 && strncmp(text + s, "printf", 6) == 0);
}

void gcl_diag_check_printf(const char *text, size_t text_len, size_t cursor,
                           GclCompletionResult *out) {
    if (!text || !out) return;
    if (cursor > text_len) cursor = text_len;

    long paren = diag_find_call_paren(text, cursor);
    if (paren < 0) return;
    if (!diag_callee_is_printf(text, (size_t)paren)) return;

    /* 1) İlk argüman kalıp string'i mi? (boşlukları/ satır sonlarını atla) */
    size_t p = (size_t)paren + 1;
    while (p < cursor && diag_is_blank(text[p])) p++;
    if (p >= cursor || text[p] != '"') return;

    /* 2) Kapanış tırnağını bul; imleç STRING İÇİNDEyse denetim yapma. */
    size_t fmt_start = p + 1;
    size_t q = fmt_start;
    while (q < text_len && text[q] != '"') {
        if (text[q] == '\\' && q + 1 < text_len) q++;
        q++;
    }
    if (q >= text_len) return;          /* kapanmamış string */
    if (q >= cursor) return;            /* imleç hâlâ string içinde */
    size_t fmt_end = q + 1;             /* kapanış tırnağından sonrası */

    /* 3) {} yer-tutucu sayısı (yalnız kalıp string'i içinde). */
    int placeholders = 0;
    for (size_t i = fmt_start; i + 1 < q; i++) {
        if (text[i] == '{' && text[i + 1] == '}') { placeholders++; i++; }
    }

    /* 4) Argüman sayısı: kalıptan sonra imlece kadar üst-düzey virgüller +
       son virgülden sonra boş olmayan bir argüman varsa +1. Çağrının kapanış
       ')' işaretine gelince durulur. String'ler atlanır. */
    int commas = 0, depth = 0, has_tail = 0, sep_seen = 0;
    for (size_t i = fmt_end; i < cursor; i++) {
        char c = text[i];
        if (c == '"' || c == '\'') {
            char quote = c;
            i++;
            while (i < cursor && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < cursor) i++;
                i++;
            }
            if (depth == 0) has_tail = 1;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') { depth++; has_tail = 1; continue; }
        if (c == ')' || c == ']' || c == '}') {
            if (depth > 0) { depth--; has_tail = 1; continue; }
            break;                       /* çağrının kapanışı → argümanlar bitti */
        }
        if (c == ',' && depth == 0) {
            /* Kalıp ile 1. argüman arasındaki virgül ARGÜMAN AYIRICI DEĞİLDİR. */
            if (!sep_seen) sep_seen = 1;
            else commas++;
            has_tail = 0;
            continue;
        }
        if (!diag_is_blank(c)) has_tail = 1;
    }
    int args = commas + (has_tail ? 1 : 0);

    if (placeholders == args) return;    /* tutarlı */

    out->have_diagnostic = 1;
    snprintf(out->diag_message, sizeof(out->diag_message),
             "printf: kalıpta %d yer tutucu {}, %d argüman verildi",
             placeholders, args);
}
