#include "gcl_ide_internal.h"

#include <dirent.h>

/* ---------------------------------------------
   File system helpers
   --------------------------------------------- */

/* gcl_lsp_list_dir silindi — yerel, bağımlılıksız dizin listeleme.
   MinGW dirent.h her iki platformda da çalışır (windows.h ile raylib tipleri çakışır). */
char **fs_list_dir(const char *dir, int *count) {
    *count = 0;
    if (!dir || !dir[0]) return NULL;
    char **entries = NULL;
    int cap = 0;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (*count >= cap) {
            cap = cap ? cap * 2 : 32;
            char **ne = (char **)realloc(entries, sizeof(char *) * (size_t)cap);
            if (!ne) {
                for (int i = 0; i < *count; i++) free(entries[i]);
                free(entries);
                return NULL;
            }
            entries = ne;
        }
        entries[*count] = gcl_strdup(de->d_name);
        (*count)++;
    }
    closedir(d);
    return entries;
}

int fs_is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int fs_mkdir(const char *p) {
    if (p && p[0]) {
        struct stat st;
        if (stat(p, &st) == 0) return 0;
#ifdef _WIN32
        return mkdir(p);
#else
        return mkdir(p, 0755);
#endif
    }
    return -1;
}

void fs_copy_file(const char *src, const char *dst) {
    if (!src || !dst) return;
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
}

void fs_copy_dir_rec(const char *src, const char *dst) {
    if (!src || !dst) return;
    fs_mkdir(dst);
    int n = 0;
    char **e = fs_list_dir(src, &n);
    if (!e) return;
    for (int i = 0; i < n; i++) {
        char sp[4096], dp[4096];
        snprintf(sp, sizeof(sp), "%s/%s", src, e[i]);
        snprintf(dp, sizeof(dp), "%s/%s", dst, e[i]);
        if (fs_is_dir(sp)) fs_copy_dir_rec(sp, dp);
        else fs_copy_file(sp, dp);
        free(e[i]);
    }
    free(e);
}

void fs_remove_rec(const char *p) {
    if (!p || !p[0]) return;
    if (fs_is_dir(p)) {
        int n = 0;
        char **e = fs_list_dir(p, &n);
        if (e) {
            for (int i = 0; i < n; i++) {
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", p, e[i]);
                fs_remove_rec(full);
                free(e[i]);
            }
            free(e);
        }
        rmdir(p);
    } else {
        remove(p);
    }
}

void fs_parent_dir(const char *path, char *out, size_t outsz) {
    snprintf(out, outsz, "%s", path);
    char *slash = strrchr(out, '/');
    char *bslash = strrchr(out, '\\');
    char *s = slash;
    if (!s || (bslash && bslash > s)) s = bslash;
    if (s) *s = '\0';
    else out[0] = '\0';
}

const char *path_basename(const char *p) {
    if (!p) return "";
    const char *s1 = strrchr(p, '/');
    const char *s2 = strrchr(p, '\\');
    const char *s = s1;
    if (!s || (s2 && s2 > s)) s = s2;
    return s ? s + 1 : p;
}

/* ---------------------------------------------
   Tab management
   --------------------------------------------- */

void tab_add(Editor *ed, const char *path) {
    if (ed->tab_count >= MAX_TABS) return;
    /* aynı dosya zaten açıksa yeni tab açma, mevcut tab'a geç */
    if (path && path[0]) {
        for (int i = 0; i < ed->tab_count; i++) {
            if (ed->tabs[i].path && strcmp(ed->tabs[i].path, path) == 0) {
                ed->active_tab = i;
                /* Seçim anchor'ını yeni aktif buffer'ın imleçine eşitle: yoksa 0'dan cursor'a
                   rastgele mavi seçim görünür. */
                ed->sel_anchor = editor_cur(ed)->cursor;
                return;
            }
        }
    }
    GclIdeBuffer *n = (GclIdeBuffer *)realloc(ed->tabs, sizeof(GclIdeBuffer) * (ed->tab_count + 1));
    if (!n) return;
    ed->tabs = n;
    GclIdeBuffer *nb = &ed->tabs[ed->tab_count];
    gcl_ide_buffer_init(nb);
    if (path && path[0]) gcl_ide_buffer_load(nb, path);
    ed->tab_count++;
    ed->active_tab = ed->tab_count - 1;
    ed->sel_anchor = editor_cur(ed)->cursor;
}

void tab_close(Editor *ed, int idx) {
    if (idx < 0 || idx >= ed->tab_count) return;
    gcl_ide_buffer_free(&ed->tabs[idx]);
    memmove(&ed->tabs[idx], &ed->tabs[idx + 1], (size_t)(ed->tab_count - idx - 1) * sizeof(GclIdeBuffer));
    ed->tab_count--;
    if (ed->tab_count == 0) {
        ed->active_tab = -1;
        ed->sel_anchor = 0;
        return;
    }
    if (ed->active_tab >= ed->tab_count) ed->active_tab = ed->tab_count - 1;
    ed->sel_anchor = editor_cur(ed)->cursor;
}

void tab_activate(Editor *ed, int idx) {
    if (idx < 0 || idx >= ed->tab_count) return;
    ed->active_tab = idx;
    ed->sel_anchor = editor_cur(ed)->cursor;
}

/* ---------------------------------------------
   Tree model
   --------------------------------------------- */

void tree_clear(TreeModel *tm) {
    if (!tm->nodes) return;
    for (int i = 0; i < tm->count; i++) free(tm->nodes[i].path);
    free(tm->nodes);
    tm->nodes = NULL;
    tm->count = 0;
    tm->cap = 0;
}

void tree_add(TreeModel *tm, const char *path, int is_dir, int depth) {
    if (tm->count >= tm->cap) {
        tm->cap = tm->cap ? tm->cap * 2 : 128;
        TreeNode *n = (TreeNode *)realloc(tm->nodes, sizeof(TreeNode) * tm->cap);
        if (!n) return;
        tm->nodes = n;
    }
    tm->nodes[tm->count].path = gcl_strdup(path);
    tm->nodes[tm->count].is_dir = is_dir;
    tm->nodes[tm->count].depth = depth;
    tm->count++;
}

int is_supported_ext(const char *name) {
    size_t ln = strlen(name);
    /* Her türlü metin/kod formatı (kullanıcı: "her türlü text formatını desteklesin") */
    const char *code_exts[] = {
        /* GCL */
        ".gcsf", ".gclib", ".gcdl", ".gc", ".gcsettings", ".gcdata",
        /* C ailesi */
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hxx",
        /* Lua / Python */
        ".lua", ".py", ".pyw",
        /* Web */
        ".html", ".htm", ".css", ".scss", ".less", ".js", ".jsx", ".ts", ".tsx",
        ".json", ".xml", ".yaml", ".yml", ".toml",
        /* Markdown / belge */
        ".md", ".markdown", ".txt", ".rst", ".doc", ".docx", ".rtf",
        /* Yapılandırma */
        ".ini", ".cfg", ".conf", ".properties", ".env", ".editorconfig",
        ".gitignore", ".gitattributes", ".gradle", ".cmake", ".mk", ".mak",
        ".project", ".cproject", ".prefs",
        /* Script / kabuk */
        ".sh", ".bash", ".zsh", ".bat", ".cmd", ".ps1", ".psm1", ".psd1",
        /* Veri */
        ".csv", ".tsv", ".sql", ".log",
        /* Diğer diller */
        ".go", ".rs", ".java", ".kt", ".kts", ".rb", ".php", ".swift",
        ".m", ".mm", ".cs", ".fs", ".fsx", ".pl", ".pm", ".r", ".jl",
        ".vb", ".f", ".f90", ".dart", ".ex", ".exs", ".erl", ".hrl",
        /* Godot (örnek projeler) */
        ".gd", ".tscn", ".tres", ".godot", ".gdignore",
        NULL
    };
    for (int i = 0; code_exts[i]; i++) {
        size_t el = strlen(code_exts[i]);
        if (ln > el && strcmp(name + ln - el, code_exts[i]) == 0) return 1;
    }
    /* Asset dosyaları (resim, ses, font) — IDE explorer'da da görünsün */
    const char *asset_exts[] = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga",
                                ".ttf", ".otf", ".wav", ".ogg", ".mp3", ".flac", NULL};
    for (int i = 0; asset_exts[i]; i++) {
        size_t el = strlen(asset_exts[i]);
        if (ln > el && strcmp(name + ln - el, asset_exts[i]) == 0) return 1;
    }
    return 0;
}

int is_expanded(Editor *ed, const char *path) {
    for (int i = 0; i < ed->expanded_count; i++) {
        if (strcmp(ed->expanded_dirs[i], path) == 0) return 1;
    }
    return 0;
}

void set_expanded(Editor *ed, const char *path, int expand) {
    if (expand) {
        for (int i = 0; i < ed->expanded_count; i++) {
            if (strcmp(ed->expanded_dirs[i], path) == 0) return;
        }
        if (ed->expanded_count < MAX_EXPANDED) {
            snprintf(ed->expanded_dirs[ed->expanded_count], sizeof(ed->expanded_dirs[0]), "%s", path);
            ed->expanded_count++;
        }
    } else {
        for (int i = 0; i < ed->expanded_count; i++) {
            if (strcmp(ed->expanded_dirs[i], path) == 0) {
                memmove(&ed->expanded_dirs[i], &ed->expanded_dirs[i + 1],
                        (size_t)(ed->expanded_count - i - 1) * sizeof(ed->expanded_dirs[0]));
                ed->expanded_count--;
                return;
            }
        }
    }
}

void tree_scan_node(Editor *ed, const char *dir, int depth, int max_depth) {
    if (depth > max_depth) return;
    int n = 0;
    char **e = fs_list_dir(dir, &n);
    if (!e) return;
    for (int i = 0; i < n; i++) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, e[i]);
        if (fs_is_dir(full)) {
            tree_add(&ed->tree, full, 1, depth);
            if (is_expanded(ed, full)) tree_scan_node(ed, full, depth + 1, max_depth);
        }
    }
    for (int i = 0; i < n; i++) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, e[i]);
        if (!fs_is_dir(full) && is_supported_ext(e[i])) tree_add(&ed->tree, full, 0, depth);
    }
    for (int i = 0; i < n; i++) free(e[i]);
    free(e);
}

void tree_rescan(Editor *ed) {
    /* seçili yolu koru */
    char selpath[4096] = "";
    if (ed->tree_selected >= 0 && ed->tree_selected < ed->tree.count) {
        snprintf(selpath, sizeof(selpath), "%s", ed->tree.nodes[ed->tree_selected].path);
    }
    tree_clear(&ed->tree);
    set_expanded(ed, ed->cwd, 1);
    tree_scan_node(ed, ed->cwd, 0, 6);
    ed->tree_selected = -1;
    if (selpath[0]) {
        for (int i = 0; i < ed->tree.count; i++) {
            if (strcmp(ed->tree.nodes[i].path, selpath) == 0) { ed->tree_selected = i; break; }
        }
    }
}
