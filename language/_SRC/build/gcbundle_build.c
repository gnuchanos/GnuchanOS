/*
 * gcbundle_build.c — `gcl -build` output generation.
 *
 * Doc:
 *   gcl -build project.gcdata -o path/dir
 *       -> Project_name.exe
 *       -> Project_name.gcBundle   (all runtime + dll/so in a single package)
 *
 * This file produces the .gcBundle. `project_dir` = project root
 * (scripts/, assets/, Library/, etc.). `project_name` is the output name.
 * The produced .gcBundle is opened in memory at runtime and the files it
 * contains are accessed directly (no extract).
 */

#include "gcbundle.h"
#include "gcbundle_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build the output .gcBundle path: <out_dir><sep><project_name>.gcBundle.
   BD-1: the native path separator is used (Windows '\\', '/' on other platforms).
         Even though fopen accepts '/' on Windows, the native separator is more
         consistent for all code that produces/reads the file and for external
         tools (IDE, shell).
   BD-2: snprintf overflow is CHECKED — a truncated path is not written silently.
   Success: 0, error: -1. */
static int make_bundle_out_path(char *out, size_t outsz,
                                const char *out_dir, const char *project_name) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    size_t dl = strlen(out_dir);
    /* If out_dir ends with a separator, a second separator is NOT ADDED (avoids a double separator). */
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

/* Packs the project root directory into a .gcBundle.
   Writes <project_name>.gcBundle into out_dir.
   Success: 0, error: -1. */
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

/* Packs the project root + runtime (Library/) directory into a .gcBundle.
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
