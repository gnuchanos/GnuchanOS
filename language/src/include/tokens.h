#ifndef GCL_TOKENS_H
#define GCL_TOKENS_H

// ============================================================
// GCL Token Types
// ============================================================

typedef enum {
    // ── Special ───────────────────────────────────────────
    TOK_EOF = 0,
    TOK_ERROR,
    TOK_NEWLINE,

    // ── Literals ──────────────────────────────────────────
    TOK_INT_LITERAL,         // 5, -5, 0xFF, 077, 0b1010, 'A', '\n'
    TOK_FLOAT_LITERAL,       // 1.5f, 2.5, 3.5L
    TOK_CHAR_LITERAL,        // 'A', '\n'
    TOK_STRING_LITERAL,      // "hello"
    TOK_IDENTIFIER,          // variable_name, function_name

    // ── Keywords ──────────────────────────────────────────
    TOK_INT,                 // int
    TOK_CHAR,                // char
    TOK_SHORT,               // short
    TOK_LONG,                // long
    TOK_FLOAT,               // float
    TOK_DOUBLE,              // double
    TOK_VOID,                // void
    TOK_BOOL,                // bool

    // ── Signed/Unsigned ───────────────────────────────────
    TOK_SIGNED,              // signed
    TOK_UNSIGNED,            // unsigned

    // ── Fixed-width ints ──────────────────────────────────
    TOK_INT8,                // int8
    TOK_INT16,               // int16
    TOK_INT32,               // int32
    TOK_INT64,               // int64
    TOK_INT128,              // int128

    TOK_UINT8,               // uint8
    TOK_UINT16,              // uint16
    TOK_UINT32,              // uint32
    TOK_UINT64,              // uint64

    // ── Struct / Enum / Union / Typedef ───────────────────
    TOK_STRUCT,              // struct
    TOK_ENUM,                // enum
    TOK_UNION,               // union
    TOK_TYPEDEF,             // typedef

    // ── Control Flow ──────────────────────────────────────
    TOK_IF,                  // if
    TOK_ELSE,                // else
    TOK_FOR,                 // for
    TOK_WHILE,               // while
    TOK_DO,                  // do
    TOK_SWITCH,              // switch
    TOK_CASE,                // case
    TOK_DEFAULT,             // default
    TOK_BREAK,               // break
    TOK_RETURN,              // return
    TOK_CONTINUE,            // continue
    TOK_GOTO,                // goto

    // ── Operators: Arithmetic ─────────────────────────────
    TOK_PLUS,                // +
    TOK_MINUS,               // -
    TOK_STAR,                // *
    TOK_SLASH,               // /
    TOK_PERCENT,             // %

    // ── Operators: Bitwise ────────────────────────────────
    TOK_AMPERSAND,           // &
    TOK_PIPE,                // |
    TOK_CARET,               // ^
    TOK_TILDE,               // ~
    TOK_LSHIFT,              // <<
    TOK_RSHIFT,              // >>

    // ── Operators: Logical ────────────────────────────────
    TOK_AND,                 // &&
    TOK_OR,                  // ||
    TOK_BANG,                // !

    // ── Operators: Comparison ─────────────────────────────
    TOK_GT,                  // >
    TOK_LT,                  // <
    TOK_GE,                  // >=
    TOK_LE,                  // <=
    TOK_EQ,                  // ==
    TOK_NE,                  // !=

    // ── Operators: Assignment ─────────────────────────────
    TOK_ASSIGN,              // =
    TOK_PLUS_EQ,             // +=
    TOK_MINUS_EQ,            // -=
    TOK_STAR_EQ,             // *=
    TOK_SLASH_EQ,            // /=
    TOK_PERCENT_EQ,          // %=
    TOK_AMP_EQ,              // &=
    TOK_PIPE_EQ,             // |=
    TOK_CARET_EQ,            // ^=
    TOK_LSHIFT_EQ,           // <<=
    TOK_RSHIFT_EQ,           // >>=

    // ── Operators: Increment/Decrement ────────────────────
    TOK_INC,                 // ++
    TOK_DEC,                 // --

    // ── Operators: Other ──────────────────────────────────
    TOK_DOT,                 // .
    TOK_ARROW,               // ->
    TOK_QUESTION,            // ?
    TOK_COLON,               // :
    TOK_SIZEOF,              // sizeof

    // ── Delimiters ────────────────────────────────────────
    TOK_LBRACE,              // {
    TOK_RBRACE,              // }
    TOK_LPAREN,              // (
    TOK_RPAREN,              // )
    TOK_LBRACKET,            // [
    TOK_RBRACKET,            // ]
    TOK_SEMICOLON,           // ;
    TOK_COMMA,               // ,
    TOK_ELLIPSIS,            // ...

    // ── Preprocessor ──────────────────────────────────────
    TOK_PREP_LIB,            // #lib
    TOK_PREP_INCLUDE,        // #include
    TOK_PREP_EXTERN,         // #extern
    TOK_PREP_DEFINE,         // #define
    TOK_PREP_UNDEF,          // #undef
    TOK_PREP_IFDEF,          // #ifdef
    TOK_PREP_IFNDEF,         // #ifndef
    TOK_PREP_IF,             // #if
    TOK_PREP_ELIF,           // #elif
    TOK_PREP_ELSE,           // #else
    TOK_PREP_ENDIF,          // #endif
    TOK_PREP_ERROR,          // #error
    TOK_PREP_PRAGMA,         // #pragma
    TOK_PREP_LINE,           // #line

    // ── GCL Comment Tokens (stripped by lexer, but tracked) ──
    TOK_GCL_COMMENT,         // # ...
    TOK_GCL_COMMENT_BLOCK,   // #| ... |#
    TOK_GCL_COMMENT_CPP,     // #// ...

    // ── Built-in ──────────────────────────────────────────
    TOK_MALLOC,              // malloc
    TOK_CALLOC,              // calloc
    TOK_REALLOC,             // realloc
    TOK_FREE,                // free
    TOK_STRLEN,              // strlen
    TOK_CONST,               // const
    TOK_SIZEOF_BUILTIN,      // sizeof (keyword)

    // ── Boolean ───────────────────────────────────────────
    TOK_TRUE,                // true
    TOK_FALSE,               // false

    TOK_COUNT
} GclTokenType;

// ── Token Structure ───────────────────────────────────────
typedef struct {
    GclTokenType type;
    int          line;
    int          col;
    const char  *lexeme;      // pointer into source
    int          length;      // length of lexeme

    // ── Literal values (parsed) ───────────────────────────
    union {
        long long    int_val;
        double       float_val;
        char         char_val;
        // string_val is the lexeme pointer for strings
    } value;
} GclToken;

// ── Token name lookup ─────────────────────────────────────
const char *token_type_name(GclTokenType type);
const char *token_type_spelling(GclTokenType type); // human-readable

// ── Keyword lookup ────────────────────────────────────────
GclTokenType lookup_keyword(const char *word, int length);

#endif // GCL_TOKENS_H
