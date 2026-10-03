/*
 * browser_theme.h — the browser's one look.
 *
 * The whole appearance is a Qt style sheet applied to the application, so every
 * widget — tool bar, tabs, address bar, buttons, menus — is painted from the
 * same palette without a subclass in sight. The colours are the GnuchanOS
 * violet the rest of the desktop uses, so the browser belongs to the desktop
 * rather than to Qt's default grey.
 */
#ifndef GNUCHAN_BROWSER_THEME_H
#define GNUCHAN_BROWSER_THEME_H

#include <QString>

/* The style sheet. Returned as text rather than applied here so that the
   application decides when it goes on, and so a test can read it. */
QString browser_style_sheet();

#endif /* GNUCHAN_BROWSER_THEME_H */
