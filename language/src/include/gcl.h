#ifndef GCL_H
#define GCL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_PATHS 32

/* ── Token Types ──────────────────────────────────────────── */
typedef enum {
    T_EOF, T_ERROR,
    T_INT, T_IDENT, T_NUMBER, T_CHAR, T_SIZEOF,
    T_SEMICOLON,
    T_ASSIGN, T_ADD_ASSIGN, T_SUB_ASSIGN, T_MUL_ASSIGN,
    T_DIV_ASSIGN, T_MOD_ASSIGN, T_AND_ASSIGN, T_OR_ASSIGN,
    T_XOR_ASSIGN, T_LSHIFT_ASSIGN, T_RSHIFT_ASSIGN,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_AMPERSAND, T_PIPE, T_CARET, T_TILDE,
    T_LSHIFT, T_RSHIFT,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_AND, T_OR, T_NOT,
    T_INC, T_DEC,
    T_QUESTION, T_COLON,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE,
    T_LBRACKET, T_RBRACKET, T_COMMA, T_DOT
} TokenType;

/* ── AST Node Types ───────────────────────────────────────── */
typedef enum {
    N_PROGRAM, N_VAR_DECL, N_EXPR_STMT,
    N_INT_LIT, N_CHAR_LIT,
    N_IDENT,
    N_BINARY, N_UNARY, N_POSTFIX,
    N_ASSIGN, N_TERNARY,
    N_CALL, N_CAST, N_SIZEOF
} NodeType;

/* ── Source Location ──────────────────────────────────────── */
typedef struct {
    const char *filename;
    int line, col;
} SourceLoc;

/* ── Debug ────────────────────────────────────────────────── */
typedef enum {
    DBG_NONE = 0,
    DBG_LEXER = 1 << 0,
    DBG_PARSER = 1 << 1,
    DBG_AST = 1 << 2,
    DBG_CODEGEN = 1 << 3,
    DBG_ALL = 0xFF
} DebugFlags;

/* ── Config ───────────────────────────────────────────────── */
typedef struct {
    const char *input_file;
    const char *output_file;
    bool stop_lexer;
    bool stop_parser;
    bool stop_ast;
    bool stop_ir;
    bool do_run;
    DebugFlags debug;
    const char *include_dirs[MAX_PATHS];
    int  num_include_dirs;
    const char *lib_dirs[MAX_PATHS];
    int  num_lib_dirs;
    const char *extend_dirs[MAX_PATHS];
    int  num_extend_dirs;
} GCLConfig;

/* ── CLI ──────────────────────────────────────────────────── */
int parse_cli(int argc, char **argv, GCLConfig *cfg);

#endif
