/*
 * browser_window.cpp — the interface: tabs, toolbar, bookmark bar.
 *
 * Tuned for a 2007 laptop: a software-rendered engine on two cores, where the
 * cost of doing more than is asked is a fan that never stops. Two things are
 * therefore deliberate and worth naming.
 *
 *   The page's own console is NOT relayed to stderr unless it is asked for.
 *   The engine drops a page's console by default; echoing it — which this
 *   browser used to do on every message — is a write and a flush on the
 *   browser's own thread for every line a modern page logs, and a chatty page
 *   (a site that logs per animation frame, or a failure loop) turns that into
 *   thousands of syscalls a second that buy the user nothing. It is kept
 *   behind GNUCHANBROWSER_JS_LOG for the testing that needs it, and it is off
 *   otherwise.
 *
 *   A page that asks for a new window gets a TAB. Qt WebEngine refuses a
 *   window-opening request when the page does not override createWindow, so a
 *   target="_blank" link or a window.open() did nothing at all before. The
 *   override below hands the request back to the window, which opens a tab.
 */
#include "browser_window.h"

#include "url_utils.h"

#include <QAction>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUrl>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <QtGlobal>

#include <cstdio>

namespace {

/* Whether the page's console is echoed to stderr. Read once: the answer cannot
   change while the program runs, and asking the environment per message would
   be a small cost paid on the one path this exists to make cheap. */
bool js_log_enabled()
{
    static const bool enabled =
        qEnvironmentVariableIsSet("GNUCHANBROWSER_JS_LOG");
    return enabled;
}

}  // namespace

/* A page that repeats what the page itself logged, on stderr — but only when
   GNUCHANBROWSER_JS_LOG is set. See the note at the top of the file for why it
   is not on by default. No Q_OBJECT: only virtuals are overridden. */
class ReportingPage : public QWebEnginePage {
public:
    ReportingPage(QWebEngineProfile *profile, class BrowserWindow *browser,
                  QObject *parent)
        : QWebEnginePage(profile, parent), browser_(browser) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message, int line,
                                  const QString &source) override
    {
        Q_UNUSED(level);
        if (!js_log_enabled()) {
            return;
        }
        fprintf(stderr, "js: %s\n", qPrintable(message));
        fflush(stderr);
        Q_UNUSED(line);
        Q_UNUSED(source);
    }

    /* A page that wants a new window — target="_blank", window.open() — is
       given a tab of this window instead. The engine loads the requested
       content into the page returned here, so the link opens where a person
       expects rather than being dropped. */
    QWebEnginePage *createWindow(WebWindowType type) override
    {
        Q_UNUSED(type);
        return browser_ ? browser_->newTabPage() : nullptr;
    }

private:
    class BrowserWindow *browser_;
};

namespace {

/* A tab's label: the page title when it has one, the host otherwise, trimmed
   so a long title does not stretch the strip. */
QString tab_label(const QString &title, const QUrl &url)
{
    QString label = title.trimmed();
    if (label.isEmpty()) {
        label = url.host().isEmpty() ? QStringLiteral("New tab") : url.host();
    }
    if (label.length() > 22) {
        label = label.left(20) + QStringLiteral("…");
    }
    return label;
}

/* The zoom is kept inside this range. A page at 0.25 is a page of unreadable
   specks and one at 5.0 is a handful of giant words; neither is something a
   person reaches on purpose, and holding the ends is what stops a key held
   down from walking off them. */
constexpr double kMinZoom = 0.25;
constexpr double kMaxZoom = 5.0;

double clamp_zoom(double value)
{
    if (value < kMinZoom) {
        return kMinZoom;
    }
    if (value > kMaxZoom) {
        return kMaxZoom;
    }
    return value;
}

}  // namespace

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("GnuChanBrowser"));
    resize(1100, 720);

    /* One page for every tab, drawn with the engine's settings chosen for a
       processor with no GPU: WebGL off (there is nothing to run it on once the
       GPU process is disabled), and the page features that cost the processor
       and buy nothing here turned off. */
    tabs = new QTabWidget(this);
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    tabs->setDocumentMode(true);
    setCentralWidget(tabs);

    QToolBar *bar = addToolBar(QStringLiteral("Navigation"));
    bar->setMovable(false);

    QAction *back = bar->addAction(QStringLiteral("◀"));
    back->setToolTip(QStringLiteral("Back"));
    QAction *forward = bar->addAction(QStringLiteral("▶"));
    forward->setToolTip(QStringLiteral("Forward"));
    QAction *reload = bar->addAction(QStringLiteral("⟳"));
    reload->setToolTip(QStringLiteral("Reload"));
    QAction *home = bar->addAction(QStringLiteral("⌂"));
    home->setToolTip(QStringLiteral("Home"));

    address = new QLineEdit(this);
    address->setClearButtonEnabled(true);
    address->setPlaceholderText(
        QStringLiteral("Search DuckDuckGo or type an address"));
    bar->addWidget(address);

    QAction *star = bar->addAction(QStringLiteral("★"));
    star->setToolTip(QStringLiteral("Bookmark this page"));
    QAction *new_tab = bar->addAction(QStringLiteral("✚"));
    new_tab->setToolTip(QStringLiteral("New tab (Ctrl+T)"));

    /* The bookmark bar: one button per saved favourite, rebuilt whenever the
       list changes. */
    bookmark_bar = new QToolBar(QStringLiteral("Bookmarks"), this);
    bookmark_bar->setMovable(false);
    addToolBarBreak();
    addToolBar(bookmark_bar);

    bookmarks = load_bookmarks();
    rebuildBookmarkBar();

    /* Downloads land in the user's Downloads directory without a dialog. A
       browser that refuses a download — which is what the engine does when
       nothing answers this — is a browser where a link that is a file does
       nothing at all, which is worse than guessing at a directory the world
       already agrees on. */
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    const QString download_dir =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    connect(profile, &QWebEngineProfile::downloadRequested, this,
            [download_dir](QWebEngineDownloadRequest *download) {
                if (!download) {
                    return;
                }
                download->setDownloadDirectory(download_dir);
                download->accept();
                fprintf(stderr, "download: %s -> %s\n",
                        qPrintable(download->downloadFileName()),
                        qPrintable(download_dir));
                fflush(stderr);
            });

    /* The page leading and the toolbar following. Every connection is a lambda
       on a signal Qt already has, so no meta-object is generated. */
    connect(address, &QLineEdit::returnPressed, this,
            [this]() { navigateFromAddress(); });
    connect(back, &QAction::triggered, this, [this]() {
        if (QWebEngineView *view = currentView()) {
            view->back();
        }
    });
    connect(forward, &QAction::triggered, this, [this]() {
        if (QWebEngineView *view = currentView()) {
            view->forward();
        }
    });
    connect(reload, &QAction::triggered, this, [this]() {
        if (QWebEngineView *view = currentView()) {
            view->reload();
        }
    });
    connect(home, &QAction::triggered, this, [this]() {
        if (QWebEngineView *view = currentView()) {
            view->setUrl(home_url());
        }
    });
    connect(star, &QAction::triggered, this, [this]() { toggleBookmark(); });
    connect(new_tab, &QAction::triggered, this, [this]() {
        openTab(home_url());
        focusAddress();
    });

    connect(tabs, &QTabWidget::tabCloseRequested, this,
            [this](int index) { closeTab(index); });
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        QWebEngineView *view = currentView();
        if (view) {
            address->setText(view->url().toString());
            updateWindowTitle(view->title());
        }
    });

    /* The keyboard the window already answers to. These used to be missing,
       and on a browser that is a real absence: Ctrl+T, Ctrl+W and Ctrl+L are
       how a person works and reaching for the toolbar instead is slower. The
       window keeps its handful of keys and lets the PAGE have the rest — the
       application context is not taken, so a page's own Ctrl+F still reaches
       it. */
    auto shortcut = [this](const QKeySequence &key, void (BrowserWindow::*method)()) {
        QAction *action = new QAction(this);
        action->setShortcut(key);
        action->setShortcutContext(Qt::WindowShortcut);
        addAction(action);
        connect(action, &QAction::triggered, this, method);
    };
    shortcut(QKeySequence(QStringLiteral("Ctrl+T")), &BrowserWindow::openNewTab);
    shortcut(QKeySequence::Close, &BrowserWindow::closeCurrentTab);
    shortcut(QKeySequence(QStringLiteral("Ctrl+L")), &BrowserWindow::focusAddress);
    shortcut(QKeySequence::Reload, &BrowserWindow::reloadCurrent);
    shortcut(QKeySequence::ZoomIn, &BrowserWindow::zoomIn);
    shortcut(QKeySequence::ZoomOut, &BrowserWindow::zoomOut);
    shortcut(QKeySequence(QStringLiteral("Ctrl+0")), &BrowserWindow::zoomReset);
    shortcut(QKeySequence::Back, &BrowserWindow::goBack);
    shortcut(QKeySequence::Forward, &BrowserWindow::goForward);

    /* The first tab, and the home page: the browser opens on DuckDuckGo. */
    openTab(home_url());
}

QWebEngineView *BrowserWindow::addTab(bool switch_to_it)
{
    QWebEngineView *view = new QWebEngineView(this);
    ReportingPage *page =
        new ReportingPage(QWebEngineProfile::defaultProfile(), this, view);
    view->setPage(page);

    /* HTML5 fullscreen, ALLOWED.
     *
     * Qt WebEngine refuses a page's fullscreen request unless the program both
     * enables it and accepts it, and it is refused by DEFAULT — so a video told
     * to go fullscreen did nothing at all and the window stayed exactly as it
     * was. That matters beyond the picture: the session's screen saver decides
     * the screen is in use when a fullscreen window is present (see GnuChanSS),
     * so a fullscreen request that never lands is also a video the saver can be
     * dropped over. Accepting it makes the request real, which is what a person
     * pressing the video's fullscreen button expects anyway. */
    connect(page, &QWebEnginePage::fullScreenRequested, this,
            [](QWebEngineFullScreenRequest request) { request.accept(); });

    QWebEngineSettings *settings = view->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, false);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    /* window.open() and target="_blank" reach createWindow only when this is
       on; it is on by default, but saying so here keeps the behaviour from
       being an accident of a default that another line might change. */
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

    const int index = tabs->addTab(view, QStringLiteral("New tab"));
    if (switch_to_it) {
        tabs->setCurrentIndex(index);
    }

    /* The tab's own events: its label follows the title, the address bar
       follows it while it is the visible tab, and its loading shows in the
       status bar. */
    connect(view, &QWebEngineView::titleChanged, this,
            [this, view](const QString &title) {
                const int at = tabs->indexOf(view);
                if (at >= 0) {
                    tabs->setTabText(at, tab_label(title, view->url()));
                }
                if (view == currentView()) {
                    updateWindowTitle(title);
                }
            });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &u) {
        const int at = tabs->indexOf(view);
        if (at >= 0) {
            tabs->setTabText(at, tab_label(view->title(), u));
        }
        if (view == currentView()) {
            address->setText(u.toString());
        }
    });
    connect(view, &QWebEngineView::loadFinished, this,
            [this, view](bool ok) {
                if (view == currentView()) {
                    statusBar()->showMessage(ok ? QStringLiteral("Done")
                                                : QStringLiteral("Load failed"),
                                             3000);
                }
                /* One line to the process output per page: what loaded, and
                   whether it worked. It is how the browser is checked from a
                   script, and it is the honest place to see the address a
                   redirect finally landed on. */
                fprintf(stderr, "loaded %s: %s\n",
                        ok ? "ok" : "failed",
                        qPrintable(view->url().toString()));
                fflush(stderr);
            });

    return view;
}

QWebEnginePage *BrowserWindow::newTabPage()
{
    QWebEngineView *view = addTab(true);
    return view->page();
}

QWebEngineView *BrowserWindow::openTab(const QUrl &url, bool switch_to_it)
{
    QWebEngineView *view = addTab(switch_to_it);
    view->setUrl(url.isValid() && !url.isEmpty() ? url : home_url());
    return view;
}

QWebEngineView *BrowserWindow::currentView() const
{
    return qobject_cast<QWebEngineView *>(tabs->currentWidget());
}

void BrowserWindow::navigateFromAddress()
{
    QWebEngineView *view = currentView();
    if (!view) {
        view = addTab(true);
    }
    const QUrl url = url_from_input(address->text());
    if (url.isValid() && !url.isEmpty()) {
        view->setUrl(url);
    }
}

void BrowserWindow::updateWindowTitle(const QString &page_title)
{
    const QString base = QStringLiteral("GnuChanBrowser");
    setWindowTitle(page_title.trimmed().isEmpty()
                       ? base
                       : page_title.trimmed() + QStringLiteral(" — ") + base);
}

void BrowserWindow::closeTab(int index)
{
    QWidget *page = tabs->widget(index);
    tabs->removeTab(index);
    if (page) {
        /* deleteLater, not delete: the view may still be inside the signal
           that led here (its own tab-close signal), and freeing it now would
           free an object the stack still points at. */
        page->deleteLater();
    }
    /* Never leave the window empty: closing the last tab opens the home page,
       which is what a browser is expected to do rather than quitting. */
    if (tabs->count() == 0) {
        openTab(home_url());
    }
}

void BrowserWindow::closeCurrentTab()
{
    const int index = tabs->currentIndex();
    if (index >= 0) {
        closeTab(index);
    }
}

void BrowserWindow::focusAddress()
{
    address->setFocus();
    address->selectAll();
}

void BrowserWindow::adjustZoom(double factor)
{
    QWebEngineView *view = currentView();
    if (!view) {
        return;
    }
    view->setZoomFactor(clamp_zoom(view->zoomFactor() * factor));
}

void BrowserWindow::goBack()
{
    if (QWebEngineView *view = currentView()) {
        view->back();
    }
}

void BrowserWindow::goForward()
{
    if (QWebEngineView *view = currentView()) {
        view->forward();
    }
}

void BrowserWindow::reloadCurrent()
{
    if (QWebEngineView *view = currentView()) {
        view->reload();
    }
}

void BrowserWindow::openNewTab()
{
    openTab(home_url());
    focusAddress();
}

void BrowserWindow::zoomIn()
{
    adjustZoom(1.1);
}

void BrowserWindow::zoomOut()
{
    adjustZoom(1.0 / 1.1);
}

void BrowserWindow::zoomReset()
{
    if (QWebEngineView *view = currentView()) {
        view->setZoomFactor(1.0);
    }
}

void BrowserWindow::toggleBookmark()
{
    QWebEngineView *view = currentView();
    if (!view) {
        return;
    }
    const QUrl url = view->url();
    if (url.isEmpty() || url.toString() == QStringLiteral("about:blank")) {
        return;
    }
    const QString title = view->title().trimmed().isEmpty()
                              ? url.host()
                              : view->title().trimmed();

    /* Toggle: the star removes a page that is already saved. */
    for (int i = 0; i < bookmarks.size(); ++i) {
        if (bookmarks.at(i).url == url) {
            bookmarks.removeAt(i);
            save_bookmarks(bookmarks);
            rebuildBookmarkBar();
            return;
        }
    }
    bookmarks.append(Bookmark{title, url});
    save_bookmarks(bookmarks);
    rebuildBookmarkBar();
}

void BrowserWindow::rebuildBookmarkBar()
{
    bookmark_bar->clear();
    for (const Bookmark &bookmark : bookmarks) {
        QAction *entry = bookmark_bar->addAction(bookmark.title);
        const QUrl url = bookmark.url;
        entry->setToolTip(url.toString());
        connect(entry, &QAction::triggered, this, [this, url]() {
            QWebEngineView *view = currentView();
            if (view) {
                view->setUrl(url);
            } else {
                openTab(url);
            }
        });
    }
    bookmark_bar->setVisible(!bookmarks.isEmpty());
}
