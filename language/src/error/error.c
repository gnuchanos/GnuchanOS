#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================
// GCL Error System Implementation
// ============================================================

static int g_use_colors = 1;
static int g_use_json   = 0;
static int g_werror     = 0;
static int g_error_count = 0;
static int g_warning_count = 0;

void error_init(void) {
    g_error_count = 0;
    g_warning_count = 0;
    g_use_colors = 1;
    g_use_json = 0;
    g_werror = 0;
}

void error_set_colors(int enable) { g_use_colors = enable; }
void error_set_json(int enable)   { g_use_json = enable; }
void error_set_werror(int enable)  { g_werror = enable; }

int error_count(void)   { return g_error_count; }
int warning_count(void) { return g_warning_count; }

static const char *code_to_string(int code, int is_warning) {
    static char buf[16];
    if (is_warning) {
        snprintf(buf, sizeof(buf), "W%03d", code - 100);
    } else {
        snprintf(buf, sizeof(buf), "E%03d", code);
    }
    return buf;
}

static const char *level_label(GclErrorLevel level) {
    switch (level) {
    case LEVEL_ERROR:   return "error";
    case LEVEL_WARNING: return "warning";
    case LEVEL_NOTE:    return "note";
    case LEVEL_HELP:    return "help";
    default: return "?";
    }
}

static const char *level_color(GclErrorLevel level) {
    if (!g_use_colors) return "";
    switch (level) {
    case LEVEL_ERROR:   return COLOR_RED;
    case LEVEL_WARNING: return COLOR_YELLOW;
    case LEVEL_NOTE:    return COLOR_BLUE;
    case LEVEL_HELP:    return COLOR_GREEN;
    default: return "";
    }
}

void error_report(GclErrorLevel level, int code, const char *stage,
                  GclSourceLoc loc, const char *message,
                  const char *help, const char *note,
                  const char *source_line) {
    if (level == LEVEL_ERROR) g_error_count++;
    if (level == LEVEL_WARNING) g_warning_count++;

    if (g_werror && level == LEVEL_WARNING) {
        level = LEVEL_ERROR;
        g_error_count++;
    }

    if (g_use_json) {
        // JSON output
        fprintf(stderr, "{\n");
        fprintf(stderr, "  \"level\": \"%s\",\n", level_label(level));
        fprintf(stderr, "  \"code\": \"%s\",\n", code_to_string(code, level == LEVEL_WARNING));
        fprintf(stderr, "  \"stage\": \"%s\",\n", stage ? stage : "");
        fprintf(stderr, "  \"file\": \"%s\",\n", loc.filename ? loc.filename : "");
        fprintf(stderr, "  \"line\": %d,\n", loc.line);
        fprintf(stderr, "  \"col\": %d,\n", loc.col);
        fprintf(stderr, "  \"message\": \"%s\"", message ? message : "");
        if (help) fprintf(stderr, ",\n  \"suggestion\": \"%s\"", help);
        if (note) fprintf(stderr, ",\n  \"note\": \"%s\"", note);
        fprintf(stderr, "\n}\n");
        return;
    }

    // Clang/Rust style output
    const char *color = level_color(level);
    const char *reset = g_use_colors ? COLOR_RESET : "";

    // Header: error[E001] or warning[W001]
    fprintf(stderr, "%s%s[%s]%s", color, level_label(level),
            code_to_string(code, level == LEVEL_WARNING), reset);
    if (stage) {
        fprintf(stderr, " (%s)", stage);
    }
    fprintf(stderr, ": %s\n", message ? message : "");

    // Source location
    if (loc.filename) {
        fprintf(stderr, " %s--> %s:%d:%d\n", color, loc.filename, loc.line, loc.col);
    } else {
        fprintf(stderr, " %s--> %d:%d\n", color, loc.line, loc.col);
    }
    fprintf(stderr, "%s", reset);

    // Source line with caret
    if (source_line) {
        // Show line number
        fprintf(stderr, "  %s|\n", g_use_colors ? COLOR_BLUE : "");
        fprintf(stderr, "%d %s| %s\n", loc.line, g_use_colors ? COLOR_BLUE : "", source_line);
        fprintf(stderr, "  %s|%s ", g_use_colors ? COLOR_BLUE : "", reset);

        // Caret at column
        for (int i = 1; i < loc.col; i++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "%s^%s", color, reset);

        // Underline
        int len = (int)strlen(source_line);
        int remaining = len - loc.col;
        if (remaining > 0) {
            for (int i = 0; i < remaining && i < 10; i++) {
                fprintf(stderr, "%s~%s", color, reset);
            }
        }
        fprintf(stderr, "\n");
    }

    // Help
    if (help) {
        fprintf(stderr, "  %s|%s %shelp:%s %s\n",
                g_use_colors ? COLOR_BLUE : "", reset,
                g_use_colors ? COLOR_GREEN : "", reset, help);
    }

    // Note
    if (note) {
        fprintf(stderr, "  %s|%s %snote:%s %s\n",
                g_use_colors ? COLOR_BLUE : "", reset,
                g_use_colors ? COLOR_BLUE : "", reset, note);
    }

    fprintf(stderr, "\n");
}

void error_syntax(int code, GclSourceLoc loc, const char *message,
                  const char *help, const char *source_line) {
    error_report(LEVEL_ERROR, code, "parser", loc, message, help, NULL, source_line);
}

void error_warning(int code, GclSourceLoc loc, const char *message,
                   const char *help, const char *source_line) {
    error_report(LEVEL_WARNING, code, "parser", loc, message, help, NULL, source_line);
}
