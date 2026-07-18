#ifndef GCL_TYPE_H
#define GCL_TYPE_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================= */
/*  GCL Type System — Gnuchan C-Like Type Definitions                */
/* ================================================================= */

/* Forward declare AstNode for ast_to_type */
struct AstNode;

typedef enum {
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LONG_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LONG_DOUBLE,
    /* ----------------------------------------------------------- */
    /*  Yeni eklenen tipler — signed integers                       */
    /* ----------------------------------------------------------- */
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,
    TYPE_INT128,
    /* ----------------------------------------------------------- */
    /*  Yeni eklenen tipler — unsigned / standard                   */
    /* ----------------------------------------------------------- */
    TYPE_UCHAR,          /* unsigned char */
    TYPE_USHORT,         /* unsigned short */
    TYPE_UINT,           /* unsigned int */
    TYPE_ULONG,          /* unsigned long */
    TYPE_ULONG_LONG,     /* unsigned long long */
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_UNSIGNED_FLOAT,
    TYPE_UNSIGNED_DOUBLE,
    TYPE_BOOL,
    TYPE_SIZE_T,
    TYPE_SSIZE_T,
    TYPE_INTPTR_T,
    TYPE_UINTPTR_T,
    /* ----------------------------------------------------------- */
    /*  Compound tipler (mevcut)                                    */
    /* ----------------------------------------------------------- */
    TYPE_POINTER,      /* T* */
    TYPE_ARRAY,        /* T[n] */
    TYPE_FUNCTION,     /* T (params) → ret */
    TYPE_STRUCT,       /* struct { ... } */
} TypeKind;

typedef struct GclType {
    TypeKind kind;
    size_t   size;       /* byte cinsinden toplam boyut */
    size_t   align;      /* byte cinsinden hizalama */
    int      is_signed;  /* signed / unsigned */
    union {
        struct { struct GclType *base; } pointer;        /* TYPE_POINTER */
        struct { struct GclType *base; size_t count; } array;  /* TYPE_ARRAY */
        struct { struct GclType **params; int pcount; struct GclType *ret; } func; /* TYPE_FUNCTION */
        struct { struct GclType **members; const char **names; int mcount; } strct; /* TYPE_STRUCT */
    } data;
} GclType;

/* ----------------------------------------------------------------- */
/*  Built-in type singleton erişimleri (mevcut)                       */
/* ----------------------------------------------------------------- */
GclType *type_void(void);
GclType *type_char(void);
GclType *type_short(void);
GclType *type_int(void);
GclType *type_long(void);
GclType *type_long_long(void);
GclType *type_float(void);
GclType *type_double(void);
GclType *type_long_double(void);

/* ----------------------------------------------------------------- */
/*  Yeni singleton erişimleri — signed integers                       */
/* ----------------------------------------------------------------- */
GclType *type_int8(void);
GclType *type_int16(void);
GclType *type_int32(void);
GclType *type_int64(void);
GclType *type_int128(void);

/* ----------------------------------------------------------------- */
/*  Yeni singleton erişimleri — unsigned / standard                   */
/* ----------------------------------------------------------------- */
GclType *type_uchar(void);
GclType *type_ushort(void);
GclType *type_uint(void);
GclType *type_ulong(void);
GclType *type_ulong_long(void);
GclType *type_uint8(void);
GclType *type_uint16(void);
GclType *type_uint32(void);
GclType *type_uint64(void);
GclType *type_unsigned_float(void);
GclType *type_unsigned_double(void);
GclType *type_bool(void);
GclType *type_size_t(void);
GclType *type_ssize_t(void);
GclType *type_intptr_t(void);
GclType *type_uintptr_t(void);

/* ----------------------------------------------------------------- */
/*  Compound type oluşturucular                                       */
/* ----------------------------------------------------------------- */
GclType *type_pointer(GclType *base);
GclType *type_array(GclType *base, size_t count);
GclType *type_function(GclType **params, int pcount, GclType *ret);
GclType *type_struct(GclType **members, const char **names, int mcount);

/* ----------------------------------------------------------------- */
/*  Sorgulamalar                                                      */
/* ----------------------------------------------------------------- */
const char *type_name(GclType *t);         /* "int", "char*", "int[16]" */
const char *type_c_name(GclType *t);       /* C type name for codegen */
int         type_equal(GclType *a, GclType *b); /* derin karşılaştırma */
int         type_is_integer(GclType *t);
int         type_is_float(GclType *t);
int         type_is_unsigned(GclType *t);  /* is_signed == 0 */
int         type_is_numeric(GclType *t);
int         type_is_arithmetic(GclType *t); /* numeric + pointer */
int         type_width(GclType *t);         /* byte cinsinden genişlik */

/* ----------------------------------------------------------------- */
/*  AST → GclType dönüşümü                                            */
/* ----------------------------------------------------------------- */
GclType *ast_to_type(struct AstNode *node);

/* ----------------------------------------------------------------- */
/*  Temel tip boyutları (platform)                                    */
/* ----------------------------------------------------------------- */
size_t type_sizeof(GclType *t);
size_t type_alignof(GclType *t);

#endif /* GCL_TYPE_H */