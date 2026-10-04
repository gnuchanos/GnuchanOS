/*
 * term_vt_osc.h — the OSC strings this terminal acts on.
 *
 * An OSC ("operating system command") is a string a program sends inside
 * ESC ] ... BEL/ST. Most are none of the terminal's business and are consumed
 * and dropped, which is what stops their bytes reaching the screen. Two
 * families are acted on and they are the ones here:
 *
 *   - the colours a program sets and asks about: OSC 4, 10, 11, 12, 104 and
 *     110-112 (see term_vt.h's TermVtHost.color for what the callback means);
 *   - a picture placed over the cells: OSC 1338, this terminal's own sequence
 *     (see term_image.h).
 *
 * They live in their own file because they are a group of their own: they are
 * a grammar inside a grammar — a colour is written `rgb:RR/GG/BB` or `#RGB`,
 * with none of the CSI state machine's parameters — and a reader looking for
 * "how does this terminal read a colour a program sends" should not have to
 * find it among the cursor and erase sequences. The dispatcher, vt_finish_osc()
 * in term_vt.c, keeps the marker and title families; each family's own grammar
 * is kept with the family.
 */
#ifndef GNUCHANTERM_VT_OSC_H
#define GNUCHANTERM_VT_OSC_H

#include "term_vt.h"

/* Interpret an OSC body as a colour request — OSC 4, 10, 11, 12, 104 and
   110-112 — and report it to the host's colour callback. Returns 1 when the
   string WAS a colour request (handled or not) and 0 when it is not one of the
   codes this reads. The return value is what keeps a title from being offered
   to the colour callback: the two families are told apart by the leading
   number and by nothing else. */
int term_vt_handle_color_osc(TermVt *vt, const char *body);

/* Interpret an OSC body as the `ESC ] 1338 ; x ; y ; cols ; rows ; path`
   picture sequence and report it to the host's image callback. A body that is
   not that sequence, or a host with no image callback, is left alone. */
void term_vt_handle_image_osc(TermVt *vt, const char *body);

#endif /* GNUCHANTERM_VT_OSC_H */
