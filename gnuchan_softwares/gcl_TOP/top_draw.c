/*
 * top_draw.c — drawing one frame: the panels, the memory line, the process list.
 *
 * The layout is btop's, cut down: a header line, then a band across the top with
 * the CPU on the left and the GPU on the right, a line of memory and swap, a
 * column header for the process list, and the list filling everything below.
 *
 * The two panels are always both drawn, even when this machine has no GPU
 * reading. The right panel is part of the shape of the program — a person who
 * has seen btop expects a second panel there — and taking it away on the
 * machines that cannot fill it would make the program look broken rather than
 * different. When there is no reading the value shows `n/a` and the graph is
 * flat, which is the truth, where a hidden panel is not.
 *
 * Everything is drawn with the cursor moved to an absolute position and the line
 * cleared first (`\033[K`), so a value that shrank — a long command replaced by
 * a short one — leaves no tail of the old text behind.
 */
#include "top_draw.h"

#include <stdio.h>
#include <string.h>

/* The palette, the desktop's violet. */
#define ACCENT_R 0xa8
#define ACCENT_G 0x55
#define ACCENT_B 0xf7
#define DIM_R    0xb7
#define DIM_G    0x8e
#define DIM_B    0xd6
#define CPU_R    0xa8
#define CPU_G    0x55
#define CPU_B    0xf7
#define GPU_R    0xc7
#define GPU_G    0x7d
#define GPU_B    0xff
#define ERR_R    0xff
#define ERR_G    0x5c
#define ERR_B    0x5c

/* The height of the top band in rows: a title row and the graph under it. */
#define PANEL_ROWS 6
#define RESET "\033[0m"

/* --- small colors ---------------------------------------------------------- */

static void fg(int red, int green, int blue) {
    printf("\033[38;2;%d;%d;%dm", red, green, blue);
}

static void bg(int red, int green, int blue) {
    printf("\033[48;2;%d;%d;%dm", red, green, blue);
}

/* Move the cursor to (row, col), one-based, and clear the line. */
static void to(int row, int column) {
    printf("\033[%d;%dH\033[K", row, column);
}

/* Format a byte count in KB into a short human string: 1234 -> "1.2M". */
static void human_kb(unsigned long long kb, char *out, size_t size) {
    if (kb >= 1024ULL * 1024ULL) {
        snprintf(out, size, "%.1fG", (double)kb / (1024.0 * 1024.0));
    } else if (kb >= 1024ULL) {
        snprintf(out, size, "%.0fM", (double)kb / 1024.0);
    } else {
        snprintf(out, size, "%lluK", kb);
    }
}

/* --- a panel --------------------------------------------------------------- */

/* Draw one panel: a title on the left and the current value on the right on the
   first row, and the history graph filling the rows below. `left` is the first
   column, `width` its width in cells. */
static void draw_panel(int top_row, int left, int width, const char *title,
                       const char *value_text, const TopHistory *history,
                       int red, int green, int blue) {
    if (width < 8) {
        return;
    }
    to(top_row, left);
    fg(red, green, blue);
    printf("%s", title);
    fputs(RESET, stdout);

    int value_length = (int)strlen(value_text);
    if (value_length < width) {
        to(top_row, left + width - value_length);
        fg(DIM_R, DIM_G, DIM_B);
        printf("%s", value_text);
        fputs(RESET, stdout);
    }

    /* The graph, one text row at a time, under the title. */
    int graph_rows = PANEL_ROWS - 1;
    for (int row = 0; row < graph_rows; row++) {
        to(top_row + 1 + row, left);
        top_graph_draw(history, width, graph_rows, row, red, green, blue);
    }
}

/* --- the memory line ------------------------------------------------------- */

static void draw_memory(int row, const TopMem *mem, int width) {
    to(row, 1);
    fg(ACCENT_R, ACCENT_G, ACCENT_B);
    printf("MEM");
    fputs(RESET, stdout);

    char used[32];
    char total[32];
    char swap_used[32];
    char swap_total[32];
    human_kb(mem->used_kb, used, sizeof(used));
    human_kb(mem->total_kb, total, sizeof(total));
    human_kb(mem->swap_used_kb, swap_used, sizeof(swap_used));
    human_kb(mem->swap_total_kb, swap_total, sizeof(swap_total));

    double percent = top_mem_percent(mem->used_kb, mem->total_kb);
    printf("  %s / %s  (%.0f%%)", used, total, percent);
    if (mem->swap_total_kb > 0) {
        printf("   SWAP %s / %s", swap_used, swap_total);
    }
    (void)width;
}

/* --- the process list ------------------------------------------------------ */

static void draw_columns(int row, int width) {
    to(row, 1);
    fg(DIM_R, DIM_G, DIM_B);
    printf("  %-7s %-10s %6s %6s %-8s %s", "PID", "USER", "CPU%", "MEM%",
           "STATE", "COMMAND");
    (void)width;
    fputs(RESET, stdout);
}

/* One process row. `selected` draws it inverted. `name_width` is how many
   characters of the command fit. */
static void draw_process(int row, const TopProc *proc, int selected,
                         int name_width) {
    to(row, 1);
    if (selected) {
        bg(ACCENT_R, ACCENT_G, ACCENT_B);
        fg(0x1c, 0x05, 0x32);
    }
    char name[TOP_TEXT];
    snprintf(name, sizeof(name), "%s", proc->name);
    if (name_width > 0 && (int)strlen(name) > name_width) {
        name[name_width] = '\0';
    }
    printf("  %-7d %-10s %6.1f %6.1f %-8c %s", proc->pid, proc->user,
           proc->cpu, proc->mem, proc->state, name);
    fputs(RESET, stdout);
}

/* --- the whole frame ------------------------------------------------------- */

void top_draw_frame(const TopFrame *frame) {
    int width = frame->size.cols;
    int rows = frame->size.rows;

    /* Header: name, uptime, the sort key, and the key hints. */
    to(1, 1);
    fg(ACCENT_R, ACCENT_G, ACCENT_B);
    printf("GnuChanTop");
    fputs(RESET, stdout);
    unsigned long long hours = frame->uptime_seconds / 3600;
    unsigned long long minutes = (frame->uptime_seconds % 3600) / 60;
    printf("   up %lluh%llum   sort:%s   [arrows] move  [k] kill  [s] sort  [q] quit",
           hours, minutes, top_sort_name(frame->config->sort));

    /* The top band: CPU left, GPU right, the two halves of the width. */
    int band_top = 2;
    int half = width / 2;

    char cpu_value[48];
    snprintf(cpu_value, sizeof(cpu_value), "%.1f%%  %d cores",
             frame->cpu->overall, frame->cpu->core_count);
    draw_panel(band_top, 1, half - 1, "CPU", cpu_value, frame->cpu_history,
               CPU_R, CPU_G, CPU_B);

    char gpu_value[48];
    const char *gpu_title = "GPU";
    if (frame->gpu->present && frame->gpu->name[0]) {
        gpu_title = frame->gpu->name;
    }
    if (frame->config->show_gpu && frame->gpu->present && frame->gpu->has_reading) {
        snprintf(gpu_value, sizeof(gpu_value), "%.1f%%", frame->gpu->usage);
    } else {
        /* No reading on this machine: say so in the value, rather than hiding
           the panel or lying with a zero. */
        snprintf(gpu_value, sizeof(gpu_value), "n/a");
    }
    draw_panel(band_top, half + 1, width - half - 1, gpu_title, gpu_value,
               frame->gpu_history, GPU_R, GPU_G, GPU_B);

    /* Memory, then the column header, then the list. */
    int memory_row = band_top + PANEL_ROWS;
    draw_memory(memory_row, frame->mem, width);

    int header_row = memory_row + 1;
    draw_columns(header_row, width);

    int list_top = header_row + 1;
    int list_rows = rows - list_top;
    if (list_rows < 1) {
        list_rows = 0;
    }
    int name_width = width - 45;
    if (name_width < 8) {
        name_width = 8;
    }

    const TopProcs *procs = frame->procs;
    for (int i = 0; i < list_rows; i++) {
        int index = procs->scroll + i;
        int row = list_top + i;
        if (index >= procs->count) {
            to(row, 1);
            continue;
        }
        draw_process(row, &procs->items[index], index == procs->cursor,
                     name_width);
    }

    to(rows, 1);
    fflush(stdout);
}

void top_draw_status(const char *message, int is_error, int width, int rows) {
    to(rows, 1);
    if (is_error) {
        fg(ERR_R, ERR_G, ERR_B);
    } else {
        fg(ACCENT_R, ACCENT_G, ACCENT_B);
    }
    printf("%s", message);
    fputs(RESET, stdout);
    int length = (int)strlen(message);
    for (int i = length; i < width; i++) {
        putchar(' ');
    }
    fflush(stdout);
}
