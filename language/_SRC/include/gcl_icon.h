/*
 * gcl_icon.h — project icon handling.
 *
 * Doc (simple_doc.md):
 *   project.gcdata "icon": "assets/icon.png" — at build time this PNG becomes
 *   the icon of the produced executable on BOTH Windows and Linux.
 *
 *   Windows : the PNG is written into the copied exe's PE resources
 *             (RT_ICON / RT_GROUP_ICON); Explorer then shows it as the icon
 *             of the program (taskbar, Alt-Tab, file list).
 *   Linux   : the ELF format has no icon slot, so the PNG is copied next to the
 *             executable as "<project_name>.png" and a "<project_name>.desktop"
 *             launcher is generated that points at it (the standard desktop
 *             integration used by every file manager / shell).
 */
#ifndef GCL_ICON_H
#define GCL_ICON_H

#include <stddef.h>

/* Relative path (from the project root) of the project icon: it is written by
   `gcl -new` / the IDE new-project flow and declared in project.gcdata as
   "icon". Shared by gcl_main.c and the IDE so both agree on the location. */
#define GCL_ICON_REL_PATH "assets/icon.png"

#ifdef __cplusplus
extern "C" {
#endif

/* Applies the icon declared by <project_dir>/project.gcdata to the built
   executable at exe_path.
   project_name is used for the Linux "<name>.png" / "<name>.desktop" names.
   Returns 0 on success, -1 when the project declares no usable icon or the
   operation failed. Callers treat a failure as a WARNING (never as a build
   error) — a project without an icon still builds. */
int gcl_icon_apply_project(const char *project_dir, const char *exe_path,
                           const char *project_name);

/* Writes the default GCL icon (the embedded gnuchan logo PNG) to dst_path.
   Used by `gcl -new` so a fresh project already has assets/icon.png.
   Returns 0 on success, -1 otherwise. */
int gcl_icon_write_default(const char *dst_path);

/* Human-readable reason why the last gcl_icon_apply_project call failed
   (Win32 error codes included on Windows). Never NULL. */
const char *gcl_icon_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GCL_ICON_H */
