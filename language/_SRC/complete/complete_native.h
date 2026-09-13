/*
 * complete_native.h — Native modül API veritabanı (§4).
 *
 * Raylib/Raygui girdileri tools/gen_native_db.py tarafından Modules altındaki
 * C dosyalarından ÜRETİLİR (complete_native_db.c). Math/Stdio/Embed + struct
 * alanları elle tutulur (complete_native.c). Dönüş tipi (ret) zincirleme
 * tamamlama içindir:
 *   Raylib.GetMousePosition() -> Vector2  =>  ".x / .y" önerilir.
 */
#ifndef GCL_COMPLETE_NATIVE_H
#define GCL_COMPLETE_NATIVE_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NF_FUNC  = 1u,
    NF_TYPE  = 2u,
    NF_CONST = 4u,
    NF_ENUM  = 8u
} GclNativeFlags;

typedef struct {
    const char *name;
    const char *ret;       /* dönüş/alan tipi ("void" | "Vector2" | "int" ...) */
    const char *params;    /* "int width, int height, gcChar title" (boş olabilir) */
    const char *category;  /* "shapes2d" | "text" | "type" | "const" ... */
    const char *doc;       /* kısa açıklama (opsiyonel) */
    unsigned    flags;     /* NF_* */
} GclNativeMember;

typedef struct {
    const char *module;
    const GclNativeMember *members;
    int member_count;
} GclNativeModule;

/* Modül tanımı (yoksa NULL). */
const GclNativeModule *gcl_native_module(const char *name);
/* Modül üyesi (yoksa NULL). */
const GclNativeMember *gcl_native_find(const char *module, const char *member);
/* Modül adı mı? */
int gcl_native_is_module(const char *name);
/* Tüm modül adları (NULL ile biten). */
const char *const *gcl_native_module_names(void);

/* ---- Struct alanları (§4.5) ---- */
typedef struct { const char *name; const char *type; } GclNativeField;
typedef struct {
    const char *type;                 /* "Vector2", "Camera3D" ... */
    const GclNativeField *fields;
    int field_count;
} GclNativeStruct;

/* Native bir struct tipinin alanları (yoksa NULL). Camera ve Camera3D aynıdır. */
const GclNativeStruct *gcl_native_struct(const char *type);
/* 'type' native bir struct mı? */
int gcl_native_is_struct(const char *type);

/* ---- Üretilen tablolar (complete_native_db.c) ---- */
extern const GclNativeMember gcl_native_db_Raylib[];
extern const int gcl_native_db_Raylib_count;
extern const GclNativeMember gcl_native_db_Raygui[];
extern const int gcl_native_db_Raygui_count;

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_NATIVE_H */
