/*
 * wm_config_file.c — turn the settings script into the desktop.
 *
 * The parser in wm_config_parser.c reads the script; this file is the part
 * that knows what it read. It walks the statements the parser produced and
 * gives each one its meaning:
 *
 *     gcl_Window.set_active_window_border_color("#c369ff")
 *         -> the focused frame's colour
 *     gcl_BAR.call(Position=..., Widgets=[gcl_Widgets.Clock(...)])
 *         -> the bar, and the widgets on it
 *     gcl_keys.all = [gcl_key.MultiKey(keys=[...], action=...)]
 *         -> the key table
 *
 * It also owns the two things that make the script reloadable: the name it is
 * looked for under, and the reload itself. Reload is asked for by hand — the
 * reload key — and never watched for: a person presses Ctrl+Alt+R when they
 * want the script read again, and the desktop does not re-read the file behind
 * their back. That is what lets a read be all-or-nothing: the whole file is
 * parsed into a copy and swapped in only when it parsed, a mistake leaves the
 * desktop it already had, and the mistake is put in a window rather than
 * silently applied half-read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "wm_config_parser.h"
#include "wm_core.h"
#include "wm_frame.h"
#include "wm_spawn.h"
#include "wm_theme.h"

/* Values the script named for itself, so a name used as a value can be
   resolved: `super_key1 = "Mod1"` followed by `keys=[super_key1, "return"]`
   is the script naming its own value, and reading it means remembering what
   was assigned. */
typedef struct ScriptVariable {
    char name[WM_CONFIG_TEXT_LENGTH];
    char text[WM_CONFIG_TEXT_LENGTH];
} ScriptVariable;

#define WM_CONFIG_MAX_VARIABLES 64

typedef struct Script {
    ScriptVariable variables[WM_CONFIG_MAX_VARIABLES];
    int variable_count;
} Script;

/* --- small helpers -------------------------------------------------------- */

static void copy_text(char *destination, unsigned int size, const char *source) {
    if (!destination || size == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, size, "%s", source);
}

/* Add a line to the config's notes: the things the script asked for that could
   not be given. They are collected while the file is read and shown once there
   is a screen to show them on — see WmConfig.notes and wm_config_apply().
 *
 * A note that does not fit is dropped rather than clipping the notes already
 * there: a half-written sentence is worse than a missing one, and the notes are
 * a courtesy on top of the log, which has every one of them whole. */
static void append_note(WmConfig *config, const char *note) {
    if (!config || !note || !note[0]) {
        return;
    }
    size_t used = strlen(config->notes);
    /* Room for a separator, the note and the terminator. */
    if (used + 2 + strlen(note) + 1 > sizeof(config->notes)) {
        return;
    }
    if (used > 0) {
        config->notes[used++] = '\n';
    }
    snprintf(config->notes + used, sizeof(config->notes) - used, "%s", note);
}

/* A value as text, with variables resolved. An unresolved name is kept as
   itself, which is what makes a command like "xterm" written unquoted still
   work. */
static void value_text(const Script *script, const WmValue *value,
                       char *out, unsigned int size) {
    if (!value) {
        wm_config_value_text(NULL, out, size);
        return;
    }
    if (value->kind == WM_VALUE_NAME && script) {
        for (int i = 0; i < script->variable_count; i++) {
            if (strcmp(script->variables[i].name, value->text) == 0) {
                copy_text(out, size, script->variables[i].text);
                return;
            }
        }
    }
    wm_config_value_text(value, out, size);
}

/* The first of several argument names a call actually passed.
 *
 * One setting is spelled two ways in practice: the group box's separator is
 * `Seperator` in the shipped script — the way the person who wrote it spells
 * the word — and `Separator` for anyone who spells it correctly. Reading only
 * one of them silently drops the other, so a reader that has more than one
 * name for the same argument asks for them in turn. */
static const WmValue *first_argument(const WmStatement *call,
                                     const char *const *names, int name_count) {
    for (int i = 0; i < name_count; i++) {
        const WmValue *value = wm_config_argument(call, names[i]);
        if (value) {
            return value;
        }
    }
    return NULL;
}

static const char *const SEPARATOR_NAMES[] = {
    "Seperator", "Separator", "seperator", "separator",
};
#define SEPARATOR_NAME_COUNT \
    ((int)(sizeof(SEPARATOR_NAMES) / sizeof(SEPARATOR_NAMES[0])))

static const char *const SEPARATOR_COLOUR_NAMES[] = {
    "SeperatorColor", "SeparatorColor",
    "SeperatorColour", "SeparatorColour",
};
#define SEPARATOR_COLOUR_NAME_COUNT \
    ((int)(sizeof(SEPARATOR_COLOUR_NAMES) / sizeof(SEPARATOR_COLOUR_NAMES[0])))

/* --- widgets -------------------------------------------------------------- */

/* One widget out of a gcl_Widgets.X(...) call. The widget's kind is the call's
   own name, which is how the script spells it: the parser keeps
   "gcl_Widgets.Clock" whole and this reads the part after the dot. */
static int widget_from_call(const Script *script, const WmStatement *call,
                            WmWidget *widget) {
    const char *dot = strrchr(call->target, '.');
    const char *kind_name = dot ? dot + 1 : call->target;

    memset(widget, 0, sizeof(*widget));
    if (strcmp(kind_name, "CurrentLayout") == 0) {
        widget->kind = WM_WIDGET_CURRENT_LAYOUT;
    } else if (strcmp(kind_name, "GroupBox") == 0) {
        widget->kind = WM_WIDGET_GROUP_BOX;
    } else if (strcmp(kind_name, "EmptySpace") == 0) {
        widget->kind = WM_WIDGET_EMPTY_SPACE;
    } else if (strcmp(kind_name, "TextBox") == 0) {
        widget->kind = WM_WIDGET_TEXT_BOX;
    } else if (strcmp(kind_name, "Clock") == 0) {
        widget->kind = WM_WIDGET_CLOCK;
    } else {
        /* A widget this build does not draw. Reported, not fatal: the rest of
           the bar still appears, which is the difference between a missing
           widget and a missing bar. */
        fprintf(stderr, "gnuchanwm: config: widget '%s' is not known\n",
                kind_name);
        return -1;
    }

    const WmValue *argument;
    char text[WM_CONFIG_TEXT_LENGTH];

    argument = wm_config_argument(call, "BackgroundColor");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->background, sizeof(widget->background), text);

    argument = wm_config_argument(call, "ForegroundColor");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->foreground, sizeof(widget->foreground), text);

    argument = wm_config_argument(call, "FontFamily");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->font_family, sizeof(widget->font_family), text);

    widget->font_size = wm_config_value_number(
        wm_config_argument(call, "FontSize"), 12);
    /* A bar's script writes "start_layout" in one widget and nothing in the
       others; a widget that says nothing keeps the range of one layout, which
       draws a single number rather than nothing. */
    widget->start_layout = wm_config_value_number(
        wm_config_argument(call, "start_layout"), 0);
    widget->end_layout = wm_config_value_number(
        wm_config_argument(call, "end_layout"), widget->start_layout);

    /* Held inside the number of workspaces a session may have. The bar draws
       one cell per workspace that exists, and the session's count is derived
       from these two numbers — so a script that wrote a range wider than the
       ceiling, or one that ended at the largest number an int can hold, would
       have that arithmetic overflow before anything was drawn. Clamping at the
       one place the range is read keeps every later use of it in range, and a
       range wider than the ceiling is a range a session could never show
       anyway. */
    if (widget->start_layout < 0) {
        widget->start_layout = 0;
    }
    if (widget->start_layout > WM_WORKSPACE_MAX - 1) {
        widget->start_layout = WM_WORKSPACE_MAX - 1;
    }
    if (widget->end_layout < widget->start_layout) {
        widget->end_layout = widget->start_layout;
    }
    if (widget->end_layout > WM_WORKSPACE_MAX - 1) {
        widget->end_layout = WM_WORKSPACE_MAX - 1;
    }

    /* The room between two workspace cells. Zero means "not written", and the
       bar then uses its own default spacing — a script that never mentions Gap
       keeps the bar it always had instead of getting a row of cells that touch
       each other. A negative or zero value written on purpose is treated the
       same way, because a cell with no gap is unreadable and nobody asks for
       it deliberately. */
    widget->gap = wm_config_value_number(
        wm_config_argument(call, "Gap"), 0);

    argument = wm_config_argument(call, "symbol");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->symbol, sizeof(widget->symbol), text);

    argument = wm_config_argument(call, "text");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->text, sizeof(widget->text), text);

    argument = wm_config_argument(call, "format");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->format, sizeof(widget->format), text);

    /* The group box's separator, and the colour it is drawn in.
     *
     * The mark between two open windows' icons never appeared whatever the
     * script said: the reader asked for `Separator` while the shipped script
     * writes `Seperator`, so the field the drawing code checks was never
     * filled in and the box drew the icons hard against each other. Both
     * spellings are read now. An unnamed colour is left as an empty string,
     * which the drawing code already reads as "the widget's own foreground". */
    argument = first_argument(call, SEPARATOR_NAMES, SEPARATOR_NAME_COUNT);
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->separator, sizeof(widget->separator), text);

    argument = first_argument(call, SEPARATOR_COLOUR_NAMES,
                              SEPARATOR_COLOUR_NAME_COUNT);
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->separator_color, sizeof(widget->separator_color), text);

    /* An EmptySpace grows by default: that is what makes a bar of "layout,
       space, clock" put the clock at the right edge without anyone saying
       where the edge is. `Expanding=False` turns the same widget into a fixed
       gap instead, and then Horizontal is the gap in pixels — which is how a
       script writes "eight pixels here" between two widgets.
     *
     * The default is 1 whether or not the widget is an EmptySpace, because a
     * widget that is not one never reads the field, and a script that wrote
     * something other than True/False gets the default rather than a zero that
     * would silently collapse the bar. */
    widget->expanding = wm_config_value_bool(
        wm_config_argument(call, "Expanding"), 1);
    widget->horizontal = wm_config_value_number(
        wm_config_argument(call, "Horizontal"), 1);
    if (widget->horizontal < 1) {
        widget->horizontal = 1;
    }

    return 0;
}

/* The widgets of a bar, out of the Widgets list: a list of widget calls. */
static void set_bar_widgets(const Script *script, WmBar *bar,
                            const WmValue *widgets) {
    if (!widgets || widgets->kind != WM_VALUE_LIST) {
        return;
    }
    for (int i = 0; i < widgets->item_count; i++) {
        const WmValue *item = &widgets->items[i];
        if (item->kind != WM_VALUE_CALL || !item->call) {
            continue;
        }
        if (bar->widget_count >= WM_CONFIG_MAX_WIDGETS) {
            fprintf(stderr, "gnuchanwm: config: too many widgets, one ignored\n");
            break;
        }
        if (widget_from_call(script, item->call,
                             &bar->widgets[bar->widget_count]) == 0) {
            bar->widget_count++;
        }
    }
}

/* The values a bar has when a gcl_BAR.call(...) does not name them.
 *
 * Written once so a bar the script wrote and the built-in bar of a machine
 * with no script cannot disagree about what "no Y" or "no pose" means. X and Y
 * are -1, which is "the script did not write them" — the corner the bar takes
 * is then the one Position names — because 0 is a real place on the screen and
 * must not be mistaken for "unset". */
static void bar_defaults(WmBar *bar) {
    memset(bar, 0, sizeof(*bar));
    bar->present = 1;
    bar->size = 24;
    bar->vsync = 1;
    bar->x = -1;
    bar->y = -1;
    copy_text(bar->position, sizeof(bar->position), "top");
    copy_text(bar->pose, sizeof(bar->pose), "horizontal");
    copy_text(bar->background, sizeof(bar->background), "#27022b");
}

/* gcl_BAR.call(Position=..., Size=..., BackgroundColor=..., Vsync=...,
 *              X=..., Y=..., Left_EmptySpace=..., Right_EmptySpace=...,
 *              pose=..., Widgets=[...])
 *
 * One call is one bar, and a script may make as many calls as it likes: a bar
 * along the top, another along the bottom, a small one down the side. There is
 * a ceiling (WM_CONFIG_MAX_BARS), because each bar is a window and a set of
 * widgets drawn on a timer, and a call past the ceiling is reported and
 * dropped rather than growing the array without end. */
static void set_bar(const Script *script, WmConfig *config,
                    const WmStatement *statement) {
    if (config->bar_count >= WM_CONFIG_MAX_BARS) {
        fprintf(stderr, "gnuchanwm: config: too many bars, one ignored\n");
        return;
    }
    WmBar *bar = &config->bars[config->bar_count];
    bar_defaults(bar);
    char text[WM_CONFIG_TEXT_LENGTH];

    value_text(script, wm_config_argument(statement, "Position"),
               text, sizeof(text));
    if (text[0]) {
        copy_text(bar->position, sizeof(bar->position), text);
    }

    bar->size = wm_config_value_number(wm_config_argument(statement, "Size"),
                                       bar->size);
    if (bar->size <= 0) {
        bar->size = 24;
    }

    value_text(script, wm_config_argument(statement, "BackgroundColor"),
               text, sizeof(text));
    if (text[0]) {
        copy_text(bar->background, sizeof(bar->background), text);
    }

    /* Off unless the script says otherwise — no: on unless the script says
       otherwise. See WmBar.vsync; the field is read as a bool with a default
       of 1 so a script that never mentions it gets the flicker-free bar. */
    bar->vsync = wm_config_value_bool(
        wm_config_argument(statement, "Vsync"), 1);

    /* Where the bar is, when the script says so itself rather than naming an
       edge. -1 is "not written"; any other number, zero included, is a place
       on the screen and is kept. */
    bar->x = wm_config_value_number(wm_config_argument(statement, "X"), -1);
    bar->y = wm_config_value_number(wm_config_argument(statement, "Y"), -1);

    value_text(script, wm_config_argument(statement, "pose"),
               text, sizeof(text));
    if (text[0]) {
        copy_text(bar->pose, sizeof(bar->pose), text);
    }
    int vertical = strcmp(bar->pose, "vertical") == 0;

    /* The room left at each end of the bar. A negative number would be a bar
       wider than the screen asks for, so it is held at zero. */
    bar->left_empty = wm_config_value_number(
        wm_config_argument(statement, "Left_EmptySpace"), 0);
    if (bar->left_empty < 0) {
        bar->left_empty = 0;
    }
    bar->right_empty = wm_config_value_number(
        wm_config_argument(statement, "Right_EmptySpace"), 0);
    if (bar->right_empty < 0) {
        bar->right_empty = 0;
    }
    bar->up_empty = wm_config_value_number(
        wm_config_argument(statement, "Up_EmptySpace"), 0);
    if (bar->up_empty < 0) {
        bar->up_empty = 0;
    }
    bar->down_empty = wm_config_value_number(
        wm_config_argument(statement, "Down_EmptySpace"), 0);
    if (bar->down_empty < 0) {
        bar->down_empty = 0;
    }

    /* The two pairs of empty spaces each belong to one pose, and a script that
       names the wrong pair has asked for something this bar cannot do: a
       horizontal bar runs left to right, so there is no top or bottom end for
       Up_EmptySpace to shorten, and a vertical bar has no left or right one.
       The number is not applied — it would be a spacing on an axis the bar
       does not have — and the mistake is put in the config's notes to be shown
       once there is a screen to show it on. See WmConfig.notes and
       wm_config_apply().
     *
     * Only a value that is actually set is reported: a script that writes
     * `Up_EmptySpace=0` on a horizontal bar is asking for no space, which is
     * what it already has, and a message about nothing would be noise. */
    if (!vertical && (bar->up_empty > 0 || bar->down_empty > 0)) {
        append_note(config,
                    "A horizontal bar cannot use Up_EmptySpace or "
                    "Down_EmptySpace; those belong to a bar with "
                    "pose=\"vertical\". The value was ignored.");
    }
    if (vertical && (bar->left_empty > 0 || bar->right_empty > 0)) {
        append_note(config,
                    "A vertical bar cannot use Left_EmptySpace or "
                    "Right_EmptySpace; those belong to a bar with "
                    "pose=\"horizontal\". The value was ignored.");
    }

    set_bar_widgets(script, bar, wm_config_argument(statement, "Widgets"));

    /* The bar is only counted once it is finished, so a call that fails the
       ceiling check above never leaves a half-written bar in the array. */
    config->bar_count++;
}

/* --- the other statements ------------------------------------------------- */

/* gcl_Switcher.<name> = "#..." — one colour of the Alt+` overlay.
 *
 * Written as assignments rather than calls, the way the desktop's wallpaper and
 * flat colour are, because the seven describe one surface rather than one call
 * with arguments. The name after the dot is matched WHOLE against the seven the
 * overlay draws with: a substring match would make `background_colour` fill in
 * `background`, and a colour meant for the switcher and misspelled would then
 * be a colour nobody ever sees. A name that matches none is reported for the
 * same reason.
 *
 * Each one lands in the config as text; wm_config_apply() turns them into
 * pixels, because a colour name needs a display to become one and this reader
 * has none. */
static void set_switcher_colour(const Script *script, WmConfig *config,
                                const WmStatement *statement) {
    const char *dot = strchr(statement->target, '.');
    const char *name = dot ? dot + 1 : "";
    char *destination = NULL;
    char text[WM_CONFIG_TEXT_LENGTH];

    if (strcmp(name, "background") == 0) {
        destination = config->switcher_background;
    } else if (strcmp(name, "panel") == 0) {
        destination = config->switcher_panel;
    } else if (strcmp(name, "cell_border") == 0) {
        destination = config->switcher_cell_border;
    } else if (strcmp(name, "select_border") == 0) {
        destination = config->switcher_select_border;
    } else if (strcmp(name, "text") == 0) {
        destination = config->switcher_text;
    } else if (strcmp(name, "select_text") == 0) {
        destination = config->switcher_select_text;
    } else if (strcmp(name, "field") == 0) {
        destination = config->switcher_field;
    }

    if (!destination) {
        fprintf(stderr,
                "gnuchanwm: config: gcl_Switcher.%s is not a colour the "
                "switcher has; the ones it has are background, panel, "
                "cell_border, select_border, text, select_text and field\n",
                name);
        return;
    }
    value_text(script, &statement->value, text, sizeof(text));
    copy_text(destination, WM_CONFIG_TEXT_LENGTH, text);
}

static void set_border_colours(WmConfig *config, const WmStatement *statement) {
    /* The script passes its one value as the only argument, so it is
       positional: set_active_window_border_color("#c369ff") and
       set_window_border_width(2). */
    const WmValue *value = wm_config_argument(statement, "color");
    if (!value) {
        value = wm_config_argument(statement, "width");
    }
    if (!value && statement->arg_count > 0) {
        value = &statement->args[0].value;
    }
    if (!value) {
        return;
    }

    /* The border's thickness, set the same way the colours are but with a
       number. Zero or less is refused rather than applied: a border of no
       width is a window with nothing to grab, and a value like that is a
       mistake the person who wrote it wants to hear about rather than have
       silently remove the frame. */
    if (strcmp(statement->target,
               "gcl_Window.set_window_border_width") == 0) {
        int width = wm_config_value_number(value, 0);
        if (width >= 1) {
            config->border_width = width;
        } else {
            fprintf(stderr,
                    "gnuchanwm: config: set_window_border_width(%d) ignored; "
                    "a border has to be at least 1 pixel\n", width);
        }
        return;
    }

    char text[WM_CONFIG_TEXT_LENGTH];
    wm_config_value_text(value, text, sizeof(text));

    if (strcmp(statement->target,
               "gcl_Window.set_active_window_border_color") == 0) {
        copy_text(config->active_border, sizeof(config->active_border), text);
    } else if (strcmp(statement->target,
                      "gcl_Window.set_inactive_window_border_color") == 0) {
        copy_text(config->inactive_border, sizeof(config->inactive_border), text);
    } else if (strcmp(statement->target,
                      "gcl_Window.set_moved_window_border_color") == 0) {
        /* The third border colour, for a window that has just been sent to
           another workspace. It is read here with the other two because it is
           set the same way — one positional colour argument — and because a
           script that names all three should see them read by one function
           rather than two. See WmConfig.moved_border. */
        copy_text(config->moved_border, sizeof(config->moved_border), text);
    }
}

/* gcl_themes.Theme_gtk(ThemeName=..., ThemePath=...) and its two siblings. */
static void set_theme(WmConfig *config, const WmStatement *statement) {
    char name[WM_CONFIG_TEXT_LENGTH];
    char path[WM_CONFIG_TEXT_LENGTH];
    wm_config_value_text(wm_config_argument(statement, "ThemeName"),
                         name, sizeof(name));
    wm_config_value_text(wm_config_argument(statement, "ThemePath"),
                         path, sizeof(path));

    if (strcmp(statement->target, "gcl_themes.Theme_gtk") == 0) {
        copy_text(config->gtk_theme, sizeof(config->gtk_theme), name);
        copy_text(config->gtk_path, sizeof(config->gtk_path), path);
    } else if (strcmp(statement->target, "gcl_themes.Theme_icon") == 0) {
        copy_text(config->icon_theme, sizeof(config->icon_theme), name);
        copy_text(config->icon_path, sizeof(config->icon_path), path);
    } else if (strcmp(statement->target, "gcl_themes.Theme_cursor") == 0) {
        copy_text(config->cursor_theme, sizeof(config->cursor_theme), name);
        copy_text(config->cursor_path, sizeof(config->cursor_path), path);
    }
}

/* What a written mouse action means. The script names them in its own words;
   this is the table that turns a name into the thing the manager does.
 *
 * Several names reach the same action because a script is written by a person
   and the same idea has more than one obvious spelling — "context_menu" and
   "menu" are the same request. A name that matches nothing is reported and
   the button is left to the program, which is what "select" on a button the
   manager does not use would do anyway. */
static WmMouseAction mouse_action_of(const char *written) {
    if (!written || !written[0]) {
        return WM_MOUSE_NONE;
    }
    if (strcmp(written, "select") == 0) {
        return WM_MOUSE_SELECT;
    }
    if (strcmp(written, "context_menu") == 0 || strcmp(written, "menu") == 0 ||
        strcmp(written, "context") == 0) {
        return WM_MOUSE_MENU;
    }
    if (strcmp(written, "paste") == 0) {
        return WM_MOUSE_PASTE;
    }
    if (strcmp(written, "workspace_next") == 0) {
        return WM_MOUSE_WORKSPACE_NEXT;
    }
    if (strcmp(written, "workspace_prev") == 0) {
        return WM_MOUSE_WORKSPACE_PREV;
    }
    /* The wheel's own two. "scroll_up" is the wheel scrolling — not the same
       request as "nothing", because scrolling is what a wheel is for and on
       the desktop there is nothing else for it to do. See wm_input.c. */
    if (strcmp(written, "scroll_up") == 0) {
        return WM_MOUSE_SCROLL_UP;
    }
    if (strcmp(written, "scroll_down") == 0) {
        return WM_MOUSE_SCROLL_DOWN;
    }
    if (strcmp(written, "none") == 0 || strcmp(written, "nothing") == 0) {
        return WM_MOUSE_NONE;
    }
    fprintf(stderr, "gnuchanwm: config: mouse action '%s' is not known\n",
            written);
    return WM_MOUSE_NONE;
}

/* gcl_mouse.MouseBehavior(LeftClick=..., RightClick=..., ...) */
static void set_mouse(WmConfig *config, const WmStatement *statement) {
    char text[WM_CONFIG_TEXT_LENGTH];
    struct {
        const char *argument;
        WmMouseAction *destination;
    } fields[] = {
        { "LeftClick",   &config->mouse_left        },
        { "RightClick",  &config->mouse_right       },
        { "MiddleClick", &config->mouse_middle      },
        { "ScrollUp",    &config->mouse_scroll_up   },
        { "ScrollDown",  &config->mouse_scroll_down },
    };
    for (unsigned int i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        const WmValue *argument =
            wm_config_argument(statement, fields[i].argument);
        if (!argument) {
            continue;
        }
        wm_config_value_text(argument, text, sizeof(text));
        *fields[i].destination = mouse_action_of(text);
    }
}

/* gcl_touchpad.TouchpadBehavior(TapToClick=True, ...) */
static void set_touchpad(WmConfig *config, const WmStatement *statement) {
    const struct {
        const char *argument;
        int *destination;
        int fallback;
    } fields[] = {
        { "TapToClick",       &config->touchpad_tap_to_click,       1 },
        { "TwoFingerScroll",  &config->touchpad_two_finger_scroll,  1 },
        { "ThreeFingerSwipe", &config->touchpad_three_finger_swipe, 0 },
        { "FourFingerSwipe",  &config->touchpad_four_finger_swipe,  0 },
    };
    for (unsigned int i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        const WmValue *argument =
            wm_config_argument(statement, fields[i].argument);
        if (argument) {
            *fields[i].destination =
                wm_config_value_bool(argument, fields[i].fallback);
        }
    }
}

/* gcl_Power.Lid(OnLidCloseSuspend=..., LockBeforeSuspend=...,
 *               OnLidOpenScreenSaver=..., OnLidOpenLockScreen=...,
 *               ScreensaverCommand=..., LockScreenCommand=...)
 *
 * The lid and what a wake-up does. Read by wm_lid.c, which polls the lid and
 * acts on a change; these are the settings that decide what "acts" means.
 *
 * The two commands are text and not booleans: a machine whose screen saver is
 * not the shipped one names its own. Empty means the module looks GnuChanSS /
 * GnuChanSL up on PATH itself, so a script that names neither still gets the
 * desktop's own programs. */
static void set_power(WmConfig *config, const WmStatement *statement) {
    const struct {
        const char *argument;
        int *destination;
        int fallback;
    } switches[] = {
        { "OnLidCloseSuspend",    &config->lid_suspend_on_close, 1 },
        { "LockBeforeSuspend",    &config->lock_before_suspend,  0 },
        { "OnLidOpenScreenSaver", &config->lid_open_screensaver, 1 },
        { "OnLidOpenLockScreen",  &config->lid_open_lockscreen,  0 },
    };
    for (unsigned int i = 0; i < sizeof(switches) / sizeof(switches[0]); i++) {
        const WmValue *argument =
            wm_config_argument(statement, switches[i].argument);
        if (argument) {
            *switches[i].destination =
                wm_config_value_bool(argument, switches[i].fallback);
        }
    }

    const WmValue *screensaver =
        wm_config_argument(statement, "ScreensaverCommand");
    if (screensaver) {
        wm_config_value_text(screensaver, config->screensaver_command,
                             sizeof(config->screensaver_command));
    }
    const WmValue *lockscreen =
        wm_config_argument(statement, "LockScreenCommand");
    if (lockscreen) {
        wm_config_value_text(lockscreen, config->lockscreen_command,
                             sizeof(config->lockscreen_command));
    }
}

/* gcl_Power.Idle(Enabled=True, Seconds=300, ScreenSaver=True, LockScreen=False)
 *
 * The desk that goes quiet. The X server's own blanking and DPMS are turned
 * off by wm_idle.c, and this is what replaces them: how long the keyboard and
 * pointer have to be still, and what happens when they have been. The two
 * "what happens" switches are separate because a machine may want the picture
 * without the password (an office desk) or the password without the picture
 * (a machine left in a room) — and neither is the other's special case.
 *
 * The programs are not named here: the same screensaver_command and
 * lockscreen_command the lid already uses are what run, so a machine that
 * named its own saver once gets it for both a wake-up and an idle spell. */
static void set_idle(WmConfig *config, const WmStatement *statement) {
    const struct {
        const char *argument;
        int *destination;
        int fallback;
    } switches[] = {
        { "Enabled",     &config->idle_enabled,     1 },
        { "ScreenSaver", &config->idle_screensaver, 1 },
        { "LockScreen",  &config->idle_lockscreen,  0 },
    };
    for (unsigned int i = 0; i < sizeof(switches) / sizeof(switches[0]); i++) {
        const WmValue *argument =
            wm_config_argument(statement, switches[i].argument);
        if (argument) {
            *switches[i].destination =
                wm_config_value_bool(argument, switches[i].fallback);
        }
    }

    const WmValue *seconds = wm_config_argument(statement, "Seconds");
    if (seconds) {
        config->idle_seconds =
            wm_config_value_number(seconds, config->idle_seconds);
    }
}

/* --- key bindings --------------------------------------------------------- */

/* The modifier a name stands for. The script writes its keys as names —
   super_key1, and super_key1 = "Mod1" — so this is where "Mod1" becomes
   Mod1Mask. */
static unsigned int modifier_of(const Script *script, const char *written) {
    char resolved[WM_CONFIG_TEXT_LENGTH];
    copy_text(resolved, sizeof(resolved), written);
    for (int i = 0; i < script->variable_count; i++) {
        if (strcmp(script->variables[i].name, written) == 0) {
            copy_text(resolved, sizeof(resolved), script->variables[i].text);
            break;
        }
    }

    if (strcmp(resolved, "Mod1") == 0 || strcmp(resolved, "mod1") == 0 ||
        strcmp(resolved, "alt") == 0 || strcmp(resolved, "Alt") == 0) {
        return Mod1Mask;
    }
    if (strcmp(resolved, "Mod2") == 0 || strcmp(resolved, "mod2") == 0) {
        return Mod2Mask;
    }
    if (strcmp(resolved, "Mod3") == 0 || strcmp(resolved, "mod3") == 0) {
        return Mod3Mask;
    }
    if (strcmp(resolved, "Mod4") == 0 || strcmp(resolved, "mod4") == 0 ||
        strcmp(resolved, "super") == 0) {
        return Mod4Mask;
    }
    if (strcmp(resolved, "Control") == 0 || strcmp(resolved, "ctrl") == 0) {
        return ControlMask;
    }
    if (strcmp(resolved, "Shift") == 0 || strcmp(resolved, "shift") == 0) {
        return ShiftMask;
    }
    return 0;
}

/* gcl_key.MultiKey(keys=[super_key1, "return"], action="...") — one binding.
 *
 * Every entry in the list but the last is a modifier; the last is the key.
 * That is how the script spells it, and it is why the list is walked from the
 * left rather than the key being picked out of the middle. */
static void add_binding(const Script *script, WmConfig *config,
                        const WmStatement *statement) {
    if (config->binding_count >= WM_CONFIG_MAX_BINDINGS) {
        fprintf(stderr, "gnuchanwm: config: too many key bindings, one ignored\n");
        return;
    }
    const WmValue *keys = wm_config_argument(statement, "keys");
    if (!keys || keys->kind != WM_VALUE_LIST || keys->item_count == 0) {
        return;
    }

    unsigned int modifiers = 0;
    for (int i = 0; i < keys->item_count - 1; i++) {
        char written[WM_CONFIG_TEXT_LENGTH];
        wm_config_value_text(&keys->items[i], written, sizeof(written));
        modifiers |= modifier_of(script, written);
    }
    if (modifiers == 0) {
        return;
    }

    char key[WM_CONFIG_TEXT_LENGTH];
    wm_config_value_text(&keys->items[keys->item_count - 1], key, sizeof(key));
    if (!key[0]) {
        return;
    }

    /* The action, as the script writes it: a call into the runtime.
     *
     *   action=gcl_spawn.RunProgram(command=default_terminal)
     *   action=gcl_window.Close()
     *
     * A call is not a string, so it is not flattened into one. The call's own
     * name is what identifies it — "gcl_spawn.RunProgram" — and its arguments
     * are read as arguments: `command` is resolved through the script's own
     * variables, so `command=default_terminal` becomes the terminal the script
     * named rather than the word "default_terminal". The window manager then
     * looks the name up and runs it with the command it was given; see
     * wm_keys.c's action_for().
     *
     * The bare-string form — action="gcl_window.Close()" — is still read, so a
     * script written before actions were calls keeps working. A string has no
     * arguments, so its command is left empty and the window manager falls
     * back to the terminal the script configured. */
    const WmValue *action_value = wm_config_argument(statement, "action");
    char action[WM_CONFIG_TEXT_LENGTH];
    action[0] = '\0';

    char command[WM_CONFIG_TEXT_LENGTH];
    command[0] = '\0';

    if (action_value && action_value->kind == WM_VALUE_CALL &&
        action_value->call) {
        const WmStatement *call = action_value->call;
        copy_text(action, sizeof(action), call->target);

        /* The program a RunProgram names. It is the call's own argument, read
           through value_text so a name the script assigned resolves: the
           shipped script writes command=default_terminal and means the xterm
           it set above. */
        value_text(script, wm_config_argument(call, "command"),
                   command, sizeof(command));
        /* A first argument written without a name is the program too:
           RunProgram("xterm"). */
        if (!command[0] && call->arg_count > 0 &&
            call->args[0].name[0] == '\0') {
            value_text(script, &call->args[0].value, command, sizeof(command));
        }
    } else {
        value_text(script, action_value, action, sizeof(action));
    }

    WmBinding *binding = &config->bindings[config->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->modifiers = modifiers;
    copy_text(binding->key, sizeof(binding->key), key);
    copy_text(binding->action, sizeof(binding->action), action);
    copy_text(binding->command, sizeof(binding->command), command);
}

/* gcl_keys.all = [gcl_key.MultiKey(...), ...] — the whole key table. */
static void add_bindings_from_list(const Script *script, WmConfig *config,
                                   const WmValue *list) {
    if (!list || list->kind != WM_VALUE_LIST) {
        return;
    }
    for (int i = 0; i < list->item_count; i++) {
        const WmValue *item = &list->items[i];
        if (item->kind == WM_VALUE_CALL && item->call) {
            add_binding(script, config, item->call);
        }
    }
}

/* gcl_autostart.all = ["GnuChanNotification", "GnuChanDock --bottom"] — the
 * programs the session starts by itself.
 *
 * Each entry is a command LINE, so it is read through value_text and a name
 * the script assigned resolves to what it assigned: the same rule the keys and
 * every other written value follow. An entry that is empty after resolution is
 * dropped rather than kept as a blank command, because a blank command is a
 * line in the log that says nothing. The list is a plain list of strings and
 * not a list of calls, so there is nothing here to give an argument name to —
 * the whole line, arguments and all, is the entry. */
static void add_autostart_from_list(const Script *script, WmConfig *config,
                                    const WmValue *list) {
    if (!list || list->kind != WM_VALUE_LIST) {
        return;
    }
    for (int i = 0; i < list->item_count; i++) {
        if (config->autostart_count >= WM_CONFIG_MAX_AUTOSTART) {
            fprintf(stderr,
                    "gnuchanwm: config: too many autostart programs, one "
                    "ignored\n");
            break;
        }
        char text[WM_CONFIG_TEXT_LENGTH];
        value_text(script, &list->items[i], text, sizeof(text));
        if (!text[0]) {
            continue;
        }
        copy_text(config->autostart[config->autostart_count],
                  WM_CONFIG_TEXT_LENGTH, text);
        config->autostart_count++;
    }
}

/* A name given a value, remembered so a later statement can use it: the script
   assigns super_key1 = "Mod1" and then writes keys=[super_key1, ...]. */
static void remember_assignment(Script *script, const WmStatement *statement) {
    if (script->variable_count >= WM_CONFIG_MAX_VARIABLES) {
        return;
    }
    ScriptVariable *variable = &script->variables[script->variable_count++];
    memset(variable, 0, sizeof(*variable));
    copy_text(variable->name, sizeof(variable->name), statement->target);
    wm_config_value_text(&statement->value, variable->text,
                         sizeof(variable->text));
}

/* --- walking the script --------------------------------------------------- */

static void walk(Script *script, WmConfig *config,
                 const WmStatement *statements, int count) {
    for (int i = 0; i < count; i++) {
        const WmStatement *statement = &statements[i];
        if (statement->target[0] == '\0') {
            continue;
        }

        if (statement->kind == WM_STMT_ASSIGN) {
            if (strcmp(statement->target, "default_terminal") == 0) {
                wm_config_value_text(&statement->value, config->terminal,
                                     sizeof(config->terminal));
            } else if (strcmp(statement->target, "gcl_keys.all") == 0) {
                add_bindings_from_list(script, config, &statement->value);
            } else if (strcmp(statement->target, "gcl_autostart.all") == 0) {
                add_autostart_from_list(script, config, &statement->value);
            } else if (strcmp(statement->target,
                              "gcl_Window.BackgroundImage") == 0) {
                /* The wallpaper, written as an assignment rather than a call:
                   `gcl_Window.BackgroundImage = "bg.png"`. It is the desktop
                   behind every window, not the bar — which is why it lives on
                   the config and not on WmBar. */
                value_text(script, &statement->value,
                           config->desktop_background_image,
                           sizeof(config->desktop_background_image));
            } else if (strcmp(statement->target,
                              "gcl_Window.background_color") == 0) {
                /* The flat desktop colour, written the same way:
                   `gcl_Window.background_color = "#27022b"`. It is what the
                   desktop is painted in when no wallpaper is named — or when
                   the one named cannot be read — so commenting the picture out
                   is all it takes to fall back to this colour. */
                value_text(script, &statement->value,
                           config->desktop_background_color,
                           sizeof(config->desktop_background_color));
            } else if (strncmp(statement->target, "gcl_Switcher.", 13) == 0) {
                /* The Alt+` overlay's own colours. They are assignments and
                   not a call for the same reason the two above are: they
                   describe one surface, and there is no list of arguments to
                   give them. See set_switcher_colour(). */
                set_switcher_colour(script, config, statement);
            }
            remember_assignment(script, statement);
            continue;
        }

        if (strcmp(statement->target, "gcl_BAR.call") == 0) {
            set_bar(script, config, statement);
        } else if (strncmp(statement->target, "gcl_Window.", 11) == 0) {
            set_border_colours(config, statement);
        } else if (strncmp(statement->target, "gcl_themes.", 11) == 0) {
            set_theme(config, statement);
        } else if (strcmp(statement->target, "gcl_mouse.MouseBehavior") == 0) {
            set_mouse(config, statement);
        } else if (strcmp(statement->target,
                          "gcl_touchpad.TouchpadBehavior") == 0) {
            set_touchpad(config, statement);
        } else if (strcmp(statement->target, "gcl_Power.Lid") == 0) {
            set_power(config, statement);
        } else if (strcmp(statement->target, "gcl_Power.Idle") == 0) {
            set_idle(config, statement);
        }
        /* Every other call is something this build does not act on. It is not
           reported: the config is shared with a runtime that gives those calls
           their meaning, and calling them unknown would make a valid file look
           broken. */
    }
}

/* --- the public entry points ---------------------------------------------- */

static void bar_default_widget(WmBar *bar, WmWidgetKind kind,
                               const char *background, const char *foreground)
{
    if (bar->widget_count >= WM_CONFIG_MAX_WIDGETS) {
        return;
    }
    WmWidget *widget = &bar->widgets[bar->widget_count++];
    memset(widget, 0, sizeof(*widget));
    widget->kind = kind;
    copy_text(widget->background, sizeof(widget->background), background);
    copy_text(widget->foreground, sizeof(widget->foreground), foreground);
    widget->font_size = 12;
    copy_text(widget->font_family, sizeof(widget->font_family), "monospace");

    /* The same default the parser gives a written widget: an EmptySpace grows
       unless it is told not to. Without this the built-in bar's space would be
       a zero-width gap and the clock would sit against the label instead of
       against the right edge, which is the one thing the built-in bar is for. */
    widget->expanding = 1;
    widget->horizontal = 1;

    if (kind == WM_WIDGET_CURRENT_LAYOUT) {
        widget->start_layout = 0;
        widget->end_layout = 5;
    } else if (kind == WM_WIDGET_CLOCK) {
        copy_text(widget->format, sizeof(widget->format), "%H:%M");
    }
}

void wm_config_defaults(WmConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));

    /* The colours wm_style.c falls back to, so a session with no script is the
       desktop the code was written against. */
    copy_text(config->active_border, sizeof(config->active_border), "#c77dff");
    copy_text(config->inactive_border, sizeof(config->inactive_border), "#32143f");
    /* The third border colour. It is written out here as well as in the
       palette so the shipped script and this default cannot drift: a person
       reading either should see the same orange, and a machine with no script
       should get the same colour a script that named none would. */
    copy_text(config->moved_border, sizeof(config->moved_border), "#ff8a3d");
    config->border_width = 2;

    /* Empty: "look at $TERMINAL, then at the usual terminals", which is what a
       machine that never wrote a script gets. */
    config->terminal[0] = '\0';

    /* The programs a session starts with no script to name them. The
       notification server is the one that matters: a program that notifies a
       moment after it starts — a browser, a mail client, Steam — finds no
       server and the notification is lost. It is written here as well as in
       the shipped script so a machine with no script still gets it, and the
       two cannot drift. The lock screen and the screen saver are NOT here:
       they are run on demand by the idle and lid modules, and starting one at
       login would lock a user out of their own session. */
    copy_text(config->autostart[0], WM_CONFIG_TEXT_LENGTH,
              "GnuChanNotification");
    config->autostart_count = 1;

    /* The three names the installers in dotfile/ actually install under:
       dotfile/GTK_THEME/theme_install.py writes gnuchanpurple's GTK theme as
       GnuChanTheme, dotfile/ICON_THEME writes the icon theme as GnuChanIcon,
       and dotfile/ICON_MOUSE_THEME writes the cursor theme as
       GnuChanMouseIcons. They are the names wm_theme.c looks for, and a name
       that does not match is a theme that is installed but never found. */
    copy_text(config->gtk_theme, sizeof(config->gtk_theme), "GnuChanTheme");
    copy_text(config->icon_theme, sizeof(config->icon_theme), "GnuChanIcon");
    copy_text(config->cursor_theme, sizeof(config->cursor_theme),
              "GnuChanMouseIcons");

    config->mouse_left = WM_MOUSE_SELECT;
    config->mouse_right = WM_MOUSE_MENU;
    config->mouse_middle = WM_MOUSE_PASTE;
    config->mouse_scroll_up = WM_MOUSE_SCROLL_UP;
    config->mouse_scroll_down = WM_MOUSE_SCROLL_DOWN;

    config->touchpad_tap_to_click = 1;
    config->touchpad_two_finger_scroll = 1;

    /* The lid, as a machine that never wrote a script gets it: closing the lid
       suspends the machine (what a laptop is expected to do), no lock before
       the suspend, run the screen saver and not the lock screen on wake. These
       are the same defaults set_power() falls back to, so a script that names
       some of them and not others gets the same answer for the ones it left
       out. The two commands are left empty, which wm_lid.c reads as "look
       GnuChanSS / GnuChanSL up on PATH". */
    config->lid_suspend_on_close = 1;
    config->lock_before_suspend = 0;
    config->lid_open_screensaver = 1;
    config->lid_open_lockscreen = 0;
    config->screensaver_command[0] = '\0';
    config->lockscreen_command[0] = '\0';

    /* The idle desk, as a machine that never wrote a script gets it. On, at
       five minutes — which is the same number GnuChanSS waits for on its own,
       so a session and its saver agree about when "quiet" is — with the screen
       saver and not the lock screen. That is what a person who sets a saver up
       and walks away expects: a picture, not a password. The lock is the
       setting to turn on, and it is off by default so an idle desk does not
       surprise anyone by asking for a password. These are the same values
       set_idle() falls back to, so a script that names some and not others
       gets the same answer for the ones it left out. */
    config->idle_enabled = 1;
    config->idle_seconds = 300;
    config->idle_screensaver = 1;
    config->idle_lockscreen = 0;

    /* The bar the shipped script asks for, so a machine with no script still
       has a bar rather than an empty edge. It is one bar, the first of the
       array, and it is filled the same way the script's own call fills one. */
    config->bar_count = 1;
    WmBar *bar = &config->bars[0];
    bar_defaults(bar);
    bar_default_widget(bar, WM_WIDGET_CURRENT_LAYOUT, "#53055c", "#f069ff");
    bar_default_widget(bar, WM_WIDGET_EMPTY_SPACE, "#940da3", "#f069ff");
    bar_default_widget(bar, WM_WIDGET_CLOCK, "#53055c", "#f069ff");
}

char *wm_config_path(char *buffer, unsigned int size) {
    if (!buffer || size == 0) {
        return buffer;
    }
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanWM/GnuChanWM.py", xdg);
    } else if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanWM/GnuChanWM.py", home);
    } else {
        buffer[0] = '\0';
    }
    return buffer;
}

int wm_config_load(WmConfig *config, const char *path) {
    if (!config || !path || !path[0]) {
        return -1;
    }
    WmStatement *statements = NULL;
    int count = 0;
    if (wm_config_parse_file(path, &statements, &count) != 0) {
        return -1;
    }

    /* A file with nothing in it is not a desktop. An empty file is what an
       interrupted install leaves behind, and it used to parse cleanly into "a
       desktop with no widgets and one workspace" — which silently half-erased
       the built-in one, so the desktop came up looking broken with nothing in
       the log to say why. Refusing it makes the caller fall back to the
       defaults and say so, so the empty file names itself. */
    if (count == 0) {
        wm_config_statements_free(statements, count);
        fprintf(stderr, "gnuchanwm: config: %s is empty; using the defaults\n",
                path);
        return -1;
    }

    /* Which of the collections the script actually names. They are cleared
       only when it names them: a script that says nothing about the bar keeps
       the bar it did not mention, and one that binds no keys keeps the keys it
       did not mention. Without this every field the script left out was reset
       to nothing, so writing one line — a terminal, a colour — also emptied
       the bar and unbound every key, and a small change to a complete desktop
       looked like most of the desktop being deleted. */
    int defines_bar = 0;
    int defines_keys = 0;
    int defines_autostart = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(statements[i].target, "gcl_BAR.call") == 0) {
            defines_bar = 1;
        } else if (strcmp(statements[i].target, "gcl_keys.all") == 0) {
            defines_keys = 1;
        } else if (strcmp(statements[i].target, "gcl_autostart.all") == 0) {
            defines_autostart = 1;
        }
    }

    /* The script is read into a copy first, then swapped in. A script that
       fails to parse leaves the desktop it already had, which is the whole
       reason the read is a separate step: a half-applied config is a desktop
       nobody asked for. */
    WmConfig parsed = *config;
    if (defines_bar) {
        /* The script names its own bars, so the ones read before are dropped
           whole. A script that lists one top bar and one bottom bar means two
           bars, not the two it listed added to whatever was there — so the
           count goes to zero and each gcl_BAR.call(...) appends one. */
        parsed.bar_count = 0;
    }
    if (defines_keys) {
        parsed.binding_count = 0;
    }
    /* The autostart list is a collection, and it is replaced whole for the
       same reason the bar and the keys are: a script that lists what starts
       means exactly what it listed, not that list added to the one read
       before. A script that says nothing about it keeps the list it had — so
       an edit that touches only a colour does not stop the notification
       server from starting. */
    if (defines_autostart) {
        parsed.autostart_count = 0;
    }

    /* The desktop's picture and its flat colour are single settings and not
       collections, so neither is carried over from the read before: a script
       that commented its picture out means "no picture", and a name left over
       from the previous read is a wallpaper that stays on the screen however
       the file is edited. Everything that is carried over — the bar, the keys,
       the border colours — is carried over because a script that says nothing
       about it is a script that did not mean to change it; a background is the
       opposite, since the whole way to ask for no picture is to stop naming
       one. */
    parsed.desktop_background_image[0] = '\0';
    parsed.desktop_background_color[0] = '\0';

    /* The switcher's seven are cleared here for the same reason and by the
       same rule: they are written as assignments, so the way to ask for the
       default back is to stop writing one. A colour left over from the read
       before would be a switcher that cannot be reset by editing the file,
       which is exactly the fault the two above are cleared to avoid. */
    parsed.switcher_background[0] = '\0';
    parsed.switcher_panel[0] = '\0';
    parsed.switcher_cell_border[0] = '\0';
    parsed.switcher_select_border[0] = '\0';
    parsed.switcher_text[0] = '\0';
    parsed.switcher_select_text[0] = '\0';
    parsed.switcher_field[0] = '\0';

    /* The notes are last read's, not this one's: they are collected while the
       file is walked below, and a note left over from a script the user has
       since fixed would name a mistake that is not in the file any more. */
    parsed.notes[0] = '\0';

    Script script;
    memset(&script, 0, sizeof(script));
    walk(&script, &parsed, statements, count);

    wm_config_statements_free(statements, count);

    /* How many workspaces the session has is read from the layout widget that
       draws them: a script asking for start_layout=0, end_layout=5 is asking
       for six, and that is the number the switcher keys are bound from and the
       number the bar hints at. It is taken from what the script asked for
       rather than fixed here, so "the last one the config names" is the rule
       the two places follow and not a constant that has to be kept in step.
       Only a bar the script wrote can answer, so a script that wrote none
       keeps the number it had. */
    if (defines_bar) {
        int highest = -1;
        /* Every bar is asked, not just the first: a layout widget on the
           bottom bar and a range on the top one both count, and the session's
           workspace number is the highest any of them names. */
        for (int b = 0; b < parsed.bar_count; b++) {
            const WmBar *bar = &parsed.bars[b];
            for (int i = 0; i < bar->widget_count; i++) {
                const WmWidget *widget = &bar->widgets[i];
                if (widget->kind != WM_WIDGET_CURRENT_LAYOUT) {
                    continue;
                }
                int top = widget->end_layout;
                if (widget->start_layout > top) {
                    top = widget->start_layout;
                }
                if (top > highest) {
                    highest = top;
                }
            }
        }
        if (highest >= 0 && highest < WM_WORKSPACE_MAX) {
            parsed.workspace_count = highest + 1;
        }
        if (highest >= WM_WORKSPACE_MAX) {
            parsed.workspace_count = WM_WORKSPACE_MAX;
        }
    }
    if (parsed.workspace_count < 1) {
        /* A session with no desks at all cannot place a window on one. */
        parsed.workspace_count = 1;
    }

    *config = parsed;
    return 0;
}
