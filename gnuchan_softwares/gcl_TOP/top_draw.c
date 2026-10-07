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

/* Move the cursor to (row, col), one-based. This does not clear the line, so a
   box border drawn before it is left alone; use `to()` when the line must be
   blanked first. */
static void move_to(int row, int column) {
    printf("\033[%d;%dH", row, column);
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

/* --- box drawing ----------------------------------------------------------- */

/* The box characters, U+2500 block. Every console font has them, which the
   braille a graph might otherwise use is not. */
#define BOX_TL "\xe2\x94\x8c"  /* ┌ */
#define BOX_TR "\xe2\x94\x90"  /* ┐ */
#define BOX_BL "\xe2\x94\x94"  /* └ */
#define BOX_BR "\xe2\x94\x98"  /* ┘ */
#define BOX_H  "\xe2\x94\x80"  /* ─ */
#define BOX_V  "\xe2\x94\x82"  /* │ */

/* The top border of a box: a corner, " title ", a run of dashes, another
   corner. The width counts the whole line, borders included. */
static void box_top(int row, int left, int width, const char *title,
                    int red, int green, int blue) {
    if (width < 6) {
        return;
    }
    to(row, left);
    fg(red, green, blue);
    fputs(BOX_TL, stdout);
    printf(" %s ", title);
    int used = 3 + (int)strlen(title);
    for (int i = used; i < width - 1; i++) {
        fputs(BOX_H, stdout);
    }
    fputs(BOX_TR, stdout);
    fputs(RESET, stdout);
}

static void box_bottom(int row, int left, int width,
                       int red, int green, int blue) {
    if (width < 2) {
        return;
    }
    to(row, left);
    fg(red, green, blue);
    fputs(BOX_BL, stdout);
    for (int i = 1; i < width - 1; i++) {
        fputs(BOX_H, stdout);
    }
    fputs(BOX_BR, stdout);
    fputs(RESET, stdout);
}

/* A horizontal bar of `width` cells for a 0..100 value: the filled part in the
   value's colour, the rest dim. The heavy block is the fill, the light shade
   the empty track. */
static void draw_bar(double percent, int width,
                     int red, int green, int blue) {
    if (width < 1) {
        return;
    }
    if (percent < 0.0)   percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    int full = (int)(percent / 100.0 * width + 0.5);
    if (full > width) full = width;
    fg(red, green, blue);
    for (int i = 0; i < full; i++) {
        fputs("\xe2\x96\x88", stdout);  /* █ */
    }
    fg(DIM_R, DIM_G, DIM_B);
    for (int i = full; i < width; i++) {
        fputs("\xe2\x96\x91", stdout);  /* ░ */
    }
    fputs(RESET, stdout);
}

/* --- temperature color ----------------------------------------------------- */

/* Green while cool, amber as it warms, red when it is hot. This is the one
   place a colour means a judgement, and it is the judgement a person watching
   the temperature wants at a glance. */
static void temp_color(double celsius) {
    if (celsius >= 80.0) {
        fg(0xff, 0x5c, 0x5c);
    } else if (celsius >= 65.0) {
        fg(0xf7, 0xc8, 0x55);
    } else {
        fg(0x6f, 0xd7, 0x8e);
    }
}

/* --- box rows -------------------------------------------------------------- */

/* A box interior row drawn empty, with its two vertical borders and spaces
   between them. Text is put on it afterwards with move_to(), which does not
   clear the line, so the borders a caller drew first stay put. */
static void box_row(int row, int left, int width,
                    int rr, int gg, int bb) {
    if (width < 2) {
        return;
    }
    move_to(row, left);
    fg(rr, gg, bb);
    fputs(BOX_V, stdout);
    for (int i = 0; i < width - 2; i++) {
        putchar(' ');
    }
    fputs(BOX_V, stdout);
    fputs(RESET, stdout);
}

/* --- a meter line ---------------------------------------------------------- */

/* One interior line of a box: a short label, a bar, the value as a percentage,
   and — when there is a temperature — it as well, ending in the degree sign.

   The bar's width is derived from the box so the line can never write past the
   right border: the label, the percentage and the temperature take a fixed
   number of cells, and the bar takes what is left of the interior. When even
   that is too little the temperature is dropped rather than the line being
   aloud to overflow, which is the failure that breaks the box. */
static void draw_meter(int row, int left, int width, const char *label,
                       double percent, double temp, int has_temp,
                       int rr, int gg, int bb) {
    int inner = width - 4;            /* cells between the borders, less padding */
    if (inner < 8) {
        return;
    }
    /* Fixed cells: label(4) + space + "100.0%"(6) + space + "100°C"(5). */
    int fixed = 4 + 7 + (has_temp ? 6 : 0);
    int bar_w = inner - fixed;
    if (bar_w < 4) {
        has_temp = 0;
        fixed = 4 + 7;
        bar_w = inner - fixed;
    }
    if (bar_w < 4) {
        bar_w = 4;
    }

    move_to(row, left + 2);
    fg(rr, gg, bb);
    printf("%-4s", label);
    fputs(RESET, stdout);
    draw_bar(percent, bar_w, rr, gg, bb);
    fg(rr, gg, bb);
    printf(" %5.1f%%", percent);
    if (has_temp) {
        temp_color(temp);
        printf(" %3.0f\xc2\xb0" "C", temp);
    }
    fputs(RESET, stdout);
}

/* --- the process list ------------------------------------------------------ */

/* The column header inside the process box. */
static void draw_columns(int row, int left) {
    move_to(row, left + 2);
    fg(DIM_R, DIM_G, DIM_B);
    printf("%-6s %-9s %6s %6s %-4s %s", "PID", "USER", "CPU%", "MEM%",
           "S", "COMMAND");
    fputs(RESET, stdout);
}

/* One process row inside the box. `selected` draws it inverted. `name_width`
   caps the command so the line never reaches the right border. */
static void draw_process(int row, int left, const TopProc *proc, int selected,
                         int name_width) {
    move_to(row, left + 2);
    if (selected) {
        bg(ACCENT_R, ACCENT_G, ACCENT_B);
        fg(0x1c, 0x05, 0x32);
    }
    char name[TOP_TEXT];
    snprintf(name, sizeof(name), "%s", proc->name);
    if (name_width > 0 && (int)strlen(name) > name_width) {
        name[name_width] = '\0';
    }
    printf("%-6d %-9.9s %6.1f %6.1f %-4c %s", proc->pid, proc->user,
           proc->cpu, proc->mem, proc->state, name);
    fputs(RESET, stdout);
}

/* --- the whole frame ------------------------------------------------------- */

void top_draw_frame(const TopFrame *frame) {
    int width = frame->size.cols;
    int rows = frame->size.rows;
    if (width < 60 || rows < 14) {
        /* Too small for the boxed layout; leave the screen as the terminal
           cleared it rather than writing a box that does not fit. */
        to(rows, 1);
        fflush(stdout);
        return;
    }

    int half = width / 2;
    int cpu_left = 1, cpu_w = half - 1;
    int gpu_left = half + 1, gpu_w = width - half - 1;
    int cores = frame->cpu->core_count;
    if (cores < 1) {
        cores = 1;
    }

    /* Header line (not boxed): the name, the uptime and the key hints. */
    to(1, 1);
    fg(ACCENT_R, ACCENT_G, ACCENT_B);
    printf("GnuChanTop");
    fputs(RESET, stdout);
    unsigned long long hours = frame->uptime_seconds / 3600;
    unsigned long long minutes = (frame->uptime_seconds % 3600) / 60;
    printf("  up %lluh%llum  sort:%s  [^.v] move  [</>] sort  [k] kill  [q] quit",
           hours, minutes, top_sort_name(frame->config->sort));

    /* The CPU box on the left and the GPU box on the right, the same height:
       one line per core, a total line, then a graph line. */
    int top_row = 2;
    int total_row = top_row + 1 + cores;   /* the average line */
    int graph_row = total_row + 1;         /* the graph line */
    int top_bottom = graph_row + 1;

    box_top(top_row, cpu_left, cpu_w, "CPU", CPU_R, CPU_G, CPU_B);
    box_top(top_row, gpu_left, gpu_w, "GPU", GPU_R, GPU_G, GPU_B);

    for (int i = 0; i < cores; i++) {
        int row = top_row + 1 + i;
        char label[8];
        snprintf(label, sizeof(label), "C%d", i);
        box_row(row, cpu_left, cpu_w, CPU_R, CPU_G, CPU_B);
        draw_meter(row, cpu_left, cpu_w, label, frame->cpu->usage[i],
                   frame->cpu->temp_core[i],
                   frame->cpu->has_temp && frame->cpu->temp_core[i] > 0.0,
                   CPU_R, CPU_G, CPU_B);
    }
    /* The average across every core, with the package temperature. An integrated
       GPU shares the CPU die, so the package temperature is the GPU's too. */
    box_row(total_row, cpu_left, cpu_w, CPU_R, CPU_G, CPU_B);
    draw_meter(total_row, cpu_left, cpu_w, "Tot", frame->cpu->overall,
               frame->cpu->temp_package,
               frame->cpu->has_temp && frame->cpu->temp_package > 0.0,
               CPU_R, CPU_G, CPU_B);

    box_row(graph_row, cpu_left, cpu_w, CPU_R, CPU_G, CPU_B);
    move_to(graph_row, cpu_left + 2);
    top_graph_draw(frame->cpu_history, cpu_w - 4, 1, 0, CPU_R, CPU_G, CPU_B);

    /* The GPU box: the usage line at the top, its graph at the bottom, and the
       lines between left blank so the box is as tall as the CPU's. */
    box_row(top_row + 1, gpu_left, gpu_w, GPU_R, GPU_G, GPU_B);
    if (frame->config->show_gpu && frame->gpu->present && frame->gpu->has_reading) {
        draw_meter(top_row + 1, gpu_left, gpu_w, "GPU", frame->gpu->usage,
                   frame->cpu->temp_package,
                   frame->cpu->has_temp && frame->cpu->temp_package > 0.0,
                   GPU_R, GPU_G, GPU_B);
    } else {
        move_to(top_row + 1, gpu_left + 2);
        fg(DIM_R, DIM_G, DIM_B);
        printf("no reading on this machine");
        fputs(RESET, stdout);
    }
    for (int row = top_row + 2; row < graph_row; row++) {
        box_row(row, gpu_left, gpu_w, GPU_R, GPU_G, GPU_B);
    }
    box_row(graph_row, gpu_left, gpu_w, GPU_R, GPU_G, GPU_B);
    move_to(graph_row, gpu_left + 2);
    top_graph_draw(frame->gpu_history, gpu_w - 4, 1, 0, GPU_R, GPU_G, GPU_B);

    box_bottom(top_bottom, cpu_left, cpu_w, CPU_R, CPU_G, CPU_B);
    box_bottom(top_bottom, gpu_left, gpu_w, GPU_R, GPU_G, GPU_B);

    /* The memory box, full width: top border, one content line, bottom border. */
    int mem_top = top_bottom + 1;
    int mem_row = mem_top + 1;
    int mem_bottom = mem_row + 1;
    box_top(mem_top, 1, width, "MEM", ACCENT_R, ACCENT_G, ACCENT_B);
    box_row(mem_row, 1, width, ACCENT_R, ACCENT_G, ACCENT_B);
    box_bottom(mem_bottom, 1, width, ACCENT_R, ACCENT_G, ACCENT_B);

    double mem_percent = top_mem_percent(frame->mem->used_kb, frame->mem->total_kb);
    char used[32], total[32], swap_used[32], swap_total[32];
    human_kb(frame->mem->used_kb, used, sizeof(used));
    human_kb(frame->mem->total_kb, total, sizeof(total));
    human_kb(frame->mem->swap_used_kb, swap_used, sizeof(swap_used));
    human_kb(frame->mem->swap_total_kb, swap_total, sizeof(swap_total));
    move_to(mem_row, 3);
    draw_bar(mem_percent, 24, ACCENT_R, ACCENT_G, ACCENT_B);
    fg(ACCENT_R, ACCENT_G, ACCENT_B);
    printf("  %s / %s (%.0f%%)", used, total, mem_percent);
    if (frame->mem->swap_total_kb > 0) {
        printf("   SWAP %s / %s", swap_used, swap_total);
    }
    fputs(RESET, stdout);
    /* No temperature on this line. The package temperature is already drawn in
       the CPU box above (its "Tot" meter), and repeating it here was a second
       copy of the same number taking the right end of the memory line for no
       reason. */

    /* The process box: top border, a column header, the list, bottom border. */
    int proc_top = mem_bottom + 1;
    int proc_header = proc_top + 1;
    int list_top = proc_top + 2;
    int list_bottom = rows - 1;            /* the last row is the status line */
    box_top(proc_top, 1, width, "Processes", ACCENT_R, ACCENT_G, ACCENT_B);
    box_row(proc_header, 1, width, ACCENT_R, ACCENT_G, ACCENT_B);
    draw_columns(proc_header, 1);

    int name_width = width - 42;
    if (name_width < 8) {
        name_width = 8;
    }
    const TopProcs *procs = frame->procs;
    for (int row = list_top; row < list_bottom; row++) {
        int index = procs->scroll + (row - list_top);
        box_row(row, 1, width, ACCENT_R, ACCENT_G, ACCENT_B);
        if (index >= procs->count) {
            continue;
        }
        draw_process(row, 1, &procs->items[index], index == procs->cursor,
                     name_width);
    }
    box_bottom(list_bottom, 1, width, ACCENT_R, ACCENT_G, ACCENT_B);

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
