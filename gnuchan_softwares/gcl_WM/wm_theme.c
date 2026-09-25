/*
 * wm_theme.c — publish the session's theme, icon set and cursor.
 *
 * There are three places a program looks for "which theme is this session
 * using", and a session that fills in only one of them gets windows that
 * disagree with each other. This fills in all three:
 *
 *   1. The X resource manager (the RESOURCE_MANAGER property on the root).
 *      This is where X itself and every program without a toolkit look, and
 *      it is the only place the cursor is named. The cursor's theme and size
 *      are read by Xlib the first time a client asks for a cursor, so a
 *      session that never writes them has to make do with whatever the
 *      pointer theme happened to be left at.
 *
 *   2. The GTK settings files (~/.config/gtk-3.0/settings.ini and gtk-4.0).
 *      This is where GTK itself looks when a program starts, and it is the
 *      one place a theme and an icon set can be named without any toolkit
 *      running to hear about it.
 *
 *   3. The environment. GTK_THEME, XCURSOR_THEME and XCURSOR_SIZE are read by
 *      a program before its own settings files and are what the WM passes on
 *      to everything it starts, so a terminal it opens gets the same cursor as
 *      the desktop around it.
 *
 * The names come from the environment first, so a session entry can name a
 * theme without this file being changed, and fall back to the themes
 * GnuchanOS installs. A name is only used when the theme is actually there:
 * naming a theme that is not installed draws every window in the toolkit's
 * built-in grey, which looks less like a theme was chosen and more like one
 * was lost.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "wm_core.h"
#include "wm_theme.h"

/* The themes GnuchanOS ships, and what to fall back to when they are not
   installed. The fallback is Adwaita because every GTK installation has it —
   a machine without it has no GTK theme at all and is beyond what a name can
   fix. */
#define GNUCHAN_GTK_THEME     "GnuChanTheme"
#define GNUCHAN_ICON_THEME    "GnuChanIconTheme"
#define GNUCHAN_CURSOR_THEME  "GnuChanCursorTheme"
#define FALLBACK_GTK_THEME    "Adwaita"
#define FALLBACK_ICON_THEME   "Adwaita"
#define FALLBACK_CURSOR_THEME "Adwaita"

#define CURSOR_SIZE_DEFAULT   24

static int is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

/* Make a directory and every directory above it.
 *
 * mkdir() makes one level and fails when the level above is missing, so
 * "make ~/.config/gtk-3.0" on a home that has no ~/.config does nothing at
 * all. A fresh home is exactly the case this runs in, so the parents are made
 * first, from the root down. An existing directory is not an error, because
 * two GTK versions are written and the second finds the first's parents. */
static void make_directories(const char *path) {
    if (is_directory(path)) {
        return;
    }

    char partial[4096];
    snprintf(partial, sizeof(partial), "%s", path);

    for (char *p = partial + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (!is_directory(partial)) {
            mkdir(partial, 0700);
        }
        *p = '/';
    }
    mkdir(partial, 0700);
}

/* Whether a theme of this name is installed, wherever a theme may live. The
   user's own directories are checked first because that is where a theme the
   user put there ends up, and a name in both places means the user's. */
static int theme_installed(const char *name) {
    char path[4096];
    const char *home = getenv("HOME");

    if (home && home[0]) {
        snprintf(path, sizeof(path), "%s/.themes/%s", home, name);
        if (is_directory(path)) return 1;
        snprintf(path, sizeof(path), "%s/.icons/%s", home, name);
        if (is_directory(path)) return 1;
        snprintf(path, sizeof(path), "%s/.local/share/themes/%s", home, name);
        if (is_directory(path)) return 1;
        snprintf(path, sizeof(path), "%s/.local/share/icons/%s", home, name);
        if (is_directory(path)) return 1;
    }

    snprintf(path, sizeof(path), "/usr/share/themes/%s", name);
    if (is_directory(path)) return 1;
    snprintf(path, sizeof(path), "/usr/share/icons/%s", name);
    if (is_directory(path)) return 1;

    return 0;
}

/* The first installed name from the environment, the shipped theme and then
   the fallback, so a user's choice wins, a default is used when it is there,
   and something usable is used when it is not. */
static const char *choose_theme(const char *variable, const char *shipped,
                                const char *fallback) {
    const char *chosen = getenv(variable);
    if (chosen && chosen[0] && theme_installed(chosen)) {
        return chosen;
    }
    if (shipped && theme_installed(shipped)) {
        return shipped;
    }
    return fallback;
}

static int choose_cursor_size(void) {
    const char *set = getenv("XCURSOR_SIZE");
    if (set && set[0]) {
        int size = atoi(set);
        if (size >= 8 && size <= 512) {
            return size;
        }
    }
    return CURSOR_SIZE_DEFAULT;
}

/* Write the X resource manager. Each setting is one line of "name:\tvalue",
   which is what every reader of this property expects. */
static void publish_to_resource_manager(WmCore *core, const char *cursor_theme,
                                        int cursor_size) {
    char resources[1024];
    snprintf(resources, sizeof(resources),
             "Xcursor.theme:\t%s\n"
             "Xcursor.size:\t%d\n"
             "Xft.dpi:\t96\n",
             cursor_theme, cursor_size);

    Atom resources_atom = wm_atom(core, "RESOURCE_MANAGER");
    /* xrdb and every reader of this property use STRING; UTF8_STRING is what
       the toolkit's own settings daemon uses, and both are accepted. STRING is
       the one that has always meant "this is an X resource file". */
    Atom utf8 = XA_STRING;
    XChangeProperty(core->display, core->root, resources_atom, utf8, 8,
                    PropModeReplace, (unsigned char *)resources,
                    (int)strlen(resources));
    XFlush(core->display);
}

/* One GTK settings file. GTK 3 and GTK 4 read the same keys from their own
   file, so the same text goes to both. */
static void write_gtk_settings(const char *home, const char *version,
                               const char *gtk_theme, const char *icon_theme,
                               const char *cursor_theme, int cursor_size) {
    char directory[4096];
    snprintf(directory, sizeof(directory), "%s/.config/gtk-%s.0",
             home, version);
    make_directories(directory);

    char path[4096 + 32];
    snprintf(path, sizeof(path), "%s/settings.ini", directory);

    FILE *file = fopen(path, "w");
    if (!file) {
        return;
    }
    fprintf(file,
            "[Settings]\n"
            "gtk-theme-name=%s\n"
            "gtk-icon-theme-name=%s\n"
            "gtk-cursor-theme-name=%s\n"
            "gtk-cursor-theme-size=%d\n"
            "gtk-font-name=Sans 10\n"
            "gtk-application-prefer-dark-theme=true\n",
            gtk_theme, icon_theme, cursor_theme, cursor_size);
    fclose(file);
}

/* --- the module ----------------------------------------------------------- */

static int theme_init(WmCore *core) {
    const char *gtk_theme = choose_theme("GTK_THEME", GNUCHAN_GTK_THEME,
                                         FALLBACK_GTK_THEME);
    const char *icon_theme = choose_theme("GTK_ICON_THEME",
                                          GNUCHAN_ICON_THEME,
                                          FALLBACK_ICON_THEME);
    const char *cursor_theme = choose_theme("XCURSOR_THEME",
                                            GNUCHAN_CURSOR_THEME,
                                            FALLBACK_CURSOR_THEME);
    int cursor_size = choose_cursor_size();

    publish_to_resource_manager(core, cursor_theme, cursor_size);

    const char *home = getenv("HOME");
    if (home && home[0]) {
        write_gtk_settings(home, "3", gtk_theme, icon_theme,
                           cursor_theme, cursor_size);
        write_gtk_settings(home, "4", gtk_theme, icon_theme,
                           cursor_theme, cursor_size);
    }

    /* The environment, so everything this session starts — a terminal from
       the key binding, a program from the menu — is born knowing the same
       theme, icon set and cursor as the desktop it was opened from. */
    setenv("GTK_THEME", gtk_theme, 1);
    setenv("XCURSOR_THEME", cursor_theme, 1);

    char size[16];
    snprintf(size, sizeof(size), "%d", cursor_size);
    setenv("XCURSOR_SIZE", size, 1);

    fprintf(stderr,
            "gnuchanwm: theme %s, icons %s, cursor %s (%d)\n",
            gtk_theme, icon_theme, cursor_theme, cursor_size);
    return 0;
}

const WmModule wm_theme_module = {
    .name = "theme",
    .init = theme_init,
    .event = NULL,
    .cleanup = NULL,
};
