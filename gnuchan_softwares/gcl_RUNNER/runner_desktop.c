/*
 * runner_desktop.c — one .desktop file, read into a program.
 *
 * The format is the freedesktop one, and only the part a launcher needs is
 * read here. A file is a set of headings — [Desktop Entry], [Desktop Action
 * ...] — and each heading is a set of key = value lines. Only [Desktop Entry]
 * is read: an action is a submenu a file manager draws, and a launcher that
 * listed one would be listing "Open a new window" as if it were a program.
 *
 * Keys this cares about, whole:
 *
 *     Type=Application    what makes the file a program at all
 *     Name=Firefox        what is shown and searched
 *     Comment=Browse      the line under it
 *     Exec=firefox %u     what to run, with the field codes taken out
 *     Terminal=true       whether it has to run in a terminal
 *     NoDisplay=true      installed but not to be shown
 *     Hidden=true         installed but not to be shown
 *
 * The field codes are the specification's own: %u, %U, %f, %F, %i, %c, %k
 * stand for a file, a URL, an icon or a name this launcher never has, and
 * leaving one in would be passing a literal "%u" to the program. They are
 * removed rather than replaced — this launcher runs a program and nothing
 * else, which is what a launcher is for.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "runner_desktop.h"

/* Strip the trailing newline and carriage return a line arrives with, so a
   file written on Windows does not put a stray byte in a name. */
static void trim_end(char *line) {
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' ||
                          line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

static void trim_space(char *line) {
    char *start = line;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1);
    }
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == ' ' ||
                          line[length - 1] == '\t')) {
        line[--length] = '\0';
    }
}

/* Whether a value reads as yes. The specification says the two hidden keys
   are "true" or "false", but files in the wild write True and 1 as well, and
   reading them the way their writer meant is kinder than treating anything
   but the exact word as no. */
static int value_is_true(const char *value) {
    return strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0;
}

/* Take the field codes out of an Exec line.
 *
 * The codes are a '%' and one letter, and there is one that is not a field
 * code at all: "%%" is a literal per cent sign, which the specification
 * defines and a command may contain. So the scan is by hand rather than a
 * replace of every "%?": a percent written twice must come out as one, and a
 * percent written once must go, and those are different answers. */
static void strip_field_codes(const char *source, char *out,
                              unsigned int size) {
    unsigned int written = 0;
    for (unsigned int i = 0; source[i] && written < size - 1; i++) {
        if (source[i] != '%') {
            out[written++] = source[i];
            continue;
        }
        char next = source[i + 1];
        if (next == '%') {
            out[written++] = '%';   /* "%%" is one per cent sign */
            i++;
        } else if (next != '\0') {
            i++;                    /* a field code: dropped, with its letter */
        }
    }
    /* A code at the end of a word leaves the space before it behind — "run
       firefox  %u" becomes "run firefox " — so the tail is tidied. */
    while (written > 0 && (out[written - 1] == ' ' ||
                           out[written - 1] == '\t')) {
        written--;
    }
    out[written] = '\0';
}

/* Read a file into memory, up to a ceiling. NULL when it cannot be read or is
   absurdly large, which the caller treats as "not a program". */
static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || size > 256 * 1024) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    text[got] = '\0';
    fclose(file);
    return text;
}

int runner_desktop_read(const char *path, RunnerProgram *program,
                        int show_no_display, int show_hidden) {
    char *text = read_whole_file(path);
    if (!text) {
        return -1;
    }

    char name[RUNNER_TEXT_LENGTH];
    char generic[RUNNER_TEXT_LENGTH];
    char keywords[RUNNER_TEXT_LENGTH];
    char comment[RUNNER_TEXT_LENGTH];
    char exec[RUNNER_TEXT_LENGTH];
    char type[RUNNER_TEXT_LENGTH];
    int no_display = 0;
    int hidden = 0;
    int in_terminal = 0;

    name[0] = '\0';
    generic[0] = '\0';
    keywords[0] = '\0';
    comment[0] = '\0';
    exec[0] = '\0';
    type[0] = '\0';

    int in_entry = 0;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        trim_end(line);
        trim_space(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            /* A new heading. Only the desktop entry's own keys are read; the
               rest of the file — the actions, the translations — belongs to
               something else. */
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
        trim_space(key);
        trim_space(value);

        /* The last key wins, which is what the specification says and what a
           file that means to override its own name relies on. */
        if (strcmp(key, "Name") == 0) {
            snprintf(name, sizeof(name), "%s", value);
        } else if (strcmp(key, "GenericName") == 0) {
            /* A program's generic name is another word for it — nemo says
               "File Manager" — and a person typing one of those words means
               the program. It is kept so the search can find it. */
            snprintf(generic, sizeof(generic), "%s", value);
        } else if (strcmp(key, "Keywords") == 0) {
            /* The keyword list is the entry's own set of search words, written
               semicolon-separated. The semicolons become spaces so the whole
               list matches letter by letter, the way the name does. */
            snprintf(keywords, sizeof(keywords), "%s", value);
            for (char *k = keywords; *k; k++) {
                if (*k == ';') {
                    *k = ' ';
                }
            }
        } else if (strcmp(key, "Comment") == 0) {
            snprintf(comment, sizeof(comment), "%s", value);
        } else if (strcmp(key, "Exec") == 0) {
            snprintf(exec, sizeof(exec), "%s", value);
        } else if (strcmp(key, "Type") == 0) {
            snprintf(type, sizeof(type), "%s", value);
        } else if (strcmp(key, "Terminal") == 0) {
            in_terminal = value_is_true(value);
        } else if (strcmp(key, "NoDisplay") == 0) {
            no_display = value_is_true(value);
        } else if (strcmp(key, "Hidden") == 0) {
            hidden = value_is_true(value);
        }
    }
    free(text);

    /* What has to be true for the file to be a program: it is an application,
       it has a name to be found by, and it has something to run. A file
       missing any of the three describes no program, whatever else it holds,
       and the many files in the directory that are not programs are all
       turned down here rather than one key at a time. */
    if (strcmp(type, "Application") != 0) {
        return -1;
    }
    if (name[0] == '\0' || exec[0] == '\0') {
        return -1;
    }
    if (!show_no_display && no_display) {
        return -1;
    }
    if (!show_hidden && hidden) {
        return -1;
    }

    memset(program, 0, sizeof(*program));
    snprintf(program->name, sizeof(program->name), "%s", name);
    snprintf(program->comment, sizeof(program->comment), "%s", comment);

    /* The other words the program may be found by, in one field: the generic
       name first, then the keyword list. A search looks at the name, this, and
       the command's own program name (see runner_match.c), so "nemo" finds the
       entry whose displayed name is "Files". */
    if (generic[0] && keywords[0]) {
        snprintf(program->keywords, sizeof(program->keywords), "%s %s",
                 generic, keywords);
    } else if (generic[0]) {
        snprintf(program->keywords, sizeof(program->keywords), "%s", generic);
    } else if (keywords[0]) {
        snprintf(program->keywords, sizeof(program->keywords), "%s", keywords);
    }

    char cleaned[RUNNER_TEXT_LENGTH];
    strip_field_codes(exec, cleaned, sizeof(cleaned));

    if (runner_apps_split_command(program, cleaned) != 0) {
        return -1;      /* a program with nothing left to run is not one */
    }
    program->in_terminal = in_terminal;
    return 0;
}
