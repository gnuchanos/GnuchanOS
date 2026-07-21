// flags.c — GCL CLI flag parser
#include "flags/flags.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void add_path(const char*** list, int* count, const char* value) {
    *list = realloc(*list, (*count + 1) * sizeof(const char*));
    (*list)[*count] = value;
    (*count)++;
}

GclFlags* gcl_flags_parse(int argc, char** argv) {
    GclFlags* f = calloc(1, sizeof(GclFlags));
    if (!f) return NULL;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        // Meta flags (no value after)
        if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0) {
            f->show_version = 1;
        }
        else if (strcmp(arg, "-help") == 0 || strcmp(arg, "-h") == 0) {
            f->show_help = 1;
        }
        else if (strcmp(arg, "-debug") == 0) {
            f->debug = 1;
        }

        // Pipeline mode flags (takes a file argument)
        else if (strcmp(arg, "-lexer") == 0) {
            f->pipeline = PIPELINE_LEXER;
            if (i + 1 < argc) { f->input_file = argv[++i]; }
        }
        else if (strcmp(arg, "-parser") == 0) {
            f->pipeline = PIPELINE_PARSER;
            if (i + 1 < argc) { f->input_file = argv[++i]; }
        }
        else if (strcmp(arg, "-ast") == 0) {
            f->pipeline = PIPELINE_AST;
            if (i + 1 < argc) { f->input_file = argv[++i]; }
        }
        else if (strcmp(arg, "-ir") == 0) {
            f->pipeline = PIPELINE_IR;
            if (i + 1 < argc) { f->input_file = argv[++i]; }
        }
        else if (strcmp(arg, "-codegen") == 0) {
            f->pipeline = PIPELINE_CODEGEN;
            if (i + 1 < argc) { f->input_file = argv[++i]; }
        }

        // Output file
        else if (strcmp(arg, "-o") == 0) {
            if (i + 1 < argc) { f->output_file = argv[++i]; }
        }

        // Path / Library flags (takes one value)
        else if (strcmp(arg, "-linclude") == 0) {
            if (i + 1 < argc) { add_path(&f->linclude, &f->linclude_count, argv[++i]); }
        }
        else if (strcmp(arg, "-llib") == 0) {
            if (i + 1 < argc) { add_path(&f->llib, &f->llib_count, argv[++i]); }
        }
        else if (strcmp(arg, "-lextend") == 0) {
            if (i + 1 < argc) { add_path(&f->lextend, &f->lextend_count, argv[++i]); }
        }

        // Unknown flag starting with -
        else if (arg[0] == '-') {
            fprintf(stderr, "gcl: unknown flag '%s'\n", arg);
        }

        // Positional argument: input file
        else {
            f->input_file = arg;
        }
    }

    // If no pipeline explicitly set but we have an input file → full pipeline
    if (f->pipeline == PIPELINE_NONE && f->input_file != NULL) {
        f->pipeline = PIPELINE_FULL;
    }

    // If nothing at all → interactive shell
    if (!f->show_version && !f->show_help && !f->input_file) {
        f->interactive = 1;
    }

    return f;
}

void gcl_flags_free(GclFlags* f) {
    if (!f) return;
    free(f->linclude);
    free(f->llib);
    free(f->lextend);
    free(f);
}
