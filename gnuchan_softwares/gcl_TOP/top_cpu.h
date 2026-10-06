/*
 * top_cpu.h — how busy each core is, and the machine as a whole.
 *
 * The numbers come from /proc/stat, which reports a running total of the time
 * each core has spent in every state since boot. Busy-ness is therefore not a
 * value the kernel keeps: it is the DIFFERENCE between two reads, which is why
 * a caller must sample twice before the first number means anything.
 *
 * Two reads one update apart, and the fraction of that interval spent not idle
 * is the percentage drawn. A core that was not in either sample — a machine
 * that hot-added one — reads as zero rather than as a jump.
 */
#ifndef GNUCHANTOP_CPU_H
#define GNUCHANTOP_CPU_H

#include "top_common.h"

typedef struct {
    int    core_count;                 /* cores the last read saw */
    double usage[TOP_MAX_CORES];       /* 0..100, busy share of the interval */
    double overall;                    /* 0..100 across every core together */
} TopCpu;

/* Read /proc/stat. The first call has nothing to compare with, so the usage
   stays as it was — zero on a fresh struct — until the next call. Returns 0,
   or -1 when /proc/stat could not be read. */
int top_cpu_sample(TopCpu *cpu);

#endif /* GNUCHANTOP_CPU_H */
