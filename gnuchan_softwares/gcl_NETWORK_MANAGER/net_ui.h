/*
 * net_ui.h — the window: the panels, the buttons, and what each does.
 *
 * Everything the manager is made of comes together here: the display, the
 * palette, the interfaces, the resolver, and what has been typed so far. The
 * state is one struct rather than a set of globals, because there is exactly one
 * window per process and one struct is what makes that visible.
 *
 * --- the panels ---
 *
 *   DEVICES  every interface the machine has — wired, wireless, virtual,
 *            loopback — with its state and its address. Up/Down brings the
 *            chosen one up or down; Open Wi-Fi starts the wifi manager, which
 *            is the program that knows how to JOIN a network; DNS switches to
 *            the resolver panel.
 *
 *   DNS      the resolver the machine asks: the servers, where they came from
 *            (resolvectl or /etc/resolv.conf), and a line to edit them. Apply
 *            sets them; Revert goes back to automatic.
 *
 * The two are one window with two faces rather than two windows, because they
 * are two questions about the same machine and a person edits one after looking
 * at the other.
 *
 * --- this is an ORDINARY window ---
 *
 * GnuChanWM frames it, moves it, focuses it and closes it. It is NOT
 * override-redirect and it does NOT grab the keyboard — a network manager is an
 * application a person leaves open, not a launcher that appears and goes away.
 * (The one exception is the password dialog, which is its own window and does
 * grab the keyboard; see net_login.c.)
 */
#ifndef GNUCHANNET_UI_H
#define GNUCHANNET_UI_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "net_config.h"
#include "net_device.h"
#include "net_dns.h"
#include "net_style.h"

/* The most a DNS line typed here may be. Far longer than a row of addresses. */
#define NET_MAX_DNS_INPUT 512

/* The most buttons a panel's foot has. Four or five are drawn; the ceiling is
   room to add one without touching the array. */
#define NET_MAX_BUTTONS 8

typedef enum NetMode {
    NET_MODE_DEVICES,   /* the interfaces, and what to do with the chose one */
    NET_MODE_DNS,       /* the resolver, and a line to edit it               */
} NetMode;

/* What a button does. A value of its own rather than a key passed around, so
   the drawing and the clicking name the same thing and a button cannot be wired
   to the wrong action by a mistyped character. */
typedef enum NetAction {
    NET_ACTION_TOGGLE,       /* bring the chosen interface up or down      */
    NET_ACTION_WIFI,         /* start the wifi manager                     */
    NET_ACTION_DNS,          /* show the DNS panel                         */
    NET_ACTION_BACK,         /* back to the devices panel                  */
    NET_ACTION_REFRESH,      /* read the interfaces again                  */
    NET_ACTION_DNS_APPLY,    /* set the resolver to the line               */
    NET_ACTION_DNS_REVERT,   /* put the resolver back to automatic         */
} NetAction;

typedef struct NetButton {
    int x, y, width, height;
    NetAction action;
} NetButton;

typedef struct NetUi {
    Display *display;
    int screen;
    Window root;
    Window window;
    GC gc;

    /* The off-screen buffer every frame is drawn into before it is copied to
       the window. Drawing straight to the window shows the background being
       cleared and the rows redrawn one at a time, which reads as a flicker when
       the pointer moves over the list; the whole picture is built off-screen
       and put up in one copy instead. */
    Pixmap buffer;
    int buffer_width;
    int buffer_height;

    int screen_width;
    int screen_height;
    int width;
    int height;

    NetConfig config;
    NetStyle style;

    NetDeviceList devices;   /* the interfaces, from net_device.c         */
    NetDnsList dns;          /* the resolver, from net_dns.c              */

    NetMode mode;

    /* The chosen row into `devices`, and the first row the list is scrolled to.
       Two numbers so the chosen row and the top of the visible list are told
       apart: they are only the same until the list is longer than the window. */
    int selected;
    int scroll;

    /* The row the pointer is over, or -1. Drawn with a faint fill so the list
       answers the mouse before it is clicked. */
    int hover;

    /* The two DNS fields, the shape a Windows adapter's own DNS boxes have: a
       Preferred server and an Alternate one. They are seeded from the current
       servers when the panel opens — the first server into Preferred, the
       second into Alternate — so Apply sends back what is shown unless it is
       changed. Both empty applied means "back to automatic", which is that
       same Windows dialog's "Obtain automatically". */
    char dns_primary[NET_TEXT];
    char dns_alternate[NET_TEXT];
    int dns_focus;   /* 0 = the Preferred field has the caret, 1 = Alternate */

    /* The buttons, filled in by the layout before they are drawn and read by
       the click handler. Kept in the state rather than recomputed on a click
       because the click has only a position. */
    NetButton buttons[NET_MAX_BUTTONS];
    int button_count;

    /* The last thing that happened, shown at the foot: a failure, "Applying…",
       or nothing. Cleared when the next action starts. */
    char status[NET_TEXT * 2];

    int running;
} NetUi;

/* Open the display, make the window, read the interfaces and the resolver.
   Returns 0 on success, -1 when there is no display or the window could not be
   made. `config_path` is the settings file to read; an empty path uses the one
   net_config_path() finds. */
int net_ui_open(NetUi *ui, const char *config_path);

/* Read keys, clicks and redraws until the manager is dismissed. Returns 0 when
   it closed normally. */
int net_ui_run(NetUi *ui);

/* Close the window and free everything the open made. Safe to call whether or
   not the open succeeded. */
void net_ui_close(NetUi *ui);

#endif /* GNUCHANNET_UI_H */
