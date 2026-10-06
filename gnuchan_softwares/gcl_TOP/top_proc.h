/*
 * top_proc.h — the process list.
 *
 * One sample is the whole list, read in a single pass over /proc: each numeric
 * directory's `stat` for the state, the CPU time and the command, and `status`
 * for the memory. A process's CPU share is again a difference — its own jiffies
 * between two samples against the machine's total jiffies between the same two —
 * which is why the list holds a value carried over from the previous sample and
 * why the caller samples on the same timer the CPU does.
 *
 * The list is kept sorted the way the config asks, and the cursor is an index
 * into it: the drawing code moves the cursor by number, and the actions — kill —
 * are taken on whatever the cursor is on. The list is rebuilt every sample but
 * the cursor is carried across by pid, so a process does not jump out from under
 * the key that was about to be pressed.
 */
#ifndef GNUCHANTOP_PROC_H
#define GNUCHANTOP_PROC_H

#include <stddef.h>

#include "top_common.h"
#include "top_config.h"

typedef struct {
    int   pid;
    int   uid;
    char  user[TOP_USER];
    char  name[TOP_TEXT];      /* the command line, or the comm when there is none */
    char  state;               /* the single-letter state from stat */
    double cpu;                /* 0..100 * cores, the share of the interval */
    double mem;                /* 0..100 of physical memory */
    unsigned long long rss_kb;
    unsigned long long last_cpu_jiffies; /* so the next sample can difference it */
    int   is_new;              /* no previous sample; its cpu is not drawn yet */
} TopProc;

typedef struct {
    TopProc *items;
    int      count;
    int      capacity;
    int      cursor;           /* the selected row */
    int      scroll;           /* the first row drawn */
    long long total_jiffies;   /* the machine's total at this sample */
    int      num_cores;
} TopProcs;

/* A process list that owns its rows. */
void top_procs_init(TopProcs *list);
void top_procs_free(TopProcs *list);

/* Read every process. `total_jiffies` is the machine total at this sample, from
   the same /proc/stat read the CPU module made; it is what a process's jiffies
   are measured against. Returns the number of processes read, or -1 when /proc
   could not be listed. */
int top_procs_sample(TopProcs *list, long long total_jiffies, int num_cores,
                     const TopConfig *config);

/* Order the list by the config's key. Called after every sample; the cursor
   follows its process by pid. */
void top_procs_sort(TopProcs *list, TopSortKey key);

/* Move the cursor, keeping it inside the list and the list scrolled to show it.
   `visible_rows` is how many fit on screen, which the drawing knows and this
   does not. */
void top_procs_move(TopProcs *list, int delta, int visible_rows);

/* The process under the cursor, or NULL when the list is empty. */
TopProc *top_procs_selected(TopProcs *list);

/* Send `signal` to a process, returning 0 on success and -1 with `error` set on
   failure. `error` is a caller buffer of at least TOP_TEXT. */
int top_procs_signal(int pid, int signal, char *error, size_t error_size);

#endif /* GNUCHANTOP_PROC_H */
