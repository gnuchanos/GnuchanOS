/*
 * wifi_ui.h — the window, the list, and the keys.
 *
 * Everything the manager is made of comes together here: the display, the
 * palette, the wireless interface, the scan, and what has been typed so far.
 * The state is one struct rather than a set of globals, because there is
 * exactly one window per process and one struct is what makes that visible.
 *
 * There are two modes, and only two. The LIST is what a person sees first: the
 * networks in range, one per row, with the joined one marked and the selected
 * one highlighted. The PASSWORD is entered by choosing a locked network that is
 * not already saved: the list is replaced by a single line the password is
 * typed into. Enter in the list connects (asking for a password first when one
 * is needed), Enter in the password line connects with what was typed, Escape
 * goes back a mode or closes.
 *
 * This is an ORDINARY window: GnuChanWM, the window manager on this desktop,
 * frames it, moves it, focuses it and closes it — the program hands the window
 * a name and a class, maps it, and then answers the events that reach it, the
 * same as the terminal does. It is NOT an override-redirect window and it does
 * NOT grab the keyboard: a wifi window is an application a person leaves open
 * while they do something else, not a launcher that appears, is answered, and
 * goes away. The window manager is what puts it in the taskbar, gives it its
 * title bar, and lets the title bar's close button end it — through the
 * WM_DELETE_WINDOW the window asks for.
 */
#ifndef GNUCHANWIFI_UI_H
#define GNUCHANWIFI_UI_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "wifi_config.h"
#include "wifi_nm.h"
#include "wifi_style.h"

/* The most a password may be. Far longer than any real one, and the ceiling is
   what keeps a key held down from growing the buffer without end. */
#define WIFI_MAX_PASSWORD 256

typedef enum WifiMode {
    WIFI_MODE_LIST,       /* choosing a network                       */
    WIFI_MODE_PASSWORD,   /* typing the key for the chosen one        */
} WifiMode;

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

    WifiMode mode;            /* which of the two screens is showing      */

    /* The chosen row into `networks`, and the first row the list is scrolled
       to. Two numbers rather than one so the chosen row and the top of the
       visible list are told apart: they are only the same until the list is
       longer than the window. */
    int selected;
    int scroll;

    /* The SSID the password is being typed for, kept across the redraw so the
       password screen can name the network it is for. */
    char pending_ssid[WIFI_TEXT];
    char password[WIFI_MAX_PASSWORD];
    int password_length;

    /* The last thing that happened, shown at the bottom: "Connecting…", a
       failure nmcli gave back, or nothing. Cleared when the next action
       starts. */
    char status[WIFI_TEXT];

    int running;
} WifiUi;

/* Open the display, make the window, and take the first scan. Returns 0 on
   success, -1 when there is no display or the window could not be made.
   `config_path` is the settings file to read; an empty path uses the one
   wifi_config_path() finds. */
int wifi_ui_open(WifiUi *ui, const char *config_path);

/* Read keys and redraw until the manager is dismissed. Returns 0 when it
   closed normally. The connecting happens inside — see wifi_ui.c — because a
   wifi manager that only reported a choice would leave the joining to a caller
   that is this program. */
int wifi_ui_run(WifiUi *ui);

/* Close the window and free everything the open made. Safe to call whether or
   not the open succeeded. */
void wifi_ui_close(WifiUi *ui);

#endif /* GNUCHANWIFI_UI_H */
