/*
 * wm_theme.c — make the session's theme the one the config asked for.
 *
 * There are three places a program looks for "which theme is this session
 * using", and a session that fills in only one of them gets windows that
 * disagree with each other. This fills in all three:
 *
 *   1. The X resource manager (the RESOURCE_MANAGER property on the root).
 *      This is where X itself and every program without a toolkit look, and
 *      it is the only place the cursor is named. The cursor's theme and size
 *      are read by Xlib the first time a client asks for a cursor.
 *
 *   2. The GTK settings files (~/.config/gtk-3.0/settings.ini and gtk-4.0).
 *      This is where GTK looks when a program starts, and the one place a
 *      theme and an icon set can be named without a toolkit running to hear
 *      about it.
 *
 *   3. The environment. GTK_THEME, XCURSOR_THEME and XCURSOR_SIZE are read by
 *      a program before its own settings files, and are what the WM passes on
 *      to everything it starts.
 *
 * The names come from the settings script, which may also name a path each
 * theme was installed from. A path is what a machine that built its own theme
 * has and a packaged one does not, so it is used when the name was not found
 * where themes normally live: the theme is linked into the user's own theme
 * directory from where the script says it is, and then the name resolves like
 * any other.
 *
 * A name is only published when the theme is actually there. Naming a theme
 * that is not installed draws every window in the toolkit's built-in grey,
 * which looks less like a theme was chosen and more like one was lost.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_theme.h"

/* The themes GnuchanOS ships, and what to fall back to when they are not
   installed. The fallback is Adwaita because every GTK installation has it —
   a machine without it has no GTK theme at all and is beyond what a name can
   fix. */
/* The names the installers in dotfile/ actually use. They are read by
   theme_installed() as the last resort before the fallback, so a machine that
   has run those installers is found here even when the settings script names
   nothing. An icon theme and a cursor theme are both read from
   ~/.local/share/icons/<name>, which is why the two names differ. */
#define GNUCHAN_GTK_THEME     "GnuChanTheme"
#define GNUCHAN_ICON_THEME    "GnuChanIcon"
#define GNUCHAN_CURSOR_THEME  "GnuChanMouseIcons"
#define FALLBACK_GTK_THEME    "Adwaita"
#define FALLBACK_ICON_THEME   "Adwaita"
#define FALLBACK_CURSOR_THEME "Adwaita"

#define CURSOR_SIZE_DEFAULT   24

static int is_directory(const char *path) {
    struct stat info;
    return path && path[0] && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
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
    if (!name || !name[0]) {
        return 0;
    }
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

/* Link a theme that was installed somewhere unusual into the place the toolkit
   looks. The script names both the theme and where it was installed from; when
   the name does not resolve on its own — which is what happens for a theme
   built in a home directory rather than packaged — the name is made to resolve
   by a symbolic link from the user's own theme directory.
 *
 * A link is used rather than a copy so that rebuilding the theme is seen at
 * once and two copies of it never disagree. An existing link is left alone:
 * it is either the one this made on an earlier session or one the user made. */
static void link_theme(const char *source, const char *name,
                       const char *kind, const char *extension) {
    if (!is_directory(source) || !name || !name[0]) {
        return;
    }
    if (theme_installed(name)) {
        return;
    }

    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return;
    }

    char parent[4096];
    snprintf(parent, sizeof(parent), "%s/.local/share/%s", home, kind);

    char destination[4096 + 256];
    snprintf(destination, sizeof(destination), "%s/%s%s", parent, name,
             extension);

    struct stat info;
    if (lstat(destination, &info) == 0) {
        return;
    }

    make_directories(parent);
    if (symlink(source, destination) == 0) {
        fprintf(stderr, "gnuchanwm: linked %s -> %s\n", destination, source);
    }
}

/* The first installed name from the script, then the environment, then the
   shipped theme, then the fallback. The script comes first because it is the
   desktop's own answer; the environment is next so a session entry can still
   override it without the script being edited. */
static const char *choose_theme(const char *from_config, const char *variable,
                                const char *shipped, const char *fallback) {
    if (theme_installed(from_config)) {
        return from_config;
    }
    const char *environment = getenv(variable);
    if (theme_installed(environment)) {
        return environment;
    }
    if (theme_installed(shipped)) {
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
    XChangeProperty(core->display, core->root, resources_atom, XA_STRING, 8,
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
    WmConfig *config = &core->config;

    /* A theme the script named but the system does not have is linked in from
       where the script says it lives, so that the name it asked for is the
       name it gets. Done before the names are chosen, because choosing is
       what looks them up. */
    link_theme(config->gtk_path, config->gtk_theme, "themes", "");
    link_theme(config->icon_path, config->icon_theme, "icons", "");
    link_theme(config->cursor_path, config->cursor_theme, "icons", "");

    const char *gtk_theme = choose_theme(config->gtk_theme, "GTK_THEME",
                                         GNUCHAN_GTK_THEME,
                                         FALLBACK_GTK_THEME);
    const char *icon_theme = choose_theme(config->icon_theme, "GTK_ICON_THEME",
                                          GNUCHAN_ICON_THEME,
                                          FALLBACK_ICON_THEME);
    const char *cursor_theme = choose_theme(config->cursor_theme,
                                            "XCURSOR_THEME",
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
    setenv("GTK_ICON_THEME", icon_theme, 1);
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
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};
