#ifndef GCL_SEMANTIC_H
#define GCL_SEMANTIC_H

#include "gcl.h"
#include "ast.h"

/* sembol tablosu */
typedef struct Sym {
    char name[256];
    int  type;           /* 0=int, 1=char vb */
    int  scope_level;
    struct Sym *next;
} Sym;

/* scope (bagli liste) */
typedef struct Scope {
    Sym *syms;
    struct Scope *parent;
    int level;
} Scope;

typedef struct {
    Scope *cur;
    int errors;
} SemState;

void sem_init(SemState *s);
void sem_enter_scope(SemState *s);
void sem_exit_scope(SemState *s);
int  sem_add_sym(SemState *s, const char *name, int type, SourceLoc loc);
Sym *sem_lookup(SemState *s, const char *name);
int  sem_analyze(SemState *s, Node *prog, const char *src);

#endif
