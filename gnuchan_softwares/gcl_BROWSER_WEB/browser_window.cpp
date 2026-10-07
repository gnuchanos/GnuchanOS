/*
 * browser_window.cpp — the interface: tabs, toolbar, bookmark bar.
 */
#include "browser_window.h"

#include "url_utils.h"

#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUrl>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <cstdio>

namespace {

/* A page that repeats what the page itself logged, on stderr. Qt WebEngine
   drops a page's console by default, so printing it is how this browser is
   tested from a script and how a page's own errors are seen. No Q_OBJECT: only
   a virtual is overridden. */
class ReportingPage : public QWebEnginePage {
public:
    explicit ReportingPage(QWebEngineProfile *profile, QObject *parent)
        : QWebEnginePage(profile, parent) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message, int line,
                                  const QString &source) override
    {
        Q_UNUSED(level);
        Q_UNUSED(line);
        Q_UNUSED(source);
        fprintf(stderr, "js: %s\n", qPrintable(message));
        fflush(stderr);
    }
};

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
    QAction *forward = bar->addAction(QStringLiteral("▶"));
    QAction *reload = bar->addAction(QStringLiteral("⟳"));
    QAction *home = bar->addAction(QStringLiteral("⌂"));

    address = new QLineEdit(this);
    address->setClearButtonEnabled(true);
    address->setPlaceholderText(
        QStringLiteral("Search DuckDuckGo or type an address"));
    bar->addWidget(address);

    QAction *star = bar->addAction(QStringLiteral("★"));
    star->setToolTip(QStringLiteral("Bookmark this page"));
    QAction *new_tab = bar->addAction(QStringLiteral("✚"));
    new_tab->setToolTip(QStringLiteral("New tab"));

    /* The bookmark bar: one button per saved favourite, rebuilt whenever the
       list changes. */
    bookmark_bar = new QToolBar(QStringLiteral("Bookmarks"), this);
    bookmark_bar->setMovable(false);
    addToolBarBreak();
    addToolBar(bookmark_bar);

    bookmarks = load_bookmarks();
    rebuildBookmarkBar();

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
    connect(new_tab, &QAction::triggered, this,
            [this]() { openTab(home_url()); });

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

    /* The first tab, and the home page: the browser opens on DuckDuckGo. */
    openTab(home_url());
}

QWebEngineView *BrowserWindow::openTab(const QUrl &url, bool switch_to_it)
{
    QWebEngineView *view = new QWebEngineView(this);
    ReportingPage *page =
        new ReportingPage(QWebEngineProfile::defaultProfile(), view);
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
        view = openTab(url_from_input(address->text()));
        return;
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
        page->deleteLater();
    }
    /* Never leave the window empty: closing the last tab opens the home page,
       which is what a browser is expected to do rather than quitting. */
    if (tabs->count() == 0) {
        openTab(home_url());
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
