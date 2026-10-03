/*
 * bookmarks.cpp — reading and writing the bookmark file.
 */
#include "bookmarks.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

/* The program owns one directory under ~/.config, the same shape the other
   GnuchanOS programs use. */
QString config_dir()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/GnuChanBrowser");
}

/* The separator between a title and a url on one line. A tab, because a title
   may contain almost anything but a tab, and it keeps the file readable in a
   terminal `cat` as two columns. */
const QChar kFieldSeparator = QLatin1Char('\t');

}  // namespace

QString bookmarks_file_path()
{
    return config_dir() + QStringLiteral("/bookmarks.txt");
}

QList<Bookmark> load_bookmarks()
{
    QList<Bookmark> bookmarks;
    QFile file(bookmarks_file_path());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return bookmarks;
    }
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int tab = line.indexOf(kFieldSeparator);
        if (tab < 0) {
            continue;
        }
        const QString title = line.left(tab).trimmed();
        const QString urlText = line.mid(tab + 1).trimmed();
        const QUrl url(urlText);
        if (url.isValid() && !url.isEmpty()) {
            bookmarks.append(
                Bookmark{title.isEmpty() ? urlText : title, url});
        }
    }
    file.close();
    return bookmarks;
}

bool save_bookmarks(const QList<Bookmark> &bookmarks)
{
    const QString directory = config_dir();
    if (!QDir().mkpath(directory)) {
        return false;
    }
    QFile file(bookmarks_file_path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text |
                   QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&file);
    for (const Bookmark &bookmark : bookmarks) {
        stream << bookmark.title << kFieldSeparator
               << bookmark.url.toString() << '\n';
    }
    file.close();
    return true;
}
