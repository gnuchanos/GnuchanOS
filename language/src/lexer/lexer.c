#include "lexer.h"
#include "tokens.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

// ============================================================
// GCL Lexer Implementation
// ============================================================

// ── Keyword table ──────────────────────────────────────────
typedef struct {
    const char  *word;
    int          len;
    GclTokenType type;
} GclKeyword;

static const GclKeyword keywords[] = {
    // types
    {"int",       3, TOK_INT},
    {"char",      4, TOK_CHAR},
    {"short",     5, TOK_SHORT},
    {"long",      4, TOK_LONG},
    {"float",     5, TOK_FLOAT},
    {"double",    6, TOK_DOUBLE},
    {"void",      4, TOK_VOID},
    {"bool",      4, TOK_BOOL},
    {"signed",    6, TOK_SIGNED},
    {"unsigned",  8, TOK_UNSIGNED},

    // fixed-width signed
    {"int8",      4, TOK_INT8},
    {"int16",     5, TOK_INT16},
    {"int32",     5, TOK_INT32},
    {"int64",     5, TOK_INT64},
    {"int128",    6, TOK_INT128},

    // fixed-width unsigned
    {"uint8",     5, TOK_UINT8},
    {"uint16",    6, TOK_UINT16},
    {"uint32",    6, TOK_UINT32},
    {"uint64",    6, TOK_UINT64},

    // struct/enum/union/typedef
    {"struct",    6, TOK_STRUCT},
    {"enum",      4, TOK_ENUM},
    {"union",     5, TOK_UNION},
    {"typedef",   7, TOK_TYPEDEF},

    // control flow
    {"if",        2, TOK_IF},
    {"else",      4, TOK_ELSE},
    {"for",       3, TOK_FOR},
    {"while",     5, TOK_WHILE},
    {"do",        2, TOK_DO},
    {"switch",    6, TOK_SWITCH},
    {"case",      4, TOK_CASE},
    {"default",   7, TOK_DEFAULT},
    {"break",     5, TOK_BREAK},
    {"return",    6, TOK_RETURN},
    {"continue",  8, TOK_CONTINUE},
    {"goto",      4, TOK_GOTO},

    // operator keywords
    {"sizeof",    6, TOK_SIZEOF_BUILTIN},

    // boolean
    {"true",      4, TOK_TRUE},
    {"false",     5, TOK_FALSE},

    // built-in functions
    {"malloc",    6, TOK_MALLOC},
    {"calloc",    6, TOK_CALLOC},
    {"realloc",   7, TOK_REALLOC},
    {"free",      4, TOK_FREE},
    {"strlen",    6, TOK_STRLEN},

    {NULL, 0, TOK_EOF}
};

// ── Helpers ────────────────────────────────────────────────
static int lexer_peek(GclLexer *lexer) {
    return (unsigned char)*lexer->current;
}

static int lexer_peek_next(GclLexer *lexer) {
    if (*lexer->current == '\0') return '\0';
    return (unsigned char)*(lexer->current + 1);
}

static char lexer_advance(GclLexer *lexer) {
    char c = *lexer->current;
    if (c != '\0') {
        lexer->current++;
        lexer->col++;
    }
    return c;
}

static int lexer_match(GclLexer *lexer, char expected) {
    if (lexer_peek(lexer) == expected) {
        lexer_advance(lexer);
        return 1;
    }
    return 0;
}

static void lexer_skip_whitespace(GclLexer *lexer) {
    for (;;) {
        char c = lexer_peek(lexer);
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
            lexer_advance(lexer);
            break;
        case '\n':
            lexer->line++;
            lexer->col = 1;
            lexer->line_start = lexer->current + 1;
            lexer_advance(lexer);
            break;
        default:
            return;
        }
    }
}

static GclToken make_token(GclLexer *lexer, GclTokenType type) {
    GclToken tok;
    tok.type   = type;
    tok.line   = lexer->line;
    tok.col    = lexer->col - 1; // approximate
    tok.lexeme = lexer->line_start;
    tok.length = (int)(lexer->current - lexer->line_start);
    tok.value.int_val = 0;
    return tok;
}

static GclToken make_token_val(GclLexer *lexer, GclTokenType type,
                                const char *start, int len) {
    GclToken tok;
    tok.type   = type;
    tok.line   = lexer->line;
    tok.col    = (int)(start - lexer->line_start) + 1;
    tok.lexeme = start;
    tok.length = len;
    tok.value.int_val = 0;
    return tok;
}

// ── Keyword lookup ────────────────────────────────────────
GclTokenType lookup_keyword(const char *word, int length) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if (keywords[i].len == length &&
            strncmp(keywords[i].word, word, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENTIFIER;
}

// ── Number parsing ────────────────────────────────────────
static int is_bin_digit(char c)  { return c == '0' || c == '1'; }
static int is_oct_digit(char c)  { return c >= '0' && c <= '7'; }
static int is_hex_digit(char c)  { return isdigit((unsigned char)c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static GclToken lexer_number(GclLexer *lexer) {
    const char *start = lexer->current - 1; // already consumed first digit
    int base = 10;
    int is_float = 0;

    // check for 0x, 0b, 0 prefix
    if (lexer_peek(lexer) == 'x' || lexer_peek(lexer) == 'X') {
        if (start[0] == '0') {
            lexer_advance(lexer);
            base = 16;
        }
    } else if (lexer_peek(lexer) == 'b' || lexer_peek(lexer) == 'B') {
        if (start[0] == '0') {
            lexer_advance(lexer);
            base = 2;
        }
    } else if (*start == '0' && isdigit((unsigned char)lexer_peek(lexer))) {
        // could be octal
        base = 8;
    }

    // consume digits
    for (;;) {
        char c = lexer_peek(lexer);
        if (c == '_') { lexer_advance(lexer); continue; } // digit separator
        if (base == 16 && is_hex_digit(c)) { lexer_advance(lexer); continue; }
        if (base == 2  && is_bin_digit(c)) { lexer_advance(lexer); continue; }
        if (base == 8  && is_oct_digit(c)) { lexer_advance(lexer); continue; }
        if (base == 10 && isdigit((unsigned char)c)) { lexer_advance(lexer); continue; }
        break;
    }

    // check for float suffix
    if (base == 10) {
        if (lexer_peek(lexer) == '.' && isdigit((unsigned char)lexer_peek_next(lexer))) {
            is_float = 1;
            lexer_advance(lexer); // consume '.'
            while (isdigit((unsigned char)lexer_peek(lexer))) lexer_advance(lexer);
        }
        // exponent
        if (lexer_peek(lexer) == 'e' || lexer_peek(lexer) == 'E') {
            is_float = 1;
            lexer_advance(lexer);
            if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-') lexer_advance(lexer);
            while (isdigit((unsigned char)lexer_peek(lexer))) lexer_advance(lexer);
        }
        // float suffix
        if (lexer_peek(lexer) == 'f' || lexer_peek(lexer) == 'F') {
            is_float = 1;
            lexer_advance(lexer);
        } else if (lexer_peek(lexer) == 'l' || lexer_peek(lexer) == 'L') {
            lexer_advance(lexer);
            /* long double suffix consumed — float type remains */
        }
    }

    // hex float? (0x...p+...)
    if (base == 16 && (lexer_peek(lexer) == 'p' || lexer_peek(lexer) == 'P')) {
        is_float = 1;
        lexer_advance(lexer);
        if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-') lexer_advance(lexer);
        while (isdigit((unsigned char)lexer_peek(lexer))) lexer_advance(lexer);
    }

    // unsigned suffix
    if (lexer_peek(lexer) == 'u' || lexer_peek(lexer) == 'U') {
        lexer_advance(lexer);
        if (lexer_peek(lexer) == 'l' || lexer_peek(lexer) == 'L') lexer_advance(lexer);
        if (lexer_peek(lexer) == 'l' || lexer_peek(lexer) == 'L') lexer_advance(lexer);
    } else if (lexer_peek(lexer) == 'l' || lexer_peek(lexer) == 'L') {
        lexer_advance(lexer);
        if (lexer_peek(lexer) == 'l' || lexer_peek(lexer) == 'L') lexer_advance(lexer);
        if (lexer_peek(lexer) == 'u' || lexer_peek(lexer) == 'U') lexer_advance(lexer);
    }

    int len = (int)(lexer->current - start);
    GclToken tok = make_token_val(lexer, is_float ? TOK_FLOAT_LITERAL : TOK_INT_LITERAL, start, len);

    // parse value
    if (is_float) {
        char *end;
        tok.value.float_val = strtod(start, &end);
    } else {
        tok.value.int_val = strtoll(start, NULL, 0);
    }
    return tok;
}

// ── Character literal ─────────────────────────────────────
static GclToken lexer_char(GclLexer *lexer) {
    const char *start = lexer->current - 1; // past '
    int code = 0;

    if (lexer_peek(lexer) == '\\') {
        lexer_advance(lexer);
        switch (lexer_peek(lexer)) {
        case 'n':  code = '\n'; break;
        case 't':  code = '\t'; break;
        case 'r':  code = '\r'; break;
        case '0':  code = '\0'; break;
        case '\\': code = '\\'; break;
        case '\'': code = '\''; break;
        case '\"': code = '\"'; break;
        case 'a':  code = '\a'; break;
        case 'b':  code = '\b'; break;
        case 'f':  code = '\f'; break;
        case 'v':  code = '\v'; break;
        case 'x': {
            lexer_advance(lexer);
            code = 0;
            while (is_hex_digit(lexer_peek(lexer))) {
                int d;
                char c2 = lexer_peek(lexer);
                if (c2 >= '0' && c2 <= '9') d = c2 - '0';
                else if (c2 >= 'a' && c2 <= 'f') d = c2 - 'a' + 10;
                else d = c2 - 'A' + 10;
                code = code * 16 + d;
                lexer_advance(lexer);
            }
            break;
        }
        default:
            // just take the char
            code = (unsigned char)lexer_peek(lexer);
            break;
        }
        lexer_advance(lexer);
    } else {
        code = (unsigned char)lexer_advance(lexer);
    }

    if (lexer_peek(lexer) == '\'') {
        lexer_advance(lexer);
    }

    int len = (int)(lexer->current - start);
    GclToken tok = make_token_val(lexer, TOK_CHAR_LITERAL, start, len);
    tok.value.char_val = (char)code;
    return tok;
}

// ── String literal ────────────────────────────────────────
static GclToken lexer_string(GclLexer *lexer) {
    const char *start = lexer->current - 1; // past opening "

    while (lexer_peek(lexer) != '"' && lexer_peek(lexer) != '\0' && lexer_peek(lexer) != '\n') {
        if (lexer_peek(lexer) == '\\') {
            lexer_advance(lexer); // skip escape
            if (lexer_peek(lexer) != '\0') lexer_advance(lexer);
        } else {
            lexer_advance(lexer);
        }
    }

    if (lexer_peek(lexer) == '"') {
        lexer_advance(lexer);
    }

    int len = (int)(lexer->current - start);
    return make_token_val(lexer, TOK_STRING_LITERAL, start, len);
}

// ── Identifier ────────────────────────────────────────────
static GclToken lexer_identifier(GclLexer *lexer) {
    const char *start = lexer->current - 1; // already consumed first char

    while (isalnum((unsigned char)lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
        lexer_advance(lexer);
    }

    int len = (int)(lexer->current - start);
    GclTokenType type = lookup_keyword(start, len);
    GclToken tok = make_token_val(lexer, type, start, len);

    if (type == TOK_TRUE) {
        tok.value.int_val = 1;
    } else if (type == TOK_FALSE) {
        tok.value.int_val = 0;
    }

    return tok;
}

// ── C-style block comment ─────────────────────────────────
static void lexer_skip_block_comment(GclLexer *lexer) {
    while (lexer_peek(lexer) != '\0') {
        if (lexer_peek(lexer) == '*' && lexer_peek_next(lexer) == '/') {
            lexer_advance(lexer); // *
            lexer_advance(lexer); // /
            return;
        }
        if (lexer_peek(lexer) == '\n') {
            lexer->line++;
            lexer->col = 1;
            lexer->line_start = lexer->current + 1;
        }
        lexer_advance(lexer);
    }
}

// ── GCL comment block #| ... |# ───────────────────────────
static void lexer_skip_gcl_block_comment(GclLexer *lexer) {
    while (lexer_peek(lexer) != '\0') {
        if (lexer_peek(lexer) == '|' && lexer_peek_next(lexer) == '#') {
            lexer_advance(lexer); // |
            lexer_advance(lexer); // #
            return;
        }
        if (lexer_peek(lexer) == '\n') {
            lexer->line++;
            lexer->col = 1;
            lexer->line_start = lexer->current + 1;
        }
        lexer_advance(lexer);
    }
}

// ── Preprocessor directive ────────────────────────────────
static GclToken lexer_preprocessor(GclLexer *lexer) {
    const char *start = lexer->current - 1; // past #

    // Check for GCL comments first: #|, #//
    if (lexer_peek(lexer) == '|') {
        lexer_advance(lexer); // |
        lexer_skip_gcl_block_comment(lexer);
        int len = (int)(lexer->current - start);
        return make_token_val(lexer, TOK_GCL_COMMENT_BLOCK, start, len);
    }

    if (lexer_peek(lexer) == '/' && lexer_peek_next(lexer) == '/') {
        lexer_advance(lexer); // /
        lexer_advance(lexer); // /
        // GCL-style C++ comment
        while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0') {
            lexer_advance(lexer);
        }
        int len = (int)(lexer->current - start);
        return make_token_val(lexer, TOK_GCL_COMMENT_CPP, start, len);
    }

    // Read the directive keyword
    while (isalpha((unsigned char)lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    int kw_len = (int)(lexer->current - start - 1);
    const char *kw = start + 1;

    GclTokenType type = TOK_GCL_COMMENT; // default: # comment line

    if (kw_len > 0) {
        if (strncmp(kw, "lib", kw_len) == 0 && kw_len == 3) {
            type = TOK_PREP_LIB;
            goto prep_read_rest;
        } else if (strncmp(kw, "include", kw_len) == 0 && kw_len == 7) {
            type = TOK_PREP_INCLUDE;
            goto prep_read_rest;
        } else if (strncmp(kw, "extern", kw_len) == 0 && kw_len == 6) {
            type = TOK_PREP_EXTERN;
            goto prep_read_rest;
        } else if (strncmp(kw, "define", kw_len) == 0 && kw_len == 6) {
            type = TOK_PREP_DEFINE;
            goto prep_read_rest;
        } else if (strncmp(kw, "undef", kw_len) == 0 && kw_len == 5) {
            type = TOK_PREP_UNDEF;
            goto prep_read_rest;
        } else if (strncmp(kw, "ifdef", kw_len) == 0 && kw_len == 5) {
            type = TOK_PREP_IFDEF;
            goto prep_read_rest;
        } else if (strncmp(kw, "ifndef", kw_len) == 0 && kw_len == 6) {
            type = TOK_PREP_IFNDEF;
            goto prep_read_rest;
        } else if (strncmp(kw, "elif", kw_len) == 0 && kw_len == 4) {
            type = TOK_PREP_ELIF;
            goto prep_read_rest;
        } else if (strncmp(kw, "else", kw_len) == 0 && kw_len == 4) {
            type = TOK_PREP_ELSE;
            goto prep_read_rest;
        } else if (strncmp(kw, "endif", kw_len) == 0 && kw_len == 5) {
            type = TOK_PREP_ENDIF;
            goto prep_read_rest;
        } else if (strncmp(kw, "if", kw_len) == 0 && kw_len == 2) {
            type = TOK_PREP_IF;
            goto prep_read_rest;
        } else if (strncmp(kw, "error", kw_len) == 0 && kw_len == 5) {
            type = TOK_PREP_ERROR;
            goto prep_read_rest;
        } else if (strncmp(kw, "pragma", kw_len) == 0 && kw_len == 6) {
            type = TOK_PREP_PRAGMA;
            goto prep_read_rest;
        } else if (strncmp(kw, "line", kw_len) == 0 && kw_len == 4) {
            type = TOK_PREP_LINE;
            goto prep_read_rest;
        }
    }

    // Plain # comment → consume rest of line
    while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0') {
        lexer_advance(lexer);
    }
    {
        int len = (int)(lexer->current - start);
        return make_token_val(lexer, TOK_GCL_COMMENT, start, len);
    }

prep_read_rest:
    // Skip whitespace, then read the rest of the line (the value/argument)
    while (lexer_peek(lexer) == ' ' || lexer_peek(lexer) == '\t') {
        lexer_advance(lexer);
    }
    // For #if / #elif / conditionals, read condition as rest of line
    while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0') {
        lexer_advance(lexer);
    }
    {
        int len = (int)(lexer->current - start);
        return make_token_val(lexer, type, start, len);
    }
}

// ── Main token dispatch ───────────────────────────────────
GclToken lexer_next_token(GclLexer *lexer) {
    lexer_skip_whitespace(lexer);

    // remember start of this token
    const char *tok_start = lexer->current;
    lexer->line_start = tok_start; // rough but gets col right
    lexer->col = 1;

    char c = lexer_advance(lexer);

    if (c == '\0') return make_token(lexer, TOK_EOF);

    // ── Preprocessor / Comment ────────────────────────────
    if (c == '#') {
        return lexer_preprocessor(lexer);
    }

    // ── Numbers ───────────────────────────────────────────
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)lexer_peek(lexer)))) {
        return lexer_number(lexer);
    }

    // ── Identifiers / Keywords ────────────────────────────
    if (isalpha((unsigned char)c) || c == '_') {
        return lexer_identifier(lexer);
    }

    // ── Characters & Strings ──────────────────────────────
    if (c == '\'') return lexer_char(lexer);
    if (c == '"')  return lexer_string(lexer);

    // ── Operators & Delimiters ────────────────────────────
    switch (c) {
    case '+':
        if (lexer_match(lexer, '+')) return make_token(lexer, TOK_INC);
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_PLUS_EQ);
        return make_token(lexer, TOK_PLUS);

    case '-':
        if (lexer_match(lexer, '-')) return make_token(lexer, TOK_DEC);
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_MINUS_EQ);
        if (lexer_match(lexer, '>')) return make_token(lexer, TOK_ARROW);
        return make_token(lexer, TOK_MINUS);

    case '*':
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_STAR_EQ);
        return make_token(lexer, TOK_STAR);

    case '/':
        if (lexer_match(lexer, '/')) {
            // C++ style comment
            while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0') lexer_advance(lexer);
            return lexer_next_token(lexer); // skip comment, return next token
        }
        if (lexer_match(lexer, '*')) {
            lexer_skip_block_comment(lexer);
            return lexer_next_token(lexer); // skip comment
        }
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_SLASH_EQ);
        return make_token(lexer, TOK_SLASH);

    case '%':
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_PERCENT_EQ);
        return make_token(lexer, TOK_PERCENT);

    case '&':
        if (lexer_match(lexer, '&')) return make_token(lexer, TOK_AND);
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_AMP_EQ);
        return make_token(lexer, TOK_AMPERSAND);

    case '|':
        if (lexer_match(lexer, '|')) return make_token(lexer, TOK_OR);
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_PIPE_EQ);
        return make_token(lexer, TOK_PIPE);

    case '^':
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_CARET_EQ);
        return make_token(lexer, TOK_CARET);

    case '~':
        return make_token(lexer, TOK_TILDE);

    case '!':
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_NE);
        return make_token(lexer, TOK_BANG);

    case '=':
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_EQ);
        return make_token(lexer, TOK_ASSIGN);

    case '<':
        if (lexer_match(lexer, '<')) {
            if (lexer_match(lexer, '=')) return make_token(lexer, TOK_LSHIFT_EQ);
            return make_token(lexer, TOK_LSHIFT);
        }
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_LE);
        return make_token(lexer, TOK_LT);

    case '>':
        if (lexer_match(lexer, '>')) {
            if (lexer_match(lexer, '=')) return make_token(lexer, TOK_RSHIFT_EQ);
            return make_token(lexer, TOK_RSHIFT);
        }
        if (lexer_match(lexer, '=')) return make_token(lexer, TOK_GE);
        return make_token(lexer, TOK_GT);

    case '?': return make_token(lexer, TOK_QUESTION);
    case ':': return make_token(lexer, TOK_COLON);
    case '.': 
        if (lexer_match(lexer, '.') && lexer_match(lexer, '.')) return make_token(lexer, TOK_ELLIPSIS);
        return make_token(lexer, TOK_DOT);

    case '{': return make_token(lexer, TOK_LBRACE);
    case '}': return make_token(lexer, TOK_RBRACE);
    case '(': return make_token(lexer, TOK_LPAREN);
    case ')': return make_token(lexer, TOK_RPAREN);
    case '[': return make_token(lexer, TOK_LBRACKET);
    case ']': return make_token(lexer, TOK_RBRACKET);
    case ';': return make_token(lexer, TOK_SEMICOLON);
    case ',': return make_token(lexer, TOK_COMMA);

    default:
        // Unexpected character
        return make_token(lexer, TOK_ERROR);
    }
}

// ── Lexer init ────────────────────────────────────────────
void lexer_init(GclLexer *lexer, const char *source) {
    lexer->source     = source;
    lexer->current    = source;
    lexer->line_start = source;
    lexer->line       = 1;
    lexer->col        = 1;
    lexer->had_error  = 0;
}

// ── Tokenize all ──────────────────────────────────────────
void lexer_tokenize_all(GclLexer *lexer, GclToken *tokens, int max_tokens, int *count) {
    *count = 0;
    for (;;) {
        GclToken tok = lexer_next_token(lexer);
        if (*count < max_tokens) {
            tokens[*count] = tok;
        }
        (*count)++;
        if (tok.type == TOK_EOF) break;
    }
}

// ── Token name helper ─────────────────────────────────────
const char *token_name(GclTokenType type) {
    static const char *names[] = {
        [TOK_EOF]              = "EOF",
        [TOK_ERROR]            = "ERROR",
        [TOK_NEWLINE]          = "NEWLINE",
        [TOK_INT_LITERAL]      = "INT_LITERAL",
        [TOK_FLOAT_LITERAL]    = "FLOAT_LITERAL",
        [TOK_CHAR_LITERAL]     = "CHAR_LITERAL",
        [TOK_STRING_LITERAL]   = "STRING_LITERAL",
        [TOK_IDENTIFIER]       = "IDENTIFIER",
        [TOK_INT]              = "int",
        [TOK_CHAR]             = "char",
        [TOK_SHORT]            = "short",
        [TOK_LONG]             = "long",
        [TOK_FLOAT]            = "float",
        [TOK_DOUBLE]           = "double",
        [TOK_VOID]             = "void",
        [TOK_BOOL]             = "bool",
        [TOK_SIGNED]           = "signed",
        [TOK_UNSIGNED]         = "unsigned",
        [TOK_INT8]             = "int8",
        [TOK_INT16]            = "int16",
        [TOK_INT32]            = "int32",
        [TOK_INT64]            = "int64",
        [TOK_INT128]           = "int128",
        [TOK_UINT8]            = "uint8",
        [TOK_UINT16]           = "uint16",
        [TOK_UINT32]           = "uint32",
        [TOK_UINT64]           = "uint64",
        [TOK_STRUCT]           = "struct",
        [TOK_ENUM]             = "enum",
        [TOK_UNION]            = "union",
        [TOK_TYPEDEF]          = "typedef",
        [TOK_IF]               = "if",
        [TOK_ELSE]             = "else",
        [TOK_FOR]              = "for",
        [TOK_WHILE]            = "while",
        [TOK_DO]               = "do",
        [TOK_SWITCH]           = "switch",
        [TOK_CASE]             = "case",
        [TOK_DEFAULT]          = "default",
        [TOK_BREAK]            = "break",
        [TOK_RETURN]           = "return",
        [TOK_CONTINUE]         = "continue",
        [TOK_GOTO]             = "goto",
        [TOK_SIZEOF_BUILTIN]   = "sizeof",
        [TOK_TRUE]             = "true",
        [TOK_FALSE]            = "false",
        [TOK_MALLOC]           = "malloc",
        [TOK_CALLOC]           = "calloc",
        [TOK_REALLOC]          = "realloc",
        [TOK_FREE]             = "free",
        [TOK_STRLEN]           = "strlen",
        [TOK_PLUS]             = "+",
        [TOK_MINUS]            = "-",
        [TOK_STAR]             = "*",
        [TOK_SLASH]            = "/",
        [TOK_PERCENT]          = "%",
        [TOK_AMPERSAND]        = "&",
        [TOK_PIPE]             = "|",
        [TOK_CARET]            = "^",
        [TOK_TILDE]            = "~",
        [TOK_LSHIFT]           = "<<",
        [TOK_RSHIFT]           = ">>",
        [TOK_AND]              = "&&",
        [TOK_OR]               = "||",
        [TOK_BANG]             = "!",
        [TOK_GT]               = ">",
        [TOK_LT]               = "<",
        [TOK_GE]               = ">=",
        [TOK_LE]               = "<=",
        [TOK_EQ]               = "==",
        [TOK_NE]               = "!=",
        [TOK_ASSIGN]           = "=",
        [TOK_PLUS_EQ]          = "+=",
        [TOK_MINUS_EQ]         = "-=",
        [TOK_STAR_EQ]          = "*=",
        [TOK_SLASH_EQ]         = "/=",
        [TOK_PERCENT_EQ]       = "%=",
        [TOK_AMP_EQ]           = "&=",
        [TOK_PIPE_EQ]          = "|=",
        [TOK_CARET_EQ]         = "^=",
        [TOK_LSHIFT_EQ]        = "<<=",
        [TOK_RSHIFT_EQ]        = ">>=",
        [TOK_INC]              = "++",
        [TOK_DEC]              = "--",
        [TOK_DOT]              = ".",
        [TOK_ARROW]            = "->",
        [TOK_QUESTION]         = "?",
        [TOK_COLON]            = ":",
        [TOK_LBRACE]           = "{",
        [TOK_RBRACE]           = "}",
        [TOK_LPAREN]           = "(",
        [TOK_RPAREN]           = ")",
        [TOK_LBRACKET]         = "[",
        [TOK_RBRACKET]         = "]",
        [TOK_SEMICOLON]        = ";",
        [TOK_COMMA]            = ",",
        [TOK_ELLIPSIS]         = "...",
        [TOK_PREP_LIB]         = "#lib",
        [TOK_PREP_INCLUDE]     = "#include",
        [TOK_PREP_EXTERN]      = "#extern",
        [TOK_PREP_DEFINE]      = "#define",
        [TOK_PREP_UNDEF]       = "#undef",
        [TOK_PREP_IFDEF]       = "#ifdef",
        [TOK_PREP_IFNDEF]      = "#ifndef",
        [TOK_PREP_IF]          = "#if",
        [TOK_PREP_ELIF]        = "#elif",
        [TOK_PREP_ELSE]        = "#else",
        [TOK_PREP_ENDIF]       = "#endif",
        [TOK_PREP_ERROR]       = "#error",
        [TOK_PREP_PRAGMA]      = "#pragma",
        [TOK_PREP_LINE]        = "#line",
        [TOK_GCL_COMMENT]      = "#comment",
        [TOK_GCL_COMMENT_BLOCK]= "#|...|#",
        [TOK_GCL_COMMENT_CPP]  = "#//",
    };
    if (type >= 0 && type < TOK_COUNT) return names[type];
    return "UNKNOWN";
}

const char *token_type_name(GclTokenType type) { return token_name(type); }
const char *token_type_spelling(GclTokenType type) { return token_name(type); }
