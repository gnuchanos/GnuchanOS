/*
 * term_input.h — the keyboard and the mouse, turned into the bytes a program
 * reads.
 *
 * The other direction from everything else in this program. The parser turns a
 * program's bytes into a screen; this turns a key press into bytes. Both are
 * translations between the two worlds, and both have to be exact, because what
 * is on the other end is a program that was written against the sequences a
 * real terminal sends.
 *
 * --- what a key press becomes ---
 *
 * A letter becomes itself — one byte of UTF-8 — and a key that is not a letter
 * becomes a sequence of bytes introduced by ESC. Which sequence depends on
 * three things:
 *
 *   the KEY          an arrow is `ESC [ A`, but the delete key is `ESC [ 3 ~`
 *                    and the function keys are their own numbers
 *   the MODE         the program may have asked for "application cursor keys"
 *                    with DECCKM, and then an arrow is `ESC O A` instead. A
 *                    program that asked for one form and is sent the other
 *                    sees nothing at all when the arrow is pressed, which is
 *                    the kind of fault that looks like a broken keyboard.
 *   the MODIFIERS    shift, alt and control change the sequence's LAST number
 *                    and nothing else. `ESC [ 1 ; 5 A` is ctrl-up, and a
 *                    program that reads it (an editor moving a whole block,
 *                    say) reads the `5`.
 *
 * The tables below are the ones xterm sends, which is what every program on
 * the system was written against. They are not derived from anything; they are
 * copied, because being different here means being wrong.
 *
 * --- what is NOT done ---
 *
 * A key that has no sequence — a multimedia key, a key the program has no way
 * to hear — sends nothing. Sending a guess would put characters into whatever
 * has focus, and a terminal that types for the user is worse than one that
 * cannot hear a key.
 *
 * Composition (dead keys, an input method) is the toolkit's job and is not done
 * here: a key event carries the composed character already on this platform,
 * and there is nothing to compose. A session that needs an IME needs one at the
 * X level.
 */
 
#ifndef GNUCHANTERM_INPUT_H
#define GNUCHANTERM_INPUT_H

#include "term_core.h"
#include "term_pty.h"

/* --- the module ----------------------------------------------------------- */

/* The input as the core sees it: it answers KeyPress, ButtonPress and
   MotionNotify, and it writes to the PTY. It has no tick — a terminal that is
   not being typed into has nothing to send. */
extern const TermModule term_input_module;

/* --- what a module may call ---------------------------------------------- */

/* Send a string to the program, through the same path a keystroke takes. It is
   public because a paste is a string and it has to go through every escape the
   input module would apply — a pasted `\n` is an enter key and a pasted page of
   text is many characters, not one. */
void term_input_send(TermCore *core, const char *bytes, int len);

/* Send one Unicode code point. The code point is encoded to UTF-8 and sent, and
   the program's locale decides what it makes of it. */
void term_input_send_codepoint(TermCore *core, uint32_t codepoint);

/* Whether the program asked for the alternate cursor key form. Read from the
   parser rather than set here: the program is what asks. */
int  term_input_application_cursor(const TermCore *core);

#endif /* GNUCHANTERM_INPUT_H */
