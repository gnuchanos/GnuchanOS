/*
 * term_suggest.h — the fish-style suggestion: a ghost of the command you are
 * about to type, taken from the ones you have typed before.
 *
 * --- what this is for ---
 *
 * Typing `python main.py` once should be enough. The second time, `python` is
 * on the line and the rest of the command — the part you have not typed yet —
 * is shown faintly after the cursor. Tab takes it. That is the whole feature,
 * and it is the one thing a shell's own readline cannot do because readline
 * does not remember what you typed in a previous SESSION in a form it can
 * suggest from at the prompt.
 *
 * --- where the line comes from, and why not from the keyboard ---
 *
 * The obvious way to know what has been typed is to count keystrokes: every
 * printable key appends, backspace removes. It is also wrong, and the reason
 * is worth writing down because it is not obvious until it bites.
 *
 * The shell's line editor — readline — is the thing that ACTUALLY owns the
 * line. It moves the cursor, kills words, yanks, searches its own history, and
 * it echoes all of that back as bytes that the grid draws. A terminal that
 * counted keys would therefore disagree with the screen the moment anything
 * was pressed that is not a plain character: press Up and readline replaces the
 * whole line while the terminal's count still holds the old one, and from then
 * on every suggestion is computed from text nobody typed and every Tab sends
 * the wrong suffix. There is no repairing it, because the terminal cannot ask
 * readline what the line is.
 *
 * So the line is READ OFF THE SCREEN instead, and that is exact by
 * construction: the text between where the prompt ended and where the cursor
 * is IS the line, whatever put it there — typing, a history recall, a
 * completion, a paste. Nothing has to be kept in step with anything.
 *
 * That needs one thing the terminal cannot otherwise know: WHERE THE PROMPT
 * ENDED. The shell is asked to say so with the OSC 133 semantic-prompt
 * markers, which are the convention every modern terminal uses for exactly
 * this — see GnuChanTerm.c's build_env(), which wraps the configured prompt in
 * them. A shell that does not emit them (or a user who has configured no
 * prompt) leaves this feature simply switched off, which is the only safe
 * default: without a known input start there is no way to tell the line from
 * the prompt, and guessing would put a ghost over a script's own output.
 *
 * --- what is NOT done ---
 *
 * No completion of file names, options or commands. fish does all of that, and
 * it does it from the inside with knowledge of the shell's grammar; a terminal
 * has neither. What is here is history, which needs no grammar: a line that was
 * run before and begins with what is on the line now is a line worth offering.
 */
#ifndef GNUCHANTERM_SUGGEST_H
#define GNUCHANTERM_SUGGEST_H

#include "term_module.h"
#include "term_core.h"

/* The longest command line this remembers or suggests. It is the same kind of
   ceiling the grid has on a row: a line longer than this is not suggested from
   and is not stored whole, because a terminal that grew a buffer per keystroke
   would be a terminal a program could exhaust. */
#define TERM_SUGGEST_LINE_MAX 1024

/* How many commands are remembered. The file on disk is trimmed to this many
   lines when it is written, so the store does not grow without bound across
   sessions — which it did in an earlier shape of this, where the history file
   was appended to on every command and never read back. */
#define TERM_SUGGEST_HISTORY_MAX 2000

/* What the terminal is offering, and the history it is drawn from.
 *
 * It lives in the core the way the selection does — see term_core.h — because
 * the input module is what acts on it and the renderer is what draws it, and
 * neither owns the other. */
typedef struct TermSuggest {
    /* The commands, oldest first, as C strings. They are kept whole because a
       suggestion is a whole line: a suggestion of half a command is a
       suggestion of nothing. */
    char **history;
    int    history_count;
    int    history_capacity;

    /* Which entry the up and down arrows are on, counting back from the newest
       as 1. Zero means "not browsing", which is the line the user is typing. */
    int    browsing;

    /* The line the user has typed, read off the grid. It is what the
       suggestion is computed FROM and it is re-read every time rather than
       maintained — see the file comment for why. */
    char   line[TERM_SUGGEST_LINE_MAX];
    int    line_length;

    /* Where the input starts, in the active grid's cells, as the shell last
       said with OSC 133. `input_y` is a SCREEN ROW and not a remembered line:
       the prompt is drawn on the screen and the suggestion is about what is on
       the screen. A cursor that is not on this row means the line has wrapped
       or the screen has scrolled, and no suggestion is made — see
       term_suggest_update(). */
    int    input_x;
    int    input_y;
    int    input_known;   /* 0 until a prompt end has been seen             */

    /* What is being offered: the tail of a remembered command, with the typed
       line taken off the front. Empty when there is nothing to offer. */
    char   suggestion[TERM_SUGGEST_LINE_MAX];
    int    suggestion_length;

    /* Where the ghost was DRAWN, and WHAT was drawn there, so the frame that
       takes it away knows which row to redraw. A ghost is not a cell and does
       not erase itself: without this the last suggestion stays on the window
       after the line changes under it. See term_render.c.
     *
     * `drawn_text` and not just the length, because two completions of the same
       length — `cd Doc` and `cd Dow` — would otherwise leave the first standing
       under the second, since a frame that only compared lengths would see no
       change to redraw for. */
    int    drawn_x;
    int    drawn_y;
    int    drawn_length;
    char   drawn_text[TERM_SUGGEST_LINE_MAX];

    /* The history file, and whether it has been read. Read once, on the first
       tick, and not in init: init runs before the loop and a file read that
       failed there would have nowhere to report it. */
    char   file[TERM_SUGGEST_LINE_MAX];
    int    loaded;
} TermSuggest;

/* --- life ----------------------------------------------------------------- */

void term_suggest_init(TermSuggest *suggest);
void term_suggest_free(TermSuggest *suggest);

/* --- the shell's word on where the input is ------------------------------- */

/* Called by the parser when the shell emits an OSC 133 semantic-prompt marker.
 * `kind` is the marker's letter: 'A' the prompt is starting, 'B' the prompt has
 * ended and the user may type, 'C' a command has begun running, 'D' it has
 * finished.
 *
 * Only B and C change anything here, and what they change is opposite: B says
 * the input starts at the cursor, C says there is no input any more and any
 * suggestion is stale. A and D are accepted and do nothing, which is right —
 * they carry no position. */
void term_suggest_marker(TermSuggest *suggest, TermCore *core, char kind);

/* --- the line, and the suggestion ----------------------------------------- */

/* Read the line off the grid and work out what to offer. Called on the
 * module's tick, which is the only place that can see both the grid as the
 * shell's echo has just left it and the keystroke that caused it — the two
 * arrive in that order and not in the same call. */
void term_suggest_update(TermCore *core);

/* Take the suggestion: send the part that has not been typed to the shell, so
 * its own line becomes the whole command and its own cursor lands at the end.
 *
 * Sending the tail and NOT the whole line is deliberate, and it is what keeps
 * readline the owner of the line. The text is inserted where its cursor
 * already is, so everything readline knows about the line — its cursor
 * position, its undo list, its history — stays true. Replacing the line would
 * mean sending escapes that mean "kill to start" in some readline modes and
 * something else in others.
 *
 * Returns 1 when something was taken, 0 when there was nothing to take. */
int term_suggest_accept(TermCore *core);

/* Forget the suggestion without sending anything. Called when the line changes
 * under a keystroke that the shell has not echoed yet, so a ghost from the
 * previous line is not left standing over the new one. */
void term_suggest_drop(TermSuggest *suggest);

/* --- the history ---------------------------------------------------------- */

/* Remember a finished command. Called when Enter is pressed at a prompt, with
 * the line that was on the screen at that moment.
 *
 * An empty line is not remembered, and neither is one that repeats the newest
 * entry: pressing Enter twice is not two commands, and a history of `ls`,
 * `ls`, `ls` makes every suggestion the same. */
void term_suggest_remember(TermSuggest *suggest, const char *line);

/* Write the history back to its file, newest last, at most
   TERM_SUGGEST_HISTORY_MAX lines. Best-effort: a machine with a read-only home
   keeps its history for the session and loses it at exit, which is worth more
   than refusing to run. */
void term_suggest_save(const TermSuggest *suggest);

/* Load the history file, if it has not been. Called from the first update. */
void term_suggest_load(TermSuggest *suggest);

/* --- the arrows: walking the history -------------------------------------- */

/* Move through the history with the up and down arrows, and put the entry on
 * the line.
 *
 * `direction` is +1 for the up arrow (back into the history) and -1 for the
 * down arrow (forward, towards the empty line that follows the newest). The
 * line is REPLACED rather than appended to, which is what a history recall
 * means, and the replacement is sent as backspaces followed by the text: a
 * backspace deletes backwards in every readline editing mode, where a
 * "kill to start" escape means one thing in emacs mode and another in vi. That
 * is the same reasoning that makes Tab send only the tail.
 *
 * It acts only while a prompt is on the line — see input_known — so a full
 * screen program that uses the arrows (an editor, a pager) keeps them.
 *
 * Returns 1 when the line was replaced, 0 when there was nothing to recall. */
int term_suggest_history(TermCore *core, int direction);

/* --- the module ----------------------------------------------------------- */

/* The suggestion, as a module. It is registered so its tick runs: the line is
   read off the grid on the tick and not on the keystroke, because the shell's
   echo has not been drawn yet when the key arrives — see term_suggest.c. */
extern const TermModule term_suggest_module;

#endif /* GNUCHANTERM_SUGGEST_H */
