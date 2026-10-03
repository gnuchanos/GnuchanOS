/*
 * term_suggest.c — the history, and the ghost of a command taken from it.
 *
 * See term_suggest.h for why the line is read off the grid rather than counted
 * from the keyboard, and for what this deliberately does not do. What is here
 * is the three things that follow from that: a store of past commands, a way to
 * read the line the shell is echoing, and a lookup of the newest stored command
 * that begins with it.
 *
 * --- the file, and where it goes ---
 *
 * The history is a plain text file, one command per line, which is the same
 * shape every shell already uses and what makes it possible to seed this from
 * a ~/.bash_history by copying it. The path is chosen the way the settings
 * script's is — an environment variable first, then $XDG_DATA_HOME, then
 * ~/.local/share — so a session can point it somewhere else without a rebuild
 * and a machine with no HOME keeps its history for the session and no longer.
 */
 
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "term_suggest.h"
#include "term_grid.h"
#include "term_vt.h"
#include "term_input.h"

/* The history starts at this many slots and doubles. A terminal that opened a
   two-thousand-line store at start would pay for it on every launch, and a
   session that runs three commands never uses the rest. */
#define SUGGEST_HISTORY_INITIAL 64

/* --- the file ------------------------------------------------------------- */

/* Where the history is kept. The search is the settings script's, and it is
   deliberately a DIFFERENT variable and a different directory: a person who
   wants their terminal to forget must be able to move the history without
   moving the colours, and the two files have nothing to do with each other. */
static void suggest_file_path(TermSuggest *suggest) {
    suggest->file[0] = '\0';

    const char *named = getenv("GCL_TERMINAL_HISTORY");
    if (named != NULL && named[0] != '\0') {
        snprintf(suggest->file, sizeof(suggest->file), "%s", named);
        return;
    }

    const char *data_home = getenv("XDG_DATA_HOME");
    if (data_home != NULL && data_home[0] == '/') {
        snprintf(suggest->file, sizeof(suggest->file), "%s/gnuchanterm/history",
                 data_home);
        return;
    }

    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        snprintf(suggest->file, sizeof(suggest->file),
                 "%s/.local/share/gnuchanterm/history", home);
        return;
    }

    /* No HOME and no XDG_DATA_HOME: nowhere to keep it. The file stays empty,
       which the two halves below read as "do not load, do not save" — a
       session with no home keeps its history in memory and loses it, which is
       worth more than a terminal that refuses to run. */
}

/* --- the store ------------------------------------------------------------ */

/* Add one line, growing the array if it is full. The string is COPIED: the
   caller's buffer is the grid's scratch and is rewritten by the next keystroke. */
static void history_push(TermSuggest *suggest, const char *line) {
    if (suggest->history_count >= TERM_SUGGEST_HISTORY_MAX) {
        /* Full. The oldest entry goes and everything moves up by one, which is
           what keeps a long-running session's suggestions recent rather than
           frozen on the first two thousand commands it ever ran. */
        if (suggest->history_count > 0) {
            free(suggest->history[0]);
            memmove(&suggest->history[0], &suggest->history[1],
                    (size_t)(suggest->history_count - 1) * sizeof(char *));
            suggest->history_count--;
        }
    }

    if (suggest->history_count >= suggest->history_capacity) {
        int grown = suggest->history_capacity > 0
                        ? suggest->history_capacity * 2
                        : SUGGEST_HISTORY_INITIAL;
        if (grown > TERM_SUGGEST_HISTORY_MAX) {
            grown = TERM_SUGGEST_HISTORY_MAX;
        }
        char **fresh = (char **)realloc(suggest->history,
                                        (size_t)grown * sizeof(char *));
        if (fresh == NULL) {
            return;
        }
        suggest->history = fresh;
        suggest->history_capacity = grown;
    }

    char *copy = strdup(line);
    if (copy == NULL) {
        return;
    }
    suggest->history[suggest->history_count++] = copy;
}

/* --- life ----------------------------------------------------------------- */

void term_suggest_init(TermSuggest *suggest) {
    if (suggest == NULL) {
        return;
    }
    memset(suggest, 0, sizeof(*suggest));
    suggest->input_known = 0;
    suggest->drawn_y = -1;
    suggest_file_path(suggest);
}

void term_suggest_free(TermSuggest *suggest) {
    if (suggest == NULL) {
        return;
    }
    for (int i = 0; i < suggest->history_count; i++) {
        free(suggest->history[i]);
    }
    free(suggest->history);
    suggest->history = NULL;
    suggest->history_count = 0;
    suggest->history_capacity = 0;
}

/* --- the shell's markers -------------------------------------------------- */

void term_suggest_marker(TermSuggest *suggest, TermCore *core, char kind) {
    if (suggest == NULL || core == NULL) {
        return;
    }
    const TermGrid *grid = term_vt_screen(&core->vt);

    switch (kind) {
    case 'B':
        /* The prompt has ended and the line begins HERE, at the cursor. The
           position is the shell's own and not a guess, which is the whole
           reason the markers exist. */
        suggest->input_x = grid->cursor_x;
        suggest->input_y = grid->cursor_y;
        suggest->input_known = 1;
        /* The ghost belongs to the line that was on the screen before this
           prompt: the shell has just drawn a new one, so it is dropped and
           recomputed on the next tick. */
        term_suggest_drop(suggest);
        break;
    case 'C':
        /* A command is running: what was on the line is history now and there
           is no input at this prompt to suggest for. Dropping the ghost is
           what stops it hanging over the command's own output. */
        suggest->input_known = 0;
        term_suggest_drop(suggest);
        break;
    default:
        /* 'A' and 'D' carry no position. */
        break;
    }
}

/* --- reading the line off the grid ---------------------------------------- */

/* How many bytes a code point becomes in UTF-8. It is the encoding the shell
   sent and the encoding the PTY reads back, so the line this sends on Tab is
   the same bytes the user typed. */
static int utf8_encode(uint32_t ch, char *out) {
    if (ch < 0x80) {
        out[0] = (char)ch;
        return 1;
    }
    if (ch < 0x800) {
        out[0] = (char)(0xC0 | (ch >> 6));
        out[1] = (char)(0x80 | (ch & 0x3F));
        return 2;
    }
    if (ch < 0x10000) {
        out[0] = (char)(0xE0 | (ch >> 12));
        out[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
        out[2] = (char)(0x80 | (ch & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (ch >> 18));
    out[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[3] = (char)(0x80 | (ch & 0x3F));
    return 4;
}

/* Copy the cells from `from` to `to` of one screen row into `out` as UTF-8.
 *
 * A cell holding 0 is a blank and becomes a space, and that is not cosmetic:
 * `python main.py` typed with the space missed out is a different command, and
 * a suggestion that glossed over the gap would offer a line the user never ran.
 * The blank has to survive the trip from the grid to the comparison.
 *
 * A wide character's continuation cell holds no character and is skipped: its
 * two columns are one byte sequence and counting the second as a blank would
 * put a space inside a Chinese word.
 *
 * Returns the number of bytes written, never more than `size` - 1. */
static int read_line_cells(const TermGrid *grid, int row, int from, int to,
                           char *out, int size) {
    if (grid == NULL || out == NULL || size <= 0) {
        return 0;
    }
    if (row < 0 || row >= grid->rows) {
        return 0;
    }
    if (from < 0) from = 0;
    if (to >= grid->cols) to = grid->cols - 1;

    const TermLine *line = &grid->lines[row];
    if (line->cells == NULL) {
        return 0;
    }

    int used = 0;
    for (int x = from; x <= to; x++) {
        const TermCell *cell = &line->cells[x];
        if (cell->wide_cont) {
            continue;
        }
        char bytes[4];
        int length;
        if (cell->ch == 0) {
            bytes[0] = ' ';
            length = 1;
        } else {
            length = utf8_encode(cell->ch, bytes);
        }
        if (used + length >= size) {
            break;
        }
        memcpy(out + used, bytes, (size_t)length);
        used += length;
    }
    out[used] = '\0';
    return used;
}

/* --- the suggestion ------------------------------------------------------- */

/* The newest remembered command that begins with `line` and is longer than it,
   or NULL. Searching from the NEWEST backwards is the whole of the ordering:
   the command you ran last is the one you are most likely to run again, and a
   suggestion from three sessions ago is the one nobody wants. */
static const char *newest_with_prefix(const TermSuggest *suggest,
                                      const char *line, int line_length) {
    /* An empty line suggests nothing. Offering the last command to a prompt
       nobody has typed at would put a ghost on every fresh prompt. */
    if (line_length <= 0) {
        return NULL;
    }
    for (int i = suggest->history_count - 1; i >= 0; i--) {
        const char *candidate = suggest->history[i];
        int candidate_length = (int)strlen(candidate);
        if (candidate_length <= line_length) {
            continue;
        }
        if (strncmp(candidate, line, (size_t)line_length) == 0) {
            return candidate;
        }
    }
    return NULL;
}

void term_suggest_drop(TermSuggest *suggest) {
    if (suggest == NULL) {
        return;
    }
    suggest->suggestion[0] = '\0';
    suggest->suggestion_length = 0;
}

/* Mark one screen row to be drawn again. The ghost is painted by the renderer
 * and it is not a cell, so nothing about it survives a repaint: the only way to
 * take one away or move it is to say "this row is no longer what is on the
 * window", which is exactly what a dirty mark means. */
static void suggest_mark_row(TermCore *core, int row) {
    TermGrid *grid = (TermGrid *)term_vt_screen(&core->vt);
    if (grid == NULL || row < 0 || row >= grid->rows) {
        return;
    }
    grid->lines[row].dirty = 1;
    term_core_damage(core);
}

/* Put what is being offered in step with what is on the window.
 *
 * Called at the end of every update. When the ghost's row or its TEXT has
 * changed, the old row is marked and so is the new one, and the text is
 * remembered. It is the TEXT that is compared and not its length: two
 * completions of the same length — `cd Doc` and `cd Dow` — would otherwise
 * leave the first standing under the second.
 *
 * Nothing is marked when nothing changed, which is what keeps a prompt with a
 * standing suggestion from redrawing a row fifty times a second. */
static void suggest_sync_drawn(TermCore *core, TermSuggest *suggest) {
    TermGrid *grid = (TermGrid *)term_vt_screen(&core->vt);
    int want_row = suggest->suggestion_length > 0 ? suggest->input_y : -1;

    int changed = (want_row != suggest->drawn_y) ||
                  (strcmp(suggest->suggestion, suggest->drawn_text) != 0);
    if (!changed) {
        return;
    }

    if (suggest->drawn_y >= 0) {
        suggest_mark_row(core, suggest->drawn_y);
    }
    if (want_row >= 0) {
        suggest_mark_row(core, want_row);
        suggest->drawn_x = grid != NULL ? grid->cursor_x : 0;
        suggest->drawn_length = suggest->suggestion_length;
    } else {
        suggest->drawn_length = 0;
    }

    suggest->drawn_y = want_row;
    snprintf(suggest->drawn_text, sizeof(suggest->drawn_text), "%s",
             suggest->suggestion);
}

void term_suggest_update(TermCore *core) {
    if (core == NULL || core->suggest == NULL) {
        return;
    }
    TermSuggest *suggest = (TermSuggest *)core->suggest;

    if (!suggest->loaded) {
        term_suggest_load(suggest);
    }

    /* Every path below ENDS at suggest_sync_drawn(), and that is the whole
       reason this is written as it is. The ghost's state on the window has to
       be brought in step with whatever was decided — a fresh suggestion, or
       none — on EVERY tick and not only on the happy path. An early return that
       skipped the sync would leave a ghost standing on the row of a line that
       had just gone away, which is exactly the "the ghost does not disappear"
       fault this exists to prevent. */
    if (!suggest->input_known) {
        /* Without a prompt end there is no way to tell the line from the
           prompt, and a guess would put a ghost over a program's own output. */
        term_suggest_drop(suggest);
        suggest_sync_drawn(core, suggest);
        return;
    }

    const TermGrid *grid = term_vt_screen(&core->vt);

    /* The line is on ONE row, and the check is the whole of what keeps this
       honest. A cursor that has left the row the prompt is on means the line
       has wrapped to a second row or the screen has scrolled since the prompt
       was drawn — and in both cases the cells between the two positions are no
       longer the line. Declining to suggest is the correct answer, and it is
       what a suggestion of garbage would not be. */
    if (grid->cursor_y != suggest->input_y ||
        grid->cursor_x < suggest->input_x) {
        term_suggest_drop(suggest);
        suggest_sync_drawn(core, suggest);
        return;
    }

    suggest->line_length = read_line_cells(
        grid, suggest->input_y, suggest->input_x, grid->cursor_x,
        suggest->line, (int)sizeof(suggest->line));

    const char *best = newest_with_prefix(suggest, suggest->line,
                                          suggest->line_length);
    if (best == NULL) {
        term_suggest_drop(suggest);
        suggest_sync_drawn(core, suggest);
        return;
    }

    /* The suggestion is the TAIL: the part after what is already typed. The
       whole command is not stored, because sending it on Tab would repeat the
       text the user has already typed and readline would insert it in the
       middle of its own line. */
    const char *tail = best + suggest->line_length;
    snprintf(suggest->suggestion, sizeof(suggest->suggestion), "%s", tail);
    suggest->suggestion_length = (int)strlen(suggest->suggestion);

    suggest_sync_drawn(core, suggest);
}

int term_suggest_accept(TermCore *core) {
    if (core == NULL || core->suggest == NULL) {
        return 0;
    }
    TermSuggest *suggest = (TermSuggest *)core->suggest;
    if (suggest->suggestion_length <= 0) {
        return 0;
    }

    /* The tail goes to the shell and NOT the whole line, so readline inserts it
       at its own cursor and everything it knows about the line — the cursor
       position, the undo list, its history — stays true. Sending the whole line
       would mean replacing what is there, and readline has no single sequence
       for that which means the same in every editing mode. */
    term_input_send(core, suggest->suggestion, suggest->suggestion_length);

    /* Taken, so it is no longer on offer. The shell echoes the text back and
       the next update reads the finished line off the grid and finds nothing
       longer to suggest — unless a still-longer command matches, which is the
       right answer and how pressing Tab twice walks up to the longest match. */
    term_suggest_drop(suggest);
    return 1;
}

/* --- the arrows: walking the history -------------------------------------- */

/* Put a whole line where the line already is, by deleting what is there and
   typing the new one.
 *
 * The delete is one backspace per CHARACTER and not a "kill to start" escape,
 * and that is deliberate: a backspace means "delete the character before the
 * cursor" in emacs mode, in vi insert mode and in every readline a shell is
 * configured with. A kill escape means one thing in one of them and nothing in
 * another, which is the kind of difference that works on the machine it was
 * written on and erases the wrong text elsewhere. */
static void replace_line(TermCore *core, TermSuggest *suggest, const char *line) {
    /* What is on the line now, read from the grid, so the number of backspaces
       is what the shell actually has and not what was last suggested. */
    int current = suggest->line_length;
    if (current < 0) {
        current = 0;
    }

    char backspaces[TERM_SUGGEST_LINE_MAX];
    int room = (int)sizeof(backspaces);
    if (current < room) {
        room = current;
    }
    for (int i = 0; i < room; i++) {
        backspaces[i] = 0x7F;   /* DEL, which is what a backspace key sends */
    }
    if (room > 0) {
        term_input_send(core, backspaces, room);
    }

    int length = (int)strlen(line);
    if (length > 0) {
        term_input_send(core, line, length);
    }

    /* The line is now the recalled one, and the suggestion is recomputed from
       it on the next tick. Dropping it means a stale ghost is not left standing
       for a frame over the line that just replaced it. */
    term_suggest_drop(suggest);
}

int term_suggest_history(TermCore *core, int direction) {
    if (core == NULL || core->suggest == NULL) {
        return 0;
    }
    TermSuggest *suggest = (TermSuggest *)core->suggest;

    /* Only at a prompt. A full-screen program that reads the arrows itself —
       an editor, a pager, a shell's own menu — is not asking for history, and
       replacing its line with a command from another session would be worse
       than doing nothing. */
    if (!suggest->input_known) {
        return 0;
    }
    if (suggest->history_count == 0) {
        return 0;
    }

    int next = suggest->browsing + direction;
    if (next < 0) {
        next = 0;
    }
    if (next > suggest->history_count) {
        next = suggest->history_count;
    }
    if (next == suggest->browsing) {
        return 0;
    }
    suggest->browsing = next;

    /* Zero is the line the user was typing, and it is at the END of the walk
       rather than its start: `browsing` counts back from the newest, so 1 is
       the last command run and history_count is the oldest kept. Walking down
       past 1 lands on 0 and puts an empty line back, which is what Down after
       the newest command should do. */
    if (next == 0) {
        replace_line(core, suggest, "");
        return 1;
    }

    replace_line(core, suggest,
                 suggest->history[suggest->history_count - next]);
    return 1;
}

/* --- the history ---------------------------------------------------------- */

void term_suggest_remember(TermSuggest *suggest, const char *line) {
    if (suggest == NULL || line == NULL || line[0] == '\0') {
        return;
    }
    /* Whitespace-only is not a command. Enter at a blank prompt is not
       something to suggest from, and storing it would make the first suggestion
       after every empty line a blank. */
    int all_space = 1;
    for (const char *p = line; *p != '\0'; p++) {
        if (*p != ' ' && *p != '\t') {
            all_space = 0;
            break;
        }
    }
    if (all_space) {
        return;
    }

    /* The same command twice in a row is once. A history of `ls`, `ls`, `ls`
       makes every suggestion for `l` the same entry three times over, and it
       is what a person pressing Enter twice does not mean to record. */
    if (suggest->history_count > 0) {
        const char *newest = suggest->history[suggest->history_count - 1];
        if (strcmp(newest, line) == 0) {
            return;
        }
    }

    history_push(suggest, line);

    /* A new command resets the arrow walk to the line being typed: the next Up
       recalls THIS command and not wherever the last walk was left. */
    suggest->browsing = 0;
}

/* --- the file, read and written ------------------------------------------- */

void term_suggest_load(TermSuggest *suggest) {
    if (suggest == NULL) {
        return;
    }
    suggest->loaded = 1;
    if (suggest->file[0] == '\0') {
        return;
    }

    FILE *file = fopen(suggest->file, "r");
    if (file == NULL) {
        /* No file is the ordinary state of a machine that has never run a
           command in this terminal, and it is not worth a word anywhere. */
        return;
    }

    char line[TERM_SUGGEST_LINE_MAX];
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        int length = (int)strlen(line);
        while (length > 0 && (line[length - 1] == '\n' ||
                              line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length > 0) {
            /* Pushed directly and not through remember(): the newest-entry
               check would drop a repeated line from a file where the same
               command was run twice on purpose, an hour apart. */
            history_push(suggest, line);
        }
    }
    fclose(file);
}

void term_suggest_save(const TermSuggest *suggest) {
    if (suggest == NULL || suggest->file[0] == '\0') {
        return;
    }

    /* The directory is made before the file is, so the first save on a machine
       that has never run this does not silently fail on a missing
       ~/.local/share/gnuchanterm. */
    char directory[TERM_SUGGEST_LINE_MAX];
    snprintf(directory, sizeof(directory), "%s", suggest->file);
    char *slash = strrchr(directory, '/');
    if (slash != NULL && slash != directory) {
        *slash = '\0';
        /* Two levels at once: ~/.local and ~/.local/share, and the mkdir of a
           path whose parent does not exist fails, so both are made. */
        char parent[TERM_SUGGEST_LINE_MAX];
        snprintf(parent, sizeof(parent), "%s", directory);
        char *grand = strrchr(parent, '/');
        if (grand != NULL && grand != parent) {
            *grand = '\0';
            mkdir(parent, 0700);
        }
        mkdir(directory, 0700);
    }

    FILE *file = fopen(suggest->file, "w");
    if (file == NULL) {
        return;
    }

    /* The newest TERM_SUGGEST_HISTORY_MAX lines are written, and it is the
       NEWEST and not the oldest: a store trimmed to its first two thousand
       commands offers suggestions from a year ago and never from yesterday. */
    int first = suggest->history_count - TERM_SUGGEST_HISTORY_MAX;
    if (first < 0) {
        first = 0;
    }
    for (int i = first; i < suggest->history_count; i++) {
        fprintf(file, "%s\n", suggest->history[i]);
    }
    fclose(file);
}

/* --- the module ----------------------------------------------------------- */

static int suggest_module_init(TermCore *core) {
    core->suggest = calloc(1, sizeof(TermSuggest));
    if (core->suggest == NULL) {
        return -1;
    }
    term_suggest_init((TermSuggest *)core->suggest);
    return 0;
}

/* Read the line and re-offer, every tick.
 *
 * It is a tick and not an event handler, and that is the whole of the timing:
 * the shell echoes a keystroke back as output the PTY path draws, and only
 * afterwards is the grid the line is read from in its final state. An event
 * handler runs BEFORE the echo has been pumped, so it would read the line as it
 * was one keystroke ago and send the wrong suffix on Tab. */
static void suggest_module_tick(TermCore *core) {
    term_suggest_update(core);
}

static void suggest_module_cleanup(TermCore *core) {
    if (core->suggest != NULL) {
        term_suggest_save((TermSuggest *)core->suggest);
        term_suggest_free((TermSuggest *)core->suggest);
        free(core->suggest);
        core->suggest = NULL;
    }
}

const TermModule term_suggest_module = {
    .name = "suggest",
    .init = suggest_module_init,
    .event = NULL,
    .tick = suggest_module_tick,
    /* Twenty milliseconds, the renderer's own rate, and not lower: the tick
       reads the grid, which is a walk of one row, and a keystroke's echo is
       drawn within a frame of it arriving. */
    .interval_ms = 20,
    .cleanup = suggest_module_cleanup,
};
