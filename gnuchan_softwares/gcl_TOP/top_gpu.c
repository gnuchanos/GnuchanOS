/*
 * top_gpu.c — reading the GPU's busy-ness from whichever driver is there.
 *
 * The two paths this tries, in order:
 *
 *   1. amdgpu. /sys/class/drm/cardN/device/gpu_busy_percent is a single integer
 *      0..100, already the answer. The card's vendor file beside it says the
 *      driver is 0x1002/AMD, but the file's presence is the real test and the
 *      vendor is only used to label the panel.
 *
 *   2. i915, newer parts. Per-engine millisecond counters under
 *      /sys/class/drm/cardN/gt/gt0/engines/<engine>/busy. The busiest engine's
 *      growth over the interval, against the wall-clock milliseconds that
 *      passed, is the percentage. The engine list differs between chips, so the
 *      files are discovered by reading the directory rather than by name.
 *
 *   3. i915, older parts. The driver publishes a perf PMU whose rcs0-busy event
 *      counts the render engine's busy nanoseconds; the growth of that counter
 *      over the wall-clock interval is the percentage. This is the only reading
 *      a GMA-era chip offers, and it needs the CAP_PERFMON the installer grants.
 *
 * A reading that cannot be taken leaves `present` at 0, and the caller draws the
 * panel with `n/a`; that is not an error, it is a machine with no readable GPU.
 */
#include "top_gpu.h"

#include <dirent.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/* The card this reads, which is the first one. */
#define GPU_CARD_DIR "/sys/class/drm/card0"

/* The most engine counters the i915 path tracks. */
#define GPU_MAX_ENGINES 16

/* --- the simple path ------------------------------------------------------- */

/* Read a single whole number from a file. Returns 0 and sets *out when it read
   one, -1 otherwise. */
static int read_number(const char *path, long *out) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    long value = -1;
    int matched = fscanf(file, "%ld", &value);
    fclose(file);
    if (matched != 1) {
        return -1;
    }
    *out = value;
    return 0;
}

/* The driver's name, for the panel's label. */
static void read_driver_name(char *out, size_t size) {
    snprintf(out, size, "GPU");
    long vendor = 0;
    if (read_number(GPU_CARD_DIR "/device/vendor", &vendor) != 0) {
        return;
    }
    if (vendor == 0x1002) {
        snprintf(out, size, "AMD GPU");
    } else if (vendor == 0x8086) {
        snprintf(out, size, "Intel GPU");
    } else if (vendor == 0x10de) {
        snprintf(out, size, "NVIDIA GPU");
    } else {
        snprintf(out, size, "GPU %#lx", vendor);
    }
}

/* --- the i915 path --------------------------------------------------------- */

typedef struct {
    char path[TOP_TEXT * 3];  /* room for the card, tile, engine and `busy` */
    long last_busy;           /* milliseconds reported by the last read */
    int  seen;                /* 1 once a first read has been taken */
} EngineCounter;

static EngineCounter engines[GPU_MAX_ENGINES];
static int engine_count = 0;

/* Discover the engine busy files under the card's gt directory. */
static void discover_engines(void) {
    engine_count = 0;
    DIR *gt = opendir(GPU_CARD_DIR "/gt");
    if (!gt) {
        return;
    }
    struct dirent *tile;
    while ((tile = readdir(gt)) != NULL) {
        if (strncmp(tile->d_name, "gt", 2) != 0) {
            continue;
        }
        char engine_dir[TOP_TEXT];
        /* The tile name is short (`gt0`); the precision keeps a surprise name
           from being flagged as a possible overflow. */
        snprintf(engine_dir, sizeof(engine_dir), "%s/gt/%.16s/engines",
                 GPU_CARD_DIR, tile->d_name);
        DIR *engines_opened = opendir(engine_dir);
        if (!engines_opened) {
            continue;
        }
        struct dirent *entry;
        while ((entry = readdir(engines_opened)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }
            if (engine_count >= GPU_MAX_ENGINES) {
                break;
            }
            char busy_path[TOP_TEXT * 3];
            snprintf(busy_path, sizeof(busy_path), "%s/%s/busy", engine_dir,
                     entry->d_name);
            FILE *probe = fopen(busy_path, "r");
            if (!probe) {
                continue;
            }
            fclose(probe);
            snprintf(engines[engine_count].path,
                     sizeof(engines[engine_count].path), "%s", busy_path);
            engines[engine_count].last_busy = 0;
            engines[engine_count].seen = 0;
            engine_count++;
        }
        closedir(engines_opened);
    }
    closedir(gt);
}

/* The wall-clock time in milliseconds, from CLOCK_MONOTONIC so a clock change
   does not corrupt the interval. */
static long long now_millis(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (long long)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

static long long previous_gpu_millis = 0;

/* The i915 reading: for each engine, how much its counter grew over the interval
   as a share of the wall-clock interval. The busiest engine is the answer. */
static int sample_i915(TopGpu *gpu) {
    if (engine_count == 0) {
        discover_engines();
        if (engine_count == 0) {
            return -1;
        }
    }

    long long now = now_millis();
    long long elapsed = now - previous_gpu_millis;
    if (previous_gpu_millis == 0 || elapsed <= 0) {
        elapsed = 0;
    }

    double busiest = 0.0;
    for (int i = 0; i < engine_count; i++) {
        long busy = 0;
        if (read_number(engines[i].path, &busy) != 0) {
            continue;
        }
        if (engines[i].seen && elapsed > 0 && busy >= engines[i].last_busy) {
            double percent = ((double)(busy - engines[i].last_busy) * 100.0) /
                             (double)elapsed;
            if (percent > busiest) {
                busiest = percent;
            }
        }
        engines[i].last_busy = busy;
        engines[i].seen = 1;
    }
    previous_gpu_millis = now;

    if (busiest > 100.0) {
        busiest = 100.0;
    }
    gpu->usage = busiest;

    for (int i = 0; i < engine_count; i++) {
        if (engines[i].seen) {
            gpu->has_reading = 1;
            break;
        }
    }
    return 0;
}

/* --- the i915 perf path ---------------------------------------------------- */

/* The perf PMU the i915 driver publishes, and the render engine's busy event in
   it. The type number the PMU answers to is read from the PMU's own `type` file
   rather than assumed, because the kernel assigns it per boot. */
#define I915_PMU_DIR   "/sys/bus/event_source/devices/i915"
#define I915_BUSY_EVENT 0x0    /* rcs0-busy, busy nanoseconds of the render engine */

static int       perf_fd = -1;
static long long perf_last_busy_ns = 0;
static long long perf_last_wall_ns = 0;
static int       perf_seen = 0;

static long long now_nanos(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (long long)time.tv_sec * 1000000000LL + time.tv_nsec;
}

/* Open the i915 PMU's busy event once. Returns 0 when the event is open, -1
   when this machine has no such PMU or the kernel refused the open — the last
   is the CAP_PERFMON case, which the installer fixes with setcap. */
static int open_perf(void) {
    long type = 0;
    if (read_number(I915_PMU_DIR "/type", &type) != 0) {
        return -1;
    }
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = (unsigned int)type;
    attr.size = sizeof(attr);
    attr.config = I915_BUSY_EVENT;
    attr.disabled = 1;
    long fd = syscall(__NR_perf_event_open, &attr, -1, 0, -1, 0);
    if (fd < 0) {
        return -1;
    }
    ioctl((int)fd, PERF_EVENT_IOC_RESET, 0);
    ioctl((int)fd, PERF_EVENT_IOC_ENABLE, 0);
    perf_fd = (int)fd;
    return 0;
}

/* The perf reading: the busy nanoseconds the render engine gained over the
   wall-clock nanoseconds that passed. The first call has nothing to subtract
   from and so leaves `has_reading` unset, exactly like the CPU's first sample. */
static int sample_perf(TopGpu *gpu) {
    if (perf_fd < 0 && open_perf() != 0) {
        return -1;
    }

    long long busy = 0;
    if (read(perf_fd, &busy, sizeof(busy)) != (ssize_t)sizeof(busy)) {
        return -1;
    }
    long long wall = now_nanos();

    if (perf_seen && wall > perf_last_wall_ns && busy >= perf_last_busy_ns) {
        long long busy_delta = busy - perf_last_busy_ns;
        long long wall_delta = wall - perf_last_wall_ns;
        double percent = (double)busy_delta * 100.0 / (double)wall_delta;
        if (percent < 0.0)   percent = 0.0;
        if (percent > 100.0) percent = 100.0;
        gpu->usage = percent;
        gpu->has_reading = 1;
    }
    perf_last_busy_ns = busy;
    perf_last_wall_ns = wall;
    perf_seen = 1;
    return 0;
}

/* --- the entry point ------------------------------------------------------- */

int top_gpu_sample(TopGpu *gpu) {
    /* A first call picks which reading this machine offers, and sets the name
       with it; the steady-state block below then uses that same path. */
    if (!gpu->present && gpu->name[0] == '\0') {
        long percent = 0;
        if (read_number(GPU_CARD_DIR "/device/gpu_busy_percent", &percent) == 0) {
            read_driver_name(gpu->name, sizeof(gpu->name));
            gpu->present = 1;
        } else if (sample_i915(gpu) == 0 && engine_count > 0) {
            snprintf(gpu->name, sizeof(gpu->name), "Intel GPU");
            gpu->present = 1;
        } else if (sample_perf(gpu) == 0) {
            snprintf(gpu->name, sizeof(gpu->name), "Intel GPU");
            gpu->present = 1;
        } else {
            gpu->present = 0;
            gpu->has_reading = 0;
            return -1;
        }
    }

    if (!gpu->present) {
        return -1;
    }

    /* amdgpu's own percentage, re-read every sample. */
    long percent = 0;
    if (read_number(GPU_CARD_DIR "/device/gpu_busy_percent", &percent) == 0) {
        if (percent < 0)   percent = 0;
        if (percent > 100) percent = 100;
        gpu->usage = (double)percent;
        gpu->has_reading = 1;
        return 0;
    }

    /* The newer i915 engine counters, then the older i915 perf event. */
    if (engine_count > 0 && sample_i915(gpu) == 0) {
        return 0;
    }
    if (sample_perf(gpu) == 0) {
        return 0;
    }
    gpu->has_reading = 0;
    return -1;
}
