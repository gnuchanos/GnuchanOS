#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len + 1);
    return d;
}

// ============================================================
// GCL Type System Implementation
// ============================================================

static GclType *alloc_type(void) {
    return (GclType *)calloc(1, sizeof(GclType));
}

GclType *type_primitive(const char *name, int is_unsigned) {
    GclType *t = alloc_type();
    t->category = TYPE_PRIMITIVE;
    t->name = name ? strdup_safe(name) : NULL;
    t->is_unsigned = is_unsigned;
    return t;
}

GclType *type_pointer(GclType *base) {
    GclType *t = alloc_type();
    t->category = TYPE_POINTER;
    t->base_type = base;
    t->pointer_depth = (base ? base->pointer_depth + 1 : 1);
    return t;
}

GclType *type_array(GclType *base, int size) {
    GclType *t = alloc_type();
    t->category = TYPE_ARRAY;
    t->base_type = base;
    t->array_size = size;
    return t;
}

GclType *type_func(GclType *ret, GclAstList *params) {
    GclType *t = alloc_type();
    t->category = TYPE_FUNCTION;
    t->return_type = ret;
    t->param_types = params;
    return t;
}

GclType *type_struct(const char *name) {
    GclType *t = alloc_type();
    t->category = TYPE_STRUCT;
    t->name = name ? strdup_safe(name) : NULL;
    return t;
}

GclType *type_enum(const char *name) {
    GclType *t = alloc_type();
    t->category = TYPE_ENUM;
    t->name = name ? strdup_safe(name) : NULL;
    return t;
}

GclType *type_union(const char *name) {
    GclType *t = alloc_type();
    t->category = TYPE_UNION;
    t->name = name ? strdup_safe(name) : NULL;
    return t;
}

GclType *type_alias(GclType *original, const char *alias_name) {
    GclType *t = alloc_type();
    t->category = TYPE_TYPEDEF_ALIAS;
    t->base_type = original;
    t->name = alias_name ? strdup_safe(alias_name) : NULL;
    return t;
}

GclType *type_clone(GclType *src) {
    if (!src) return NULL;
    GclType *t = alloc_type();
    memcpy(t, src, sizeof(GclType));
    if (src->name) t->name = strdup_safe(src->name);
    if (src->base_type) t->base_type = type_clone(src->base_type);
    if (src->return_type) t->return_type = type_clone(src->return_type);
    // Don't clone param_types (shallow copy is fine for now)
    return t;
}

void type_free(GclType *type) {
    if (!type) return;
    free((void *)type->name);
    if (type->base_type) type_free(type->base_type);
    if (type->return_type) type_free(type->return_type);
    free(type);
}

// ── Singleton types ───────────────────────────────────────

static GclType *cache_int = NULL;
static GclType *cache_uint = NULL;
static GclType *cache_char = NULL;
static GclType *cache_short = NULL;
static GclType *cache_long = NULL;
static GclType *cache_llong = NULL;
static GclType *cache_float = NULL;
static GclType *cache_double = NULL;
static GclType *cache_ldouble = NULL;
static GclType *cache_void = NULL;
static GclType *cache_bool = NULL;
static GclType *cache_int8 = NULL;
static GclType *cache_int16 = NULL;
static GclType *cache_int32 = NULL;
static GclType *cache_int64 = NULL;
static GclType *cache_int128 = NULL;
static GclType *cache_uint8 = NULL;
static GclType *cache_uint16 = NULL;
static GclType *cache_uint32 = NULL;
static GclType *cache_uint64 = NULL;
static GclType *cache_ufloat = NULL;
static GclType *cache_udouble = NULL;
static GclType *cache_char_ptr = NULL;
static GclType *cache_size_t = NULL;

void types_init(void) {
    cache_int      = type_primitive("int", 0);
    cache_uint     = type_primitive("int", 1);
    cache_char     = type_primitive("char", 0);
    cache_short    = type_primitive("short", 0);
    cache_long     = type_primitive("long", 0);
    cache_llong    = type_primitive("long long", 0);
    cache_float    = type_primitive("float", 0);
    cache_double   = type_primitive("double", 0);
    cache_ldouble  = type_primitive("long double", 0);
    cache_void     = type_primitive("void", 0);
    cache_bool     = type_primitive("bool", 0);
    cache_int8     = type_primitive("int8", 0);
    cache_int16    = type_primitive("int16", 0);
    cache_int32    = type_primitive("int32", 0);
    cache_int64    = type_primitive("int64", 0);
    cache_int128   = type_primitive("int128", 0);
    cache_uint8    = type_primitive("uint8", 0);
    cache_uint16   = type_primitive("uint16", 0);
    cache_uint32   = type_primitive("uint32", 0);
    cache_uint64   = type_primitive("uint64", 0);
    cache_ufloat   = type_primitive("float", 1);
    cache_udouble  = type_primitive("double", 1);
    cache_char_ptr = type_pointer(type_char());
    cache_size_t   = type_primitive("size_t", 0);
}

void types_cleanup(void) {
    // Singletons — intentionally leaked for simplicity
    // In production, use refcounting or arena
}

#define SINGLETON_GET(fn_name, cache_var, init_fn) \
    GclType *fn_name(void) { \
        if (!cache_var) { types_init(); } \
        return cache_var; \
    }

SINGLETON_GET(type_int, cache_int, type_primitive("int", 0))
SINGLETON_GET(type_unsigned_int, cache_uint, type_primitive("int", 1))
SINGLETON_GET(type_char, cache_char, type_primitive("char", 0))
SINGLETON_GET(type_short, cache_short, type_primitive("short", 0))
SINGLETON_GET(type_long, cache_long, type_primitive("long", 0))
SINGLETON_GET(type_long_long, cache_llong, type_primitive("long long", 0))
SINGLETON_GET(type_float, cache_float, type_primitive("float", 0))
SINGLETON_GET(type_double, cache_double, type_primitive("double", 0))
SINGLETON_GET(type_long_double, cache_ldouble, type_primitive("long double", 0))
SINGLETON_GET(type_void, cache_void, type_primitive("void", 0))
SINGLETON_GET(type_bool, cache_bool, type_primitive("bool", 0))
SINGLETON_GET(type_int8, cache_int8, type_primitive("int8", 0))
SINGLETON_GET(type_int16, cache_int16, type_primitive("int16", 0))
SINGLETON_GET(type_int32, cache_int32, type_primitive("int32", 0))
SINGLETON_GET(type_int64, cache_int64, type_primitive("int64", 0))
SINGLETON_GET(type_int128, cache_int128, type_primitive("int128", 0))
SINGLETON_GET(type_uint8, cache_uint8, type_primitive("uint8", 0))
SINGLETON_GET(type_uint16, cache_uint16, type_primitive("uint16", 0))
SINGLETON_GET(type_uint32, cache_uint32, type_primitive("uint32", 0))
SINGLETON_GET(type_uint64, cache_uint64, type_primitive("uint64", 0))
SINGLETON_GET(type_unsigned_float, cache_ufloat, type_primitive("float", 1))
SINGLETON_GET(type_unsigned_double, cache_udouble, type_primitive("double", 1))
SINGLETON_GET(type_char_ptr, cache_char_ptr, type_pointer(type_char()))
SINGLETON_GET(type_size_t, cache_size_t, type_primitive("size_t", 0))

#undef SINGLETON_GET

// ── Type queries ──────────────────────────────────────────
int type_is_integer(GclType *type) {
    if (!type) return 0;
    if (type->category != TYPE_PRIMITIVE) return 0;
    const char *n = type->name;
    if (!n) return 0;
    if (strcmp(n, "int") == 0 || strcmp(n, "char") == 0 ||
        strcmp(n, "short") == 0 || strcmp(n, "long") == 0 ||
        strcmp(n, "long long") == 0) return 1;
    if (strncmp(n, "int", 3) == 0 || strncmp(n, "uint", 4) == 0) return 1;
    if (strcmp(n, "bool") == 0) return 1;
    return 0;
}

int type_is_float(GclType *type) {
    if (!type || type->category != TYPE_PRIMITIVE) return 0;
    const char *n = type->name;
    if (!n) return 0;
    return strcmp(n, "float") == 0 || strcmp(n, "double") == 0 ||
           strcmp(n, "long double") == 0;
}

int type_is_arithmetic(GclType *type) {
    return type_is_integer(type) || type_is_float(type);
}

int type_is_pointer(GclType *type) {
    return type && type->category == TYPE_POINTER;
}

int type_is_array(GclType *type) {
    return type && type->category == TYPE_ARRAY;
}

int type_is_string(GclType *type) {
    return type_is_array(type) || type_is_pointer(type);
}

int type_is_void(GclType *type) {
    return type && type->category == TYPE_PRIMITIVE &&
           type->name && strcmp(type->name, "void") == 0;
}

int type_is_bool(GclType *type) {
    return type && type->category == TYPE_PRIMITIVE &&
           type->name && strcmp(type->name, "bool") == 0;
}

int type_is_struct(GclType *type) {
    return type && type->category == TYPE_STRUCT;
}

int type_eq(GclType *a, GclType *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->category != b->category) return 0;
    if (a->is_unsigned != b->is_unsigned) return 0;
    if (a->name && b->name) return strcmp(a->name, b->name) == 0;
    if (a->name || b->name) return 0;
    return 1;
}

int type_compatible(GclType *src, GclType *dst) {
    if (type_eq(src, dst)) return 1;
    if (type_is_arithmetic(src) && type_is_arithmetic(dst)) return 1;
    if (type_is_pointer(src) && type_is_pointer(dst)) return 1;
    return 0;
}

size_t type_size(GclType *type) {
    if (!type) return 0;
    switch (type->category) {
    case TYPE_PRIMITIVE: {
        const char *n = type->name;
        if (!n) return 4;
        if (strcmp(n, "char") == 0) return 1;
        if (strcmp(n, "short") == 0) return 2;
        if (strcmp(n, "int") == 0) return 4;
        if (strcmp(n, "long") == 0) return 8;
        if (strcmp(n, "long long") == 0) return 8;
        if (strcmp(n, "float") == 0) return 4;
        if (strcmp(n, "double") == 0) return 8;
        if (strcmp(n, "long double") == 0) return 16;
        if (strcmp(n, "void") == 0) return 0;
        if (strcmp(n, "bool") == 0) return 1;
        if (strncmp(n, "int8", 4) == 0) return 1;
        if (strncmp(n, "int16", 5) == 0) return 2;
        if (strncmp(n, "int32", 5) == 0) return 4;
        if (strncmp(n, "int64", 5) == 0) return 8;
        if (strcmp(n, "int128") == 0) return 16;
        if (strncmp(n, "uint8", 5) == 0) return 1;
        if (strncmp(n, "uint16", 6) == 0) return 2;
        if (strncmp(n, "uint32", 6) == 0) return 4;
        if (strncmp(n, "uint64", 6) == 0) return 8;
        return 4;
    }
    case TYPE_POINTER: return 8;
    case TYPE_ARRAY: return type_size(type->base_type) * (type->array_size > 0 ? type->array_size : 1);
    default: return 0;
    }
}

const char *type_to_string(GclType *type) {
    if (!type) return "unknown";
    switch (type->category) {
    case TYPE_PRIMITIVE: return type->name ? type->name : "?";
    case TYPE_POINTER: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s*", type_to_string(type->base_type));
        return buf;
    }
    case TYPE_ARRAY: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s[]", type_to_string(type->base_type));
        return buf;
    }
    case TYPE_STRUCT: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "struct %s", type->name ? type->name : "");
        return buf;
    }
    case TYPE_ENUM: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "enum %s", type->name ? type->name : "");
        return buf;
    }
    default: return "?";
    }
}
