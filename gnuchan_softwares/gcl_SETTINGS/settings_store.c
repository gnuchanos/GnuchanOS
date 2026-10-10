/*
 * settings_store.c — read a program's settings file, change only the values.
 *
 * The whole of this file exists to keep one promise: a save replaces the
 * VALUE of the settings the panel was asked to change and copies every other
 * byte of the file through untouched. The comments a person wrote, the
 * settings this program does not know, the parts of the file it has no
 * business touching — they survive.
 *
 * It does that by scanning, not by rewriting. A setting is found by its token,
 * the span that holds its value is remembered, and the file is rebuilt as
 * "everything before the value, the new value, everything after" — repeated
 * for each change. The scanner knows four ways a value is written (see
 * settings_types.h) and one rule everywhere: what is inside a quoted string is
 * never a comment and never a delimiter.
 *
 * A row may be written differently from the rest of its program, and a file may
 * write the same call more than once — the terminal splits its settings across
 * several `gcl_Terminal.call(...)` blocks. Both are handled here: the writing
 * style and the call prefix are resolved per row, and every call block is
 * walked rather than only the first.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings_store.h"

/* The most edits one save may make: a value change for every row, and an
   insertion for every row the file did not name. Twice the row ceiling is far
   above both. */
#define SETTINGS_MAX_EDITS (SETTINGS_MAX_ROWS * 2)

/* One change to the file: replace [start, end) with `text`, or — when
   start == end — insert `text` at that point. */
typedef struct Edit {
    size_t start;
    size_t end;
    char   text[SETTINGS_TEXT_LENGTH + 32];
} Edit;

/* The style a row is written with: its own when it names one, the program's
   otherwise. */
static SettingStyle row_style(const AppDef *app, const SettingDef *def) {
    return def->style == SETTING_STYLE_INHERIT ? app->style : def->style;
}

/* The call prefix a row is written under: its own when it names one, the
   program's otherwise. */
static const char *row_call(const AppDef *app, const SettingDef *def) {
    return def->call ? def->call : app->call;
}

/* --- small scanning helpers ------------------------------------------------ */

static int is_name_char(int c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* Past a comment that starts at `i` (which points at '#'), to the newline that
   ends it, or to `end`. */
static size_t skip_comment(const char *text, size_t i, size_t end) {
    while (i < end && text[i] != '\n') {
        i++;
    }
    return i;
}

/* Past a double-quoted string that starts at `i` (which points at the opening
   quote), to the character after the closing one. A backslash escapes the next
   character, so an escaped quote does not close the string. */
static size_t skip_string(const char *text, size_t i, size_t end) {
    if (i >= end || text[i] != '"') {
        return i;
    }
    i++;
    while (i < end) {
        if (text[i] == '\\' && i + 1 < end) {
            i += 2;
            continue;
        }
        if (text[i] == '"') {
            return i + 1;
        }
        i++;
    }
    return end;
}

/* The index one past the end of the value that begins at `i`, stopping at the
   comma, the closing bracket or the comment that ends it. Nested brackets and
   braces are stepped over, so a value written `[a, b]` is not cut at its own
   comma, and a '#' inside a string is not read as a comment. */
static size_t scan_value_end(const char *text, size_t i, size_t end) {
    int depth = 0;
    while (i < end) {
        char c = text[i];
        if (c == '"') {
            i = skip_string(text, i, end);
            continue;
        }
        if (depth == 0 && c == '#') {
            break;
        }
        if (depth == 0 && c == '\n') {
            break;
        }
        if (c == '(' || c == '[' || c == '{') {
            depth++;
        } else if (c == ')' || c == ']' || c == '}') {
            if (depth == 0) {
                break;
            }
            depth--;
        } else if (c == ',' && depth == 0) {
            break;
        }
        i++;
    }
    return i;
}

static void trim_span(const char *text, size_t *start, size_t *end) {
    while (*end > *start && isspace((unsigned char)text[*end - 1])) {
        (*end)--;
    }
    while (*start < *end && isspace((unsigned char)text[*start])) {
        (*start)++;
    }
}

/* Whether `token` appears at `pos` as a whole word: the character before it is
   not part of a name, and the character after it is not either. */
static int token_at(const char *text, size_t len, size_t pos, const char *token) {
    size_t n = strlen(token);
    if (pos + n > len) {
        return 0;
    }
    if (memcmp(text + pos, token, n) != 0) {
        return 0;
    }
    if (pos > 0 && is_name_char((unsigned char)text[pos - 1])) {
        return 0;
    }
    if (pos + n < len && is_name_char((unsigned char)text[pos + n])) {
        return 0;
    }
    return 1;
}

/* --- finding a value ------------------------------------------------------- */

/* The calling block named `call`, searched from `from` onwards: the span
   between its '(' and its matching ')'. Returns 1 and fills the two when the
   call is found. The caller walks the matches because a file may write the
   same call more than once. */
static int find_call_from(const char *text, size_t len, const char *call,
                          size_t from, size_t *args_start, size_t *args_end) {
    size_t n = strlen(call);
    for (size_t p = from; p + n <= len; p++) {
        if (!token_at(text, len, p, call)) {
            continue;
        }
        size_t q = p + n;
        while (q < len && isspace((unsigned char)text[q])) {
            q++;
        }
        if (q >= len || text[q] != '(') {
            continue;
        }
        /* The matching close, stepping over strings and comments. */
        size_t i = q + 1;
        int depth = 1;
        while (i < len) {
            char c = text[i];
            if (c == '"') {
                i = skip_string(text, i, len);
                continue;
            }
            if (c == '#') {
                i = skip_comment(text, i, len);
                continue;
            }
            if (c == '(') {
                depth++;
            } else if (c == ')') {
                depth--;
                if (depth == 0) {
                    *args_start = q + 1;
                    *args_end = i;
                    return 1;
                }
            }
            i++;
        }
    }
    return 0;
}

/* The first calling block named `call`. */
static int find_call(const char *text, size_t len, const char *call,
                     size_t *args_start, size_t *args_end) {
    return find_call_from(text, len, call, 0, args_start, args_end);
}

/* The value of the named argument inside an argument list. Returns 1 and fills
   the raw span — the quotes still around a string — when it is there. */
static int find_arg_value(const char *text, size_t start, size_t end,
                          const char *key, size_t *vstart, size_t *vend) {
    size_t i = start;
    while (i < end) {
        while (i < end && (isspace((unsigned char)text[i]) || text[i] == ',')) {
            i++;
        }
        if (i >= end) {
            break;
        }
        if (text[i] == '#') {
            i = skip_comment(text, i, end);
            continue;
        }
        /* The argument's name: a bare identifier. */
        size_t name_start = i;
        while (i < end && is_name_char((unsigned char)text[i])) {
            i++;
        }
        size_t name_len = i - name_start;
        while (i < end && isspace((unsigned char)text[i])) {
            i++;
        }
        if (i < end && text[i] == '=') {
            i++;
            while (i < end && isspace((unsigned char)text[i])) {
                i++;
            }
            size_t vs = i;
            size_t ve = scan_value_end(text, i, end);
            if (name_len == strlen(key) &&
                strncmp(text + name_start, key, name_len) == 0) {
                *vstart = vs;
                *vend = ve;
                return 1;
            }
            i = ve;
        } else {
            /* A positional argument, or something this reader does not name:
               step past it so the next one is found. */
            i = scan_value_end(text, i, end);
        }
    }
    return 0;
}

/* The first positional value of an argument list — a function's own argument —
   used by SETTING_FUNC, whose call takes the value and not a named one. */
static int find_first_value(const char *text, size_t start, size_t end,
                            size_t *vstart, size_t *vend) {
    size_t i = start;
    while (i < end && (isspace((unsigned char)text[i]) || text[i] == ',')) {
        i++;
    }
    if (i >= end || text[i] == '#') {
        return 0;
    }
    *vstart = i;
    *vend = scan_value_end(text, i, end);
    return 1;
}

/* The value of a line `left_side = value`. `left_side` is matched at the start
   of a line, after any leading whitespace. */
static int find_line_value(const char *text, size_t len, const char *left_side,
                           size_t *vstart, size_t *vend) {
    size_t n = strlen(left_side);
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && (text[j] == ' ' || text[j] == '\t')) {
            j++;
        }
        if (j < len && text[j] != '#' && text[j] != '\n' &&
            j + n <= len && memcmp(text + j, left_side, n) == 0 &&
            (j + n >= len || (!is_name_char((unsigned char)text[j + n]) &&
                              text[j + n] != '.'))) {
            size_t k = j + n;
            while (k < len && (text[k] == ' ' || text[k] == '\t')) {
                k++;
            }
            if (k < len && text[k] == '=') {
                k++;
                while (k < len && (text[k] == ' ' || text[k] == '\t')) {
                    k++;
                }
                *vstart = k;
                *vend = scan_value_end(text, k, len);
                return 1;
            }
        }
        while (i < len && text[i] != '\n') {
            i++;
        }
        if (i < len) {
            i++;
        }
    }
    return 0;
}

/* Where a setting's value is, for the row's writing style. Returns 1 and fills
   the raw span. */
static int locate(const char *text, size_t len, const AppDef *app,
                  const SettingDef *def, size_t *vstart, size_t *vend) {
    char name[SETTINGS_TEXT_LENGTH * 2];
    SettingStyle style = row_style(app, def);
    const char *call = row_call(app, def);

    if (style == SETTING_CALL) {
        /* Walk every block of the call: the terminal writes one call per
           group of settings, and a key may be in any of them. */
        size_t scan = 0;
        size_t as = 0;
        size_t ae = 0;
        while (find_call_from(text, len, call, scan, &as, &ae)) {
            if (find_arg_value(text, as, ae, def->key, vstart, vend)) {
                return 1;
            }
            scan = ae + 1;
        }
        return 0;
    }
    if (style == SETTING_FLAT) {
        return find_line_value(text, len, def->key, vstart, vend);
    }
    if (style == SETTING_FUNC) {
        /* gcl_Window.set_active_window_border_color("#..."): a call whose name
           is <call>.<key>, and the value is its first argument. */
        snprintf(name, sizeof(name), "%s.%s", call, def->key);
        size_t as = 0;
        size_t ae = 0;
        if (!find_call(text, len, name, &as, &ae)) {
            return 0;
        }
        return find_first_value(text, as, ae, vstart, vend);
    }
    /* SETTING_ASSIGN: gcl_Switcher.background = "#...". */
    snprintf(name, sizeof(name), "%s.%s", call, def->key);
    return find_line_value(text, len, name, vstart, vend);
}

/* --- turning a raw span into a value, and back ----------------------------- */

/* Copy the string between a pair of quotes into `out`, turning the two escapes
   that matter — \" and \\ — back into what they stand for, and leaving every
   other backslash sequence exactly as written: a prompt's \e, \[ and \] are
   the shell's own and must reach it unchanged. */
static void copy_unquoted(char *out, unsigned int size,
                          const char *text, size_t start, size_t end) {
    trim_span(text, &start, &end);
    if (start < end && text[start] == '"') {
        start++;
    }
    if (end > start && text[end - 1] == '"') {
        end--;
    }
    unsigned int o = 0;
    for (size_t i = start; i < end && o + 1 < size; i++) {
        if (text[i] == '\\' && i + 1 < end &&
            (text[i + 1] == '"' || text[i + 1] == '\\')) {
            out[o++] = text[i + 1];
            i++;
            continue;
        }
        out[o++] = text[i];
    }
    out[o] = '\0';
}

/* Write `value` between quotes, escaping only a quote and a backslash so the
   result reads back as the same value. */
static void append_quoted(char *out, unsigned int size, const char *value) {
    unsigned int o = (unsigned int)strlen(out);
    if (o + 1 < size) {
        out[o++] = '"';
    }
    for (const char *p = value; *p && o + 2 < size; p++) {
        if (*p == '"' || *p == '\\') {
            out[o++] = '\\';
        }
        out[o++] = *p;
    }
    if (o + 1 < size) {
        out[o++] = '"';
    }
    out[o] = '\0';
}

/* The value as it should stand in the file: a string for a colour or a text, a
   bare number for an int, a bare fraction for a real, True/False (or
   true/false) for a switch. */
static void render_value(char *out, unsigned int size, const SettingDef *def,
                         const SettingValue *value, int capital_bools) {
    out[0] = '\0';
    if (def->type == SETTING_INT) {
        snprintf(out, size, "%d", setting_value_int(value));
        return;
    }
    if (def->type == SETTING_REAL) {
        /* A fraction is written bare, the way the file writes it — 1.0 and not
           "1.0" — so a value a person typed lands in the file unchanged. */
        snprintf(out, size, "%s", value->text);
        return;
    }
    if (def->type == SETTING_BOOL) {
        int flag = setting_value_bool(value);
        const char *word;
        if (capital_bools) {
            word = flag ? "True" : "False";
        } else {
            word = flag ? "true" : "false";
        }
        snprintf(out, size, "%s", word);
        return;
    }
    append_quoted(out, size, value->text);
}

/* --- public: values -------------------------------------------------------- */

int setting_value_int(const SettingValue *value) {
    const char *p = value->text;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (!*p) {
        return 0;
    }
    return (int)strtol(p, NULL, 10);
}

int setting_value_bool(const SettingValue *value) {
    if (strcmp(value->text, "True") == 0 || strcmp(value->text, "true") == 0 ||
        strcmp(value->text, "1") == 0 || strcmp(value->text, "yes") == 0 ||
        strcmp(value->text, "on") == 0) {
        return 1;
    }
    return 0;
}

void setting_value_set_int(SettingValue *value, int number) {
    snprintf(value->text, sizeof(value->text), "%d", number);
}

void setting_value_set_bool(SettingValue *value, int flag, int capital) {
    const char *word;
    if (capital) {
        word = flag ? "True" : "False";
    } else {
        word = flag ? "true" : "false";
    }
    snprintf(value->text, sizeof(value->text), "%s", word);
}

/* --- public: the path ------------------------------------------------------ */

char *settings_path(char *out, unsigned int size, const AppDef *app) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(out, size, "%s/%s/%s", xdg, app->dir, app->file);
        return out;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(out, size, "%s/.config/%s/%s", home, app->dir, app->file);
        return out;
    }
    out[0] = '\0';
    return out;
}

/* --- reading --------------------------------------------------------------- */

static char *read_file(const char *path, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[got] = '\0';
    *out_length = got;
    return buffer;
}

int settings_load(const AppDef *app, SettingValue *values) {
    for (int i = 0; i < app->setting_count; i++) {
        snprintf(values[i].text, sizeof(values[i].text), "%s",
                 app->settings[i].fallback);
        values[i].present = 0;
    }

    char path[SETTINGS_TEXT_LENGTH * 2];
    settings_path(path, sizeof(path), app);
    if (!path[0]) {
        return -1;
    }

    size_t length = 0;
    char *text = read_file(path, &length);
    if (!text) {
        /* A missing file is not a failure: the panel opens on the defaults the
           program itself would use, and the save creates the file. */
        return 0;
    }

    for (int i = 0; i < app->setting_count; i++) {
        size_t vs = 0;
        size_t ve = 0;
        if (locate(text, length, app, &app->settings[i], &vs, &ve)) {
            copy_unquoted(values[i].text, sizeof(values[i].text), text, vs, ve);
            values[i].present = 1;
        }
    }
    free(text);
    return 1;
}

/* --- writing --------------------------------------------------------------- */

/* One line a value is written as, when it is added to a file that did not name
   it. The form follows the row's style. */
static void render_added(char *out, unsigned int size, const AppDef *app,
                         const SettingDef *def, const SettingValue *value) {
    char rendered[SETTINGS_TEXT_LENGTH + 32];
    render_value(rendered, sizeof(rendered), def, value, app->capital_bools);
    SettingStyle style = row_style(app, def);
    const char *call = row_call(app, def);

    if (style == SETTING_CALL) {
        snprintf(out, size, "    %s=%s,\n", def->key, rendered);
    } else if (style == SETTING_FUNC) {
        snprintf(out, size, "\n%s.%s(%s)", call, def->key, rendered);
    } else if (style == SETTING_ASSIGN) {
        snprintf(out, size, "%s.%s = %s\n", call, def->key, rendered);
    } else {
        snprintf(out, size, "%s = %s\n", def->key, rendered);
    }
}

/* The whole file for a program that had none: the program's call around every
   value, so the result is a file the program can read. */
static char *build_new_file(const AppDef *app, const SettingValue *values,
                            size_t *out_length) {
    size_t capacity = 256;
    for (int i = 0; i < app->setting_count; i++) {
        capacity += SETTINGS_TEXT_LENGTH + 80;
    }
    char *buffer = malloc(capacity);
    if (!buffer) {
        return NULL;
    }
    buffer[0] = '\0';

    const char *header =
        "# GnuChanSettings tarafindan olusturuldu.\n"
        "# Bu bir ayar dosyasidir; elle de duzenlenebilir.\n";
    strncat(buffer, header, capacity - strlen(buffer) - 1);

    SettingStyle style = app->style;
    char line[SETTINGS_TEXT_LENGTH * 3];

    if (style == SETTING_CALL) {
        snprintf(line, sizeof(line), "%s(\n", app->call);
        strncat(buffer, line, capacity - strlen(buffer) - 1);
    }
    for (int i = 0; i < app->setting_count; i++) {
        render_added(line, sizeof(line), app, &app->settings[i], &values[i]);
        strncat(buffer, line, capacity - strlen(buffer) - 1);
    }
    if (style == SETTING_CALL) {
        strncat(buffer, ")\n", capacity - strlen(buffer) - 1);
    }

    *out_length = strlen(buffer);
    return buffer;
}

/* Where a missing argument is inserted into a call block: just before the
   closing bracket of the last block. Returns the insertion point, or
   `(size_t)-1` when there is no such block. */
static size_t call_insert_point(const char *text, size_t length,
                                const char *call) {
    size_t scan = 0;
    size_t as = 0;
    size_t ae = 0;
    size_t last = (size_t)-1;
    while (find_call_from(text, length, call, scan, &as, &ae)) {
        last = ae;
        scan = ae + 1;
    }
    return last;
}

int settings_save(const AppDef *app, const SettingValue *values, char *reason) {
    reason[0] = '\0';
    char path[SETTINGS_TEXT_LENGTH * 2];
    settings_path(path, sizeof(path), app);
    if (!path[0]) {
        snprintf(reason, SETTINGS_TEXT_LENGTH,
                 "no home directory to write into");
        return -1;
    }

    size_t length = 0;
    char *text = read_file(path, &length);
    char *created = NULL;
    if (!text) {
        /* No file yet: build one whole. */
        created = build_new_file(app, values, &length);
        if (!created) {
            snprintf(reason, SETTINGS_TEXT_LENGTH, "out of memory");
            return -1;
        }
        text = created;
    }

    Edit edits[SETTINGS_MAX_EDITS];
    int edit_count = 0;

    for (int i = 0; i < app->setting_count && edit_count < SETTINGS_MAX_EDITS;
         i++) {
        const SettingDef *def = &app->settings[i];
        size_t vs = 0;
        size_t ve = 0;
        if (locate(text, length, app, def, &vs, &ve)) {
            edits[edit_count].start = vs;
            edits[edit_count].end = ve;
            render_value(edits[edit_count].text,
                         sizeof(edits[edit_count].text), def, &values[i],
                         app->capital_bools);
            edit_count++;
        } else if (!created) {
            /* The file does not name this setting. It is added so a person who
               changed it in the panel gets it saved, not silently dropped. */
            size_t at = length;
            if (row_style(app, def) == SETTING_CALL) {
                size_t point = call_insert_point(text, length, row_call(app, def));
                if (point != (size_t)-1) {
                    at = point;
                }
            }
            edits[edit_count].start = at;
            edits[edit_count].end = at;
            render_added(edits[edit_count].text, sizeof(edits[edit_count].text),
                         app, def, &values[i]);
            edit_count++;
        }
    }

    /* Apply the edits in file order. An insertion and a replacement at the same
       point keep the order they were added, which is the order the rows are
       written in. */
    for (int a = 0; a < edit_count; a++) {
        for (int b = a + 1; b < edit_count; b++) {
            if (edits[b].start < edits[a].start) {
                Edit swap = edits[a];
                edits[a] = edits[b];
                edits[b] = swap;
            }
        }
    }

    size_t capacity = length + 64;
    for (int i = 0; i < edit_count; i++) {
        capacity += strlen(edits[i].text) + 4;
    }
    char *out = malloc(capacity);
    if (!out) {
        free(created);
        snprintf(reason, SETTINGS_TEXT_LENGTH, "out of memory");
        return -1;
    }

    size_t o = 0;
    size_t cursor = 0;
    for (int i = 0; i < edit_count; i++) {
        if (edits[i].start < cursor) {
            continue;
        }
        size_t gap = edits[i].start - cursor;
        memcpy(out + o, text + cursor, gap);
        o += gap;
        size_t t = strlen(edits[i].text);
        memcpy(out + o, edits[i].text, t);
        o += t;
        cursor = edits[i].end;
    }
    size_t rest = length - cursor;
    memcpy(out + o, text + cursor, rest);
    o += rest;
    out[o] = '\0';

    /* Write beside the target, then rename: a crash mid-write leaves the old
       file, never half of a new one. */
    char temporary[SETTINGS_TEXT_LENGTH * 2 + 8];
    snprintf(temporary, sizeof(temporary), "%s.new", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        free(out);
        free(created);
        snprintf(reason, SETTINGS_TEXT_LENGTH, "cannot write %.200s",
                 temporary);
        return -1;
    }
    size_t wrote = fwrite(out, 1, o, file);
    int close_ok = (fclose(file) == 0);
    free(out);
    free(created);
    if (wrote != o || !close_ok) {
        remove(temporary);
        snprintf(reason, SETTINGS_TEXT_LENGTH, "cannot finish writing %.200s",
                 path);
        return -1;
    }
    if (rename(temporary, path) != 0) {
        remove(temporary);
        snprintf(reason, SETTINGS_TEXT_LENGTH, "cannot replace %.200s", path);
        return -1;
    }
    return 0;
}
