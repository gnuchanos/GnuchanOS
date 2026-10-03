/*
 * fetch_info.h — the machine, read a piece at a time.
 *
 * A fetch program is a short list of facts about the machine it is run on, each
 * with a label in front of it. This is where the facts come from: the files
 * under /proc and /sys, the environment, and a few small commands whose output
 * is the answer. Every reader here returns a line of text, or an empty line when
 * the fact could not be found — a machine that does not report its own CPU is a
 * machine whose CPU line is left out, not one that stops printing.
 *
 * Nothing here draws anything. A "field" is a name — "os", "kernel", "memory" —
 * and this module turns a name into the line that goes beside it. A name it does
 * not know is reported as unknown rather than guessed at, because the list of
 * things a person may ask for is fixed and short, and a typo in the settings
 * script should look like a typo.
 */
#ifndef GNUCHANFETCH_INFO_H
#define GNUCHANFETCH_INFO_H

#include "fetch_config.h"

/* The fields that have a label and a reader. A name passed by the settings file
   that is not one of these is refused by fetch_field_from_name(), which answers
   FETCH_FIELD_UNKNOWN. The set is the same as the table in fetch_info.c. */
typedef enum FetchField {
    FETCH_FIELD_UNKNOWN = 0,
    FETCH_FIELD_USER,
    FETCH_FIELD_HOST,
    FETCH_FIELD_OS,
    FETCH_FIELD_KERNEL,
    FETCH_FIELD_UPTIME,
    FETCH_FIELD_PACKAGES,
    FETCH_FIELD_SHELL,
    FETCH_FIELD_WM,
    FETCH_FIELD_DM,
    FETCH_FIELD_THEME,
    FETCH_FIELD_ICONS,
    FETCH_FIELD_CURSOR,
    FETCH_FIELD_TERMINAL,
    FETCH_FIELD_MODEL,
    FETCH_FIELD_CPU,
    FETCH_FIELD_GPU,
    FETCH_FIELD_MEMORY,
    FETCH_FIELD_DISK,
} FetchField;

/* The field a name refers to, or FETCH_FIELD_UNKNOWN. */
FetchField fetch_field_from_name(const char *name);

/* The value of a field, written into `out` and including the label and the
   separator — "OS: Debian GNU/Linux 12". An unknown field, and a field whose
   value could not be read, both give an empty string, and the caller leaves the
   line out rather than printing a label with nothing after it. */
void fetch_field_line(FetchField field, char *out, unsigned int size);

/* The heading of the whole block: "user@host", which is drawn above the fields
   with the swatch under it. Empty when neither could be read. */
void fetch_title_line(char *out, unsigned int size);

#endif /* GNUCHANFETCH_INFO_H */
