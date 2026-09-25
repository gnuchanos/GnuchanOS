/*
 * wm_config_file.c — load the user's settings and keep them hot-reloadable.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/keysym.h>

#include "wm_core.h"
#include "wm_config.h"
#include "wm_style.h"

#define WM_CONFIG_PATH_SIZE 4096

static void copy_string(char *dst, unsigned int size, const char *src) {
    if (!dst || size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, size, "%s", src);
}

static int is_true_string(const char *value) {
    if (!value) {
        return 0;
    }
    return strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0 || strcmp(value, "1") == 0;
}

static char *trim_in_place(char *text) {
    if (!text) {
        return text;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    if (text == NULL) {
        return text;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return text;
}

static void extract_quoted_value(const char *text, const char *key,
                                 char *out, unsigned int len) {
    if (!out || len == 0) {
        return;
    }
    out[0] = '\0';
    if (!text || !key) {
        return;
    }
    const char *slot = strstr(text, key);
    if (!slot) {
        return;
    }
    slot += strlen(key);
    while (*slot == ' ' || *slot == '\t' || *slot == '=') {
        slot++;
    }
    if (*slot != '"') {
        return;
    }
    slot++;
    unsigned int i = 0;
    while (*slot && *slot != '"' && i + 1 < len) {
        out[i++] = *slot++;
    }
    out[i] = '\0';
}

static void extract_number_value(const char *text, const char *key, int *out) {
    if (!out) {
        return;
    }
    *out = 0;
    if (!text || !key) {
        return;
    }
    const char *slot = strstr(text, key);
    if (!slot) {
        return;
    }
    slot += strlen(key);
    while (*slot == ' ' || *slot == '\t' || *slot == '=') {
        slot++;
    }
    *out = atoi(slot);
}

static int extract_json_string_value(const char *json, const char *key,
                                     char *out, unsigned int len) {
    if (!json || !key || !out || len == 0) {
        return 0;
    }
    out[0] = '\0';

    const char *slot = strstr(json, key);
    if (!slot) {
        return 0;
    }
    const char *colon = strchr(slot, ':');
    if (!colon) {
        return 0;
    }
    const char *cur = colon + 1;
    while (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
        cur++;
    }
    if (*cur != '"') {
        return 0;
    }
    cur++;
    unsigned int i = 0;
    while (*cur && *cur != '"' && i + 1 < len) {
        out[i++] = *cur++;
    }
    out[i] = '\0';
    return 1;
}

static int extract_json_bool_value(const char *json, const char *key, int *out) {
    if (!json || !key || !out) {
        return 0;
    }
    *out = 0;
    const char *slot = strstr(json, key);
    if (!slot) {
        return 0;
    }
    const char *colon = strchr(slot, ':');
    if (!colon) {
        return 0;
    }
    const char *cur = colon + 1;
    while (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
        cur++;
    }
    if (strncmp(cur, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if (strncmp(cur, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static void extract_bool_value(const char *text, const char *key, int *out) {
    if (!out) {
        return;
    }
    *out = 0;
    if (!text || !key) {
        return;
    }
    const char *slot = strstr(text, key);
    if (!slot) {
        return;
    }
    slot += strlen(key);
    while (*slot == ' ' || *slot == '\t' || *slot == '=') {
        slot++;
    }
    char value[64];
    unsigned int i = 0;
    while (*slot && *slot != ',' && *slot != ')' && *slot != '\n' && *slot != '\r' &&
           i + 1 < sizeof(value)) {
        value[i++] = *slot++;
    }
    value[i] = '\0';
    *out = is_true_string(trim_in_place(value));
}

static int load_runtime_state_snapshot(WmConfig *config, const char *config_path) {
    if (!config || !config_path || !config_path[0]) {
        return -1;
    }

    char state_path[WM_CONFIG_PATH_SIZE];
    snprintf(state_path, sizeof(state_path), "%s", config_path);
    char *dot = strrchr(state_path, '.');
    if (dot) {
        *dot = '\0';
    }
    snprintf(state_path + strlen(state_path), sizeof(state_path) - strlen(state_path),
             ".state.json");

    FILE *file = fopen(state_path, "rb");
    if (!file) {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return -1;
    }
    rewind(file);

    char *json = (char *)malloc((size_t)len + 1);
    if (!json) {
        fclose(file);
        return -1;
    }
    size_t read = fread(json, 1, (size_t)len, file);
    json[read] = '\0';
    fclose(file);

    extract_json_string_value(json, "\"wm.active_border\"", config->active_border,
                              sizeof(config->active_border));
    extract_json_string_value(json, "\"wm.inactive_border\"", config->inactive_border,
                              sizeof(config->inactive_border));
    extract_json_string_value(json, "\"wm.bar\"", config->bar_position,
                              sizeof(config->bar_position));
    extract_json_string_value(json, "\"wm.theme.gtk\"", config->gtk_theme,
                              sizeof(config->gtk_theme));
    extract_json_string_value(json, "\"wm.theme.icon\"", config->icon_theme,
                              sizeof(config->icon_theme));
    extract_json_string_value(json, "\"wm.theme.cursor\"", config->cursor_theme,
                              sizeof(config->cursor_theme));

    /* bar->position and bar->background are nested JSON, so the loader reads the
       direct snapshot from the Python bridge before the generic text scan runs. */
    char nested_bar[4096];
    nested_bar[0] = '\0';
    extract_json_string_value(json, "\"Position\"", nested_bar, sizeof(nested_bar));
    if (nested_bar[0]) {
        copy_string(config->bar_position, sizeof(config->bar_position), nested_bar);
    }
    nested_bar[0] = '\0';
    extract_json_string_value(json, "\"BackgroundColor\"", nested_bar, sizeof(nested_bar));
    if (nested_bar[0]) {
        copy_string(config->bar_background, sizeof(config->bar_background), nested_bar);
    }
    nested_bar[0] = '\0';
    extract_json_string_value(json, "\"BackgroundImage\"", nested_bar, sizeof(nested_bar));
    if (nested_bar[0]) {
        copy_string(config->bar_background_image, sizeof(config->bar_background_image), nested_bar);
    }

    int tap_to_click = 0;
    if (extract_json_bool_value(json, "\"TapToClick\"", &tap_to_click)) {
        config->touchpad_tap_to_click = tap_to_click;
    }
    int two_finger_scroll = 0;
    if (extract_json_bool_value(json, "\"TwoFingerScroll\"", &two_finger_scroll)) {
        config->touchpad_two_finger_scroll = two_finger_scroll;
    }

    free(json);
    return 0;
}

static int apply_python_script(WmConfig *config, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }

    char line[8192];
    while (fgets(line, sizeof(line), file)) {
        char *text = trim_in_place(line);
        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }

        extract_quoted_value(text, "gcl_Window.set_active_window_border_color(",
                             config->active_border, sizeof(config->active_border));
        extract_quoted_value(text, "gcl_Window.set_inactive_window_border_color(",
                             config->inactive_border, sizeof(config->inactive_border));

        if (strstr(text, "default_terminal") != NULL) {
            extract_quoted_value(text, "default_terminal = ", config->terminal,
                                 sizeof(config->terminal));
        }

        if (strstr(text, "gcl_BAR.call") != NULL) {
            extract_quoted_value(text, "Position=", config->bar_position,
                                 sizeof(config->bar_position));
            extract_quoted_value(text, "BackgroundColor=", config->bar_background,
                                 sizeof(config->bar_background));
            extract_quoted_value(text, "BackgroundImage=", config->bar_background_image,
                                 sizeof(config->bar_background_image));
            extract_number_value(text, "Size=", &config->bar_size);
        }

        if (strstr(text, "gcl_themes.Theme_gtk") != NULL) {
            extract_quoted_value(text, "ThemeName=", config->gtk_theme,
                                 sizeof(config->gtk_theme));
            extract_quoted_value(text, "ThemePath=", config->gtk_path,
                                 sizeof(config->gtk_path));
        }
        if (strstr(text, "gcl_themes.Theme_icon") != NULL) {
            extract_quoted_value(text, "ThemeName=", config->icon_theme,
                                 sizeof(config->icon_theme));
            extract_quoted_value(text, "ThemePath=", config->icon_path,
                                 sizeof(config->icon_path));
        }
        if (strstr(text, "gcl_themes.Theme_cursor") != NULL) {
            extract_quoted_value(text, "ThemeName=", config->cursor_theme,
                                 sizeof(config->cursor_theme));
            extract_quoted_value(text, "ThemePath=", config->cursor_path,
                                 sizeof(config->cursor_path));
        }

        if (strstr(text, "gcl_mouse.MouseBehavior") != NULL) {
            extract_quoted_value(text, "LeftClick=", config->mouse_left,
                                 sizeof(config->mouse_left));
            extract_quoted_value(text, "RightClick=", config->mouse_right,
                                 sizeof(config->mouse_right));
            extract_quoted_value(text, "MiddleClick=", config->mouse_middle,
                                 sizeof(config->mouse_middle));
            extract_quoted_value(text, "ScrollUp=", config->mouse_scroll_up,
                                 sizeof(config->mouse_scroll_up));
            extract_quoted_value(text, "ScrollDown=", config->mouse_scroll_down,
                                 sizeof(config->mouse_scroll_down));
        }

        if (strstr(text, "gcl_touchpad.TouchpadBehavior") != NULL) {
            extract_bool_value(text, "TapToClick=", &config->touchpad_tap_to_click);
            extract_bool_value(text, "TwoFingerScroll=", &config->touchpad_two_finger_scroll);
            extract_bool_value(text, "ThreeFingerSwipe=", &config->touchpad_three_finger_swipe);
            extract_bool_value(text, "FourFingerSwipe=", &config->touchpad_four_finger_swipe);
        }
    }

    fclose(file);
    return 0;
}

void wm_config_defaults(WmConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));

    copy_string(config->background, sizeof(config->background), "#1a0b2e");
    copy_string(config->panel, sizeof(config->panel), "#32143f");
    copy_string(config->panel_edge, sizeof(config->panel_edge), "#7b2cbf");
    copy_string(config->field, sizeof(config->field), "#241033");
    copy_string(config->text, sizeof(config->text), "#e0c3fc");
    copy_string(config->text_muted, sizeof(config->text_muted), "#9d7bba");
    copy_string(config->accent, sizeof(config->accent), "#c77dff");
    copy_string(config->accent_dim, sizeof(config->accent_dim), "#7b2cbf");
    copy_string(config->active_border, sizeof(config->active_border), "#c77dff");
    copy_string(config->inactive_border, sizeof(config->inactive_border), "#32143f");
    config->border_width = 2;

    copy_string(config->terminal, sizeof(config->terminal), "xterm");
    config->binding_count = 0;

    copy_string(config->gtk_theme, sizeof(config->gtk_theme), "GnuChanTheme");
    copy_string(config->icon_theme, sizeof(config->icon_theme), "GnuChanIconTheme");
    copy_string(config->cursor_theme, sizeof(config->cursor_theme), "GnuChanCursorTheme");
    config->gtk_path[0] = '\0';
    config->icon_path[0] = '\0';
    config->cursor_path[0] = '\0';

    copy_string(config->mouse_left, sizeof(config->mouse_left), "select");
    copy_string(config->mouse_right, sizeof(config->mouse_right), "context_menu");
    copy_string(config->mouse_middle, sizeof(config->mouse_middle), "paste");
    copy_string(config->mouse_scroll_up, sizeof(config->mouse_scroll_up), "scroll_up");
    copy_string(config->mouse_scroll_down, sizeof(config->mouse_scroll_down), "scroll_down");

    config->touchpad_tap_to_click = 1;
    config->touchpad_two_finger_scroll = 1;
    config->touchpad_three_finger_swipe = 0;
    config->touchpad_four_finger_swipe = 0;

    copy_string(config->bar_position, sizeof(config->bar_position), "top");
    config->bar_size = 24;
    copy_string(config->bar_background, sizeof(config->bar_background), "#27022b");
    config->bar_background_image[0] = '\0';
}

char *wm_config_path(char *buffer, unsigned int size) {
    if (!buffer || size == 0) {
        return buffer;
    }
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanWM/GnuChanWM.py", xdg);
        return buffer;
    }
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanWM/GnuChanWM.py", home);
        return buffer;
    }

    buffer[0] = '\0';
    return buffer;
}

int wm_config_load(WmConfig *config, const char *path) {
    if (!config) {
        return -1;
    }

    char resolved[WM_CONFIG_PATH_SIZE];
    const char *source = path;
    if (!source || source[0] == '\0') {
        wm_config_path(resolved, sizeof(resolved));
        source = resolved;
    }
    if (!source || source[0] == '\0') {
        return -1;
    }

    FILE *probe = fopen(source, "r");
    if (!probe) {
        return -1;
    }
    fclose(probe);

    if (apply_python_script(config, source) != 0) {
        return -1;
    }
    if (load_runtime_state_snapshot(config, source) == 0) {
        return 0;
    }
    return 0;
}

void wm_config_apply(WmCore *core) {
    if (!core || !core->display) {
        return;
    }

    core->style.background = wm_style_colour(core->display, core->screen,
                                             core->config.background, BlackPixel(core->display, core->screen));
    core->style.panel = wm_style_colour(core->display, core->screen,
                                        core->config.panel, BlackPixel(core->display, core->screen));
    core->style.panel_edge = wm_style_colour(core->display, core->screen,
                                             core->config.panel_edge, WhitePixel(core->display, core->screen));
    core->style.field = wm_style_colour(core->display, core->screen,
                                        core->config.field, BlackPixel(core->display, core->screen));
    core->style.text = wm_style_colour(core->display, core->screen,
                                       core->config.text, WhitePixel(core->display, core->screen));
    core->style.text_muted = wm_style_colour(core->display, core->screen,
                                            core->config.text_muted, WhitePixel(core->display, core->screen));
    core->style.accent = wm_style_colour(core->display, core->screen,
                                         core->config.accent, WhitePixel(core->display, core->screen));
    core->style.accent_dim = wm_style_colour(core->display, core->screen,
                                             core->config.accent_dim, WhitePixel(core->display, core->screen));
    core->style.border = wm_style_colour(core->display, core->screen,
                                         core->config.active_border, WhitePixel(core->display, core->screen));
    core->style.border_unfocused = wm_style_colour(core->display, core->screen,
                                                  core->config.inactive_border, BlackPixel(core->display, core->screen));
    core->style.border_width = core->config.border_width > 0 ? core->config.border_width : 2;

    if (core->config.gtk_theme[0]) {
        setenv("GTK_THEME", core->config.gtk_theme, 1);
    }
    if (core->config.cursor_theme[0]) {
        setenv("XCURSOR_THEME", core->config.cursor_theme, 1);
    }
}

static int execute_python_config(const char *path) {
    if (!path || !path[0]) {
        return -1;
    }

    char command[WM_CONFIG_PATH_SIZE * 2];
    snprintf(command, sizeof(command), "gcl -pyrun \"%s\" >/dev/null 2>&1", path);
    int rc = system(command);
    if (rc != 0) {
        fprintf(stderr, "gnuchanwm: python config failed for %s (rc=%d)\n", path, rc);
        return -1;
    }
    fprintf(stderr, "gnuchanwm: python config executed from %s\n", path);
    return 0;
}

static int config_init(WmCore *core) {
    if (!core) {
        return -1;
    }
    wm_config_defaults(&core->config);

    char path[WM_CONFIG_PATH_SIZE];
    wm_config_path(path, sizeof(path));
    if (path[0]) {
        if (execute_python_config(path) != 0) {
            fprintf(stderr, "gnuchanwm: config not applied from %s; defaults in use\n", path);
        }
    }

    wm_config_apply(core);
    return 0;
}

static void config_cleanup(WmCore *core) {
    (void)core;
}

const WmModule wm_config_module = {
    .name = "config",
    .init = config_init,
    .event = NULL,
    .cleanup = config_cleanup,
};

int wm_config_reload(WmCore *core) {
    if (!core) {
        return -1;
    }

    char path[WM_CONFIG_PATH_SIZE];
    wm_config_path(path, sizeof(path));
    if (!path[0]) {
        fprintf(stderr, "gnuchanwm: no config path available for reload\n");
        return -1;
    }

    if (execute_python_config(path) == 0) {
        fprintf(stderr, "gnuchanwm: config reloaded from %s\n", path);
        return 0;
    }

    fprintf(stderr, "gnuchanwm: config reload failed; defaults restored\n");
    return -1;
}
