#ifndef GCL_ERRORS_H
#define GCL_ERRORS_H

// ============================================================
// GCL Error Code Table — Clang/Rust style error reporting
// ============================================================

typedef enum {
    // ── Internal ──────────────────────────────────────────
    E000 = 0,   // internal compiler error

    // ── Syntax ────────────────────────────────────────────
    E001,       // syntax error (general)
    E002,       // expected identifier
    E003,       // expected ';'
    E004,       // expected expression
    E005,       // invalid token / unexpected character

    // ── Type ──────────────────────────────────────────────
    E010,       // type error (general)
    E011,       // undeclared variable
    E012,       // redeclaration
    E013,       // type mismatch

    // ── Memory (runtime) ──────────────────────────────────
    E020,       // memory error (general)
    E021,       // double free
    E022,       // null pointer dereference
    E023,       // out of bounds

    // ── I/O ───────────────────────────────────────────────
    E030,       // I/O error (general)
    E031,       // file not found
} GclErrorCode;

// ── Warning Codes ─────────────────────────────────────────
typedef enum {
    W001 = 100, // unused variable
    W002,       // implicit cast (possible data loss)
    W003,       // unreachable code
    W004,       // deprecated feature
} GclWarningCode;

// ── Error Level ───────────────────────────────────────────
typedef enum {
    LEVEL_ERROR   = 0,
    LEVEL_WARNING = 1,
    LEVEL_NOTE    = 2,
    LEVEL_HELP    = 3,
} GclErrorLevel;

// ── Source Location ───────────────────────────────────────
typedef struct {
    const char *filename;
    int         line;
    int         col;
} GclSourceLoc;

// ── Error Report ──────────────────────────────────────────
typedef struct {
    GclErrorLevel  level;
    int            code;
    const char    *stage;       // lexer, parser, codegen, etc.
    GclSourceLoc   loc;
    const char    *message;
    const char    *help;        // optional suggestion
    const char    *note;        // optional extra info
    const char    *source_line; // the actual source line for caret display
} GclErrorReport;

// ── Error API ─────────────────────────────────────────────
void error_init(void);
void error_set_colors(int enable);        // 1 = color, 0 = plain
void error_set_json(int enable);          // 1 = JSON output
void error_set_werror(int enable);        // 1 = warnings as errors

void error_report(GclErrorLevel level, int code, const char *stage,
                  GclSourceLoc loc, const char *message,
                  const char *help, const char *note,
                  const char *source_line);

void error_syntax(int code, GclSourceLoc loc, const char *message,
                  const char *help, const char *source_line);

void error_warning(int code, GclSourceLoc loc, const char *message,
                   const char *help, const char *source_line);

int  error_count(void);                   // total errors reported
int  warning_count(void);                 // total warnings reported

// ── Color codes (ANSI) ────────────────────────────────────
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

#endif // GCL_ERRORS_H
