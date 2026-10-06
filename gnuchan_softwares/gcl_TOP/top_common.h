/*
 * top_common.h — the sizes every GnuChanTop module shares.
 *
 * One place for the constants, because they are referenced across modules: the
 * process list owns a process named TOP_TEXT long, the drawing code lays out a
 * history that TOP_HISTORY long, and a mismatch between the two would be a
 * buffer written past its end. A single definition is the only way to keep them
 * in step.
 *
 * A fixed ceiling rather than growth: this is a monitor, it samples on a timer,
 * and an allocation per frame is work the timer does not need. The numbers are
 * chosen larger than a desktop is likely to need — 4096 processes, 64 cores,
 * 256 samples of history — and a machine past them is reported truncated rather
 * than crashing.
 */
#ifndef GNUCHANTOP_COMMON_H
#define GNUCHANTOP_COMMON_H

/* A line of text: a command name, a device path, an error message. */
#define TOP_TEXT 256

/* A process or thread name, which the kernel caps at 15 characters plus a NUL. */
#define TOP_NAME 64

/* A login name. */
#define TOP_USER 32

/* How many samples of history the CPU and GPU graphs keep. At the default one
   second per sample this is a little over four minutes of the past. */
#define TOP_HISTORY 256

/* The most CPU cores this draws a per-core view for. Machines with more still
   run; the extra cores are left out of the per-core rows and every overall
   number is unaffected. */
#define TOP_MAX_CORES 64

/* The most processes this reads in one sample. */
#define TOP_MAX_PROCS 4096

#endif /* GNUCHANTOP_COMMON_H */
