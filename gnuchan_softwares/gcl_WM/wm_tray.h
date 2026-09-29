/*
 * wm_tray.h — the system tray: the place a running program leaves an icon.
 *
 * A program that keeps running with no window of its own — a chat client, a
 * music player, Steam — still has to be reachable, and the freedesktop system
 * tray is where it is reachable from. The program asks the manager for room,
 * the manager gives it a place, and the program puts a small window of its own
 * there. What the user clicks is that window: the program draws it, the
 * program receives the click, and the program opens its own menu. The manager
 * supplies the room and nothing else, which is the whole point — a manager
 * that drew the icon itself would have to know what every program's menu is.
 *
 * The protocol is XEmbed: the manager owns the _NET_SYSTEM_TRAY_S<screen>
 * selection, which is how a program finds the tray at all, and the program
 * sends its icon's window to the owner when it wants to dock. The icon is
 * reparented into the manager's dock window and moved into place.
 *
 * The dock window is a child of the ROOT rather than of a bar, and that is a
 * rule of X and not a choice: a window's children must have the window's own
 * depth, and the bar is drawn at the screen's depth while a tray icon may ask
 * to be drawn at another. A window of another depth can only be a child of the
 * root, so the dock lives on the root and the bar tells it where to sit.
 */
#ifndef GNUCHANWM_TRAY_H
#define GNUCHANWM_TRAY_H

#include <X11/Xlib.h>

#include "wm_module.h"

/* WmCore is pointed at, never inspected here; its struct lives in wm_core.h. */
typedef struct WmCore WmCore;

/* The room left between two docked icons. An icon is sized to the bar's own
   thickness, so there is no icon size here. */
#define WM_TRAY_GAP 2

/* The most icons the tray holds. A tray with more programs docked than this is
   a tray nobody can read, and the ceiling is a bound on the array rather than
   on what a program may ask for. */
#define WM_TRAY_MAX_ICONS 32

/* The room the docked icons take along the bar's own axis: their sides added
   up on a horizontal bar and the same on a vertical one, with the gap between
   each pair. Zero when nothing is docked, which is what makes an empty tray
   take no room rather than leave a gap in the bar.

   `thickness` is the bar across its own axis — the height of a horizontal bar,
   the width of a vertical one — because it is what an icon is sized to: an
   icon is a square no bigger than the strip it sits in. The measuring pass and
   the drawing pass both give the bar's own thickness, so the room the box asks
   for and the room the icons take are the same number. */
int wm_tray_extent(WmCore *core, int thickness);

/* Where the tray sits, and how big it is. Called by the bar's own drawing for
   the cell the group box was given, with the cell's place INSIDE that bar —
   `bar` is the bar's window, which is what turns the cell's own coordinates
   into the screen coordinates the dock window, a child of the root, needs.
   `vertical` is which way the bar runs, so the icons are laid out left to
   right or top to bottom. `background` is the pixel the cell is filled with,
   and becomes the dock's own background, so the tray sits on the bar rather
   than in a rectangle of its own. */
void wm_tray_place(WmCore *core, Window bar, int vertical, int x, int y,
                   int width, int height, unsigned long background);

/* Put the tray back above the bar. Called after the bar is lowered, so the
   icons stay visible over the strip they sit in. */
void wm_tray_raise(WmCore *core);

/* Put the dock directly above the bar it was placed against, and below every
   other window.
 *
 * This is the one that keeps a tray icon on the bar. `raise` above says "over
   the strip it sits in", and the way to be over the strip and not over the
   screen is to be stacked against the bar rather than lifted to the top: the
   dock and the bar are both the root's children, so the top of the stack is
   where the windows are.
 *
 * Called from wm_desktop_lower_bar(), after the bars have been put at the
 * bottom, so the order is settled in one place: bar lowest, dock on the bar,
 * windows above both. */
void wm_tray_lower(WmCore *core);

/* Forget where the last frame put the tray, and put it away again if nothing
   put it back. The pair is the bar's own drawing saying "this is where the
   group box is this time" and "and if there was no group box, the tray is not
   wanted": a script that took the group box out of its bar gets a tray that is
   not on the screen, rather than one left behind at the place it last was. */
void wm_tray_begin(WmCore *core);
void wm_tray_finish(WmCore *core);

/* Whether a window is the tray's own dock. The menu's logout walk asks, for
   the same reason it asks about the bar: killing a window this process owns
   closes its connection to the server, and every kill queued behind it is
   discarded with it. The docked icons are the programs' own windows and are
   left alone — they are the session's programs, and ending the session is what
   the walk is for. */
int wm_tray_is_window(Window window);

/* The module claims the tray selection and answers programs that want to dock. */
extern const WmModule wm_tray_module;

#endif /* GNUCHANWM_TRAY_H */
