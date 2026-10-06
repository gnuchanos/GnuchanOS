/*
 * top_config.h — the settings GnuChanTop reads from its own config file.
 *
 * The settings live in a small Python-shaped file, ~/.config/GnuChanTop/
 * GnuChanTop.py, and every one of them has a default, so the program runs
 * whether or not that file is there — the same rule the other Gnuchan programs
 * follow. TopConfig is the whole of what that file can say; nothing else in the
 * program reads it.
 */
#ifndef GNUCHANTOP_CONFIG_H
#define GNUCHANTOP_CONFIG_H

#include "top_common.h"

/* The column the process list is ordered by. */
typedef enum {
    TOP_SORT_CPU = 0,
    TOP_SORT_MEM,
    TOP_SORT_PID,
    TOP_SORT_NAME
} TopSortKey;

typedef struct {
    double    update_time;   /* seconds between samples */
    int       proc_limit;    /* most processes listed */
    int       show_gpu;      /* draw the GPU half of the top bar */
    TopSortKey sort;         /* the column the list is ordered by */
    char      error[TOP_TEXT]; /* why the file could not be read, if it could not */
} TopConfig;

/* The defaults, laid down before any file is read. */
void top_config_defaults(TopConfig *config);

/* Read the settings file over the defaults.
   Returns 0 when a file was read, -1 when none was found and the defaults
   stand. A bad value on one line leaves that setting at its default and the
   rest of the file is still read. */
int top_config_load(TopConfig *config);

/* The name of a sort key, and the key a name stands for. The names are the ones
   the config file and the on-screen footer both use: cpu, mem, pid, name. */
const char *top_sort_name(TopSortKey key);
int top_sort_from_name(const char *name, TopSortKey *out);

#endif /* GNUCHANTOP_CONFIG_H */
