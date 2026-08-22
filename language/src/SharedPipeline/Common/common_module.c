#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcl_common.h"
#include "gcl_token.h"

/* === PART 1: Memory Arena === */
static GclArenaBlock *arena_new_block(size_t min_size) {
    size_t cap = GCL_ARENA_BLOCK_SIZE;
    if (min_size > cap) cap = min_size;
    GclArenaBlock *b = (GclArenaBlock *)malloc(sizeof(GclArenaBlock) + cap);
    if (!b) {
        fprintf(stderr, "gcl: fatal: arena OOM (%zu bytes)\n", cap);
        exit(1);
    }
    b->next = NULL;
    b->used = 0;
    b->capacity = cap;
    return b;
}

void gcl_arena_init(GclArena *arena) {
    arena->head = arena_new_block(GCL_ARENA_BLOCK_SIZE);
    arena->current = arena->head;
    arena->total_allocated = 0;
}

void *gcl_arena_alloc(GclArena *arena, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;
    GclArenaBlock *cur = arena->current;
    if (cur->used + aligned > cur->capacity) {
        GclArenaBlock *nb = arena_new_block(aligned);
        cur->next = nb;
        arena->current = nb;
        cur = nb;
    }
    void *ptr = cur->data + cur->used;
    cur->used += aligned;
    arena->total_allocated += aligned;
    return ptr;
}

char *gcl_arena_strdup(GclArena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *dst = (char *)gcl_arena_alloc(arena, len + 1);
    memcpy(dst, str, len + 1);
    return dst;
}

char *gcl_arena_strndup(GclArena *arena, const char *str, size_t len) {
    if (!str) return NULL;
    char *dst = (char *)gcl_arena_alloc(arena, len + 1);
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

void gcl_arena_free(GclArena *arena) {
    GclArenaBlock *b = arena->head;
    while (b) {
        GclArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    arena->head = NULL;
    arena->current = NULL;
    arena->total_allocated = 0;
}

/* === PART 2: Safe Memory Utils === */
void *gcl_safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return ptr;
}

void *gcl_safe_calloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory (%zu * %zu bytes)\n", count, size);
        exit(1);
    }
    return ptr;
}

void *gcl_safe_realloc(void *ptr, size_t size) {
    void *newptr = realloc(ptr, size);
    if (!newptr && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory on realloc (%zu bytes)\n", size);
        exit(1);
    }
    return newptr;
}

void gcl_safe_free(void *ptr) {
    free(ptr);
}

/* === PART 3: String Interning === */
static uint32_t fnv1a(const char *str, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)str[i];
        h *= 16777619u;
    }
    return h;
}

void gcl_intern_init(GclStringIntern *intern, GclArena *arena) {
    memset(intern->buckets, 0, sizeof(intern->buckets));
    intern->arena = arena;
}

const char *gcl_intern(GclStringIntern *intern, const char *str, size_t len) {
    uint32_t hash = fnv1a(str, len);
    uint32_t idx = hash % GCL_INTERN_BUCKETS;
    GclInternEntry *e = intern->buckets[idx];
    while (e) {
        if (e->hash == hash && e->len == len && memcmp(e->str, str, len) == 0) {
            return e->str;
        }
        e = e->next;
    }
    char *dup = gcl_arena_strndup(intern->arena, str, len);
    GclInternEntry *entry = (GclInternEntry *)gcl_arena_alloc(intern->arena, sizeof(GclInternEntry));
    entry->str = dup;
    entry->len = len;
    entry->hash = hash;
    entry->next = intern->buckets[idx];
    intern->buckets[idx] = entry;
    return dup;
}

const char *gcl_intern_cstr(GclStringIntern *intern, const char *str) {
    if (!str) return NULL;
    return gcl_intern(intern, str, strlen(str));
}

void gcl_intern_free(GclStringIntern *intern) {
    memset(intern->buckets, 0, sizeof(intern->buckets));
    intern->arena = NULL;
}

/* === PART 4: Token Names === */
const char *gcl_token_kind_name(GclTokenKind kind) {
    switch (kind) {
    case TOK_INT_LIT:       return "INT_LIT";
    case TOK_FLOAT_LIT:     return "FLOAT_LIT";
    case TOK_STRING_LIT:    return "STRING_LIT";
    case TOK_CHAR_LIT:      return "CHAR_LIT";
    case TOK_IDENT:         return "IDENT";
    case TOK_KW_INT:        return "int";
    case TOK_KW_INT8:       return "int8";
    case TOK_KW_INT16:      return "int16";
    case TOK_KW_INT32:      return "int32";
    case TOK_KW_INT64:      return "int64";
    case TOK_KW_INT128:     return "int128";
    case TOK_KW_UINT8:      return "uint8";
    case TOK_KW_UINT16:     return "uint16";
    case TOK_KW_UINT32:     return "uint32";
    case TOK_KW_UINT64:     return "uint64";
    case TOK_KW_UINT128:    return "uint128";
    case TOK_KW_FLOAT:      return "float";
    case TOK_KW_FLOAT16:    return "float16";
    case TOK_KW_FLOAT32:    return "float32";
    case TOK_KW_FLOAT64:    return "float64";
    case TOK_KW_FLOAT128:   return "float128";
    case TOK_KW_DOUBLE:     return "double";
    case TOK_KW_LONG:       return "long";
    case TOK_KW_SHORT:      return "short";
    case TOK_KW_UNSIGNED:   return "unsigned";
    case TOK_KW_CHAR:       return "char";
    case TOK_KW_GCCHAR:     return "gcChar";
    case TOK_KW_BOOL:       return "bool";
    case TOK_KW_VOID:       return "void";
    case TOK_KW_NULL:       return "null";
    case TOK_KW_CONST:      return "const";
    case TOK_KW_INLINE:     return "inline";
    case TOK_KW_GLOBAL:     return "global";
    case TOK_KW_PUBLIC:     return "public";
    case TOK_KW_PRIVATE:    return "private";
    case TOK_KW_IF:         return "if";
    case TOK_KW_ELSE:       return "else";
    case TOK_KW_ELIF:       return "elif";
    case TOK_KW_FOR:        return "for";
    case TOK_KW_WHILE:      return "while";
    case TOK_KW_DO:         return "do";
    case TOK_KW_SWITCH:     return "switch";
    case TOK_KW_CASE:       return "case";
    case TOK_KW_DEFAULT:    return "default";
    case TOK_KW_BREAK:      return "break";
    case TOK_KW_CONTINUE:   return "continue";
    case TOK_KW_RETURN:     return "return";
    case TOK_KW_STRUCT:     return "struct";
    case TOK_KW_ENUM:       return "enum";
    case TOK_KW_TYPEDEF:    return "typedef";
    case TOK_KW_CLASS:      return "class";
    case TOK_KW_TUPLE:      return "tuple";
    case TOK_KW_DICT:       return "dict";
    case TOK_KW_SIZEOF:     return "sizeof";
    case TOK_KW_MALLOC:     return "malloc";
    case TOK_KW_FREE:       return "free";
    case TOK_KW_GCMALLOC:   return "gcMalloc";
    case TOK_KW_PRINTF:     return "printf";
    case TOK_KW_SCANF:      return "scanf";
    case TOK_KW_EXTERN:     return "extern";
    case TOK_PP_INCLUDE:    return "#include";
    case TOK_PP_EXTERN:     return "#extern";
    case TOK_PP_REGISTER:   return "#register";
    case TOK_PP_DEFINE:     return "#define";
    case TOK_PP_UNDEF:      return "#undef";
    case TOK_PP_IFDEF:      return "#ifdef";
    case TOK_PP_IFNDEF:     return "#ifndef";
    case TOK_PP_IF:         return "#if";
    case TOK_PP_ELIF_PP:    return "#elif";
    case TOK_PP_ELSE:       return "#else";
    case TOK_PP_ENDIF:      return "#endif";
    case TOK_PP_WARNING:    return "#warning";
    case TOK_PP_ERROR:      return "#error";
    case TOK_PP_DEBUG:      return "#debug";
    case TOK_PLUS:          return "+";
    case TOK_MINUS:         return "-";
    case TOK_STAR:          return "*";
    case TOK_SLASH:         return "/";
    case TOK_PERCENT:       return "%";
    case TOK_PLUSPLUS:      return "++";
    case TOK_MINUSMINUS:   return "--";
    case TOK_PLUS_EQ:      return "+=";
    case TOK_MINUS_EQ:     return "-=";
    case TOK_STAR_EQ:      return "*=";
    case TOK_SLASH_EQ:     return "/=";
    case TOK_EQ:            return "=";
    case TOK_EQEQ:         return "==";
    case TOK_BANGEQ:       return "!=";
    case TOK_LT:            return "<";
    case TOK_GT:            return ">";
    case TOK_LTEQ:          return "<=";
    case TOK_GTEQ:          return ">=";
    case TOK_AMP:           return "&";
    case TOK_PIPE:          return "|";
    case TOK_CARET:         return "^";
    case TOK_TILDE:         return "~";
    case TOK_AMPAMP:        return "&&";
    case TOK_PIPEPIPE:     return "||";
    case TOK_BANG:          return "!";
    case TOK_LSHIFT:        return "<<";
    case TOK_RSHIFT:        return ">>";
    case TOK_ARROW:         return "->";
    case TOK_DOT:           return ".";
    case TOK_LPAREN:        return "(";
    case TOK_RPAREN:        return ")";
    case TOK_LBRACE:        return "{";
    case TOK_RBRACE:        return "}";
    case TOK_LBRACKET:      return "[";
    case TOK_RBRACKET:      return "]";
    case TOK_SEMICOLON:     return ";";
    case TOK_COMMA:         return ",";
    case TOK_COLON:         return ":";
    case TOK_AT:            return "@";
    case TOK_EOF:           return "EOF";
    case TOK_NEWLINE:       return "NEWLINE";
    case TOK_UNKNOWN:       return "UNKNOWN";
    }
    return "???";
}

void gcl_token_init(GclToken *tok, GclTokenKind kind,
                    const char *start, size_t length, int line, int col) {
    tok->kind = kind;
    tok->start = start;
    tok->length = length;
    tok->line = line;
    tok->col = col;
}
