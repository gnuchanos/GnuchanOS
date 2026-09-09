/*
 * gcbundle_build.c — `gcl -build` çıktı üretimi.
 *
 * Doc:
 *   gcl -build project.gcdata -o path/dir
 *       -> Project_name.exe
 *       -> Project_name.gcBundle   (tüm runtime + dll/so tek pakette)
 *
 * Bu dosya .gcBundle üretimini yapar. `project_dir` = proje kökü
 * (scripts/, assets/, Library/ vb.). `project_name` çıktı adıdır.
 * Üretilen .gcBundle, çalışma zamanında bellekte açılır ve içerdiği
 * dosyalara doğrudan erişilir (extract yok).
 */

#include "gcbundle.h"
#include "gcbundle_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Proje kök dizinini .gcBundle'a paketler.
   out_dir içine <project_name>.gcBundle yazar.
   Başarı: 0, hata: -1. */
int gcb_build_project(const char *project_dir, const char *project_name,
                      const char *out_dir) {
    if (!project_dir || !project_name || !out_dir) {
        gcb_set_error("build: null parameter");
        return -1;
    }

    char out_path[4096];
    snprintf(out_path, sizeof(out_path), "%s/%s.gcBundle", out_dir, project_name);
    return gcb_pack_dir(project_dir, project_name, out_path);
}

/* Proje kökü + runtime (Library/) dizinini .gcBundle'a paketler.
   Doc: "Project_name.gcBundle — all runtime and dll or so in this place". */
int gcb_build_project_runtime(const char *project_dir, const char *runtime_dir,
                              const char *project_name, const char *out_dir) {
    if (!project_dir || !runtime_dir || !project_name || !out_dir) {
        gcb_set_error("build: null parameter");
        return -1;
    }

    char out_path[4096];
    snprintf(out_path, sizeof(out_path), "%s/%s.gcBundle", out_dir, project_name);
    return gcb_pack_project_runtime(project_dir, runtime_dir, project_name, out_path);
}
