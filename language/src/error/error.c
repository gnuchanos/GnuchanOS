/* Error: Rust-stili hata raporlama (mor tema) */
#include "gcl.h"
#include "error.h"
#include <stdarg.h>

static const char *g_filename = "<input>";

static const char *code_str(ErrCode c) {
    switch (c) {
    case E_OK: return "E000"; case E_INTERNAL: return "E001";
    case E_SYNTAX: return "E002"; case E_EXPECTED_IDENT: return "E003";
    case E_EXPECTED_SEMI: return "E004"; case E_EXPECTED_EXPR: return "E005";
    case E_INVALID_TOKEN: return "E006";
    case E_TYPE: return "E010"; case E_UNDECLARED: return "E011";
    case E_REDECLARED: return "E012"; case E_MISMATCH: return "E013";
    case E_MEM_OUT: return "E020"; case E_DOUBLE_FREE: return "E021";
    case E_NULL_DEREF: return "E022"; case E_BOUNDS: return "E023";
    case E_IO_OPEN: return "E030"; case E_IO_READ: return "E031";
    case E_IO_WRITE: return "E032";
    default: return "E???";
    }
}

void error_set_filename(const char *fname) {
    if (fname) g_filename = fname;
}

/* source satirini bul ve caret ile goster */
static void print_source_line(const char *src, int line, int col, int col_end) {
    if (!src) return;

    int l = 1;
    const char *p = src;
    const char *line_start = src;
    while (*p && l < line) {
        if (*p == '\n') { l++; line_start = p + 1; }
        p++;
    }
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n') line_end++;

    fprintf(stderr, " %s|%s\n", C_LPURPLE, C_RESET);
    fprintf(stderr, " %s%d%s %s|%s ", C_LPURPLE, line, C_RESET, C_LPURPLE, C_RESET);
    fwrite(line_start, 1, (size_t)(line_end - line_start), stderr);
    fputc('\n', stderr);
    fprintf(stderr, " %s|%s ", C_LPURPLE, C_RESET);

    int pos = 1;
    const char *cp = line_start;
    while (cp < line_end && pos < col) { fputc(' ', stderr); cp++; pos++; }

    fprintf(stderr, "%s", C_PURPLE);
    int end = col_end > col ? col_end : col + 1;
    for (int i = col; i < end && cp <= line_end; i++) {
        if (*cp == '\t') fputc('^', stderr);
        else fputc('^', stderr);
        cp++;
    }
    fprintf(stderr, "%s\n", C_RESET);
}

void error_report(ErrCode code, SourceLoc loc, const char *src,
                  const char *fmt, ...) {
    const char *fname = loc.filename ? loc.filename : g_filename;
    fprintf(stderr, "%serror%s[%s]: ", C_RED, C_RESET, code_str(code));

    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);

    fprintf(stderr, " %s-->%s %s:%d:%d\n", C_PURPLE, C_RESET, fname, loc.line, loc.col);
    print_source_line(src, loc.line, loc.col, loc.col);
}

void warning_report(SourceLoc loc, const char *fmt, ...) {
    const char *fname = loc.filename ? loc.filename : g_filename;
    fprintf(stderr, "%swarning%s: ", C_YELLOW, C_RESET);

    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n %s-->%s %s:%d:%d\n", C_PURPLE, C_RESET, fname, loc.line, loc.col);
}
