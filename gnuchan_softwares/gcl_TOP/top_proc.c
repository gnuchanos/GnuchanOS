/*
 * top_proc.c — reading the process list out of /proc.
 *
 * The work of one sample is a walk of /proc, and for every numeric directory
 * three files are read: `stat` for the state, the command and the CPU time,
 * `status` for the resident memory, and `cmdline` for the full command when the
 * process has one. Nothing is read that is not drawn.
 *
 * The CPU share is a difference between two samples, the same idea the CPU
 * module uses: a process's utime+stime grew by some ticks, the machine's total
 * grew by some ticks, and the ratio — scaled by the core count so one busy core
 * reads as 100% — is the percentage. A process with no previous sample has no
 * ratio yet and is drawn as 0 until the next sample.
 *
 * The cursor is carried across samples by pid, so the row under the arrow keys
 * does not change identity when the sort moves it. That is what makes `k` act on
 * the process the person was looking at and not on whatever landed there.
 */
#include "top_proc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- the list -------------------------------------------------------------- */

void top_procs_init(TopProcs *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->cursor = 0;
    list->scroll = 0;
    list->total_jiffies = 0;
    list->num_cores = 1;
}

void top_procs_free(TopProcs *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Make room for one more row, growing by doubling. Returns the row to fill, or
   NULL when memory ran out, in which case the sample is cut short rather than
   crashing. */
static TopProc *push_row(TopProcs *list) {
    if (list->count >= list->capacity) {
        int next = list->capacity == 0 ? 512 : list->capacity * 2;
        TopProc *grown = realloc(list->items, sizeof(TopProc) * (size_t)next);
        if (!grown) {
            return NULL;
        }
        list->items = grown;
        list->capacity = next;
    }
    TopProc *row = &list->items[list->count];
    memset(row, 0, sizeof(*row));
    /* The row is claimed here, not by the caller: the caller fills it and has
       no reason to know that the list has grown, and a caller that forgot to
       advance the count would write every process over the first row and then
       report an empty list. */
    list->count++;
    return row;
}

/* --- the uid -> name map --------------------------------------------------- */

/* A cache of uid to login name, so a process list of a few hundred rows does not
   ask the password database a few hundred times per second. */
typedef struct {
    int  uid;
    char name[TOP_USER];
} UserEntry;

static UserEntry *users = NULL;
static int        user_count = 0;
static int        user_capacity = 0;

static const char *user_name(int uid) {
    for (int i = 0; i < user_count; i++) {
        if (users[i].uid == uid) {
            return users[i].name;
        }
    }
    if (user_count >= user_capacity) {
        int next = user_capacity == 0 ? 32 : user_capacity * 2;
        UserEntry *grown = realloc(users, sizeof(UserEntry) * (size_t)next);
        if (!grown) {
            return "?";
        }
        users = grown;
        user_capacity = next;
    }
    struct passwd *entry = getpwuid((uid_t)uid);
    UserEntry *slot = &users[user_count++];
    slot->uid = uid;
    if (entry && entry->pw_name) {
        snprintf(slot->name, sizeof(slot->name), "%s", entry->pw_name);
    } else {
        snprintf(slot->name, sizeof(slot->name), "%d", uid);
    }
    return slot->name;
}

/* --- one /proc/<pid>/stat -------------------------------------------------- */

/* Parse a `stat` line. The second field is the command, in parentheses, and it
   can itself contain spaces and parentheses, so everything after it is found
   from the LAST `)`. After the state letter the numbers are, in order and
   counting from zero: 0 ppid, 1 pgrp, 2 session, 3 tty_nr, 4 tpgid, 5 flags,
   6 minflt, 7 cminflt, 8 majflt, 9 cmajflt, 10 utime, 11 stime. Returns 0 on
   success. */
static int parse_stat(const char *buffer, char *comm, size_t comm_size,
                      char *state, unsigned long long *jiffies) {
    const char *open = strchr(buffer, '(');
    const char *close = strrchr(buffer, ')');
    if (!open || !close || close < open) {
        return -1;
    }
    size_t name_length = (size_t)(close - open - 1);
    if (name_length >= comm_size) {
        name_length = comm_size - 1;
    }
    memcpy(comm, open + 1, name_length);
    comm[name_length] = '\0';

    /* Step past the `)` to the state letter. */
    const char *cursor = close + 1;
    while (*cursor == ' ') {
        cursor++;
    }
    if (*cursor == '\0') {
        return -1;
    }
    *state = *cursor;
    cursor++;

    unsigned long long fields[20];
    int count = 0;
    while (count < 20) {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0' || *cursor == '\n') {
            break;
        }
        char *end = NULL;
        unsigned long long value = strtoull(cursor, &end, 10);
        if (end == cursor) {
            break;
        }
        fields[count++] = value;
        cursor = end;
    }

    *jiffies = 0;
    if (count > 10) {
        *jiffies += fields[10]; /* utime */
    }
    if (count > 11) {
        *jiffies += fields[11]; /* stime */
    }
    return 0;
}

/* --- one /proc/<pid>/status ------------------------------------------------ */

/* The resident set size in kilobytes, or 0 when it was not in the file. */
static unsigned long long rss_from_status(const char *contents) {
    const char *line = contents;
    while (*line) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            return strtoull(line + 6, NULL, 10);
        }
        const char *newline = strchr(line, '\n');
        if (!newline) {
            break;
        }
        line = newline + 1;
    }
    return 0;
}

/* Read a whole small file into a buffer, returning the length or -1. */
static long read_file(const char *path, char *out, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    size_t got = fread(out, 1, size - 1, file);
    fclose(file);
    out[got] = '\0';
    return (long)got;
}

/* --- the sample ------------------------------------------------------------ */

/* The machine's physical memory, for the per-process percentage. */
static unsigned long long physical_total_kb = 0;

static void ensure_physical_total(void) {
    if (physical_total_kb != 0) {
        return;
    }
    char line[256];
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        physical_total_kb = 1;
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            physical_total_kb = strtoull(line + 9, NULL, 10);
            break;
        }
    }
    fclose(file);
    if (physical_total_kb == 0) {
        physical_total_kb = 1;
    }
}

int top_procs_sample(TopProcs *list, long long total_jiffies, int num_cores,
                     const TopConfig *config) {
    (void)config;
    ensure_physical_total();

    long long total_delta = total_jiffies - list->total_jiffies;
    if (list->total_jiffies == 0 || total_delta <= 0) {
        total_delta = 0;
    }

    /* Remember what the cursor was on, so it can be found again afterwards. */
    int cursor_pid = -1;
    if (list->cursor >= 0 && list->cursor < list->count) {
        cursor_pid = list->items[list->cursor].pid;
    }

    DIR *proc = opendir("/proc");
    if (!proc) {
        return -1;
    }

    /* The previous rows are searched by pid for the previous jiffies. */
    TopProc *old_copy = NULL;
    int old_count = list->count;
    if (old_count > 0) {
        old_copy = malloc(sizeof(TopProc) * (size_t)old_count);
        if (old_copy) {
            memcpy(old_copy, list->items, sizeof(TopProc) * (size_t)old_count);
        }
    }

    list->count = 0;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0])) {
            continue;
        }
        int pid = atoi(entry->d_name);
        if (pid <= 0) {
            continue;
        }

        char path[TOP_TEXT];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        char stat_buffer[1024];
        if (read_file(path, stat_buffer, sizeof(stat_buffer)) < 0) {
            continue;
        }

        char comm[TOP_NAME];
        char state = '?';
        unsigned long long jiffies = 0;
        if (parse_stat(stat_buffer, comm, sizeof(comm), &state, &jiffies) != 0) {
            continue;
        }

        TopProc *row = push_row(list);
        if (!row) {
            break;
        }
        row->pid = pid;
        row->state = state;
        row->last_cpu_jiffies = jiffies;

        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        char cmdline[TOP_TEXT];
        long length = read_file(path, cmdline, sizeof(cmdline));
        if (length > 0) {
            for (long i = 0; i < length - 1; i++) {
                if (cmdline[i] == '\0') {
                    cmdline[i] = ' ';
                }
            }
            snprintf(row->name, sizeof(row->name), "%s", cmdline);
        } else {
            snprintf(row->name, sizeof(row->name), "[%s]", comm);
        }

        snprintf(path, sizeof(path), "/proc/%d", pid);
        struct stat st;
        int uid = 0;
        if (stat(path, &st) == 0) {
            uid = (int)st.st_uid;
        }
        row->uid = uid;
        snprintf(row->user, sizeof(row->user), "%s", user_name(uid));

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        char status[8192];
        unsigned long long rss_kb = 0;
        if (read_file(path, status, sizeof(status)) > 0) {
            rss_kb = rss_from_status(status);
        }
        row->rss_kb = rss_kb;
        row->mem = ((double)rss_kb * 100.0) / (double)physical_total_kb;

        /* The CPU share, from the process's previous jiffies and the machine's
           previous total. No previous sample means no share yet. */
        row->cpu = 0.0;
        row->is_new = 1;
        for (int i = 0; i < old_count; i++) {
            if (old_copy[i].pid == pid) {
                if (total_delta > 0 && jiffies >= old_copy[i].last_cpu_jiffies) {
                    unsigned long long proc_delta =
                        jiffies - old_copy[i].last_cpu_jiffies;
                    double share =
                        ((double)proc_delta * 100.0 * (double)num_cores) /
                        (double)total_delta;
                    if (share < 0.0)   share = 0.0;
                    if (share > 999.9) share = 999.9;
                    row->cpu = share;
                }
                row->is_new = 0;
                break;
            }
        }
    }
    closedir(proc);

    free(old_copy);
    list->total_jiffies = total_jiffies;
    list->num_cores = num_cores;

    if (cursor_pid >= 0) {
        for (int i = 0; i < list->count; i++) {
            if (list->items[i].pid == cursor_pid) {
                list->cursor = i;
                break;
            }
        }
    }
    if (list->cursor >= list->count) {
        list->cursor = list->count > 0 ? list->count - 1 : 0;
    }
    if (list->cursor < 0) {
        list->cursor = 0;
    }
    return list->count;
}

/* --- ordering -------------------------------------------------------------- */

static int compare_cpu(const void *a, const void *b) {
    const TopProc *left = a;
    const TopProc *right = b;
    if (left->cpu < right->cpu) return 1;
    if (left->cpu > right->cpu) return -1;
    return left->pid - right->pid;
}

static int compare_mem(const void *a, const void *b) {
    const TopProc *left = a;
    const TopProc *right = b;
    if (left->rss_kb < right->rss_kb) return 1;
    if (left->rss_kb > right->rss_kb) return -1;
    return left->pid - right->pid;
}

static int compare_pid(const void *a, const void *b) {
    const TopProc *left = a;
    const TopProc *right = b;
    return left->pid - right->pid;
}

static int compare_name(const void *a, const void *b) {
    const TopProc *left = a;
    const TopProc *right = b;
    int order = strcmp(left->name, right->name);
    if (order != 0) {
        return order;
    }
    return left->pid - right->pid;
}

void top_procs_sort(TopProcs *list, TopSortKey key) {
    if (list->count < 2) {
        return;
    }
    int cursor_pid = -1;
    if (list->cursor >= 0 && list->cursor < list->count) {
        cursor_pid = list->items[list->cursor].pid;
    }

    int (*compare)(const void *, const void *) = compare_cpu;
    if (key == TOP_SORT_MEM) {
        compare = compare_mem;
    } else if (key == TOP_SORT_PID) {
        compare = compare_pid;
    } else if (key == TOP_SORT_NAME) {
        compare = compare_name;
    }
    qsort(list->items, (size_t)list->count, sizeof(TopProc), compare);

    if (cursor_pid >= 0) {
        for (int i = 0; i < list->count; i++) {
            if (list->items[i].pid == cursor_pid) {
                list->cursor = i;
                break;
            }
        }
    }
}

/* --- the cursor ------------------------------------------------------------ */

void top_procs_move(TopProcs *list, int delta, int visible_rows) {
    if (list->count == 0) {
        list->cursor = 0;
        list->scroll = 0;
        return;
    }
    list->cursor += delta;
    if (list->cursor < 0) {
        list->cursor = 0;
    }
    if (list->cursor >= list->count) {
        list->cursor = list->count - 1;
    }

    if (visible_rows < 1) {
        visible_rows = 1;
    }
    if (list->cursor < list->scroll) {
        list->scroll = list->cursor;
    }
    if (list->cursor >= list->scroll + visible_rows) {
        list->scroll = list->cursor - visible_rows + 1;
    }
    int max_scroll = list->count - visible_rows;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    if (list->scroll > max_scroll) {
        list->scroll = max_scroll;
    }
    if (list->scroll < 0) {
        list->scroll = 0;
    }
}

TopProc *top_procs_selected(TopProcs *list) {
    if (list->count == 0 || list->cursor < 0 || list->cursor >= list->count) {
        return NULL;
    }
    return &list->items[list->cursor];
}

/* --- signalling ------------------------------------------------------------ */

int top_procs_signal(int pid, int signal_number, char *error, size_t error_size) {
    if (kill(pid, signal_number) != 0) {
        snprintf(error, error_size, "kill(%d, %d): %s", pid, signal_number,
                 strerror(errno));
        return -1;
    }
    return 0;
}
