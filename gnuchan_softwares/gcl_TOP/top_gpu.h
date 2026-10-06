/*
 * top_gpu.h — how busy the GPU is, when the driver says.
 *
 * There is no one file for this. The kernel's DRM drivers each expose what they
 * choose: amdgpu has /sys/class/drm/cardN/device/gpu_busy_percent, which is a
 * plain percentage; the Intel i915 driver has no such file, and its busy-ness is
 * only visible as per-engine millisecond counters under the card's gt
 * directory, in an engine subdirectory's `busy` file, which have to be
 * differenced between two reads the same way the CPU's /proc/stat counters do.
 *
 * So this module tries the simple file first and falls back to the counters.
 * When neither exists — no GPU, a driver that reports nothing, a machine whose
 * card is not card0 — it says so and the top bar draws the CPU side only. A
 * monitor that refused to run without a GPU reading would be useless on the
 * machines that have none.
 */
#ifndef GNUCHANTOP_GPU_H
#define GNUCHANTOP_GPU_H

#include "top_common.h"

typedef struct {
    int    present;      /* 1 when a reading is possible on this machine */
    int    has_reading;  /* 1 once two samples have made a percentage */
    double usage;        /* 0..100 */
    char   name[TOP_NAME]; /* the driver, for the panel's label */
} TopGpu;

/* Sample the GPU. Returns 0 when a reading can be taken (which does not mean
   the first call has a percentage yet), -1 when this machine has no GPU
   reading. `gpu->present` says which. */
int top_gpu_sample(TopGpu *gpu);

#endif /* GNUCHANTOP_GPU_H */
