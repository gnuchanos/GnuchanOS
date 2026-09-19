/*
 * gcl_source.c — source registry + preprocessed-line → original-line map.
 *
 * The whole module is deliberately small and allocation-only: it runs on the
 * error path and inside the preprocessor, so it must not pull in the lexer,
 * the runner, or any platform header.
 */

#include "gcl_source.h"

#include <stdlib.h>
#include <string.h>

/* ---------- small helpers ---------- */

static char *dup_n(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    if (n) memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static char *dup_str(const char *s) {
    if (!s) return NULL;
    return dup_n(s, strlen(s));
}

/* Grow an array to at least `need` elements. Returns 0 on success. */
static int grow(void **arr, int *cap, size_t elem, int need) {
    if (*cap >= need) return 0;
    int nc = *cap ? *cap * 2 : 8;
    while (nc < need) nc *= 2;
    void *na = realloc(*arr, (size_t)nc * elem);
    if (!na) return -1;
    *arr = na;
    *cap = nc;
    return 0;
}

/* ---------- lifecycle ---------- */

void gcl_smap_init(GclSourceMap *m) {
    if (m) memset(m, 0, sizeof(*m));
}

static void free_files(GclSourceMap *m) {
    for (int i = 0; i < m->file_count; i++) {
        free(m->files[i].name);
        free(m->files[i].text);
    }
    free(m->files);
    m->files = NULL;
    m->file_count = 0;
    m->file_cap = 0;
}

void gcl_smap_reset(GclSourceMap *m) {
    if (!m) return;
    free_files(m);
    free(m->origins);
    m->origins = NULL;
    m->origin_count = 0;
    m->origin_cap = 0;
}

void gcl_smap_free(GclSourceMap *m) {
    if (!m) return;
    gcl_smap_reset(m);
}

/* ---------- files ---------- */

int gcl_smap_find_file(const GclSourceMap *m, const char *name) {
    if (!m || !name) return -1;
    for (int i = 0; i < m->file_count; i++) {
        if (m->files[i].name && strcmp(m->files[i].name, name) == 0) return i;
    }
    return -1;
}

int gcl_smap_add_file(GclSourceMap *m, const char *name, const char *text, size_t len) {
    if (!m || !text) return GCL_SMAP_NO_FILE;
    if (grow((void **)&m->files, &m->file_cap, sizeof(GclSourceFile), m->file_count + 1) != 0)
        return GCL_SMAP_NO_FILE;

    GclSourceFile *f = &m->files[m->file_count];
    f->name = dup_str(name && name[0] ? name : "<input>");
    f->text = dup_n(text, len);
    f->len = len;
    if (!f->name || !f->text) {
        free(f->name);
        free(f->text);
        f->name = f->text = NULL;
        f->len = 0;
        return GCL_SMAP_NO_FILE;
    }
    return m->file_count++;
}

const char *gcl_smap_file_text(const GclSourceMap *m, int file_index, size_t *len_out) {
    if (len_out) *len_out = 0;
    if (!m || file_index < 0 || file_index >= m->file_count) return NULL;
    if (len_out) *len_out = m->files[file_index].len;
    return m->files[file_index].text;
}

const char *gcl_smap_file_name(const GclSourceMap *m, int file_index) {
    if (!m || file_index < 0 || file_index >= m->file_count) return NULL;
    return m->files[file_index].name;
}

/* ---------- origins ---------- */

int gcl_smap_add_line(GclSourceMap *m, int file, int line, int expanded) {
    if (!m) return -1;
    if (grow((void **)&m->origins, &m->origin_cap, sizeof(GclLineOrigin),
             m->origin_count + 1) != 0)
        return -1;
    GclLineOrigin *o = &m->origins[m->origin_count];
    o->file = file;
    o->line = line;
    o->expanded = expanded ? 1 : 0;
    m->origin_count++;
    return m->origin_count;   /* 1-based line number */
}

void gcl_smap_truncate(GclSourceMap *m, int n) {
    if (!m) return;
    if (n < 0) n = 0;
    if (n < m->origin_count) m->origin_count = n;
}

int gcl_smap_line_count(const GclSourceMap *m) { return m ? m->origin_count : 0; }

int gcl_smap_line_expanded(const GclSourceMap *m, int line) {
    if (!m || line < 1 || line > m->origin_count) return 0;
    return m->origins[line - 1].expanded;
}

int gcl_smap_lookup(const GclSourceMap *m, int line,
                    const char **file_name, const char **text, size_t *text_len,
                    int *orig_line) {
    if (!m || line < 1 || line > m->origin_count) return 0;
    const GclLineOrigin *o = &m->origins[line - 1];
    if (o->file == GCL_SMAP_NO_FILE || o->file >= m->file_count) return 0;

    if (file_name) *file_name = m->files[o->file].name;
    if (text)      *text      = m->files[o->file].text;
    if (text_len)  *text_len  = m->files[o->file].len;
    if (orig_line) *orig_line = o->line;
    return 1;
}
