#ifndef GCL_TYPES_H
#define GCL_TYPES_H

#include "ast.h"  // for GclType, GclTypeCategory
#include <stddef.h>

// ============================================================
// GCL Type System Helpers
// ============================================================

// ── Type creation ─────────────────────────────────────────
GclType *type_primitive(const char *name, int is_unsigned);
GclType *type_pointer(GclType *base);
GclType *type_array(GclType *base, int size);
GclType *type_func(GclType *ret, GclAstList *params);
GclType *type_struct(const char *name);
GclType *type_enum(const char *name);
GclType *type_union(const char *name);
GclType *type_alias(GclType *original, const char *alias_name);
GclType *type_clone(GclType *src);
void     type_free(GclType *type);

// ── Type name from keyword token ──────────────────────────
GclType *type_from_token(GclTokenType tok);
GclType *type_unsigned_from_token(GclTokenType tok);

// ── Type comparison / query ───────────────────────────────
int type_eq(GclType *a, GclType *b);          // structural equality
int type_compatible(GclType *src, GclType *dst); // can src assign to dst?
int type_is_integer(GclType *type);
int type_is_float(GclType *type);
int type_is_arithmetic(GclType *type);        // int or float
int type_is_pointer(GclType *type);
int type_is_array(GclType *type);
int type_is_string(GclType *type);            // char[] or char*
int type_is_void(GclType *type);
int type_is_bool(GclType *type);
int type_is_struct(GclType *type);

// ── Type to string ────────────────────────────────────────
const char *type_to_string(GclType *type);    // human-readable
char       *type_to_c_string(GclType *type);  // C type string (caller frees)

// ── Size queries (approximate, for sizeof) ────────────────
size_t type_size(GclType *type);              // approximate byte size

// ── Common types (singletons, do not free) ────────────────
GclType *type_int(void);
GclType *type_unsigned_int(void);
GclType *type_char(void);
GclType *type_short(void);
GclType *type_long(void);
GclType *type_long_long(void);
GclType *type_float(void);
GclType *type_double(void);
GclType *type_long_double(void);
GclType *type_void(void);
GclType *type_bool(void);
GclType *type_int8(void);
GclType *type_int16(void);
GclType *type_int32(void);
GclType *type_int64(void);
GclType *type_int128(void);
GclType *type_uint8(void);
GclType *type_uint16(void);
GclType *type_uint32(void);
GclType *type_uint64(void);
GclType *type_unsigned_float(void);
GclType *type_unsigned_double(void);
GclType *type_char_ptr(void);                 // char*
GclType *type_size_t(void);

// ── Init / cleanup ────────────────────────────────────────
void types_init(void);
void types_cleanup(void);

#endif // GCL_TYPES_H
