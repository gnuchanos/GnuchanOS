#ifndef GCL_PARSER_H
#define GCL_PARSER_H

#include "ast.h"
#include "tokens.h"
#include "lexer/lexer.h"

// ============================================================
// GCL Parser — Recursive Descent
// ============================================================

typedef struct {
    GclLexer     lexer;         // embedded lexer
    GclToken     current;       // current token
    GclToken     previous;      // previous token
    int          had_error;     // error flag
    int          panic_mode;    // skip-to-semicolon recovery
    const char  *filename;      // source filename

    // Debug flags
    int          dump_tokens;
    int          dump_ast;
} GclParser;

// ── Parser API ────────────────────────────────────────────
void        parser_init(GclParser *parser, const char *source, const char *filename);
GclAstNode *parser_parse(GclParser *parser);   // returns AST_PROGRAM
int         parser_had_error(GclParser *parser);

#endif // GCL_PARSER_H
