/*
 * top_mem.h — physical and swap memory, as /proc/meminfo reports it.
 *
 * Everything is kilobytes, which is the unit meminfo uses; the drawing code
 * turns them into the GiB a person reads. `used` is total minus available and
 * not total minus free: available is the kernel's own estimate of what a new
 * program could get without swapping, which is the number a monitor should
 * show, where free counts the cache as gone and makes a healthy machine look
 * full.
 */
#ifndef GNUCHANTOP_MEM_H
#define GNUCHANTOP_MEM_H

typedef struct {
    unsigned long long total_kb;
    unsigned long long available_kb;
    unsigned long long used_kb;
    unsigned long long swap_total_kb;
    unsigned long long swap_used_kb;
} TopMem;

/* Read /proc/meminfo. Returns 0, or -1 when the file could not be read; a
   struct that could not be read is left zeroed. */
int top_mem_sample(TopMem *mem);

/* The used share of a total, 0..100, and 0 when the total is 0. */
double top_mem_percent(unsigned long long used_kb, unsigned long long total_kb);

#endif /* GNUCHANTOP_MEM_H */
