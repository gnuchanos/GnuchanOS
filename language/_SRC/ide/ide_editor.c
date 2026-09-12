#include "gcl_ide_internal.h"
#include "gcl_ide_clipboard.h"
#include <dirent.h>

/* ---------------------------------------------
   Text buffer helpers
   --------------------------------------------- */

char *editor_buffer_text_cstr(const GclIdeBuffer *b) {
    char *s = (char *)malloc(b->size + 1);
    if (!s) return NULL;
    if (b->size > 0) memcpy(s, b->content, b->size);
    s[b->size] = '\0';
    return s;
}

void buffer_delete_range(GclIdeBuffer *b, size_t start, size_t end) {
    if (end <= start || !b->content) return;
    if (end > b->size) end = b->size;
    memmove(b->content + start, b->content + end, b->size - end);
    b->size -= end - start;
    if (b->size < b->cap) b->content[b->size] = '\0';
    b->cursor = start;
    b->dirty = 1;
}

int editor_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

int editor_is_word_char(char c) {
    return editor_is_ident_char(c) || c == '#';
}

/* Trim leading/trailing blanks IN PLACE.
   gcl_lsp_trim() allocates and RETURNS a new string; calling it and throwing
   the result away leaves "    Stdio" (indented) untouched, so
   strcmp(module_name, "Stdio") never matched and member completion stayed
   empty for every indented line (todo #4). */
static void editor_trim_inplace(char *s) {
    if (!s) return;
    char *a = s;
    while (*a == ' ' || *a == '\t') a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
}

/* Cursor's lexical context: is it inside a string/char literal or inside a
   comment? The scan starts at the beginning of the buffer, so multi-line block
   comments and unterminated strings are detected correctly. Auto-completion
   must never open in these contexts (todo item: the "there is no ..." popup
   used to appear while typing inside a string literal). */
typedef struct {
    int in_string;
    int in_comment;
} EditorLexContext;

static void editor_lex_context_at(const char *text, size_t cursor, EditorLexContext *ctx) {
    ctx->in_string = 0;
    ctx->in_comment = 0;
    if (!text) return;
    int block = 0;         /* 1 = block comment, 2 = GCL #| ... |# comment */
    int line_comment = 0;
    char quote = 0;
    for (size_t i = 0; i < cursor; i++) {
        char c = text[i];
        if (line_comment) {
            if (c == '\n') line_comment = 0;
            continue;
        }
        if (block == 1) {
            if (c == '*' && i + 1 < cursor && text[i + 1] == '/') { block = 0; i++; }
            continue;
        }
        if (block == 2) {
            if (c == '|' && i + 1 < cursor && text[i + 1] == '#') { block = 0; i++; }
            continue;
        }
        if (quote) {
            if (c == '\\' && i + 1 < cursor) { i++; continue; }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '/' && i + 1 < cursor && text[i + 1] == '/') { line_comment = 1; i++; continue; }
        if (c == '/' && i + 1 < cursor && text[i + 1] == '*') { block = 1; i++; continue; }
        if (c == '#' && i + 1 < cursor && text[i + 1] == '|') { block = 2; i++; continue; }
        if (c == '"' || c == '\'') quote = c;
    }
    ctx->in_string = quote != 0;
    ctx->in_comment = (block != 0) || line_comment;
}

/* Is the current line a preprocessor line (first non-blank char is '#')?
   Preprocessor lines keep their own completion rules (#include "..." etc.). */
static int editor_line_is_preproc(const char *line, size_t col) {
    size_t i = 0;
    while (i < col && (line[i] == ' ' || line[i] == '\t')) i++;
    return i < col && line[i] == '#';
}

void editor_handle_bracket_close(Editor *ed, char open_char, char close_char) {
    gcl_ide_buffer_insert_char(&CURP, open_char);
    gcl_ide_buffer_insert_char(&CURP, close_char);
    gcl_ide_buffer_cursor_left(&CURP);
    ed->sel_anchor = CURP.cursor;
}

/* ==== Otomatik tamamlama (yalnızca gcl_lsp_inscript_scan) ==== */

/* Aktif sayfadaki struct/enum/typedef üyelerini topla.
   "Test." yazıldıysa Test'in üyelerini öner. */
static int add_members_for(LspSymbol *syms, int count, const char *type_name,
                           const char *member_prefix,
                           const char *current_file,
                           LspSymbol **out, int *out_count, int *cap) {
    /* SADECE LSP_KIND_TYPE (struct/enum/typedef) sembolü eşleşsin.
       Bir değişken/alan adıyla aynı isimde bir type varsa, o type'ın
       üyeleri önerilir — ama yalnızca type olarak kayıtlıysa. */
    int first_match = -1;
    for (int i = 0; i < count; i++) {
        if (syms[i].name && syms[i].kind == LSP_KIND_TYPE &&
            strcmp(syms[i].name, type_name) == 0) {
            if (first_match < 0) first_match = i;
            if (current_file && syms[i].file && strcmp(syms[i].file, current_file) == 0) {
                first_match = i;
                break;
            }
        }
    }
    if (first_match >= 0) {
        int i = first_match;
        if (!syms[i].members || syms[i].member_count == 0) return 1;
        for (int m = 0; m < syms[i].member_count; m++) {
            const char *mn = syms[i].members[m].name;
            if (member_prefix[0] && strncmp(mn, member_prefix, strlen(member_prefix)) != 0) continue;
            /* Private üye başka dosyadan görünmesin */
            if (syms[i].members[m].vis == LSP_VIS_PRIVATE &&
                syms[i].members[m].file && current_file &&
                strcmp(syms[i].members[m].file, current_file) != 0) continue;
            lsp_list_add(out, out_count, cap, mn,
                         syms[i].members[m].kind, syms[i].members[m].vis,
                         syms[i].members[m].detail ? syms[i].members[m].detail : "member",
                         syms[i].members[m].params, syms[i].file);
        }
        return 1;
    }
    return 0;
}

/* GCL/C benzeri primitive tipler — bunlar syms listesinde YOK ama
   değer oluşturma bağlamında "Type name" olarak kabul edilir. */
static const char **gcl_primitive_types = gcl_builtin_types;

/* #native modül üyeleri — Math., Stdio., Embed. erişimi */
/* Üye listeleri (gcl_native_math_members / _stdio_ / _embed_) ve
   gcl_native_modules artık gcl_lsp_internal.h'da paylaşılıyor — LSP
   tarayıcısı da #native <X> modüllerini tanısın diye (todo #2). */

/* Native üye imza (parametre) bilgisi — tamamlama penceresinde görünür */
static const char *native_member_params(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "printf") == 0 || strcmp(name, "scanf") == 0) return "format, ...";
    if (strcmp(name, "openFile") == 0 || strcmp(name, "openfile") == 0) return "path";
    if (strcmp(name, "writeFile") == 0 || strcmp(name, "writefile") == 0 ||
        strcmp(name, "appendFile") == 0 || strcmp(name, "appendfile") == 0) return "path, content";
    if (strcmp(name, "readFile") == 0 || strcmp(name, "readfile") == 0) return "path";
    if (strcmp(name, "closeFile") == 0 || strcmp(name, "closefile") == 0) return "path";
    if (strcmp(name, "fileExists") == 0 || strcmp(name, "fileexists") == 0) return "path";
    if (strcmp(name, "deleteFile") == 0 || strcmp(name, "deletefile") == 0) return "path";
    if (strcmp(name, "renameFile") == 0 || strcmp(name, "renamefile") == 0) return "path, newName";
    if (strcmp(name, "fileSize") == 0 || strcmp(name, "filesize") == 0) return "path";
    if (strcmp(name, "flushFile") == 0 || strcmp(name, "flushfile") == 0) return "path";
    return NULL;
}

/* İmlecin öncesindeki metinde "değer oluşturma" bağlamı var mı?
   "int x", "Test t", "Color c" gibi — yeni bir değişken tanımlanıyorsa
   "there is no 'x'" mesajı gösterilmez (x henüz tanım değil, oluşturuluyor). */
static int editor_is_declaration_context(const char *line_str, size_t col,
                                          const char *cur_word,
                                          LspSymbol *syms, int sym_count) {
    if (!line_str || cur_word[0] == '\0') return 0;

    /* cur_word başlangıcını bul (imlecin solundaki kelime) */
    const char *q = line_str + col;
    const char *ws = q;
    while (ws > line_str && gcl_lsp_is_ident_char(ws[-1])) ws--;

    /* cur_word'dan önceki kelimeyi bul (tip olabilir) */
    const char *tp = ws;
    while (tp > line_str && (tp[-1] == ' ' || tp[-1] == '\t')) tp--;
    const char *type_start = tp;
    while (type_start > line_str && gcl_lsp_is_ident_char(type_start[-1])) type_start--;

    /* type_start..tp arası bilinen bir tip adı mı? (Test, Color, int...) */
    size_t tl = (size_t)(tp - type_start);
    if (tl == 0) return 0;

    /* 1) primitive tipler (int, float, char, bool...) */
    for (int p = 0; gcl_primitive_types[p]; p++) {
        size_t pn = strlen(gcl_primitive_types[p]);
        if (pn == tl && strncmp(type_start, gcl_primitive_types[p], pn) == 0) {
            return 1; /* "int name" — değer oluşturuluyor */
        }
    }
    /* 2) syms listesindeki struct/enum/typedef tip adları */
    for (int i = 0; i < sym_count; i++) {
        if (!syms[i].name) continue;
        size_t tn = strlen(syms[i].name);
        if (tn == tl && strncmp(type_start, syms[i].name, tn) == 0) {
            return 1; /* "Test name" — değer oluşturuluyor */
        }
    }
    return 0;
}

/* "Type var = " atama bağlamı mı? İmleç '=' sonrasındaysa SADECE değer önerilir.
   Örn: "int x = " → x bir değer bekler; sadece değişken/enum/macro gösterilir. */
static int editor_is_value_assign_context(const char *line_str, size_t col,
                                          LspSymbol *syms, int sym_count) {
    if (!line_str || col == 0) return 0;
    char prefix[4096];
    size_t plen = col;
    if (plen >= sizeof(prefix)) plen = sizeof(prefix) - 1;
    memcpy(prefix, line_str, plen);
    prefix[plen] = '\0';

    const char *eq = strrchr(prefix, '=');
    if (!eq) return 0;

    /* '=' den önce "Type var" deseni ara: tip adı + değişken adı */
    const char *p = eq;
    while (p > prefix && (p[-1] == ' ' || p[-1] == '\t')) p--;
    const char *var_end = p;
    while (var_end > prefix && gcl_lsp_is_ident_char(var_end[-1])) var_end--;
    if (var_end == p) return 0;  /* değişken adı yok */

    const char *tp = var_end;
    while (tp > prefix && (tp[-1] == ' ' || tp[-1] == '\t')) tp--;
    const char *type_start = tp;
    while (type_start > prefix && gcl_lsp_is_ident_char(type_start[-1])) type_start--;
    if (type_start == tp) return 0;  /* tip adı yok */

    size_t tl = (size_t)(tp - type_start);
    for (int p = 0; gcl_primitive_types[p]; p++) {
        size_t pn = strlen(gcl_primitive_types[p]);
        if (pn == tl && strncmp(type_start, gcl_primitive_types[p], pn) == 0) return 1;
    }
    for (int i = 0; i < sym_count; i++) {
        if (!syms[i].name) continue;
        size_t tn = strlen(syms[i].name);
        if (tn == tl && strncmp(type_start, syms[i].name, tn) == 0) return 1;
    }
    return 0;
}

/* "struct X {" / "enum X {" / "typedef ... {" tip tanımı bağlamı mı?
   Böyle bir gövde içinde SADECE tip adları + builtin tipler önerilir. */
static int editor_is_type_decl_context(const char *prefix) {
    const char *last_brace = strrchr(prefix, '{');
    if (!last_brace) return 0;
    const char *line_start = last_brace;
    while (line_start > prefix && line_start[-1] != '\n') line_start--;
    char line[512];
    size_t ll = (size_t)(last_brace - line_start);
    if (ll >= sizeof(line)) ll = sizeof(line) - 1;
    memcpy(line, line_start, ll);
    line[ll] = '\0';
    const char *q = line;
    while (*q == ' ' || *q == '\t') q++;
    return (strncmp(q, "struct", 6) == 0 || strncmp(q, "enum", 4) == 0 ||
            strncmp(q, "typedef", 7) == 0);
}

/* --- Akıllı tamamlama: case-insensitive filtre + sıralama yardımcıları --- */

static int editor_ch_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int editor_stricmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = editor_ch_lower((unsigned char)*a);
        int cb = editor_ch_lower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return editor_ch_lower((unsigned char)*a) - editor_ch_lower((unsigned char)*b);
}

static int editor_strnicmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = editor_ch_lower((unsigned char)a[i]);
        int cb = editor_ch_lower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (!a[i] || !b[i]) break;
    }
    return 0;
}

/* VSCode benzeri fuzzy eşleşme: query karakterleri name içinde SIRALI olarak bulunmalı.
   CamelCase segment başlangıçları otomatik avantaj sağlar. Örn: "inw" → "InitWindow". */
static int editor_fuzzy_match(const char *name, const char *query) {
    if (!name || !query || !query[0]) return 1; /* boş query → eşleşir */
    size_t qi = 0;
    for (const char *p = name; *p && query[qi]; p++) {
        if (editor_ch_lower((unsigned char)*p) == editor_ch_lower((unsigned char)query[qi])) {
            qi++;
        }
    }
    return query[qi] == '\0';
}

/* Aktif kelimeye göre normalizasyonlu filtre:
   önce case-insensitive prefix, sonra fuzzy. İkisi de başarısızsa eşleşme yok. */
static int editor_match_cur(const char *name, const char *cur_word) {
    if (!cur_word || !cur_word[0]) return 1;
    if (!name) return 0;
    if (editor_strnicmp(name, cur_word, strlen(cur_word)) == 0) return 1;
    return editor_fuzzy_match(name, cur_word);
}

/* Native modül adları (gcl_native_modules) ve gcl_is_native_module artık
   gcl_lsp_internal.h'da paylaşılıyor — LSP tarayıcısı da aynı listeyi
   kullansın diye (todo #2). */
static int editor_is_native_module(const char *name) {
    return gcl_is_native_module(name);
}

/* Add the members of a native module (Math./Stdio./Embed./Raylib./Raygui.)
   to the completion list. Returns 1 when 'module_name' is a native module. */
static int editor_add_native_members(Editor *ed, int *cap, const char *module_name,
                                     const char *cur_word, const char *file) {
    if (!module_name || !module_name[0]) return 0;

    if (strcmp(module_name, "Math") == 0) {
        for (int mi = 0; gcl_native_math_members[mi]; mi++) {
            if (cur_word[0] && !editor_match_cur(gcl_native_math_members[mi], cur_word)) continue;
            lsp_list_add(&ed->completions, &ed->completion_count, cap,
                         gcl_native_math_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                         "Math member", native_member_params(gcl_native_math_members[mi]), file);
        }
        return 1;
    }
    if (strcmp(module_name, "Stdio") == 0) {
        for (int mi = 0; gcl_native_stdio_members[mi]; mi++) {
            if (cur_word[0] && !editor_match_cur(gcl_native_stdio_members[mi], cur_word)) continue;
            lsp_list_add(&ed->completions, &ed->completion_count, cap,
                         gcl_native_stdio_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                         "Stdio member", native_member_params(gcl_native_stdio_members[mi]), file);
        }
        return 1;
    }
    if (strcmp(module_name, "Embed") == 0) {
        for (int mi = 0; gcl_native_embed_members[mi]; mi++) {
            if (cur_word[0] && !editor_match_cur(gcl_native_embed_members[mi], cur_word)) continue;
            lsp_list_add(&ed->completions, &ed->completion_count, cap,
                         gcl_native_embed_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                         "Embed member", native_member_params(gcl_native_embed_members[mi]), file);
        }
        return 1;
    }
    if (strcmp(module_name, "Raylib") == 0) {
        for (int ri = 0; gcl_Raylib_types[ri]; ri++) {
            const char *n = gcl_Raylib_types[ri];
            if (!n || !n[0]) continue;
            if (cur_word[0] && !editor_match_cur(n, cur_word)) continue;
            int is_type = 0;
            for (int ti = 0; gcl_Raylib_type_names[ti]; ti++) {
                if (strcmp(n, gcl_Raylib_type_names[ti]) == 0) { is_type = 1; break; }
            }
            lsp_list_add(&ed->completions, &ed->completion_count, cap,
                         n, is_type ? LSP_KIND_TYPE : LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                         is_type ? "Raylib type" : "Raylib function", NULL, file);
        }
        return 1;
    }
    if (strcmp(module_name, "Raygui") == 0) {
        for (int gi = 0; gcl_Raygui_types[gi]; gi++) {
            const char *n = gcl_Raygui_types[gi];
            if (!n || !n[0]) continue;
            if (cur_word[0] && !editor_match_cur(n, cur_word)) continue;
            int is_type = 0;
            for (int ti = 0; gcl_Raygui_type_names[ti]; ti++) {
                if (strcmp(n, gcl_Raygui_type_names[ti]) == 0) { is_type = 1; break; }
            }
            lsp_list_add(&ed->completions, &ed->completion_count, cap,
                         n, is_type ? LSP_KIND_TYPE : LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                         is_type ? "Raygui type" : "Raygui function", NULL, file);
        }
        return 1;
    }
    return 0;
}

/* Active file path for completion sorting (qsort cannot take a context). */
static const char *g_sort_current_file = NULL;

/* Dosya kökeni önceliği: yerel dosya > include/proje > builtin.
   Faz 3: "yerel dosya > include > proje > native > keyword" */
static int editor_sort_priority(const LspSymbol *s) {
    if (!s) return 4;
    if (s->detail && (strcmp(s->detail, "keyword") == 0 || strcmp(s->detail, "type") == 0))
        return 4;  /* builtin keyword/type */
    if (s->file && g_sort_current_file && strcmp(s->file, g_sort_current_file) == 0)
        return 1;  /* yerel dosya */
    if (s->file) return 2;  /* include/proje */
    return 3;
}

/* Sıralama önceliği: önce kind (VAR > FUNC > MACRO > TYPE > UNKNOWN),
   sonra dosya kökeni (yerel > include > diğer), en son alfabetik.
   LspKind: UNKNOWN=0, FUNC=1, VAR=2, MACRO=3, TYPE=4 */
static int editor_cmp_completion(const void *a, const void *b) {
    const LspSymbol *sa = (const LspSymbol *)a;
    const LspSymbol *sb = (const LspSymbol *)b;
    static const int prio[] = { 4, 1, 0, 2, 3 };
    int pa = (sa->kind >= 0 && sa->kind <= 4) ? prio[sa->kind] : 4;
    int pb = (sb->kind >= 0 && sb->kind <= 4) ? prio[sb->kind] : 4;
    if (pa != pb) return pa < pb ? -1 : 1;
    /* Aynı kind içinde dosya kökeni: yerel > include > diğer */
    int fa = editor_sort_priority(sa);
    int fb = editor_sort_priority(sb);
    if (fa != fb) return fa - fb;
    if (sa->name && sb->name) {
        int c = editor_stricmp(sa->name, sb->name);
        if (c) return c;
        return strcmp(sa->name, sb->name);
    }
    return 0;
}

/* İmleç bir fonksiyon çağrısı parantezinin İÇİNDE mi? (string atla) */
static int editor_inside_arguments(const char *prefix) {
    int depth = 0;
    char quote = 0;
    int line_comment = 0;
    int block_comment = 0;  /* 1 = C block comment, 2 = GCL #| ... |# */
    for (const char *p = prefix; *p; p++) {
        char c = *p;
        if (line_comment) {
            if (c == '\n') line_comment = 0;
            continue;
        }
        if (block_comment == 1) {
            if (c == '*' && p[1] == '/') { block_comment = 0; p++; }
            continue;
        }
        if (block_comment == 2) {
            if (c == '|' && p[1] == '#') { block_comment = 0; p++; }
            continue;
        }
        if (quote) {
            if (c == '\\' && p[1]) { p++; continue; }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '/' && p[1] == '/') { line_comment = 1; p++; continue; }
        if (c == '/' && p[1] == '*') { block_comment = 1; p++; continue; }
        if (c == '#' && p[1] == '|') { block_comment = 2; p++; continue; }
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '(') depth++;
        else if (c == ')') { if (depth > 0) depth--; }
    }
    return depth > 0;
}

void editor_show_completion(Editor *ed, int manual) {
    /* Once ESC dismisses the popup it stays closed until a NEW deliberate
       trigger. Only a typed '.' and Ctrl+Space reset completion_dismissed
       in the caller, so this single check stops the popup from popping back
       up (the old code set the flag but never read it - todo items 5 and 17). */
    if (ed->completion_dismissed && !manual) return;
    /* eski listeyi temizle */
    lsp_list_clear(&ed->completions, &ed->completion_count);
    ed->completion_selected = 0;
    ed->completion_visible = 0;
    ed->completion_no_match = 0;
    ed->completion_message[0] = '\0';
    /* manual = 1 → kullanıcı açıkça istedi (Ctrl+Space): eşleşme yoksa
       "there is no 'X'" mesajı gösterilir. manual = 0 → '.' üzerine otomatik
       açıldı: mesaj gösterilmez, yalnızca gerçek öneriler listelenir. */

    if (ed->tab_count <= 0 || ed->active_tab < 0 || ed->active_tab >= ed->tab_count) return;

    GclIdeBuffer *b = &ed->tabs[ed->active_tab];
    char *text = editor_buffer_text_cstr(b);
    if (!text) return;

    /* imleç konumu */
    size_t line = gcl_ide_buffer_line_of_cursor(b);
    size_t col = gcl_ide_buffer_cursor_in_line(b);
    size_t l = 0;
    const char *line_str = gcl_ide_buffer_line_at(b, line, &l);
    if (col > l) col = l;

    /* satırın imlece kadar olan kısmı */
    char prefix[1024];
    size_t plen = col;
    if (plen >= sizeof(prefix)) plen = sizeof(prefix) - 1;
    memcpy(prefix, line_str, plen);
    prefix[plen] = '\0';

    /* aktif kelimeyi bul — imleçten GERİYE doğru (last_dot'tan bağımsız).
       "Stdio.printf(...)" içinde last_dot "Stdio."'yu gösterir; cur_word ise
       imlecin hemen öncesindeki gerçek kelime olmalı, yoksa "printf" kalır. */
    char *last_dot = strrchr(prefix, '.');
    char *word_start = prefix + strlen(prefix);
    while (word_start > prefix && gcl_lsp_is_ident_char(word_start[-1])) word_start--;
    char cur_word[256];
    size_t wlen = 0;
    while (word_start[wlen] && gcl_lsp_is_ident_char(word_start[wlen]) && wlen < sizeof(cur_word)-1) {
        cur_word[wlen] = word_start[wlen];
        wlen++;
    }
    cur_word[wlen] = '\0';
    /* A word that starts with a digit or symbol is not a completion word (0, 123...) */
    if (cur_word[0] && !gcl_lsp_is_ident_start(cur_word[0])) cur_word[0] = '\0';

    /* Lexical guard: never complete inside a string/char literal or a comment.
       Preprocessor lines are excluded — #include "..." has its own rules. */
    if (!editor_line_is_preproc(line_str, col)) {
        EditorLexContext lex;
        editor_lex_context_at(text, b->cursor, &lex);
        if (lex.in_string || lex.in_comment) {
            free(text);
            ed->completion_visible = 0;
            ed->completion_no_match = 0;
            ed->completion_message[0] = '\0';
            return;
        }
    }

    /* tam metni workspace-aware tara: aktif dosya + #include recursive */
    LspSymbol *syms = NULL;
    int sym_count = 0;
    if (b->path) {
        gcl_lsp_scan(b->path, text, b->size, ed->cwd, &syms, &sym_count);
    }

    int cap = 0;

    /* === Python (.py) tamamlama === */
    {
        const char *ext = b->path ? strrchr(b->path, '.') : NULL;
        if (ext && strcmp(ext, ".py") == 0) {
            if (last_dot) {
                /* raylib.Vector2, raylib.InitWindow vb. */
                char module_name[256];
                size_t mod_len = (size_t)(last_dot - prefix);
                if (mod_len >= sizeof(module_name)) mod_len = sizeof(module_name) - 1;
                memcpy(module_name, prefix, mod_len);
                module_name[mod_len] = '\0';
                editor_trim_inplace(module_name);

                if (strcmp(module_name, "raylib") == 0 || strcmp(module_name, "rl") == 0) {
                    for (int pi = 0; gcl_python_raylib_names[pi]; pi++) {
                        if (cur_word[0] && strncmp(gcl_python_raylib_names[pi], cur_word, strlen(cur_word)) != 0) continue;
                        lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                     gcl_python_raylib_names[pi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                                     "raylib function", NULL, b->path);
                    }
                    lsp_list_clear(&syms, &sym_count);
                    free(text);
                    ed->completion_visible = (ed->completion_count > 0);
                    return;
                }
                if (strcmp(module_name, "math") == 0) {
                    static const char *math_names[] = {
                        "pi","e","sqrt","pow","floor","ceil","fabs","sin","cos","tan",
                        "asin","acos","atan","log","log10","exp","fmod","degrees","radians", NULL
                    };
                    for (int pi = 0; math_names[pi]; pi++) {
                        if (cur_word[0] && strncmp(math_names[pi], cur_word, strlen(cur_word)) != 0) continue;
                        lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                     math_names[pi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                                     "math member", NULL, b->path);
                    }
                    lsp_list_clear(&syms, &sym_count);
                    free(text);
                    ed->completion_visible = (ed->completion_count > 0);
                    return;
                }
            }

            /* import/from satırı → scripts altindaki .py + stdlib modülleri */
            {
                const char *p = prefix;
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, "import", 6) == 0 || strncmp(p, "from", 4) == 0) {
                    static const char *stdlib_mods[] = {
                        "raylib","raygui","gcl","math","random","sys","os","time",
                        "json","datetime","collections","itertools","functools", NULL
                    };
                    for (int mi = 0; stdlib_mods[mi]; mi++) {
                        if (cur_word[0] && strncmp(stdlib_mods[mi], cur_word, strlen(cur_word)) != 0) continue;
                        lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                     stdlib_mods[mi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                     "python module", NULL, b->path);
                    }
                    char scripts_dir[2048];
                    snprintf(scripts_dir, sizeof(scripts_dir), "%s/scripts", ed->cwd);
                    DIR *d = opendir(scripts_dir);
                    if (d) {
                        struct dirent *ent;
                        while ((ent = readdir(d)) != NULL) {
                            const char *name = ent->d_name;
                            if (name[0] == '.') continue;
                            const char *e = strrchr(name, '.');
                            if (!e || strcmp(e, ".py") != 0) continue;
                            char modname[256];
                            snprintf(modname, sizeof(modname), "%.*s", (int)(e - name), name);
                            if (cur_word[0] && strncmp(modname, cur_word, strlen(cur_word)) != 0) continue;
                            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                         modname, LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                         "python module", NULL, b->path);
                        }
                        closedir(d);
                    }
                    lsp_list_clear(&syms, &sym_count);
                    free(text);
                    ed->completion_visible = (ed->completion_count > 0);
                    return;
                }
            }

            /* Normal Python: keyword + builtin + raylib fonksiyonları */
            if (!last_dot) {
                for (int ki = 0; gcl_python_keywords[ki]; ki++) {
                    if (cur_word[0] && strncmp(gcl_python_keywords[ki], cur_word, strlen(cur_word)) != 0) continue;
                    lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                 gcl_python_keywords[ki], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                 "python keyword", NULL, b->path);
                }
                for (int pi = 0; gcl_python_raylib_names[pi]; pi++) {
                    if (cur_word[0] && strncmp(gcl_python_raylib_names[pi], cur_word, strlen(cur_word)) != 0) continue;
                    lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                 gcl_python_raylib_names[pi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                                 "raylib function", NULL, b->path);
                }
                lsp_list_clear(&syms, &sym_count);
                free(text);
                ed->completion_visible = (ed->completion_count > 0);
                return;
            }
        }
    }

    /* Preprocessor direktifleri tamamlama: #native, #include, #lib, #extern */
    {
        const char *nat = line_str;
        while (*nat == ' ' || *nat == '\t') nat++;
        if (nat[0] == '#') {
            const char *after_hash = nat + 1;
            while (*after_hash == ' ' || *after_hash == '\t') after_hash++;
            char dname[64];
            int di = 0;
            while (after_hash[di] && gcl_lsp_is_ident_char(after_hash[di]) && di < 63) {
                dname[di] = after_hash[di];
                di++;
            }
            dname[di] = '\0';
            const char *dval = after_hash + di;
            while (*dval == ' ' || *dval == '\t') dval++;
            int in_angle = 0;
            if (*dval == '<') { in_angle = 1; dval++; }
            else if (*dval == '"') dval++;
            char cur_dval[256];
            size_t dvl = 0;
            while (dval[dvl] && dval[dvl] != '>' && dval[dvl] != '"' &&
                   !isspace((unsigned char)dval[dvl]) && dvl < sizeof(cur_dval)-1) {
                cur_dval[dvl] = dval[dvl];
                dvl++;
            }
            cur_dval[dvl] = '\0';

            int handle = 0;
            /* Her # satırı özel işlenir — asla "there is no" gösterilmez */
            handle = 1;

            /* # veya kısmi direktif adı yazıldıysa direktif adlarını öner */
            {
                /* Every GCL preprocessor directive (simple_doc.md). */
                static const char *directives[] = {
                    "include","lib","native","extern","register","define","undef",
                    "if","ifdef","ifndef","elif","else","endif",
                    "pragma","warning","error","debug", NULL
                };
                int is_partial = 1;
                if (dname[0] != '\0') {
                    is_partial = 1;
                    for (int dmi = 0; directives[dmi]; dmi++) {
                        if (strcmp(directives[dmi], dname) == 0) {
                            is_partial = 0; /* tam ad yazılmış, direktif adı önerme */
                            break;
                        }
                    }
                }
                if (is_partial && !in_angle) {
                    for (int dmi = 0; directives[dmi]; dmi++) {
                        if (dname[0] && strncmp(directives[dmi], dname, strlen(dname)) != 0) continue;
                        lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                     directives[dmi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                     "directive", NULL, b->path);
                    }
                    handle = 1;
                }
            }

            if (strcmp(dname, "native") == 0) {
                /* '#native <Math>'  → module names inside the angle brackets.
                   '#native <Math>.randInt' → members of the closed module. */
                const char *lt = strchr(prefix, '<');
                const char *gt = lt ? strchr(lt, '>') : NULL;
                char module_name[256] = {0};
                if (lt) {
                    const char *ms = lt + 1;
                    const char *me = gt ? gt : prefix + strlen(prefix);
                    while (ms < me && (*ms == ' ' || *ms == '\t')) ms++;
                    while (me > ms && (me[-1] == ' ' || me[-1] == '\t')) me--;
                    size_t ml = (size_t)(me - ms);
                    if (ml >= sizeof(module_name)) ml = sizeof(module_name) - 1;
                    memcpy(module_name, ms, ml);
                    module_name[ml] = '\0';
                }
                const char *dot_after = gt ? strchr(gt, '.') : NULL;
                if (gt && dot_after) {
                    /* <Mod>.member → module members */
                    editor_add_native_members(ed, &cap, module_name, cur_word, b->path);
                } else if (gt) {
                    /* module is already closed — nothing to suggest on this line */
                } else {
                    const char *filter = in_angle ? cur_dval : cur_word;
                    for (int mi = 0; gcl_native_modules[mi]; mi++) {
                        if (filter[0] && strncmp(gcl_native_modules[mi], filter, strlen(filter)) != 0) continue;
                        lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                     gcl_native_modules[mi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                     "native module", NULL, b->path);
                    }
                }
                handle = 1;
            } else if (strcmp(dname, "pragma") == 0) {
                /* #pragma commandline — marks the program as a terminal app. */
                static const char *pragma_names[] = {"commandline","commendline","cmdline", NULL};
                const char *filter = in_angle ? cur_dval : cur_word;
                for (int pi = 0; pragma_names[pi]; pi++) {
                    if (filter[0] && strncmp(pragma_names[pi], filter, strlen(filter)) != 0) continue;
                    lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                 pragma_names[pi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                 "pragma", NULL, b->path);
                }
                handle = 1;
            } else if (strcmp(dname, "include") == 0 ||
                       strcmp(dname, "extern") == 0 ||
                       strcmp(dname, "lib") == 0) {
                const char *filter = in_angle ? cur_dval : cur_word;
                if (strcmp(dname, "include") == 0) {
                    char dirpath[2048];
                    snprintf(dirpath, sizeof(dirpath), "%s/include", ed->cwd);
                    DIR *d = opendir(dirpath);
                    if (d) {
                        struct dirent *ent;
                        while ((ent = readdir(d)) != NULL) {
                            const char *name = ent->d_name;
                            if (name[0] == '.') continue;
                            const char *e = strrchr(name, '.');
                            if (!e || strcmp(e, ".gcsf") != 0) continue;
                            if (filter[0] && strncmp(name, filter, strlen(filter)) != 0) continue;
                            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                         name, LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                         "include file", NULL, b->path);
                        }
                        closedir(d);
                    }
                    handle = 1;
                } else if (strcmp(dname, "extern") == 0) {
                    /* #extern <raylib.dll> → shared libraries in external/ */
                    char dirpath[2048];
                    snprintf(dirpath, sizeof(dirpath), "%s/external", ed->cwd);
                    DIR *d = opendir(dirpath);
                    if (d) {
                        struct dirent *ent;
                        while ((ent = readdir(d)) != NULL) {
                            const char *name = ent->d_name;
                            if (name[0] == '.') continue;
                            const char *e = strrchr(name, '.');
                            if (!e || (strcmp(e, ".dll") != 0 && strcmp(e, ".so") != 0 &&
                                       strcmp(e, ".dylib") != 0)) continue;
                            if (filter[0] && strncmp(name, filter, strlen(filter)) != 0) continue;
                            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                         name, LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                         "external library", NULL, b->path);
                        }
                        closedir(d);
                    }
                    handle = 1;
                } else {
                    /* #lib <x.gclib> → library scripts in lib/ */
                    char dirpath[2048];
                    snprintf(dirpath, sizeof(dirpath), "%s/lib", ed->cwd);
                    DIR *d = opendir(dirpath);
                    if (d) {
                        struct dirent *ent;
                        while ((ent = readdir(d)) != NULL) {
                            const char *name = ent->d_name;
                            if (name[0] == '.') continue;
                            const char *e = strrchr(name, '.');
                            if (!e || strcmp(e, ".gclib") != 0) continue;
                            if (filter[0] && strncmp(name, filter, strlen(filter)) != 0) continue;
                            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                         name, LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                         "library file", NULL, b->path);
                        }
                        closedir(d);
                    }
                    handle = 1;
                }
            }

            if (handle) {
                lsp_list_clear(&syms, &sym_count);
                free(text);
                ed->completion_visible = (ed->completion_count > 0);
                return;
            }
        }
    }

    /* Fonksiyon çağrısı parantezİ İÇİNDE → SADECE değer (var + macro) öner.
       last_dot'tan ÖNCE kontrol edilir; çünkü "Stdio.printf(" içinde '.' hâlâ prefix'te var. */
    {
        char *full_prefix = NULL;
        if (b->cursor > 0 && b->cursor <= b->size) {
            full_prefix = (char *)malloc(b->cursor + 1);
            if (full_prefix) {
                memcpy(full_prefix, text, b->cursor);
                full_prefix[b->cursor] = '\0';
            }
        }
        int in_args = full_prefix ? editor_inside_arguments(full_prefix) : 0;
        free(full_prefix);

        if (in_args) {
            for (int i = 0; i < sym_count; i++) {
                if (!syms[i].name || !syms[i].name[0]) continue;
                if (cur_word[0] && !editor_match_cur(syms[i].name, cur_word)) continue;
                if (syms[i].vis == LSP_VIS_PRIVATE &&
                    syms[i].file && b->path && strcmp(syms[i].file, b->path) != 0) continue;
                if (syms[i].kind != LSP_KIND_VAR && syms[i].kind != LSP_KIND_MACRO) continue;
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             syms[i].name, syms[i].kind, syms[i].vis,
                             syms[i].detail, syms[i].params, syms[i].file);
            }
            lsp_list_clear(&syms, &sym_count);
            free(text);
            if (ed->completion_count == 0 && cur_word[0] && manual) {
                snprintf(ed->completion_message, sizeof(ed->completion_message),
                         "there is no '%s'", cur_word);
                ed->completion_no_match = 1;
                ed->completion_visible = 1;
            } else {
                ed->completion_visible = (ed->completion_count > 0);
            }
            return;
        }
    }

    if (last_dot) {
        /* Nokta tamamlama SADECE TEK seviye: prefix'te ikinci bir nokta varsa
           ("testo.hello." gibi) zincirleme tamamlama YASAK — popup hiç açılmaz.
           Yalnızca #native modül ve struct/enum/typedef'in İLK noktasında çalışır. */
        if (strchr(prefix, '.') != last_dot) {
            lsp_list_clear(&syms, &sym_count);
            free(text);
            ed->completion_visible = 0;
            return;
        }

        /* "Test." → Test struct/unun üyelerini öner */
        char module_name[256];
        size_t mod_len = (size_t)(last_dot - prefix);
        if (mod_len >= sizeof(module_name)) mod_len = sizeof(module_name) - 1;
        memcpy(module_name, prefix, mod_len);
        module_name[mod_len] = '\0';
        editor_trim_inplace(module_name);

        /* #native modül üyeleri (Math., Stdio., Embed., Raylib., Raygui.) */
        int native_found = 0;
        if (strcmp(module_name, "Math") == 0) {
            for (int mi = 0; gcl_native_math_members[mi]; mi++) {
                if (cur_word[0] && !editor_match_cur(gcl_native_math_members[mi], cur_word)) continue;
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_native_math_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                             "Math member", NULL, b->path);
            }
            native_found = 1;
        } else if (strcmp(module_name, "Stdio") == 0) {
            for (int mi = 0; gcl_native_stdio_members[mi]; mi++) {
                if (cur_word[0] && !editor_match_cur(gcl_native_stdio_members[mi], cur_word)) continue;
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_native_stdio_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                             "Stdio member", native_member_params(gcl_native_stdio_members[mi]), b->path);
            }
            native_found = 1;
        } else if (strcmp(module_name, "Embed") == 0) {
            for (int mi = 0; gcl_native_embed_members[mi]; mi++) {
                if (cur_word[0] && !editor_match_cur(gcl_native_embed_members[mi], cur_word)) continue;
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_native_embed_members[mi], LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                             "Embed member", NULL, b->path);
            }
            native_found = 1;
        } else if (strcmp(module_name, "Raylib") == 0) {
            for (int ri = 0; gcl_Raylib_types[ri]; ri++) {
                const char *n = gcl_Raylib_types[ri];
                if (!n || !n[0]) continue;
                if (cur_word[0] && !editor_match_cur(n, cur_word)) continue;
                int is_type = 0;
                for (int ti = 0; gcl_Raylib_type_names[ti]; ti++) {
                    if (strcmp(n, gcl_Raylib_type_names[ti]) == 0) { is_type = 1; break; }
                }
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             n, is_type ? LSP_KIND_TYPE : LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                             is_type ? "Raylib type" : "Raylib function", NULL, b->path);
            }
            native_found = 1;
        } else if (strcmp(module_name, "Raygui") == 0) {
            for (int gi = 0; gcl_Raygui_types[gi]; gi++) {
                const char *n = gcl_Raygui_types[gi];
                if (!n || !n[0]) continue;
                if (cur_word[0] && !editor_match_cur(n, cur_word)) continue;
                int is_type = 0;
                for (int ti = 0; gcl_Raygui_type_names[ti]; ti++) {
                    if (strcmp(n, gcl_Raygui_type_names[ti]) == 0) { is_type = 1; break; }
                }
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             n, is_type ? LSP_KIND_TYPE : LSP_KIND_FUNC, LSP_VIS_PUBLIC,
                             is_type ? "Raygui type" : "Raygui function", NULL, b->path);
            }
            native_found = 1;
        }

        if (!native_found) {
            /* Nokta tamamlama SADECE #native modüller (yukarıda) ve
               struct/enum/typedef tipleri için tetiklenir.
               Rastgele bir değişken (VAR) adı ise hiçbir şey gösterilmez.
               Aynı isim hem VAR hem TYPE olarak kayıtlıysa VAR tercih edilir
               ve popup KAPANIR — "int_var.int_var" zinciri imkânsız olur. */
            if (module_name[0] == '\0') {
                lsp_list_clear(&syms, &sym_count);
                free(text);
                ed->completion_visible = 0;
                return;
            }
            int is_type_sym = 0;
            int is_var_sym  = 0;
            for (int i = 0; i < sym_count; i++) {
                if (syms[i].name && strcmp(syms[i].name, module_name) == 0) {
                    if (syms[i].kind == LSP_KIND_TYPE) is_type_sym = 1;
                    else if (syms[i].kind == LSP_KIND_VAR) is_var_sym = 1;
                }
            }
            if (is_type_sym && !is_var_sym) {
                add_members_for(syms, sym_count, module_name, cur_word,
                                b->path ? b->path : "",
                                &ed->completions, &ed->completion_count, &cap);
                /* Tip bulundu ama hiç üyesi/uygun üyesi yoksa popup kapanır */
                if (ed->completion_count == 0) {
                    ed->completion_visible = 0;
                }
            }
        }
    } else {
        /* önek ile eşleşen tüm tanımları öner (struct/enum/typedef + enum değerleri).
           PRIVATE semboller yalnızca tanımlandıkları dosyada görünür. */
        int in_args = editor_inside_arguments(prefix);
        int is_value_assign = editor_is_value_assign_context(line_str, col, syms, sym_count);
        int is_type_decl = editor_is_type_decl_context(prefix);

        /* GCL keyword + primitive type tamamlama — fonksiyon çağrısı İÇİNDE önerilmez.
           gcl_keywords[] (ide_font.c) + gcl_builtin_types[] (gcl_lsp_internal.h) birleştirilir.
           BAĞLAM FİLTRELERİ:
           - value_assign ("Type var = ") → keyword/builtin önerilmez (sadece değerler)
           - type_decl ("struct X {") → sadece tip adları önerilir                       */
        if (!in_args) {
            for (int ki = 0; gcl_keywords[ki]; ki++) {
                if (cur_word[0] && !editor_match_cur(gcl_keywords[ki], cur_word)) continue;
                int kw_is_type = 0;
                for (int pi = 0; gcl_builtin_types[pi]; pi++) {
                    if (strcmp(gcl_keywords[ki], gcl_builtin_types[pi]) == 0) { kw_is_type = 1; break; }
                }
                if (is_value_assign) continue;                       /* değer bağlamında keyword yok */
                if (is_type_decl && !kw_is_type) continue;           /* tip tanımında sadece tip */
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_keywords[ki], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                             kw_is_type ? "type" : "keyword", NULL, b->path);
            }
            for (int pi = 0; gcl_builtin_types[pi]; pi++) {
                if (cur_word[0] && !editor_match_cur(gcl_builtin_types[pi], cur_word)) continue;
                int dupe = 0;
                for (int ki = 0; gcl_keywords[ki]; ki++) {
                    if (strcmp(gcl_builtin_types[pi], gcl_keywords[ki]) == 0) { dupe = 1; break; }
                }
                if (dupe) continue;
                if (is_value_assign) continue;                       /* değer bağlamında tip yok */
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_builtin_types[pi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                             "type", NULL, b->path);
            }
            /* #native modülleri (Math/Stdio/Embed/Raylib/Raygui) normal kod
               içinde de tanınsın: 'Stdio' yazıp Ctrl+Space denildiğinde eskiden
               "there is no 'Stdio'" çıkıyordu; modül adı yalnızca `#native <X>`
               satırında görünüyordu (todo #2). Üye erişimi (Stdio.printf) için
               bkz. editor_add_native_members / last_dot dalı. */
            for (int nmi = 0; gcl_native_modules[nmi]; nmi++) {
                if (!editor_is_native_module(gcl_native_modules[nmi])) continue;
                if (is_value_assign || is_type_decl) continue;  /* değer/tip bağlamında namespace önerilmez */
                if (cur_word[0] && !editor_match_cur(gcl_native_modules[nmi], cur_word)) continue;
                lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                             gcl_native_modules[nmi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                             "native module", NULL, b->path);
            }
        }

        for (int i = 0; i < sym_count; i++) {
            if (!syms[i].name || !syms[i].name[0]) continue;
            if (cur_word[0] && !editor_match_cur(syms[i].name, cur_word)) continue;
            if (syms[i].vis == LSP_VIS_PRIVATE &&
                syms[i].file && b->path && strcmp(syms[i].file, b->path) != 0) continue;
            /* #native modülleri zaten yukarıdaki native loop ile eklendi.
               Tarayıcı da aynı modülü TYPE olarak ürettiği için burada tekrar
               eklemeyelim — çift "Stdio" girdisini önler (todo #2). */
            if (gcl_is_native_module(syms[i].name)) continue;
            /* Fonksiyon çağrısı içinde DEĞER öner — fonksiyon/tip adı gösterme */
            if (in_args && syms[i].kind != LSP_KIND_VAR && syms[i].kind != LSP_KIND_MACRO) continue;
            /* "Type var = " → SADECE değer (var + macro) */
            if (is_value_assign && syms[i].kind != LSP_KIND_VAR && syms[i].kind != LSP_KIND_MACRO) continue;
            /* "struct X {" → SADECE tip adları */
            if (is_type_decl && syms[i].kind != LSP_KIND_TYPE) continue;
            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                         syms[i].name, syms[i].kind, syms[i].vis,
                         syms[i].detail, syms[i].params, syms[i].file);
        }
    }

    /* Eşleşme yoksa "there is no 'X'" göster — yalnızca KULLANICI açıkça istediyse
       (manual) ve değer oluşturma bağlamında DEĞİLSE. Otomatik/dot tetiklemesinde
       bu mesaj gösterilmez (kullanıcı geri bildirimi: alakasız "there is no"). */
    if (ed->completion_count == 0 && manual) {
        int is_decl = editor_is_declaration_context(line_str, col, cur_word,
                                                    syms, sym_count);
        if (!is_decl && cur_word[0]) {
            snprintf(ed->completion_message, sizeof(ed->completion_message),
                     "there is no '%s'", cur_word);
            ed->completion_no_match = 1;
            ed->completion_visible = 1; /* boş liste + mesaj görünsün */
        }
    }

    lsp_list_clear(&syms, &sym_count);
    free(text);

    /* akıllı sıralama: yerel/önceliğe göre (VAR>FUNC>MACRO>TYPE) + alfabetik.
       qsort context alamaz; aktif dosya yolunu globalde geçiririz. */
    if (ed->completion_count > 0) {
        g_sort_current_file = b->path;
        qsort(ed->completions, (size_t)ed->completion_count,
              sizeof(LspSymbol), editor_cmp_completion);
        g_sort_current_file = NULL;
        /* VSCode benzeri: en iyi eşleşmeyi otomatik seç (önce prefix, sonra fuzzy,
           sonra alfabetik ilk). İlk eleman zaten VAR>FUNC>MACRO>TYPE önceliğinde. */
        ed->completion_selected = 0;
        if (cur_word[0]) {
            int best = 0;
            int best_score = 0;
            for (int i = 0; i < ed->completion_count; i++) {
                const char *nm = ed->completions[i].name;
                if (!nm) continue;
                int score = 0;
                if (editor_strnicmp(nm, cur_word, strlen(cur_word)) == 0) score = 100;
                else if (editor_fuzzy_match(nm, cur_word)) score = 50;
                else if (editor_stricmp(nm, cur_word) == 0) score = 200;
                if (score > best_score) { best_score = score; best = i; }
            }
            ed->completion_selected = best;
        }
    }
    if (ed->completion_count > 0) ed->completion_visible = 1;
}

void editor_accept_completion(Editor *ed) {
    if (!ed->completion_visible || ed->completion_count <= 0) return;
    int idx = ed->completion_selected;
    if (idx < 0 || idx >= ed->completion_count) return;

    LspSymbol *s = &ed->completions[idx];
    if (!s->name || !s->name[0]) return;

    GclIdeBuffer *b = &ed->tabs[ed->active_tab];

    size_t line = gcl_ide_buffer_line_of_cursor(b);
    size_t col = gcl_ide_buffer_cursor_in_line(b);
    size_t l = 0;
    const char *line_str = gcl_ide_buffer_line_at(b, line, &l);
    if (col > l) col = l;

    char prefix[1024];
    size_t plen = col;
    if (plen >= sizeof(prefix)) plen = sizeof(prefix) - 1;
    memcpy(prefix, line_str, plen);
    prefix[plen] = '\0';

    /* aktif kelimenin başlangıcını bul — imleçten geriye doğru */
    char *last_dot = strrchr(prefix, '.');
    char *word_start = prefix + strlen(prefix);
    while (word_start > prefix && gcl_lsp_is_ident_char(word_start[-1])) word_start--;

    /* aktif kelimeyi sil — rakamla başlayan yazım (0, 123) tamamlama kelimesi değildir */
    size_t wlen = 0;
    if (word_start < prefix + strlen(prefix) && gcl_lsp_is_ident_start(*word_start)) {
        while (word_start[wlen] && gcl_lsp_is_ident_char(word_start[wlen])) wlen++;
    }
    while (wlen > 0) { gcl_ide_buffer_backspace(b); wlen--; }

    /* etiketi yaz — "Test._test1" ise sadece dot sonrasını ekle.
       AMA dosya adları (#include <test.gcsf>) ve direktif isimleri için
       dot-split UYGULANMAZ — aksi halde "test.gcsf" → "gcsf" yazılır. */
    const char *insert_name = s->name;
    int is_external_name = (s->detail &&
                            (strstr(s->detail, "include file") != NULL ||
                             strstr(s->detail, "external library") != NULL ||
                             strstr(s->detail, "directive") != NULL));
    if (!is_external_name) {
        char *dot_in_name = strrchr(s->name, '.');
        if (dot_in_name) insert_name = dot_in_name + 1;
    }
    for (const char *q = insert_name; *q; q++) gcl_ide_buffer_insert_char(b, *q);

    /* fonksiyon ise () ekle */
    if (s->kind == LSP_KIND_FUNC) {
        gcl_ide_buffer_insert_char(b, '(');
        gcl_ide_buffer_insert_char(b, ')');
        gcl_ide_buffer_cursor_left(b);
    }

    ed->completion_visible = 0;
    ed->completion_no_match = 0;
    ed->completion_message[0] = '\0';
    ed->sel_anchor = b->cursor;
}
   
/* ---------------------------------------------
   Selection / clipboard
   --------------------------------------------- */

size_t sel_start(Editor *ed) {
    return ed->sel_anchor < CURP.cursor ? ed->sel_anchor : CURP.cursor;
}

size_t sel_end(Editor *ed) {
    return ed->sel_anchor < CURP.cursor ? CURP.cursor : ed->sel_anchor;
}

int selection_active(Editor *ed) {
    return ed->sel_anchor != CURP.cursor;
}

void editor_copy(Editor *ed) {
    if (!selection_active(ed)) return;
    size_t s0 = sel_start(ed), s1 = sel_end(ed);
    if (!CURP.content || s1 > CURP.size) return;
    size_t len = s1 - s0;
    char *nt = (char *)realloc(ed->clip_text, len + 1);
    if (!nt) return;
    ed->clip_text = nt;
    memcpy(ed->clip_text, CURP.content + s0, len);
    ed->clip_text[len] = '\0';
    ed->clip_len = len;
    gcl_ide_clipboard_set_text(ed->clip_text, ed->clip_len);
}

void editor_cut(Editor *ed) {
    if (!selection_active(ed)) return;
    editor_copy(ed);
    size_t s0 = sel_start(ed), s1 = sel_end(ed);
    buffer_delete_range(&CURP, s0, s1);
    ed->sel_anchor = CURP.cursor;
}

void editor_paste(Editor *ed) {
    /* Seçili metin varsa yapıştırılan metin onun ÜZERİNE yazılır. */
    if (selection_active(ed)) {
        size_t s0 = sel_start(ed), s1 = sel_end(ed);
        buffer_delete_range(&CURP, s0, s1);
        ed->sel_anchor = CURP.cursor;
    }
    /* Önce sistem panosundan al; yoksa iç clip buffer kullan.
       Windows pano metnindeki '\r' (CRLF) editörde '?'/kare olarak görünüyordu;
       CR atlanır, yalnızca LF korunur. */
    char *sys_text = gcl_ide_clipboard_get_text();
    if (sys_text) {
        for (const char *p = sys_text; *p; p++) {
            if (*p == '\r') continue;
            gcl_ide_buffer_insert_char(&CURP, *p);
        }
        ed->sel_anchor = CURP.cursor;
        free(sys_text);
        return;
    }
    if (!ed->clip_text || ed->clip_len == 0) return;
    for (size_t i = 0; i < ed->clip_len; i++) {
        if (ed->clip_text[i] == '\r') continue;
        gcl_ide_buffer_insert_char(&CURP, ed->clip_text[i]);
    }
    ed->sel_anchor = CURP.cursor;
}

void editor_select_all(Editor *ed) {
    ed->sel_anchor = 0;
    CURP.cursor = CURP.size;
}

/* Belirli bir bayt offset'inin kaçıncı satırda olduğunu döndür. */
static size_t editor_line_of_byte(const GclIdeBuffer *b, size_t pos) {
    size_t line = 0;
    if (pos > b->size) pos = b->size;
    for (size_t i = 0; i < pos; i++) {
        if (b->content[i] == '\n') line++;
    }
    return line;
}

/* Belirli bir satırın başlangıç bayt offset'ini döndür. */
static size_t editor_line_start_byte(const GclIdeBuffer *b, size_t line) {
    size_t cur = 0, start = 0;
    for (size_t i = 0; i < b->size; i++) {
        if (cur == line) return start;
        if (b->content[i] == '\n') { cur++; start = i + 1; }
    }
    return start;
}

/* Dosya SEKME (TAB) karakteriyle mi girintilenmiş? Satır başında '\t' varsa
   evet. Python dosyaları sekme ile girintilenebilir; Tab tuşu ve otomatik
   girinti bu dosyalarda '\t' yazar (todo #1: "IDE python sekmelerini
   desteklemiyor"). */
static int editor_uses_tab_indent(const GclIdeBuffer *b) {
    if (!b || !b->content) return 0;
    size_t i = 0;
    while (i < b->size) {
        if (b->content[i] == '\t') return 1;
        while (i < b->size && b->content[i] != '\n') i++;
        i++;
    }
    return 0;
}

/* Girinti birimi: sekme kullanan dosyada '\t', aksi halde 4 boşluk. */
static void editor_indent_unit(const GclIdeBuffer *b, char *out, size_t outsz, size_t *out_len) {
    if (outsz == 0) return;
    if (editor_uses_tab_indent(b)) {
        out[0] = '\t';
        if (outsz > 1) out[1] = '\0';
        if (out_len) *out_len = 1;
        return;
    }
    const char *sp = "    ";
    size_t n = strlen(sp);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, sp, n);
    out[n] = '\0';
    if (out_len) *out_len = n;
}

/* Sekme tuşu — seçim YOKKEN imlece girinti ekler. Eskiden Tab yalnızca satır
   girintiliyordu ve imleç satır başına sıçradığı için yazılan metin girintinin
   SOLUNA düşüyordu ("tab çalışmıyor" geri bildirimi). */
void editor_insert_tab(Editor *ed) {
    GclIdeBuffer *b = &CURP;
    if (!b) return;
    char unit[8];
    size_t un = 0;
    editor_indent_unit(b, unit, sizeof(unit), &un);
    for (size_t i = 0; i < un; i++) gcl_ide_buffer_insert_char(b, unit[i]);
    ed->sel_anchor = b->cursor;
}

/* Enter: yeni satır + otomatik gövde girintisi (Python/GCL).
   - Geçerli satırın baştaki boşluğu kopyalanır.
   - Satırın imlece kadarki son anlamlı karakteri ':' ise bir girinti birimi eklenir.
   - İmleçten sonraki ilk anlamlı karakter kapatıcıysa ({ } ) ]) girinti eklenmez.
   - Satır else/elif/except/finally ise girinti bir birim azaltılır. */
void editor_insert_newline_autoindent(Editor *ed) {
    GclIdeBuffer *b = &CURP;
    if (!b || !b->content || b->size == 0) {
        gcl_ide_buffer_insert_newline(b);
        if (ed) ed->sel_anchor = b->cursor;
        return;
    }
    size_t line = gcl_ide_buffer_line_of_cursor(b);
    size_t l = 0;
    const char *ls = gcl_ide_buffer_line_at(b, line, &l);
    size_t line_off = (size_t)(ls - b->content);

    size_t ind = 0;
    while (ind < l && (ls[ind] == ' ' || ls[ind] == '\t')) ind++;
    char indent[512];
    if (ind >= sizeof(indent)) ind = sizeof(indent) - 1;
    memcpy(indent, ls, ind);
    indent[ind] = '\0';

    size_t curcol = (b->cursor > line_off) ? b->cursor - line_off : 0;
    if (curcol > l) curcol = l;

    int opens = 0;
    for (size_t i = curcol; i-- > 0; ) {
        char c = ls[i];
        if (c == ' ' || c == '\t' || c == '\r') continue;
        opens = (c == ':');
        break;
    }
    int closes = 0;
    for (size_t i = curcol; i < l; i++) {
        char c = ls[i];
        if (c == ' ' || c == '\t' || c == '\r') continue;
        closes = (c == '}' || c == ')' || c == ']');
        break;
    }
    int dedent_kw = 0;
    {
        static const char *kw[] = { "else", "elif", "except", "finally", NULL };
        char word[32];
        size_t wl = 0;
        size_t i = ind;
        while (i < l && gcl_lsp_is_ident_char(ls[i]) && wl < sizeof(word) - 1) word[wl++] = ls[i++];
        word[wl] = '\0';
        for (int k = 0; kw[k]; k++) {
            if (wl > 0 && strcmp(word, kw[k]) == 0) { dedent_kw = 1; break; }
        }
    }

    char unit[8];
    size_t un = 0;
    editor_indent_unit(b, unit, sizeof(unit), &un);

    size_t n = ind;
    if (dedent_kw) {
        if (n > 0 && indent[n - 1] == '\t') n -= 1;
        else n = (n >= 4) ? n - 4 : 0;
    }

    char out[600];
    if (n > sizeof(out) - 1) n = sizeof(out) - 1;
    memcpy(out, indent, n);
    size_t on = n;
    if (opens && !closes && un > 0 && on + un < sizeof(out)) {
        memcpy(out + on, unit, un);
        on += un;
    }
    out[on] = '\0';

    gcl_ide_buffer_insert_char(b, '\n');
    for (size_t i = 0; i < on; i++) gcl_ide_buffer_insert_char(b, out[i]);
    ed->sel_anchor = b->cursor;
}

/* Tab: seçili satırları (yoksa imleç satırını) girintiler.
   Shift+Tab (outdent): satır başındaki bir TAB veya en fazla 4 boşluğu kaldırır.
   İmleç ve seçim çapası, girintiden sonra satır içi konumlarını KORUR
   (eskiden ikisi de satır başına sıçrıyordu). */
void editor_indent_selection(Editor *ed, int outdent) {
    GclIdeBuffer *b = &CURP;
    if (!b || !b->content || b->size == 0) return;
    size_t sel0 = sel_start(ed), sel1 = sel_end(ed);
    size_t first = editor_line_of_byte(b, sel0);
    size_t last  = editor_line_of_byte(b, sel1);
    /* Seçim bir satırın başında bitiyorsa son satırı dahil etme. */
    if (sel0 != sel1 && sel1 > 0 && b->content[sel1 - 1] == '\n' && last > first) last--;

    /* İmleç ve çapanın satır içi offset'lerini sakla. */
    size_t cursor_line = editor_line_of_byte(b, b->cursor);
    size_t cursor_off  = b->cursor - editor_line_start_byte(b, cursor_line);
    size_t anchor_line = editor_line_of_byte(b, ed->sel_anchor);
    size_t anchor_off  = ed->sel_anchor - editor_line_start_byte(b, anchor_line);

    char unit[8];
    size_t unit_len = 0;
    editor_indent_unit(b, unit, sizeof(unit), &unit_len);

    long cursor_shift = 0, anchor_shift = 0;
    /* Sondan başa doğru işle — offset kayması önceki satırları etkilemesin. */
    for (size_t ln = last + 1; ln-- > first; ) {
        size_t ls = editor_line_start_byte(b, ln);
        b->cursor = ls;
        long delta = 0;
        if (outdent) {
            if (ls < b->size && b->content[ls] == '\t') {
                gcl_ide_buffer_delete(b);
                delta = -1;
            } else {
                for (int k = 0; k < 4 && b->cursor < b->size &&
                     b->content[b->cursor] == ' '; k++) {
                    gcl_ide_buffer_delete(b);
                    delta -= 1;
                }
            }
        } else {
            for (size_t k = 0; k < unit_len; k++) gcl_ide_buffer_insert_char(b, unit[k]);
            delta = (long)unit_len;
        }
        if (ln == cursor_line) cursor_shift = delta;
        if (ln == anchor_line) anchor_shift = delta;
    }
    /* İmleç/çapa konumlarını geri yükle (satır içi offset korunur). */
    {
        long off = (long)cursor_off + cursor_shift;
        if (off < 0) off = 0;
        b->cursor = editor_line_start_byte(b, cursor_line) + (size_t)off;
        if (b->cursor > b->size) b->cursor = b->size;
        long aoff = (long)anchor_off + anchor_shift;
        if (aoff < 0) aoff = 0;
        ed->sel_anchor = editor_line_start_byte(b, anchor_line) + (size_t)aoff;
        if (ed->sel_anchor > b->size) ed->sel_anchor = b->size;
    }
}

/* ---------------------------------------------
   Typing
   --------------------------------------------- */

void editor_process_typing(Editor *ed, int ctrl, int shift) {
    int ch;
    int typed_any = 0;
    int typed_dot = 0;
    int last_char = 0;
    while ((ch = GetCharPressed()) != 0) {
        if (ctrl) continue;  /* Ctrl+harf kombinasyonlarını metin olarak ekleme */
        if (shift && ch == ' ') continue; /* Shift+Space: boşluk yazma (kısayol toggle) */
        last_char = ch;
        /* Seçili metin varken yazılan karakter seçimin ÜZERİNE yazar:
           önce seçimi sil, sonra karakteri ekle. */
        if (selection_active(ed)) {
            size_t s0 = sel_start(ed), s1 = sel_end(ed);
            buffer_delete_range(&CURP, s0, s1);
            ed->sel_anchor = CURP.cursor;
        }
        if (ch == '.') typed_dot = 1;
        if (ch == '\r' || ch == '\n') {
        } else if (ch == '(') { editor_handle_bracket_close(ed, '(', ')'); }
        else if (ch == ')') {
            if (CURP.cursor < CURP.size && CURP.content[CURP.cursor] == ')') {
                gcl_ide_buffer_cursor_right(&CURP);
                ed->sel_anchor = CURP.cursor;
            } else {
                gcl_ide_buffer_insert_char(&CURP, ')');
                ed->sel_anchor = CURP.cursor;
            }
        }
        else if (ch == '{') editor_handle_bracket_close(ed, '{', '}');
        else if (ch == '}') {
            if (CURP.cursor < CURP.size && CURP.content[CURP.cursor] == '}') {
                gcl_ide_buffer_cursor_right(&CURP);
                ed->sel_anchor = CURP.cursor;
            } else {
                gcl_ide_buffer_insert_char(&CURP, '}');
                ed->sel_anchor = CURP.cursor;
            }
        }
        else if (ch == '[') editor_handle_bracket_close(ed, '[', ']');
        else if (ch == ']') {
            if (CURP.cursor < CURP.size && CURP.content[CURP.cursor] == ']') {
                gcl_ide_buffer_cursor_right(&CURP);
                ed->sel_anchor = CURP.cursor;
            } else {
                gcl_ide_buffer_insert_char(&CURP, ']');
                ed->sel_anchor = CURP.cursor;
            }
        }
        else if (ch == '"') { gcl_ide_buffer_insert_char(&CURP, '"'); gcl_ide_buffer_insert_char(&CURP, '"'); gcl_ide_buffer_cursor_left(&CURP); ed->sel_anchor = CURP.cursor; }
        else if (ch == '\'') { gcl_ide_buffer_insert_char(&CURP, '\''); gcl_ide_buffer_insert_char(&CURP, '\''); gcl_ide_buffer_cursor_left(&CURP); ed->sel_anchor = CURP.cursor; }
        else {
            /* Tamamlama penceresi açıkken ';' yazınca pencereyi KAPAT.
               Otomatik tamamlama yazarken açılmaz; yalnızca açıkken ';' kapatır. */
            if (ed->completion_visible && ch == ';') {
                gcl_ide_buffer_insert_char(&CURP, ';');
                ed->sel_anchor = CURP.cursor;
                ed->completion_visible = 0;
                ed->completion_no_match = 0;
                ed->completion_message[0] = '\0';
                continue;
            }
            if (ch < 128) {
                gcl_ide_buffer_insert_char(&CURP, (char)ch);
            } else {
                unsigned char u8[4];
                int ulen;
                if (ch < 0x800) {
                    u8[0] = (unsigned char)(0xC0 | (ch >> 6));
                    u8[1] = (unsigned char)(0x80 | (ch & 0x3F));
                    ulen = 2;
                } else if (ch < 0x10000) {
                    u8[0] = (unsigned char)(0xE0 | (ch >> 12));
                    u8[1] = (unsigned char)(0x80 | ((ch >> 6) & 0x3F));
                    u8[2] = (unsigned char)(0x80 | (ch & 0x3F));
                    ulen = 3;
                } else {
                    u8[0] = (unsigned char)(0xF0 | (ch >> 18));
                    u8[1] = (unsigned char)(0x80 | ((ch >> 12) & 0x3F));
                    u8[2] = (unsigned char)(0x80 | ((ch >> 6) & 0x3F));
                    u8[3] = (unsigned char)(0x80 | (ch & 0x3F));
                    ulen = 4;
                }
                char u8s[5];
                memcpy(u8s, u8, (size_t)ulen);
                u8s[ulen] = '\0';
                gcl_ide_buffer_insert_utf8(&CURP, u8s);
            }
            typed_any = 1; ed->sel_anchor = CURP.cursor;
        }
    }
    if (typed_any) {
        ed->last_typed_key = GetTime();
        ed->edited_this_frame = 1;
        if (ed->typewriter) editor_typewriter_sound(ed, 0); /* normal daktilo sesi */
        ed->completion_dismissed = 0; /* yeni yazım — ESC durumu sıfırlanır */
    }
    /* Otomatik tamamlama YALNIZCA bilinçli tetikleyicilerle açılır:
       - '.' yazıldığında (nesne.üye / #native <Mod>.üye)
       - Ctrl+Space (ide_main.c)
       Normal harf yazımında kendiliğinden AÇILMAZ; ESC ile kapatıldığında
       tekrar açılmaz (kullanıcı geri bildirimi: popup sürekli açılıyordu). */
    if (typed_dot) {
        ed->completion_dismissed = 0;
        editor_show_completion(ed, 0);
    } else if (ed->completion_visible && last_char) {
        if (editor_is_ident_char((char)last_char)) {
            editor_show_completion(ed, 0);  /* açık popup'ın üye önekini canlı filtrele */
        } else {
            ed->completion_visible = 0;
            ed->completion_no_match = 0;
            ed->completion_message[0] = '\0';
        }
    }
}
