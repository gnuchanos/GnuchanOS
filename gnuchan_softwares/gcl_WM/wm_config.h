/*
 * wm_config.h — the settings the desktop is built from.
 *
 * GnuChanWM is configured by a script, not by a file of key = value pairs:
 * the same file a user would write to move the bar, recolour a window border
 * or bind a key. That script is Python, and this header is the shape of what
 * running it produces — the small, fixed set of things the desktop can be
 * asked for.
 *
 * Everything here is plain data. The script is read by wm_config_parser.c,
 * walked by wm_config_file.c, and drawn by whichever module owns the thing
 * being described: the bar by wm_desktop.c, the window border by wm_frame.c
 * through the style wm_config_apply() fills in. Nothing in this header knows
 * about X.
 */
#ifndef GNUCHANWM_CONFIG_H
#define GNUCHANWM_CONFIG_H

#include <stddef.h>

/* WmCore is only ever pointed at here, never inspected — the struct itself
   is defined in wm_core.h, which includes this one. */
typedef struct WmCore WmCore;

/* A written value: a colour, a command, a font name. */
#define WM_CONFIG_TEXT_LENGTH 256

/* What a single config file may ask for. */
#define WM_CONFIG_MAX_WIDGETS  64
#define WM_CONFIG_MAX_BINDINGS 64

/* The kinds of widget a bar can hold — one per gcl_Widgets.X the script can
   name. Adding a widget kind is adding one enumerator and one branch in the
   drawing code. */
typedef enum WmWidgetKind {
    WM_WIDGET_CURRENT_LAYOUT,   /* the numbered layouts, the current one lit */
    WM_WIDGET_GROUP_BOX,        /* a separator drawn as a symbol            */
    WM_WIDGET_EMPTY_SPACE,      /* flexible space; shares what is left over */
    WM_WIDGET_TEXT_BOX,         /* fixed text                               */
    WM_WIDGET_CLOCK,            /* the time, in a strftime format           */
} WmWidgetKind;

/* One widget, as written. The colours are text, not pixels, because a colour
   name only becomes a pixel once there is a display to allocate it on — the
   same reason WmStyle holds text until it is loaded. */
typedef struct WmWidget {
    WmWidgetKind kind;

    char background[WM_CONFIG_TEXT_LENGTH];
    char foreground[WM_CONFIG_TEXT_LENGTH];
    int  font_size;
    char font_family[WM_CONFIG_TEXT_LENGTH];

    /* WM_WIDGET_CURRENT_LAYOUT: the range of layouts, as written. The desktop
       has WM_WORKSPACE_COUNT workspaces (wm_workspace.h) and switches between
       them, but the widget still draws the whole range as labels rather than
       marking the one that is current — the picture the user wrote, not yet
       the state of the desktop. */
    int start_layout;
    int end_layout;

    char symbol[WM_CONFIG_TEXT_LENGTH];   /* WM_WIDGET_GROUP_BOX */
    char text[WM_CONFIG_TEXT_LENGTH];     /* WM_WIDGET_TEXT_BOX  */
    char format[WM_CONFIG_TEXT_LENGTH];   /* WM_WIDGET_CLOCK     */
} WmWidget;

/* The bar. A config may omit it; `present` is then 0 and no bar is drawn. */
typedef struct WmBar {
    int  present;
    char position[WM_CONFIG_TEXT_LENGTH];         /* "top" or "bottom" */
    int  size;
    char background[WM_CONFIG_TEXT_LENGTH];
    char background_image[WM_CONFIG_TEXT_LENGTH]; /* empty: use background */

    WmWidget widgets[WM_CONFIG_MAX_WIDGETS];
    int widget_count;
} WmBar;

/* One key binding. The script writes the keys as a list of names —
   `keys=[super_key1, "return"]` — which the config module resolves to a
   modifier and a key before this is filled in. */
typedef struct WmBinding {
    unsigned int modifiers;                  /* Mod1Mask, ControlMask, ... */
    char key[WM_CONFIG_TEXT_LENGTH];         /* an X keysym name           */
    char action[WM_CONFIG_TEXT_LENGTH];      /* the GCL action to run      */
} WmBinding;

typedef struct WmConfig {
    /* The frame's colours: the two things the script sets through gcl_Window,
       because a window border is drawn before anything else is on screen. */
    char active_border[WM_CONFIG_TEXT_LENGTH];
    char inactive_border[WM_CONFIG_TEXT_LENGTH];
    int border_width;

    /* What Alt+Enter opens. Empty means "look at $TERMINAL, then at the usual
       terminals", which is what a machine with no config gets. */
    char terminal[WM_CONFIG_TEXT_LENGTH];

    WmBinding bindings[WM_CONFIG_MAX_BINDINGS];
    int binding_count;

    /* Themes, published by wm_theme.c. */
    char gtk_theme[WM_CONFIG_TEXT_LENGTH];
    char gtk_path[WM_CONFIG_TEXT_LENGTH];
    char icon_theme[WM_CONFIG_TEXT_LENGTH];
    char icon_path[WM_CONFIG_TEXT_LENGTH];
    char cursor_theme[WM_CONFIG_TEXT_LENGTH];
    char cursor_path[WM_CONFIG_TEXT_LENGTH];

    /* The pointer. Parsed and held, but no C subsystem applies them: the
       desktop answers its own right-click with the menu in wm_menu.c, and the
       rest is left to whatever the programs under the pointer do with the
       button. They are the script's record of intent until a module reads
       them. */
    char mouse_left[WM_CONFIG_TEXT_LENGTH];
    char mouse_right[WM_CONFIG_TEXT_LENGTH];
    char mouse_middle[WM_CONFIG_TEXT_LENGTH];
    char mouse_scroll_up[WM_CONFIG_TEXT_LENGTH];
    char mouse_scroll_down[WM_CONFIG_TEXT_LENGTH];

    int touchpad_tap_to_click;
    int touchpad_two_finger_scroll;
    int touchpad_three_finger_swipe;
    int touchpad_four_finger_swipe;

    WmBar bar;
} WmConfig;

/* The desktop the code had before it was configurable: the palette in
   wm_style.c, Alt+Enter, and the bar from the shipped script. A machine whose
   config is missing or broken gets exactly this. */
void wm_config_defaults(WmConfig *config);

/* The four borders of the desktop. */
enum {
    WM_EDGE_TOP    = 0,
    WM_EDGE_BOTTOM = 1,
    WM_EDGE_LEFT   = 2,
    WM_EDGE_RIGHT  = 3,
};

/* The edges the bar occupies, one bit per WM_EDGE_*. Empty when no bar is
   configured. This is what the frame code asks before placing a window, so a
   window does not open underneath the bar. */
unsigned int wm_config_bar_edges(const WmConfig *config);

/* The rectangle a window may occupy: the whole screen less whatever strip the
   bar has taken. `x` and `y` are where a window may be put and `width` and
   `height` how large it may be, both already reduced for a top or bottom bar.
   Called by the frame code both when a window opens and when one is maximised,
   so a bar is never covered by the thing it is meant to sit above. */
void wm_config_workarea(const WmConfig *config, int screen_width,
                        int screen_height, int *x, int *y,
                        int *width, int *height);

/* Where the script is looked for: $XDG_CONFIG_HOME/GnuChanWM/GnuChanWM.py, or
   ~/.config/GnuChanWM/GnuChanWM.py. Written into buffer, which is returned. */
char *wm_config_path(char *buffer, unsigned int size);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed far enough to
   trust — a half-read config is not applied. */
int wm_config_load(WmConfig *config, const char *path);

/* Re-read the script if it has changed since the last read, and repaint if it
   has. Called from the loop's idle tick, so editing the script is enough to
   change the desktop — no key, no logout. Returns 1 when a reload happened. */
int wm_config_reload(WmCore *core);

/* Push the palette out of config into the desktop's style, the theme names
   into the environment, and the pointer onto the desktop. */
void wm_config_apply(WmCore *core);

#endif /* GNUCHANWM_CONFIG_H */
