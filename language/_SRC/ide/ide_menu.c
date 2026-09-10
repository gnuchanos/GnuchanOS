#include "gcl_ide_internal.h"

/* Safe fixed-size string copy - eliminates truncation warnings. */
static void copy_fixed(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    size_t n = src ? strlen(src) : 0;
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ---------------------------------------------
   Menu
   --------------------------------------------- */

const char *g_menu_titles[] = { "File", "View", "Project", "Help", NULL };
const char *g_menu_items[4][8] = {
    { "Open File...", "Open Directory...", "Save", "Save As...", "Run", NULL },
    { "Toggle Explorer", "Output", "Settings...", "About", NULL },
    { "New Project...", "Open Project...", "Run Project", "Build Project...", NULL },
    { "Keyboard Shortcuts", NULL }
};
MenuAction g_menu_actions[4][8] = {
    { MACT_OPEN_FILE, MACT_OPEN_DIRECTORY, MACT_SAVE, MACT_SAVE_AS, MACT_RUN },
    { MACT_TOGGLE_EXPLORER, MACT_TOGGLE_OUTPUT, MACT_SETTINGS, MACT_ABOUT },
    { MACT_NEW_PROJECT, MACT_OPEN_PROJECT, MACT_RUN_PROJECT, MACT_BUILD_PROJECT },
    { MACT_HELP }
};

int menu_item_count(int mi) {
    int n = 0;
    while (g_menu_items[mi][n]) n++;
    return n;
}

void editor_get_active_dir(Editor *ed, char *out, size_t outsz) {
    if (CURP.path && CURP.path[0]) {
        fs_parent_dir(CURP.path, out, outsz);
        if (out[0]) return;
    }
    snprintf(out, outsz, "%s", ed->cwd);
}

void editor_run_program(Editor *ed) {
    if (!CURP.path || !CURP.path[0]) {
        snprintf(ed->output, sizeof(ed->output), "Error: no file to run (save first)\n");
        ed->output_len = strlen(ed->output);
        return;
    }
    /* use our own exe (gcl may not be in PATH) — ide.dll can no longer access g_gcl_argv */
    const char *self = getenv("GCL_EXE_PATH");
    if (!self || !self[0]) self = "gcl";
    const char *ext = strrchr(CURP.path, '.');
    const char *mode;
    if (ext && strcmp(ext, ".lua") == 0) mode = "-luarun";
    else if (ext && strcmp(ext, ".py") == 0) mode = "-pyrun";
    else mode = "-run";
    char cmd[4096];
#ifdef _WIN32
    /* cmd /c ""..."" 2>&1 — output is read asynchronously from the pipe */
    snprintf(cmd, sizeof(cmd), "cmd /c \"\"%s\" %s \"%s\"\" 2>&1", self, mode, CURP.path);
#else
    snprintf(cmd, sizeof(cmd), "\"%s\" %s \"%s\" 2>&1", self, mode, CURP.path);
#endif
    /* Async process instead of synchronous popen: the IDE does not freeze, and the process
       terminates when the IDE closes. There is no project.gclog for single-file Run -> logf = NULL. */
    gcl_ide_proc_start(ed, cmd, CURP.path, NULL);
}

void menu_action_run(Editor *ed, MenuAction a) {
    switch (a) {
        case MACT_OPEN_FILE: {
            char dir[2048];
            editor_get_active_dir(ed, dir, sizeof(dir));
            char *p = gcl_ide_native_dialog(0, dir);
            if (p) { tab_add(ed, p); free(p); }
        } break;
        case MACT_OPEN_DIRECTORY: {
            char out[4096] = "";
            char *p = gcl_ide_native_dialog_folder(out, sizeof(out));
            if (p && out[0]) {
                copy_fixed(ed->cwd, sizeof(ed->cwd), out);
                ed->expanded_count = 0;
                tree_rescan(ed);
            }
        } break;
        case MACT_SAVE: {
            GclIdeBuffer *b = &CURP;
            if (b->path && b->path[0]) {
                gcl_ide_buffer_save(b);
            } else {
                char dir[2048];
                editor_get_active_dir(ed, dir, sizeof(dir));
                char *p = gcl_ide_native_dialog(1, dir);
                if (p) {
                    free(b->path);
                    b->path = gcl_strdup(p);
                    gcl_ide_buffer_save(b);
                    free(p);
                }
            }
        } break;
        case MACT_SAVE_AS: {
            GclIdeBuffer *b = &CURP;
            char dir[2048];
            if (b->path && b->path[0]) fs_parent_dir(b->path, dir, sizeof(dir));
            else editor_get_active_dir(ed, dir, sizeof(dir));
            char *p = gcl_ide_native_dialog(1, dir);
            if (p) {
                free(b->path);
                b->path = gcl_strdup(p);
                gcl_ide_buffer_save(b);
                free(p);
                tree_rescan(ed);
            }
        } break;
        case MACT_RUN:
            editor_run_program(ed);
            break;
        case MACT_TOGGLE_EXPLORER: ed->sidebar_visible = !ed->sidebar_visible; break;
        case MACT_TOGGLE_OUTPUT: ed->output_visible = !ed->output_visible; break;
        case MACT_SETTINGS:
            gcl_settings_panel_open(ed);
            break;
        case MACT_ABOUT: ed->about = 1; break;
        case MACT_NEW_PROJECT:
            ide_new_project_open(ed);
            break;
        case MACT_OPEN_PROJECT: {
            char dir[2048];
            editor_get_active_dir(ed, dir, sizeof(dir));
            char *p = gcl_ide_native_dialog(0, dir);
            if (p) {
                char parent[4096];
                fs_parent_dir(p, parent, sizeof(parent));
                if (parent[0]) {
                    copy_fixed(ed->current_project, sizeof(ed->current_project), parent);
                    copy_fixed(ed->cwd, sizeof(ed->cwd), parent);
                    ed->expanded_count = 0;
                    tree_rescan(ed);
                }
                free(p);
            }
        } break;
        case MACT_RUN_PROJECT:
            if (ed->current_project[0]) editor_run_project(ed);
            else {
                snprintf(ed->warning_msg, sizeof(ed->warning_msg),
                         "No project opened. Open a project (project.gcdata) first.");
                ed->warning_open = 1;
            }
            break;
        case MACT_BUILD_PROJECT:
            if (ed->current_project[0]) {
                editor_build_project(ed);
            } else {
                snprintf(ed->warning_msg, sizeof(ed->warning_msg),
                         "No project opened. Open a project (project.gcdata) first.");
                ed->warning_open = 1;
            }
            break;
        case MACT_HELP: ed->help_open = 1; break;
        default: break;
    }
}
