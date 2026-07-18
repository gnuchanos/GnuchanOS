#include "type.h"
#include "../parser/ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------- */
/*  Built-in type singleton'lari — init via function to avoid -Wmissing-braces */
/* ----------------------------------------------------------------- */

static GclType _type_void;
static GclType _type_char;
static GclType _type_short;
static GclType _type_int;
static GclType _type_long;
static GclType _type_llong;
static GclType _type_float;
static GclType _type_double;
static GclType _type_ldouble;

/* Yeni signed integer tipleri */
static GclType _type_int8;
static GclType _type_int16;
static GclType _type_int32;
static GclType _type_int64;
static GclType _type_int128;

/* Yeni unsigned / standard tipleri */
static GclType _type_uchar;
static GclType _type_ushort;
static GclType _type_uint;
static GclType _type_ulong;
static GclType _type_ulong_long;
static GclType _type_uint8;
static GclType _type_uint16;
static GclType _type_uint32;
static GclType _type_uint64;
static GclType _type_unsigned_float;
static GclType _type_unsigned_double;
static GclType _type_bool;
static GclType _type_size_t;
static GclType _type_ssize_t;
static GclType _type_intptr_t;
static GclType _type_uintptr_t;

static int _types_initialized = 0;

static void init_types(void) {
    if (_types_initialized) return;
    _types_initialized = 1;

    /* --- mevcut tipler --- */
    memset(&_type_void,   0, sizeof(GclType)); _type_void.kind = TYPE_VOID;
    memset(&_type_char,   0, sizeof(GclType)); _type_char.kind = TYPE_CHAR;   _type_char.size = 1;  _type_char.align = 1; _type_char.is_signed = 1;
    memset(&_type_short,  0, sizeof(GclType)); _type_short.kind = TYPE_SHORT;  _type_short.size = 2; _type_short.align = 2; _type_short.is_signed = 1;
    memset(&_type_int,    0, sizeof(GclType)); _type_int.kind = TYPE_INT;     _type_int.size = 4;  _type_int.align = 4; _type_int.is_signed = 1;
    memset(&_type_long,   0, sizeof(GclType)); _type_long.kind = TYPE_LONG;    _type_long.size = 8; _type_long.align = 8; _type_long.is_signed = 1;
    memset(&_type_llong,  0, sizeof(GclType)); _type_llong.kind = TYPE_LONG_LONG;  _type_llong.size = 8;  _type_llong.align = 8; _type_llong.is_signed = 1;
    memset(&_type_float,  0, sizeof(GclType)); _type_float.kind = TYPE_FLOAT; _type_float.size = 4; _type_float.align = 4; _type_float.is_signed = 1;
    memset(&_type_double, 0, sizeof(GclType)); _type_double.kind = TYPE_DOUBLE; _type_double.size = 8; _type_double.align = 8; _type_double.is_signed = 1;
    memset(&_type_ldouble,0, sizeof(GclType)); _type_ldouble.kind = TYPE_LONG_DOUBLE; _type_ldouble.size = 16; _type_ldouble.align = 16; _type_ldouble.is_signed = 1;

    /* --- signed int8/16/32/64/128 --- */
    memset(&_type_int8,   0, sizeof(GclType)); _type_int8.kind   = TYPE_INT8;   _type_int8.size = 1;  _type_int8.align = 1;  _type_int8.is_signed = 1;
    memset(&_type_int16,  0, sizeof(GclType)); _type_int16.kind  = TYPE_INT16;  _type_int16.size = 2; _type_int16.align = 2; _type_int16.is_signed = 1;
    memset(&_type_int32,  0, sizeof(GclType)); _type_int32.kind  = TYPE_INT32;  _type_int32.size = 4; _type_int32.align = 4; _type_int32.is_signed = 1;
    memset(&_type_int64,  0, sizeof(GclType)); _type_int64.kind  = TYPE_INT64;  _type_int64.size = 8; _type_int64.align = 8; _type_int64.is_signed = 1;
    memset(&_type_int128, 0, sizeof(GclType)); _type_int128.kind = TYPE_INT128; _type_int128.size = 16; _type_int128.align = 16; _type_int128.is_signed = 1;

    /* --- unsigned char/short/int/long/long_long --- */
    memset(&_type_uchar,       0, sizeof(GclType)); _type_uchar.kind      = TYPE_UCHAR;      _type_uchar.size = 1; _type_uchar.align = 1; _type_uchar.is_signed = 0;
    memset(&_type_ushort,      0, sizeof(GclType)); _type_ushort.kind     = TYPE_USHORT;     _type_ushort.size = 2; _type_ushort.align = 2; _type_ushort.is_signed = 0;
    memset(&_type_uint,        0, sizeof(GclType)); _type_uint.kind       = TYPE_UINT;       _type_uint.size = 4; _type_uint.align = 4; _type_uint.is_signed = 0;
    memset(&_type_ulong,       0, sizeof(GclType)); _type_ulong.kind      = TYPE_ULONG;      _type_ulong.size = 8; _type_ulong.align = 8; _type_ulong.is_signed = 0;
    memset(&_type_ulong_long,  0, sizeof(GclType)); _type_ulong_long.kind = TYPE_ULONG_LONG; _type_ulong_long.size = 8; _type_ulong_long.align = 8; _type_ulong_long.is_signed = 0;

    /* --- uint8/16/32/64 --- */
    memset(&_type_uint8,  0, sizeof(GclType)); _type_uint8.kind  = TYPE_UINT8;  _type_uint8.size = 1; _type_uint8.align = 1; _type_uint8.is_signed = 0;
    memset(&_type_uint16, 0, sizeof(GclType)); _type_uint16.kind = TYPE_UINT16; _type_uint16.size = 2; _type_uint16.align = 2; _type_uint16.is_signed = 0;
    memset(&_type_uint32, 0, sizeof(GclType)); _type_uint32.kind = TYPE_UINT32; _type_uint32.size = 4; _type_uint32.align = 4; _type_uint32.is_signed = 0;
    memset(&_type_uint64, 0, sizeof(GclType)); _type_uint64.kind = TYPE_UINT64; _type_uint64.size = 8; _type_uint64.align = 8; _type_uint64.is_signed = 0;

    /* --- unsigned float/double --- */
    memset(&_type_unsigned_float,  0, sizeof(GclType)); _type_unsigned_float.kind  = TYPE_UNSIGNED_FLOAT;  _type_unsigned_float.size = 4; _type_unsigned_float.align = 4; _type_unsigned_float.is_signed = 0;
    memset(&_type_unsigned_double, 0, sizeof(GclType)); _type_unsigned_double.kind = TYPE_UNSIGNED_DOUBLE; _type_unsigned_double.size = 8; _type_unsigned_double.align = 8; _type_unsigned_double.is_signed = 0;

    /* --- bool --- */
    memset(&_type_bool, 0, sizeof(GclType)); _type_bool.kind = TYPE_BOOL; _type_bool.size = 1; _type_bool.align = 1; _type_bool.is_signed = 0;

    /* --- size_t, ssize_t, intptr_t, uintptr_t --- */
    memset(&_type_size_t,    0, sizeof(GclType)); _type_size_t.kind    = TYPE_SIZE_T;    _type_size_t.size = 8; _type_size_t.align = 8; _type_size_t.is_signed = 0;
    memset(&_type_ssize_t,   0, sizeof(GclType)); _type_ssize_t.kind   = TYPE_SSIZE_T;   _type_ssize_t.size = 8; _type_ssize_t.align = 8; _type_ssize_t.is_signed = 0;
    memset(&_type_intptr_t,  0, sizeof(GclType)); _type_intptr_t.kind  = TYPE_INTPTR_T;  _type_intptr_t.size = 8; _type_intptr_t.align = 8; _type_intptr_t.is_signed = 0;
    memset(&_type_uintptr_t, 0, sizeof(GclType)); _type_uintptr_t.kind = TYPE_UINTPTR_T; _type_uintptr_t.size = 8; _type_uintptr_t.align = 8; _type_uintptr_t.is_signed = 0;
}

/* ---- mevcut accessor'lar ---- */
GclType *type_void(void)        { init_types(); return &_type_void; }
GclType *type_char(void)        { init_types(); return &_type_char; }
GclType *type_short(void)       { init_types(); return &_type_short; }
GclType *type_int(void)         { init_types(); return &_type_int; }
GclType *type_long(void)        { init_types(); return &_type_long; }
GclType *type_long_long(void)   { init_types(); return &_type_llong; }
GclType *type_float(void)       { init_types(); return &_type_float; }
GclType *type_double(void)      { init_types(); return &_type_double; }
GclType *type_long_double(void) { init_types(); return &_type_ldouble; }

/* ---- yeni signed integer accessor'lar ---- */
GclType *type_int8(void)        { init_types(); return &_type_int8; }
GclType *type_int16(void)       { init_types(); return &_type_int16; }
GclType *type_int32(void)       { init_types(); return &_type_int32; }
GclType *type_int64(void)       { init_types(); return &_type_int64; }
GclType *type_int128(void)      { init_types(); return &_type_int128; }

/* ---- yeni unsigned / standard accessor'lar ---- */
GclType *type_uchar(void)            { init_types(); return &_type_uchar; }
GclType *type_ushort(void)           { init_types(); return &_type_ushort; }
GclType *type_uint(void)             { init_types(); return &_type_uint; }
GclType *type_ulong(void)            { init_types(); return &_type_ulong; }
GclType *type_ulong_long(void)       { init_types(); return &_type_ulong_long; }
GclType *type_uint8(void)            { init_types(); return &_type_uint8; }
GclType *type_uint16(void)           { init_types(); return &_type_uint16; }
GclType *type_uint32(void)           { init_types(); return &_type_uint32; }
GclType *type_uint64(void)           { init_types(); return &_type_uint64; }
GclType *type_unsigned_float(void)   { init_types(); return &_type_unsigned_float; }
GclType *type_unsigned_double(void)  { init_types(); return &_type_unsigned_double; }
GclType *type_bool(void)             { init_types(); return &_type_bool; }
GclType *type_size_t(void)           { init_types(); return &_type_size_t; }
GclType *type_ssize_t(void)          { init_types(); return &_type_ssize_t; }
GclType *type_intptr_t(void)         { init_types(); return &_type_intptr_t; }
GclType *type_uintptr_t(void)        { init_types(); return &_type_uintptr_t; }

/* ----------------------------------------------------------------- */
/*  AST -> GclType                                                   */
/* ----------------------------------------------------------------- */

GclType *ast_to_type(AstNode *node) {
    if (!node || node->type != AST_IDENTIFIER) return type_int();
    const char *name = node->data.id;
    if (!name) return type_int();

    /* mevcut */
    if (strcmp(name, "void") == 0)          return type_void();
    if (strcmp(name, "char") == 0)          return type_char();
    if (strcmp(name, "short") == 0)         return type_short();
    if (strcmp(name, "int") == 0)           return type_int();
    if (strcmp(name, "long") == 0)          return type_long();
    if (strcmp(name, "float") == 0)         return type_float();
    if (strcmp(name, "double") == 0)        return type_double();
    if (strcmp(name, "long long") == 0)     return type_long_long();
    if (strcmp(name, "long double") == 0)   return type_long_double();

    /* yeni signed integer */
    if (strcmp(name, "int8") == 0)          return type_int8();
    if (strcmp(name, "int16") == 0)         return type_int16();
    if (strcmp(name, "int32") == 0)         return type_int32();
    if (strcmp(name, "int64") == 0)         return type_int64();
    if (strcmp(name, "int128") == 0)        return type_int128();

    /* unsigned */
    if (strcmp(name, "unsigned char") == 0)       return type_uchar();
    if (strcmp(name, "unsigned short") == 0)      return type_ushort();
    if (strcmp(name, "unsigned int") == 0)        return type_uint();
    if (strcmp(name, "unsigned long") == 0)       return type_ulong();
    if (strcmp(name, "unsigned long long") == 0)  return type_ulong_long();

    if (strcmp(name, "uint8") == 0)  return type_uint8();
    if (strcmp(name, "uint16") == 0) return type_uint16();
    if (strcmp(name, "uint32") == 0) return type_uint32();
    if (strcmp(name, "uint64") == 0) return type_uint64();

    if (strcmp(name, "unsigned float") == 0)  return type_unsigned_float();
    if (strcmp(name, "unsigned double") == 0) return type_unsigned_double();

    if (strcmp(name, "bool") == 0)     return type_bool();
    if (strcmp(name, "size_t") == 0)   return type_size_t();
    if (strcmp(name, "ssize_t") == 0)  return type_ssize_t();
    if (strcmp(name, "intptr_t") == 0) return type_intptr_t();
    if (strcmp(name, "uintptr_t") == 0) return type_uintptr_t();

    /* true/false -> bool */
    if (strcmp(name, "true") == 0)     return type_bool();
    if (strcmp(name, "false") == 0)    return type_bool();

    return type_int();
}

/* ----------------------------------------------------------------- */
/*  Compound type olusturucular                                       */
/* ----------------------------------------------------------------- */

GclType *type_pointer(GclType *base) {
    GclType *t = (GclType*)calloc(1, sizeof(GclType));
    if (!t) return NULL;
    t->kind = TYPE_POINTER;
    t->size = 8;
    t->align = 8;
    t->data.pointer.base = base;
    return t;
}

GclType *type_array(GclType *base, size_t count) {
    GclType *t = (GclType*)calloc(1, sizeof(GclType));
    if (!t) return NULL;
    t->kind = TYPE_ARRAY;
    t->size = base->size * count;
    t->align = base->align;
    t->data.array.base = base;
    t->data.array.count = count;
    return t;
}

GclType *type_function(GclType **params, int pcount, GclType *ret) {
    GclType *t = (GclType*)calloc(1, sizeof(GclType));
    if (!t) return NULL;
    t->kind = TYPE_FUNCTION;
    t->size = 0;
    t->align = 0;
    t->data.func.params = params;
    t->data.func.pcount = pcount;
    t->data.func.ret = ret;
    return t;
}

GclType *type_struct(GclType **members, const char **names, int mcount) {
    GclType *t = (GclType*)calloc(1, sizeof(GclType));
    if (!t) return NULL;
    t->kind = TYPE_STRUCT;
    t->data.strct.members = members;
    t->data.strct.names = names;
    t->data.strct.mcount = mcount;
    size_t off = 0;
    size_t maxalign = 0;
    for (int i = 0; i < mcount; i++) {
        size_t a = members[i]->align;
        if (a > maxalign) maxalign = a;
        off = (off + a - 1) & ~(a - 1);
        off += members[i]->size;
    }
    off = (off + maxalign - 1) & ~(maxalign - 1);
    t->size = off;
    t->align = maxalign;
    return t;
}

/* ----------------------------------------------------------------- */
/*  Sorgulamalar                                                      */
/* ----------------------------------------------------------------- */

int type_width(GclType *t) {
    if (!t) return 4;
    return (int)t->size;
}

static const char *builtin_name(TypeKind k) {
    switch (k) {
    case TYPE_VOID:   return "void";
    case TYPE_CHAR:   return "char";
    case TYPE_SHORT:  return "short";
    case TYPE_INT:    return "int";
    case TYPE_LONG:   return "long";
    case TYPE_LONG_LONG: return "long long";
    case TYPE_FLOAT:  return "float";
    case TYPE_DOUBLE: return "double";
    case TYPE_LONG_DOUBLE: return "long double";
    /* signed integers */
    case TYPE_INT8:   return "int8";
    case TYPE_INT16:  return "int16";
    case TYPE_INT32:  return "int32";
    case TYPE_INT64:  return "int64";
    case TYPE_INT128: return "int128";
    /* unsigned / standard */
    case TYPE_UCHAR:      return "unsigned char";
    case TYPE_USHORT:     return "unsigned short";
    case TYPE_UINT:       return "unsigned int";
    case TYPE_ULONG:      return "unsigned long";
    case TYPE_ULONG_LONG: return "unsigned long long";
    case TYPE_UINT8:   return "uint8";
    case TYPE_UINT16:  return "uint16";
    case TYPE_UINT32:  return "uint32";
    case TYPE_UINT64:  return "uint64";
    case TYPE_UNSIGNED_FLOAT:  return "unsigned float";
    case TYPE_UNSIGNED_DOUBLE: return "unsigned double";
    case TYPE_BOOL:       return "bool";
    case TYPE_SIZE_T:     return "size_t";
    case TYPE_SSIZE_T:    return "ssize_t";
    case TYPE_INTPTR_T:   return "intptr_t";
    case TYPE_UINTPTR_T:  return "uintptr_t";
    default:          return "?";
    }
}

const char *type_name(GclType *t) {
    if (!t) return "null";
    /* All builtin kinds (including new ones) are <= TYPE_UINTPTR_T,
       which is before TYPE_POINTER in the enum, so this works */
    if (t->kind <= TYPE_UINTPTR_T) return builtin_name(t->kind);
    if (t->kind == TYPE_POINTER) {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s*", type_name(t->data.pointer.base));
        return buf;
    }
    if (t->kind == TYPE_ARRAY) {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s[%zu]", type_name(t->data.array.base), t->data.array.count);
        return buf;
    }
    if (t->kind == TYPE_FUNCTION) return "function";
    if (t->kind == TYPE_STRUCT)   return "struct";
    return "?";
}

const char *type_c_name(GclType *t) {
    if (!t) return "int";
    switch (t->kind) {
    case TYPE_VOID:   return "void";
    case TYPE_CHAR:   return "char";
    case TYPE_SHORT:  return "short";
    case TYPE_INT:    return "int";
    case TYPE_LONG:   return "long";
    case TYPE_LONG_LONG: return "long long";
    case TYPE_FLOAT:  return "float";
    case TYPE_DOUBLE: return "double";
    case TYPE_LONG_DOUBLE: return "long double";
    /* signed integers -> stdint.h types */
    case TYPE_INT8:   return "int8_t";
    case TYPE_INT16:  return "int16_t";
    case TYPE_INT32:  return "int32_t";
    case TYPE_INT64:  return "int64_t";
    case TYPE_INT128: return "__int128";
    /* unsigned -> C unsigned types */
    case TYPE_UCHAR:      return "unsigned char";
    case TYPE_USHORT:     return "unsigned short";
    case TYPE_UINT:       return "unsigned int";
    case TYPE_ULONG:      return "unsigned long";
    case TYPE_ULONG_LONG: return "unsigned long long";
    /* uint -> stdint.h */
    case TYPE_UINT8:   return "uint8_t";
    case TYPE_UINT16:  return "uint16_t";
    case TYPE_UINT32:  return "uint32_t";
    case TYPE_UINT64:  return "uint64_t";
    case TYPE_UNSIGNED_FLOAT:  return "float";
    case TYPE_UNSIGNED_DOUBLE: return "double";
    case TYPE_BOOL:       return "bool";
    case TYPE_SIZE_T:     return "size_t";
    case TYPE_SSIZE_T:    return "ssize_t";
    case TYPE_INTPTR_T:   return "intptr_t";
    case TYPE_UINTPTR_T:  return "uintptr_t";
    case TYPE_POINTER: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s*", type_c_name(t->data.pointer.base));
        return buf;
    }
    default: return "int";
    }
}

int type_equal(GclType *a, GclType *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->kind == TYPE_POINTER)
        return type_equal(a->data.pointer.base, b->data.pointer.base);
    if (a->kind == TYPE_ARRAY)
        return a->data.array.count == b->data.array.count &&
               type_equal(a->data.array.base, b->data.array.base);
    return 1;
}

int type_is_integer(GclType *t) {
    if (!t) return 0;
    TypeKind k = t->kind;
    /* mevcut integer tipleri */
    if (k == TYPE_CHAR || k == TYPE_SHORT || k == TYPE_INT ||
        k == TYPE_LONG || k == TYPE_LONG_LONG)
        return 1;
    /* yeni signed integer tipleri */
    if (k == TYPE_INT8 || k == TYPE_INT16 || k == TYPE_INT32 ||
        k == TYPE_INT64 || k == TYPE_INT128)
        return 1;
    /* yeni unsigned integer tipleri */
    if (k == TYPE_UCHAR || k == TYPE_USHORT || k == TYPE_UINT ||
        k == TYPE_ULONG || k == TYPE_ULONG_LONG)
        return 1;
    if (k == TYPE_UINT8 || k == TYPE_UINT16 || k == TYPE_UINT32 ||
        k == TYPE_UINT64)
        return 1;
    if (k == TYPE_SIZE_T || k == TYPE_SSIZE_T ||
        k == TYPE_INTPTR_T || k == TYPE_UINTPTR_T)
        return 1;
    if (k == TYPE_BOOL)
        return 1;
    return 0;
}

int type_is_float(GclType *t) {
    if (!t) return 0;
    TypeKind k = t->kind;
    if (k == TYPE_FLOAT || k == TYPE_DOUBLE || k == TYPE_LONG_DOUBLE)
        return 1;
    /* unsigned float/double da float sayilir */
    if (k == TYPE_UNSIGNED_FLOAT || k == TYPE_UNSIGNED_DOUBLE)
        return 1;
    return 0;
}

int type_is_unsigned(GclType *t) {
    if (!t) return 0;
    return t->is_signed == 0;
}

int type_is_numeric(GclType *t) {
    return type_is_integer(t) || type_is_float(t);
}

int type_is_arithmetic(GclType *t) {
    return type_is_numeric(t) || (t && t->kind == TYPE_POINTER);
}

size_t type_sizeof(GclType *t) {
    return t ? t->size : 0;
}

size_t type_alignof(GclType *t) {
    return t ? t->align : 0;
}