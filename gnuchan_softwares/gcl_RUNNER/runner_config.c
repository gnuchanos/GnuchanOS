/*
 * runner_config.c — the walk that gives the settings script its meaning.
 *
 * runner_parser.c reads the file as a name and a value, or a name and a call.
 * This is the other half: it knows which names and which calls a launcher
 * understands, and what each one means. The two are kept apart because they
 * are two jobs — reading a file and understanding it — and only this one needs
 * to know that "GnuChanRunner.add_program" is a program.
 *
 * The names it understands, whole:
 *
 *     Prompt = "Run:"                       a word on the screen
 *     Placeholder = "..."                   the line before anything is typed
 *     EmptyMessage = "..."                  the line when nothing matches
 *     FontFamily = "monospace"              the font the window is drawn in
 *     FontSize = 12
 *     Width = 520                           the window's size
 *     Rows = 8                              how many matches fit at once
 *     Position = "center"                   where it sits, "top" or "bottom"
 *     CaseSensitive = False                 whether a capital letter matters
 *     Fuzzy = True                          whether "xte" finds "xterm"
 *     CommandMode = "typed"                 "off" / "typed" / "always"
 *     ShowNoDisplay = False                 whether hidden entries are listed
 *     ShowHidden = False
 *     Background / Panel / PanelEdge / Field / Text / TextMuted / Accent
 *
 *     GnuChanRunner.add_program(name="...", command="...")
 *     GnuChanRunner.add_desktop_dir(path="...")
 *
 * A name that is not one of these is not a mistake worth refusing the file
 * for: a script is written while it is being lived with, and a value the
 * launcher does not use is a value it does not use. That is different from a
 * statement that cannot be READ — an unclosed bracket, a missing quote —
 * which is refused, because a half-read file is a file nobody can trust.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_config.h"

/* --- the launcher a machine with no settings file gets -------------------- */

void runner_config_defaults(RunnerConfig *config) {
    memset(config, 0, sizeof(*config));

    snprintf(config->prompt, sizeof(config->prompt), "Run:");
    snprintf(config->placeholder, sizeof(config->placeholder),
             "Type to search programs");
    snprintf(config->empty_message, sizeof(config->empty_message),
             "Nothing matches");

    snprintf(config->font_family, sizeof(config->font_family), "monospace");
    config->font_size = 13;

    /* Wide enough for a program's name and its description on one line, and
       eight rows tall: enough to see what a query found without the list
       becoming the whole screen. */
    config->width = 520;
    config->rows = 8;
    snprintf(config->position, sizeof(config->position), "center");

    /* The same purple the window manager and the greeter are drawn in, as
       names. They are resolved to pixels by whoever has a display. */
    snprintf(config->background, sizeof(config->background), "#1a0b2e");
    snprintf(config->panel, sizeof(config->panel), "#32143f");
    snprintf(config->panel_edge, sizeof(config->panel_edge), "#7b2cbf");
    snprintf(config->field, sizeof(config->field), "#241033");
    snprintf(config->text, sizeof(config->text), "#e0c3fc");
    snprintf(config->text_muted, sizeof(config->text_muted), "#9d7bba");
    snprintf(config->accent, sizeof(config->accent), "#c77dff");

    /* Forgiving by default. A capital letter is not something a person
       remembers about a program's name, and a launcher that missed a program
       for one would be a launcher nobody uses twice. */
    config->case_sensitive = 0;
    config->fuzzy = 1;

    /* A command line is offered when what was typed matches no program: that
       is the moment a person wants one, and offering it on every query would
       put a line that runs anything above the programs they meant. */
    config->command_mode = RUNNER_COMMAND_TYPED;

    config->desktop_dir_count = 0;
    config->extra_program_count = 0;
    config->show_no_display = 0;
    config->show_hidden = 0;
    config->error[0] = '\0';
}

/* --- reading one call ----------------------------------------------------- */

/* If the call passed an argument called `name`, copy its text into out and
   return 1. Returns 0 when the call did not pass it, which is what lets a
   caller keep the value it already has.

   There is no fallback parameter, and that is deliberate: a caller that
   already has a value passes the same buffer it wants filled, and a helper
   that copied that buffer onto itself would be calling snprintf with its
   source and destination overlapping — which glibc answers by emptying the
   string. So the caller keeps what it has, and this only ever writes a value
   it read. */
static int argument_text(const RunnerStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const RunnerValue *value = runner_argument(statement, name);
    if (!value) {
        return 0;
    }
    runner_value_text(value, out, size);
    return out[0] != '\0';
}

/* One program the script adds itself. It needs a name to be found by and a
   command to run; a call missing either is not a program and is skipped, which
   a launcher can do without refusing the whole file. */
static void read_add_program(RunnerConfig *config,
                             const RunnerStatement *statement) {
    if (config->extra_program_count >= RUNNER_MAX_EXTRA_PROGRAMS) {
        return;
    }

    /* Both spellings work: the named pair, and two positional ones —
       `add_program("Calculator", "galculator")` reads the way a person would
       write it. A named argument wins when both are given, because it is the
       one that says which is which. */
    char name[RUNNER_TEXT_LENGTH];
    char command[RUNNER_TEXT_LENGTH];
    name[0] = '\0';
    command[0] = '\0';

    const RunnerValue *first = runner_argument_first(statement);
    if (first) {
        runner_value_text(first, name, sizeof(name));
    }

    /* The second positional, which is the one after the first: the parser
       keeps them in order, so it is found by walking past the first. */
    int seen = 0;
    for (int i = 0; i < statement->arg_count; i++) {
        if (statement->args[i].name[0] != '\0') {
            continue;
        }
        if (seen == 1) {
            runner_value_text(&statement->args[i].value, command,
                              sizeof(command));
            break;
        }
        seen++;
    }

    /* A named argument wins over the positional one it would otherwise stand
       for, because it is the one that says which is which. */
    argument_text(statement, "name", name, sizeof(name));
    argument_text(statement, "command", command, sizeof(command));

    if (name[0] == '\0' || command[0] == '\0') {
        return;
    }

    snprintf(config->extra_programs[config->extra_program_count].name,
             sizeof(config->extra_programs[0].name), "%s", name);
    snprintf(config->extra_programs[config->extra_program_count].command,
             sizeof(config->extra_programs[0].command), "%s", command);
    config->extra_program_count++;
}

/* One directory the scan reads. The default is left in place when the script
   names none, so a machine that never mentions it reads where programs are
   installed. */
static void read_add_desktop_dir(RunnerConfig *config,
                                 const RunnerStatement *statement) {
    if (config->desktop_dir_count >= RUNNER_MAX_LIST_ITEMS) {
        return;
    }

    char path[RUNNER_TEXT_LENGTH];
    path[0] = '\0';

    const RunnerValue *first = runner_argument_first(statement);
    if (first) {
        runner_value_text(first, path, sizeof(path));
    }
    argument_text(statement, "path", path, sizeof(path));

    if (path[0] == '\0') {
        return;
    }

    snprintf(config->desktop_dirs[config->desktop_dir_count],
             sizeof(config->desktop_dirs[0]), "%s", path);
    config->desktop_dir_count++;
}

/* A directory named as a plain assignment — `DesktopDirs = ["/a", "/b"]` —
   read item by item. A list is the one assignment whose value is more than
   one thing, and the only one that needs its items walked. */
static void read_desktop_dirs_list(RunnerConfig *config,
                                   const RunnerValue *value) {
    if (!value || value->kind != RUNNER_VALUE_LIST) {
        return;
    }
    config->desktop_dir_count = 0;
    for (int i = 0; i < value->item_count; i++) {
        if (config->desktop_dir_count >= RUNNER_MAX_LIST_ITEMS) {
            return;
        }
        char path[RUNNER_TEXT_LENGTH];
        runner_value_text(&value->items[i], path, sizeof(path));
        if (path[0] == '\0') {
            continue;
        }
        snprintf(config->desktop_dirs[config->desktop_dir_count],
                 sizeof(config->desktop_dirs[0]), "%s", path);
        config->desktop_dir_count++;
    }
}

/* --- the walk ------------------------------------------------------------- */

/* One assignment. The target is compared whole: `Prompt` is the prompt, and
   anything else — a name a script kept for its own use, a word the launcher
   does not have — is left alone rather than reported. */
static void apply_assignment(RunnerConfig *config,
                             const RunnerStatement *statement) {
    const char *target = statement->target;
    char text[RUNNER_TEXT_LENGTH];
    text[0] = '\0';

    if (strcmp(target, "Prompt") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->prompt, sizeof(config->prompt), "%s", text);
    } else if (strcmp(target, "Placeholder") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->placeholder, sizeof(config->placeholder), "%s", text);
    } else if (strcmp(target, "EmptyMessage") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->empty_message, sizeof(config->empty_message), "%s", text);
    } else if (strcmp(target, "FontFamily") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->font_family, sizeof(config->font_family), "%s", text);
    } else if (strcmp(target, "FontSize") == 0) {
        config->font_size = runner_value_number(&statement->value, config->font_size);
    } else if (strcmp(target, "Width") == 0) {
        config->width = runner_value_number(&statement->value, config->width);
    } else if (strcmp(target, "Rows") == 0) {
        config->rows = runner_value_number(&statement->value, config->rows);
    } else if (strcmp(target, "Position") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->position, sizeof(config->position), "%s", text);
    } else if (strcmp(target, "Background") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->background, sizeof(config->background), "%s", text);
    } else if (strcmp(target, "Panel") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->panel, sizeof(config->panel), "%s", text);
    } else if (strcmp(target, "PanelEdge") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->panel_edge, sizeof(config->panel_edge), "%s", text);
    } else if (strcmp(target, "Field") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->field, sizeof(config->field), "%s", text);
    } else if (strcmp(target, "Text") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->text, sizeof(config->text), "%s", text);
    } else if (strcmp(target, "TextMuted") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->text_muted, sizeof(config->text_muted), "%s", text);
    } else if (strcmp(target, "Accent") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (text[0]) snprintf(config->accent, sizeof(config->accent), "%s", text);
    } else if (strcmp(target, "CaseSensitive") == 0) {
        config->case_sensitive = runner_value_bool(&statement->value,
                                                   config->case_sensitive);
    } else if (strcmp(target, "Fuzzy") == 0) {
        config->fuzzy = runner_value_bool(&statement->value, config->fuzzy);
    } else if (strcmp(target, "ShowNoDisplay") == 0) {
        config->show_no_display = runner_value_bool(&statement->value,
                                                    config->show_no_display);
    } else if (strcmp(target, "ShowHidden") == 0) {
        config->show_hidden = runner_value_bool(&statement->value,
                                                config->show_hidden);
    } else if (strcmp(target, "DesktopDirs") == 0) {
        read_desktop_dirs_list(config, &statement->value);
    } else if (strcmp(target, "CommandMode") == 0) {
        runner_value_text(&statement->value, text, sizeof(text));
        if (strcmp(text, "always") == 0) {
            config->command_mode = RUNNER_COMMAND_ALWAYS;
        } else if (strcmp(text, "off") == 0) {
            config->command_mode = RUNNER_COMMAND_OFF;
        } else if (strcmp(text, "typed") == 0) {
            config->command_mode = RUNNER_COMMAND_TYPED;
        }
    }
    /* Anything else is a name this launcher does not have. It is left alone:
       a script is lived in, and not every name in it is meant for here. */
}

/* One call. Only the two the launcher offers are acted on; the rest are
   names the script used for its own arrangement. */
static void apply_call(RunnerConfig *config,
                       const RunnerStatement *statement) {
    const char *target = statement->target;

    if (strcmp(target, "GnuChanRunner.add_program") == 0 ||
        strcmp(target, "add_program") == 0) {
        read_add_program(config, statement);
    } else if (strcmp(target, "GnuChanRunner.add_desktop_dir") == 0 ||
               strcmp(target, "add_desktop_dir") == 0) {
        read_add_desktop_dir(config, statement);
    }
}

/* --- the public entry points ---------------------------------------------- */

char *runner_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanRunner/config.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanRunner/config.py", home);
        return buffer;
    }
    /* No home and no XDG directory: there is nowhere to look, and the caller
       reads that as "no script" rather than as an error. */
    snprintf(buffer, size, "%s", "");
    return buffer;
}

int runner_config_load(RunnerConfig *config, const char *path) {
    /* The defaults are laid down first, and the walk only ever changes what
       the file names. That is what makes a settings file able to name one
       colour and get the rest of the desktop it expects: the file says what
       is different, not what everything is. */
    runner_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    RunnerStatement *statements = NULL;
    int count = 0;
    if (runner_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 runner_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == RUNNER_STMT_ASSIGN) {
            apply_assignment(config, &statements[i]);
        } else {
            apply_call(config, &statements[i]);
        }
    }
    runner_parse_free(statements, count);

    /* Two settings with a sensible floor, clamped here rather than at every
       use: a window narrower than a word or a font of no size is a window
       nobody can read, and the file may say so by mistake. */
    if (config->width < 160) {
        config->width = 160;
    }
    if (config->rows < 1) {
        config->rows = 1;
    }
    if (config->font_size <= 0) {
        config->font_size = 13;
    }
    if (config->font_family[0] == '\0') {
        snprintf(config->font_family, sizeof(config->font_family), "monospace");
    }

    return 0;
}
