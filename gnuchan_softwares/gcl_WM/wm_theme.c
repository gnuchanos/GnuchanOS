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
#include <sys/wait.h>
#include <fcntl.h>
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

/* The keys this module owns in the resource manager. Everything else in the
   property is somebody else's — xterm's faceName, Xft's rendering, whatever a
   login script put there with xrdb — and must survive a write from here. */
static const char *const MANAGED_RESOURCE_KEYS[] = {
    "Xcursor.theme",
    "Xcursor.size",
    "Xft.dpi",
};
#define MANAGED_RESOURCE_KEY_COUNT \
    ((int)(sizeof(MANAGED_RESOURCE_KEYS) / sizeof(MANAGED_RESOURCE_KEYS[0])))

/* Whether a line of an X resource file sets one of the keys managed here, so
   the old value of it can be dropped before the new one is added. */
static int resource_line_is_managed(const char *line, unsigned int length) {
    for (int i = 0; i < MANAGED_RESOURCE_KEY_COUNT; i++) {
        size_t key_length = strlen(MANAGED_RESOURCE_KEYS[i]);
        if (length < key_length) {
            continue;
        }
        if (strncmp(line, MANAGED_RESOURCE_KEYS[i], key_length) == 0) {
            unsigned int after = (unsigned int)key_length;
            if (after < length &&
                (line[after] == ':' || line[after] == '\t' ||
                 line[after] == ' ' || line[after] == '=')) {
                return 1;
            }
        }
    }
    return 0;
}

/* Read the resource manager property as it stands, into a freshly allocated
   string. NULL when the property is unset, which is a display where nothing
   has run xrdb yet. */
static char *read_resource_manager(WmCore *core, Atom resources_atom) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(core->display, core->root, resources_atom,
                           0, 256 * 1024, False, XA_STRING, &actual_type,
                           &actual_format, &items, &after, &data) != Success) {
        return NULL;
    }
    if (!data) {
        return NULL;
    }
    if (actual_type != XA_STRING || actual_format != 8 || items == 0) {
        XFree(data);
        return NULL;
    }
    char *text = malloc(items + 1);
    if (text) {
        memcpy(text, data, items);
        text[items] = '\0';
    }
    XFree(data);
    return text;
}

/* Write the resource manager, keeping what is already in it.
 *
 * This used to replace the whole property, and that is what broke xterm: the
 * terminal's own resources are loaded into this same property by `xrdb -merge
 * ~/.Xresources`, so writing the three lines below erased every one of them —
 * the font, the colours, the scrollbar — and a terminal opened afterwards came
 * up as the stripped default the reset reports. The property is a shared file
 * of settings, not this module's private state, so it is merged: the lines
 * that set a key owned here are dropped and the new ones appended, and every
 * other line is left exactly as it was. */
static void publish_to_resource_manager(WmCore *core, const char *cursor_theme,
                                        int cursor_size) {
    char ours[256];
    snprintf(ours, sizeof(ours),
             "Xcursor.theme:\t%s\n"
             "Xcursor.size:\t%d\n"
             "Xft.dpi:\t96\n",
             cursor_theme, cursor_size);

    Atom resources_atom = wm_atom(core, "RESOURCE_MANAGER");
    char *existing = read_resource_manager(core, resources_atom);

    /* Room for what was there plus what is added. */
    size_t existing_length = existing ? strlen(existing) : 0;
    char *merged = malloc(existing_length + sizeof(ours) + 1);
    if (!merged) {
        free(existing);
        return;
    }

    size_t merged_length = 0;
    if (existing) {
        const char *line = existing;
        while (*line) {
            const char *end = strchr(line, '\n');
            size_t length = end ? (size_t)(end - line) : strlen(line);
            if (!resource_line_is_managed(line, (unsigned int)length)) {
                memcpy(merged + merged_length, line, length);
                merged_length += length;
                merged[merged_length++] = '\n';
            }
            if (!end) {
                break;
            }
            line = end + 1;
        }
    }
    memcpy(merged + merged_length, ours, strlen(ours));
    merged_length += strlen(ours);
    merged[merged_length] = '\0';

    XChangeProperty(core->display, core->root, resources_atom, XA_STRING, 8,
                    PropModeReplace, (unsigned char *)merged,
                    (int)merged_length);
    XFlush(core->display);

    free(merged);
    free(existing);
}

/* Load the user's own resource file into the running server.
 *
 * ~/.Xresources is where a person's terminal settings live, and the login
 * hooks that load it (dotfile/XTERM writes ~/.xsessionrc and ~/.xprofile) are
 * sourced by a session that goes through a shell. A session whose window
 * manager is started directly by a display manager sources neither, so the
 * file would never reach the server and the terminal would come up as the
 * bare default — the same symptom as the overwrite above, from the other
 * direction. Running `xrdb -merge` here is what makes the file apply whatever
 * started the session. It is silent on failure: a machine with no xrdb, or no
 * file, is a machine whose terminal simply uses the server's current answer,
 * and that is not a reason to stop a session starting. */
static void load_user_resources(void) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return;
    }
    char path[4096];
    snprintf(path, sizeof(path), "%s/.Xresources", home);

    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return;
    }
    if (pid == 0) {
        /* The child's noise is the server's, not the session's: a missing
           xrdb or a warning about one line is not what a login log is for. */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        char *argv[] = { "xrdb", "-merge", path, NULL };
        execvp(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        /* A signal before the child finished is not a reason to leave it
           unreaped; the loop retries the wait. */
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        fprintf(stderr, "gnuchanwm: loaded %s with xrdb\n", path);
    }
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

void wm_theme_apply(WmCore *core) {
    if (!core) {
        return;
    }
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

    /* ~/.Xresources first, then this module's own three lines on top. The
       order matters: xrdb -merge replaces the values for the keys it sets, so
       loading the file before publishing means a cursor named here wins over
       one the file happened to name, while every other setting the file holds
       — the terminal's font and colours — is left in place. */
    load_user_resources();
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
}

/* The module's init is the first apply. A later apply is a reload asking for
   the same work again, which is why the body is a function of its own: the
   reload has to reach the GTK settings files and the resource manager, not
   only the environment the first run also set. */
static int theme_init(WmCore *core) {
    wm_theme_apply(core);
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
