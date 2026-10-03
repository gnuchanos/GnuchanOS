/*
 * dock_theme.c — a window's class, followed to a picture on disk.
 *
 * The road has three steps and no more:
 *
 *   class -> .desktop -> Icon name -> a file in the icon theme
 *
 * The first is a scan of the applications directories for the file that names
 * the class. A file names it in StartupWMClass, or — when it names it nowhere,
 * which is the case for the GnuchanOS programs — by being called after it:
 * gnuchanterm.desktop is the file for the class GnuChanTerm. Only the
 * [Desktop Entry] heading is read, the same rule runner_desktop.c follows.
 *
 * The second is the Icon= key, which is a name and not a path: "utilities-
 * terminal", not a file. The third turns that name into a file by walking the
 * icon theme directories, taking the size nearest the one the dock asked for,
 * and preferring a bitmap to a vector because Imlib2 reads a PNG without a
 * plugin and an SVG only with one. A name that is already a path — Icon=
 * writing an absolute one — is used as it is.
 *
 * Everything here is bounded and read once per window, which is once per item
 * in the dock, so nothing is cached and nothing grows.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "dock_theme.h"

/* A picture's extension, tried in this order: a bitmap first, a vector last. */
static const char *const EXTENSIONS[] = { "png", "xpm", "svg" };
#define EXTENSION_COUNT 3

/* No size is nearer than this, so it stands for "nothing found". */
#define NO_MATCH (1 << 30)

static int readable(const char *path) {
    return path && path[0] && access(path, R_OK) == 0;
}

/* The path with `ext` appended, tried; `out` is touched only when the file is
   really there, so a caller that fails is left with what it had. */
static int check_file(const char *directory, const char *name,
                      const char *extension, char *out, unsigned int size) {
    char candidate[DOCK_THEME_TEXT * 2];
    snprintf(candidate, sizeof(candidate), "%s/%s.%s", directory, name,
             extension);
    if (!readable(candidate)) {
        return 0;
    }
    snprintf(out, size, "%s", candidate);
    return 1;
}

/* The side a theme's size directory names — "48x48" is 48 — or 0 when the
   directory is not one. "scalable" holds vectors and is given a large side so
   it is a last resort rather than a first. */
static int size_directory_side(const char *name) {
    int first = 0;
    int second = 0;
    if (sscanf(name, "%dx%d", &first, &second) == 2 && first == second &&
        first > 0) {
        return first;
    }
    if (strcmp(name, "scalable") == 0) {
        return 256;
    }
    return 0;
}

/*--- the icon name a class names ------------------------------------------ */

/* Read the two keys this needs — StartupWMClass and Icon — out of one
   [Desktop Entry]. The last of either key wins, as the specification says. */
static void parse_entry(const char *path, char *wm_class,
                        unsigned int class_size, char *icon,
                        unsigned int icon_size) {
    wm_class[0] = '\0';
    icon[0] = '\0';

    FILE *file = fopen(path, "rb");
    if (!file) {
        return;
    }
    int in_entry = 0;
    char line[DOCK_THEME_TEXT * 2];
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' ||
                              line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            in_entry = strncmp(line, "[Desktop Entry]", 15) == 0;
            continue;
        }
        if (!in_entry) {
            continue;
        }
        char *equals = strchr(line, '=');
        if (!equals) {
            continue;
        }
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        if (strcmp(key, "StartupWMClass") == 0) {
            snprintf(wm_class, class_size, "%s", value);
        } else if (strcmp(key, "Icon") == 0) {
            snprintf(icon, icon_size, "%s", value);
        }
    }
    fclose(file);
}

/* The file name without its directory and its ".desktop". The name is what a
   file that writes no StartupWMClass is matched by. */
static void desktop_stem(const char *path, char *out, unsigned int size) {
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    snprintf(out, size, "%s", name);
    char *dot = strrchr(out, '.');
    if (dot && strcmp(dot, ".desktop") == 0) {
        *dot = '\0';
    }
}

static int names_class(const char *value, const char *class_name) {
    return value && value[0] && strcasecmp(value, class_name) == 0;
}

/* Walk one applications directory; return 1 the moment a file names the class,
   with its Icon= written into `icon`. */
static int scan_applications(const char *directory, const char *class_name,
                             char *icon, unsigned int icon_size) {
    DIR *dir = opendir(directory);
    if (!dir) {
        return 0;
    }
    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        size_t length = strlen(entry->d_name);
        if (length <= 8 ||
            strcmp(entry->d_name + length - 8, ".desktop") != 0) {
            continue;
        }
        char full[DOCK_THEME_TEXT * 2];
        snprintf(full, sizeof(full), "%s/%s", directory, entry->d_name);

        char wm_class[DOCK_THEME_TEXT];
        char icon_name[DOCK_THEME_TEXT];
        parse_entry(full, wm_class, sizeof(wm_class), icon_name,
                    sizeof(icon_name));
        if (!icon_name[0]) {
            continue;
        }

        char stem[DOCK_THEME_TEXT];
        desktop_stem(full, stem, sizeof(stem));

        if (names_class(wm_class, class_name) ||
            names_class(stem, class_name)) {
            snprintf(icon, icon_size, "%s", icon_name);
            found = 1;
        }
    }
    closedir(dir);
    return found;
}

static int icon_name_for_class(const char *class_name, char *out,
                               unsigned int size) {
    const char *bases[8];
    int count = 0;
    char home_apps[DOCK_THEME_TEXT];
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(home_apps, sizeof(home_apps),
                 "%s/.local/share/applications", home);
        bases[count++] = home_apps;
    }
    bases[count++] = "/usr/local/share/applications";
    bases[count++] = "/usr/share/applications";

    for (int i = 0; i < count; i++) {
        if (scan_applications(bases[i], class_name, out, size)) {
            return 1;
        }
    }
    return 0;
}

/*--- the file an icon name names ------------------------------------------ */

/* One theme's size directory: the picture whose side is nearest `want` wins,
   and among equal sides the first found does. */
static void consider(const char *apps_directory, const char *name, int want,
                     int side, char *out, unsigned int size, int *best) {
    int difference = side > want ? side - want : want - side;
    if (difference >= *best) {
        return;
    }
    for (int i = 0; i < EXTENSION_COUNT; i++) {
        if (check_file(apps_directory, name, EXTENSIONS[i], out, size)) {
            *best = difference;
            return;
        }
    }
}

/* Every theme under one icon root, every size each theme holds. */
static void scan_icon_root(const char *root, const char *name, int want,
                           char *out, unsigned int size, int *best) {
    DIR *themes = opendir(root);
    if (!themes) {
        return;
    }
    struct dirent *theme;
    while ((theme = readdir(themes)) != NULL) {
        if (theme->d_name[0] == '.') {
            continue;
        }
        char theme_dir[DOCK_THEME_TEXT * 2];
        snprintf(theme_dir, sizeof(theme_dir), "%s/%s", root, theme->d_name);

        DIR *sizes = opendir(theme_dir);
        if (!sizes) {
            continue;
        }
        struct dirent *entry;
        while ((entry = readdir(sizes)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }
            int side = size_directory_side(entry->d_name);
            if (side <= 0) {
                continue;
            }
            char apps[DOCK_THEME_TEXT * 2];
            snprintf(apps, sizeof(apps), "%s/%s/apps", theme_dir,
                     entry->d_name);
            consider(apps, name, want, side, out, size, best);
        }
        closedir(sizes);
    }
    closedir(themes);
}

/* The flat directories a name may sit in whole, which is where a hand-installed
   picture lands. */
static int find_in_pixmaps(const char *name, char *out, unsigned int size) {
    const char *bases[8];
    int count = 0;
    char home_pixmaps[DOCK_THEME_TEXT];
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(home_pixmaps, sizeof(home_pixmaps),
                 "%s/.local/share/pixmaps", home);
        bases[count++] = home_pixmaps;
    }
    bases[count++] = "/usr/local/share/pixmaps";
    bases[count++] = "/usr/share/pixmaps";

    for (int i = 0; i < count; i++) {
        for (int e = 0; e < EXTENSION_COUNT; e++) {
            if (check_file(bases[i], name, EXTENSIONS[e], out, size)) {
                return 1;
            }
        }
    }
    return 0;
}

static int resolve_icon(const char *name, int want, char *out,
                        unsigned int size) {
    if (name[0] == '/') {
        if (!readable(name)) {
            return 0;
        }
        snprintf(out, size, "%s", name);
        return 1;
    }
    if (find_in_pixmaps(name, out, size)) {
        return 1;
    }

    const char *roots[8];
    int count = 0;
    char home_icons[DOCK_THEME_TEXT];
    char home_dot_icons[DOCK_THEME_TEXT];
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(home_icons, sizeof(home_icons), "%s/.local/share/icons",
                 home);
        roots[count++] = home_icons;
        snprintf(home_dot_icons, sizeof(home_dot_icons), "%s/.icons", home);
        roots[count++] = home_dot_icons;
    }
    roots[count++] = "/usr/local/share/icons";
    roots[count++] = "/usr/share/icons";

    int best = NO_MATCH;
    for (int i = 0; i < count; i++) {
        scan_icon_root(roots[i], name, want, out, size, &best);
    }
    return best < NO_MATCH;
}

int dock_theme_icon_for_class(const char *wm_class, int size, char *out,
                              unsigned int out_size) {
    if (!wm_class || !wm_class[0] || !out || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char icon_name[DOCK_THEME_TEXT];
    if (!icon_name_for_class(wm_class, icon_name, sizeof(icon_name))) {
        return 0;
    }
    if (size <= 0) {
        size = 48;
    }
    return resolve_icon(icon_name, size, out, out_size);
}
