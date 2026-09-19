/*
 * complete_context.h — Cursor context analysis.
 *
 * Extracts the syntactic state of the cursor: whether it is inside a string/comment,
 * a member access (X.), a chain (a.b.c.), a function argument list, a preprocessor line, etc.
 * The old engine's "double-dot protection" (strchr(prefix,'.') != last_dot) has been removed here;
 * chained completion is supported (bug fix, §1.2).
 */
#ifndef GCL_COMPLETE_CONTEXT_H
#define GCL_COMPLETE_CONTEXT_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCLC_MAX_CHAIN 16
#define GCLC_CHAIN_NAME 128

typedef struct {
    /* Anchor prefix: the [A-Za-z0-9_] sequence immediately to the left of the cursor. */
    size_t         prefix_begin;   /* byte offset */
    size_t         prefix_end;     /* == cursor */
    char           prefix[256];    /* active word */

    /* Context */
    GclContextKind kind;
    int            in_string;
    int            in_comment;
    int            line_is_preproc;
    int            call_depth;      /* > 0 when inside f( */

    /* Member chain (left to right). chain[chain_len-1] is the closest base. */
    int            chain_len;
    char           chain[GCLC_MAX_CHAIN][GCLC_CHAIN_NAME];
    int            chain_is_call[GCLC_MAX_CHAIN]; /* 1 if "GetMousePosition()" */

    /* Preprocessor info */
    char           directive[32];   /* "#native" -> "native" */
    char           pp_arg[256];     /* text after '<' or after the directive */
    int            in_angle;        /* inside '<' ... '>' */

    /* "Type var = " context */
    int            has_decl_type;
    char           decl_type[128];  /* "Type" */

    /* Is this the body of a struct/enum definition? */
    int            in_type_body;

    /* String-path context (§Phase 6): the cursor is inside a string argument for a path-taking function
       (LoadTexture, readFile, …). */
    int            is_path_string;
    char           str_prefix[512];  /* full path typed inside the quote */
} GclContext;

/* Analyze the context at the cursor position. text/text_len: the entire buffer. */
void gcl_context_analyze(const char *text, size_t text_len, size_t cursor,
                         GclContext *ctx);

/* Returns the base of a member access: gives chain; if empty, it is a plain prefix and chain_len==0. */
static inline int gcl_context_is_member(const GclContext *c) {
    return c->kind == CTX_MEMBER && c->chain_len > 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_CONTEXT_H */
