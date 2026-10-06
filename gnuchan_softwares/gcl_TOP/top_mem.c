/*
 * top_mem.c — reading /proc/meminfo.
 *
 * meminfo is a list of `Name: value kB` lines. Only a few matter here, and each
 * is matched by its whole name with the colon, so `MemTotal` never matches
 * `MemTotalWhatever`. A value that could not be parsed leaves its field at zero
 * rather than at a number that would be drawn as real.
 */
#include "top_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The value on a line whose name is `key`, or 0 when the line is not there. The
   key is compared with the trailing colon, so the name has to match whole. */
static unsigned long long value_of(const char *line, const char *key) {
    size_t key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0) {
        return 0;
    }
    const char *cursor = line + key_length;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    return strtoull(cursor, NULL, 10);
}

int top_mem_sample(TopMem *mem) {
    memset(mem, 0, sizeof(*mem));

    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            mem->total_kb = value_of(line, "MemTotal:");
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            mem->available_kb = value_of(line, "MemAvailable:");
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            /* Kept only as a fallback for a kernel with no MemAvailable,
               which is the pre-3.14 case. */
            if (mem->available_kb == 0) {
                mem->available_kb = value_of(line, "MemFree:");
            }
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            mem->swap_total_kb = value_of(line, "SwapTotal:");
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            unsigned long long swap_free = value_of(line, "SwapFree:");
            if (mem->swap_total_kb >= swap_free) {
                mem->swap_used_kb = mem->swap_total_kb - swap_free;
            }
        }
    }
    fclose(file);

    if (mem->total_kb >= mem->available_kb) {
        mem->used_kb = mem->total_kb - mem->available_kb;
    } else {
        mem->used_kb = 0;
    }
    return 0;
}

double top_mem_percent(unsigned long long used_kb, unsigned long long total_kb) {
    if (total_kb == 0) {
        return 0.0;
    }
    double percent = ((double)used_kb * 100.0) / (double)total_kb;
    if (percent < 0.0) {
        percent = 0.0;
    }
    if (percent > 100.0) {
        percent = 100.0;
    }
    return percent;
}
