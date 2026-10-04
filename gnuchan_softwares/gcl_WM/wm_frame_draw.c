/*
 * wm_frame_draw.c — the title bar and the border, drawn into a copy and put up
 * in one operation.
 *
 * This is the whole of what a frame looks like: the band that is the border,
 * the bar above the client, the line under it that carries the focus colour,
 * the title text and the three buttons. It was the drawing half of wm_frame.c
 * and it is its own file because it touches NOTHING but the frame, the style
 * and the compositor — it opens no window, answers no event and keeps no state
 * of its own — so a change to the way a window looks is a change to one file
 * that cannot move a window or swallow a click.
 *
 * Everything is drawn into an off-screen copy (the frame's buffer) and copied
 * onto the window in one XCopyArea. Drawing straight to the window means the
 * bar is cleared, then the title is put in, then the buttons and the outline,
 * and while a window is dragged or its focus is changing that order is
 * repeated many times a second — which is seen as a flicker. One copy per
 * redraw is what removes it.
 *
 * The shape and the size of the chrome are not decided here: frame_width(),
 * frame_height(), frame_border_of(), frame_title_of(), button_x() and
 * button_y() all come from wm_frame_geometry.c, so the bar drawn here and the
 * geometry every other part of the frame works from cannot disagree.
 */
#include <stdio.h>
#include <string.h>

#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_frame.h"
#include "wm_frame_geometry.h"

/* --- drawing -------------------------------------------------------------- */

/* Shorten text until it fits, ending it with an ellipsis. A window title can
   be any length and a bar is a fixed width, so the title has to give. */
static void frame_clip(Display *display, XftFont *font, const char *text,
                       int room, char *out, unsigned int size) {
    out[0] = '\0';
    if (!display || !font || !text || room <= 0) {
        return;
    }
    if (wm_style_text_width(display, font, text) <= room) {
        snprintf(out, size, "%s", text);
        return;
    }
    /* Cut at character boundaries but measure whole characters: a UTF-8 string
       cut mid-character is an invalid sequence, and Xft draws its pieces as
       nothing at all. Walking back a byte at a time and measuring what is left
       stops at the first attempt that fits, and the extra bytes of a character
       are skipped rather than counted — see the continuation test below. */
    size_t length = strlen(text);
    while (length > 0) {
        length--;
        /* A continuation byte (10xxxxxx) is the middle of a character, so it is
           not a place a string may end. */
        if (((unsigned char)text[length] & 0xc0) == 0x80) {
            continue;
        }
        char attempt[192];
        snprintf(attempt, sizeof(attempt), "%.*s...", (int)length, text);
        if (wm_style_text_width(display, font, attempt) <= room) {
            snprintf(out, size, "%s", attempt);
            return;
        }
    }
    snprintf(out, size, "...");
}

/* The copy the bar is drawn into, made to the frame's current size. A copy of
   the wrong size is thrown away rather than stretched, because a stretched
   title bar is a blur, not a title bar. */
static void frame_ensure_buffer(WmCore *core, WmFrame *frame,
                                int width, int height) {
    if (frame->buffer != None &&
        frame->buffer_width == width && frame->buffer_height == height) {
        return;
    }
    if (frame->buffer != None) {
        XFreePixmap(core->display, frame->buffer);
        frame->buffer = None;
    }
    frame->buffer = XCreatePixmap(core->display, frame->frame,
                                  (unsigned int)width, (unsigned int)height,
                                  (unsigned int)DefaultDepth(core->display,
                                                             core->screen));
    frame->buffer_width = width;
    frame->buffer_height = height;
}

/* One button's glyph. The three are drawn from the same box so the bar reads
   as one row of controls rather than three decorations. */
static void frame_draw_button(WmCore *core, Drawable target, int x, int y,
                              WmFrameButton button, int maximized) {
    Display *display = core->display;
    const WmStyle *style = &core->style;
    int inset = 4;

    XSetForeground(display, core->gc, style->field);
    XFillRectangle(display, target, core->gc, x, y,
                   WM_BUTTON_SIZE, WM_BUTTON_SIZE);
    XSetForeground(display, core->gc, style->accent);
    XDrawRectangle(display, target, core->gc, x, y,
                   WM_BUTTON_SIZE - 1, WM_BUTTON_SIZE - 1);

    switch (button) {
    case WM_BUTTON_CLOSE:
        XDrawLine(display, target, core->gc,
                  x + inset, y + inset,
                  x + WM_BUTTON_SIZE - inset, y + WM_BUTTON_SIZE - inset);
        XDrawLine(display, target, core->gc,
                  x + WM_BUTTON_SIZE - inset, y + inset,
                  x + inset, y + WM_BUTTON_SIZE - inset);
        break;

    case WM_BUTTON_MAXIMIZE:
        /* One box while the window is not maximised, two while it is: the glyph
           says what the button will do, which is put it back. */
        XDrawRectangle(display, target, core->gc,
                       x + inset, y + inset,
                       WM_BUTTON_SIZE - 2 * inset, WM_BUTTON_SIZE - 2 * inset);
        if (maximized) {
            XDrawRectangle(display, target, core->gc,
                           x + inset + 2, y + inset - 2,
                           WM_BUTTON_SIZE - 2 * inset,
                           WM_BUTTON_SIZE - 2 * inset);
        }
        break;

    case WM_BUTTON_MINIMIZE:
        XDrawLine(display, target, core->gc,
                  x + inset, y + WM_BUTTON_SIZE - inset,
                  x + WM_BUTTON_SIZE - inset, y + WM_BUTTON_SIZE - inset);
        break;

    default:
        break;
    }
}

void wm_frame_draw(WmCore *core, WmFrame *frame) {
    if (frame->minimized || frame->frame == None) {
        return;
    }

    Display *display = core->display;
    const WmStyle *style = &core->style;
    int width = frame_width(frame);
    int height = frame_height(frame);
    int focused = (core->focused == frame->client);

    /* Three colours, in an order that says what matters: the keyboard first,
       then the mark, then the ordinary unfocused window.
     *
     *   focused          the window the user is typing in, which is the one
     *                    thing that must never be ambiguous
     *   moved            a window that has just been sent to another desk and
     *                    not looked at since — see WmFrame.moved
     *   else             a window that is neither
     *
     * A window cannot be both focused and marked: focusing it is what clears
     * the mark (wm_focus_set), because focusing it is the act of having found
     * it again. The test is written the other way round anyway — focused
     * first — so that a frame whose mark has not been cleared yet still shows
     * the colour the user is looking for. */
    unsigned long border_colour =
        focused ? style->border
                : (frame->moved ? style->border_moved
                                : style->border_unfocused);

    /* The chrome is drawn for EVERY window, a fullscreen-like one included —
       see frame_border_of() for why. There is no branch on the fullscreen flag
       here at all, and that is on purpose: a fullscreen window is an ordinary
       window that happens to fill the screen, so it gets the same border, the
       same bar and the same buttons as every other. The client's picture is
       drawn into the frame's client area lower down when the compositor owns
       it, exactly as on any other window. */
    frame_ensure_buffer(core, frame, width, height);
    Drawable target = frame->buffer != None ? frame->buffer : frame->frame;

    /* The border, painted as a filled band across the whole frame in the focus
       colour the script named. It is the band the client is inset by: the
       client sits at (frame->border, WM_TITLE_HEIGHT) inside a frame that is
       2*border wider and border taller, so the room left uncovered on the
       left, right and bottom is exactly this band - and this band is the
       border the user sees. The bar and the client are drawn over it, which
       leaves it visible on the three sides the client does not reach.

       Painting the whole frame before anything else is also what keeps a
       resize clean. frame_ensure_buffer makes a fresh pixmap whose contents
       are undefined, so any pixel the drawing below does not reach would show
       whatever the server left there - which is the stray line that appeared
       while only a one-pixel outline was drawn and the band beside it was left
       untouched. No pixel of the frame is left to the server now. */
    XSetForeground(display, core->gc, border_colour);
    XFillRectangle(display, target, core->gc, 0, 0,
                   (unsigned int)width, (unsigned int)height);

    /* The window's content, when it is being drawn scaled. It goes over the
       border band exactly where the client sits — the same rectangle
       frame_apply() puts the client in — so the picture and the border agree
       about where the window is. When the compositor has nothing to draw the
       band is left showing, which is the frame's own colour rather than a
       hole.

       This is here and not in the XCopyArea below because the copy is of the
       buffer onto the window: the content has to be in the buffer to be
       copied, and this is where the buffer is filled. */
    if (frame->scaled) {
        wm_compositor_draw(core, frame->client, target,
                           frame->border, WM_TITLE_HEIGHT,
                           frame->client_width, frame->client_height);
    }

    /* The bar. */
    XSetForeground(display, core->gc, style->panel);
    XFillRectangle(display, target, core->gc, 0, 0,
                   (unsigned int)width, WM_TITLE_HEIGHT);

    /* The line under it that says which window is active. It follows the band
       above — the same three cases, the same colour — so a frame reads as one
       mark rather than a coloured border with an unrelated stripe in it. The
       band alone is two pixels and is the first thing lost against a busy
       window; the stripe is what carries the colour across the title bar. */
    XSetForeground(display, core->gc, border_colour);
    XFillRectangle(display, target, core->gc, 0, WM_TITLE_HEIGHT - 2,
                   (unsigned int)width, 2);

    /* The title, given the room the buttons leave. */
    int padding = 8;
    int room = button_x(frame, WM_BUTTON_MINIMIZE) - padding - 8;
    if (room < 12) {
        room = 12;
    }
    char label[192];
    frame_clip(display, style->font, frame->name, room, label, sizeof(label));
    int ascent = style->font ? style->font->ascent : 8;
    int descent = style->font ? style->font->descent : 2;
    int baseline = (WM_TITLE_HEIGHT + ascent - descent) / 2;
    wm_style_text(display, core->screen, target, style->font, padding,
                  baseline, label, style->text);

    /* The buttons. */
    for (int i = 0; i < WM_BUTTON_COUNT; i++) {
        frame_draw_button(core, target, button_x(frame, (WmFrameButton)i),
                          button_y(), (WmFrameButton)i, frame->maximized);
    }

    /* No outline is drawn here. The border is the filled band painted at the
       top of this function, so a rectangle around the edge as well would be
       the same colour drawn twice - and while the band was left unpainted and
       only this rectangle was drawn, it was the second border: a one-pixel
       line around a frame whose own border pixels were never painted. The
       width the script asked for and the width of the border are now the same
       thing, because the band painted above is frame->border thick. */

    /* Onto the screen in one operation, which is the whole point of the
       copy above. */
    if (target != frame->frame) {
        XCopyArea(display, frame->buffer, frame->frame, core->gc,
                  0, 0, (unsigned int)width, (unsigned int)height, 0, 0);
    }
    XFlush(display);
}

void wm_frame_draw_all(WmCore *core) {
    for (int i = 0; i < core->frame_count; i++) {
        wm_frame_draw(core, &core->frames[i]);
    }
}
