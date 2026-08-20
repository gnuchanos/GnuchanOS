/*
 * gcl_parser.h — GCL Parser interface
 */

#ifndef GCL_PARSER_H
#define GCL_PARSER_H

#include "../Common/gcl_common.h"
#include "../Common/gcl_token.h"
#include "../Lexer/gcl_lexer.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

#define GCL_MACRO_MAX 128
#define GCL_ALIAS_MAX 128
#define GCL_ENUM_MAX  256

typedef struct {
    char name[128];
    char value[256];
} GclMacro;

typedef struct {
    char name[128];
    char type[128];
    int  ptr;
} GclTypeAlias;

typedef struct {
    GclLexer         lex;
    GclToken         current;
    GclArena        *arena;
    GclStringIntern *intern;
    GclDiagBag      *diag;
    const char      *filepath;
    struct GclAstNode *program;     /* program root (for #include merge) */
    GclMacro         macros[GCL_MACRO_MAX];
    int              macro_count;
    /* Part 6: typedef aliases */
    GclTypeAlias     type_aliases[GCL_ALIAS_MAX];
    int              type_alias_count;
    char             op_alias_name[GCL_ALIAS_MAX][128];
    char             op_alias_op[GCL_ALIAS_MAX][16];
    int              op_alias_count;
    /* Part 7: enum constants */
    char             enum_names[GCL_ENUM_MAX][128];
    int64_t          enum_values[GCL_ENUM_MAX];
    int              enum_count;
} GclParser;

void        gcl_parser_init(GclParser *p, const char *source, GclArena *arena, GclStringIntern *intern, GclDiagBag *diag, const char *filepath);
GclAstNode *gcl_parser_parse(GclParser *p);

#endif /* GCL_PARSER_H */
