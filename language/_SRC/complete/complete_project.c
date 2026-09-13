/*
 * complete_project.c — Proje modeli & dizin taraması (§8).
 *
 * project.gcdata basit bir JSON'dur; burada tam bir JSON parser yerine
 * anahtar arama (key lookup) yeterlidir. Bulunamayan alanlar varsayılana
 * düşer; böylece gcdata olmayan klasörlerde de motor güvenli çalışır.
 */
#include "complete_project.h"
#include "complete_index.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

/* ---- küçük JSON yardımcıları ---- */

/* json içinde "key" anahtarını bul; iki nokta sonrası ilk ':' konumunu ver. */
static const char *json_find_key(const char *json, const char *key) {
    if (!json || !key) return NULL;
    size_t klen = strlen(key);
    for (const char *p = json; (p = strstr(p, key)) != NULL; p += klen) {
        /* "..." tam anahtar mı? (öncesi/sonrası tırnak) */
        if (p == json || p[-1] != '"') continue;
        const char *after = p + klen;
        if (*after != '"') continue;
        after++;
        while (*after == ' ' || *after == '\t') after++;
        if (*after != ':') continue;
        after++;
        while (*after == ' ' || *after == '\t') after++;
        return after;
    }
    return NULL;
}

/* "key": "değer" → out'a değeri yaz. Başarıda 1. */
static int json_get_string(const char *json, const char *key, char *out, size_t cap) {
    const char *v = json_find_key(json, key);
    if (!v || *v != '"') return 0;
    v++;
    size_t i = 0;
    while (*v && *v != '"' && i + 1 < cap) {
        if (*v == '\\' && v[1]) v++;
        out[i++] = *v++;
    }
    out[i] = '\0';
    return 1;
}

/* "key": true|false → 1/0. */
static int json_get_bool(const char *json, const char *key, int dflt) {
    const char *v = json_find_key(json, key);
    if (!v) return dflt;
    if (strncmp(v, "true", 4) == 0) return 1;
    if (strncmp(v, "false", 5) == 0) return 0;
    return dflt;
}

static void copy_str(char *dst, size_t cap, const char *src) {
    if (cap == 0) return;
    snprintf(dst, cap, "%s", src ? src : "");
}

void gcl_project_sub_path(const GclProject *p, const char *sub,
                          char *out, size_t cap) {
    if (!p || !out || cap == 0) return;
    if (!sub || !sub[0]) snprintf(out, cap, "%s", p->root);
    else if (sub[0] == '/') snprintf(out, cap, "%s", sub);
    else snprintf(out, cap, "%s/%s", p->root, sub);
}

void gcl_project_load(GclProject *p, const char *workspace) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    copy_str(p->root, sizeof(p->root), workspace ? workspace : ".");
    copy_str(p->include_path, sizeof(p->include_path), "include");
    copy_str(p->lib_path, sizeof(p->lib_path), "lib");
    copy_str(p->external_path, sizeof(p->external_path), "external");
    copy_str(p->lua_import_path, sizeof(p->lua_import_path), "scripts");
    copy_str(p->python_import_path, sizeof(p->python_import_path), "scripts");
    copy_str(p->asset_path, sizeof(p->asset_path), "assets");
    copy_str(p->lua_main, sizeof(p->lua_main), "main.lua");
    copy_str(p->python_main, sizeof(p->python_main), "main.py");
    p->open_lua = 0;
    p->open_python = 0;
    p->valid = 0;

    if (!workspace || !workspace[0]) return;

    char gcdata_path[1200];
    snprintf(gcdata_path, sizeof(gcdata_path), "%s/project.gcdata", workspace);
    const char *json = NULL;
    size_t jlen = 0;
    if (!gcl_index_read(gcdata_path, &json, &jlen, NULL) || !json) return;
    p->valid = 1;

    char tmp[512];
    if (json_get_string(json, "gcl_include_path", tmp, sizeof(tmp)))
        copy_str(p->include_path, sizeof(p->include_path), tmp);
    if (json_get_string(json, "gcl_lib_path", tmp, sizeof(tmp)))
        copy_str(p->lib_path, sizeof(p->lib_path), tmp);
    if (json_get_string(json, "gcl_external_path", tmp, sizeof(tmp)))
        copy_str(p->external_path, sizeof(p->external_path), tmp);
    if (json_get_string(json, "lua_import_path", tmp, sizeof(tmp)))
        copy_str(p->lua_import_path, sizeof(p->lua_import_path), tmp);
    if (json_get_string(json, "python_import_path", tmp, sizeof(tmp)))
        copy_str(p->python_import_path, sizeof(p->python_import_path), tmp);
    if (json_get_string(json, "raylib_asset_directory_path", tmp, sizeof(tmp)))
        copy_str(p->asset_path, sizeof(p->asset_path), tmp);
    if (json_get_string(json, "lua_main_file", tmp, sizeof(tmp)))
        copy_str(p->lua_main, sizeof(p->lua_main), tmp);
    if (json_get_string(json, "python_main_file", tmp, sizeof(tmp)))
        copy_str(p->python_main, sizeof(p->python_main), tmp);
    p->open_lua    = json_get_bool(json, "open_lua", 0);
    p->open_python = json_get_bool(json, "open_python", 0);
}

int gcl_project_list_dir(const char *dir, const char *ext,
                         char out[][GCL_PROJECT_NAME], int max) {
    if (!dir || !out || max <= 0) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (name[0] == '.') continue;
        if (ext && ext[0]) {
            const char *e = strrchr(name, '.');
            if (!e || strcmp(e, ext) != 0) continue;
        }
        if (n >= max) break;
        snprintf(out[n], GCL_PROJECT_NAME, "%s", name);
        n++;
    }
    closedir(d);
    return n;
}

/* Dizin girdileri: dosyalar adıyla, alt dizinler sonuna '/' ile (§Faz 6).
   Yol tamamlamasında "textures/" seçilince bir alt dizine inilir. */
int gcl_project_list_entries(const char *dir,
                             char out[][GCL_PROJECT_NAME], int max) {
    if (!dir || !out || max <= 0) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < max) {
        const char *name = ent->d_name;
        if (name[0] == '.') continue;
        char full[1400];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        int is_dir = 0;
#ifdef _WIN32
        struct _stat st;
        if (_stat(full, &st) == 0 && (st.st_mode & _S_IFDIR)) is_dir = 1;
#else
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
#endif
        if (is_dir) snprintf(out[n], GCL_PROJECT_NAME, "%s/", name);
        else        snprintf(out[n], GCL_PROJECT_NAME, "%s", name);
        n++;
    }
    closedir(d);
    return n;
}

/* Tek bir .gcsf dosyasını scope'a ekle (içerik complete_index cache'inden). */
static void scan_file_into_scope(GclScope *scope, const char *path, int rank) {
    const char *text = NULL;
    size_t len = 0;
    if (!gcl_index_read(path, &text, &len, NULL) || !text) return;
    gcl_scope_add_source(scope, path, text, len, rank);
}

void gcl_project_scan_into_scope(const GclProject *p, GclScope *scope,
                                 const char *active_file) {
    if (!p || !scope) return;

    /* 1) Proje kökündeki .gcsf (main.gcsf vb.) — aktif dosya HARİÇ. */
    char root_files[64][GCL_PROJECT_NAME];
    int rn = gcl_project_list_dir(p->root, ".gcsf", root_files, 64);
    for (int i = 0; i < rn; i++) {
        char full[1200];
        snprintf(full, sizeof(full), "%s/%s", p->root, root_files[i]);
        if (active_file && strcmp(full, active_file) == 0) continue;
        scan_file_into_scope(scope, full, 3);
    }

    /* 2) include/ altındaki tüm .gcsf dosyaları (public sembol kaynağı). */
    char inc_dir[1200];
    gcl_project_sub_path(p, p->include_path, inc_dir, sizeof(inc_dir));
    char inc_files[256][GCL_PROJECT_NAME];
    int in = gcl_project_list_dir(inc_dir, ".gcsf", inc_files, 256);
    for (int i = 0; i < in; i++) {
        char full[1200];
        snprintf(full, sizeof(full), "%s/%s", inc_dir, inc_files[i]);
        if (active_file && strcmp(full, active_file) == 0) continue;
        scan_file_into_scope(scope, full, 3);
    }

    /* 3) lib/ altındaki .gclib dosyaları. */
    char lib_dir[1200];
    gcl_project_sub_path(p, p->lib_path, lib_dir, sizeof(lib_dir));
    char lib_files[128][GCL_PROJECT_NAME];
    int ln = gcl_project_list_dir(lib_dir, ".gclib", lib_files, 128);
    for (int i = 0; i < ln; i++) {
        char full[1200];
        snprintf(full, sizeof(full), "%s/%s", lib_dir, lib_files[i]);
        if (active_file && strcmp(full, active_file) == 0) continue;
        scan_file_into_scope(scope, full, 3);
    }
}
