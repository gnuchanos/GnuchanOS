#ifndef GCL_CLI_H
#define GCL_CLI_H

// ============================================================
// GCL CLI — Command-line interface
// ============================================================

typedef struct {
    const char *input_file;
    const char *output_file;
    int         run_mode;
    int         lexer_dump;
    int         parser_dump;
    int         ast_dump;
    int         ir_dump;
    int         codegen_dump;
    int         debug;
    int         version;
    int         help;
    const char *include_paths[16];
    int         include_count;
    const char *lib_paths[16];
    int         lib_count;
} GclCliOptions;

void cli_parse(int argc, char *argv[], GclCliOptions *opts);
void cli_print_help(void);
void cli_print_version(void);

#endif // GCL_CLI_H
