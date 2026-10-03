/*
 * notif_render.h — the stack of bubbles, laid out and drawn.
 *
 * One window holds the whole stack, and this module owns the window too: it
 * makes it, cuts it into the right shape, lays the bubbles out inside it, and
 * paints them. That makes it the one place that knows both how big the stack is
 * and where each bubble sits, which is what lets the hit test and the drawing
 * share one account of the geometry.
 *
 * The colours and fonts are kept resolved: a colour name is turned into a pixel
 * once at start-up, not once a notification, and the two Xft fonts are opened
 * once and shared by every bubble.
 *
 * Everything is painted into a buffer the window's own size and then copied up
 * in one operation, so a redraw is a single repaint and no flicker.
 */
#ifndef GNUCHANNOTIFICATION_RENDER_H
#define GNUCHANNOTIFICATION_RENDER_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "notif_config.h"
#include "notif_item.h"

typedef struct NotifRender {
    Display *display;
    int screen;
    Window root;
    Visual *visual;
    int depth;
    Colormap colormap;
    GC gc;

    NotifConfig config;

    unsigned long background;
    unsigned long background_alt;
    unsigned long frame;
    unsigned long title;
    unsigned long body;
    unsigned long accent;
    unsigned long urgent;
    unsigned long icon_background;

    XftFont *font_title;
    XftFont *font_body;
    XftColor colour_title;
    XftColor colour_body;
    XftColor colour_accent;
    int have_colours;

    Window window;
    Pixmap buffer;
    int buffer_width;
    int buffer_height;

    int window_width;
    int window_height;

    /* The clock the countdown bars are measured against, set by the loop
       before each draw. A bubble's remaining time is its own born_ms against
       this; the renderer is handed the number rather than reading a clock
       itself, so the same instant governs every bubble in one frame. */
    long long now_ms;
} NotifRender;

/* Open the display's resources, resolve the palette, make the two fonts, and
   create the stack window. Returns 0 on success, -1 when the display or the
   window could not be opened, in which case nothing is left allocated. */
int notif_render_init(NotifRender *render, Display *display, int screen,
                      const NotifConfig *config);

/* Lay the stack out for the current number of items. Called after any change
   to the stack. */
void notif_render_layout(NotifRender *render, NotifStack *stack);

/* Paint the whole stack into the window. Nothing is drawn when the stack is
   empty: the window is unmapped instead. */
void notif_render_draw(NotifRender *render, NotifStack *stack);

/* Move the window to the corner the settings name. Called by the layout. */
void notif_render_place(NotifRender *render);

/* Release the window, the buffer, the fonts and the colours. Safe to call
   twice and safe on a half-built renderer. */
void notif_render_free(NotifRender *render);

#endif /* GNUCHANNOTIFICATION_RENDER_H */
