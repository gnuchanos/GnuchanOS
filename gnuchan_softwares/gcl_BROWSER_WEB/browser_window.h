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
class QWebEnginePage;
class QWebEngineView;

class BrowserWindow : public QMainWindow {
public:
    explicit BrowserWindow(QWidget *parent = nullptr);

    /* Make a tab the ENGINE can fill, and give back its page. This is what a
       page that asks for a new window is handed (see ReportingPage's
       createWindow): the engine loads the new content into the page this
       returns, so a target="_blank" link or a window.open() lands in a tab of
       this window rather than being dropped on the floor. Public because the
       page, which is not this class, is what calls it. */
    QWebEnginePage *newTabPage();

private:
    /* A view with its page, its settings and its signals, added to the strip
       and selected when asked, but pointed at NOTHING — about:blank — so a
       caller can set the url itself. openTab() is this plus the url. */
    QWebEngineView *addTab(bool switch_to_it);

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

    /* Close the tab the strip is showing — what Ctrl+W does. */
    void closeCurrentTab();

    /* Open a tab on the home page and put the cursor in the address bar: what
       the toolbar's ✚ and Ctrl+T both do. */
    void openNewTab();

    /* Navigation, split out so the toolbar's buttons and the keys share one
       body each rather than one copy per caller. */
    void goBack();
    void goForward();
    void reloadCurrent();

    /* Zoom the current tab. adjustZoom() does the work; the three below are
       what the keys call, and their names are what the shortcut table reads
       as. */
    void zoomIn();
    void zoomOut();
    void zoomReset();

    /* Put the cursor in the address bar, ready to be typed over: what a person
       wants the instant a new tab opens, and all Ctrl+L does. */
    void focusAddress();

    /* Multiply the current tab's zoom by `factor`, held to a sane range so a
       key held down cannot scroll the page into a single letter or out of the
       window. */
    void adjustZoom(double factor);

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
