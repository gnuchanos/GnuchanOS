/*
 * dock_theme.h — the icon a running window means, found by its class.
 *
 * Not every window publishes its own icon in _NET_WM_ICON. The GnuchanOS
 * programs are the clear case: GnuChanTerm runs with no _NET_WM_ICON at all
 * (checked with xprop), so a dock that trusted that property alone would draw
 * a nameless plate for every one of them. What a window always has is a
 * WM_CLASS, and a WM_CLASS is exactly what the freedesktop files key an
 * application to: the .desktop file names its StartupWMClass, and its Icon=
 * names a picture in the icon theme.
 *
 * So this is the other road to the same place: class -> .desktop -> Icon name
 * -> a file on disk. dock_icon.c takes it only when the property road led
 * nowhere, so a window that does publish its icon is never second-guessed.
 */
#ifndef GNUCHANDOCK_THEME_H
#define GNUCHANDOCK_THEME_H

/* The longest class name, icon name, or path this deals in. Long enough for a
   reverse-DNS desktop file name and the path under a theme that holds it. */
#define DOCK_THEME_TEXT 512

/* The icon file the class names, written into `out`, and 1 returned, or 0
   when the class names nothing installed. `size` is the side the dock wants,
   used to pick the nearest picture a theme holds. */
int dock_theme_icon_for_class(const char *wm_class, int size,
                              char *out, unsigned int out_size);

#endif /* GNUCHANDOCK_THEME_H */
