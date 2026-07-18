#ifndef GCL_SEMANTIC_H
#define GCL_SEMANTIC_H

#include "../parser/ast.h"
#include "../type/type.h"

/* ================================================================= */
/*  GCL Semantic Analysis                                             */
/* ================================================================= */

/* ------ Semantic Error/Warning Codes ------ */
typedef enum {
    SEM_ERR_NONE              = 0,
    SEM_ERR_TYPE_MISMATCH     = 1,
    SEM_ERR_UNDECLARED_VAR    = 2,
    SEM_ERR_UNDECLARED_FUNC   = 3,
    SEM_ERR_REDECLARED_VAR    = 4,
    SEM_ERR_RETURN_TYPE       = 5,
    SEM_ERR_ARG_COUNT         = 6,
    SEM_ERR_ARG_TYPE          = 7,
    SEM_ERR_NULL_POINTER      = 8,
    SEM_ERR_ARRAY_BOUNDS      = 9,
    SEM_ERR_OVERFLOW          = 10,
    SEM_ERR_UNINIT_VAR        = 11,
    SEM_ERR_LIFETIME          = 12,
    SEM_ERR_DIV_BY_ZERO       = 13,
    SEM_ERR_INVALID_OPERATION = 14,
    SEM_ERR_CONST_ASSIGN      = 15,
    SEM_WARN_UNUSED_VAR       = 16,
    SEM_WARN_UNUSED_PARAM     = 17,
} SemErrorCode;

typedef struct {
    SemErrorCode code;
    int          line;
    int          col;
    const char  *msg;
} SemError;

/* ------ Symbol Table Entry ------ */
typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_PARAMETER,
} SymKind;

typedef struct SymEntry {
    SymKind       kind;
    const char   *name;
    GclType      *type;
    int           depth;        /* scope depth */
    int           declared_line;
    int           is_initialized;
    int           is_used;
    struct SymEntry *next;      /* hash chain */
} SymEntry;

typedef struct {
    SymEntry **buckets;
    int        capacity;
    int        count;
} SymTable;

/* ------ Scope Stack ------ */
typedef struct ScopeNode {
    int                 depth;
    SymTable           *table;
    struct ScopeNode   *parent;
} ScopeNode;

/* ------ Semantic State (internal) ------ */
typedef struct {
    SemError *errors;
    int       error_count;
    int       error_cap;
    int       warnings;
    ScopeNode *scope;
    GclType   *current_func_ret_type;  /* icinde bulunulan fonksiyonun donus tipi */
} SemanticState;

/* ------ Semantic Result (public API return) ------ */
typedef struct {
    int errors;
    int warnings;
} SemanticResult;

/* ------ API ------ */
SemanticState *semantic_new(void);
SemanticResult semantic_analyze(AstNode *ast);
void           semantic_free(SemanticState *s);

/* Error reporting */
void sem_error(SemanticState *s, SemErrorCode code, int line, int col, const char *fmt, ...);
void sem_warning(SemanticState *s, SemErrorCode code, int line, int col, const char *fmt, ...);

/* Scope management */
void scope_enter(SemanticState *s);
void scope_exit(SemanticState *s);
int  scope_add_var(SemanticState *s, const char *name, GclType *type, int line, SymKind kind);
SymEntry *scope_lookup(SemanticState *s, const char *name);

/* Type checking helper */
int type_check_assignment(SemanticState *s, GclType *dst, GclType *src, int line, int col);
int type_check_comparison(SemanticState *s, GclType *a, GclType *b, int line, int col);

#endif /* GCL_SEMANTIC_H */
