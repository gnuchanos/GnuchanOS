/* ================================================================
   gcl.c — Gnuchan C-Like HEADERS implementation (v0.1)
   ONLY implements what's in GCL_READ_ONLY.MD HEADERS section:
   #include, #lib, #extern, comments, #define, #undef,
   #ifdef, #ifndef, #if, #elif, #else, #endif, #error,
   #pragma message, #line
   ================================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_LINE 8192
#define MAX_INCLUDE_DEPTH 64
#define MAX_MACROS 256

/* ── Simple macro table ──────────────────────────────────── */
typedef struct {
    char name[256];
    char value[1024];
} Macro;

static Macro macros[MAX_MACROS];
static int macro_count = 0;

static void macro_set(const char *name, const char *value) {
    for (int i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            strncpy(macros[i].value, value, sizeof(macros[i].value)-1);
            return;
        }
    }
    if (macro_count < MAX_MACROS) {
        strncpy(macros[macro_count].name, name, sizeof(macros[macro_count].name)-1);
        strncpy(macros[macro_count].value, value, sizeof(macros[macro_count].value)-1);
        macro_count++;
    }
}

static const char *macro_get(const char *name) {
    for (int i = 0; i < macro_count; i++)
        if (strcmp(macros[i].name, name) == 0) return macros[i].value;
    return NULL;
}

static void macro_del(const char *name) {
    for (int i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            macros[i] = macros[macro_count-1];
            macro_count--;
            return;
        }
    }
}

/* ── Process a .gcsf file ─────────────────────────────────── */
static int depth = 0;

static int process_file(const char *path, FILE *out);

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end+1) = '\0';
    return s;
}

static char *eat_line_rest(FILE *f) {
    static char buf[MAX_LINE];
    int c, i = 0;
    while ((c = fgetc(f)) != EOF && c != '\n' && i < MAX_LINE-1)
        buf[i++] = (char)c;
    buf[i] = '\0';
    return buf;
}

static int process_file(const char *path, FILE *out) {
    if (depth >= MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "gcl: max include depth exceeded for '%s'\n", path);
        return -1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        /* Try adding .gcsf */
        char alt[1024];
        snprintf(alt, sizeof(alt), "%s.gcsf", path);
        f = fopen(alt, "r");
        if (!f) {
            fprintf(stderr, "gcl: cannot open '%s'\n", path);
            return -1;
        }
    }

    char line[MAX_LINE];
    int skipping = 0; /* skip depth for #if 0 */
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char *s = trim(line);

        /* Empty line */
        if (*s == '\0') { fputs(line, out); continue; }

        /* ── Comments ──────────────────────────────────── */
        /* #| ... |# block comment */
        if (strncmp(s, "#|", 2) == 0) {
            while (fgets(line, sizeof(line), f)) {
                line_num++;
                if (strstr(line, "|#")) break;
            }
            continue;
        }
        /* // comment */
        if (strncmp(s, "//", 2) == 0) {
            fprintf(out, "//%s\n", s+2);
            continue;
        }
        /* # single line comment (not a directive) */
        if (s[0] == '#' && s[1] != 'i' && s[1] != 'l' && s[1] != 'e' &&
            s[1] != 'd' && s[1] != 'u' && s[1] != 'p' && s[1] != '|') {
            /* Check specific directives */
            if (strncmp(s, "#include", 8) != 0 &&
                strncmp(s, "#lib", 4) != 0 &&
                strncmp(s, "#extern", 7) != 0 &&
                strncmp(s, "#define", 7) != 0 &&
                strncmp(s, "#undef", 6) != 0 &&
                strncmp(s, "#ifdef", 6) != 0 &&
                strncmp(s, "#ifndef", 7) != 0 &&
                strncmp(s, "#if", 3) != 0 &&
                strncmp(s, "#elif", 5) != 0 &&
                strncmp(s, "#else", 5) != 0 &&
                strncmp(s, "#endif", 6) != 0 &&
                strncmp(s, "#error", 6) != 0 &&
                strncmp(s, "#pragma", 7) != 0 &&
                strncmp(s, "#line", 5) != 0) {
                continue; /* # comment */
            }
        }

        /* ── #include ──────────────────────────────────── */
        if (strncmp(s, "#include", 8) == 0) {
            char *p = s + 8;
            while (*p == ' ' || *p == '\t') p++;
            char fname[512]; int fi = 0;
            if (*p == '<') {
                p++;
                while (*p && *p != '>' && fi < 511) fname[fi++] = *p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"' && fi < 511) fname[fi++] = *p++;
            } else {
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && fi < 511)
                    fname[fi++] = *p++;
            }
            fname[fi] = '\0';

            depth++;
            process_file(fname, out);
            depth--;
            continue;
        }

        /* ── #lib ──────────────────────────────────────── */
        if (strncmp(s, "#lib", 4) == 0) {
            char *p = s + 4;
            while (*p == ' ' || *p == '\t') p++;
            char fname[512]; int fi = 0;
            if (*p == '<') { p++; while (*p && *p != '>' && fi < 511) fname[fi++] = *p++; }
            else if (*p == '"') { p++; while (*p && *p != '"' && fi < 511) fname[fi++] = *p++; }
            else { while (*p && *p != ' ' && fi < 511) fname[fi++] = *p++; }
            fname[fi] = '\0';
            fprintf(out, "/* #lib \"%s\" */\n", fname);
            continue;
        }

        /* ── #extern ──────────────────────────────────── */
        if (strncmp(s, "#extern", 7) == 0) {
            char *p = s + 7;
            while (*p == ' ' || *p == '\t') p++;
            char fname[512]; int fi = 0;
            if (*p == '<') { p++; while (*p && *p != '>' && fi < 511) fname[fi++] = *p++; }
            else if (*p == '"') { p++; while (*p && *p != '"' && fi < 511) fname[fi++] = *p++; }
            else { while (*p && *p != ' ' && fi < 511) fname[fi++] = *p++; }
            fname[fi] = '\0';
            fprintf(out, "/* #extern \"%s\" */\n", fname);
            continue;
        }

        /* ── #define ──────────────────────────────────── */
        if (strncmp(s, "#define", 7) == 0) {
            char *p = s + 7;
            while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && ni < 255)
                name[ni++] = *p++;
            name[ni] = '\0';

            while (*p == ' ' || *p == '\t') p++;
            char val[1024]; int vi = 0;
            while (*p && *p != '\n' && vi < 1023) val[vi++] = *p++;
            val[vi] = '\0';

            /* Trim trailing whitespace from value */
            char *ve = val + vi - 1;
            while (ve >= val && (*ve == ' ' || *ve == '\t')) *(ve--) = '\0';

            macro_set(name, val);
            fprintf(out, "#define %s %s\n", name, val);
            continue;
        }

        /* ── #undef ──────────────────────────────────── */
        if (strncmp(s, "#undef", 6) == 0) {
            char *p = s + 6;
            while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && ni < 255)
                name[ni++] = *p++;
            name[ni] = '\0';
            macro_del(name);
            fprintf(out, "#undef %s\n", name);
            continue;
        }

        /* ── #ifdef / #ifndef ─────────────────────────── */
        if (strncmp(s, "#ifdef", 6) == 0 || strncmp(s, "#ifndef", 7) == 0) {
            int is_ifndef = (s[2] == 'n'); /* #ifndef */
            char *p = is_ifndef ? s + 7 : s + 6;
            while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\n' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';

            int defined = (macro_get(name) != NULL);
            int skip = (is_ifndef ? defined : !defined);

            if (skip) {
                int nest = 1;
                while (nest > 0 && fgets(line, sizeof(line), f)) {
                    char *ls = trim(line);
                    if (strncmp(ls, "#if", 3) == 0) nest++;
                    else if (strncmp(ls, "#endif", 6) == 0) nest--;
                    else if (nest == 1 && strncmp(ls, "#else", 5) == 0) {
                        nest = 0; /* stop skipping at #else */
                    }
                }
            } else {
                fprintf(out, "#ifdef %s\n", name);
                /* Process until #else or #endif */
                int nest = 1;
                while (nest > 0 && fgets(line, sizeof(line), f)) {
                    char *ls = trim(line);
                    if (strncmp(ls, "#if", 3) == 0) { nest++; fprintf(out, "%s", line); }
                    else if (strncmp(ls, "#endif", 6) == 0) nest--;
                    else if (nest == 1 && strncmp(ls, "#else", 5) == 0) {
                        /* Skip else block */
                        nest = 0;
                        break;
                    }
                    else fprintf(out, "%s", line);
                }
                /* If we hit #else, skip until #endif */
                if (nest == 0) {
                    int nest2 = 1;
                    while (nest2 > 0 && fgets(line, sizeof(line), f)) {
                        char *ls = trim(line);
                        if (strncmp(ls, "#if", 3) == 0) nest2++;
                        else if (strncmp(ls, "#endif", 6) == 0) nest2--;
                    }
                }
                fprintf(out, "\n#endif\n");
            }
            continue;
        }

        /* ── #if / #elif / #else / #endif ─────────────── */
        if (strncmp(s, "#if", 3) == 0 && s[3] != 'd' && s[3] != 'n') {
            char *p = s + 3;
            /* Very simple: just check if macro value != "0" */
            while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '=' && *p != '!' && *p != '\n' && ni < 255)
                name[ni++] = *p++;
            name[ni] = '\0';

            const char *val = macro_get(name);
            int truthy = (val && val[0] != '\0' && strcmp(val, "0") != 0);

            if (!truthy) {
                int nest = 1;
                while (nest > 0 && fgets(line, sizeof(line), f)) {
                    char *ls = trim(line);
                    if (strncmp(ls, "#if", 3) == 0) nest++;
                    else if (strncmp(ls, "#endif", 6) == 0) nest--;
                    else if (nest == 1 && (strncmp(ls, "#elif", 5) == 0 || strncmp(ls, "#else", 5) == 0))
                        nest = 0;
                }
            } else {
                fprintf(out, "#if %s\n", name);
                int nest = 1;
                while (nest > 0 && fgets(line, sizeof(line), f)) {
                    char *ls = trim(line);
                    if (strncmp(ls, "#if", 3) == 0) { nest++; fprintf(out, "%s", line); }
                    else if (strncmp(ls, "#endif", 6) == 0) nest--;
                    else if (nest == 1 && (strncmp(ls, "#elif", 5) == 0 || strncmp(ls, "#else", 5) == 0)) {
                        int nest2 = 1;
                        while (nest2 > 0 && fgets(line, sizeof(line), f)) {
                            ls = trim(line);
                            if (strncmp(ls, "#if", 3) == 0) nest2++;
                            else if (strncmp(ls, "#endif", 6) == 0) nest2--;
                        }
                        break;
                    }
                    else fprintf(out, "%s", line);
                }
                fprintf(out, "#endif\n");
            }
            continue;
        }

        /* ── #error ──────────────────────────────────── */
        if (strncmp(s, "#error", 6) == 0) {
            fprintf(stderr, "gcl: %s:%d: #error %s\n", path, line_num, s+6);
            fclose(f);
            return -1;
        }

        /* ── #pragma message ─────────────────────────── */
        if (strncmp(s, "#pragma message", 15) == 0) {
            char *msg = strchr(s, '(');
            if (msg) {
                msg++;
                char *end = strchr(msg, ')');
                if (end) *end = '\0';
                if (*msg == '"') msg++;
                end = strchr(msg, '"');
                if (end) *end = '\0';
                fprintf(stderr, "note: %s\n", msg);
            }
            continue;
        }

        /* ── #line ──────────────────────────────────── */
        if (strncmp(s, "#line", 5) == 0) {
            fprintf(out, "%s\n", s);
            continue;
        }

        /* ── Regular code — macro expansion ──────────── */
        {
            char expanded[MAX_LINE * 2];
            int ei = 0;
            char *p = s;
            while (*p && ei < (int)(sizeof(expanded) - 2)) {
                /* Check for macro name at current position */
                int matched = 0;
                for (int mi = 0; mi < macro_count; mi++) {
                    size_t mlen = strlen(macros[mi].name);
                    if (strncmp(p, macros[mi].name, mlen) == 0 &&
                        (p[mlen] == '\0' || p[mlen] == ' ' || p[mlen] == '\t' ||
                         p[mlen] == '\n' || p[mlen] == '\r' || p[mlen] == ';' ||
                         p[mlen] == ',' || p[mlen] == ')' || p[mlen] == '(' ||
                         p[mlen] == '[' || p[mlen] == ']' || p[mlen] == '+' ||
                         p[mlen] == '-' || p[mlen] == '*' || p[mlen] == '/' ||
                         p[mlen] == '%' || p[mlen] == '=' || p[mlen] == '<' ||
                         p[mlen] == '>' || p[mlen] == '!' || p[mlen] == '.' ||
                         p[mlen] == '&' || p[mlen] == '|' || p[mlen] == '^')) {
                        strcpy(expanded + ei, macros[mi].value);
                        ei += (int)strlen(macros[mi].value);
                        p += mlen;
                        matched = 1;
                        break;
                    }
                }
                if (!matched) {
                    expanded[ei++] = *p++;
                }
            }
            expanded[ei] = '\0';
            /* Also keep trailing whitespace/newline from original line */
            fputs(expanded, out);
            /* If original line had trailing content after trim, preserve newline */
            if (line[strlen(line)-1] == '\n') fputc('\n', out);
        }
    }

    fclose(f);
    return 0;
}

/* ── Main ─────────────────────────────────────────────────── */
static void print_help(void) {
    printf("GCL v0.1 — Gnuchan C-Like Language (HEADERS)\n\n");
    printf("gcl <file.gcsf> [-o out.c]  → process file\n");
    printf("gcl -h                       → help\n");
    printf("gcl -v                       → version\n");
}

int main(int argc, char **argv) {
    char *input = NULL;
    char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            print_help(); return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-version") == 0) {
            printf("GCL v0.1\n"); return 0;
        }
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            output = argv[++i];
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input) {
        print_help();
        return 1;
    }

    FILE *out = output ? fopen(output, "w") : stdout;
    if (!out) {
        fprintf(stderr, "gcl: cannot open output '%s'\n", output);
        return 1;
    }

    fprintf(out, "/* Generated by GCL v0.1 */\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");

    int ret = process_file(input, out);

    if (output) fclose(out);
    return ret;
}
