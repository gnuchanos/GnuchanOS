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
    config->border_width = 2;

    /* Empty: "look at $TERMINAL, then at the usual terminals", which is what a
       machine that never wrote a script gets. */
    config->terminal[0] = '\0';

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
    for (int i = 0; i < count; i++) {
        if (strcmp(statements[i].target, "gcl_BAR.call") == 0) {
            defines_bar = 1;
        } else if (strcmp(statements[i].target, "gcl_keys.all") == 0) {
            defines_keys = 1;
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

unsigned int wm_config_bar_edges(const WmConfig *config) {
    if (!config) {
        return 0;
    }
    /* Every bar is asked, and the answer is the union of them: a session with
       a top bar and a bottom bar takes room at both edges, and a window is
       kept clear of each. A bar that placed itself with X and Y rather than
       naming an edge is not an edge bar and contributes nothing here — it is
       floating somewhere on the screen rather than against a side. */
    unsigned int edges = 0;
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        if (!bar->present || bar->size <= 0) {
            continue;
        }
        if (strcmp(bar->position, "bottom") == 0) {
            edges |= 1u << WM_EDGE_BOTTOM;
        } else if (strcmp(bar->position, "left") == 0) {
            edges |= 1u << WM_EDGE_LEFT;
        } else if (strcmp(bar->position, "right") == 0) {
            edges |= 1u << WM_EDGE_RIGHT;
        } else {
            edges |= 1u << WM_EDGE_TOP;
        }
    }
    return edges;
}

WmMouseAction wm_config_mouse_action(const WmConfig *config,
                                     unsigned int button) {
    if (!config) {
        return WM_MOUSE_NONE;
    }
    switch (button) {
    case 1: return config->mouse_left;
    case 2: return config->mouse_middle;
    case 3: return config->mouse_right;
    case 4: return config->mouse_scroll_up;
    case 5: return config->mouse_scroll_down;
    default: return WM_MOUSE_NONE;
    }
}

void wm_config_workarea(const WmConfig *config, int screen_width,
                        int screen_height, int *x, int *y,
                        int *width, int *height) {
    *x = 0;
    *y = 0;
    *width = screen_width;
    *height = screen_height;
    if (!config) {
        return;
    }

    /* Every edge bar takes its strip out of the workarea, one after another,
       so a top bar *and* a bottom bar both come off the height and a left bar
       off the width. Only bars that named an edge count: one placed with X and
       Y floats over the screen and does not sit against a side, so it takes no
       room from the windows below. A strip that would leave nothing is held so
       that at least one pixel of screen is left, which is what stops a script
       asking for a bar taller than the screen from producing a negative
       workarea. */
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        if (!bar->present || bar->size <= 0) {
            continue;
        }
        int strip = bar->size;

        if (strcmp(bar->position, "bottom") == 0) {
            if (strip >= *height) {
                strip = *height > 0 ? *height - 1 : 0;
            }
            *height -= strip;
        } else if (strcmp(bar->position, "left") == 0) {
            if (strip >= *width) {
                strip = *width > 0 ? *width - 1 : 0;
            }
            *x += strip;
            *width -= strip;
        } else if (strcmp(bar->position, "right") == 0) {
            if (strip >= *width) {
                strip = *width > 0 ? *width - 1 : 0;
            }
            *width -= strip;
        } else if (strncmp(bar->position, "top", 3) == 0 ||
                   bar->position[0] == '\0') {
            if (strip >= *height) {
                strip = *height > 0 ? *height - 1 : 0;
            }
            *y += strip;
            *height -= strip;
        }
    }
}

/* --- reporting what was read ----------------------------------------------
 *
 * A script that parsed but asked for nothing visible looks exactly like a
 * script that never loaded: the only trace either leaves is one line saying a
 * file was read. The summary below is what tells the two apart. It is written
 * to the log, which is where a session with no terminal can be read back from.
 */

static void config_report(const WmConfig *config, const char *origin) {
    fprintf(stderr,
            "gnuchanwm: config %s: border %s / %s, %d binding(s), "
            "%d bar(s), %d workspace(s), terminal '%s'\n",
            origin,
            config->active_border[0] ? config->active_border : "(default)",
            config->inactive_border[0] ? config->inactive_border : "(default)",
            config->binding_count,
            config->bar_count,
            config->workspace_count,
            config->terminal[0] ? config->terminal : "(from $TERMINAL)");
    /* One line per bar, so a wrong pose or a bar that did not appear can be
       told apart from a bar that was never read. The empty spaces named are
       the pair the bar's own pose uses, so the line never shows a number that
       was not applied. */
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        int vertical = strcmp(bar->pose, "vertical") == 0;
        fprintf(stderr,
                "gnuchanwm: config %s: bar %d %s %s size %d, %d widget(s), "
                "at (%d,%d), space %d/%d\n",
                origin, i, bar->position[0] ? bar->position : "(top)",
                bar->pose[0] ? bar->pose : "horizontal",
                bar->size, bar->widget_count, bar->x, bar->y,
                vertical ? bar->up_empty : bar->left_empty,
                vertical ? bar->down_empty : bar->right_empty);
    }
    fprintf(stderr,
            "gnuchanwm: config %s: theme %s, icons %s, cursor %s\n",
            origin,
            config->gtk_theme[0] ? config->gtk_theme : "(default)",
            config->icon_theme[0] ? config->icon_theme : "(default)",
            config->cursor_theme[0] ? config->cursor_theme : "(default)");
}

/* --- applying it to the desktop ------------------------------------------- */

/* The message window lives further down this file — it belongs with the other
   window the config module owns — so the one call that reaches it from here
   needs the name stated first. */
void wm_config_show_message(WmCore *core, const char *title,
                            const char *detail, const char *where);

void wm_config_apply(WmCore *core) {
    if (!core || !core->display) {
        return;
    }

    /* The script's terminal is published to the spawn module, which Alt+Enter
       and the first window both go through. A name the machine cannot run is
       discarded there, so a typo falls back to $TERMINAL rather than leaving
       the key opening nothing. */
    wm_spawn_set_terminal(core->config.terminal);

    /* The two frame colours are the ones the script sets through gcl_Window;
       the rest of the palette stays what wm_style.c resolved. */
    if (core->config.active_border[0]) {
        core->style.border = wm_style_colour(core->display, core->screen,
                                             core->config.active_border,
                                             core->style.border);
    }
    if (core->config.inactive_border[0]) {
        core->style.border_unfocused =
            wm_style_colour(core->display, core->screen,
                            core->config.inactive_border,
                            core->style.border_unfocused);
    }
    if (core->config.border_width > 0) {
        core->style.border_width = core->config.border_width;
    }

    /* The border width is a number every open frame's geometry is computed
       from, so a script that changed it has to reach the windows that are
       already open. wm_frame_apply_border() puts each frame back together at
       the new width; a screen with no windows is a no-op. */
    wm_frame_apply_border(core);

    /* And every frame is drawn again, whatever the width did. The two colours
       above are what a frame's border and its title line are drawn in, and
       they are read at draw time — so a script that changed only a colour
       would otherwise leave the windows already on screen showing the old one
       until something else happened to repaint them. Doing it here, in the one
       place both reload paths go through, is what makes a colour change appear
       the moment the script is read. */
    wm_frame_draw_all(core);

    /* The theme names are published the same three ways the first run published
       them: the GTK settings files, the resource manager where the cursor is
       named, and the environment. This is not only setenv: a reload that
       changed gcl_themes.Theme_icon(...) has to reach ~/.config/gtk-3.0, and a
       reload that changed the cursor has to reach Xcursor.theme, or half the
       desktop keeps the theme the session started with. wm_theme_apply() is
       the whole of that, and it is the same call the module's init makes. */
    wm_theme_apply(core);

    /* The touchpad settings are pushed into the running X server again when
       they change. wm_input.c applies them at start, but a script whose
       TapToClick was just edited has to take effect on the next save rather
       than at the next login — which is what the whole reload path exists
       for, and a setting that waited would be the one part of the script that
       did not follow it. */
    wm_input_apply(core);

    /* Whatever the script asked for that could not be given — a bar with an
       empty-space setting that belongs to the other pose — is shown once here,
       because this is the one place both reads pass through and both have a
       core to draw on. The reader that found the mistake only had the file: it
       recorded the notes on the config (see WmConfig.notes and set_bar()) and
       the message goes up now, where the person who wrote the script will see
       it. The log has every note whether or not a message can be drawn. */
    if (core->config.notes[0]) {
        fprintf(stderr, "gnuchanwm: config notes: %s\n", core->config.notes);
        wm_config_show_message(core,
                               "GnuChanWM: the config was applied, with notes",
                               core->config.notes,
                               "the log has the same list");
    }
}

/* --- the window a config mistake is shown in ------------------------------ */

/* When the reload key asks for the script and it cannot be read, the reason is
   put on screen and not only in the log.
 *
 * A person who pressed the key is looking at the desktop, not at a file: a
 * change that silently did not happen reads as a key that does nothing, and
 * the log is somewhere a running session has no terminal to read. So the
 * mistake is shown where the person already is — the statement that could not
 * be understood, and the file it came from. The window is override-redirect so
 * the manager does not try to manage its own message, and it is closed by a
 * click or a key, which is the one gesture a reader will try. */
#define WM_ERROR_WINDOW_WIDTH  640
#define WM_ERROR_WINDOW_HEIGHT 168
#define WM_ERROR_PADDING       16
#define WM_ERROR_LINE_HEIGHT   22

static Window error_window = None;

/* The message window, or None when no message is up. Read by the menu's logout
   walk, which has to leave the windows this manager owns alone: killing one
   closes this manager's own connection, and the kills queued behind it are
   discarded with it. */
Window wm_config_error_window(void) {
    return error_window;
}
static char error_title[WM_CONFIG_TEXT_LENGTH];
static char error_detail[WM_CONFIG_TEXT_LENGTH * 2];
/* Room for "in " and a path: the path buffer the reload builds is four text
   lengths, so the line that names the file is one length larger than that. A
   short buffer here would silently clip the path of the very file the message
   exists to name. */
static char error_where[WM_CONFIG_TEXT_LENGTH * 5];

static void config_draw_error(WmCore *core) {
    if (error_window == None) {
        return;
    }
    Display *display = core->display;
    XftFont *font = core->style.font;

    XSetForeground(display, core->gc, core->style.panel);
    XFillRectangle(display, error_window, core->gc, 0, 0,
                   WM_ERROR_WINDOW_WIDTH, WM_ERROR_WINDOW_HEIGHT);
    XSetForeground(display, core->gc, core->style.accent);
    XDrawRectangle(display, error_window, core->gc, 0, 0,
                   WM_ERROR_WINDOW_WIDTH - 1, WM_ERROR_WINDOW_HEIGHT - 1);

    if (!font) {
        return;
    }
    int baseline = WM_ERROR_PADDING + font->ascent;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_title, core->style.accent);
    baseline += WM_ERROR_LINE_HEIGHT;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_detail, core->style.text);
    baseline += WM_ERROR_LINE_HEIGHT;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_where,
                  core->style.text_muted);
    XFlush(display);
}

/* Show a message on the screen. Public because the problems a session can have
   are not all the config module's: a settings picture that will not load, a
   key that cannot be bound, no terminal to open. Every one of them reaches the
   log — a session started by a display manager has no terminal, so the log is
   the record — but a log is where a person looks after they have already
   noticed something is wrong, and the whole point of a message is to be the
   thing they notice. So the same message that goes to the log is put in a
   window here.
 *
 * The window is override-redirect so the manager does not try to manage its own
 * message, and it is closed by a click or a key. A message already up is
 * replaced rather than stacked: two windows, one behind the other, is one
 * message nobody can read. */
void wm_config_show_message(WmCore *core, const char *title,
                            const char *detail, const char *where) {
    if (!core || !core->display) {
        return;
    }

    snprintf(error_title, sizeof(error_title), "%s",
             (title && title[0]) ? title : "GnuChanWM");
    snprintf(error_detail, sizeof(error_detail), "%s",
             (detail && detail[0]) ? detail : "something went wrong");
    snprintf(error_where, sizeof(error_where), "%s",
             (where && where[0]) ? where : "");

    fprintf(stderr, "gnuchanwm: message shown: %s (%s)\n",
            error_detail, error_where);

    if (error_window != None) {
        XDestroyWindow(core->display, error_window);
        error_window = None;
    }

    int x = (core->width - WM_ERROR_WINDOW_WIDTH) / 2;
    if (x < 0) {
        x = 0;
    }
    int y = (core->height - WM_ERROR_WINDOW_HEIGHT) / 3;
    if (y < 0) {
        y = 0;
    }

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.panel;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    error_window = XCreateWindow(core->display, core->root,
                                 x, y,
                                 WM_ERROR_WINDOW_WIDTH, WM_ERROR_WINDOW_HEIGHT,
                                 1, CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask,
                                 &attributes);
    if (error_window == None) {
        return;
    }
    XStoreName(core->display, error_window, "GnuChanWM");
    XMapRaised(core->display, error_window);
    XFlush(core->display);
    config_draw_error(core);
}

/* Show the reason the script could not be applied. Called from the forced
   reload, which is the path a person took by pressing the key, so it is the
   path where a message on screen is what they asked for. */
static void config_show_error(WmCore *core, const char *path,
                              const char *reason) {
    char where[WM_CONFIG_TEXT_LENGTH * 4 + 8];
    snprintf(where, sizeof(where), "in %s", path ? path : "");
    wm_config_show_message(core,
                           "GnuChanWM: the config was not applied",
                           (reason && reason[0]) ? reason
                                                 : "the file could not be read",
                           where);
}

int wm_config_reload_forced(WmCore *core) {
    if (!core) {
        return 0;
    }
    char path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(path, sizeof(path));
    if (!path[0]) {
        return 0;
    }

    /* The unchanged-file shortcut is deliberately skipped: the key means
       "read it again now", and a person who saved a change between two ticks
       would otherwise press it and see nothing. */
    WmConfig fresh = core->config;
    if (wm_config_load(&fresh, path) != 0) {
        /* A script that does not parse is not a half-desktop: the copy above
           is thrown away, the desktop keeps what it had, and the reason is
           put on screen. */
        config_show_error(core, path, wm_config_last_error());
        return 0;
    }

    core->config = fresh;
    wm_config_apply(core);
    config_report(&core->config, "reloaded");
    fprintf(stderr, "gnuchanwm: config reloaded from %s\n", path);
    return 1;
}

/* The module's event callback: the error window's own two gestures. Every
   other event is passed over, so adding this callback costs the rest of the
   loop one comparison. */
static void config_event(WmCore *core, XEvent *event) {
    if (error_window == None || event->xany.window != error_window) {
        return;
    }
    switch (event->type) {
    case Expose:
        if (event->xexpose.count == 0) {
            config_draw_error(core);
        }
        break;
    case ButtonPress:
    case KeyPress:
        XDestroyWindow(core->display, error_window);
        error_window = None;
        XFlush(core->display);
        break;
    default:
        break;
    }
}

/* --- the module ----------------------------------------------------------- */

static int config_init(WmCore *core) {
    if (!core) {
        return -1;
    }
    wm_config_defaults(&core->config);

    char path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(path, sizeof(path));

    struct stat info;
    if (path[0] && stat(path, &info) == 0 &&
        wm_config_load(&core->config, path) == 0) {
        fprintf(stderr, "gnuchanwm: config read from %s\n", path);
    } else {
        /* No script is not a failure: it is a machine that never wrote one,
           and it gets the built-in desktop — including the built-in bar. */
        fprintf(stderr, "gnuchanwm: no config at %s; using the defaults\n",
                path[0] ? path : "(no home directory)");
    }

    wm_config_apply(core);
    config_report(&core->config, "in use");
    return 0;
}

const WmModule wm_config_module = {
    .name = "config",
    .init = config_init,
    .event = config_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};
