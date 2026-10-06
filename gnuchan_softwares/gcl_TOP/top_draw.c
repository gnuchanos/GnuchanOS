/*
 * top_draw.c — drawing one frame: the panels, the memory line, the process list.
 *
 * The layout is btop's, cut down: a header line, then a band across the top with
 * the CPU on the left half and the GPU on the right, a line of memory and swap,
 * a column header for the process list, and the list filling everything below.
 *
 * Everything is drawn with the cursor moved to an absolute position and the line
 * cleared first (`\033[K`), so a value that shrank — 9% to 10% is fine, but a
 * long command replaced by a short one is not — leaves no tail of the old text
 * behind. The colours are 24-bit escapes and the palette is the desktop's own
 * violet, the same one the panel and the icon theme use, so the monitor looks
 * like part of the system rather than a program with a palette of its own.
 *
 * The process list is the only part with a cursor of its own. The selected row
 * is drawn inverted — the accent as a background — so the eye finds it at once
 * and the `k` that kills goes on the row that is lit.
 */
#include "top_draw.h"

#include <stdio.h>
#include <string.h>

/* The palette, the desktop's violet. Kept here rather than read from a file:
   these are the colours the rest of the system is drawn in and a monitor that
   read them would have one more thing to fail at. */
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

/* The height of the top band: the graph, its two label lines, and a border. */
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

/* --- the top band ---------------------------------------------------------- */

/* Draw one panel: a title, the current value, and its graph. `left` is the
   first column, `width` its width in cells. */
static void draw_panel(int top_row, int left, int width, const char *title,
                       double value, int core_count, const TopHistory *history,
                       int red, int green, int blue) {
    if (width < 8) {
        return;
    }
    /* Title on the left, value on the right, both on the first row. */
    to(top_row, left);
    fg(red, green, blue);
    printf("%s", title);
    fputs(RESET, stdout);

    char value_text[32];
    if (core_count > 0) {
        snprintf(value_text, sizeof(value_text), "%.1f%%  %d cores", value,
                 core_count);
    } else {
        snprintf(value_text, sizeof(value_text), "%.1f%%", value);
    }
    int value_length = (int)strlen(value_text);
    if (value_length < width) {
        to(top_row, left + width - value_length);
        fg(DIM_R, DIM_G, DIM_B);
        printf("%s", value_text);
        fputs(RESET, stdout);
    }

    /* The graph under the title, filling the rest of the panel height. */
    int graph_rows = PANEL_ROWS - 2;
    for (int row = 0; row < graph_rows; row++) {
        to(top_row + 1 + row, left);
        top_graph_draw(history, width, 1, red, green, blue);
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

/* The column header of the process list. */
static void draw_columns(int row, int width) {
    to(row, 1);
    fg(DIM_R, DIM_G, DIM_B);
    /* The name column takes whatever is left after the fixed ones. */
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

    /* The top band. When there is a GPU to report it takes the right half and
       the CPU the left, which is the shape btop has. When there is not — an
       Intel part old enough to have no busy counter anywhere in sysfs, a
       virtual machine, a driver that reports nothing — the CPU takes the whole
       width. A reserved half with nothing to put in it would be a hole in the
       frame, and this is the layout that says "one processor to watch" instead
       of "the second one is broken". */
    int band_top = 2;
    int has_gpu = frame->config->show_gpu && frame->gpu->present;
    if (has_gpu) {
        int half = width / 2;
        draw_panel(band_top, 1, half - 1, "CPU", frame->cpu->overall,
                   frame->cpu->core_count, frame->cpu_history,
                   CPU_R, CPU_G, CPU_B);
        const char *gpu_title = frame->gpu->name[0] ? frame->gpu->name : "GPU";
        double gpu_value = frame->gpu->has_reading ? frame->gpu->usage : 0.0;
        draw_panel(band_top, half + 1, width - half - 1, gpu_title, gpu_value,
                   0, frame->gpu_history, GPU_R, GPU_G, GPU_B);
    } else {
        draw_panel(band_top, 1, width, "CPU", frame->cpu->overall,
                   frame->cpu->core_count, frame->cpu_history,
                   CPU_R, CPU_G, CPU_B);
    }

    /* Memory, then the column header, then the list. */
    int memory_row = band_top + PANEL_ROWS;
    draw_memory(memory_row, frame->mem, width);

    int header_row = memory_row + 1;
    draw_columns(header_row, width);

    int list_top = header_row + 1;
    int list_rows = rows - list_top;          /* one row left for the status line */
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
            to(row, 1);   /* blank the rest of the list */
            continue;
        }
        draw_process(row, &procs->items[index], index == procs->cursor,
                     name_width);
    }

    /* Park the cursor at the foot so the terminal does not scroll. */
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
    /* Pad to the width so nothing of the previous line shows through. */
    int length = (int)strlen(message);
    for (int i = length; i < width; i++) {
        putchar(' ');
    }
    fflush(stdout);
}
