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

/* Çıktı .gcBundle yolunu kur: <out_dir><sep><project_name>.gcBundle.
   BD-1: yerel yol ayracı kullanılır (Windows '\\', diğer platformlarda '/').
         Windows'ta fopen '/' kabul etse de, dosyayı üreten/okuyan tüm kod
         ve dış araçlar (IDE, kabuk) için native ayraç daha tutarlıdır.
   BD-2: snprintf taşması KONTROL edilir — kesilmiş yol sessizce yazılmaz.
   Başarı: 0, hata: -1. */
static int make_bundle_out_path(char *out, size_t outsz,
                                const char *out_dir, const char *project_name) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    size_t dl = strlen(out_dir);
    /* out_dir ayraçla bitiyorsa ikinci ayraç EKLENMEZ (çift ayraç üretme). */
    int use_sep = (dl > 0 && out_dir[dl - 1] != '/' && out_dir[dl - 1] != '\\');

    int n;
    if (use_sep)
        n = snprintf(out, outsz, "%s%c%s.gcBundle", out_dir, sep, project_name);
    else
        n = snprintf(out, outsz, "%s%s.gcBundle", out_dir, project_name);

    if (n < 0 || (size_t)n >= outsz) {
        gcb_set_error("build: output path too long");
        return -1;
    }
    return 0;
}

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
    if (make_bundle_out_path(out_path, sizeof(out_path), out_dir, project_name) != 0)
        return -1;
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
    if (make_bundle_out_path(out_path, sizeof(out_path), out_dir, project_name) != 0)
        return -1;
    return gcb_pack_project_runtime(project_dir, runtime_dir, project_name, out_path);
}
