/*
 * gcl_lsp_scan.c — GCL workspace-aware sembol tarayıcı (IDE otomatik tamamlama için).
 *
 * Aktif dosyadan ve #include <x.gcsf> ile yüklenen dosyalardan şunları çıkarır:
 *   - değişkenler (STMT_VAR_DECL)
 *   - fonksiyonlar  (func(...) { ... })
 *   - struct        (struct Name { ... })  → members
 *   - enum          (enum Name { A, B })  → members (enum değerleri)
 *   - typedef       (typedef int Number; / typedef struct {...} Name;)
 *   - macro         (#define NAME ...)
 *
 * LspSymbol listesi döndürür (gcl_lsp_internal.h'deki inline yardımcılarla bellek yönetimi).
 */

#include "gcl_lsp_internal.h"
#include "gcl_ide_internal.h"  // gcl_strdup, path_basename vb. için

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GCL_LSP_MAX_DEPTH 8
#define GCL_LSP_MAX_SYMS 4096

/* satırı kopyala: 'dst' en fazla 'cap' bayt; yeni satır/null kesilir */
static void copy_line(const char *src, size_t len, char *dst, size_t cap) {
    size_t n = len;
    while (n > 0 && (src[n-1] == '\n' || src[n-1] == '\r')) n--;
    if (n >= cap) n = cap - 1;
    if (n > 0) memcpy(dst, src, n);
    dst[n] = '\0';
}

/* baştaki boşlukları atla */
static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return p;
}

/* sol baştaki boşlukları atla (satır başına döner) */
static const char *line_start_ws(const char *line, const char *p) {
    while (p > line && (p[-1] == ' ' || p[-1] == '\t')) p--;
    return p;
}

/* 'p' bir ident başlangıcı mı? */
static int is_ident_start(char c) {
    return gcl_lsp_is_ident_start(c);
}

/* 'p' bir ident karakteri mi? */
static int is_ident_char(char c) {
    return gcl_lsp_is_ident_char(c);
}

/* p'den başlayan ident'i kopyala (uzunluk döner). İdent yoksa 0. */
static size_t copy_ident(const char *p, char *out, size_t outcap) {
    if (!is_ident_start(*p)) return 0;
    size_t n = 0;
    while (is_ident_char(p[n]) && n + 1 < outcap) { out[n] = p[n]; n++; }
    out[n] = '\0';
    return n;
}

/* Aynı isimli sembol zaten var mı? (duplicate önle) */
static int sym_exists(LspSymbol *syms, int count, const char *name, const char *file) {
    for (int i = 0; i < count; i++) {
        if (syms[i].name && strcmp(syms[i].name, name) == 0) {
            if (!file || !syms[i].file || strcmp(syms[i].file, file) == 0) return 1;
        }
    }
    return 0;
}

/* struct/enum/typedef member listesine bir üye ekle */
static void add_member(LspSymbol *parent, const char *name, LspKind kind,
                       const char *detail, const char *file) {
    if (!parent || !name || !name[0]) return;
    if (parent->member_count >= parent->member_cap) {
        int nc = parent->member_cap == 0 ? 8 : parent->member_cap * 2;
        LspSymbol *n = (LspSymbol *)realloc(parent->members, (size_t)nc * sizeof(LspSymbol));
        if (!n) return;
        parent->members = n;
        parent->member_cap = nc;
    }
    LspSymbol *m = &parent->members[parent->member_count++];
    memset(m, 0, sizeof(*m));
    m->name = gcl_lsp_strdup(name);
    m->kind = kind;
    m->vis = LSP_VIS_PUBLIC;
    m->detail = detail ? gcl_lsp_strdup(detail) : NULL;
    m->params = NULL;
    m->file = file ? gcl_lsp_strdup(file) : NULL;
    m->members = NULL;
    m->member_count = 0;
    m->member_cap = 0;
}

/* Struct tanımını parse et: struct Name { ... }; — Name verilmişse soyut tip olarak ekle.
   typedef struct { ... } Name; — typedef bloğunda ayrı ele alınır. */
static int scan_struct(LspSymbol **out, int *count, int *cap,
                       const char *body, size_t body_len,
                       const char *struct_name, const char *file) {
    if (!body) return 0;
    LspSymbol *parent = NULL;
    /* struct Name → tip olarak ekle (üye listesi aşağıda doldurulacak) */
    if (struct_name && struct_name[0] && !sym_exists(*out, *count, struct_name, file)) {
        parent = lsp_list_add(out, count, cap, struct_name, LSP_KIND_TYPE,
                              LSP_VIS_PUBLIC, "struct", NULL, file);
    }
    /* Body içindeki üyeleri tara: her satır "type name[...];" */
    const char *p = body;
    const char *end = body + body_len;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *line_end = nl ? nl : end;
        char line[256];
        copy_line(p, (size_t)(line_end - p), line, sizeof(line));
        const char *q = skip_ws(line);
        char type_name[128] = {0};
        size_t tn = copy_ident(q, type_name, sizeof(type_name));
        if (tn > 0) {
            q += tn;
            while (*q == ' ' || *q == '\t') q++;
            char member_name[128] = {0};
            size_t mn = copy_ident(q, member_name, sizeof(member_name));
            if (mn > 0) {
                if (parent) add_member(parent, member_name, LSP_KIND_VAR, type_name, file);
            }
        }
        p = line_end + (nl ? 1 : 0);
    }
    return parent != NULL;
}

/* Enum tanımını parse et: enum Name { A, B, C }; */
static void scan_enum(LspSymbol **out, int *count, int *cap,
                      const char *body, size_t body_len,
                      const char *enum_name, const char *file) {
    LspSymbol *parent = NULL;
    if (enum_name && enum_name[0] && !sym_exists(*out, *count, enum_name, file)) {
        parent = lsp_list_add(out, count, cap, enum_name, LSP_KIND_TYPE,
                              LSP_VIS_PUBLIC, "enum", NULL, file);
    }
    /* Body'de virgülle ayrılmış sabit adları topla */
    char tmp[512];
    size_t body_tmp = body_len < sizeof(tmp) - 1 ? body_len : sizeof(tmp) - 1;
    memcpy(tmp, body, body_tmp);
    tmp[body_tmp] = '\0';
    const char *q = tmp;
    while (*q) {
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == ',') q++;
        char name[128] = {0};
        size_t n = copy_ident(q, name, sizeof(name));
        if (n > 0) {
            if (parent) add_member(parent, name, LSP_KIND_MACRO, "enum value", file);
            q += n;
            /* '= değer' kısmını atla */
            while (*q && *q != ',' && *q != '}' && *q != '\n') q++;
        } else {
            q++;
        }
    }
}

/* Bir metni (tam dosya) tara. 'workspace' #include çözümlemesi için kullanılır. */
static void scan_text(const char *text, size_t len, const char *file,
                      const char *workspace, int depth,
                      LspSymbol **out, int *count, int *cap);

/* #include <x.gcsf> satırını bul ve o dosyayı recursive tara */
static void scan_include(const char *line, size_t line_len, const char *workspace,
                         const char *cur_file, int depth,
                         LspSymbol **out, int *count, int *cap) {
    (void)cur_file;
    if (!workspace || depth >= GCL_LSP_MAX_DEPTH) return;
    /* #include <...> veya #include "..." */
    const char *lt = memchr(line, '<', line_len);
    const char *gt = lt ? memchr(lt + 1, '>', line_len - (size_t)(lt + 1 - line)) : NULL;
    const char *q1 = memchr(line, '"', line_len);
    const char *q2 = q1 ? memchr(q1 + 1, '"', line_len - (size_t)(q1 + 1 - line)) : NULL;
    const char *start = NULL, *end = NULL;
    if (lt && gt) { start = lt + 1; end = gt; }
    else if (q1 && q2) { start = q1 + 1; end = q2; }
    if (!start || !end || end <= start) return;

    char fname[512];
    size_t fn = (size_t)(end - start);
    if (fn >= sizeof(fname)) fn = sizeof(fname) - 1;
    memcpy(fname, start, fn);
    fname[fn] = '\0';

    char path[2048];
    snprintf(path, sizeof(path), "%s/include/%s", workspace, fname);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "%s/%s", workspace, fname);
        f = fopen(path, "rb");
    }
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    scan_text(buf, rd, path, workspace, depth + 1, out, count, cap);
    free(buf);
}

static void scan_text(const char *text, size_t len, const char *file,
                      const char *workspace, int depth,
                      LspSymbol **out, int *count, int *cap) {
    if (!text || len == 0 || *count >= GCL_LSP_MAX_SYMS) return;
    const char *p = text;
    const char *end = text + len;

    while (p < end && *count < GCL_LSP_MAX_SYMS) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *line_end = nl ? nl : end;
        size_t line_len = (size_t)(line_end - p);
        char line[256];
        copy_line(p, line_len, line, sizeof(line));
        const char *q = skip_ws(line);

        /* #define NAME → macro */
        if (q[0] == '#' && strncmp(q, "#define", 7) == 0) {
            const char *d = q + 7;
            while (*d == ' ' || *d == '\t') d++;
            char mname[128] = {0};
            size_t mn = copy_ident(d, mname, sizeof(mname));
            if (mn > 0 && !sym_exists(*out, *count, mname, file)) {
                lsp_list_add(out, count, cap, mname, LSP_KIND_MACRO,
                             LSP_VIS_PUBLIC, "macro", NULL, file);
            }
            p = line_end + (nl ? 1 : 0);
            continue;
        }

        /* #include <x.gcsf> → workspace recursive tarama */
        if (q[0] == '#' && strncmp(q, "#include", 8) == 0) {
            scan_include(q, strlen(q), workspace, file, depth, out, count, cap);
            p = line_end + (nl ? 1 : 0);
            continue;
        }

        /* typedef → type sembolü */
        if (strncmp(q, "typedef", 7) == 0) {
            const char *t = q + 7;
            while (*t == ' ' || *t == '\t') t++;
            char alias[128] = {0};
            size_t an = 0;
            /* typedef struct { ... } Name; veya typedef enum { ... } Name; */
            if (strncmp(t, "struct", 6) == 0 || strncmp(t, "enum", 4) == 0) {
                const char *body_open = strchr(t, '{');
                const char *body_close = body_open ? strchr(body_open, '}') : NULL;
                if (body_open && body_close) {
                    /* } sonrasını tara → Name */
                    const char *after = body_close + 1;
                    while (*after == ' ' || *after == '\t') after++;
                    an = copy_ident(after, alias, sizeof(alias));
                    if (an > 0 && !sym_exists(*out, *count, alias, file)) {
                        int c1 = *count, cp1 = *cap;
                        LspSymbol *added = lsp_list_add(out, count, cap, alias,
                                                        LSP_KIND_TYPE, LSP_VIS_PUBLIC,
                                                        "typedef", NULL, file);
                        if (added) {
                            if (strncmp(t, "struct", 6) == 0) {
                                scan_struct(out, count, cap, body_open + 1,
                                            (size_t)(body_close - (body_open + 1)), NULL, file);
                            } else {
                                scan_enum(out, count, cap, body_open + 1,
                                          (size_t)(body_close - (body_open + 1)), NULL, file);
                            }
                        }
                        (void)c1; (void)cp1;
                    }
                }
            } else {
                /* typedef int Number; → "Number" son ident */
                copy_ident(t, alias, sizeof(alias));
                const char *r = t + strlen(alias);
                const char *semi = strchr(r, ';');
                if (semi) {
                    const char *rn = semi;
                    while (rn > r && is_ident_char(rn[-1])) rn--;
                    an = copy_ident(rn, alias, sizeof(alias));
                    if (an > 0 && !sym_exists(*out, *count, alias, file)) {
                        lsp_list_add(out, count, cap, alias, LSP_KIND_TYPE,
                                     LSP_VIS_PUBLIC, "typedef", NULL, file);
                    }
                }
            }
            p = line_end + (nl ? 1 : 0);
            continue;
        }

        /* struct Name { ... }; → type + members. Sonraki satırlara da bakmalı (multi-line struct). */
        if (strncmp(q, "struct", 6) == 0 && (q[6] == ' ' || q[6] == '\t')) {
            const char *n = q + 6;
            while (*n == ' ' || *n == '\t') n++;
            if (!strchr(n, '{')) {
                char sname[128] = {0};
                copy_ident(n, sname, sizeof(sname));
                if (sname[0] && !sym_exists(*out, *count, sname, file)) {
                    /* multi-line: birleşik satırlarda body'yi bul */
                    const char *scan = p;
                    const char *body_open = NULL, *body_close = NULL;
                    int guard = 0;
                    while (scan < end && guard++ < 1000) {
                        const char *s2 = memchr(scan, '{', (size_t)(end - scan));
                        if (s2) { body_open = s2; break; }
                        const char *s3 = memchr(scan, '\n', (size_t)(end - scan));
                        if (!s3) break;
                        scan = s3 + 1;
                    }
                    if (body_open) {
                        const char *scan2 = body_open;
                        guard = 0;
                        while (scan2 < end && guard++ < 5000) {
                            const char *s4 = memchr(scan2, '}', (size_t)(end - scan2));
                            if (s4) { body_close = s4; break; }
                            scan2++;
                        }
                    }
                    if (body_open && body_close) {
                        scan_struct(out, count, cap, body_open + 1,
                                    (size_t)(body_close - (body_open + 1)), sname, file);
                        p = body_close + 1;
                        continue;
                    } else {
                        lsp_list_add(out, count, cap, sname, LSP_KIND_TYPE,
                                     LSP_VIS_PUBLIC, "struct", NULL, file);
                    }
                }
            }
        }

        /* enum Name { A, B }; */
        if (strncmp(q, "enum", 4) == 0 && (q[4] == ' ' || q[4] == '\t')) {
            const char *n = q + 4;
            while (*n == ' ' || *n == '\t') n++;
            char ename[128] = {0};
            copy_ident(n, ename, sizeof(ename));
            const char *body_open = strchr(q, '{');
            const char *body_close = body_open ? strchr(body_open, '}') : NULL;
            if (body_open && body_close) {
                scan_enum(out, count, cap, body_open + 1,
                          (size_t)(body_close - (body_open + 1)), ename, file);
                p = body_close + 1;
                continue;
            }
        }

        /* Fonksiyon: "type name(params...) {" — satır başında tip + ident + '(' */
        /* Değişken: "type name = ...;" veya "type name;" */
        {
            char type1[128] = {0};
            size_t t1 = copy_ident(q, type1, sizeof(type1));
            if (t1 > 0) {
                const char *after_t = q + t1;
                while (*after_t == ' ' || *after_t == '\t') after_t++;
                char name1[128] = {0};
                size_t n1 = copy_ident(after_t, name1, sizeof(name1));
                if (n1 > 0) {
                    const char *rest = after_t + n1;
                    while (*rest == ' ' || *rest == '\t') rest++;
                    if (*rest == '(') {
                        /* fonksiyon — parametre dizesini çıkar */
                        const char *po = rest + 1;
                        int depth_p = 1;
                        const char *pc = po;
                        while (*pc && depth_p > 0) {
                            if (*pc == '(') depth_p++;
                            else if (*pc == ')') depth_p--;
                            pc++;
                        }
                        char params[256] = {0};
                        size_t pn = (size_t)(pc - po - 1);
                        if (pn >= sizeof(params)) pn = sizeof(params) - 1;
                        memcpy(params, po, pn);
                        params[pn] = '\0';
                        if (!sym_exists(*out, *count, name1, file)) {
                            lsp_list_add(out, count, cap, name1, LSP_KIND_FUNC,
                                         LSP_VIS_PUBLIC, type1, params, file);
                        }
                    } else if (*rest == '=' || *rest == ';' || *rest == '[') {
                        /* değişken (dizi dahil) */
                        if (!sym_exists(*out, *count, name1, file)) {
                            lsp_list_add(out, count, cap, name1, LSP_KIND_VAR,
                                         LSP_VIS_PUBLIC, type1, NULL, file);
                        }
                    }
                }
            }
        }

        p = line_end + (nl ? 1 : 0);
    }
}

/* Public API: aktif dosyayı + #include'ları tara.
   'text' aktif dosya içeriği (null-terminated, len = strlen), 'path' dosya yolu,
   'workspace' proje kökü (include/ için). */
int gcl_lsp_scan(const char *path, const char *text, size_t len,
                 const char *workspace, LspSymbol **out, int *out_count) {
    if (!out || !out_count) return -1;
    *out = NULL;
    *out_count = 0;
    if (!text || len == 0) return 0;
    int cap = 0;
    scan_text(text, len, path ? path : "", workspace, 0, out, out_count, &cap);
    return *out_count;
}
