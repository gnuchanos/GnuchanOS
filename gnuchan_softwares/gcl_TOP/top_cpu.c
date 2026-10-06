/*
 * top_cpu.c — reading /proc/stat and turning two reads into a busy percentage.
 *
 * The first line of /proc/stat is the machine's own total and each `cpuN` line
 * is a core. Every line is a list of running totals in USER_HZ ticks: user,
 * nice, system, idle, iowait, irq, softirq, steal. What is wanted is how much
 * of the time between two reads was NOT idle, so both reads are kept — the
 * previous totals are in this file's static state, the current ones are parsed
 * into a local — and the difference is the answer.
 *
 * Busy, for this purpose, is everything except idle and iowait: a core waiting
 * on a disk is not doing work, and counting it as busy would make a machine
 * copying a large file look like a machine compiling one.
 */
#include "top_cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The counters a `cpu` line carries, reduced to the two sums this needs. */
typedef struct {
    unsigned long long total; /* every field summed */
    unsigned long long idle;  /* idle + iowait */
} CpuTimes;

/* The last read, so the next one has something to subtract. Static because it
   is the whole of this module's memory and nothing else reads it. */
static CpuTimes previous[TOP_MAX_CORES + 1];
static int       previous_seen = 0;  /* cores in `previous`, 0 before the first */

/* Sum the fields of one `cpu` line. The line has already been matched as a CPU
   line. `total` is every field, `idle` is the two that mean not-busy. */
static CpuTimes parse_cpu_line(const char *line) {
    CpuTimes times;
    times.total = 0;
    times.idle = 0;

    /* Step past the "cpu"/"cpuN" word to the numbers. */
    const char *cursor = line;
    while (*cursor && *cursor != ' ') {
        cursor++;
    }

    unsigned long long fields[10];
    int count = 0;
    while (count < 10) {
        char *end = NULL;
        unsigned long long value = strtoull(cursor, &end, 10);
        if (end == cursor) {
            break;
        }
        fields[count++] = value;
        cursor = end;
    }

    for (int i = 0; i < count; i++) {
        times.total += fields[i];
    }
    /* The order is fixed by the kernel: idle is field 3, iowait field 4
       (counting from zero), so both are subtracted from busy. */
    if (count > 3) {
        times.idle += fields[3];
    }
    if (count > 4) {
        times.idle += fields[4];
    }
    return times;
}

/* The busy share of the interval between `before` and `after`, 0..100. A total
   that did not move is reported as 0, never as a divide by zero. */
static double busy_percent(CpuTimes before, CpuTimes after) {
    if (after.total <= before.total || after.idle < before.idle) {
        /* A counter that did not move, or went backwards — a wrap, or a read of
           a removed core — is not a percentage; the interval cannot be trusted,
           so it reads as 0. */
        return 0.0;
    }
    unsigned long long total_delta = after.total - before.total;
    unsigned long long idle_delta = after.idle - before.idle;
    if (idle_delta > total_delta) {
        return 0.0;
    }
    double busy = (double)(total_delta - idle_delta);
    double percent = (busy * 100.0) / (double)total_delta;
    if (percent < 0.0) {
        percent = 0.0;
    }
    if (percent > 100.0) {
        percent = 100.0;
    }
    return percent;
}

int top_cpu_sample(TopCpu *cpu) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        return -1;
    }

    CpuTimes current[TOP_MAX_CORES + 1];
    int seen = 0;          /* 0 is the "cpu" total, 1.. are cores */
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "cpu", 3) != 0) {
            continue;
        }
        /* "cpu " is the total; "cpuN" is a core. */
        int is_total = (line[3] == ' ' || line[3] == '\t' || line[3] == '\n');
        if (!is_total && seen >= TOP_MAX_CORES) {
            continue; /* more cores than we draw; the total still counts */
        }
        if (is_total) {
            if (seen > 0) {
                break; /* the total line came after a core: unexpected, stop */
            }
            current[0] = parse_cpu_line(line);
            seen = 1;
        } else {
            current[seen] = parse_cpu_line(line);
            seen++;
        }
    }
    fclose(file);

    if (seen == 0) {
        return -1;
    }

    cpu->core_count = seen - 1;
    if (cpu->core_count > TOP_MAX_CORES) {
        cpu->core_count = TOP_MAX_CORES;
    }

    if (previous_seen > 0) {
        for (int i = 0; i < cpu->core_count; i++) {
            if (i + 1 < previous_seen) {
                cpu->usage[i] = busy_percent(previous[i + 1], current[i + 1]);
            } else {
                cpu->usage[i] = 0.0;
            }
        }
        cpu->overall = busy_percent(previous[0], current[0]);
    } else {
        for (int i = 0; i < cpu->core_count; i++) {
            cpu->usage[i] = 0.0;
        }
        cpu->overall = 0.0;
    }

    /* Keep this read for the next call, up to what we draw. */
    int keep = seen > (TOP_MAX_CORES + 1) ? (TOP_MAX_CORES + 1) : seen;
    memcpy(previous, current, sizeof(CpuTimes) * (size_t)keep);
    previous_seen = keep;
    return 0;
}
