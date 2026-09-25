/*
 * dm_sessions.c — read the session files this machine has installed.
 *
 * This is a small reader for one file format, not a general .desktop parser:
 * the greeter needs two keys, and the specification's other few hundred are
 * somebody else's problem. What matters is that a session the machine offers
 * is a session the user can choose, so the list is whatever is on disk.
 *
 * Two directories are read, the distribution's and the local one.
 * /usr/local/share/xsessions is searched too because that is where a session
 * installed by hand — and where this project's own makefile can put one —
 * lands, and a greeter that only read /usr would not offer it.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dm_sessions.h"

/* The session this system ships. It is put first in the list because it is
   the one a user of this system is here for; nothing else is special-cased. */
static const char *PREFERRED = "GnuChanWM";

static const char *SESSION_DIRS[] = {
    "/usr/share/xsessions",
    "/usr/local/share/xsessions",
    NULL,
};

/* --- reading one file ------------------------------------------------------ */

/* The value of a key in a .desktop file, or an empty string.
 *
 * Only the [Desktop Entry] group is read, because a file may carry other
 * groups and a key in one of them — a Translations entry, say — is not the
 * key the greeter wants. Reading past the group is how a name in the wrong
 * language ends up on the button.
 */
static void read_key(const char *text, const char *key,
                     char *out, size_t size) {
    out[0] = '\0';
    size_t key_length = strlen(key);

    const char *line = text;
    int in_entry_group = 0;

    while (*line) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);

        if (length > 0 && line[0] == '[') {
            /* A group header. Only the one this reader understands is entered. */
            in_entry_group = length >= 15 &&
                             strncmp(line, "[Desktop Entry]", 15) == 0;
        } else if (in_entry_group && length > key_length &&
                   strncmp(line, key, key_length) == 0 &&
                   line[key_length] == '=') {
            const char *value = line + key_length + 1;
            size_t value_length = length - key_length - 1;
            if (value_length >= size) {
                value_length = size - 1;
            }
            while (value_length > 0 &&
                   (value[value_length - 1] == '\r' || value[value_length - 1] == '\n')) {
                value_length--;
            }
            while (value_length > 0 && (*value == ' ' || *value == '\t')) {
                value++;
                value_length--;
            }
            while (value_length > 0 &&
                   (value[value_length - 1] == ' ' || value[value_length - 1] == '\t')) {
                value_length--;
            }
            memcpy(out, value, value_length);
            out[value_length] = '\0';
            return;
        }

        if (!end) {
            break;
        }
        line = end + 1;
    }
}

/* Whether a file says it should not be shown. A session marked Hidden or
   NoDisplay is installed but not offered — that is what the keys mean, and a
   greeter that ignored them would list sessions the machine deliberately
   took out of its menu. */
static int suppressed(const char *text) {
    char value[16];
    read_key(text, "Hidden", value, sizeof(value));
    if (value[0] == 't' || value[0] == 'T' || value[0] == '1') {
        return 1;
    }
    read_key(text, "NoDisplay", value, sizeof(value));
    if (value[0] == 't' || value[0] == 'T' || value[0] == '1') {
        return 1;
    }
    return 0;
}

static char *slurp(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || size > 1024 * 1024) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *text = (char *)malloc((size_t)size + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    text[got] = '\0';
    fclose(file);
    return text;
}

/* --- one directory --------------------------------------------------------- */

static int scan_directory(const char *directory, DmSession *out,
                          int count, int max) {
    DIR *dir = opendir(directory);
    if (!dir) {
        return count;
    }

    struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".desktop") != 0) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        char *text = slurp(path);
        if (!text) {
            continue;
        }
        if (suppressed(text)) {
            free(text);
            continue;
        }

        char name[DM_SESSION_NAME];
        char exec[DM_SESSION_EXEC];
        read_key(text, "Name", name, sizeof(name));
        read_key(text, "Exec", exec, sizeof(exec));
        free(text);

        /* A session with no name cannot be shown and one with no command
           cannot be run; either way there is nothing to offer. */
        if (name[0] == '\0' || exec[0] == '\0') {
            continue;
        }

        /* The same session in both directories is one session. */
        int duplicate = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(out[i].name, name) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        snprintf(out[count].name, sizeof(out[count].name), "%s", name);
        snprintf(out[count].exec, sizeof(out[count].exec), "%s", exec);
        count++;
    }

    closedir(dir);
    return count;
}

/* --- the list -------------------------------------------------------------- */

static int by_name(const void *left, const void *right) {
    const DmSession *a = (const DmSession *)left;
    const DmSession *b = (const DmSession *)right;
    return strcmp(a->name, b->name);
}

/* Move the session called `name` to the front, keeping the rest in order. */
static void promote(DmSession *out, int count, const char *name) {
    int found = dm_sessions_find(out, count, name);
    if (found <= 0) {
        return;
    }
    DmSession first = out[found];
    for (int i = found; i > 0; i--) {
        out[i] = out[i - 1];
    }
    out[0] = first;
}

int dm_sessions_scan(DmSession *out, int max) {
    if (!out || max <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; SESSION_DIRS[i] && count < max; i++) {
        count = scan_directory(SESSION_DIRS[i], out, count, max);
    }

    if (count > 1) {
        qsort(out, (size_t)count, sizeof(out[0]), by_name);
    }
    promote(out, count, PREFERRED);
    return count;
}

int dm_sessions_find(const DmSession *sessions, int count, const char *name) {
    if (!sessions || !name) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(sessions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
