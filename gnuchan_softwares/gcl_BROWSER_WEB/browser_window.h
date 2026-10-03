/*
 * browser_window.h — the whole browser, in one window.
 *
 *   tab strip       one tab per page, each a QWebEngineView
 *   tool bar        back / forward / reload / home / address / star / new tab
 *   bookmark bar    one button per saved favourite
 *   status bar      "Done" when a page lands
 *
 * The window carries no Q_OBJECT. Every connection it makes is a lambda on a
 * signal Qt already emits — the tab strip changing its page, the view
 * reporting its url or title, a button being pressed — so the program builds
 * with one g++ call and no meta-object compiler. A browser this small does not
 * need a code generator to run it.
 */
#ifndef GNUCHAN_BROWSER_WINDOW_H
#define GNUCHAN_BROWSER_WINDOW_H

#include <QList>
#include <QMainWindow>
#include <QUrl>

#include "bookmarks.h"

class QLineEdit;
class QTabWidget;
class QToolBar;
class QWebEngineView;

class BrowserWindow : public QMainWindow {
public:
    explicit BrowserWindow(QWidget *parent = nullptr);

private:
    /* A fresh tab, pointing at `url` (home when empty), selected when asked.
       Returns the view so a caller can keep working with it. */
    QWebEngineView *openTab(const QUrl &url, bool switch_to_it = true);

    /* The view of the selected tab, or nullptr if none. */
    QWebEngineView *currentView() const;

    /* Load whatever the address bar says, in the current tab. */
    void navigateFromAddress();

    /* Put the page's own title in the window title. */
    void updateWindowTitle(const QString &page_title);

    /* Close a tab, opening the home page if it was the last one. */
    void closeTab(int index);

    /* Add the current page to the favourites, or remove it if it is there. */
    void toggleBookmark();

    /* Rebuild the bookmark bar from the list. */
    void rebuildBookmarkBar();

    QTabWidget *tabs;
    QLineEdit *address;
    QToolBar *bookmark_bar;
    QList<Bookmark> bookmarks;
};

#endif /* GNUCHAN_BROWSER_WINDOW_H */
