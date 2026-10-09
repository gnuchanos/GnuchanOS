/*
 * wifi_ui.h — the window, the list, the buttons, and what each does.
 *
 * Everything the manager is made of comes together here: the display, the
 * palette, the wireless interface, the scan, the saved profiles, the radio, the
 * kernel's kill switches, and what has been typed so far. The state is one
 * struct rather than a set of globals, because there is exactly one window per
 * process and one struct is what makes that visible.
 *
 * --- the modes ---
 *
 *   LIST      the networks in range, one per row, with the joined one marked,
 *             the selected one highlighted, a saved one shown as such, a bar at
 *             the top giving the connection and its address, a warning when the
 *             kernel has the radio blocked, and a row of buttons at the foot.
 *   PASSWORD  one line, the key for a locked network that is not already saved.
 *   CONFIRM   a yes/no, asked before forgetting a saved network — a thing that
 *             throws away a password is worth one keystroke of doubt.
 *   RESTART   a yes/no, asked before reloading the driver. The network drops
 *             while it happens, which is the second thing worth a confirmation.
 *
 * --- the two ways to act ---
 *
 * There are BUTTONS at the foot of the list, drawn as boxes a person clicks:
 * Rescan, Disconnect, Forget, Autoconnect, Restart, Quit. And there are the
 * same actions as LETTERS — r, d, f, a, R, q — for a person who would rather
 * not reach for the mouse. The two do the same things and go through the same
 * functions; a button is a rectangle that knows which action it is, and a
 * letter is a keysym that resolves to one.
 *
 * Escape steps back a mode, or closes the window from the list; Enter acts.
 *
 * This is an ORDINARY window: GnuChanWM, the window manager on this desktop,
 * frames it, moves it, focuses it and closes it. It is NOT an override-redirect
 * window and it does NOT grab the keyboard — a wifi window is an application a
 * person leaves open, not a launcher that appears and goes away.
 */
#ifndef GNUCHANWIFI_UI_H
#define GNUCHANWIFI_UI_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "wifi_config.h"
#include "wifi_nm.h"
#include "wifi_radio.h"
#include "wifi_rfkill.h"
#include "wifi_saved.h"
#include "wifi_style.h"

/* The most a password may be. Far longer than any real one, and the ceiling is
   what keeps a key held down from growing the buffer without end. */
#define WIFI_MAX_PASSWORD 256

/* The most buttons the foot has. Six are drawn; the ceiling is room to add one
   without touching the array. */
#define WIFI_MAX_BUTTONS 8

typedef enum WifiMode {
    WIFI_MODE_LIST,       /* choosing a network                        */
    WIFI_MODE_PASSWORD,   /* typing the key for the chosen one         */
    WIFI_MODE_CONFIRM,    /* yes/no before forgetting the chosen one   */
    WIFI_MODE_RESTART,    /* yes/no before reloading the driver        */
} WifiMode;

/* What a button does. A value of its own rather than a key passed around, so
   the drawing and the clicking name the same thing and a button cannot be wired
   to the wrong action by a mistyped character. */
typedef enum WifiAction {
    WIFI_ACTION_RESCAN,
    WIFI_ACTION_DISCONNECT,
    WIFI_ACTION_FORGET,
    WIFI_ACTION_AUTOCONNECT,
    WIFI_ACTION_RESTART,
    WIFI_ACTION_QUIT,
} WifiAction;

typedef struct WifiButton {
    int x, y, width, height;
    WifiAction action;
} WifiButton;

typedef struct WifiUi {
    Display *display;
    int screen;
    Window root;
    Window window;
    GC gc;

    int screen_width;
    int screen_height;
    int width;              /* the window's own size, worked out at open */
    int height;

    WifiConfig config;
    WifiStyle style;

    char device[WIFI_TEXT];   /* the wireless interface, or "" when none */
    WifiList networks;        /* the last scan                            */
    WifiSavedList saved;      /* the profiles NetworkManager remembers    */

    /* The radio: 1 on, 0 off, -1 not known. Read with the scan so the switch at
       the top is right when the window opens, and again after it is changed. */
    int radio_on;

    /* The kernel's kill switches. A hard block is a physical switch and cannot
       be cleared from here; a soft block the window can clear. Read with the
       scan, because a block is the reason an empty list is empty. */
    WifiBlock block;

    /* The wireless driver's module name, read off the interface for the restart
       action. Empty when it could not be read and the config named none. */
    char driver_module[WIFI_TEXT];

    /* What is joined now: the connection's name and its address, for the line at
       the top. Empty when nothing is connected. */
    char active_ssid[WIFI_TEXT];
    char active_address[WIFI_TEXT];

    WifiMode mode;            /* which of the screens is showing          */

    /* The chosen row into `networks`, and the first row the list is scrolled
       to. Two numbers rather than one so the chosen row and the top of the
       visible list are told apart: they are only the same until the list is
       longer than the window. */
    int selected;
    int scroll;

    /* The SSID the password is being typed for, and the profile name a
       confirmation is about. Both are kept across a redraw so their screens can
       name what they are about. */
    char pending_ssid[WIFI_TEXT];
    char pending_saved[WIFI_TEXT];

    char password[WIFI_MAX_PASSWORD];
    int password_length;

    /* The buttons at the foot, filled in by the layout before they are drawn
       and read by the click handler. Kept in the state rather than recomputed
       on a click because the click has only a position: the rectangle that
       holds it is what says which action it was. */
    WifiButton buttons[WIFI_MAX_BUTTONS];
    int button_count;

    /* The last thing that happened, shown at the bottom: "Connecting…", a
       failure nmcli gave back, or nothing. Cleared when the next action
       starts. Twice WIFI_TEXT, because the messages name a network and an SSID
       is itself up to WIFI_TEXT long. */
    char status[WIFI_TEXT * 2];

    int running;
} WifiUi;

/* Open the display, make the window, take the first scan, and read the saved
   profiles, the radio and the kill switches. Returns 0 on success, -1 when
   there is no display or the window could not be made. `config_path` is the
   settings file to read; an empty path uses the one wifi_config_path() finds. */
int wifi_ui_open(WifiUi *ui, const char *config_path);

/* Read keys, clicks and redraws until the manager is dismissed. Returns 0 when
   it closed normally. The connecting, forgetting, restarting and radio changes
   happen inside — see wifi_ui.c — because this program is the only consumer of
   those choices. */
int wifi_ui_run(WifiUi *ui);

/* Close the window and free everything the open made. Safe to call whether or
   not the open succeeded. */
void wifi_ui_close(WifiUi *ui);

#endif /* GNUCHANWIFI_UI_H */
