/*
 * wm_switcher.h — the window switcher: every window as a picture, one of them
 * chosen.
 *
 * This is what Alt+` opens, and it is the thing every desktop has and this one
 * did not: pressing the key shows the open windows as small pictures with the
 * name under each, pressing it again moves to the next one, and letting the
 * modifier go brings the chosen one forward. Holding the modifier is what
 * keeps it up — the switcher is not a window that stays, it is a gesture.
 *
 * The two halves are deliberately in two files:
 *
 *   wm_switcher.c        which windows are in it, which one is chosen, and the
 *                        keys and clicks that move the choice
 *   wm_switcher_view.c   the window that is drawn: the grid, the pictures read
 *                        off the screen, and the name under each
 *
 * The list is the MRU order and not the frame table's, and that is the whole
 * reason the switcher is usable: the frame table is creation order, so one
 * press of the key would land on whatever program was started second rather
 * than on the window the user was in a moment ago. wm_focus.c already keeps
 * that order — it is what wm_focus_previous() and the old one-press switch
 * read — so the list is built from it, and pressing the key twice goes there
 * and back exactly as it always did.
 *
 * A picture is read from the screen rather than asked of the window, and that
 * is a decision worth stating: XGetImage on a window whose lower parts are
 * covered returns undefined pixels for the covered parts, and almost every
 * window in a switcher is covered by something. What is on the screen is what
 * the user was looking at, so each window's rectangle is taken off the root —
 * every one of them before the overlay is mapped, or the reads would find the
 * overlay over the windows they are reading. A window that is minimised has no
 * pixels anywhere and is drawn as its icon instead.
 */
#ifndef GNUCHANWM_SWITCHER_H
#define GNUCHANWM_SWITCHER_H

#include <X11/Xlib.h>

#include "wm_module.h"

/* WmCore is pointed at, never inspected here. */
typedef struct WmCore WmCore;

/* The most windows the switcher will show. A session with more open than this
   has a switcher that is a wall of pictures nobody can read, and the ceiling
   is a bound on the array rather than on what a program may open. */
#define WM_SWITCHER_MAX_CELLS 32

/* The longest name drawn under a cell. A window title can be any length and a
   cell is a fixed width, so a longer one is clipped with an ellipsis. */
#define WM_SWITCHER_NAME_LENGTH 160

/* One cell: a window, and what is needed to draw and to activate it. */
typedef struct WmSwitcherCell {
    Window client;      /* the program's window, which is what is activated  */
    Window frame;       /* the manager's window, which is what is in the list */
    int minimized;      /* put away, so there is no picture of it anywhere    */
    int workspace;      /* which desktop it is on; always the current one     */
    char name[WM_SWITCHER_NAME_LENGTH];
} WmSwitcherCell;

/* The switcher's state. One switcher exists, so it lives here rather than on
   the core: a second one open at once is not a thing that can exist, and a
   field on the core would suggest it could. */
typedef struct WmSwitcher {
    int open;
    int count;
    int selected;       /* index into cells, -1 when there are none */
    WmSwitcherCell cells[WM_SWITCHER_MAX_CELLS];
} WmSwitcher;

/* --- the switcher (wm_switcher.c) ----------------------------------------- */

/* Open it, or move to the next window if it is already open. This one entry
   point is what the key binding calls: the first press of the gesture opens
   the switcher and every later one steps it, which is why there is no
   separate "next". Returns 1 when a switcher is up afterwards, 0 when there
   was nothing to switch between. */
int  wm_switcher_open(WmCore *core);

/* Move the choice by `step`, wrapping round at both ends. */
void wm_switcher_advance(WmCore *core, int step);

/* Bring the chosen window forward and take the switcher down. */
void wm_switcher_commit(WmCore *core);

/* Take the switcher down and leave the focus where it was. */
void wm_switcher_cancel(WmCore *core);

/* Put the overlay back above everything, when it is up. Called after the tray
   has raised itself, so the pictures are not covered by the strip they were
   opened over. */
void wm_switcher_raise(WmCore *core);

/* --- the view (wm_switcher_view.c) ---------------------------------------- */

/* Lay the grid out, read the pictures off the screen, make the overlay and put
   it up. Called once, at the start of a gesture: the pictures are a snapshot
   of the desktop as it was when the key was pressed, and are not read again
   while the modifier is held. */
int  wm_switcher_view_open(WmCore *core, WmSwitcher *switcher);

/* Draw it again, for a choice that moved. */
void wm_switcher_view_show(WmCore *core, WmSwitcher *switcher);

/* Follow the pointer for the hover highlight. */
void wm_switcher_view_motion(WmCore *core, WmSwitcher *switcher,
                             int root_x, int root_y);

/* Which cell a point on the screen is over, or -1. */
int  wm_switcher_view_cell_at(WmCore *core, int root_x, int root_y);

/* Put the overlay away and free everything the open made. */
void wm_switcher_view_close(WmCore *core);

/* The overlay's own window, or None when nothing is up. The overlay is the one
   window of this manager that is meant to be above everything else, so
   anything that raises itself — the tray sitting in a bar — asks for it after
   doing so, and the switcher puts itself back on top when it is up. */
Window wm_switcher_overlay_window(void);
void   wm_switcher_overlay_raise(void);

/* --- the module ------------------------------------------------------------ */

/* The module owns the gesture's input: the keys while the modifier is held,
   the click that chooses, and the release that confirms. It is registered
   beside the keys module and does nothing at all until a switch has been
   asked for. */
extern const WmModule wm_switcher_module;

#endif /* GNUCHANWM_SWITCHER_H */
