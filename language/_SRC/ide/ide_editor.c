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
static const char *gcl_native_math_members[] = {
    "randInt","randint","randFloat","randfloat","min","max","abs","floor","ceil","round",
    "sqrt","pow","sin","cos","tan","asin","acos","atan","log","log10","exp","clamp","sign",
    "randBool","randChoice","randSign", NULL
};
static const char *gcl_native_stdio_members[] = {
    "printf","scanf","openFile","openfile","writeFile","writefile","readFile","readfile",
    "closeFile","closefile","appendFile","appendfile","fileExists","fileexists","deleteFile",
    "deletefile","renameFile","renamefile","fileSize","filesize","flushFile","flushfile", NULL
};
static const char *gcl_native_embed_members[] = {
    "Run","Stop","IsActive","GetValue","SendValue", NULL
};

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

/* Dosya kökeni sıralama için aktif dosya yolu (qsort context sağlayamaz). */
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
    for (const char *p = prefix; *p; p++) {
        if (quote) {
            if (*p == quote && p[-1] != '\\') quote = 0;
            continue;
        }
        if (*p == '"' || *p == '\'') { quote = *p; continue; }
        if (*p == '(') depth++;
        else if (*p == ')') { if (depth > 0) depth--; }
    }
    return depth > 0;
}

void editor_show_completion(Editor *ed) {
    /* eski listeyi temizle */
    lsp_list_clear(&ed->completions, &ed->completion_count);
    ed->completion_selected = 0;
    ed->completion_visible = 0;
    ed->completion_no_match = 0;
    ed->completion_message[0] = '\0';

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
    /* Rakam veya özel karakterle başlayan yazım tamamlama kelimesi sayılmaz (0, 123...) */
    if (cur_word[0] && !gcl_lsp_is_ident_start(cur_word[0])) cur_word[0] = '\0';

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
                gcl_lsp_trim(module_name);

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
                static const char *directives[] = {"include","native","extern","register","define", NULL};
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
                static const char *native_mods[] = {"Math","Stdio","Embed","Raylib","Raygui", NULL};
                const char *filter = in_angle ? cur_dval : cur_word;
                for (int mi = 0; native_mods[mi]; mi++) {
                    if (filter[0] && strncmp(native_mods[mi], filter, strlen(filter)) != 0) continue;
                    lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                 native_mods[mi], LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                 "native module", NULL, b->path);
                }
                handle = 1;
            } else if (strcmp(dname, "include") == 0 ||
                       strcmp(dname, "extern") == 0) {
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
                } else { /* extern */
                    char dirpath[2048];
                    snprintf(dirpath, sizeof(dirpath), "%s/external", ed->cwd);
                    DIR *d = opendir(dirpath);
                    if (d) {
                        struct dirent *ent;
                        while ((ent = readdir(d)) != NULL) {
                            const char *name = ent->d_name;
                            if (name[0] == '.') continue;
                            const char *e = strrchr(name, '.');
                            if (!e || (strcmp(e, ".dll") != 0 && strcmp(e, ".so") != 0)) continue;
                            if (filter[0] && strncmp(name, filter, strlen(filter)) != 0) continue;
                            lsp_list_add(&ed->completions, &ed->completion_count, &cap,
                                         name, LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                         "external library", NULL, b->path);
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
            if (ed->completion_count == 0 && cur_word[0]) {
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
        gcl_lsp_trim(module_name);

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
        }

        for (int i = 0; i < sym_count; i++) {
            if (!syms[i].name || !syms[i].name[0]) continue;
            if (cur_word[0] && !editor_match_cur(syms[i].name, cur_word)) continue;
            if (syms[i].vis == LSP_VIS_PRIVATE &&
                syms[i].file && b->path && strcmp(syms[i].file, b->path) != 0) continue;
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

    /* Eşleşme yoksa "there is no 'X'" göster — AMA değer oluşturma bağlamında GİZLE */
    if (ed->completion_count == 0) {
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
    /* Önce sistem panosundan al; yoksa iç clip buffer kullan. */
    char *sys_text = gcl_ide_clipboard_get_text();
    if (sys_text) {
        for (const char *p = sys_text; *p; p++) gcl_ide_buffer_insert_char(&CURP, *p);
        ed->sel_anchor = CURP.cursor;
        free(sys_text);
        return;
    }
    if (!ed->clip_text || ed->clip_len == 0) return;
    for (size_t i = 0; i < ed->clip_len; i++) gcl_ide_buffer_insert_char(&CURP, ed->clip_text[i]);
    ed->sel_anchor = CURP.cursor;
}

void editor_select_all(Editor *ed) {
    ed->sel_anchor = 0;
    CURP.cursor = CURP.size;
}

/* ---------------------------------------------
   Typing
   --------------------------------------------- */

void editor_process_typing(Editor *ed, int ctrl, int shift) {
    int ch;
    int typed_any = 0;
    while ((ch = GetCharPressed()) != 0) {
        if (ctrl) continue;  /* Ctrl+harf kombinasyonlarını metin olarak ekleme */
        if (shift && ch == ' ') continue; /* Shift+Space: boşluk yazma (kısayol toggle) */
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
        /* VSCode benzeri: yazarken tamamlama DEBOUNCE ile açılır (200ms gecikme).
           Hızlı yazmada popup sürekli açılıp kapanmaz; yazma DURUNCA açılır.
           Ctrl+Space yine anında zorla açar (ide_main.c'de). */
        ed->completion_debounce_until = GetTime() + 0.2;
    }
}
