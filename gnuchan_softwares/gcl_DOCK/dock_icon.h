/*
 * dock_icon.h — a picture in the dock, as one pixmap.
 *
 * Every icon the dock draws comes through here, whether it started as a file
 * on disk — the gear, the logo — or as the ARGB list a window published in
 * _NET_WM_ICON. Both are turned into the same thing: a colour pixmap, one
 * `side` square, with the picture composited over the dock's own background
 * colour.
 *
 * The background is why there is no separate mask. An icon drawn as a bare
 * pixmap would carry a rectangle of whatever the file had where it is
 * transparent — usually black — and that rectangle would hide the dock under
 * it. Compositing onto the dock's own colour makes a transparent pixel BE the
 * dock, so the logo floats on the bar rather than in a box, and the drawing
 * code has one plain pixmap to put down.
 *
 * The side is fixed per dock, not per icon: a dock whose icons are each a
 * different size is a dock that cannot be laid out. So an icon is scaled once,
 * here, and the drawer never has to.
 *
 * Imlib2 is what reads a file, the same library GnuChanWM draws its wallpaper
 * with — so a machine that runs this desktop already has it. _NET_WM_ICON is
 * read directly, the way wm_icon.c in the window manager does, because that
 * property is already pixels and handing it to a file decoder would mean
 * writing it out and reading it back.
 */
#ifndef GNUCHANDOCK_ICON_H
#define GNUCHANDOCK_ICON_H

#include <X11/Xlib.h>

typedef struct DockIcon {
    /* The icon, one `side` by `side` pixmap, or None when nothing is loaded. */
    Pixmap pixmap;

    int side;    /* the side it was made at                             */
    int ok;      /* 1 once it holds something worth drawing             */
} DockIcon;

/* Load the picture at `name` scaled to a `side` square, over `background`. A
   leading "~" is the home directory; a relative name is looked for beside the
   settings script. Returns 0 on success, -1 when the file cannot be found or
   read, in which case the icon is left not-ok. The caller owns `icon` and must
   have zeroed it first. */
int dock_icon_load_file(Display *display, Window root, Visual *visual,
                        int depth, DockIcon *icon, const char *name, int side,
                        unsigned long background);

/* Build an icon from a window's _NET_WM_ICON property, scaled to a `side`
   square over `background`. Returns 0 on success, -1 when the client published
   no usable icon, in which case the icon is left not-ok. */
int dock_icon_load_window(Display *display, Window root, Visual *visual,
                          int depth, DockIcon *icon, Window client, int side,
                          unsigned long background);

/* Release the pixmap and mark the icon empty. Safe on an empty icon. */
void dock_icon_free(Display *display, DockIcon *icon);

/* Put the icon on a drawable at (x, y). Returns 1 when something was drawn and
   0 when the icon is empty. */
int dock_icon_draw(Display *display, GC gc, const DockIcon *icon,
                   Drawable target, int x, int y);

#endif /* GNUCHANDOCK_ICON_H */
