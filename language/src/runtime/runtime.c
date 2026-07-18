#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

/* ================================================================= */
/*  Global runtime monitor                                            */
/* ================================================================= */

static RuntimeMonitor g_monitor = {0};

RuntimeMonitor *runtime_get_monitor(void) {
    return &g_monitor;
}

/* ================================================================= */
/*  Error name & suggested fix tables                                 */
/* ================================================================= */

static const char *error_names[] = {
    [GCL_ERR_NONE]             = "No Error",
    [GCL_ERR_NULL_POINTER]     = "NULL Pointer Dereference",
    [GCL_ERR_BOUNDS]           = "Array Index Out of Bounds",
    [GCL_ERR_USE_AFTER_FREE]   = "Use After Free",
    [GCL_ERR_BUFFER_OVERFLOW]  = "Buffer Overflow",
    [GCL_ERR_INTEGER_OVERFLOW] = "Integer Overflow",
    [GCL_ERR_DIV_BY_ZERO]      = "Division by Zero",
    [GCL_ERR_INVALID_CAST]     = "Invalid Type Cast",
    [GCL_ERR_MEMORY_LEAK]      = "Memory Leak",
    [GCL_ERR_DOUBLE_FREE]      = "Double Free",
    [GCL_ERR_STACK_OVERFLOW]   = "Stack Overflow",
    [GCL_ERR_SEGFAULT]         = "Segmentation Fault",
    [GCL_ERR_HEAP_CORRUPTION]  = "Heap Corruption",
    [GCL_ERR_INVALID_FREE]     = "Invalid Free",
};

static const char *suggested_fixes[] = {
    [GCL_ERR_NULL_POINTER]     = "Check if pointer is NULL before dereferencing. Initialize pointers to a valid address.",
    [GCL_ERR_BOUNDS]           = "Verify array index is within [0, size-1]. Use array.size or pass size as parameter.",
    [GCL_ERR_USE_AFTER_FREE]   = "Set pointer to NULL after free(). Do not access memory after freeing.",
    [GCL_ERR_BUFFER_OVERFLOW]  = "Ensure buffer size is sufficient. Use bounds-checked functions.",
    [GCL_ERR_INTEGER_OVERFLOW] = "Use wider integer type or check values before arithmetic operations.",
    [GCL_ERR_DIV_BY_ZERO]      = "Check divisor is not zero before division or modulo operation.",
    [GCL_ERR_INVALID_CAST]     = "Ensure types are compatible before casting. Use safe cast patterns.",
    [GCL_ERR_MEMORY_LEAK]      = "Ensure every malloc() has a matching free(). Use RAII patterns.",
    [GCL_ERR_DOUBLE_FREE]      = "Set pointer to NULL after free. Track allocation state.",
    [GCL_ERR_STACK_OVERFLOW]   = "Reduce recursion depth or increase stack size. Use iteration instead.",
    [GCL_ERR_SEGFAULT]         = "Check pointer validity before access. Initialize all pointers.",
    [GCL_ERR_HEAP_CORRUPTION]  = "Check for buffer overflows and double frees. Use memory sanitizers.",
    [GCL_ERR_INVALID_FREE]     = "Only free memory returned by malloc/calloc/realloc.",
};

const char *runtime_error_name(GclErrorCode code) {
    if (code >= 0 && (size_t)code < sizeof(error_names)/sizeof(error_names[0]))
        return error_names[code];
    return "Unknown Error";
}

const char *runtime_suggested_fix(GclErrorCode code) {
    if (code >= 0 && (size_t)code < sizeof(suggested_fixes)/sizeof(suggested_fixes[0]))
        return suggested_fixes[code];
    return "Review the code around the error location.";
}

/* ================================================================= */
/*  Init / Cleanup                                                    */
/* ================================================================= */

void runtime_init(int debug) {
    memset(&g_monitor, 0, sizeof(g_monitor));
    g_monitor.debug_on = debug;

    if (debug) {
        g_monitor.ptr_tracker.capacity = 64;
        g_monitor.ptr_tracker.entries = (AllocEntry*)calloc(
            g_monitor.ptr_tracker.capacity, sizeof(AllocEntry));
        g_monitor.ptr_tracker.count = 0;

        g_monitor.metadata_cap = 256;
        g_monitor.metadata_table = (InsnMetadata*)calloc(
            g_monitor.metadata_cap, sizeof(InsnMetadata));
        g_monitor.metadata_count = 0;

        g_monitor.exceptions.capacity = 16;
        g_monitor.exceptions.reports = (CrashReport*)calloc(
            g_monitor.exceptions.capacity, sizeof(CrashReport));
        g_monitor.exceptions.count = 0;

        g_monitor.stack_tracker.top = NULL;
        g_monitor.stack_tracker.depth = 0;
        g_monitor.stack_tracker.max_depth = 0;

        fprintf(stderr, "[RUNTIME] Debug mode initialized\n");
    }
}

void runtime_cleanup(void) {
    if (!g_monitor.debug_on) return;

    /* Leak check */
    if (g_monitor.ptr_tracker.count > 0) {
        int leaked = 0;
        for (int i = 0; i < g_monitor.ptr_tracker.count; i++) {
            if (!g_monitor.ptr_tracker.entries[i].freed) {
                AllocEntry *e = &g_monitor.ptr_tracker.entries[i];
                fprintf(stderr, "[LEAK] %p %zu bytes at %s:%d\n",
                        e->ptr, e->size, e->file, e->line);
                leaked++;
            }
        }
        if (leaked == 0)
            fprintf(stderr, "[RUNTIME] No memory leaks ✓\n");
        else
            fprintf(stderr, "[RUNTIME] %d memory leaks detected\n", leaked);
    }

    /* Cleanup allocated memory */
    free(g_monitor.ptr_tracker.entries);
    free(g_monitor.metadata_table);
    free(g_monitor.exceptions.reports);

    /* Clear stack frames */
    StackFrame *f = g_monitor.stack_tracker.top;
    while (f) {
        StackFrame *next = f->next;
        free(f);
        f = next;
    }

    memset(&g_monitor, 0, sizeof(g_monitor));
}

/* ================================================================= */
/*  Instruction ID Metadata                                           */
/* ================================================================= */

void runtime_set_insn_metadata(int insn_id, const char *file, int line,
                                int col, const char *func) {
    if (!g_monitor.debug_on) return;
    if (g_monitor.metadata_count >= g_monitor.metadata_cap) {
        g_monitor.metadata_cap *= 2;
        g_monitor.metadata_table = (InsnMetadata*)realloc(
            g_monitor.metadata_table,
            g_monitor.metadata_cap * sizeof(InsnMetadata));
    }
    InsnMetadata *m = &g_monitor.metadata_table[g_monitor.metadata_count++];
    m->insn_id = insn_id;
    m->src_file = file;
    m->src_line = line;
    m->src_col = col;
    m->func_name = func;
}

const InsnMetadata *runtime_get_insn_metadata(int insn_id) {
    if (!g_monitor.debug_on) return NULL;
    for (int i = 0; i < g_monitor.metadata_count; i++) {
        if (g_monitor.metadata_table[i].insn_id == insn_id)
            return &g_monitor.metadata_table[i];
    }
    return NULL;
}

/* ================================================================= */
/*  Allocation Tracking                                               */
/* ================================================================= */

void runtime_track_alloc(void *ptr, size_t size, const char *file, int line) {
    runtime_track_alloc_id(ptr, size, file, line, 0);
}

void runtime_track_alloc_id(void *ptr, size_t size, const char *file,
                             int line, int insn_id) {
    if (!g_monitor.debug_on || !ptr) return;

    if (g_monitor.ptr_tracker.count >= g_monitor.ptr_tracker.capacity) {
        g_monitor.ptr_tracker.capacity *= 2;
        g_monitor.ptr_tracker.entries = (AllocEntry*)realloc(
            g_monitor.ptr_tracker.entries,
            g_monitor.ptr_tracker.capacity * sizeof(AllocEntry));
    }

    AllocEntry *e = &g_monitor.ptr_tracker.entries[g_monitor.ptr_tracker.count++];
    e->ptr = ptr;
    e->size = size;
    e->file = file;
    e->line = line;
    e->insn_id = insn_id;
    e->freed = 0;
    e->owner_file = NULL;
    e->owner_line = 0;

    g_monitor.ptr_tracker.total_allocated += size;
    int active = 0;
    for (int i = 0; i < g_monitor.ptr_tracker.count; i++) {
        if (!g_monitor.ptr_tracker.entries[i].freed) active++;
    }
    if (active > g_monitor.ptr_tracker.peak_count)
        g_monitor.ptr_tracker.peak_count = active;

    g_monitor.mem_tracker.heap_used += size;
    if (g_monitor.mem_tracker.heap_used > g_monitor.mem_tracker.heap_peak)
        g_monitor.mem_tracker.heap_peak = g_monitor.mem_tracker.heap_used;
    g_monitor.mem_tracker.alloc_count++;

    if (g_monitor.debug_on)
        fprintf(stderr, "[ALLOC] %p %zu bytes at %s:%d\n", ptr, size, file, line);
}

void runtime_track_free(void *ptr, const char *file, int line) {
    if (!g_monitor.debug_on || !ptr) return;

    for (int i = 0; i < g_monitor.ptr_tracker.count; i++) {
        AllocEntry *e = &g_monitor.ptr_tracker.entries[i];
        if (e->ptr == ptr) {
            if (e->freed) {
                runtime_error(GCL_ERR_DOUBLE_FREE,
                              "Attempted to free already freed pointer", 0);
                return;
            }
            e->freed = 1;
            g_monitor.ptr_tracker.total_freed += e->size;
            g_monitor.mem_tracker.heap_used -= e->size;
            g_monitor.mem_tracker.free_count++;
            if (g_monitor.debug_on)
                fprintf(stderr, "[FREE]  %p at %s:%d (was %zu bytes at %s:%d)\n",
                        ptr, file, line, e->size, e->file, e->line);
            return;
        }
    }
    runtime_error(GCL_ERR_INVALID_FREE,
                  "Attempted to free untracked pointer", 0);
}

/* ================================================================= */
/*  Stack Tracking                                                    */
/* ================================================================= */

void runtime_push_frame(const char *func, const char *file, int line) {
    if (!g_monitor.debug_on) return;
    StackFrame *f = (StackFrame*)calloc(1, sizeof(StackFrame));
    if (!f) return;
    f->func_name = func;
    f->src_file = file;
    f->src_line = line;
    f->frame_id = g_monitor.stack_tracker.depth;
    f->prev = g_monitor.stack_tracker.top;
    f->next = NULL;

    if (g_monitor.stack_tracker.top)
        g_monitor.stack_tracker.top->next = f;

    g_monitor.stack_tracker.top = f;
    g_monitor.stack_tracker.depth++;

    if (g_monitor.stack_tracker.depth > g_monitor.stack_tracker.max_depth)
        g_monitor.stack_tracker.max_depth = g_monitor.stack_tracker.depth;

    /* Stack overflow check */
    if (g_monitor.stack_tracker.depth > 10000) {
        runtime_fatal(GCL_ERR_STACK_OVERFLOW,
                      "Stack depth exceeded 10000 frames", 0);
    }
}

void runtime_pop_frame(void) {
    if (!g_monitor.debug_on || !g_monitor.stack_tracker.top) return;
    StackFrame *f = g_monitor.stack_tracker.top;
    g_monitor.stack_tracker.top = f->prev;
    if (g_monitor.stack_tracker.top)
        g_monitor.stack_tracker.top->next = NULL;
    g_monitor.stack_tracker.depth--;
    free(f);
}

/* ================================================================= */
/*  Bounds / Null / Overflow Checking                                 */
/* ================================================================= */

void runtime_check_bounds(void *ptr, size_t offset, size_t size,
                           int insn_id) {
    (void)size;
    if (!g_monitor.debug_on) return;

    /* Find allocation entry */
    for (int i = 0; i < g_monitor.ptr_tracker.count; i++) {
        AllocEntry *e = &g_monitor.ptr_tracker.entries[i];
        if (e->ptr == ptr) {
            if (e->freed) {
                runtime_error(GCL_ERR_USE_AFTER_FREE,
                              "Accessing freed memory", insn_id);
                return;
            }
            if (offset >= e->size) {
                CrashReport *r = runtime_create_report(
                    GCL_ERR_BOUNDS, ptr, offset, e->size, insn_id);
                r->allocation_size = e->size;
                r->allocation_address = e->ptr;
                r->allocation_file = e->file;
                r->allocation_line = e->line;
                snprintf(r->suggested_fix, sizeof(r->suggested_fix),
                         "Attempted offset %zu but allocation is only %zu bytes. "
                         "Valid range: [0, %zu]",
                         offset, e->size, e->size - 1);
                runtime_fatal(GCL_ERR_BOUNDS,
                              "Array index out of bounds", insn_id);
            }
            return;
        }
    }
    /* Pointer not tracked - check for NULL */
    runtime_check_null(ptr, insn_id);
}

void runtime_check_null(void *ptr, int insn_id) {
    if (!g_monitor.debug_on) return;
    if (ptr == NULL) {
        runtime_fatal(GCL_ERR_NULL_POINTER,
                      "NULL pointer dereference", insn_id);
    }
}

void runtime_check_overflow(int64_t result, int64_t a, int64_t b,
                             int op, int insn_id) {
    if (!g_monitor.debug_on) return;

    int overflowed = 0;
    switch (op) {
    case '+': overflowed = (b > 0 && a > INT64_MAX - b) ||
                           (b < 0 && a < INT64_MIN - b); break;
    case '-': overflowed = (b > 0 && a < INT64_MIN + b) ||
                           (b < 0 && a > INT64_MAX + b); break;
    case '*': overflowed = (a != 0 && (result / a != b)); break;
    }

    if (overflowed) {
        runtime_error(GCL_ERR_INTEGER_OVERFLOW,
                      "Integer overflow detected", insn_id);
    }
}

/* ================================================================= */
/*  Crash Report Creation                                             */
/* ================================================================= */

CrashReport *runtime_create_report(GclErrorCode code, void *addr,
                                    size_t offset, size_t size,
                                    int insn_id) {
    if (g_monitor.exceptions.count >= g_monitor.exceptions.capacity) {
        g_monitor.exceptions.capacity *= 2;
        g_monitor.exceptions.reports = (CrashReport*)realloc(
            g_monitor.exceptions.reports,
            g_monitor.exceptions.capacity * sizeof(CrashReport));
    }

    CrashReport *r = &g_monitor.exceptions.reports[g_monitor.exceptions.count++];
    memset(r, 0, sizeof(CrashReport));
    r->error_code = code;
    r->error_name = runtime_error_name(code);
    r->fault_address = addr;
    r->attempted_offset = offset;
    r->allocation_size = size;

    /* Fill from metadata if available */
    const InsnMetadata *meta = runtime_get_insn_metadata(insn_id);
    if (meta) {
        r->src_file = meta->src_file;
        r->src_line = meta->src_line;
        r->func_name = meta->func_name;
    }

    /* Copy call stack */
    r->call_stack = g_monitor.stack_tracker.top;
    r->stack_depth = g_monitor.stack_tracker.depth;

    /* Suggested fix */
    snprintf(r->suggested_fix, sizeof(r->suggested_fix), "%s",
             runtime_suggested_fix(code));

    return r;
}

/* ================================================================= */
/*  Print / Save Crash Report                                         */
/* ================================================================= */

void runtime_print_report(CrashReport *r, FILE *out) {
    fprintf(out, "\n");
    fprintf(out, "╔═══════════════════════════════════════════════════╗\n");
    fprintf(out, "║           GCL CRASH REPORT                        ║\n");
    fprintf(out, "╚═══════════════════════════════════════════════════╝\n\n");

    fprintf(out, "  Error:        %s\n", r->error_name ? r->error_name : "Unknown");
    fprintf(out, "  Error Code:   %d\n", r->error_code);

    if (r->src_file)
        fprintf(out, "  Source File:  %s\n", r->src_file);
    if (r->src_line > 0)
        fprintf(out, "  Line Number:  %d\n", r->src_line);
    if (r->func_name)
        fprintf(out, "  Function:     %s\n", r->func_name);

    fprintf(out, "\n  ── Memory Information ──\n");
    if (r->fault_address)
        fprintf(out, "  Fault Address:  %p\n", r->fault_address);
    if (r->attempted_offset > 0)
        fprintf(out, "  Attempted Offset: %zu bytes\n", r->attempted_offset);
    if (r->allocation_size > 0)
        fprintf(out, "  Allocation Size:  %zu bytes\n", r->allocation_size);
    if (r->allocation_address)
        fprintf(out, "  Allocation Addr:  %p\n", r->allocation_address);
    if (r->allocation_file)
        fprintf(out, "  Allocated At:     %s:%d\n",
                r->allocation_file, r->allocation_line);

    fprintf(out, "\n  ── Call Stack ──\n");
    if (r->call_stack) {
        StackFrame *fs = r->call_stack;
        StackFrame *frames[256];
        int nf = 0;
        while (fs && nf < 256) { frames[nf++] = fs; fs = fs->prev; }
        for (int i = nf - 1; i >= 0; i--) {
            fprintf(out, "    #%d  %s  (%s:%d)\n",
                    nf - 1 - i,
                    frames[i]->func_name ? frames[i]->func_name : "?",
                    frames[i]->src_file ? frames[i]->src_file : "?",
                    frames[i]->src_line);
        }
    } else {
        fprintf(out, "    (no call stack)\n");
    }

    fprintf(out, "\n  ── Suggested Fix ──\n");
    fprintf(out, "    %s\n", r->suggested_fix);

    fprintf(out, "\n╔═══════════════════════════════════════════════════╗\n");
    fprintf(out, "║           END CRASH REPORT                        ║\n");
    fprintf(out, "╚═══════════════════════════════════════════════════╝\n\n");
}

void runtime_save_report(CrashReport *r, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[RUNTIME] Cannot write crash report to %s\n", path);
        return;
    }
    runtime_print_report(r, fp);
    fclose(fp);
    fprintf(stderr, "[RUNTIME] Crash report saved to %s\n", path);
}

/* ================================================================= */
/*  Error Handlers                                                    */
/* ================================================================= */

void runtime_error(GclErrorCode code, const char *msg, int insn_id) {
    if (!g_monitor.debug_on) return;
    const InsnMetadata *meta = runtime_get_insn_metadata(insn_id);
    fprintf(stderr, "[RUNTIME ERROR] ");
    if (meta) {
        fprintf(stderr, "%s:%d (%s): ", meta->src_file, meta->src_line,
                meta->func_name ? meta->func_name : "?");
    }
    fprintf(stderr, "%s — %s\n", runtime_error_name(code), msg);
}

void runtime_fatal(GclErrorCode code, const char *msg, int insn_id) {
    if (!g_monitor.debug_on) return;

    /* Create and print crash report */
    CrashReport *r = runtime_create_report(code, NULL, 0, 0, insn_id);
    runtime_print_report(r, stderr);

    /* Optional: save to file */
    runtime_save_report(r, "gcl_crash_report.txt");

    fprintf(stderr, "[RUNTIME FATAL] %s\n", msg);
    exit(EXIT_FAILURE);
}

/* ================================================================= */
/*  Signal handlers (for segfault etc.)                               */
/* ================================================================= */

static void sigsegv_handler(int sig) {
    (void)sig;
    runtime_fatal(GCL_ERR_SEGFAULT,
                  "Segmentation fault - invalid memory access", 0);
}

static void sigabrt_handler(int sig) {
    (void)sig;
    runtime_fatal(GCL_ERR_HEAP_CORRUPTION,
                  "Heap corruption detected by runtime", 0);
}

void runtime_install_signal_handlers(void) {
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGABRT, sigabrt_handler);
}
