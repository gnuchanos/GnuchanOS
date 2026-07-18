#ifndef GCL_RUNTIME_H
#define GCL_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ================================================================= */
/*  GCL Runtime — Debug Mode Monitors & Crash Analyzer                */
/* ================================================================= */

/* ------ Error Type Codes ------ */
typedef enum {
    GCL_ERR_NONE              = 0,
    GCL_ERR_NULL_POINTER      = 1,
    GCL_ERR_BOUNDS            = 2,  /* Array index out of bounds */
    GCL_ERR_USE_AFTER_FREE    = 3,
    GCL_ERR_BUFFER_OVERFLOW   = 4,  /* Stack buffer overflow */
    GCL_ERR_INTEGER_OVERFLOW  = 5,
    GCL_ERR_DIV_BY_ZERO       = 6,
    GCL_ERR_INVALID_CAST      = 7,
    GCL_ERR_MEMORY_LEAK       = 8,
    GCL_ERR_DOUBLE_FREE       = 9,
    GCL_ERR_STACK_OVERFLOW    = 10,
    GCL_ERR_SEGFAULT          = 11, /* Segmentation fault */
    GCL_ERR_HEAP_CORRUPTION   = 12,
    GCL_ERR_INVALID_FREE      = 13,
} GclErrorCode;

/* ------ Metadata: Instruction ID → Source Location ------ */
typedef struct {
    int         insn_id;      /* Instruction ID */
    const char *src_file;
    int         src_line;
    int         src_col;
    const char *func_name;    /* ait oldugu fonksiyon */
} InsnMetadata;

/* ------ Allocation Entry (Pointer Tracker) ------ */
typedef struct {
    void       *ptr;
    size_t      size;
    const char *file;         /* allocation yapilan dosya */
    int         line;         /* allocation yapilan satir */
    int         insn_id;      /* allocation instruction ID */
    int         freed;        /* 0=active, 1=freed */
    const char *owner_file;   /* sahibi olan dosya */
    int         owner_line;   /* sahibi olan satir */
} AllocEntry;

/* ------ Stack Frame (Stack Tracker) ------ */
typedef struct StackFrame {
    const char *func_name;
    const char *src_file;
    int         src_line;
    int         frame_id;
    struct StackFrame *prev;
    struct StackFrame *next;
} StackFrame;

/* ------ Crash Report ------ */
typedef struct {
    GclErrorCode  error_code;
    const char   *error_name;       /* "NULL Pointer", "Buffer Overflow", ... */
    const char   *src_file;
    int           src_line;
    const char   *func_name;
    /* Call stack */
    StackFrame   *call_stack;
    int           stack_depth;
    /* Memory info */
    void         *fault_address;
    size_t        allocation_size;
    size_t        attempted_offset;
    void         *allocation_address;
    const char   *allocation_file;
    int           allocation_line;
    /* Register / CPU info (placeholder for future) */
    uint64_t      regs[16];         /* genel amaçlı register dump */
    /* Suggested fix */
    char          suggested_fix[256];
} CrashReport;

/* ------ Pointer Tracker (heap) ------ */
typedef struct {
    AllocEntry *entries;
    int         count;
    int         capacity;
    size_t      total_allocated;   /* toplam allocate edilen byte */
    size_t      total_freed;
    int         peak_count;        /* peak active allocation count */
} PointerTracker;

/* ------ Memory Tracker (heap summary) ------ */
typedef struct {
    size_t heap_used;
    size_t heap_peak;
    size_t heap_total;
    int    alloc_count;
    int    free_count;
} MemoryTracker;

/* ------ Stack Tracker ------ */
typedef struct {
    StackFrame *top;
    int         depth;
    int         max_depth;
} StackTracker;

/* ------ Exception Collector ------ */
typedef struct {
    CrashReport *reports;
    int          count;
    int          capacity;
} ExceptionCollector;

/* ------ Runtime Monitor (ana yapi) ------ */
typedef struct {
    int                debug_on;
    PointerTracker     ptr_tracker;
    MemoryTracker      mem_tracker;
    StackTracker       stack_tracker;
    ExceptionCollector exceptions;
    InsnMetadata      *metadata_table;   /* Instruction ID → source info */
    int                metadata_count;
    int                metadata_cap;
} RuntimeMonitor;

/* ------ Runtime API ------ */

/* init / cleanup */
void runtime_init(int debug);
void runtime_cleanup(void);

/* Instruction ID metadata */
void runtime_set_insn_metadata(int insn_id, const char *file, int line,
                                int col, const char *func);
const InsnMetadata *runtime_get_insn_metadata(int insn_id);

/* Allocation tracking */
void runtime_track_alloc(void *ptr, size_t size, const char *file, int line);
void runtime_track_free(void *ptr, const char *file, int line);
void runtime_track_alloc_id(void *ptr, size_t size, const char *file,
                             int line, int insn_id);

/* Stack tracking */
void runtime_push_frame(const char *func, const char *file, int line);
void runtime_pop_frame(void);

/* Bounds checking (compiler-generated call) */
void runtime_check_bounds(void *ptr, size_t offset, size_t size,
                           int insn_id);
void runtime_check_null(void *ptr, int insn_id);
void runtime_check_overflow(int64_t result, int64_t a, int64_t b,
                             int op, int insn_id);

/* Crash report */
CrashReport *runtime_create_report(GclErrorCode code, void *addr,
                                    size_t offset, size_t size,
                                    int insn_id);
void runtime_print_report(CrashReport *report, FILE *out);
void runtime_save_report(CrashReport *report, const char *path);

/* Error handlers */
void runtime_error(GclErrorCode code, const char *msg, int insn_id);
void runtime_fatal(GclErrorCode code, const char *msg, int insn_id);

/* Get runtime monitor (for debugger integration) */
RuntimeMonitor *runtime_get_monitor(void);

/* Utility */
const char *runtime_error_name(GclErrorCode code);
const char *runtime_suggested_fix(GclErrorCode code);

#endif /* GCL_RUNTIME_H */
