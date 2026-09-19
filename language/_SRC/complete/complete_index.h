/*
 * complete_index.h — Incremental file index / cache (§12).
 *
 * Project files are not re-read on every keypress: each file is stored along with its mtime.
 * If the file has not changed, it returns from the cache instead of reading from disk.
 */
#ifndef GCL_COMPLETE_INDEX_H
#define GCL_COMPLETE_INDEX_H

#include "gcl_complete.h"
#include "complete_scope.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return the file contents from the cache (or from disk if needed).
 * On success returns 1; *out_text belongs to the engine (free it), and remains valid until the next
 * gcl_index_clear().
 */
int  gcl_index_read(const char *path, const char **out_text,
                    size_t *out_len, long *out_mtime);

/* Invalidate the cache if the file changed (using the mtime stamp). */
void gcl_index_invalidate(const char *path);

/*
 * Content generation of the file cache. It increases whenever a file is
 * RE-READ from disk because its (mtime, size) changed; it does NOT change on
 * cache hits. gcl_index_scope() folds it into its cache key, so when a project
 * file (include/, lib/, root *.gcsf) changes, the scan result is rebuild from
 * scratch and definitions that were DELETED from that file disappear instead
 * of lingering in the cached scope forever (project symbols are only ever
 * added to an existing scope, never removed).
 */
unsigned long long gcl_index_generation(void);

/* Clear the entire cache (called when opening a new project). */
void gcl_index_clear(void);

/*
 * Retrieve the established SCOPE for the active buffer from cache (§12, S7).
 *
 * The engine used to reparse the full buffer on every keypress (43 KB at ~2.5 ms).
 * This cache stores the PARSE RESULT keyed by the buffer content hash: if the text has not changed
 * (cursor movement, popup refresh, repeated query after debounce), the parse is not repeated.
 *
 * The returned scope belongs to the engine and is reused — it must not be freed.
 * Callers may use it read-only; additions such as project symbols are idempotent (updated if the same name exists).
 * Callers should not use the returned pointer after the next gcl_index_scope() call.
 * If the text hash or file/workspace changes, the scope is rebuilt.
 */
GclScope *gcl_index_scope(const char *file, const char *text, size_t len,
                          const char *workspace);

/* Clear the scope cache (also called by gcl_index_clear). */
void gcl_index_scope_clear(void);

/*
 * Force the next gcl_index_scope() call to rebuild the scope even if the
 * buffer text hash is unchanged. The generation may increase DURING the
 * project scan (i.e. after the scope was fetched), which would leave exactly
 * one query's worth of stale symbols; the caller notices the generation change
 * and invalidates so the rebuilt scope is fresh in the SAME query.
 */
void gcl_index_scope_invalidate(void);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_INDEX_H */
