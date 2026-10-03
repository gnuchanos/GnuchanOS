/*
 * bookmarks.h — the favourites, on disk.
 *
 * A small store of (title, url) pairs, kept as one line per bookmark in a plain
 * text file under the user's config directory. Plain text because a bookmark is
 * not a database: it is two strings a person can read, edit, or back up with
 * anything. The directory follows the other GnuchanOS programs — one folder per
 * program under ~/.config.
 */
#ifndef GNUCHAN_BROWSER_BOOKMARKS_H
#define GNUCHAN_BROWSER_BOOKMARKS_H

#include <QList>
#include <QString>
#include <QUrl>

struct Bookmark {
    QString title;
    QUrl url;
};

/* Where the bookmark file lives: ~/.config/GnuChanBrowser/bookmarks.txt,
   honouring XDG_CONFIG_HOME when it is set. */
QString bookmarks_file_path();

/* Read the file. A missing file is an empty list, not an error: a first run
   has no bookmarks and that is not a failure. Lines that are not a bookmark
   are skipped rather than rejected, so one bad line does not lose the rest. */
QList<Bookmark> load_bookmarks();

/* Write the list back, replacing the file. Returns false if it could not be
   written, which the caller may choose to report. */
bool save_bookmarks(const QList<Bookmark> &bookmarks);

#endif /* GNUCHAN_BROWSER_BOOKMARKS_H */
