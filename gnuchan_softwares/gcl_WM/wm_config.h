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

/* How many programs a session starts by itself — see WmConfig.autostart. The
   ceiling exists for the same reason the others do: every entry is a process
   the session owns, and a script that asked for hundreds is a script that has
   stopped describing a desktop. */
#define WM_CONFIG_MAX_AUTOSTART 16

/* How many bars one session may have. A script may call gcl_BAR.call(...) as
   many times as it likes — a bar along the top, another along the bottom, a
   small one down the side — and each call is one bar. There is a ceiling
   because every bar is a window and a set of widgets the manager draws on a
   timer, and a script that asked for a hundred of them is a script that has
   stopped being a desktop. */
#define WM_CONFIG_MAX_BARS 8

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

/* One bar. A config may have none, one, or several; a bar with `present` 0 is
   not drawn, and the number of bars a session has is WmConfig.bar_count.
 *
 * Every field a script can set through gcl_BAR.call(...) lives here. Where a
 * bar goes is asked in two ways that can disagree, and the rule is the same as
 * everywhere else in this file: what the script wrote wins. X and Y, when
 * given, are the bar's own top-left corner and are used as written; when they
 * are not given, Position decides the edge the bar hugs. So a script that
 * wants the ordinary top bar writes Position="top" and nothing else, and one
 * that wants six pixels above the top edge writes Y=6. */
typedef struct WmBar {
    int  present;
    char position[WM_CONFIG_TEXT_LENGTH];   /* "top", "bottom", "left", "right" */
    int  size;                              /* thickness of the strip          */
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

    /* gcl_BAR.call(X=..., Y=...): where the bar's top-left corner is. A value
       of -1, which is what "the script did not write it" becomes, means "let
       Position decide". Zero is a real place — the very corner of the screen —
       so it is kept as written rather than treated as missing. */
    int  x;
    int  y;

    /* The room left between the bar and the end it would otherwise reach, so
       the bar does not touch the screen's corners.
     *
     * There are four of these and each pair belongs to one pose, because "the
     * left end" and "the top end" are the same end of two bars that run in
     * different directions. Left_EmptySpace and Right_EmptySpace shorten a
     * horizontal bar from its two ends; Up_EmptySpace and Down_EmptySpace do
     * the same for a vertical one. A script that names the pair belonging to
     * the other pose has made a mistake, and the reader says so rather than
     * quietly applying a number that means nothing on this bar. */
    int  left_empty;      /* horizontal bars only */
    int  right_empty;     /* horizontal bars only */
    int  up_empty;        /* vertical bars only   */
    int  down_empty;      /* vertical bars only   */

    /* gcl_BAR.call(pose="horizontal"|"vertical"): which way the bar runs and
       so which way its widgets are laid out — left to right, or top to
       bottom. A horizontal bar is as wide as it is allowed and as tall as
       `size`; a vertical one is the other way round. */
    char pose[WM_CONFIG_TEXT_LENGTH];

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

    /* gcl_Window.set_moved_window_border_color("#ff8a3d"): the border a window
       is drawn in for as long as it is marked as having been sent to another
       workspace. Super+Shift+N tags the window with the desk it is going to
       AND marks it — see wm_workspace_move() — and this is the colour that
       mark is drawn in, so a hand that has sent three windows to three desks
       can still tell which three they were afterwards.
     *
     * It is a setting of its own rather than a shade of the active border,
     * because the two mean different things: the active border says "the
     * keyboard is here", and this one says "this window was just moved". A
     * desktop that wants them the same colour writes the same string twice.
     *
     * Empty keeps the palette's own value (wm_style.c), which is an orange
     * that is neither of the two other border colours. The mark is cleared
     * when the window is next focused, because by then it has been found. */
    char moved_border[WM_CONFIG_TEXT_LENGTH];

    /* The switcher's own colours: the overlay Alt+` puts up. They are written
       as assignments — `gcl_Switcher.background = "#..."` — the same way the
       desktop's wallpaper and flat colour are, because they describe one
       surface rather than one call with arguments.
     *
     * They are separate from the desktop's palette on purpose. The overlay is
       not a title bar: most of it is a picture of a window, and the colour
       around that picture has to be quiet enough for a terminal's own colours
       to still read as that terminal. A desktop's background is chosen to sit
       behind windows; this is chosen to sit behind pictures of them, and the
       two are not the same question.
     *
     * Empty keeps the palette's own value (wm_style.c), so a script that names
     * none of them gets the switcher it always had. */
    char switcher_background[WM_CONFIG_TEXT_LENGTH];
    char switcher_panel[WM_CONFIG_TEXT_LENGTH];
    char switcher_cell_border[WM_CONFIG_TEXT_LENGTH];
    char switcher_select_border[WM_CONFIG_TEXT_LENGTH];
    char switcher_text[WM_CONFIG_TEXT_LENGTH];
    char switcher_select_text[WM_CONFIG_TEXT_LENGTH];
    char switcher_field[WM_CONFIG_TEXT_LENGTH];

    /* gcl_Window.background_color = "#27022b": the flat colour the desktop is
       painted in when no wallpaper is named — or when the one named cannot be
       read. It is what the root window's background is set to, which is the
       colour the server repaints every uncovered part of the desktop with.
       Empty keeps the palette's own background (wm_style.c), which is what a
       machine that named neither gets. */
    char desktop_background_color[WM_CONFIG_TEXT_LENGTH];

    /* gcl_Window.BackgroundImage = "...": the wallpaper, drawn across the root
       behind everything through Imlib2 (see wm_image.c), the way feh draws
       one. When it is empty — or names a file that cannot be read — the flat
       desktop colour above is used instead. */
    char desktop_background_image[WM_CONFIG_TEXT_LENGTH];

    /* What Alt+Enter opens. Empty means "look at $TERMINAL, then at the usual
       terminals", which is what a machine with no config gets. */
    char terminal[WM_CONFIG_TEXT_LENGTH];

    /* The programs the session starts by itself, written as a list:
       `gcl_autostart.all = ["GnuChanNotification", "GnuChanDock"]`.
     *
     * Each entry is a command LINE and not a bare program name — the same
     * thing a RunProgram action carries — so an entry may name arguments:
     * "GnuChanDock --bottom". They are started once, at session start, by
     * wm_autostart.c, and each is started the way every other program is:
     * fork and execvp, never a shell. A command that cannot be found fails in
     * its own child and costs the session nothing but one line in the log.
     *
     * This is where the desktop's own daemons belong — the notification server
     * above all, which has to be running before the first program that might
     * want to notify does. It is a setting and not a hard-coded list because
     * what a session starts is a policy: a machine that replaces the
     * notification server, or wants a panel, edits this and nothing else. */
    char autostart[WM_CONFIG_MAX_AUTOSTART][WM_CONFIG_TEXT_LENGTH];
    int autostart_count;

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

    /* --- session power: the lid, and what a wake-up does ------------------
     *
     * Set through `gcl_Power.Lid(...)` in the settings script. The lid is a
     * SESSION concern and not the display manager's (see gcl_DM/dm_power.c,
     * which says so itself and deliberately does not touch it), so it is
     * handled by the wm_lid module and configured here.
     *
     * The lid module reads these out of the core's config on every tick, so a
     * reload changes them live, exactly as every other setting does. */
    int lid_suspend_on_close;    /* closing the lid suspends the machine     */
    int lock_before_suspend;     /* lock before suspending, not after waking */
    int lid_open_screensaver;    /* run the screen saver on wake             */
    int lid_open_lockscreen;     /* lock the screen on wake                  */

    /* --- idle: the screen that goes dark because nobody is there --------
     *
     * Set through `gcl_Power.Idle(...)` in the settings script. The X server
     * keeps a screen-blanking timer and a DPMS timer of its own, and on a
     * machine that never turns them off the panel simply goes dark after a
     * while — a black screen with no screen saver and nothing to wake: the
     * desk looks broken and there is no picture to come back to. The window
     * manager turns both of those off at start-up (wm_idle.c) and watches the
     * idle clock itself, so what happens after a quiet spell is this
     * desktop's own decision rather than the server's.
     *
     * `idle_enabled` is the whole feature's switch: off, the server's own
     * blanking is left exactly as it was found. `idle_seconds` is how long the
     * keyboard and pointer have to be still, `idle_screensaver` runs the
     * screen saver when that passes, and `idle_lockscreen` locks the screen at
     * the same moment — a machine that wants to come back to a password
     * instead of a picture turns the second one on. Both programs are the ones
     * the lid settings already name (screensaver_command / lockscreen_command),
     * so a machine that overrode them for a wake-up gets the same ones here. */
    int idle_enabled;            /* watch the idle clock at all              */
    int idle_seconds;            /* how long quiet before acting             */
    int idle_screensaver;        /* run the screen saver when it passes      */
    int idle_lockscreen;         /* lock the screen when it passes           */

    /* The programs the lid module runs. Empty means "look GnuChanSS /
       GnuChanSL up on PATH", which is what a machine that named none gets.
       They are written commands, not program names, so the same tokeniser
       that reads a RunProgram action reads these. */
    char screensaver_command[WM_CONFIG_TEXT_LENGTH];
    char lockscreen_command[WM_CONFIG_TEXT_LENGTH];

    /* The bars, in the order the script wrote them. A session may have none,
       one, or several; `bar_count` is how many there are and every reader
       walks the first `bar_count` of the array. */
    WmBar bars[WM_CONFIG_MAX_BARS];
    int bar_count;

    /* Something the script asked for that could not be given, in the words to
       show the person who wrote it — a bar with an empty-space setting that
       belongs to the other pose, and whatever else the reader finds wrong.
     *
     * It is carried here rather than shown where it is found because the
     * reader that finds it has no display to show it on: wm_config_load()
     * reads the file and this is filled in, and wm_config_apply() — which is
     * the one place both the first read and a reload pass through, and which
     * does have the core — puts it on the screen. Empty when there is nothing
     * to say, which is the usual case. */
    char notes[WM_CONFIG_TEXT_LENGTH * 3];
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

/* The rectangle a bar actually occupies on a screen of this size, out of its
   own Position, Size, X, Y and empty-space settings.

   This is the ONE computation of a bar's rectangle. The desktop draws the bar
   from it and wm_config_workarea() subtracts it, so the space a window is kept
   out of is exactly the space the bar covers. They used to be two separate
   rules — the desktop's real geometry and the workarea's "an edge bar starts
   at the screen edge and is Size thick" — which agreed only for a bar that
   named no X, no Y and no empty spaces. A bar written Position="top", Y=10,
   Left_EmptySpace=5 sits at x=5, y=10 and is 24 tall, so its bottom edge is 34,
   while the workarea still believed the strip ended at 24: a maximised window
   came to rest ten pixels inside the bar and the bar stayed over it. */
void wm_config_bar_rect(const WmBar *bar, int screen_width, int screen_height,
                        int *x, int *y, int *width, int *height);

/* The rectangle a window may occupy: the whole screen less every bar's real
   rectangle (see wm_config_bar_rect). `x` and `y` are where a window may be
   put and `width` and `height` how large it may be. Called by the frame code
   both when a window opens and when one is maximised, so a bar is never
   covered by the thing it is meant to sit above. */
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
