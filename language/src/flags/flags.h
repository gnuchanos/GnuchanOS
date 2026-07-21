// flags.h — GCL CLI flag parser
#ifndef GCL_FLAGS_H
#define GCL_FLAGS_H

typedef enum {
    PIPELINE_NONE,
    PIPELINE_LEXER,
    PIPELINE_PARSER,
    PIPELINE_AST,
    PIPELINE_IR,
    PIPELINE_CODEGEN,
    PIPELINE_FULL,
} PipelineMode;

typedef struct {
    // Meta
    int show_version;
    int show_help;
    int debug;

    // Input / Output
    const char* input_file;
    const char* output_file;

    // Pipeline
    PipelineMode pipeline;
    int interactive;

    // Paths (can appear multiple times)
    const char** linclude;
    int linclude_count;
    const char** llib;
    int llib_count;
    const char** lextend;
    int lextend_count;
} GclFlags;

GclFlags* gcl_flags_parse(int argc, char** argv);
void      gcl_flags_free(GclFlags* f);

#endif
