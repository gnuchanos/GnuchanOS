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
 * It also owns the two things that make the script live rather than a
 * start-up file: the name it is looked for under, and the reload that notices
 * it changed. Reload is by modification time, checked on the loop's idle
 * tick, so saving the script is enough — no key to press, no session to
 * restart.
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
#include "wm_spawn.h"

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

    argument = wm_config_argument(call, "symbol");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->symbol, sizeof(widget->symbol), text);

    argument = wm_config_argument(call, "text");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->text, sizeof(widget->text), text);

    argument = wm_config_argument(call, "format");
    value_text(script, argument, text, sizeof(text));
    copy_text(widget->format, sizeof(widget->format), text);

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

/* gcl_BAR.call(Position=..., Size=..., BackgroundColor=..., Widgets=[...]) */
static void set_bar(const Script *script, WmConfig *config,
                    const WmStatement *statement) {
    WmBar *bar = &config->bar;
    char text[WM_CONFIG_TEXT_LENGTH];

    bar->present = 1;

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

    value_text(script, wm_config_argument(statement, "BackgroundImage"),
               text, sizeof(text));
    copy_text(bar->background_image, sizeof(bar->background_image), text);

    /* Off unless the script says otherwise — no: on unless the script says
       otherwise. See WmBar.vsync; the field is read as a bool with a default
       of 1 so a script that never mentions it gets the flicker-free bar. */
    bar->vsync = wm_config_value_bool(
        wm_config_argument(statement, "Vsync"), 1);

    set_bar_widgets(script, bar, wm_config_argument(statement, "Widgets"));
}

/* --- the other statements ------------------------------------------------- */

static void set_border_colours(WmConfig *config, const WmStatement *statement) {
    /* The script passes the colour as the only argument, so it is positional:
       set_active_window_border_color("#c369ff"). */
    const WmValue *colour = wm_config_argument(statement, "color");
    if (!colour && statement->arg_count > 0) {
        colour = &statement->args[0].value;
    }
    if (!colour) {
        return;
    }
    char text[WM_CONFIG_TEXT_LENGTH];
    wm_config_value_text(colour, text, sizeof(text));

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

    char action[WM_CONFIG_TEXT_LENGTH];
    value_text(script, wm_config_argument(statement, "action"),
               action, sizeof(action));

    WmBinding *binding = &config->bindings[config->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->modifiers = modifiers;
    copy_text(binding->key, sizeof(binding->key), key);
    copy_text(binding->action, sizeof(binding->action), action);
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

static void bar_default_widget(WmConfig *config, WmWidgetKind kind,
                               const char *background, const char *foreground)
{
    if (config->bar.widget_count >= WM_CONFIG_MAX_WIDGETS) {
        return;
    }
    WmWidget *widget = &config->bar.widgets[config->bar.widget_count++];
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
       has a bar rather than an empty edge. */
    config->bar.present = 1;
    config->bar.vsync = 1;
    copy_text(config->bar.position, sizeof(config->bar.position), "top");
    config->bar.size = 24;
    copy_text(config->bar.background, sizeof(config->bar.background), "#27022b");
    bar_default_widget(config, WM_WIDGET_CURRENT_LAYOUT, "#53055c", "#f069ff");
    bar_default_widget(config, WM_WIDGET_EMPTY_SPACE, "#940da3", "#f069ff");
    bar_default_widget(config, WM_WIDGET_CLOCK, "#53055c", "#f069ff");
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

    /* The script is read into a copy first, then swapped in. A script that
       fails to parse leaves the desktop it already had, which is the whole
       reason the read is a separate step: a half-applied config is a desktop
       nobody asked for. */
    WmConfig parsed = *config;
    parsed.bar.widget_count = 0;
    parsed.binding_count = 0;

    Script script;
    memset(&script, 0, sizeof(script));
    walk(&script, &parsed, statements, count);

    wm_config_statements_free(statements, count);

    /* How many workspaces the session has is read from the layout widget that
       draws them: a script asking for start_layout=0, end_layout=5 is asking
       for six, and that is the number the switcher keys are bound from and the
       number the bar hints at. It is taken from what the script asked for
       rather than fixed here, so "the last one the config names" is the rule
       the two places follow and not a constant that has to be kept in step. */
    parsed.workspace_count = 0;
    for (int i = 0; i < parsed.bar.widget_count; i++) {
        const WmWidget *widget = &parsed.bar.widgets[i];
        if (widget->kind != WM_WIDGET_CURRENT_LAYOUT) {
            continue;
        }
        int highest = widget->end_layout;
        if (widget->start_layout > highest) {
            highest = widget->start_layout;
        }
        int count = highest + 1;
        if (count > parsed.workspace_count) {
            parsed.workspace_count = count;
        }
    }
    if (parsed.workspace_count < 1) {
        /* A bar with no layout widget still has a desktop to put windows on. */
        parsed.workspace_count = 1;
    }

    *config = parsed;
    return 0;
}

unsigned int wm_config_bar_edges(const WmConfig *config) {
    if (!config || !config->bar.present) {
        return 0;
    }
    if (strcmp(config->bar.position, "bottom") == 0) {
        return 1u << WM_EDGE_BOTTOM;
    }
    return 1u << WM_EDGE_TOP;
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

    /* A bar that is not present — or is 0 pixels tall, which a hand-written
       script can ask for — leaves the whole screen to the windows. */
    if (!config || !config->bar.present || config->bar.size <= 0) {
        return;
    }

    int strip = config->bar.size;
    if (strip >= screen_height) {
        strip = screen_height > 0 ? screen_height - 1 : 0;
    }
    if (strip <= 0) {
        return;
    }

    /* The bar is along an edge, so it takes height from the top or the bottom
       and leaves the width alone. A bottom bar moves the origin up rather than
       shrinking from the top, which is the whole difference between the two:
       the window has to start below a top bar and stop above a bottom one. */
    if (strcmp(config->bar.position, "bottom") == 0) {
        *height = screen_height - strip;
    } else {
        *y = strip;
        *height = screen_height - strip;
    }
}

/* --- reporting what was read ----------------------------------------------
 *
 * A script that parsed but asked for nothing visible looks exactly like a
 * script that never loaded: the only trace either leaves is one line saying a
 * file was read. The summary below is what tells the two apart. It is written
 * to the log, which is where a session with no terminal can be read back from.
 */

static const char *config_bar_edge_name(const WmConfig *config) {
    if (!config->bar.present) {
        return "none";
    }
    return strcmp(config->bar.position, "bottom") == 0 ? "bottom" : "top";
}

static void config_report(const WmConfig *config, const char *origin) {
    fprintf(stderr,
            "gnuchanwm: config %s: border %s / %s, %d binding(s), "
            "bar %s %dx%d with %d widget(s), %d workspace(s), "
            "terminal '%s'\n",
            origin,
            config->active_border[0] ? config->active_border : "(default)",
            config->inactive_border[0] ? config->inactive_border : "(default)",
            config->binding_count,
            config_bar_edge_name(config),
            config->bar.size,
            config->bar.present,
            config->bar.widget_count,
            config->workspace_count,
            config->terminal[0] ? config->terminal : "(from $TERMINAL)");
    fprintf(stderr,
            "gnuchanwm: config %s: theme %s, icons %s, cursor %s\n",
            origin,
            config->gtk_theme[0] ? config->gtk_theme : "(default)",
            config->icon_theme[0] ? config->icon_theme : "(default)",
            config->cursor_theme[0] ? config->cursor_theme : "(default)");
}

/* --- applying it to the desktop ------------------------------------------- */

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

    /* The theme names are published to the environment the session starts its
       programs from, so a program that reads GTK_THEME sees the same answer a
       toolkit would be given by the theme module. */
    if (core->config.gtk_theme[0]) {
        setenv("GTK_THEME", core->config.gtk_theme, 1);
    }
    if (core->config.cursor_theme[0]) {
        setenv("XCURSOR_THEME", core->config.cursor_theme, 1);
    }

    /* The touchpad settings are pushed into the running X server again when
       they change. wm_input.c applies them at start, but a script whose
       TapToClick was just edited has to take effect on the next save rather
       than at the next login — which is what the whole reload path exists
       for, and a setting that waited would be the one part of the script that
       did not follow it. */
    wm_input_apply(core);
}

/* --- hot reload ----------------------------------------------------------- */

/* When the script was last read, so the tick can tell "unchanged" from
   "written since". Two saves can share a second, so the size is kept beside
   the time; the pair changes whenever a person saves a change. */
static time_t script_time = 0;
static off_t script_size = -1;

int wm_config_reload(WmCore *core) {
    if (!core) {
        return 0;
    }
    char path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(path, sizeof(path));
    if (!path[0]) {
        return 0;
    }

    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    if (info.st_mtime == script_time && info.st_size == script_size) {
        return 0;
    }

    WmConfig fresh = core->config;
    if (wm_config_load(&fresh, path) != 0) {
        /* The script was saved mid-edit or with a mistake in it. The desktop
           keeps what it had and the change waits for the next save, which is
           what makes editing a script safe to do while it is running. The
           stamp is still recorded, so the same broken file is not re-read on
           every tick. */
        fprintf(stderr, "gnuchanwm: config not reloaded; it could not be read\n");
        script_time = info.st_mtime;
        script_size = info.st_size;
        return 0;
    }

    core->config = fresh;
    wm_config_apply(core);
    config_report(&core->config, "reloaded");
    script_time = info.st_mtime;
    script_size = info.st_size;
    fprintf(stderr, "gnuchanwm: config reloaded from %s\n", path);
    return 1;
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
    int have_script = path[0] && stat(path, &info) == 0;
    if (have_script && wm_config_load(&core->config, path) == 0) {
        script_time = info.st_mtime;
        script_size = info.st_size;
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
    .event = NULL,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};
