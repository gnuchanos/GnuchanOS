/*
 * complete_project.h — Proje modeli (§8).
 *
 * `project.gcdata` sürücüsü okunur (include/lib/external/scripts/assets
 * yolları, lua/python bayrakları) ve proje sembolleri (include zinciri,
 * main.gcsf) kapsama (scope) aktarılır.
 */
#ifndef GCL_COMPLETE_PROJECT_H
#define GCL_COMPLETE_PROJECT_H

#include "gcl_complete.h"
#include "complete_scope.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCL_PROJECT_NAME 256

typedef struct {
    char root[1024];               /* proje kökü (workspace) */
    char include_path[512];        /* varsayılan "include"   */
    char lib_path[512];            /* varsayılan "lib"       */
    char external_path[512];       /* varsayılan "external"  */
    char lua_import_path[512];     /* varsayılan "scripts"   */
    char python_import_path[512];  /* varsayılan "scripts"   */
    char asset_path[512];          /* varsayılan "assets"    */
    char lua_main[256];            /* "main.lua"             */
    char python_main[256];         /* "main.py"              */
    int  open_lua;
    int  open_python;
    int  valid;                    /* project.gcdata bulundu mu */
} GclProject;

/* workspace altındaki project.gcdata'yı yükle (yoksa varsayılan değerler). */
void gcl_project_load(GclProject *p, const char *workspace);

/* include/ + proje kökündeki .gcsf dosyalarını scope'a ekle (rank 3). */
void gcl_project_scan_into_scope(const GclProject *p, GclScope *scope,
                                 const char *active_file);

/* Dizin listele. ext ".gcsf" gibi (NULL → tüm dosyalar). Eklenen sayıyı döndür. */
int  gcl_project_list_dir(const char *dir, const char *ext,
                          char out[][GCL_PROJECT_NAME], int max);

/* Dizin girdilerini listele: dosyalar adıyla, ALT DİZİNLER sonuna '/' ile.
   Yol tamamlaması (assets/) için (§Faz 6). Eklenen sayıyı döndürür. */
int  gcl_project_list_entries(const char *dir,
                              char out[][GCL_PROJECT_NAME], int max);

/* p->root + "/" + sub yolunu üret. */
void gcl_project_sub_path(const GclProject *p, const char *sub,
                          char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_PROJECT_H */
