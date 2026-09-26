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

    /* WM_WIDGET_CURRENT_LAYOUT: the range of workspaces, as written. The
       highest number any layout widget names is what decides how many
       workspaces the session has (see WmConfig.workspace_count), and the bar
       draws the range with the current one lit. */
    int start_layout;
    int end_layout;

    /* WM_WIDGET_CURRENT_LAYOUT: the room between two workspace cells, as
       written through Gap. Zero or less means "not set", and the bar uses its
       own default spacing — the value every other widget is laid out with — so
       a script that never mentions Gap gets the bar it always had rather than
       a row of cells with no room between them. */
    int gap;

    /* WM_WIDGET_EMPTY_SPACE: what the space is for.
     *
     * Expanding (the default) means "take a share of whatever room the fixed
     * widgets left" — that is how a clock is pushed to the right edge, and two
     * of them split the room evenly. Expanding=0 makes the widget a spacer of
     * exactly `horizontal` pixels instead, which is how a small fixed gap is
     * written between two widgets: `EmptySpace(Expanding=False, Horizontal=8)`
     * is eight pixels and nothing else.
     *
     * Horizontal is in pixels and only read when Expanding is off; a value of
     * zero or less is taken as one, because a zero-width widget is a widget
     * nobody asked for and a negative one cannot be drawn. */
    int expanding;
    int horizontal;

    char symbol[WM_CONFIG_TEXT_LENGTH];   /* WM_WIDGET_GROUP_BOX */
    char text[WM_CONFIG_TEXT_LENGTH];     /* WM_WIDGET_TEXT_BOX  */
    char format[WM_CONFIG_TEXT_LENGTH];   /* WM_WIDGET_CLOCK     */

    /* WM_WIDGET_GROUP_BOX: the mark drawn between two open windows' icons,
       and the colour to draw it in. `Seperator="|"` puts the character between
       every pair of icons; an empty separator draws nothing, which is what a
       bar that wants the icons hard against each other asks for. The colour is
       the widget's own foreground when the script named none. */
    char separator[WM_CONFIG_TEXT_LENGTH];
    char separator_color[WM_CONFIG_TEXT_LENGTH];
} WmWidget;

/* The bar. A config may omit it; `present` is then 0 and no bar is drawn. */
typedef struct WmBar {
    int  present;
    char position[WM_CONFIG_TEXT_LENGTH];         /* "top" or "bottom" */
    int  size;
    char background[WM_CONFIG_TEXT_LENGTH];

    /* Draw the bar off-screen and put it up in one operation, rather than
       painting it straight onto its window.
     *
     * It is on by default because a bar painted straight to its window is seen
     * half-made: the strip is filled, then each widget's background, then the
     * text — and the clock redraws the whole bar once a second, so that order
     * is repeated once a second and is visible as a flicker across the width.
     * This is the nearest a window manager gets to "wait for the frame to be
     * finished": nothing is shown until the whole bar is ready.
     *
     * It can be turned off for a display where the extra copy costs more than
     * it saves — a remote X session over a slow link is the honest example —
     * which is the only reason the option exists. */
    int  vsync;

    WmWidget widgets[WM_CONFIG_MAX_WIDGETS];
    int widget_count;
} WmBar;

/* What a mouse button does. The script names it —
   `RightClick="context_menu"` — and this is what the name means to the
   manager.
 *
 * The set is small because most of what a button does belongs to the program
 * under the pointer: scrolling is the program's, clicking is the program's,
 * and a manager that swallowed either would be a manager that broke every
 * program. What is left is the handful of things that are the desktop's own —
 * choosing a window, opening the desktop's menu, and moving between
 * workspaces — and one thing a manager can pass on faithfully: a middle click
 * sent to the window that has the focus, which is how a paste is asked for
 * when the pointer is somewhere else. */
typedef enum WmMouseAction {
    WM_MOUSE_NONE = 0,        /* left to the program                      */
    WM_MOUSE_SELECT,          /* focus and raise what the pointer is on   */
    WM_MOUSE_MENU,            /* the desktop's own menu                   */
    WM_MOUSE_WORKSPACE_NEXT,  /* the workspace after this one             */
    WM_MOUSE_WORKSPACE_PREV,  /* the workspace before it                  */
    WM_MOUSE_PASTE,           /* a middle click, sent to the focused window */

    /* The wheel's own two, and the reason they are not WM_MOUSE_NONE: on a
       window a wheel scrolls and on the desktop there is nothing to scroll, so
       the two cases have to be told apart. A wheel that scrolls is the wheel
       doing its job — left to the program over a window, and stepping the
       workspace over the desktop, which is the one useful thing a desktop can
       do with it. "workspace_next" on a wheel button asks for the step
       everywhere instead; "none" asks for nothing anywhere. */
    WM_MOUSE_SCROLL_UP,
    WM_MOUSE_SCROLL_DOWN,
} WmMouseAction;

/* One key binding. The script writes the keys as a list of names —
   `keys=[super_key1, "return"]` — which the config module resolves to a
   modifier and a key before this is filled in. */
typedef struct WmBinding {
    unsigned int modifiers;                  /* Mod1Mask, ControlMask, ... */
    char key[WM_CONFIG_TEXT_LENGTH];         /* an X keysym name           */
    char action[WM_CONFIG_TEXT_LENGTH];      /* the GCL call's own name    */
    /* The program a RunProgram call names, resolved. Empty when the action
       takes no program — Close, Switch — and when the action was written as a
       bare string, in which case the window manager falls back to the
       terminal the script configured. */
    char command[WM_CONFIG_TEXT_LENGTH];
} WmBinding;

typedef struct WmConfig {
    /* The frame's colours: the two things the script sets through gcl_Window,
       because a window border is drawn before anything else is on screen. */
    char active_border[WM_CONFIG_TEXT_LENGTH];
    char inactive_border[WM_CONFIG_TEXT_LENGTH];
    int border_width;

    /* gcl_Window.set_window_border_radius(...): how far a frame's corners are
       cut back. Zero keeps square corners, the shape every window had before
       this was written, so a script that never names it sees no change. */
    int border_radius;

    /* gcl_Window.set_window_shadow_color(...) and
       set_window_shadow_opacity(...): the drop shadow behind a frame. The
       colour is a name resolved to a pixel once there is a display, like every
       other colour here; the opacity is kept as a per-mille integer — a script
       writing 0.5 gives 500 and one writing 0.0 or nothing gives 0, which is
       "no shadow". */
    char shadow_color[WM_CONFIG_TEXT_LENGTH];
    int shadow_opacity;              /* 0..1000, i.e. 0.0 .. 1.0 */

    /* gcl_Window.BackgroundImage = "...": the wallpaper, drawn across the root
       behind everything. Empty keeps the flat desktop colour, which is what a
       machine that never named a picture gets. */
    char desktop_background_image[WM_CONFIG_TEXT_LENGTH];

    /* What Alt+Enter opens. Empty means "look at $TERMINAL, then at the usual
       terminals", which is what a machine with no config gets. */
    char terminal[WM_CONFIG_TEXT_LENGTH];

    /* How many workspaces the desktop has. It is the config's number and not a
       constant, because the keys that switch between them are written from it:
       a session with four workspaces has Super+1..4 bound and no Super+5, and
       the bar draws four numbers rather than six. Zero means "whatever the
       layout widget asked for", so a config that draws 0..5 gets six. */
    int workspace_count;

    WmBinding bindings[WM_CONFIG_MAX_BINDINGS];
    int binding_count;

    /* Themes, published by wm_theme.c. */
    char gtk_theme[WM_CONFIG_TEXT_LENGTH];
    char gtk_path[WM_CONFIG_TEXT_LENGTH];
    char icon_theme[WM_CONFIG_TEXT_LENGTH];
    char icon_path[WM_CONFIG_TEXT_LENGTH];
    char cursor_theme[WM_CONFIG_TEXT_LENGTH];
    char cursor_path[WM_CONFIG_TEXT_LENGTH];

    /* The pointer, resolved to what each button does. Read by wm_menu.c (which
       button opens the menu), by wm_input.c (what the wheel does) and by the
       frame code (what a click on a window does). */
    WmMouseAction mouse_left;
    WmMouseAction mouse_right;
    WmMouseAction mouse_middle;
    WmMouseAction mouse_scroll_up;
    WmMouseAction mouse_scroll_down;

    /* The touchpad, applied to the running X server by wm_input.c through
       xinput. They are the four settings libinput actually exposes as
       properties; anything a pad does not support is reported and skipped
       rather than stopping the rest. */
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

/* What the given X button does, from the parsed config: 1/2/3 are the left,
   middle and right buttons and 4/5 are the two directions of the wheel.
   Anything else, and any button the config left unset, is WM_MOUSE_NONE —
   which means the program under the pointer gets it, because that is what an
   unclaimed button has always done. */
WmMouseAction wm_config_mouse_action(const WmConfig *config,
                                     unsigned int button);

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

/* Read the script again and apply it, and show the reason in a window when it
   cannot be read. This is the reload key's whole job: a person presses it when
   they want the file read again, and nothing reads the file on its own. The
   read is all-or-nothing — a script that does not parse leaves the desktop
   exactly as it was, and the mistake is put on screen rather than applied
   half-read. Returns 1 when the desktop changed. */
int wm_config_reload_forced(WmCore *core);

/* Push the palette out of config into the desktop's style, the theme names
   into the environment, and the pointer onto the desktop. */
void wm_config_apply(WmCore *core);

#endif /* GNUCHANWM_CONFIG_H */
