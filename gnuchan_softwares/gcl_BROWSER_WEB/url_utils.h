/*
 * url_utils.h — what a line typed into the address bar means.
 *
 * One function, kept out of the window so the rule lives in one place and can
 * be read on its own: given whatever a person typed, what gets loaded?
 */
#ifndef GNUCHAN_BROWSER_URL_UTILS_H
#define GNUCHAN_BROWSER_URL_UTILS_H

#include <QString>
#include <QUrl>

/* The page a fresh tab opens, and the engine used for a line that is a search
   rather than an address. Keeping it here means the home page and the search
   fallback cannot drift apart. */
QUrl home_url();

/* Turn a line from the address bar into something loadable:
 *
 *   "https://a.com/x"   a scheme is already there — load it as written
 *   "about:blank"       ditto; about:, file:, data:, chrome: count as schemes
 *   "example.com"       no scheme — http:// goes in front. A typed address is
 *                       a plain host until it says otherwise, and the server
 *                       may upgrade it to https itself.
 *   "192.168.1.1:8080"  a host with a port — also http://
 *   "some words"        not an address — searched on DuckDuckGo
 */
QUrl url_from_input(const QString &text);

#endif /* GNUCHAN_BROWSER_URL_UTILS_H */
