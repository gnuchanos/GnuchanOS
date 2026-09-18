/*
 * gcl_source.h — source files + the origin map that keeps line numbers honest.
 *
 * WHY THIS EXISTS
 * ---------------
 * The pipeline is: read file → PREPROCESS (this rewrites the text) → lex →
 * parse. Preprocessing is what corrupted every reported position:
 *
 *   - a `#define` line is consumed and writes NOTHING into the output buffer,
 *     so every line AFTER it shifts up by one;
 *   - `#include <x.gcsf>` pastes an unknown number of lines in, shifting every
 *     line AFTER the include down;
 *   - `#if 0 ... #endif` drops whole regions.
 *
 * The lexer/parser only ever see the rewritten buffer, so `t->line` is a line
 * number in a document the user has never looked at. That is why the compiler
 * used to point at a line that had nothing to do with the mistake.
 *
 * The fix is not "emit blank lines instead of dropping them" — that cannot
 * survive `#include`, which by definition changes the line count. The fix is a
 * MAP: while preprocessing, record for every line written into the output
 * buffer which file it came from and which line of that file it was. The
 * lexer/parser keep working in output-buffer coordinates (they need nothing
 * else), and the RENDERER translates a position into the user's own file and
 * line before printing anything.
 *
 * The module also owns copies of the original file text, so the code frame
 * shown to the user is the line they actually wrote — not the macro-expanded
 * one.
 */

#ifndef GCL_SOURCE_H
#define GCL_SOURCE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The number used for "this line has no home" (see GCL_SMAP_NO_FILE). */
#define GCL_SMAP_NO_FILE (-1)

/* One physical file that contributed text to the pipeline. */
typedef struct {
    char  *name;   /* owned; the path as the user should see it */
    char  *text;   /* owned; the file's own bytes, NUL-terminated, never modified */
    size_t len;
} GclSourceFile;

/* Where one line of the PREPROCESSED buffer came from. */
typedef struct {
    int file;      /* index into `files`, or GCL_SMAP_NO_FILE */
    int line;      /* 1-based line inside that file */
    int expanded;  /* 1 when macro expansion rewrote this line's text */
} GclLineOrigin;

typedef struct {
    GclSourceFile *files;
    int            file_count;
    int            file_cap;

    GclLineOrigin *origins;   /* one entry per preprocessed line, in order */
    int            origin_count;
    int            origin_cap;
} GclSourceMap;

/* ---------- lifecycle ---------- */

void gcl_smap_init(GclSourceMap *m);
void gcl_smap_reset(GclSourceMap *m);   /* drop contents, keep the struct usable */
void gcl_smap_free(GclSourceMap *m);

/* ---------- building ---------- */

/* Register a copy of `text` under `name`. Returns the file index, or
   GCL_SMAP_NO_FILE when the map is full or allocation failed. */
int gcl_smap_add_file(GclSourceMap *m, const char *name, const char *text, size_t len);

/* Index of a file already registered under exactly this name, else -1. */
int gcl_smap_find_file(const GclSourceMap *m, const char *name);

/* Append the origin of the next preprocessed line. Returns its 1-based line
   number, or -1 on allocation failure. */
int gcl_smap_add_line(GclSourceMap *m, int file, int line, int expanded);

/* Keep only the first `n` origins (`n` is a 1-based line count, i.e. what
   gcl_smap_line_count() returns). A recursive `#include` records the origins of
   the lines it produced; when the PARENT cannot paste those bytes because its
   own output buffer is full, they never reach the buffer and must not stay in
   the map - otherwise every line after the include is off by exactly what was
   dropped. Nothing else needs this: add_line is the only thing that grows it. */
void gcl_smap_truncate(GclSourceMap *m, int n);

/* Number of preprocessed lines recorded so far. */
int gcl_smap_line_count(const GclSourceMap *m);

/* ---------- querying ---------- */

/* Resolve preprocessed line `line` (1-based). On success returns 1 and sets
   the out parameters; `*file_name` and `*text` are borrowed. When the line has
   no recorded origin the call returns 0 and leaves the out parameters alone. */
int gcl_smap_lookup(const GclSourceMap *m, int line,
                    const char **file_name, const char **text, size_t *text_len,
                    int *orig_line);

/* Borrowed text of a registered file; NULL when `file_index` is invalid. */
const char *gcl_smap_file_text(const GclSourceMap *m, int file_index, size_t *len_out);

/* Borrowed display name of a registered file; NULL when invalid. */
const char *gcl_smap_file_name(const GclSourceMap *m, int file_index);

/* `1` when the preprocessed line was rewritten by macro expansion. */
int gcl_smap_line_expanded(const GclSourceMap *m, int line);

#ifdef __cplusplus
}
#endif

#endif /* GCL_SOURCE_H */
