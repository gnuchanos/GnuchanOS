#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include "tokens.h"
#include <stddef.h>

// ============================================================
// GCL Lexer — Tokenizer
// ============================================================

// ── Lexer State ───────────────────────────────────────────
typedef struct {
    const char *source;          // full source text (owned by caller)
    const char *current;         // current position in source
    const char *line_start;      // start of current line (for col calculation)
    int         line;            // current line number (1-based)
    int         col;             // current column number (1-based)
    int         had_error;       // error flag
} GclLexer;

// ── Lexer API ─────────────────────────────────────────────
void      lexer_init(GclLexer *lexer, const char *source);
GclToken  lexer_next_token(GclLexer *lexer);
void      lexer_tokenize_all(GclLexer *lexer, GclToken *tokens, int max_tokens, int *count);

// ── Token name helper ─────────────────────────────────────
const char *token_name(GclTokenType type);

#endif // GCL_LEXER_H
