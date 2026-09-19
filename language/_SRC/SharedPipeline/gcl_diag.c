/*
 * gcl_diag.c — diagnostics: codes, spans, notes/hints, related locations,
 * source rendering, colour.
 *
 * Rendering shape (the de-facto standard every C-family tool uses, so editors
 * and terminals can already parse it):
 *
 *   game.gcsf:10:12: error: expected ')' to close the call to 'printf', found ';' [GCL2002]
 *      10 |     Stdio.printf("score: {}\n", scor;
 *         |                                   ^
 *         = note: the call to 'printf' opens here
 *            8 |     Stdio.printf(
 *              |                 ^
 *         = help: add ')' before the ';'
 *
 * The first line is machine-parsable. Everything after it is for the human:
 * the offending source line, a caret run whose length is the span length, then
 * the related frames, the note and the help. Nothing is emitted that a plain
 * ASCII terminal cannot show, and colour is opt-out.
 */

#include "gcl_diag.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ---------- colour ---------- */

static GclColorMode g_color_mode = GCL_COLOR_AUTO;
static int g_color_resolved = -1;   /* -1 = not decided yet */
static int g_color_on = 0;

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_RED     "\033[31m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_CYAN    "\033[36m"
#define C_GREEN   "\033[32m"
#define C_DIM     "\033[2m"

/* ENABLE_VIRTUAL_TERMINAL_PROCESSING: not exposed by every MinGW header. */
#ifndef GCL_ENABLE_VT
#define GCL_ENABLE_VT 0x0004
#endif

void gcl_diag_set_color(GclColorMode mode) {
    g_color_mode = mode;
    g_color_resolved = -1;
}

static int color_decide(void) {
    if (g_color_resolved >= 0) return g_color_on;
    g_color_resolved = 1;
    g_color_on = 0;

    if (g_color_mode == GCL_COLOR_NEVER) return g_color_on;
    /* NO_COLOR is the informal cross-tool standard: any non-empty value wins. */
    if (g_color_mode != GCL_COLOR_ALWAYS) {
        const char *nc = getenv("NO_COLOR");
        if (nc && nc[0]) return g_color_on;
    }
    if (g_color_mode == GCL_COLOR_ALWAYS) { g_color_on = 1; return g_color_on; }

#ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (h && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
            /* Windows 10+ consoles understand VT sequences once enabled. */
            SetConsoleMode(h, mode | GCL_ENABLE_VT);
            g_color_on = 1;
        }
    }
#else
    g_color_on = isatty(2) ? 1 : 0;
#endif
    return g_color_on;
}

int gcl_diag_color_enabled(void) { return color_decide(); }

static const char *c_for(GclSeverity s) {
    if (!color_decide()) return "";
    switch (s) {
        case GCL_SEV_NOTE:    return C_CYAN;
        case GCL_SEV_WARNING: return C_YELLOW;
        case GCL_SEV_ERROR:
        case GCL_SEV_FATAL:   return C_RED;
        default:              return C_RED;
    }
}

static const char *c_reset(void)  { return color_decide() ? C_RESET : ""; }
static const char *c_bold(void)   { return color_decide() ? C_BOLD : ""; }
static const char *c_dim(void)    { return color_decide() ? C_DIM : ""; }
static const char *c_note(void)   { return color_decide() ? C_CYAN : ""; }
static const char *c_help(void)   { return color_decide() ? C_GREEN : ""; }

/* ---------- small helpers ---------- */

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

static void set_str(char **dst, const char *fmt, va_list ap) {
    if (!dst || !fmt) return;
    va_list cp;
    va_copy(cp, ap);
    int n = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (n < 0) return;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return;
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    free(*dst);
    *dst = buf;
}

/* A bounded output sink. With `buf == NULL` it only counts, which lets callers
   ask "how much room do I need?" before allocating. */
typedef struct {
    char  *buf;
    size_t cap;
    size_t used;
} RenderSink;

static void rs_putn(RenderSink *rs, const char *s, size_t n) {
    if (!s || !n) return;
    if (rs->buf && rs->cap && rs->used < rs->cap) {
        size_t room = rs->cap - rs->used - 1;
        size_t cp = n < room ? n : room;
        if (cp) memcpy(rs->buf + rs->used, s, cp);
        rs->buf[rs->used + cp] = '\0';
    }
    rs->used += n;
}

static void rs_put(RenderSink *rs, const char *s) {
    if (s) rs_putn(rs, s, strlen(s));
}

static void rs_putc(RenderSink *rs, char c) {
    if (rs->buf && rs->cap && rs->used + 1 < rs->cap) {
        rs->buf[rs->used] = c;
        rs->buf[rs->used + 1] = '\0';
    }
    rs->used++;
}

static void rs_printf(RenderSink *rs, const char *fmt, ...) {
    char stack[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(stack)) {
        rs_putn(rs, stack, (size_t)n);
        return;
    }
    char *heap = (char *)malloc((size_t)n + 1);
    if (!heap) return;
    va_start(ap, fmt);
    vsnprintf(heap, (size_t)n + 1, fmt, ap);
    va_end(ap);
    rs_putn(rs, heap, (size_t)n);
    free(heap);
}

/* ---------- naming ---------- */

const char *gcl_diag_severity_name(GclSeverity s) {
    switch (s) {
        case GCL_SEV_NOTE:    return "note";
        case GCL_SEV_WARNING: return "warning";
        case GCL_SEV_ERROR:   return "error";
        case GCL_SEV_FATAL:   return "error";
        default:              return "error";
    }
}

const char *gcl_diag_code_str(GclDiagCode c, char *buf, size_t cap) {
    if (!buf || cap == 0) return "";
    if (c == GCL_E_NONE) { buf[0] = '\0'; return buf; }
    snprintf(buf, cap, "GCL%04d", (int)c);
    return buf;
}

GclDiagCode gcl_diag_code_from_string(const char *s) {
    if (!s) return GCL_E_NONE;
    while (*s == ' ' || *s == '\t') s++;
    if ((s[0] == 'G' || s[0] == 'g') &&
        (s[1] == 'C' || s[1] == 'c') &&
        (s[2] == 'L' || s[2] == 'l')) s += 3;
    if (*s < '0' || *s > '9') return GCL_E_NONE;
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    /* Accept both "GCL2001" (the printed form) and the bare "2001". */
    if (v >= 1000 && v <= 4999) return (GclDiagCode)v;
    return GCL_E_NONE;
}
const char *gcl_diag_code_title(GclDiagCode c) {
    switch (c) {
        /* lexer */
        case GCL_E_LEX_UNEXPECTED_CHAR:      return "character is not part of GCL";
        case GCL_E_LEX_UNTERMINATED_STRING:  return "string literal is never closed";
        case GCL_E_LEX_UNTERMINATED_COMMENT: return "block comment is never closed";
        case GCL_E_LEX_BAD_NUMBER:           return "numeric literal is malformed";
        case GCL_E_LEX_UNTERMINATED_HASHCOM: return "GCL block comment is never closed";
        case GCL_E_LEX_BAD_ESCAPE:           return "unknown escape sequence";
        case GCL_E_LEX_EMPTY_CHAR:           return "empty character literal";
        case GCL_E_LEX_MULTI_CHAR:           return "character literal is too long";
        case GCL_E_LEX_UNTERMINATED_CHAR:    return "character literal is never closed";
        /* parser */
        case GCL_E_PARSE_EXPECTED_EXPRESSION:  return "an expression was expected here";
        case GCL_E_PARSE_EXPECTED_TOKEN:       return "a required token is missing";
        case GCL_E_PARSE_EXPECTED_MEMBER:      return "a member name must follow '.'";
        case GCL_E_PARSE_EXPECTED_ARRAY_SIZE:  return "an array dimension must be a number";
        case GCL_E_PARSE_EXPECTED_VAR_NAME:    return "a variable name was expected after the type";
        case GCL_E_PARSE_UNCLOSED_BLOCK:       return "a block is never closed";
        case GCL_E_PARSE_QUALIFIED_TYPE:       return "module-qualified names are not types";
        case GCL_E_PARSE_STRAY_INIT_LIST:      return "initializer list is not attached to a declaration";
        case GCL_E_PARSE_EXPECTED_COND_CLAUSE: return "a condition must be written in parentheses";
        case GCL_E_PARSE_EXPECTED_STATEMENT:   return "a declaration or statement was expected";
        case GCL_E_PARSE_EXPECTED_SEMI:        return "a statement is missing its ';'";
        case GCL_E_PARSE_EXPECTED_TYPE:        return "a type name was expected";
        case GCL_E_PARSE_EXPECTED_CASE_COLON:  return "a case label is missing its ':'";
        case GCL_E_PARSE_EXPECTED_PARAM:       return "a parameter name was expected";
        case GCL_E_PARSE_EXPECTED_MEMBER_DECL: return "a struct member declaration was expected";
        case GCL_E_PARSE_EXPECTED_ENUM_CONST:  return "an enum constant name was expected";
        case GCL_E_PARSE_NOT_ASSIGNABLE:       return "this is not something you can assign to";
        case GCL_E_PARSE_EXPECTED_CLOSE:       return "a bracket is never closed";
        case GCL_E_PARSE_UNEXPECTED_TOKEN:     return "this token cannot appear here";
        /* semantic / runtime */
        case GCL_E_SEM_UNKNOWN_TYPE:    return "unknown type name";
        case GCL_E_SEM_UNKNOWN_MEMBER:  return "unknown member";
        case GCL_E_SEM_UNKNOWN_VAR:     return "name is not defined";
        case GCL_E_SEM_ASSIGN_CONST:    return "cannot assign to a const variable";
        case GCL_E_SEM_UNKNOWN_FUNC:    return "unknown function";
        case GCL_E_SEM_UNKNOWN_MODULE:  return "native module is not loaded";
        case GCL_E_SEM_BAD_ARGC:        return "wrong number of arguments";
        case GCL_E_SEM_DIV_ZERO:        return "division by zero";
        case GCL_E_SEM_INFINITE_LOOP:   return "loop guard tripped (possible infinite loop)";
        case GCL_E_SEM_OUT_OF_BOUNDS:   return "array index is out of range";
        case GCL_E_SEM_RUNTIME:         return "runtime failure";
        case GCL_E_SEM_BAD_ARRAY_SIZE:  return "a char array needs an explicit size";
        case GCL_E_SEM_BAD_CALL:        return "this value is not callable";
        case GCL_E_SEM_THROW:           return "an uncaught 'throw' stopped the run";
        case GCL_E_SEM_LOOP_CONTROL:    return "'break'/'continue' outside its loop";
        case GCL_E_SEM_CALL_DEPTH:      return "the call-depth limit was passed";
        /* preprocessor */
        case GCL_E_PP_INCLUDE_NOT_FOUND: return "included file was not found";
        case GCL_E_PP_UNTERMINATED_IF:   return "a conditional block is never closed";
        case GCL_E_PP_STRAY_ELSE:        return "this directive has no matching '#if'";
        case GCL_E_PP_BAD_DEFINE:        return "malformed '#define'";
        case GCL_E_PP_ERROR_DIRECTIVE:   return "'#error' was hit";
        case GCL_E_PP_BAD_DIRECTIVE:     return "unknown preprocessor directive";
        case GCL_E_PP_BAD_EXPRESSION:    return "malformed '#if' expression";
        default:                         return "diagnostic";
    }
}

const char *gcl_diag_code_help(GclDiagCode c) {
    switch (c) {
        case GCL_E_LEX_UNEXPECTED_CHAR:
            return "GCL source is built from identifiers, numbers, string/char literals,\n"
                   "operators and the comment forms //, /* */, # and #| ... |#.\n"
                   "A character outside that set cannot be tokenised. Look for an unclosed\n"
                   "quote or comment earlier on the line.";
        case GCL_E_LEX_UNTERMINATED_STRING:
            return "A '\"' was opened and the file ended before a closing '\"'.\n"
                   "GCL strings do not continue across a newline. Escape a quote inside the\n"
                   "text as \\\" .";
        case GCL_E_LEX_UNTERMINATED_COMMENT:
            return "A '/*' was opened and never closed with '*/'.\n"
                   "/* */ does NOT nest in GCL: the first '*/' ends the comment.";
        case GCL_E_LEX_UNTERMINATED_HASHCOM:
            return "GCL's own block comment '#| ... |#' must be closed with '|#'.";
        case GCL_E_LEX_BAD_NUMBER:
            return "A numeric literal is digits, optionally with ONE '.', an optional 0x/0X\n"
                   "hexadecimal prefix, and an optional l/L/u/U/f/F suffix.\n"
                   "'1.2.3' and a bare '0x' are not numbers.";
        case GCL_E_LEX_EMPTY_CHAR:
            return "'' is empty. A character literal holds exactly one character:\n"
                   "    char c = 'A';\n"
                   "A backslash itself must be written '\\\\' .";
        case GCL_E_LEX_MULTI_CHAR:
            return "A character literal holds exactly one character. 'ab' is a string;\n"
                   "write it with double quotes: \"ab\".";
        case GCL_E_PARSE_EXPECTED_EXPRESSION:
            return "An expression was expected in a slot that needs a value: the right side\n"
                   "of '=', a function argument, an index, a condition, a return value.\n"
                   "The '= note:' frames point at the construct that opened the slot.";
        case GCL_E_PARSE_EXPECTED_TOKEN:
            return "The parser needed a specific token to finish a construct and did not find\n"
                   "it. The message names both the expected token and the one found.";
        case GCL_E_PARSE_UNCLOSED_BLOCK:
            return "Every '{' needs a matching '}'. The '= note:' frames list the braces that\n"
                   "are still open when the file ends.";
        case GCL_E_PARSE_EXPECTED_SEMI:
            return "Every statement ends with ';' — a declaration, an assignment, a call and\n"
                   "a return all need one.";
        case GCL_E_PARSE_NOT_ASSIGNABLE:
            return "The left side of '=' must be a name, an array element or a struct member\n"
                   "(x, a[i], p.field). A call or a literal cannot be assigned to.";
        case GCL_E_SEM_UNKNOWN_VAR:
            return "The name was never declared before it was read. GCL has no implicit\n"
                   "declaration: declare it first, e.g. 'int score = 0;'. A close match is\n"
                   "suggested when one exists.";
        case GCL_E_SEM_UNKNOWN_MEMBER:
            return "GCL only knows the members a module or struct actually declares.\n"
                   "For native modules the prefix is mandatory: 'Raylib.DrawText', never the\n"
                   "bare 'DrawText'. Check the spelling and the case.";
        case GCL_E_SEM_UNKNOWN_TYPE:
            return "The type name is not one of GCL's built-in types and is not a declared\n"
                   "struct/enum/typedef. Built-ins: char short int long float double void bool\n"
                   "gcChar, the sized int8..int128 / uint8..uint128 / float16..float128, and\n"
                   "'struct X' / 'enum X' / a typedef alias.";
        case GCL_E_SEM_UNKNOWN_MODULE:
            return "The module was never loaded. Add the matching '#native <Name>' line at the\n"
                   "top of the file, e.g. '#native <Stdio>', '#native <Math>', '#native <Raylib>'.";
        case GCL_E_SEM_ASSIGN_CONST:
            return "A variable declared 'const' is read-only. GCL reports every assignment to\n"
                   "it at run time; it is not checked while parsing.";
        case GCL_E_SEM_BAD_ARRAY_SIZE:
            return "A 'char' array must know its capacity:\n"
                   "    char name[32];\n"
                   "The unsized form 'char name[];' cannot be stored.";
        case GCL_E_SEM_INFINITE_LOOP:
            return "A 'while' or 'for' body ran more than 1,000,000 iterations — the\n"
                   "runaway-loop guard. Usually the loop variable is never updated, or the\n"
                   "condition can never become false.";
        case GCL_E_SEM_THROW:
            return "A 'throw' was raised and no enclosing 'try' caught it, so the run\n"
                   "stopped with exit code 1 (an UNCAUGHT throw is fatal, exactly like an\n"
                   "exception that reaches the top of a Python program). Wrap the statement\n"
                   "in 'try { ... } catch (err) { ... }' to handle it.";
        case GCL_E_SEM_LOOP_CONTROL:
            return "'break' leaves a loop or a switch and 'continue' leaves one iteration\n"
                   "of a loop. Outside that context neither has anything to act on, so the\n"
                   "statement is reported instead of silently ending the enclosing block.";
        case GCL_E_SEM_CALL_DEPTH:
            return "Every GCL function call runs on the native C stack, so the interpreter\n"
                   "stops recursion at a fixed depth (8192 by default, 2048 on POSIX builds)\n"
                   "and reports this instead of overflowing the stack. Raise the limit with\n"
                   "the GCL_MAX_CALL_DEPTH environment variable, or make the recursion\n"
                   "iterative.";
        case GCL_E_PP_INCLUDE_NOT_FOUND:
            return "The search order for '#include' is: the directory of the running file, then\n"
                   "<project>/include, then <project>/lib. '<x.gcsf>' and \"x.gcsf\" are\n"
                   "equivalent — both mean 'paste the file here'.";
        case GCL_E_PP_UNTERMINATED_IF:
            return "Every '#if' / '#ifdef' / '#ifndef' needs a matching '#endif'.";
        case GCL_E_PP_STRAY_ELSE:
            return "'#else' / '#elif' / '#endif' close an '#if' block. This one has none open.";
        case GCL_E_PP_ERROR_DIRECTIVE:
            return "'#error' prints a message and, unlike C, does NOT stop the run.";
        default:
            return "Use 'gcl -explain GCL<code>' for the code catalogue.";
    }
}

/* ---------- token helpers ---------- */

GclSpan gcl_diag_span_of_token(const GclToken *t) {
    GclSpan s;
    s.line = t ? t->line : 0;
    s.col = t ? t->col : 0;
    s.len = (t && t->type != TOK_EOF) ? (int)t->len : 0;
    return s;
}

const char *gcl_diag_token_text(const GclToken *t, char *buf, size_t cap) {
    if (!buf || cap == 0) return "";
    if (!t) { snprintf(buf, cap, "nothing"); return buf; }
    if (t->type == TOK_EOF) { snprintf(buf, cap, "end of file"); return buf; }
    if (t->type == TOK_PREPROC) { snprintf(buf, cap, "a preprocessor line"); return buf; }
    /* Truncate absurdly long lexemes so a message stays readable. */
    int n = (int)t->len;
    if (n > 40) n = 40;
    snprintf(buf, cap, "'%.*s'", n, t->lexeme);
    return buf;
}

const char *gcl_diag_token_desc(const GclToken *t, char *buf, size_t cap) {
    if (!buf || cap == 0) return "";
    if (!t) { snprintf(buf, cap, "nothing"); return buf; }
    if (t->type == TOK_EOF) { snprintf(buf, cap, "end of file"); return buf; }

    const char *kind = gcl_token_type_name(t->type);
    if (t->type == TOK_IDENT || t->type == TOK_KEYWORD ||
        t->type == TOK_TYPE_NAME || t->type == TOK_NUMBER) {
        int n = (int)t->len;
        if (n > 40) n = 40;
        snprintf(buf, cap, "%s '%.*s'", kind, n, t->lexeme);
        return buf;
    }
    if (t->type == TOK_STRING) {
        int n = (int)t->len;
        if (n > 40) n = 40;
        snprintf(buf, cap, "string '%.*s'", n, t->lexeme);
        return buf;
    }
    if (t->type == TOK_PREPROC) {
        snprintf(buf, cap, "preprocessor line");
        return buf;
    }
    /* Punctuation/operators are self-describing: '{' , ')' , '+' */
    snprintf(buf, cap, "'%.*s'", (int)t->len, t->lexeme);
    return buf;
}

/* ---------- list lifecycle ---------- */

void gcl_diag_list_init(GclDiagList *dl, const char *file) {
    if (!dl) return;
    memset(dl, 0, sizeof(*dl));
    dl->file = file;
}

static void diag_free_fields(GclDiag *d) {
    if (!d) return;
    free(d->file);
    free(d->message);
    free(d->note);
    free(d->hint);
    for (int i = 0; i < d->related_count; i++) free(d->related[i].label);
    d->related_count = 0;
    d->file = d->message = d->note = d->hint = NULL;
}

void gcl_diag_list_clear(GclDiagList *dl) {
    if (!dl) return;
    for (int i = 0; i < dl->count; i++) diag_free_fields(&dl->items[i]);
    dl->count = 0;
    dl->error_count = 0;
}

void gcl_diag_list_free(GclDiagList *dl) {
    if (!dl) return;
    gcl_diag_list_clear(dl);
    free(dl->items);
    dl->items = NULL;
    dl->cap = 0;
}

int gcl_diag_count(const GclDiagList *dl) { return dl ? dl->count : 0; }
int gcl_diag_error_count(const GclDiagList *dl) { return dl ? dl->error_count : 0; }
int gcl_diag_has_errors(const GclDiagList *dl) { return dl ? dl->error_count > 0 : 0; }

const GclDiag *gcl_diag_first_error(const GclDiagList *dl) {
    if (!dl) return NULL;
    for (int i = 0; i < dl->count; i++)
        if (dl->items[i].severity >= GCL_SEV_ERROR) return &dl->items[i];
    return NULL;
}

/* ---------- add ---------- */

GclDiag *gcl_diag_add(GclDiagList *dl, GclDiagCode code, GclSeverity sev,
                      GclSpan span, const char *fmt, ...) {
    if (!dl) return NULL;
    if (dl->count >= dl->cap) {
        int nc = dl->cap ? dl->cap * 2 : 8;
        GclDiag *ni = (GclDiag *)realloc(dl->items, (size_t)nc * sizeof(GclDiag));
        if (!ni) return NULL;
        dl->items = ni;
        dl->cap = nc;
    }
    GclDiag *d = &dl->items[dl->count];
    memset(d, 0, sizeof(*d));
    d->code = code;
    d->severity = sev;
    d->span = span;
    d->file = dup_str(dl->file);

    va_list ap;
    va_start(ap, fmt);
    set_str(&d->message, fmt, ap);
    va_end(ap);
    if (!d->message) d->message = dup_str(gcl_diag_code_title(code));

    dl->count++;
    if (sev >= GCL_SEV_ERROR) dl->error_count++;
    return d;
}

void gcl_diag_note(GclDiag *d, const char *fmt, ...) {
    if (!d || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    set_str(&d->note, fmt, ap);
    va_end(ap);
}

void gcl_diag_hint(GclDiag *d, const char *fmt, ...) {
    if (!d || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    set_str(&d->hint, fmt, ap);
    va_end(ap);
}

void gcl_diag_relate(GclDiag *d, GclSpan span, const char *fmt, ...) {
    if (!d || d->related_count >= GCL_DIAG_MAX_RELATED) return;
    GclDiagRelated *r = &d->related[d->related_count];
    r->span = span;
    r->label = NULL;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        set_str(&r->label, fmt, ap);
        va_end(ap);
    }
    d->related_count++;
}
/* ---------- source line lookup ---------- */

/* Find [start,end) of 1-based `line` inside src; a trailing CR is dropped so
   the caret column always matches the token column on CRLF files. */
static int source_line_bounds(const char *src, size_t len, int line,
                              size_t *start, size_t *end) {
    if (!src || line < 1) return 0;
    size_t i = 0;
    int cur = 1;
    while (cur < line && i < len) {
        if (src[i] == '\n') cur++;
        i++;
    }
    if (cur != line) return 0;
    size_t s = i;
    size_t e = i;
    while (e < len && src[e] != '\n') e++;
    if (e > s && src[e - 1] == '\r') e--;
    *start = s;
    *end = e;
    return 1;
}

/* Number of decimal digits of a positive int (for the gutter width). */
static int digits_of(int v) {
    int n = 1;
    if (v < 0) v = -v;
    while (v >= 10) { v /= 10; n++; }
    return n;
}

/* ---------- one resolved location ---------- */

typedef struct {
    const char *file;        /* display path; never NULL */
    const char *source;      /* text the line lives in; may be NULL */
    size_t      source_len;
    int         line;        /* line number inside `source` */
    int         col;
    int         len;
    int         exact;       /* 0 when macro expansion makes the caret a guess */
} ResolvedLoc;

/* Translate a span into a location in the user's own file. With a source map
   `span->line` is a PREPROCESSED line; without one it is taken literally. */
static ResolvedLoc resolve_span(const GclDiag *d, const GclSpan *span,
                                const GclSourceMap *m,
                                const char *fallback_source, size_t fallback_len) {
    ResolvedLoc r;
    r.file = d && d->file ? d->file : "<input>";
    r.source = fallback_source;
    r.source_len = fallback_len;
    r.line = span->line;
    r.col = span->col;
    r.len = span->len;
    r.exact = 1;

    if (m && span->line > 0) {
        const char *fname = NULL, *text = NULL;
        size_t tlen = 0;
        int oline = 0;
        if (gcl_smap_lookup(m, span->line, &fname, &text, &tlen, &oline)) {
            r.file = fname ? fname : r.file;
            r.source = text;
            r.source_len = tlen;
            r.line = oline;
            r.exact = !gcl_smap_line_expanded(m, span->line);
        }
    }
    return r;
}

/* Emit the source excerpt and the caret run.
      10 |     Stdio.printf("x", a;
         |                     ^
   `indent` lets a related frame sit slightly deeper than the primary one.
   The caret prefix copies the TABS of the real line (everything else becomes a
   space) so the caret lands under the right character in any terminal. */
static void emit_frame(RenderSink *rs, const ResolvedLoc *loc, const char *indent) {
    if (!loc->source || loc->line < 1) return;

    size_t ls = 0, le = 0;
    if (!source_line_bounds(loc->source, loc->source_len, loc->line, &ls, &le)) return;

    int gutter = digits_of(loc->line);
    int line_len = (int)(le - ls);
    int col = loc->col > 0 ? loc->col : 1;
    if (col - 1 > line_len) col = line_len + 1;

    /* The format used to end with one more `%s` than the call supplied an
       argument for, so the trailing conversion read whatever happened to sit
       in the next register/stack slot: on this toolchain it was 0x23 and the
       `strlen` inside vsnprintf faulted (SIGSEGV). It went unnoticed for as
       long as it did because NOTHING ever called the renderer - the first
       real caller (the CLI) is what exposed it. The gutter is dim, the reset
       belongs BEFORE the source text (which is what makes the reset at
       position 4 the meaningful one), so the extra conversion is simply
       gone. */
    rs_printf(rs, "%s%s%*d | %s%.*s\n", indent, c_dim(), gutter, loc->line,
              c_reset(), line_len, loc->source + ls);

    /* Caret padding: reproduce the real prefix so tabs keep their alignment. */
    rs_printf(rs, "%s%*s | ", indent, gutter + 1, "");
    for (int i = 0; i < col - 1 && (size_t)i < (size_t)line_len; i++) {
        char c = loc->source[ls + (size_t)i];
        rs_putc(rs, c == '\t' ? '\t' : ' ');
    }
    /* Spaces past the end of the line (a zero-length span at EOL). */
    for (int i = line_len; i < col - 1; i++) rs_putc(rs, ' ');

    int caret = loc->len > 0 ? loc->len : 1;
    if (col - 1 + caret > line_len) caret = line_len - (col - 1);
    if (caret < 1) caret = 1;
    /* Never underline trailing blanks: an EOF span would otherwise paint the
       whole rest of the line. */
    while (caret > 1 &&
           (ls + (size_t)(col - 1) + (size_t)(caret - 1)) < le) {
        char c = loc->source[ls + (size_t)(col - 1) + (size_t)(caret - 1)];
        if (c != ' ' && c != '\t') break;
        caret--;
    }

    rs_put(rs, c_for(GCL_SEV_ERROR));
    for (int i = 0; i < caret; i++) rs_putc(rs, i == 0 ? '^' : '~');
    rs_put(rs, c_reset());
    if (!loc->exact) rs_put(rs, "   (macro-expanded line: caret is approximate)");
    rs_putc(rs, '\n');
}

/* ---------- rendering ---------- */

static void render_diag(RenderSink *rs, const GclDiag *d,
                        const GclSourceMap *m,
                        const char *fallback_source, size_t fallback_len) {
    if (!d) return;

    char code_buf[16];
    gcl_diag_code_str(d->code, code_buf, sizeof(code_buf));

    ResolvedLoc primary = resolve_span(d, &d->span, m, fallback_source, fallback_len);

    /* Header: file:line:col: severity: message [CODE]  (machine-parsable) */
    if (primary.line > 0) {
        rs_printf(rs, "%s:%d:%d: ", primary.file, primary.line,
                  primary.col > 0 ? primary.col : 1);
    } else {
        rs_printf(rs, "%s: ", primary.file);
    }
    rs_printf(rs, "%s%s%s%s: %s%s%s",
              c_bold(), c_for(d->severity), gcl_diag_severity_name(d->severity),
              c_reset(),
              c_bold(), d->message ? d->message : "", c_reset());
    if (code_buf[0]) rs_printf(rs, " %s[%s]%s", c_dim(), code_buf, c_reset());
    rs_putc(rs, '\n');

    emit_frame(rs, &primary, "  ");

    /* Secondary locations, each with its own label and frame. */
    for (int i = 0; i < d->related_count; i++) {
        ResolvedLoc loc = resolve_span(d, &d->related[i].span, m,
                                       fallback_source, fallback_len);
        if (d->related[i].label)
            rs_printf(rs, "    %s= note: %s%s\n", c_note(), d->related[i].label, c_reset());
        emit_frame(rs, &loc, "      ");
    }

    if (d->note)
        rs_printf(rs, "    %s= note: %s%s\n", c_note(), d->note, c_reset());
    if (d->hint)
        rs_printf(rs, "    %s= help: %s%s\n", c_help(), d->hint, c_reset());
}

size_t gcl_diag_render(const GclDiag *d, const char *source, size_t source_len,
                       char *buf, size_t cap) {
    if (buf && cap) buf[0] = '\0';
    if (!d) return 0;
    RenderSink rs;
    rs.buf = buf; rs.cap = cap; rs.used = 0;
    render_diag(&rs, d, NULL, source, source_len);
    return rs.used;
}

size_t gcl_diag_render_mapped(const GclDiag *d, const GclSourceMap *m,
                              const char *fallback_source, size_t fallback_len,
                              char *buf, size_t cap) {
    if (buf && cap) buf[0] = '\0';
    if (!d) return 0;
    RenderSink rs;
    rs.buf = buf; rs.cap = cap; rs.used = 0;
    render_diag(&rs, d, m, fallback_source, fallback_len);
    return rs.used;
}

size_t gcl_diag_list_render(const GclDiagList *dl, const char *source,
                            size_t source_len, char *buf, size_t cap) {
    if (buf && cap) buf[0] = '\0';
    if (!dl) return 0;
    RenderSink rs;
    rs.buf = buf; rs.cap = cap; rs.used = 0;
    for (int i = 0; i < dl->count; i++) {
        if (i > 0) rs_putc(&rs, '\n');
        render_diag(&rs, &dl->items[i], NULL, source, source_len);
    }
    return rs.used;
}

size_t gcl_diag_list_render_mapped(const GclDiagList *dl, const GclSourceMap *m,
                                   const char *fallback_source, size_t fallback_len,
                                   char *buf, size_t cap) {
    if (buf && cap) buf[0] = '\0';
    if (!dl) return 0;
    RenderSink rs;
    rs.buf = buf; rs.cap = cap; rs.used = 0;
    for (int i = 0; i < dl->count; i++) {
        if (i > 0) rs_putc(&rs, '\n');
        render_diag(&rs, &dl->items[i], m, fallback_source, fallback_len);
    }
    return rs.used;
}

/* Render one diagnostic into a heap buffer and write it out. */
static void write_one(FILE *out, const GclDiag *d, const GclSourceMap *m,
                      const char *source, size_t source_len) {
    if (!out || !d) return;
    RenderSink probe;
    probe.buf = NULL; probe.cap = 0; probe.used = 0;
    render_diag(&probe, d, m, source, source_len);
    char *tmp = (char *)malloc(probe.used + 1);
    if (!tmp) return;
    RenderSink rs;
    rs.buf = tmp; rs.cap = probe.used + 1; rs.used = 0;
    render_diag(&rs, d, m, source, source_len);
    fputs(tmp, out);
    free(tmp);
}

void gcl_diag_print(FILE *out, const GclDiag *d, const char *source, size_t source_len) {
    write_one(out, d, NULL, source, source_len);
}

void gcl_diag_print_mapped(FILE *out, const GclDiag *d, const GclSourceMap *m,
                           const char *fallback_source, size_t fallback_len) {
    write_one(out, d, m, fallback_source, fallback_len);
}

void gcl_diag_list_print(FILE *out, const GclDiagList *dl, const char *source,
                         size_t source_len) {
    if (!out || !dl) return;
    for (int i = 0; i < dl->count; i++) {
        if (i > 0) fputc('\n', out);
        write_one(out, &dl->items[i], NULL, source, source_len);
    }
}

void gcl_diag_list_print_mapped(FILE *out, const GclDiagList *dl, const GclSourceMap *m,
                                const char *fallback_source, size_t fallback_len) {
    if (!out || !dl) return;
    for (int i = 0; i < dl->count; i++) {
        if (i > 0) fputc('\n', out);
        write_one(out, &dl->items[i], m, fallback_source, fallback_len);
    }
}

void gcl_diag_print_summary(FILE *out, const GclDiagList *dl) {
    if (!out || !dl) return;
    int errors = gcl_diag_error_count(dl);
    int warnings = 0, notes = 0;
    for (int i = 0; i < dl->count; i++) {
        if (dl->items[i].severity == GCL_SEV_WARNING) warnings++;
        else if (dl->items[i].severity == GCL_SEV_NOTE) notes++;
    }
    if (errors == 0 && warnings == 0 && notes == 0) return;

    int parts = 0;
    char line[160];
    line[0] = '\0';
    if (errors) {
        snprintf(line + strlen(line), sizeof(line) - strlen(line),
                 "%d error%s", errors, errors == 1 ? "" : "s");
        parts++;
    }
    if (warnings) {
        snprintf(line + strlen(line), sizeof(line) - strlen(line),
                 "%s%d warning%s", parts ? ", " : "",
                 warnings, warnings == 1 ? "" : "s");
        parts++;
    }
    if (notes) {
        snprintf(line + strlen(line), sizeof(line) - strlen(line),
                 "%s%d note%s", parts ? ", " : "", notes, notes == 1 ? "" : "s");
    }
    fprintf(out, "%s%s%s generated.\n", c_bold(), line, c_reset());
}

char *gcl_diag_first_to_legacy_string(const GclDiagList *dl) {
    if (!dl || dl->count == 0) return NULL;
    const GclDiag *d = gcl_diag_first_error(dl);
    if (!d) d = &dl->items[0];
    /* The historical shape was "<message> at <line>:<col>". Callers that were
       built against the old single-string API keep working unchanged. */
    char stack[1024];
    int n;
    if (d->span.line > 0)
        n = snprintf(stack, sizeof(stack), "%s at %d:%d",
                     d->message ? d->message : "error", d->span.line,
                     d->span.col > 0 ? d->span.col : 1);
    else
        n = snprintf(stack, sizeof(stack), "%s", d->message ? d->message : "error");
    if (n < 0) return NULL;
    if ((size_t)n < sizeof(stack)) return dup_str(stack);
    char *heap = (char *)malloc((size_t)n + 1);
    if (!heap) return NULL;
    if (d->span.line > 0)
        snprintf(heap, (size_t)n + 1, "%s at %d:%d",
                 d->message ? d->message : "error", d->span.line,
                 d->span.col > 0 ? d->span.col : 1);
    else
        snprintf(heap, (size_t)n + 1, "%s", d->message ? d->message : "error");
    return heap;
}
