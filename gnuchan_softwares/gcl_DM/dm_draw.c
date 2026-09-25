/*
 * dm_draw.c — the drawing primitives the greeter is built from.
 *
 * Every primitive draws into the core's off-screen buffer, never the window.
 * The core puts the finished buffer on the window once, in dm_core_redraw().
 * Drawing to the window here would defeat that and bring the flicker back.
 */
#include <string.h>

#include "dm_core.h"

void dm_draw_clear(DmCore *core, unsigned long colour) {
    XSetForeground(core->display, core->gc, colour);
    XFillRectangle(core->display, core->buffer, core->gc,
                   0, 0, (unsigned int)core->width, (unsigned int)core->height);
}

void dm_draw_box(DmCore *core, int x, int y, int width, int height,
                 unsigned long fill, unsigned long edge, int edge_width) {
    XSetForeground(core->display, core->gc, fill);
    XFillRectangle(core->display, core->buffer, core->gc,
                   x, y, (unsigned int)width, (unsigned int)height);

    if (edge_width <= 0) {
        return;
    }
    XSetForeground(core->display, core->gc, edge);
    XFillRectangle(core->display, core->buffer, core->gc,
                   x, y, (unsigned int)width, (unsigned int)edge_width);
    XFillRectangle(core->display, core->buffer, core->gc,
                   x, y + height - edge_width,
                   (unsigned int)width, (unsigned int)edge_width);
    XFillRectangle(core->display, core->buffer, core->gc,
                   x, y, (unsigned int)edge_width, (unsigned int)height);
    XFillRectangle(core->display, core->buffer, core->gc,
                   x + width - edge_width, y,
                   (unsigned int)edge_width, (unsigned int)height);
}

int dm_draw_text_width(XFontStruct *font, const char *text, int length) {
    if (!font || !text || length <= 0) {
        return 0;
    }
    return XTextWidth(font, text, length);
}

void dm_draw_text(DmCore *core, int x, int y, const char *text,
                  XFontStruct *font, unsigned long colour) {
    if (!text || !font) {
        return;
    }
    XSetFont(core->display, core->gc, font->fid);
    XSetForeground(core->display, core->gc, colour);
    XDrawString(core->display, core->buffer, core->gc,
                x, y, text, (int)strlen(text));
}
