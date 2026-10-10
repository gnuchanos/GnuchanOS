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
 *            the resolver panel; DPI: Open starts the DPI bypass (zapret's
 *            nfqws) that lets the whole machine reach a site a filter blocks by
 *            reading the TLS name inside a packet, and turns it off again. See
 *            net_dpi.c.
 *
 *   DNS      the resolver the machine asks: the servers as they are and where
 *            they came from (resolvectl or /etc/resolv.conf), then four boxes
 *            to edit them — IPv4 and IPv6, each with a Preferred and an
 *            Alternate, the way Windows asks it. Apply sets them; Revert goes
 *            back to automatic.
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
#include "net_dpi.h"
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
    NET_ACTION_DPI,          /* start or stop the DPI bypass (zapret)      */
    NET_ACTION_BACK,         /* back to the devices panel                  */
    NET_ACTION_REFRESH,      /* read the interfaces again                  */
    NET_ACTION_DNS_APPLY,    /* set the resolver to the box                */
    NET_ACTION_DNS_REVERT,   /* put the resolver back to automatic         */
    NET_ACTION_DNS_SECURE,   /* turn encrypted DNS (DoT) on or off         */
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

    /* The DNS boxes, the shape a Windows adapter's own DNS dialogs have:
       separate IPv4 and IPv6 entries, each with a Preferred and an Alternate.
       Four boxes, because that is the four values a person is handed when they
       look up "Google DNS" — 8.8.8.8, 8.8.4.4, and the two IPv6 ones — and one
       pair of boxes cannot hold both families at once.

       They are seeded from the servers the system reports when the panel opens,
       sorted by family, so Apply sends back what is shown unless it is changed.
       All four empty applied means "back to automatic", which is the same
       Windows dialog's "Obtain automatically". */
    char dns_v4_pref[NET_TEXT];
    char dns_v4_alt[NET_TEXT];
    char dns_v6_pref[NET_TEXT];
    char dns_v6_alt[NET_TEXT];
    int dns_focus;   /* 0..3: v4 preferred, v4 alternate, v6 preferred, v6 alt */

    /* Whether encrypted DNS (DNS-over-TLS) is on. It is a toggle the person
       flips, not something read back from the system — resolvectl has no plain
       "is it on" query — so this is what was last asked for, and the button
       follows it: "Secure DNS: on" / "Secure DNS: off". Encrypted DNS is the
       one thing that gets a query past a network that filters by reading the
       name inside a port-53 packet, so it is worth a button of its own rather
       than a line in a config file. */
    int dns_secure;

    /* Whether the DPI bypass is running, and whether zapret is installed to run
       at all. Both are read back from the system (net_dpi_active() and
       net_dpi_available()) rather than remembered, because the bypass can be
       started outside this window — by a boot service or by hand — and the
       button has to show what IS, not what was last pressed. `dpi_available` is
       what marks the button unavailable on a machine with no zapret;
       `dpi_active` is what makes it read "DPI: Close" rather than "DPI: Open". */
    int dpi_active;
    int dpi_available;

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
