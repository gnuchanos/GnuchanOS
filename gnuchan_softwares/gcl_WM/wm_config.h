#ifndef GNUCHANWM_CONFIG_H
#define GNUCHANWM_CONFIG_H

#include <stddef.h>

typedef struct WmCore WmCore;

#define WM_CONFIG_TEXT_LENGTH 256
#define WM_CONFIG_MAX_BINDINGS 64

typedef struct WmBinding {
    unsigned int modifiers;
    char key[WM_CONFIG_TEXT_LENGTH];
    char action[WM_CONFIG_TEXT_LENGTH];
} WmBinding;

typedef struct WmConfig {
    char background[WM_CONFIG_TEXT_LENGTH];
    char panel[WM_CONFIG_TEXT_LENGTH];
    char panel_edge[WM_CONFIG_TEXT_LENGTH];
    char field[WM_CONFIG_TEXT_LENGTH];
    char text[WM_CONFIG_TEXT_LENGTH];
    char text_muted[WM_CONFIG_TEXT_LENGTH];
    char accent[WM_CONFIG_TEXT_LENGTH];
    char accent_dim[WM_CONFIG_TEXT_LENGTH];
    char active_border[WM_CONFIG_TEXT_LENGTH];
    char inactive_border[WM_CONFIG_TEXT_LENGTH];
    int border_width;

    char terminal[WM_CONFIG_TEXT_LENGTH];
    WmBinding bindings[WM_CONFIG_MAX_BINDINGS];
    int binding_count;

    char gtk_theme[WM_CONFIG_TEXT_LENGTH];
    char icon_theme[WM_CONFIG_TEXT_LENGTH];
    char cursor_theme[WM_CONFIG_TEXT_LENGTH];
    char gtk_path[WM_CONFIG_TEXT_LENGTH];
    char icon_path[WM_CONFIG_TEXT_LENGTH];
    char cursor_path[WM_CONFIG_TEXT_LENGTH];

    char mouse_left[WM_CONFIG_TEXT_LENGTH];
    char mouse_right[WM_CONFIG_TEXT_LENGTH];
    char mouse_middle[WM_CONFIG_TEXT_LENGTH];
    char mouse_scroll_up[WM_CONFIG_TEXT_LENGTH];
    char mouse_scroll_down[WM_CONFIG_TEXT_LENGTH];

    int touchpad_tap_to_click;
    int touchpad_two_finger_scroll;
    int touchpad_three_finger_swipe;
    int touchpad_four_finger_swipe;

    char bar_position[WM_CONFIG_TEXT_LENGTH];
    int bar_size;
    char bar_background[WM_CONFIG_TEXT_LENGTH];
    char bar_background_image[WM_CONFIG_TEXT_LENGTH];
} WmConfig;

void wm_config_defaults(WmConfig *config);
char *wm_config_path(char *buffer, unsigned int size);
int wm_config_load(WmConfig *config, const char *path);
int wm_config_reload(WmCore *core);
void wm_config_apply(WmCore *core);

#endif /* GNUCHANWM_CONFIG_H */
