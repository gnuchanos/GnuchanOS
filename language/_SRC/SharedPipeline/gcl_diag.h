/*
 * gcl_diag.h — GCL diagnostics: the shared error-reporting vocabulary.
 *
 * WHY THIS EXISTS
 * ---------------
 * Before this module a failure was one pre-formatted string: "expected
 * expression at 9:53". That shape is useless to a human and to a tool:
 *
 *   1. No span, only a point. The lexer/parser already know the token they
 *      stopped on, but the token's LENGTH was thrown away, so nothing could
 *      underline `Raylib.Rectangle` and nothing could tell a 1-char token from
 *      a 17-char one.
 *   2. No code. Two failures that happen to share wording are
 *      indistinguishable — to a test, to the IDE, to someone searching docs.
 *   3. No context. A message could not carry a note, a fix, or a SECOND
 *      location, so the parser's cascading guess ("the `,` inside what I think
 *      is a block is not an expression") was presented as if it were the
 *      actual mistake, with no pointer back to the `(` that was never closed.
 *
 * This module is the shared vocabulary for ALL GCL diagnostics: lexer, parser,
 * preprocessor and runtime. It is deliberately dependency-free apart from
 * gcl_lexer.h (token naming) so it links into gcl, the headless tests, and any
 * future front end without dragging in raylib.
 *
 * WHAT A DIAGNOSTIC IS
 * --------------------
 *   - a CODE          (stable identifier, assertable, greppable)
 *   - a SEVERITY      (note / warning / error / fatal)
 *   - a SPAN          (line, column, LENGTH — the underlined run)
 *   - a MESSAGE       (what is wrong, in one sentence, naming the tokens)
 *   - optional NOTES  (extra facts the reader cannot infer)
 *   - optional HINTS  ("did you mean…", "add ')' here")
 *   - optional RELATED SPANS (a second location with its own label, e.g. the
 *     `(` that the missing `)` was supposed to close)
 *
 * ERROR CODES
 * -----------
 * The number IS the identifier: GCL1001 is printed from the value 1001.
 * Bands are reserved so a code never changes meaning:
 *
 *   GCL1000-1099  lexer
 *   GCL2000-2099  parser (syntax)
 *   GCL3000-3099  semantic / resolution / runtime
 *   GCL4000-4099  preprocessor / module loading
 *
 * Tests assert on the CODE, never on the wording of a message.
 *
 * RENDERING
 * ---------
 * The de-facto standard every C-family tool uses, so editors and terminals
 * already parse it:
 *
 *   game.gcsf:10:12: error: expected ')' to close the call to 'printf', found ';' [GCL2002]
 *      10 |     Stdio.printf("score: {}\n", scor;
 *         |                                   ^
 *         = note: the call to 'printf' starts here
 *            8 | Stdio.printf(
 *              |         ^
 *         = help: add ')' before the ';'
 *
 * Line 1 is machine-parsable: file:line:col: severity: message [CODE].
 */

#ifndef GCL_DIAG_H
#define GCL_DIAG_H

#include <stddef.h>
#include <stdio.h>

#include "gcl_lexer.h"
#include "gcl_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- severity ---------- */

typedef enum {
    GCL_SEV_NOTE = 0,   /* informational; never fails a build */
    GCL_SEV_WARNING,    /* suspicious but compilable */
    GCL_SEV_ERROR,      /* the unit cannot run */
    GCL_SEV_FATAL       /* the unit cannot be parsed at all (stop early) */
} GclSeverity;

/* ---------- codes ---------- */

typedef enum {
    GCL_E_NONE = 0,

    /* --- lexer (GCL1xxx) --- */
    GCL_E_LEX_UNEXPECTED_CHAR      = 1001,  /* character is not part of GCL */
    GCL_E_LEX_UNTERMINATED_STRING  = 1002,  /* "… is never closed */
    GCL_E_LEX_UNTERMINATED_COMMENT = 1003,  /* slash-star block comment */
    GCL_E_LEX_BAD_NUMBER           = 1004,  /* 1.2.3 / 0x / 12abc */
    GCL_E_LEX_UNTERMINATED_HASHCOM = 1005,  /* #| … never closed with |# */
    GCL_E_LEX_BAD_ESCAPE           = 1006,  /* unknown escape in a literal */
    GCL_E_LEX_EMPTY_CHAR           = 1007,  /* '' is not a character */
    GCL_E_LEX_MULTI_CHAR           = 1008,  /* 'ab' is more than one character */
    GCL_E_LEX_UNTERMINATED_CHAR    = 1009,  /* 'a is never closed */

    /* --- parser (GCL2xxx) --- */
    GCL_E_PARSE_EXPECTED_EXPRESSION = 2001, /* an expression was expected */
    GCL_E_PARSE_EXPECTED_TOKEN      = 2002, /* a specific token is missing */
    GCL_E_PARSE_EXPECTED_MEMBER     = 2003, /* a member name must follow '.' */
    GCL_E_PARSE_EXPECTED_ARRAY_SIZE = 2004, /* an array dimension must be a number */
    GCL_E_PARSE_EXPECTED_VAR_NAME   = 2005, /* a variable name was expected */
    GCL_E_PARSE_UNCLOSED_BLOCK      = 2006, /* a block is never closed */
    GCL_E_PARSE_QUALIFIED_TYPE      = 2007, /* module-qualified names are not types */
    GCL_E_PARSE_STRAY_INIT_LIST     = 2008, /* initializer is not attached to a declaration */
    GCL_E_PARSE_EXPECTED_COND_CLAUSE = 2009,/* expected '(' after if/while */
    GCL_E_PARSE_EXPECTED_STATEMENT  = 2010, /* declaration or statement expected */
    GCL_E_PARSE_EXPECTED_SEMI       = 2011, /* expected ';' after a statement */
    GCL_E_PARSE_EXPECTED_TYPE       = 2012, /* a type name was expected */
    GCL_E_PARSE_EXPECTED_CASE_COLON = 2013, /* expected ':' after a case label */
    GCL_E_PARSE_EXPECTED_PARAM      = 2014, /* expected a parameter name */
    GCL_E_PARSE_EXPECTED_MEMBER_DECL = 2015,/* expected a struct member declaration */
    GCL_E_PARSE_EXPECTED_ENUM_CONST = 2016, /* expected an enum constant name */
    GCL_E_PARSE_NOT_ASSIGNABLE      = 2017, /* assignment target is not assignable */
    GCL_E_PARSE_EXPECTED_CLOSE      = 2018, /* a bracket is never closed */
    GCL_E_PARSE_UNEXPECTED_TOKEN    = 2019, /* token cannot start anything here */

    /* --- semantic / resolution / runtime (GCL3xxx) --- */
    GCL_E_SEM_UNKNOWN_TYPE   = 3001, /* unknown type name */
    GCL_E_SEM_UNKNOWN_MEMBER = 3002, /* unknown member */
    GCL_E_SEM_UNKNOWN_VAR    = 3003, /* name is not defined */
    GCL_E_SEM_ASSIGN_CONST   = 3004, /* cannot assign to a const */
    GCL_E_SEM_UNKNOWN_FUNC   = 3005, /* unknown function */
    GCL_E_SEM_UNKNOWN_MODULE = 3006, /* native module is not loaded */
    GCL_E_SEM_BAD_ARGC       = 3007, /* wrong number of arguments */
    GCL_E_SEM_DIV_ZERO       = 3008, /* division by zero */
    GCL_E_SEM_INFINITE_LOOP  = 3009, /* runaway loop guard tripped */
    GCL_E_SEM_OUT_OF_BOUNDS  = 3010, /* array index out of range */
    GCL_E_SEM_RUNTIME        = 3011, /* generic runtime failure */
    GCL_E_SEM_BAD_ARRAY_SIZE = 3012, /* char a[] with no size */
    GCL_E_SEM_BAD_CALL       = 3013, /* call target is not callable */
    /* TURN 50 - three codes the SimpleRunner had INVENTED but this vocabulary
       never declared. gcl_runner.c kept its own `GCL_RT_CODE_*` block whose
       comment said the values were "the same as gcl_diag.h's GCL3xxx" - and
       one of them (THROW = 3014) had no entry HERE at all, so the runtime was
       emitting a code that `gcl_diag_code_title()` answered with the generic
       "diagnostic" fallback. A code only half the pipeline knows is not a
       shared vocabulary, so the three real runtime failures are declared here,
       where the bands and the catalogue are. */
    GCL_E_SEM_THROW          = 3014, /* an uncaught `throw` stopped the run */
    GCL_E_SEM_LOOP_CONTROL   = 3015, /* break/continue outside its loop/switch */
    GCL_E_SEM_CALL_DEPTH     = 3016, /* recursion passed the call-depth limit */

    /* --- preprocessor / project (GCL4xxx) --- */
    GCL_E_PP_INCLUDE_NOT_FOUND = 4001, /* #include target was not found */
    GCL_E_PP_UNTERMINATED_IF   = 4002, /* #if without #endif */
    GCL_E_PP_STRAY_ELSE        = 4003, /* #else/#elif/#endif without #if */
    GCL_E_PP_BAD_DEFINE        = 4004, /* malformed #define */
    GCL_E_PP_ERROR_DIRECTIVE   = 4005, /* #error was hit */
    GCL_E_PP_BAD_DIRECTIVE     = 4006, /* unknown #directive */
    GCL_E_PP_BAD_EXPRESSION    = 4007  /* #if expression is malformed */
} GclDiagCode;

/* ---------- spans ---------- */

/* 1-based line/col, matching GclToken. `len` is the number of source
   characters the diagnostic covers (0 = unknown -> renderer underlines 1). */
typedef struct {
    int line;
    int col;
    int len;
} GclSpan;

/* How many secondary locations one diagnostic may point at. */
#define GCL_DIAG_MAX_RELATED 4

/* A second location that belongs to the story: "the '(' opened here". */
typedef struct {
    GclSpan span;    /* same coordinate space as the primary span */
    char   *label;   /* owned; shown as "= note: <label>" above its own frame */
} GclDiagRelated;

/* ---------- one diagnostic ---------- */

typedef struct {
    GclDiagCode code;
    GclSeverity severity;
    GclSpan span;
    char *file;      /* owned; overrides the map; may be NULL */
    char *message;   /* owned; never NULL once added */
    char *note;      /* owned; may be NULL */
    char *hint;      /* owned; may be NULL */
    int related_count;
    GclDiagRelated related[GCL_DIAG_MAX_RELATED];
} GclDiag;

/* ---------- sink ---------- */

/* The tag is named so gcl_lexer.h / gcl_parser.h can forward-declare the sink
   without pulling in this header (this header already includes gcl_lexer.h). */
typedef struct GclDiagList GclDiagList;
struct GclDiagList {
    GclDiag *items;
    int count;
    int cap;
    int error_count;     /* severity >= GCL_SEV_ERROR */
    const char *file;    /* borrowed default for diagnostics added without one */
};

/* Lifecycle. `file` is borrowed for the lifetime of the list. */
void gcl_diag_list_init(GclDiagList *dl, const char *file);
void gcl_diag_list_free(GclDiagList *dl);
void gcl_diag_list_clear(GclDiagList *dl);

/* Add a diagnostic. Returns the new entry (owned by the list) or NULL when
   the list is full/failed; callers may pass NULL to ignore. */
GclDiag *gcl_diag_add(GclDiagList *dl, GclDiagCode code, GclSeverity sev,
                      GclSpan span, const char *fmt, ...);

/* Attach/replace the note and the fix hint (printf-formatted, both optional). */
void gcl_diag_note(GclDiag *d, const char *fmt, ...);
void gcl_diag_hint(GclDiag *d, const char *fmt, ...);

/* Add a secondary location with its own label. Ignored once the cap is hit. */
void gcl_diag_relate(GclDiag *d, GclSpan span, const char *fmt, ...);

int gcl_diag_count(const GclDiagList *dl);
int gcl_diag_error_count(const GclDiagList *dl);
int gcl_diag_has_errors(const GclDiagList *dl);
const GclDiag *gcl_diag_first_error(const GclDiagList *dl);

/* ---------- naming ---------- */

const char *gcl_diag_severity_name(GclSeverity s);
/* "GCL2001" into buf; returns buf. */
const char *gcl_diag_code_str(GclDiagCode c, char *buf, size_t cap);
/* One-line canonical description of a code (for `gcl --explain GCL2001`). */
const char *gcl_diag_code_title(GclDiagCode c);
/* Longer explanation + a small example, for `--explain`. */
const char *gcl_diag_code_help(GclDiagCode c);
/* Convert "GCL2001" / "2001" back to a code; GCL_E_NONE when unknown. */
GclDiagCode gcl_diag_code_from_string(const char *s);

/* ---------- token helpers ---------- */

/* Human name of a token as it should appear inside a message:
   "'{'"  |  "identifier 'Player'"  |  "number '400'"  |  "end of file". */
const char *gcl_diag_token_desc(const GclToken *t, char *buf, size_t cap);

/* What a token looks like WITHOUT its kind: "'{'" / "'Player'" / "end of file". */
const char *gcl_diag_token_text(const GclToken *t, char *buf, size_t cap);

/* Build a span covering a token (line/col/len from the token itself). */
GclSpan gcl_diag_span_of_token(const GclToken *t);

/* ---------- colour ---------- */

typedef enum {
    GCL_COLOR_AUTO = 0,   /* colour when stderr is a terminal */
    GCL_COLOR_ALWAYS,
    GCL_COLOR_NEVER
} GclColorMode;

/* Process-wide switch. `AUTO` is the default and honours NO_COLOR. */
void gcl_diag_set_color(GclColorMode mode);
int  gcl_diag_color_enabled(void);

/* ---------- rendering ---------- */

/* Render ONE diagnostic into buf (NUL-terminated, always on a line boundary).
   `source`/`source_len` are the text the span refers to; when NULL the caret
   line is skipped and only the header is produced.
   Returns the number of bytes written (excluding the NUL), like snprintf. */
size_t gcl_diag_render(const GclDiag *d, const char *source, size_t source_len,
                       char *buf, size_t cap);

/* Render ONE diagnostic, translating its span through the source map:
   `d->span.line` is a line in the PREPROCESSED buffer, while the header and the
   code frame are produced from the ORIGINAL file and line. `fallback_source`
   is used when the map cannot resolve the line. */
size_t gcl_diag_render_mapped(const GclDiag *d, const GclSourceMap *m,
                              const char *fallback_source, size_t fallback_len,
                              char *buf, size_t cap);

/* Render every diagnostic in the list, separated by a blank line.
   Returns the number of bytes that WOULD have been written. */
size_t gcl_diag_list_render(const GclDiagList *dl, const char *source,
                            size_t source_len, char *buf, size_t cap);

size_t gcl_diag_list_render_mapped(const GclDiagList *dl, const GclSourceMap *m,
                                   const char *fallback_source, size_t fallback_len,
                                   char *buf, size_t cap);

/* Convenience: render directly to a stream (no fixed buffer). */
void gcl_diag_print(FILE *out, const GclDiag *d, const char *source, size_t source_len);
void gcl_diag_print_mapped(FILE *out, const GclDiag *d, const GclSourceMap *m,
                           const char *fallback_source, size_t fallback_len);

void gcl_diag_list_print(FILE *out, const GclDiagList *dl, const char *source, size_t source_len);
void gcl_diag_list_print_mapped(FILE *out, const GclDiagList *dl, const GclSourceMap *m,
                                const char *fallback_source, size_t fallback_len);

/* Summary tail printed after a list of errors ("3 errors generated."). */
void gcl_diag_print_summary(FILE *out, const GclDiagList *dl);

/* Legacy bridge: format the FIRST error of the list as the old single-line
   string ("expected ')' at 3:7"). Heap-allocated; caller frees. Used by the
   deprecated gcl_lex()/gcl_parse() wrappers so old callers keep working. */
char *gcl_diag_first_to_legacy_string(const GclDiagList *dl);

#ifdef __cplusplus
}
#endif

#endif /* GCL_DIAG_H */
