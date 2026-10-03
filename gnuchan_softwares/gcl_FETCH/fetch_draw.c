/*
 * fetch_draw.c — the whole output.
 *
 * The picture is drawn as text, which is the one trick this file is built
 * around. A terminal cell is about twice as tall as it is wide, and the
 * character U+2580 fills only its upper half, so one cell can show two pixels:
 * the upper one as the foreground colour of that character and the lower one as
 * that same cell's background. Every image pixel is scaled down to roughly one
 * per cell in each direction, so a picture `rows` text rows tall is a grid of
 * `rows * 2` pixel rows, and each pair of pixel rows becomes one line of
 * half-blocks. Nothing here guesses at the terminal's palette: the colours are
 * written as 24-bit escapes, which is what every terminal on this desktop
 * understands, and the picture keeps the colours it was drawn with.
 *
 * The information sits to the right of the picture, aligned to the top, and
 * where there is no picture it starts at the left margin and is printed on its
 * own. The layout is a column of text lines — the "user@host" heading, the
 * colour swatch, a blank, then one line per field — and the drawing walks the
 * two together, one text row at a time.
 *
 * A transparent pixel is not drawn as black. It is composited over the picture's
 * background colour, which the settings name, so a logo with a see-through
 * background sits on the terminal rather than on a black rectangle — see
 * fetch_image.h for why the alpha is carried this far.
 */
#include <stdio.h>
#include <string.h>

#include "fetch_draw.h"
#include "fetch_color.h"
#include "fetch_info.h"

/* The most lines the right-hand column can hold: the heading, the swatch, a
   blank, and one line per field, with room to spare. It is larger than
   FETCH_MAX_FIELDS on purpose, so the fixed lines are never what overflows. */
#define DRAW_MAX_LINES (FETCH_MAX_FIELDS + 8)

/* The character a cell is drawn with, and the escape that ends a colour run.
   The half block is written as its UTF-8 bytes so the source file stays plain
   ASCII and nothing depends on how the compiler reads a multi-byte literal. */
#define HALF_BLOCK "\xE2\x96\x80"
#define RESET      "\033[0m"

/* The colour the labels are drawn in, which is the desktop's own violet — the
   same accent the panel and the icon theme use — so the labels match the rest
   of the system rather than a fetch program's invented palette. */
#define LABEL_R 0xa8
#define LABEL_G 0x55
#define LABEL_B 0xf7

/* --- colour escapes -------------------------------------------------------- */

/* A foreground colour, as a 24-bit escape. */
static void set_foreground(FetchColor colour) {
    printf("\033[38;2;%d;%d;%dm", colour.red, colour.green, colour.blue);
}

/* A background colour, as a 24-bit escape. */
static void set_background(FetchColor colour) {
    printf("\033[48;2;%d;%d;%dm", colour.red, colour.green, colour.blue);
}

/* One pixel over the picture's background: a fully opaque pixel is itself, a
   fully transparent one is the background, and anything between is a blend of
   the two. This is the only place the alpha carried from fetch_image.c is used,
   and it is why a logo with soft edges does not gain a dark fringe. */
static FetchColor composite(FetchPixel pixel, FetchColor background) {
    unsigned int alpha = pixel.alpha;
    FetchColor out;
    out.red = (unsigned char)((pixel.red * alpha +
                               background.red * (255 - alpha)) / 255);
    out.green = (unsigned char)((pixel.green * alpha +
                                 background.green * (255 - alpha)) / 255);
    out.blue = (unsigned char)((pixel.blue * alpha +
                                background.blue * (255 - alpha)) / 255);
    return out;
}

/* --- the picture, one text row at a time ---------------------------------- */

/* Draw text row `row` of the picture: its two pixel rows become one line of
   half-blocks, each cell an upper pixel on the foreground and a lower pixel on
   the background. The caller has already decided that this row is inside the
   picture. */
static void draw_image_row(const FetchImage *image, int row,
                           FetchColor background) {
    int upper_y = row * 2;
    int lower_y = upper_y + 1;
    int last_fore = -1;
    int last_back = -1;

    for (int x = 0; x < image->width; x++) {
        FetchPixel upper = image->pixels[upper_y * image->width + x];
        FetchPixel lower = image->pixels[lower_y * image->width + x];
        FetchColor fore = composite(upper, background);
        FetchColor back = composite(lower, background);

        /* The escape is only written when the colour changes, which keeps a
           run of the same colour from filling the line with escapes — an image
           of a logo has large flat areas, and this is what makes them cheap. */
        int fore_key = (fore.red << 16) | (fore.green << 8) | fore.blue;
        int back_key = (back.red << 16) | (back.green << 8) | back.blue;
        if (fore_key != last_fore) {
            set_foreground(fore);
            last_fore = fore_key;
        }
        if (back_key != last_back) {
            set_background(back);
            last_back = back_key;
        }
        fputs(HALF_BLOCK, stdout);
    }
    fputs(RESET, stdout);
}

/* --- the swatch ------------------------------------------------------------ */

/* The line of colour blocks under the heading: one block per colour the settings
   named, each a pair of spaces on its own background. */
static void draw_swatch(const FetchConfig *config) {
    for (int i = 0; i < config->colour_count; i++) {
        FetchColor colour = fetch_color_parse(config->colours[i],
                                              fetch_color_default());
        set_background(colour);
        fputs("  ", stdout);
    }
    fputs(RESET, stdout);
}

/* --- the text of one line -------------------------------------------------- */

/* One field line, with the label in the accent colour and the value in the
   terminal's own foreground. "OS: Debian" prints "OS" violet and ": Debian" in
   the default colour. A line with no ": " is printed whole, so the heading and
   the blank need no special case. */
static void draw_text_line(const char *line) {
    const char *separator = strstr(line, ": ");
    if (!separator) {
        fputs(line, stdout);
        return;
    }
    FetchColor label = {LABEL_R, LABEL_G, LABEL_B};
    set_foreground(label);
    fwrite(line, 1, (size_t)(separator - line), stdout);
    fputs(RESET, stdout);
    fputs(separator, stdout);
}

/* --- the whole thing ------------------------------------------------------- */

/* Draw the whole thing when the terminal can draw a picture as a picture.
 *
 * The picture is not turned into characters here: it is HANDED to the terminal
 * with the `ESC ] 1338` sequence — see term_image.h in gcl_TERMINAL — and the
 * terminal draws the file's own pixels over the cells. The program's job is
 * only to say WHERE: the block of cells the picture gets, and then to move its
 * own text beside it.
 *
 * The width in cells is worked out from the picture's proportions and the rows
 * it was loaded for, because the text has to be placed to the right of it and
 * the terminal is told the same width so the two agree. A cell is about twice
 * as tall as it is wide, which is where the factor of two comes from: `rows`
 * text rows are `rows * 2` cell-widths tall, and the picture is as wide as its
 * own shape says at that height.
 *
 * The cursor is NOT moved by the image; the program moves it to the text column
 * itself and prints the facts there. So the picture and the text are drawn in
 * one pass and nothing has to be measured after the fact. */
static void draw_with_graphics(const FetchConfig *config,
                               const FetchImage *image) {
    int image_rows = image->height / 2;
    if (image_rows < 1) {
        image_rows = 1;
    }

    /* The cells the picture is as wide as: its own proportions at `image_rows`
       text rows, with a cell taken as twice as tall as it is wide. */
    int cols = 0;
    if (image->height > 0) {
        cols = (image->width * image_rows * 2) / image->height;
    }
    if (cols < 1) {
        cols = 1;
    }

    /* The cursor goes home first, so the picture anchors at the top-left of the
       grid whatever was on the line before. */
    fputs("\033[H", stdout);
    printf("\033]1338;0;0;%d;%d;%s\a", cols, image_rows, config->image);

    /* The text starts to the RIGHT of the picture, with the gap the settings
       name between them. */
    int text_column = cols + config->gap + 1;

    /* The right-hand column, the same lines the block path builds. */
    char lines[DRAW_MAX_LINES][FETCH_TEXT_LENGTH * 2];
    int is_swatch[DRAW_MAX_LINES];
    int line_count = 0;

    char title[FETCH_TEXT_LENGTH];
    fetch_title_line(title, sizeof(title));
    if (title[0]) {
        snprintf(lines[line_count], sizeof(lines[0]), "%s", title);
        is_swatch[line_count] = 0;
        line_count++;
    }
    if (config->colour_count > 0 && line_count < DRAW_MAX_LINES) {
        lines[line_count][0] = '\0';
        is_swatch[line_count] = 1;
        line_count++;
    }
    if (line_count < DRAW_MAX_LINES) {
        lines[line_count][0] = '\0';
        is_swatch[line_count] = 0;
        line_count++;
    }
    for (int i = 0; i < config->field_count && line_count < DRAW_MAX_LINES; i++) {
        FetchField field = fetch_field_from_name(config->fields[i]);
        if (field == FETCH_FIELD_UNKNOWN) {
            continue;
        }
        char value[FETCH_TEXT_LENGTH];
        fetch_field_line(field, value, sizeof(value));
        if (!value[0]) {
            continue;
        }
        snprintf(lines[line_count], sizeof(lines[0]), "%s", value);
        is_swatch[line_count] = 0;
        line_count++;
    }

    int rows = line_count > image_rows ? line_count : image_rows;
    for (int row = 0; row < rows; row++) {
        printf("\033[%d;%dH", row + 1, text_column);
        if (row < line_count) {
            if (is_swatch[row]) {
                draw_swatch(config);
            } else if (lines[row][0]) {
                draw_text_line(lines[row]);
            }
        }
    }
    /* The cursor is put below both, so a shell prompt after the fetch does not
       land over the picture. */
    int bottom = rows + 1;
    printf("\033[%d;1H", bottom);
    fputs(RESET, stdout);
}

void fetch_draw(const FetchConfig *config, const FetchImage *image,
                int use_graphics) {
    int has_image = image && image->ok && image->width > 0;
    if (use_graphics && has_image && config->image[0]) {
        draw_with_graphics(config, image);
        return;
    }
    /* The right-hand column, built first so the two sides can be drawn against
       each other. Lines are plain text except the swatch, which is flagged. */
    char lines[DRAW_MAX_LINES][FETCH_TEXT_LENGTH * 2];
    int is_swatch[DRAW_MAX_LINES];
    int line_count = 0;

    /* The heading, then the swatch, then a blank, then the fields. */
    char title[FETCH_TEXT_LENGTH];
    fetch_title_line(title, sizeof(title));
    if (title[0]) {
        snprintf(lines[line_count], sizeof(lines[0]), "%s", title);
        is_swatch[line_count] = 0;
        line_count++;
    }

    if (config->colour_count > 0 && line_count < DRAW_MAX_LINES) {
        lines[line_count][0] = '\0';
        is_swatch[line_count] = 1;
        line_count++;
    }

    if (line_count < DRAW_MAX_LINES) {
        lines[line_count][0] = '\0';
        is_swatch[line_count] = 0;
        line_count++;
    }

    for (int i = 0; i < config->field_count && line_count < DRAW_MAX_LINES; i++) {
        FetchField field = fetch_field_from_name(config->fields[i]);
        if (field == FETCH_FIELD_UNKNOWN) {
            fprintf(stderr, "gnuchanfetch: no field called '%s'\n",
                    config->fields[i]);
            continue;
        }
        char value[FETCH_TEXT_LENGTH];
        fetch_field_line(field, value, sizeof(value));
        if (!value[0]) {
            /* The machine did not report it: leave the line out rather than
               print a label with nothing after it. */
            continue;
        }
        snprintf(lines[line_count], sizeof(lines[0]), "%s", value);
        is_swatch[line_count] = 0;
        line_count++;
    }

    /* The picture's own background, and how many text rows it takes. The
       `has_image` above is reused: it was already computed for the graphics
       decision at the top of the function. */
    FetchColor background = fetch_color_parse(config->image_background,
                                              fetch_color_default());
    int image_rows = has_image ? image->height / 2 : 0;

    int rows = line_count > image_rows ? line_count : image_rows;

    for (int row = 0; row < rows; row++) {
        if (has_image && row < image_rows) {
            draw_image_row(image, row, background);
        }
        /* Even past the picture the text keeps its margin, so the column does
           not jump left under the last of the image. */
        for (int i = 0; i < (has_image ? config->gap : 0); i++) {
            putchar(' ');
        }

        if (row < line_count) {
            if (is_swatch[row]) {
                draw_swatch(config);
            } else if (lines[row][0]) {
                draw_text_line(lines[row]);
            }
        }
        putchar('\n');
    }
}
