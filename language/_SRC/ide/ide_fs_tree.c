#include "gcl_ide_internal.h"

#include <dirent.h>

/* ---------------------------------------------
   File system helpers
   --------------------------------------------- */

/* gcl_lsp_list_dir removed — local, dependency-free directory listing.
   MinGW dirent.h works on both platforms (windows.h collides with raylib types). */
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

/* Verilen yolu (göreli olsa bile) tam/mutlak yola çevirir.
   Explorer'daki "Copy Path" bunu panoya yazar (todo #5). Sistem çağrısı
   başarısız olursa (dosya yok vb.) yol aynen kopyalanır — böylece işlev
   her durumda çalışır. */
void fs_absolute_path(const char *path, char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    out[0] = '\0';
    if (!path || !path[0]) return;
#ifdef _WIN32
    if (_fullpath(out, path, outsz) == NULL) {
        snprintf(out, outsz, "%s", path);
    }
#else
    /* POSIX realpath resolved_path'i en az PATH_MAX bayt ister; 4096 yeterlidir. */
    if (realpath(path, out) == NULL) {
        snprintf(out, outsz, "%s", path);
    }
#endif
}

/* ---------------------------------------------
   Tab management
   --------------------------------------------- */

void tab_add_pane(Editor *ed, int pane, const char *path) {
    GclIdeBuffer *pane_tabs = (pane == 0) ? ed->tabs : ed->split_right_tabs;
    int *pane_count = (pane == 0) ? &ed->tab_count : &ed->split_right_tab_count;
    int *pane_active = (pane == 0) ? &ed->split_left_tab : &ed->split_right_tab;

    if (*pane_count >= MAX_TABS) return;
    if (path && path[0]) {
        for (int i = 0; i < *pane_count; i++) {
            if (pane_tabs[i].path && strcmp(pane_tabs[i].path, path) == 0) {
                *pane_active = i;
                if (pane == 0) ed->active_tab = i;
                pane_tabs[i].sel_anchor = pane_tabs[i].cursor;
                return;
            }
        }
    }

    GclIdeBuffer *n = (GclIdeBuffer *)realloc(pane_tabs, sizeof(GclIdeBuffer) * (*pane_count + 1));
    if (!n) return;
    if (pane == 0) ed->tabs = n; else ed->split_right_tabs = n;
    pane_tabs = (pane == 0) ? ed->tabs : ed->split_right_tabs;
    GclIdeBuffer *nb = &pane_tabs[*pane_count];
    gcl_ide_buffer_init(nb);
    if (path && path[0]) gcl_ide_buffer_load(nb, path);
    (*pane_count)++;
    int new_idx = *pane_count - 1;
    *pane_active = new_idx;
    if (pane == 0) ed->active_tab = new_idx;
    pane_tabs[new_idx].sel_anchor = pane_tabs[new_idx].cursor;
}

void tab_add(Editor *ed, const char *path) {
    if (ed->split_enabled) {
        tab_add_pane(ed, ed->split_focus, path);
        return;
    }
    if (ed->tab_count >= MAX_TABS) return;
    if (path && path[0]) {
        for (int i = 0; i < ed->tab_count; i++) {
            if (ed->tabs[i].path && strcmp(ed->tabs[i].path, path) == 0) {
                ed->active_tab = i;
                ed->tabs[i].sel_anchor = ed->tabs[i].cursor;
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
    ed->tabs[ed->active_tab].sel_anchor = ed->tabs[ed->active_tab].cursor;
}

void tab_close_pane(Editor *ed, int pane, int idx) {
    GclIdeBuffer *pane_tabs = (pane == 0) ? ed->tabs : ed->split_right_tabs;
    int *pane_count = (pane == 0) ? &ed->tab_count : &ed->split_right_tab_count;
    int *pane_active = (pane == 0) ? &ed->split_left_tab : &ed->split_right_tab;

    if (!pane_tabs || idx < 0 || idx >= *pane_count) return;
    gcl_ide_buffer_free(&pane_tabs[idx]);
    memmove(&pane_tabs[idx], &pane_tabs[idx + 1], (size_t)(*pane_count - idx - 1) * sizeof(GclIdeBuffer));
    (*pane_count)--;
    if (*pane_count == 0) {
        *pane_active = -1;
        if (pane == 0) {
            ed->active_tab = -1;
            ed->split_left_tab = -1;
        } else {
            ed->split_right_tab = -1;
        }
        return;
    }

    if (*pane_active == idx) {
        *pane_active = (idx < *pane_count) ? idx : *pane_count - 1;
    } else if (*pane_active > idx) {
        (*pane_active)--;
    }
    if (pane == 0) {
        if (ed->active_tab == idx) ed->active_tab = (idx < ed->tab_count) ? idx : ed->tab_count - 1;
        else if (ed->active_tab > idx) ed->active_tab--;
        if (ed->active_tab < 0) ed->active_tab = 0;
        if (ed->split_left_tab < 0 || ed->split_left_tab >= ed->tab_count) ed->split_left_tab = ed->active_tab;
    } else {
        if (ed->split_right_tab < 0 || ed->split_right_tab >= ed->split_right_tab_count) ed->split_right_tab = *pane_active;
    }
    if (pane == 0 && ed->tabs && ed->tab_count > 0) ed->tabs[ed->active_tab].sel_anchor = ed->tabs[ed->active_tab].cursor;
    if (pane == 1 && ed->split_right_tabs && ed->split_right_tab_count > 0) ed->split_right_tabs[*pane_active].sel_anchor = ed->split_right_tabs[*pane_active].cursor;
}

void tab_close(Editor *ed, int idx) {
    if (idx < 0 || idx >= ed->tab_count) return;
    int removed = idx;
    gcl_ide_buffer_free(&ed->tabs[idx]);
    memmove(&ed->tabs[idx], &ed->tabs[idx + 1], (size_t)(ed->tab_count - idx - 1) * sizeof(GclIdeBuffer));
    ed->tab_count--;
    if (ed->tab_count == 0) {
        ed->active_tab = -1;
        ed->split_left_tab = -1;
        ed->blank_tab.sel_anchor = 0;
        if (ed->split_enabled && ed->split_right_tab_count > 0) {
            ed->split_focus = 1;
            if (ed->split_right_tab < 0 || ed->split_right_tab >= ed->split_right_tab_count) {
                ed->split_right_tab = 0;
            }
        }
        return;
    }

    if (ed->active_tab == removed) {
        ed->active_tab = (removed < ed->tab_count) ? removed : ed->tab_count - 1;
    } else if (ed->active_tab > removed) {
        ed->active_tab--;
    }
    if (ed->split_left_tab == removed) {
        ed->split_left_tab = (removed < ed->tab_count) ? removed : ed->tab_count - 1;
    } else if (ed->split_left_tab > removed) {
        ed->split_left_tab--;
    }

    if (ed->split_left_tab < 0) ed->split_left_tab = ed->active_tab;
    if (ed->active_tab < 0) ed->active_tab = 0;
    if (ed->split_left_tab < 0 || ed->split_left_tab >= ed->tab_count) ed->split_left_tab = ed->active_tab;
    if (ed->split_enabled && ed->split_right_tab_count > 0 && ed->split_focus == 0 && ed->split_left_tab < 0) {
        ed->split_focus = 1;
    }
    editor_cur(ed)->sel_anchor = editor_cur(ed)->cursor;
}

void tab_activate_pane(Editor *ed, int pane, int idx) {
    if (pane == 0) {
        if (idx < 0 || idx >= ed->tab_count) return;
        ed->split_left_tab = idx;
        ed->active_tab = idx;
        ed->tabs[idx].sel_anchor = ed->tabs[idx].cursor;
    } else {
        if (idx < 0 || idx >= ed->split_right_tab_count) return;
        ed->split_right_tab = idx;
        ed->split_right_tabs[idx].sel_anchor = ed->split_right_tabs[idx].cursor;
    }
}

/* Activate a tab. The index always belongs to the FOCUSED pane's own array.
   The previous split branch wrote a pane-0 index (validated against tab_count)
   into split_right_tab and forced the global active_tab, so a click in one pane
   silently changed the other pane's selected tab (todo bug #3/#6). */
void tab_activate(Editor *ed, int idx) {
    if (ed->split_enabled) {
        tab_activate_pane(ed, ed->split_focus == 1 ? 1 : 0, idx);
        return;
    }
    if (idx < 0 || idx >= ed->tab_count) return;
    ed->active_tab = idx;
    editor_cur(ed)->sel_anchor = editor_cur(ed)->cursor;
}

/* ---- split view -> single view -------------------------------------------
   Closing the split used to drop the RIGHT pane's tabs on the floor: they stayed
   in split_right_tabs (invisible in single view) and the editor jumped back to
   whatever tab happened to be active before, i.e. the file the user was editing
   seemed to vanish (todo bug #1/#23: split-merge transition).

   tab_fold_right_into_main() moves ONE right-pane tab into the main list. The
   GclIdeBuffer struct is COPIED (path, text, cursor and undo history all move with
   it) and then removed from the right list with memmove WITHOUT freeing it, so no
   buffer is lost and nothing is freed twice. */

static int tab_fold_right_into_main(Editor *ed, int idx) {
    if (!ed->split_right_tabs || idx < 0 || idx >= ed->split_right_tab_count) return -1;
    if (ed->tab_count >= MAX_TABS) return -1;

    GclIdeBuffer *grown = (GclIdeBuffer *)realloc(ed->tabs, sizeof(GclIdeBuffer) * (size_t)(ed->tab_count + 1));
    if (!grown) return -1;
    ed->tabs = grown;
    ed->tabs[ed->tab_count] = ed->split_right_tabs[idx];   /* ownership moves here */

    memmove(&ed->split_right_tabs[idx], &ed->split_right_tabs[idx + 1],
            (size_t)(ed->split_right_tab_count - idx - 1) * sizeof(GclIdeBuffer));
    ed->split_right_tab_count--;
    if (ed->split_right_tab_count <= 0) ed->split_right_tab = -1;
    else if (ed->split_right_tab >= ed->split_right_tab_count) ed->split_right_tab = ed->split_right_tab_count - 1;
    else if (ed->split_right_tab > idx) ed->split_right_tab--;

    ed->tab_count++;
    ed->active_tab = ed->tab_count - 1;
    ed->split_left_tab = ed->active_tab;
    return ed->active_tab;
}

/* Merge the whole right pane back into the single view (split OFF). Every right
   tab is appended to the main list in order; if the main list is already full
   (MAX_TABS) the rest stay in the right list and reappear on the next split.
   Returns the main-list index of the tab the user was editing (or -1). */
int tab_fold_split_into_main(Editor *ed) {
    int want = (ed->split_enabled && ed->split_focus == 1) ? ed->split_right_tab : -1;
    int want_main_idx = -1;

    while (ed->split_right_tabs && ed->split_right_tab_count > 0 && ed->tab_count < MAX_TABS) {
        int res = tab_fold_right_into_main(ed, 0);
        if (res < 0) break;
        if (want == 0) want_main_idx = res;
        want--;
    }

    if (want_main_idx >= 0 && want_main_idx < ed->tab_count) {
        ed->active_tab = want_main_idx;
        ed->split_left_tab = want_main_idx;
    } else if (ed->active_tab < 0 && ed->tab_count > 0) {
        ed->active_tab = 0;
        ed->split_left_tab = 0;
    }
    if (ed->tab_count > 0 && ed->active_tab >= ed->tab_count) ed->active_tab = ed->tab_count - 1;
    if (ed->tabs && ed->active_tab >= 0 && ed->active_tab < ed->tab_count) {
        ed->tabs[ed->active_tab].sel_anchor = ed->tabs[ed->active_tab].cursor;
    }

    /* Nothing left to show in the right pane: release its array instead of keeping
       an empty allocation alive for the rest of the session. */
    if (ed->split_right_tab_count <= 0 && ed->split_right_tabs) {
        free(ed->split_right_tabs);
        ed->split_right_tabs = NULL;
        ed->split_right_tab = -1;
    }
    return want_main_idx;
}

/* ---- split view transitions (View > Split) -------------------------------
   Both transitions live here (not in ide_menu.c) so the menu and the headless
   pane test drive exactly the same code. */

/* Split ON: the LEFT pane takes over the tab that is being edited RIGHT NOW; the
   right pane keeps its own list (usually empty). Reusing a stale split_left_tab
   made the left pane show some other file than the active one, leaving
   split_left_tab and active_tab disagreeing (todo bug #1). */
void editor_split_enable(Editor *ed) {
    ed->split_enabled = 1;
    ed->split_ratio = 0.5f;
    ed->split_left_tab = (ed->active_tab >= 0 && ed->active_tab < ed->tab_count)
                         ? ed->active_tab : -1;
    ed->split_right_tab = (ed->split_right_tab_count > 0)
                          ? ed->split_right_tab_count - 1 : -1;
    ed->split_left_tab_scroll = 0.0f;
    ed->split_right_tab_scroll = 0.0f;
    ed->split_focus = (ed->split_left_tab >= 0) ? 0
                      : (ed->split_right_tab_count > 0 ? 1 : 0);
}

/* Split OFF: pull the right pane's tabs into the single view so the file the user
   was editing there stays open, and focus that tab (todo bug #1/#23). */
void editor_split_disable(Editor *ed) {
    tab_fold_split_into_main(ed);
    ed->split_enabled = 0;
    ed->split_focus = 0;
    ed->split_left_tab_scroll = 0.0f;
    ed->split_right_tab_scroll = 0.0f;
}

/* ---------------------------------------------
   Pane state (split view)
   ---------------------------------------------
   Pane 0 = the main tab list (ed->tabs / active_tab), pane 1 = the right pane's own
   list (ed->split_right_tabs / split_right_tab). These helpers used to live in
   ide_main.c as statics; they are pure state with no raylib drawing, so they live
   with the tab code and are exercised by the headless pane test. */

int editor_pane_empty(const Editor *ed, int pane) {
    if (!ed->split_enabled) return ed->tab_count <= 0 || ed->active_tab < 0;
    if (pane == 0) return ed->tab_count <= 0 || ed->split_left_tab < 0;
    return ed->split_right_tab_count <= 0 || ed->split_right_tab < 0;
}

int editor_active_pane_has_tab(const Editor *ed) {
    return !editor_pane_empty(ed, ed->split_enabled ? ed->split_focus : 0);
}

int editor_pane_tab_index(const Editor *ed, int pane) {
    if (!ed->split_enabled) return ed->active_tab;
    if (pane == 0) return ed->split_left_tab;
    return ed->split_right_tab;
}

/* The buffer a pane really displays. An out-of-range or "no tab" index resolves to
   the blank buffer, never to another tab of the same array: the previous
   `return &pane_tabs[0]` fallback made a pane render (and receive typing for) a
   buffer that its tab strip did not show (todo bug #6). */
GclIdeBuffer *editor_pane_cur(Editor *ed, int pane) {
    if (pane == 1) {
        int idx = ed->split_right_tab;
        if (idx >= 0 && idx < ed->split_right_tab_count && ed->split_right_tabs) {
            return &ed->split_right_tabs[idx];
        }
        return &ed->blank_tab;
    }
    int idx = editor_pane_tab_index(ed, pane);
    if (idx >= 0 && idx < ed->tab_count && ed->tabs) return &ed->tabs[idx];
    return &ed->blank_tab;
}

/* Focus a pane. The state stays PANE-LOCAL: the focus is never moved to the other
   pane and a pane's selected tab is never dropped behind the user's back. Two
   defects used to do exactly that (todo bug #1/#3):
     - clicking the EMPTY left pane re-targeted the focus to the right pane, so the
       left pane could not be focused to open a file into it;
     - the right pane's index was reset to -1 whenever it happened to equal the left
       pane's, which made a right pane FULL of tabs render as empty. */
void editor_set_split_focus(Editor *ed, int pane) {
    if (!ed->split_enabled) return;
    if (pane != 0 && pane != 1) pane = 0;
    /* Odak GERCEKTEN degisiyorsa acik tamamlama kaplamalarini kapat.
       Tamamlama durumu (completion_*) `Editor` uzerinde TEK bir kumedir ve
       cizim odakli pane'in dilimine hizalanir; eski pane'in imleci icin
       hesaplanmis liste/imza seridi odak degisince YENI pane'in kod alaninda
       yanlis konumda gorunurdu (split gorunumunde "popup yanlis kutuda").
       `completion_dismissed` BILEREK set edilmez: kullanici yeni pane'de
       yazmaya baslayinca popup yeniden acilmalidir (ESC'den farki bu). */
    if (ed->split_focus != pane) {
        ed->completion_visible = 0;
        ed->completion_no_match = 0;
        ed->completion_message[0] = '\0';
        ed->completion_have_sig = 0;
        ed->completion_have_diag = 0;
    }
    ed->split_focus = pane;

    if (pane == 0) {
        /* Adopt the global active tab only when this pane has none yet. */
        if (ed->split_left_tab < 0 && ed->active_tab >= 0 && ed->active_tab < ed->tab_count) {
            ed->split_left_tab = ed->active_tab;
        }
        if (ed->split_left_tab >= ed->tab_count) {
            ed->split_left_tab = ed->tab_count > 0 ? ed->tab_count - 1 : -1;
        }
    } else {
        /* An empty right pane stays empty (and focusable). */
        if (ed->split_right_tab_count <= 0) {
            ed->split_right_tab = -1;
        } else if (ed->split_right_tab < 0 || ed->split_right_tab >= ed->split_right_tab_count) {
            ed->split_right_tab = ed->split_right_tab_count - 1;
        }
    }
}

void editor_ensure_valid_split_focus(Editor *ed) {
    if (!ed->split_enabled) return;
    if (ed->split_focus != 0 && ed->split_focus != 1) ed->split_focus = 0;

    /* A pane index must stay inside that pane's OWN array. A stale index left over
       from closing a tab used to be "repaired" by editor_pane_cur()/editor_cur()
       falling back to tabs[0] — i.e. one pane silently displayed ANOTHER pane's
       buffer or, worse, typing went into it (todo bug #6/#7). */
    if (ed->split_left_tab >= ed->tab_count)
        ed->split_left_tab = ed->tab_count > 0 ? ed->tab_count - 1 : -1;
    if (ed->split_left_tab < -1) ed->split_left_tab = -1;
    if (ed->split_right_tab >= ed->split_right_tab_count)
        ed->split_right_tab = ed->split_right_tab_count > 0 ? ed->split_right_tab_count - 1 : -1;
    if (ed->split_right_tab < -1) ed->split_right_tab = -1;

    /* An EMPTY pane keeps the focus on purpose: it must stay clickable so a tab can
       be opened into it ('+' in its own tab strip). Only when both panes are empty
       does the focus fall back to the left one (todo bug #2). */
    int left_ok = !editor_pane_empty(ed, 0);
    int right_ok = !editor_pane_empty(ed, 1);
    if (!left_ok && !right_ok) ed->split_focus = 0;
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
    /* Every kind of text/code format (user: "should support every text format") */
    const char *code_exts[] = {
        /* GCL */
        ".gcsf", ".gclib", ".gcdl", ".gc", ".gcsettings", ".gcdata",
        /* C family */
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hxx",
        /* Lua / Python */
        ".lua", ".py", ".pyw",
        /* Web */
        ".html", ".htm", ".css", ".scss", ".less", ".js", ".jsx", ".ts", ".tsx",
        ".json", ".xml", ".yaml", ".yml", ".toml",
        /* Markdown / document */
        ".md", ".markdown", ".txt", ".rst", ".doc", ".docx", ".rtf",
        /* Configuration */
        ".ini", ".cfg", ".conf", ".properties", ".env", ".editorconfig",
        ".gitignore", ".gitattributes", ".gradle", ".cmake", ".mk", ".mak",
        ".project", ".cproject", ".prefs",
        /* Script / shell */
        ".sh", ".bash", ".zsh", ".bat", ".cmd", ".ps1", ".psm1", ".psd1",
        /* Data */
        ".csv", ".tsv", ".sql", ".log",
        /* Other languages */
        ".go", ".rs", ".java", ".kt", ".kts", ".rb", ".php", ".swift",
        ".m", ".mm", ".cs", ".fs", ".fsx", ".pl", ".pm", ".r", ".jl",
        ".vb", ".f", ".f90", ".dart", ".ex", ".exs", ".erl", ".hrl",
        /* Godot (sample projects) */
        ".gd", ".tscn", ".tres", ".godot", ".gdignore",
        NULL
    };
    for (int i = 0; code_exts[i]; i++) {
        size_t el = strlen(code_exts[i]);
        if (ln >= el && strcmp(name + ln - el, code_exts[i]) == 0) return 1;
    }
    /* Asset files (image, sound, font) — they should also appear in the IDE explorer */
    const char *asset_exts[] = {
        /* Images raylib can load (png, qoi, bmp, tga, gif, jpg, psd, hdr, pic, pnm) */
        ".png", ".qoi", ".bmp", ".tga", ".gif", ".jpg", ".jpeg", ".psd",
        ".hdr", ".pic", ".ppm", ".pgm", ".pbm", ".pnm", ".pfm",
        /* Fonts */
        ".ttf", ".otf", ".fnt",
        /* Audio (raylib: wav, ogg, mp3, flac, qoa, xm) */
        ".wav", ".ogg", ".mp3", ".flac", ".qoa", ".xm", ".mod",
        /* 3D models / animation */
        ".obj", ".mtl", ".gltf", ".glb", ".iqm", ".vox", ".m3d",
        /* Shaders / data */
        ".vs", ".fs", ".glsl", ".vert", ".frag",
        /* Shared + static libraries and build artifacts:
           the explorer used to hide .dll/.so, so the whole build output was invisible. */
        ".dll", ".so", ".dylib", ".a", ".lib", ".o", ".def", ".d",
        NULL
    };
    for (int i = 0; asset_exts[i]; i++) {
        size_t el = strlen(asset_exts[i]);
        if (ln >= el && strcmp(name + ln - el, asset_exts[i]) == 0) return 1;
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
    /* preserve the selected path */
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
