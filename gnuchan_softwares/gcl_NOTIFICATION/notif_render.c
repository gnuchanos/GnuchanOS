/*
 * notif_render.c — the stack of bubbles, laid out and drawn.
 *
 * The heart of this file is one function, text_lines(): it breaks a string into
 * the lines that fit a given width, and it is the ONLY thing that decides where
 * a line ends. The bubble's height and the drawing both call it and both read
 * the same answer, so a bubble can never be sized for one set of lines and
 * painted with another. That was the bug: two separate wrap routines that
 * disagreed, so short text overflowed a box that had been measured for fewer
 * lines than were drawn.
 *
 * A bubble is as tall as the lines its text makes. A one-line notification is
 * short; a paragraph is tall. Nothing is clipped to a fixed height.
 *
 * Every bubble is on its own clock. notif_render_draw is handed the current
 * instant, and each bubble's countdown bar is drawn from its own born_ms and
 * its own timeout, so one bubble running out does not touch its neighbours.
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h>

#include "notif_render.h"
#include "notif_image.h"
#include "notif_shape.h"

/* The most lines one string may make on screen. A ceiling, not a target: the
   bubble is as tall as its text until the text would be taller than reason. */
#define NOTIF_HARD_MAX_LINES 40
/* The air between the two lines of text. */
#define NOTIF_LINE_GAP 4
/* The height of the countdown bar along a bubble's bottom edge. */
#define NOTIF_TIMER_HEIGHT 3

/* One line of a broken-up string: where it starts and how many bytes it is.
   Bytes and not characters, so a UTF-8 sequence is never split. */
typedef struct TextLine {
    int start;
    int length;
} TextLine;

static int line_height(XftFont *font) {
    if (!font) {
        return 0;
    }
    return font->ascent + font->descent;
}

/* How wide `length` bytes of `text` are in `font`. */
static int text_width(Display *display, XftFont *font, const char *text,
                      int length) {
    if (!font || !text || length <= 0) {
        return 0;
    }
    XGlyphInfo extent;
    XftTextExtentsUtf8(display, font, (const FcChar8 *)text, length, &extent);
    return (int)extent.xOff;
}

/* The longest prefix of text[from..) that fits `max_width`, breaking at a space
   when there is one and mid-word when there is not. Returns the number of
   bytes taken, at least one, so the walk always advances. */
static int take_line(Display *display, XftFont *font, const char *text,
                     int from, int total, int max_width) {
    /* The whole remainder fits: that is the easy and common case. */
    if (text_width(display, font, text + from, total - from) <= max_width) {
        return total - from;
    }

    /* Walk forward, remembering the last space that still fits. */
    int last_space = -1;
    int walk = from;
    while (walk < total) {
        /* Take one whole word. */
        int word_start = walk;
        while (walk < total && text[walk] != ' ' && text[walk] != '\n') {
            walk++;
        }
        int width = text_width(display, font, text + from, walk - from);
        if (width > max_width && word_start > from) {
            /* This word pushes the line past the edge, and there is already
               something on the line: break before it. */
            return last_space > from ? last_space - from : word_start - from;
        }
        if (width <= max_width) {
            last_space = walk;
        } else {
            /* One word alone is wider than the whole line. Break it at the
               widest prefix that fits, so the word is shown rather than cut
               out entirely. */
            int cut = word_start;
            int previous = word_start;
            while (cut < walk) {
                if (text_width(display, font, text + from, cut - from + 1) >
                    max_width) {
                    break;
                }
                previous = cut + 1;
                cut++;
            }
            return previous > from ? previous - from : 1;
        }
        /* Step over the space or newline that ended the word. */
        if (walk < total && (text[walk] == ' ' || text[walk] == '\n')) {
            walk++;
        }
    }
    return total - from;
}

/* Break `text` into lines that fit `max_width`, at most `max_lines` of them,
   writing each into `out`. Returns how many lines were written. This is the one
   place a line is decided; height and drawing both come here. */
static int text_lines(Display *display, XftFont *font, const char *text,
                      int max_width, int max_lines, TextLine *out) {
    if (!font || !text || !text[0] || max_width <= 0 || max_lines <= 0) {
        return 0;
    }
    int total = (int)strlen(text);
    if (total > NOTIF_TEXT_LENGTH * 16) {
        total = NOTIF_TEXT_LENGTH * 16;
    }

    int count = 0;
    int from = 0;
    while (from < total && count < max_lines) {
        int taken = take_line(display, font, text, from, total, max_width);
        if (taken < 1) {
            taken = 1;
        }
        out[count].start = from;
        out[count].length = taken;
        count++;
        from += taken;
        /* A line never begins with the space it was broken at. */
        while (from < total && text[from] == ' ') {
            from++;
        }
    }
    return count;
}

static unsigned long resolve(NotifRender *render, const char *name,
                             unsigned long fallback) {
    return notif_config_colour(render->display, render->screen, name, fallback);
}

int notif_render_init(NotifRender *render, Display *display, int screen,
                      const NotifConfig *config) {
    if (!render || !display) {
        return -1;
    }
    memset(render, 0, sizeof(*render));
    render->display = display;
    render->screen = screen;
    render->root = RootWindow(display, screen);
    render->visual = DefaultVisual(display, screen);
    render->depth = DefaultDepth(display, screen);
    render->colormap = DefaultColormap(display, screen);
    render->config = *config;

    render->gc = XCreateGC(display, render->root, 0, NULL);

    render->background = resolve(render, config->background, 0x1a0b2e);
    render->background_alt = resolve(render, config->background_alt, 0x241033);
    render->frame = resolve(render, config->frame, 0x7b2cbf);
    render->title = resolve(render, config->title, 0xe0c3fc);
    render->body = resolve(render, config->body, 0xc9b6e4);
    render->accent = resolve(render, config->accent, 0xc77dff);
    render->urgent = resolve(render, config->urgent, 0xff5c8a);
    render->icon_background = resolve(render, config->icon_background, 0x1a0b2e);

    render->font_title = XftFontOpenName(display, screen, config->font);
    if (!render->font_title) {
        render->font_title = XftFontOpenName(display, screen,
                                             "monospace:pixelsize=13");
    }
    char body_font[NOTIF_TEXT_LENGTH * 2];
    const char *colon = strchr(config->font, ':');
    int body_size = 13 * config->body_scale / 100;
    if (body_size < 6) {
        body_size = 6;
    }
    if (colon && (size_t)(colon - config->font) < sizeof(body_font) - 32) {
        snprintf(body_font, sizeof(body_font), "%.*s:pixelsize=%d",
                 (int)(colon - config->font), config->font, body_size);
    } else {
        snprintf(body_font, sizeof(body_font), "monospace:pixelsize=%d",
                 body_size);
    }
    render->font_body = XftFontOpenName(display, screen, body_font);
    if (!render->font_body) {
        render->font_body = render->font_title;
    }

    render->have_colours =
        XftColorAllocName(display, render->visual, render->colormap,
                          config->title, &render->colour_title) &&
        XftColorAllocName(display, render->visual, render->colormap,
                          config->body, &render->colour_body) &&
        XftColorAllocName(display, render->visual, render->colormap,
                          config->accent, &render->colour_accent);

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = render->background;
    attributes.event_mask = ExposureMask | ButtonPressMask |
                            PointerMotionMask | LeaveWindowMask;

    render->window = XCreateWindow(
        display, render->root, 0, 0,
        (unsigned int)config->width, 1, 0,
        render->depth, InputOutput, render->visual,
        CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);

    if (render->window == None) {
        notif_render_free(render);
        return -1;
    }
    XStoreName(display, render->window, "GnuChanNotification");
    return 0;
}

void notif_render_free(NotifRender *render) {
    if (!render || !render->display) {
        return;
    }
    if (render->buffer != None) {
        XFreePixmap(render->display, render->buffer);
        render->buffer = None;
    }
    if (render->window != None) {
        XDestroyWindow(render->display, render->window);
        render->window = None;
    }
    if (render->have_colours) {
        XftColorFree(render->display, render->visual, render->colormap,
                     &render->colour_title);
        XftColorFree(render->display, render->visual, render->colormap,
                     &render->colour_body);
        XftColorFree(render->display, render->visual, render->colormap,
                     &render->colour_accent);
        render->have_colours = 0;
    }
    if (render->font_body && render->font_body != render->font_title) {
        XftFontClose(render->display, render->font_body);
    }
    if (render->font_title) {
        XftFontClose(render->display, render->font_title);
    }
    if (render->gc) {
        XFreeGC(render->display, render->gc);
    }
    render->font_title = NULL;
    render->font_body = NULL;
    render->gc = NULL;
    render->display = NULL;
}

/* The width the text of a bubble gets: the bubble's width less the padding on
   both sides, and less the icon column when there is an icon. */
static int text_area_width(NotifRender *render, const NotifItem *item) {
    int width = render->config.width - render->config.padding * 2;
    if (render->config.show_icon && item->has_image) {
        width -= render->config.icon_size + render->config.padding;
    }
    if (width < 40) {
        width = 40;
    }
    return width;
}

/* How tall a bubble is, by breaking its text exactly as the drawing will. */
static int bubble_height(NotifRender *render, const NotifItem *item) {
    int width = text_area_width(render, item);
    TextLine lines[NOTIF_HARD_MAX_LINES];
    int height = 0;

    if (item->summary[0] && render->font_title) {
        int count = text_lines(render->display, render->font_title,
                               item->summary, width,
                               render->config.max_summary_lines, lines);
        height += count * line_height(render->font_title);
    }
    if (render->config.show_body && item->body[0] && render->font_body) {
        if (height > 0) {
            height += NOTIF_LINE_GAP;
        }
        int count = text_lines(render->display, render->font_body, item->body,
                               width, render->config.max_body_lines, lines);
        height += count * line_height(render->font_body);
    }

    int plate = render->config.show_icon && item->has_image
                    ? render->config.icon_size
                    : 0;
    if (height < plate) {
        height = plate;
    }
    height += render->config.padding * 2;
    return height;
}

void notif_render_place(NotifRender *render) {
    if (!render || render->window == None) {
        return;
    }
    int screen_width = DisplayWidth(render->display, render->screen);
    int screen_height = DisplayHeight(render->display, render->screen);
    int margin = render->config.margin;
    const char *position = render->config.position;

    int x = screen_width - render->window_width - margin;
    int y = render->config.top_offset;

    if (strstr(position, "left")) {
        x = margin;
    } else if (strstr(position, "center")) {
        x = (screen_width - render->window_width) / 2;
    }
    if (strstr(position, "bottom")) {
        y = screen_height - render->window_height - margin;
    }

    XMoveResizeWindow(render->display, render->window, x, y,
                      (unsigned int)render->window_width,
                      (unsigned int)render->window_height);
}

void notif_render_layout(NotifRender *render, NotifStack *stack) {
    if (!render || !stack) {
        return;
    }
    if (stack->count == 0) {
        render->window_width = render->config.width;
        render->window_height = 1;
        XUnmapWindow(render->display, render->window);
        return;
    }

    int width = render->config.width;
    int y = render->config.margin;
    for (int i = 0; i < stack->count; i++) {
        NotifItem *item = &stack->items[i];
        int height = bubble_height(render, item);
        item->x = render->config.margin;
        item->y = y;
        item->width = width - render->config.margin * 2;
        item->height = height;
        y += height + render->config.gap;
    }
    y -= render->config.gap;
    y += render->config.margin;

    render->window_width = width;
    render->window_height = y;

    if (render->buffer != None &&
        (render->buffer_width != render->window_width ||
         render->buffer_height != render->window_height)) {
        XFreePixmap(render->display, render->buffer);
        render->buffer = None;
    }
    if (render->buffer == None) {
        render->buffer = XCreatePixmap(render->display, render->window,
                                       (unsigned int)render->window_width,
                                       (unsigned int)render->window_height,
                                       (unsigned int)render->depth);
        render->buffer_width = render->window_width;
        render->buffer_height = render->window_height;
    }

    notif_render_place(render);
    notif_shape_round(render->display, render->window, render->window_width,
                      render->window_height, render->config.corner);
}

/* One string, broken and drawn from `top`, returning the y the next block
   starts at. It breaks the text with text_lines(), the very function the
   height was measured with, so what is drawn is what was measured. */
static int draw_text_block(NotifRender *render, XftDraw *draw,
                           XftColor *colour, XftFont *font, const char *text,
                           int x, int top, int width, int max_lines) {
    if (!font || !text || !text[0] || !draw) {
        return top;
    }
    TextLine lines[NOTIF_HARD_MAX_LINES];
    int count = text_lines(render->display, font, text, width, max_lines,
                           lines);
    int baseline = top + font->ascent;
    int step = line_height(font);
    for (int i = 0; i < count; i++) {
        if (lines[i].length > 0 && colour) {
            XftDrawStringUtf8(draw, colour, font, x, baseline,
                              (const FcChar8 *)(text + lines[i].start),
                              lines[i].length);
        }
        baseline += step;
    }
    return top + count * step;
}

/* The countdown bar: a thin line along the bubble's bottom edge, filled from
   the left in proportion to how much of the bubble's time is left. A bubble
   that will never expire has no bar. */
static void draw_timer_bar(NotifRender *render, const NotifItem *item) {
    if (!render->config.show_timer || item->never_expire ||
        item->timeout_ms <= 0) {
        return;
    }
    long long elapsed = render->now_ms - item->born_ms;
    if (elapsed < 0) {
        elapsed = 0;
    }
    if (elapsed > item->timeout_ms) {
        elapsed = item->timeout_ms;
    }
    int full = item->width - 2;
    if (full <= 0) {
        return;
    }
    int left = (int)((long long)full * (item->timeout_ms - elapsed) /
                     item->timeout_ms);
    if (left < 0) {
        left = 0;
    }

    Display *display = render->display;
    GC gc = render->gc;
    int y = item->y + item->height - NOTIF_TIMER_HEIGHT - 1;
    XSetForeground(display, gc, render->background_alt);
    XFillRectangle(display, render->buffer, gc, item->x + 1, y,
                   (unsigned int)full, NOTIF_TIMER_HEIGHT);
    XSetForeground(display, gc, render->accent);
    XFillRectangle(display, render->buffer, gc, item->x + 1, y,
                   (unsigned int)left, NOTIF_TIMER_HEIGHT);
}

static void draw_bubble(NotifRender *render, XftDraw *draw,
                        const NotifItem *item) {
    Display *display = render->display;
    GC gc = render->gc;
    int x = item->x;
    int y = item->y;
    int width = item->width;
    int height = item->height;
    int padding = render->config.padding;
    int corner = render->config.corner;

    unsigned long frame_colour =
        item->urgency == NOTIF_URGENCY_CRITICAL ? render->urgent : render->frame;

    XSetForeground(display, gc, render->background);
    notif_shape_fill(display, render->buffer, gc, x, y, width, height, corner);
    XSetForeground(display, gc, frame_colour);
    notif_shape_outline(display, render->buffer, gc, x, y, width - 1,
                        height - 1, corner, 2);

    int content_x = x + padding;
    int content_width = width - padding * 2;

    if (render->config.show_icon && item->has_image) {
        int side = render->config.icon_size;
        int plate_x = x + padding;
        int plate_y = y + padding;
        XSetForeground(display, gc, render->icon_background);
        notif_shape_fill(display, render->buffer, gc, plate_x, plate_y, side,
                         side, corner / 2 + 2);
        notif_image_draw(display, gc, &item->image, render->buffer, plate_x,
                         plate_y);
        content_x = plate_x + side + padding;
        content_width = width - (content_x - x) - padding;
    }
    if (content_width < 20) {
        content_width = 20;
    }

    int top = y + padding;
    if (item->summary[0] && render->font_title) {
        top = draw_text_block(render, draw,
                              render->have_colours ? &render->colour_title
                                                   : NULL,
                              render->font_title, item->summary, content_x, top,
                              content_width, render->config.max_summary_lines);
    }
    if (render->config.show_body && item->body[0] && render->font_body) {
        if (item->summary[0]) {
            top += NOTIF_LINE_GAP;
        }
        draw_text_block(render, draw,
                        render->have_colours ? &render->colour_body : NULL,
                        render->font_body, item->body, content_x, top,
                        content_width, render->config.max_body_lines);
    }

    draw_timer_bar(render, item);
}

void notif_render_draw(NotifRender *render, NotifStack *stack) {
    if (!render || !stack || render->window == None || render->buffer == None) {
        return;
    }
    Display *display = render->display;
    GC gc = render->gc;

    /* Nothing to show: the window goes away and NOTHING is mapped. This is the
       case that left an empty panel on screen — the last bubble expired, the
       layout unmapped the window, and then this function mapped it again to
       paint an empty stack. An empty stack is taken down here and the drawing
       ends. */
    if (stack->count == 0) {
        XUnmapWindow(display, render->window);
        XFlush(display);
        return;
    }

    /* The window goes up BEFORE it is painted: a window mapped after it was
       painted is filled with its own background colour on the way up, and that
       fill wipes the paint already in it. */
    XMapRaised(display, render->window);

    XSetForeground(display, gc, render->background);
    XFillRectangle(display, render->buffer, gc, 0, 0,
                   (unsigned int)render->window_width,
                   (unsigned int)render->window_height);

    XftDraw *draw = XftDrawCreate(display, render->buffer, render->visual,
                                  render->colormap);
    for (int i = 0; i < stack->count; i++) {
        draw_bubble(render, draw, &stack->items[i]);
    }
    if (draw) {
        XftDrawDestroy(draw);
    }

    XCopyArea(display, render->buffer, render->window, gc, 0, 0,
              (unsigned int)render->window_width,
              (unsigned int)render->window_height, 0, 0);

    XFlush(display);
}
