/*
 * wm_frame.c — the frame a managed window lives in, and everything done to it.
 *
 * The shape of a frame is fixed and simple:
 *
 *     +--------------------------------------+
 *     | title          [-] [ ] [x]           |  WM_TITLE_HEIGHT
 *     +--------------------------------------+
 *     |                                      |
 *     |            the client                |  client_height
 *     |                                      |
 *     +--------------------------------------+
 *
 * The title bar is drawn on the frame itself rather than on a window of its
 * own. One window means one place to click, one exposure to answer and one
 * thing to move, and a bar that is 22 pixels tall needs nothing more.
 *
 * The client is placed at (WM_FRAME_BORDER, WM_TITLE_HEIGHT) inside the frame
 * and is never told about the bar: reparenting is what makes that work. The
 * program still believes it is a window on the root, because the X server
 * translates its coordinates, and every window manager relies on that.
 *
 * Drawing goes into an off-screen copy of the bar and is copied onto the window
 * in one operation. Drawing straight to the window means the bar is cleared,
 * then the title is put in, then the buttons and the outline — and while a
 * window is being dragged or its focus is changing, that whole order is
 * repeated many times a second. Each repeat is visible as a flicker. One copy
 * per redraw is what removes it, and a drag does not redraw at all: the window
 * moves and the picture inside it moves with it. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_desktop.h"
#include "wm_frame.h"
#include "wm_workspace.h"

/* --- geometry -------------------------------------------------------------
 * All of it is computed from the client's size, so there is one place that
 * decides what a frame looks like and every other function asks it. */

/* The chrome drawn around the client: the border on the left, right and
   bottom, and the title bar above.
 *
 * BOTH ARE ZERO WHILE THE WINDOW IS FULLSCREEN, and that single fact is why
 * these exist as functions rather than as the two constants they used to be.
 * A fullscreen window has no chrome — that is what fullscreen means — so every
 * rule below has to read "22" as zero for it. A hand-written WM_TITLE_HEIGHT
 * in one of those rules is a title bar's worth of offset applied to a window
 * that has no title bar, which puts the picture 22 pixels below the top of a
 * window that is exactly as tall as the screen: the bottom 22 lines of the
 * game are then off the screen, and a Direct3D game that notices moves itself
 * up by 22 to compensate, and the two fight for ever. That is the picture that
 * slides up and down, and this is where it is stopped.
 *
 * The border is read from the frame and not from the style because the style
 * may still say two pixels while a frame is fullscreen. */
static int frame_border_of(const WmFrame *frame) {
    return frame->fullscreen ? 0 : frame->border;
}

static int frame_title_of(const WmFrame *frame) {
    return frame->fullscreen ? 0 : WM_TITLE_HEIGHT;
}

static int frame_width(const WmFrame *frame) {
    return frame->client_width + 2 * frame_border_of(frame);
}

static int frame_height(const WmFrame *frame) {
    return frame->client_height + frame_title_of(frame) + frame_border_of(frame);
}

/* The border width the desktop's style asks for, held inside what may be
   drawn. It is the one place the clamping happens, so a frame made now and a
   frame adjusted after a reload get the same answer. */
static int style_border_width(const WmCore *core) {
    int width = core->style.border_width;
    if (width < 1) {
        width = WM_FRAME_BORDER;
    }
    if (width > WM_FRAME_BORDER_MAX) {
        width = WM_FRAME_BORDER_MAX;
    }
    return width;
}

/* Where a button sits inside the bar. They are laid out from the right edge
   inwards in the order of WmFrameButton, so close is rightmost — the corner a
   hand already expects — and every consumer reads the same rule. */
static int button_x(const WmFrame *frame, WmFrameButton button) {
    int step = WM_BUTTON_SIZE + WM_BUTTON_GAP;
    return frame_width(frame) - WM_BUTTON_SIZE - WM_BUTTON_GAP
           - (int)button * step;
}

static int button_y(void) {
    return (WM_TITLE_HEIGHT - WM_BUTTON_SIZE) / 2;
}

/* Which button a point on the bar is inside, or WM_BUTTON_COUNT for none. */
static WmFrameButton button_at(const WmFrame *frame, int x, int y) {
    for (int i = 0; i < WM_BUTTON_COUNT; i++) {
        int bx = button_x(frame, (WmFrameButton)i);
        int by = button_y();
        if (x >= bx && x < bx + WM_BUTTON_SIZE &&
            y >= by && y < by + WM_BUTTON_SIZE) {
            return (WmFrameButton)i;
        }
    }
    return WM_BUTTON_COUNT;
}

/* --- the table ------------------------------------------------------------ */

WmFrame *wm_frame_find(WmCore *core, Window client) {
    if (client == None) {
        return NULL;
    }
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].client == client) {
            return &core->frames[i];
        }
    }
    return NULL;
}

WmFrame *wm_frame_find_by_frame(WmCore *core, Window frame) {
    if (frame == None) {
        return NULL;
    }
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].frame == frame) {
            return &core->frames[i];
        }
    }
    return NULL;
}

/* --- naming --------------------------------------------------------------- */

/* A window's name, from the two places one is written. _NET_WM_NAME is UTF-8
   and is what every toolkit sets today; WM_NAME is the older, Latin-1 one and
   is what a bare X program such as xterm still uses. Reading both is what
   makes the bar say "xterm" rather than "window". */
static void frame_read_name(WmCore *core, WmFrame *frame) {
    frame->name[0] = '\0';
    frame->has_name = 0;

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    Atom utf8 = wm_atom(core, "UTF8_STRING");

    if (XGetWindowProperty(core->display, frame->client, core->net_wm_name,
                           0, 1024, False, utf8, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_type == utf8 && actual_format == 8 && items > 0) {
            snprintf(frame->name, sizeof(frame->name), "%.*s", (int)items,
                     (char *)data);
            frame->has_name = frame->name[0] != '\0';
        }
        if (data) {
            XFree(data);
        }
    }

    if (!frame->has_name) {
        char *legacy = NULL;
        if (XFetchName(core->display, frame->client, &legacy) && legacy) {
            snprintf(frame->name, sizeof(frame->name), "%s", legacy);
            frame->has_name = frame->name[0] != '\0';
            XFree(legacy);
        }
    }

    if (!frame->has_name) {
        snprintf(frame->name, sizeof(frame->name), "window");
    }
}

void wm_frame_update_name(WmCore *core, WmFrame *frame) {
    char previous[sizeof(frame->name)];
    snprintf(previous, sizeof(previous), "%s", frame->name);
    frame_read_name(core, frame);
    if (strcmp(previous, frame->name) != 0) {
        wm_frame_draw(core, frame);
    }
}

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

    /* A fullscreen window has no chrome, so there is nothing for the manager
       to draw: the frame is exactly the size of the client and the client
       covers it. The title bar and the border are dropped here rather than
       drawn and then covered by the program's own pixels, which is what keeps
       a strip of title bar from flashing across the top of a game that is
       repainting thirty times a second.

       The one thing that can still be drawn is the compositor's picture of a
       SCALED window, which the client does not paint itself; that path is kept
       and everything else returns. */
    if (frame->fullscreen) {
        if (frame->scaled) {
            frame_ensure_buffer(core, frame, width, height);
            Drawable scaled_target =
                frame->buffer != None ? frame->buffer : frame->frame;
            XSetForeground(display, core->gc, style->panel);
            XFillRectangle(display, scaled_target, core->gc, 0, 0,
                           (unsigned int)width, (unsigned int)height);
            wm_compositor_draw(core, frame->client, scaled_target, 0, 0,
                               frame->client_width, frame->client_height);
            if (scaled_target != frame->frame) {
                XCopyArea(display, frame->buffer, frame->frame, core->gc,
                          0, 0, (unsigned int)width, (unsigned int)height,
                          0, 0);
            }
            XFlush(display);
        }
        return;
    }

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

/* --- geometry, applied ---------------------------------------------------- */

/* Tell the client what it actually got. A reparenting window manager is
   required to do this: the client asked for a position and a size and the
   manager gave it a different position, and a toolkit that watches for the
   change draws nothing until it hears about it.
 *
 * A scaled window is not told anything, and that is not an omission: it was
 * never resized. Telling it a size it does not have — the frame's, smaller
 * than the client — would make a program that DOES listen resize itself to fit
 * a picture that is already being made to fit it, and the two would fight.
 * The client keeps believing it is the size it asked for, which is true. */
static void frame_notify_configure(WmCore *core, WmFrame *frame) {
    XEvent event;

    if (frame->scaled) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.xconfigure.type = ConfigureNotify;
    event.xconfigure.display = core->display;
    event.xconfigure.event = frame->client;
    event.xconfigure.window = frame->client;
    event.xconfigure.x = frame_border_of(frame);
    event.xconfigure.y = frame_title_of(frame);
    event.xconfigure.width = frame->client_width;
    event.xconfigure.height = frame->client_height;
    event.xconfigure.border_width = 0;
    event.xconfigure.above = None;
    event.xconfigure.override_redirect = False;
    XSendEvent(core->display, frame->client, False, StructureNotifyMask, &event);
}

/* Put everything where the frame's own numbers say it goes. Every change goes
   through here, so the frame window, the client inside it and the bar drawn on
   it cannot disagree. */
static void frame_apply(WmCore *core, WmFrame *frame) {
    XMoveResizeWindow(core->display, frame->frame, frame->x, frame->y,
                      (unsigned int)frame_width(frame),
                      (unsigned int)frame_height(frame));

    /* A scaled window is put back at its corner and NOT resized. Its content
       is drawn at the size its program chose and fitted to the frame, so
       resizing it here would be resizing the very thing being scaled — and
       the whole problem is that the program ignores the resize and draws at
       its own size anyway. Moving it is all that is left to do: the picture
       of it is then drawn into the frame by wm_frame_draw. */
    if (frame->scaled) {
        XMoveWindow(core->display, frame->client,
                    frame_border_of(frame), frame_title_of(frame));
    } else {
        XMoveResizeWindow(core->display, frame->client,
                          frame_border_of(frame), frame_title_of(frame),
                          (unsigned int)frame->client_width,
                          (unsigned int)frame->client_height);
    }
    wm_frame_draw(core, frame);
}

/* Put every open frame's border at the width the desktop's style now says.
 *
 * A script that changed set_window_border_width() has changed a number the
 * geometry of every frame is computed from, so each frame is told the new
 * width and put back together: the frame window is resized, the client inside
 * it is moved, and the border is drawn again. Frames that are not open are
 * skipped, and a screen with none is a no-op. */
void wm_frame_apply_border(WmCore *core) {
    int width = style_border_width(core);
    core->style.border_width = width;

    for (int i = 0; i < core->frame_count; i++) {
        WmFrame *frame = &core->frames[i];

        /* A frame that is put away has no pixels to rearrange; it is given
           the new width and drawn when it comes back. */
        if (frame->minimized) {
            frame->border = width;
            continue;
        }

        if (frame->border == width) {
            continue;
        }

        frame->border = width;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
    }
    XFlush(core->display);
}

void wm_frame_move(WmCore *core, WmFrame *frame, int x, int y) {
    frame->x = x;
    frame->y = y;

    /* The window is moved and nothing is redrawn. The bar's picture is inside
       the window and moves with it, so the pixels are already right — and
       clearing or repainting them here is what used to flash on every motion
       event of a drag. */
    XMoveWindow(core->display, frame->frame, x, y);
    XFlush(core->display);
}

void wm_frame_raise(WmCore *core, WmFrame *frame) {
    XRaiseWindow(core->display, frame->frame);
    /* The bar is left where it is, below the windows. It used to be put back
       on top from here, and that is exactly what trapped a window dragged onto
       the bar: the bar's own raise arrived right after the window's and covered
       it again, so the window could never come out in front. The bar lives at
       the bottom of the stack now (see wm_desktop_lower_bar), so a window moved
       over it covers it and the bar is seen again the moment the window moves
       off. */
    XFlush(core->display);
}

/* ---------- size hints: the steps a client asks to be resized in ----------

   A terminal has no size in pixels; it has a grid, and the grid is whole
   character cells. It says so in WM_NORMAL_HINTS: a BASE size, which is the
   window's own chrome, and an INCREMENT, which is one cell. A window manager
   that ignores those numbers and gives the window whatever width the pointer
   asks for produces a size that is not a whole number of cells. The terminal
   draws the cells that fit and leaves the strip past the last one exactly as
   it was — and nothing the program can print reaches that strip, so no
   `clear` and no full-screen program ever repaints it. The text that stood
   there when the window had a different size then stays on the screen for the
   life of the window. Snapping to the hints is what keeps the window a whole
   number of cells.

   The hints are read from the client on every motion rather than cached: a
   program may change them at any time — a terminal does when its font
   changes — and one round trip during a drag is nothing beside the resize
   itself.

   These live here, above their first caller: the drag (frame_resize_drag) is
   the one path that snaps. It is not frame_clamp_to_workarea() any more - that
   one no longer decides a size at all, only a place. */
typedef struct {
    int base_width;
    int base_height;
    int width_inc;
    int height_inc;
    int min_width;
    int min_height;
} WmSizeHints;

static void frame_read_size_hints(WmCore *core, WmFrame *frame,
                                  WmSizeHints *hints) {
    XSizeHints raw;
    long supplied = 0;

    memset(hints, 0, sizeof(*hints));
    hints->width_inc = 1;
    hints->height_inc = 1;

    if (!XGetWMNormalHints(core->display, frame->client, &raw, &supplied)) {
        return;
    }

    /* The increments are counted FROM the base. A program that names a base
       gives it in PBaseSize; one that gives only a minimum means the minimum
       is the base, which is what the ICCCM says the minimum stands for. */
    if (supplied & PBaseSize) {
        hints->base_width = raw.base_width;
        hints->base_height = raw.base_height;
    } else if (supplied & PMinSize) {
        hints->base_width = raw.min_width;
        hints->base_height = raw.min_height;
    }
    if ((supplied & PResizeInc) && raw.width_inc > 0 && raw.height_inc > 0) {
        hints->width_inc = raw.width_inc;
        hints->height_inc = raw.height_inc;
    }
    if (supplied & PMinSize) {
        hints->min_width = raw.min_width;
        hints->min_height = raw.min_height;
    }
}

/* The size nearest to `value` that the hints allow. `base` is where the
   increments are counted from (0 for a program that named none — a multiple of
   the increment FROM ZERO is then the grid), `increment` is one step, and
   `minimum` is what the manager will not go below. A value already at or under
   the base is left alone: it is the smallest size the client describes and
   snapping it would round it below that. */
static int frame_hint_size(int value, int base, int increment, int minimum) {
    int steps;

    if (increment > 1 && value > base) {
        steps = (value - base + increment / 2) / increment;
        value = base + steps * increment;
    }
    if (value < minimum) {
        value = minimum;
    }
    return value;
}

/* Keep a frame's TITLE BAR reachable after the client asks to be resized.
 *
 * THE SIZE IS NOT TOUCHED — and that is the correction. This function used to
 * hold the client's size to the workarea, and it was wrong for exactly the
 * case it was written for. A program may ask to be bigger than the screen (the
 * raylib demo opens at 1600x900 on a 1366x768 display); shrinking the CLIENT
 * is not how a window manager answers that. A program that listens rearranges
 * itself and never notices, but a program that does NOT — a GL game, anything
 * drawing at a fixed size — goes on drawing at the size it chose, so the frame
 * shrank and the content did not: the right-hand side and the bottom of the
 * window were outside it and simply not there. The buttons were gone, the map
 * was gone. That is a window that looks broken, and only the scaling key
 * (wm_frame_toggle_scaling) could put it right.
 *
 * Every window manager lets a program open the size it asked for; the part
 * that does not fit is off the screen and the user moves the window to see it.
 * That is what this does now.
 *
 * What is still worth doing is keeping the window from being LOST: a frame
 * whose title bar is off the top or the left cannot be grabbed and dragged
 * back, so it is pulled in by as much as it takes. A window that is merely
 * larger than the screen is left where it is — there is no place to put it
 * where both edges are inside, and moving it would only hide more of it. */
static void frame_clamp_to_workarea(WmCore *core, WmFrame *frame) {
    /* A fullscreen window IS the screen: there is no place to pull it into, and
       pulling it would put its own edges off the display it is filling. This is
       also what keeps the bar — which a fullscreen window covers by design,
       the whole screen being the point — from shoving the window it is covering
       out of the way. */
    if (frame->fullscreen) {
        return;
    }

    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;

    wm_config_workarea(&core->config, screen_width, screen_height,
                       &area_x, &area_y, &area_width, &area_height);

    /* Pull the frame back so its right and bottom edges stay inside the area —
       but only when the frame is small enough to fit. A window larger than the
       screen has no position at which both edges are inside, and taking the
       overflow off its left would move it further off than it already was. */
    if (frame_width(frame) <= area_width &&
        frame->x + frame_width(frame) > area_x + area_width) {
        frame->x = area_x + area_width - frame_width(frame);
    }
    if (frame_height(frame) <= area_height &&
        frame->y + frame_height(frame) > area_y + area_height) {
        frame->y = area_y + area_height - frame_height(frame);
    }
    if (frame->x < area_x) {
        frame->x = area_x;
    }
    if (frame->y < area_y) {
        frame->y = area_y;
    }
}

/* The largest a client on this desktop may be, and the clamp that holds a
   frame to it. Both are defined with the workarea helpers further down and
   declared here, because the size paths above them are where they are needed
   first. */
static void frame_size_limits(WmCore *core, const WmFrame *frame,
                              int *max_width, int *max_height);
static void frame_clamp_size_to_screen(WmCore *core, WmFrame *frame);

void wm_frame_resize(WmCore *core, WmFrame *frame, int width, int height) {
    /* A scaled window's frame is the user's to size, not the client's. The
       client is being drawn at the size it chose and the frame is fitted to
       it, so a client that asks to be resized is asking for the one thing
       that would undo the scaling — its content would then be drawn at a size
       nobody is scaling from. The request is dropped here rather than acted
       on, and the window keeps the size the hand gave it. */
    if (frame->scaled) {
        return;
    }

    if (width > 1) {
        frame->client_width = width;
    }
    if (height > 1) {
        frame->client_height = height;
    }
    /* The size IS held to the desktop, and this is the one thing a window
       manager can do about a window larger than the screen. X keeps no pixels
       for the part that falls past the screen's edge and sends no Expose when
       that part is dragged back into view, so the edge of a window bigger than
       the desktop stays blank until the window is made smaller. A program that
       asks for more than fits is given the fit; a picture that genuinely wants
       the whole of itself is shown with the scaling key, which is what
       wm_frame_toggle_scaling() is for and which this clamp does not touch. */
    frame_clamp_size_to_screen(core, frame);
    frame_clamp_to_workarea(core, frame);
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

/* A client that moved or resized itself. Its position is meaningless inside a
   frame — the bar lives above it — so only the size is taken and the position
   is put back to where the frame wants it.
 *
 * The position has to be put back, and that is the whole reason this test is
 * not "the size changed". Once a client is reparented it is a GRANDCHILD of
 * the root, and the root's structure redirection only covers the root's own
 * children — so a move the program makes itself is not redirected and does not
 * arrive as a ConfigureRequest. It arrives here, as a ConfigureNotify, with
 * the client already sitting wherever it put itself INSIDE the frame. Only
 * checking the size let that stand: the client slid away from (border, title)
 * and nothing brought it back.
 *
 * That is exactly what a GLFW program does — raylib's SetWindowPosition calls
 * XMoveWindow — so the two courts of the pong demo placed themselves at their
 * requested SCREEN coordinates measured from the frame's corner instead: the
 * Lua court, at x=60 in an 804 pixel frame, drew its court out past its frame,
 * and the Python court at x=880 fell outside the frame completely, leaving its
 * frame showing nothing but the border colour.
 *
 * Nothing is redrawn when nothing changed, which is still what keeps a plain
 * restack from repainting the title bar: a client whose size and place are both
 * already right returns above. */
void wm_frame_sync(WmCore *core, WmFrame *frame) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, frame->client, &attributes)) {
        return;
    }

    /* A scaled window resizes itself as much as it likes: the content is drawn
       at whatever size the program chose, and that size is what is scaled FROM.
       So the client's new size is taken as the window's NATURAL size — the size
       its content is drawn at — and the frame is not touched. Its own picture
       is not thrown away here either: the compositor watches ConfigureNotify
       for the same client and drops the pixmap it can no longer use, which is
       where that belongs, because the pixmap is its to keep.

       Nothing is returned early even when neither moved nor resized is set for
       a scaled window, because the compositor still has to be asked to draw it
       again. */
    if (frame->scaled) {
        if (attributes.width > 1) {
            frame->natural_width = attributes.width;
        }
        if (attributes.height > 1) {
            frame->natural_height = attributes.height;
        }
        /* A move is not taken from a scaled window: it believes it is a window
           on the root, and the frame is where it actually is. Letting its own
           idea of where it is move the frame would let a program that draws
           wherever it likes drag its own title bar around. */
        wm_frame_draw(core, frame);
        return;
    }

    /* A fullscreen window is FIXED, and this is the other half of the fix in
       wm_frame_set_fullscreen(). A game that could not be told it had the
       screen falls back to placing itself every frame, and a manager that took
       that placement would move the frame off (0,0) and start the same
       up-and-down fight one level up — the client asking to be somewhere, the
       manager putting it back, for ever. So a fullscreen client's own idea of
       where it is is not read at all: the frame is put back to the whole screen
       and the client is put back inside it.

       The size it reports is still followed, because a game that changes its
       own resolution is asking the desktop to change with it, and that is not a
       placement — it is a size, and taking it is what makes the change
       stick. */
    if (frame->fullscreen) {
        int screen_width = core->width > 1 ? core->width
                                           : DisplayWidth(core->display,
                                                          core->screen);
        int screen_height = core->height > 1 ? core->height
                                             : DisplayHeight(core->display,
                                                             core->screen);
        /* Nothing is done when the three numbers are already right, and that
           check is what keeps this from being a loop rather than a fix. A
           client that keeps moving itself sends a ConfigureNotify per move,
           each one arrives here, and each frame_apply() that called
           XMoveResizeWindow on an unchanged size would be answered by the
           server with the client's own ConfigureNotify — which would arrive
           here. Writing only on a change ends it. */
        if (frame->x == 0 && frame->y == 0 &&
            frame->client_width == screen_width &&
            frame->client_height == screen_height) {
            return;
        }
        frame->x = 0;
        frame->y = 0;
        frame->client_width = screen_width;
        frame->client_height = screen_height;
        frame_apply(core, frame);
        return;
    }

    int moved = attributes.x != frame_border_of(frame) ||
                attributes.y != frame_title_of(frame);
    int resized = attributes.width != frame->client_width ||
                  attributes.height != frame->client_height;
    if (!moved && !resized) {
        return;
    }

    if (resized) {
        if (attributes.width > 1) {
            frame->client_width = attributes.width;
        }
        if (attributes.height > 1) {
            frame->client_height = attributes.height;
        }
        /* A window that resized ITSELF is held to the screen exactly as one
           whose resize the manager was asked for: it is the same window either
           way, and a window bigger than the screen has the same blank edge
           whichever path made it that size. */
        frame_clamp_size_to_screen(core, frame);
    }

    /* A move is not undone, it is TAKEN, and it is taken as a SCREEN place.
       The server reports the client's new place relative to its parent, which
       is the frame — but the program has no idea it is inside a frame. It
       believes it is a window on the root, so the coordinates it asked for are
       the coordinates it wanted ON THE SCREEN. So the frame is put where the
       client would then sit where it asked: the client's own place less the
       frame's chrome, which is exactly the (border, title) offset the client
       is drawn at inside the frame.
           frame_screen = client_asked - (border, title)
           client_screen = frame_screen + (border, title) = client_asked
       Reading it as a delta from where the client already was — the obvious
       first guess — does not survive a client that asks twice: every move is
       measured from the place the previous one was taken to, so the window
       walks across the screen by the sum of its own coordinates. raylib's
       SetWindowPosition is one XMoveWindow per call, and the pong demo calls
       it once per court, which is what made the two frames land on top of one
       another instead of at 60 and 880. */
    if (moved) {
        frame->x = attributes.x - frame_border_of(frame);
        frame->y = attributes.y - frame_title_of(frame);
    }

    /* A client that resized itself goes through the same place-only clamp a
       requested resize does: its size is its own, its title bar is kept
       reachable. */
    frame_clamp_to_workarea(core, frame);
    /* frame_apply moves the client back to (frame->border, WM_TITLE_HEIGHT)
       as well as resizing it, so the placement and the resize are one call. */
    frame_apply(core, frame);
}

/* The area a maximised window gets: the screen less the bar. Read from the
   core rather than from the frame, because the frame is what is being changed
   to match it, and taken through wm_config_workarea() so a maximised window
   and a newly opened one agree about where the desktop ends. Without this a
   maximised window covers the bar, and the bar is the one thing on this
   desktop that has to stay reachable — there is no panel behind it. */
static void frame_screen_size(WmCore *core, int *x, int *y,
                              int *width, int *height) {
    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);
    wm_config_workarea(&core->config, screen_width, screen_height,
                       x, y, width, height);
}

/* The largest a client on this desktop may be: the workarea less the frame's
   own chrome.
 *
 * A window bigger than this is a window X cannot show whole. The server keeps
 * no pixels for the part that falls past the screen's edge and raises no
 * Expose when that part is dragged back into view, so the edge of the window
 * stays blank until it is made smaller or scaled. This is the failure the
 * limit exists to prevent, and it is asked for on every path that settles on a
 * size, so no window can end up larger than the screen it is on. */
static void frame_size_limits(WmCore *core, const WmFrame *frame,
                              int *max_width, int *max_height) {
    /* A fullscreen window is limited by the SCREEN, not by the workarea. The
       workarea is the screen less every bar, and a fullscreen window is meant
       to cover the bar — that is what the whole screen means. Taking the
       workarea here would hold a fullscreen game to the height of the desktop
       above the bar and leave the bar's own strip undrawn at the bottom of a
       picture the game believes fills the display: exactly the mismatch the
       game then corrects for itself, which is the fight this file is about. */
    int area_width;
    int area_height;
    int area_x = 0;
    int area_y = 0;

    if (frame->fullscreen) {
        area_width = core->width > 1 ? core->width
                                     : DisplayWidth(core->display,
                                                    core->screen);
        area_height = core->height > 1 ? core->height
                                       : DisplayHeight(core->display,
                                                       core->screen);
    } else {
        frame_screen_size(core, &area_x, &area_y, &area_width, &area_height);
        /* Only the extent is wanted; the corner is what
           frame_clamp_to_workarea() deals with. */
        (void)area_x;
        (void)area_y;
    }

    int wide = area_width - 2 * frame_border_of(frame);
    int tall = area_height - frame_title_of(frame) - frame_border_of(frame);
    *max_width = wide > 1 ? wide : 1;
    *max_height = tall > 1 ? tall : 1;
}

static void frame_clamp_size_to_screen(WmCore *core, WmFrame *frame) {
    int max_width = 0;
    int max_height = 0;

    frame_size_limits(core, frame, &max_width, &max_height);
    if (frame->client_width > max_width) {
        frame->client_width = max_width;
    }
    if (frame->client_height > max_height) {
        frame->client_height = max_height;
    }
}

void wm_frame_maximize(WmCore *core, WmFrame *frame) {
    if (frame->maximized) {
        /* The same button puts it back where it was. */
        frame->maximized = 0;
        frame->x = frame->restore_x;
        frame->y = frame->restore_y;
        frame->client_width = frame->restore_width;
        frame->client_height = frame->restore_height;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
        return;
    }

    frame->restore_x = frame->x;
    frame->restore_y = frame->y;
    frame->restore_width = frame->client_width;
    frame->restore_height = frame->client_height;

    /* The workarea, not the screen: a maximised window stops at the bar. Its
       client still has the title bar above it, so the client's height gives
       back the title bar's room as well. */
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    frame_screen_size(core, &area_x, &area_y, &area_width, &area_height);

    frame->maximized = 1;
    frame->x = area_x;
    frame->y = area_y;
    frame->client_width = area_width - 2 * frame_border_of(frame);
    frame->client_height = area_height - frame_title_of(frame)
                           - frame_border_of(frame);
    if (frame->client_width < 1) frame->client_width = 1;
    if (frame->client_height < 1) frame->client_height = 1;

    frame_apply(core, frame);
    frame_notify_configure(core, frame);
    wm_frame_raise(core, frame);
    wm_focus_set(core, frame->client);
}

/* The next window that is still on screen, after the given one and wrapping
   around.
 *
 * This is not the order the switcher key walks: that one goes through the
 * recent-focus order and may land on a minimised window, because it has to be
 * able to reach them. Here the window the keyboard is on is being taken away,
 * so the focus has to land on something the user can see - and walking on to
 * the window just put away would make the minimise button undo itself. */
static WmFrame *frame_next_visible(WmCore *core, Window client) {
    int start = -1;
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].client == client) {
            start = i;
            break;
        }
    }

    for (int step = 1; step <= core->frame_count; step++) {
        int i = (start + step) % core->frame_count;
        if (i < 0) {
            i += core->frame_count;
        }
        if (!core->frames[i].minimized &&
            core->frames[i].workspace == core->current_workspace) {
            return &core->frames[i];
        }
    }
    return NULL;
}

void wm_frame_minimize(WmCore *core, WmFrame *frame) {
    if (frame->minimized) {
        return;
    }
    frame->minimized = 1;
    XUnmapWindow(core->display, frame->frame);

    /* The keyboard cannot stay on a window that is not on the screen, so it
       goes to one that still is. */
    if (core->focused == frame->client) {
        core->focused = 0;
        WmFrame *next = frame_next_visible(core, frame->client);
        if (next) {
            wm_frame_activate(core, next);
        } else {
            XSetInputFocus(core->display, core->root, RevertToPointerRoot,
                           CurrentTime);
        }
    }
    XFlush(core->display);
}

void wm_frame_restore(WmCore *core, WmFrame *frame) {
    if (!frame->minimized) {
        return;
    }
    frame->minimized = 0;
    if (frame->workspace != core->current_workspace) { return; }
    XMapRaised(core->display, frame->frame);

    /* A window that was put away comes back whole. The frame and the client
       inside it are both mapped by the server with one call, but the bar's
       picture is the manager's, so it is drawn again. */
    wm_frame_draw(core, frame);
    XFlush(core->display);
}

void wm_frame_activate(WmCore *core, WmFrame *frame) {
    if (frame->minimized) {
        wm_frame_restore(core, frame);

        /* The map has to have reached the server before the keyboard can be
           given to the window. Every focus decision is made through
           XGetWindowAttributes, and that reports a window as not viewable
           until the server has processed the map request — so without this
           wait a restored window comes back without the focus, which is what
           made the switcher look like it did nothing to a window that had
           been minimised. */
        XSync(core->display, False);
    }
    wm_frame_raise(core, frame);
    wm_focus_set(core, frame->client);
}

/* --- fitting a window's content into it ----------------------------------- */

int wm_frame_is_scaled(const WmFrame *frame) {
    return frame ? frame->scaled : 0;
}

void wm_frame_toggle_scaling(WmCore *core, WmFrame *frame) {
    if (!core || !frame) {
        return;
    }

    if (frame->scaled) {
        /* Off. The window goes back to being the server's to draw, and it is
           given the frame's size first so the swap happens at the size the
           user was looking at — rather than the window jumping back to the
           size it asked for when it opened, which is the size the frame was
           built to avoid. */
        wm_compositor_unscale(core, frame->client);
        frame->scaled = 0;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
        XSync(core->display, False);
        wm_frame_draw(core, frame);
        return;
    }

    /* On. What is being given up has to be said: the window is about to be
       drawn at the size it chose rather than the size of the frame, and the
       hand that pressed the key may have expected the frame to stay exactly as
       it is. It does — the frame is not touched — but what is inside it
       changes, and that is the whole of the feature. */
    if (wm_compositor_scale(core, frame->client) != 0) {
        fprintf(stderr, "gnuchanwm: this window cannot be scaled to its frame; "
                        "it is left as it was.\n");
        return;
    }

    frame->scaled = 1;

    /* The window is put back to the size its content is drawn at. It is moved
       and resized directly rather than through frame_apply(), which skips the
       resize for a scaled window: the resize is wanted here, once, to put the
       window back to the size its content is drawn at. Everything after this
       goes the other way — the frame is fitted to the window. */
    XMoveResizeWindow(core->display, frame->client,
                      frame_border_of(frame), frame_title_of(frame),
                      (unsigned int)frame->natural_width,
                      (unsigned int)frame->natural_height);
    XSync(core->display, False);

    /* The window has to have drawn at its new size before the picture of it is
       anything but the old frame, so it is drawn now and drawn again by the
       damage the client sends as it paints. */
    wm_frame_draw(core, frame);
    fprintf(stderr, "gnuchanwm: '%s' is now drawn fitted to its frame\n",
            frame->has_name && frame->name[0] ? frame->name : "window");
}

/* --- fullscreen ----------------------------------------------------------- */

int wm_frame_is_fullscreen(const WmFrame *frame) {
    return frame ? frame->fullscreen : 0;
}

/* Write the fullscreen state into the client's own _NET_WM_STATE property.
 *
 * The ClientMessage is only half of the conversation. A client that asked the
 * manager for the state reads the answer back out of its own property rather
 * than taking the grant on trust: a toolkit that sets the state, is told the
 * manager speaks EWMH, and gets no property back concludes the request was
 * ignored and asks again for ever. The list is a single atom — the fullscreen
 * member — and an empty list when the state is dropped, which reads exactly as
 * "the states I am in, of which there are none". */
static void frame_publish_fullscreen_state(WmCore *core, WmFrame *frame,
                                           int on) {
    if (frame->client == None || core->net_wm_state == None) {
        return;
    }
    if (on) {
        Atom states[1];
        states[0] = core->net_wm_state_fullscreen;
        XChangeProperty(core->display, frame->client, core->net_wm_state,
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)states, 1);
    } else {
        XChangeProperty(core->display, frame->client, core->net_wm_state,
                        XA_ATOM, 32, PropModeReplace, NULL, 0);
    }
}

/* Take the whole screen, or give it back. See wm_frame.h for the contract;
 * what follows is why it has to exist at all.
 *
 * A Direct3D game — every one of them, and GTA Vice City is the example this
 * was written against — asks for the whole screen and then does its own
 * arithmetic about where its picture goes. What it needs is for the manager to
 * answer "you have the whole screen, and here is the size that is", which in
 * EWMH is one state and one ConfigureNotify. What it gets from a manager that
 * does not answer is nothing at the moment it asks, so it falls back to
 * resizing and moving itself to (0,0) at the screen's size and CHECKING every
 * frame that it is still there.
 *
 * That fallback is the whole of the bug. The client believes it is a window on
 * the root; the manager has put it inside a frame at (border, title) so the
 * bar is above it. The client moves itself to (0,0); wm_frame_sync() takes
 * that as a SCREEN place and moves the FRAME so the client would sit at (0,0)
 * — which puts the client back at (border, title) inside it — and the client,
 * seeing it is no longer at (0,0), moves itself again. Two programs each
 * undoing the other thirty times a second is a picture that slides up and down
 * and never settles.
 *
 * With the state answered none of that happens: the client is told it has the
 * whole screen, the chrome is dropped so that a window at (0,0) the size of
 * the screen really IS at (0,0) the size of the screen, and the client has
 * nothing to correct. It is still inside a frame — it always is — but the
 * frame is exactly the screen and the client is exactly the frame, so the two
 * agree and neither moves.
 *
 * A second thing is fixed at the same time. Wine, unable to get fullscreen the
 * EWMH way, reaches for the display instead and changes the MODE — the whole
 * desktop is resized under the window manager, which is what "the game changed
 * the size of the wm screen" is. Answering the state is what it is waiting
 * for, and once answered it leaves the mode alone.
 *
 * The screen is read from the display rather than from the workarea: fullscreen
 * means the whole screen, bar included, which is the point — the alternative is
 * a "fullscreen" game with a strip of desktop down one side. */
void wm_frame_set_fullscreen(WmCore *core, WmFrame *frame, int on) {
    if (!core || !frame || frame->frame == None) {
        return;
    }
    on = on ? 1 : 0;
    if (frame->fullscreen == on) {
        return;
    }

    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);

    if (on) {
        /* The place and size to come back to. It is taken here and not from
           the fields a maximise uses, because a window may be maximised and
           then made fullscreen and has to come back to the maximise, not to
           wherever it was before that. */
        frame->restore_x = frame->x;
        frame->restore_y = frame->y;
        frame->restore_width = frame->client_width;
        frame->restore_height = frame->client_height;

        /* Fullscreen and maximised at once would be two answers to one
           question, and the glyph that says which is drawn reads maximized.
           The state asked for most recently wins, and it is this one. */
        frame->maximized = 0;

        frame->fullscreen = 1;
        frame->x = 0;
        frame->y = 0;
        frame->client_width = screen_width;
        frame->client_height = screen_height;
        frame_publish_fullscreen_state(core, frame, 1);
    } else {
        frame->fullscreen = 0;
        frame->x = frame->restore_x;
        frame->y = frame->restore_y;
        frame->client_width = frame->restore_width;
        frame->client_height = frame->restore_height;
        /* The size it is coming back to was legal when it was taken, but the
           desktop may have changed since — the screen may be smaller, the bar
           may have moved — so the same two rules every other size path uses
           are applied again. */
        frame_clamp_size_to_screen(core, frame);
        frame_clamp_to_workarea(core, frame);
        frame_publish_fullscreen_state(core, frame, 0);
    }

    frame_apply(core, frame);
    frame_notify_configure(core, frame);

    if (on) {
        wm_frame_raise(core, frame);
        wm_focus_set(core, frame->client);
    }
}

/* --- creating and destroying ---------------------------------------------- */

WmFrame *wm_frame_create(WmCore *core, Window client) {
    if (wm_frame_find(core, client) != NULL) {
        return NULL;
    }
    if (core->frame_count >= WM_MAX_FRAMES) {
        fprintf(stderr, "gnuchanwm: too many windows (max %d)\n", WM_MAX_FRAMES);
        return NULL;
    }

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, client, &attributes)) {
        return NULL;
    }
    if (attributes.override_redirect) {
        return NULL;
    }

    WmFrame *frame = &core->frames[core->frame_count];
    memset(frame, 0, sizeof(*frame));
    frame->client = client;
    wm_workspace_place(core, frame);
    frame->client_width = attributes.width > 1 ? attributes.width : 80;
    frame->client_height = attributes.height > 1 ? attributes.height : 24;
    frame->x = attributes.x;
    frame->y = attributes.y;
    frame->border = style_border_width(core);

    /* The size the client asked for: the size its content is drawn at, and the
       size scaling puts the window back to — see wm_frame_toggle_scaling().
       The size is no longer cut down to the workarea on the way in, so for a
       window that has not resized itself since, this IS its size; a window
       that does resize itself has this updated by wm_frame_sync(). */
    frame->natural_width = frame->client_width;
    frame->natural_height = frame->client_height;

    /* A window's TITLE BAR is kept clear of the bar and on the screen. The
       screen's top-left corner is not where a window belongs when a bar is
       along the top: the frame's title bar would open underneath the strip and
       the window could not be dragged out from under it.

       Only a window that would land outside is moved. A window that fits where
       it asked to be is left exactly there, because a manager that repositions
       a window the user's program placed deliberately is a manager that makes
       every program's own saved geometry meaningless. The SIZE is not touched
       — see the note below. */
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    wm_config_workarea(&core->config, core->width, core->height,
                       &area_x, &area_y, &area_width, &area_height);

    if (frame->x < area_x) {
        frame->x = area_x;
    }
    if (frame->y < area_y) {
        frame->y = area_y;
    }
    if (frame->x < 0) {
        frame->x = 0;
    }
    if (frame->y < 0) {
        frame->y = 0;
    }

    /* PENCERE EKRANA SIGDIRILIR.

       ESKI DAVRANIS VE NEDEN YANLISTI: pencere, program ne istediyse O
       boyutta birakiliyordu; ekrana sigmayan kisim ekranin disinda kaliyordu
       ve "kullanici pencereyi kaydirip gorur" varsayiliyordu. Bu varsayim
       X'te TUTMAZ: ekranin disina tasan pikseller icin X sunucusu arka bellek
       tutmaz, pencere yukari kaydirildiginda o bolge icin Expose uretilmez ve
       alt kenar — BORDER'I DAHIL — bos/siyah kalir. Kullanici bunu "pencerenin
       alti siliniyor, scale yapana kadar duzelmiyor" diye bildirdi ve
       gozlem TAM OLARAK dogru: olceklenmis bir pencerenin cercevesi elle
       kucultuldugu icin ekrana sigar ve sorun gorunmez.

       Neden boyut kirpmak SIMDI dogru: eski not "GL oyunu kendi boyutunda
       cizmeye devam eder" diyordu, ama bu ancak pencere EKRANDAN BUYUK
       birakilirsa sorun olur. Ekrana sigdirilan bir pencerede cerceve de
       kuculur; icerik cerceveye sigar ve hicbir sey disarida kalmaz. Zaten
       bunu yapmayan bir program icin OLCEKLEME anahtari var
       (wm_frame_toggle_scaling) ve o yol da ekrana sigdirmanin USTUNE kurulur.

       Kirpilan sey YALNIZCA buyukluktur; programin kendi bildirdigi natural
       boyut da AYNI sayiya cekilir, yoksa sonradan acilan olcekleme pencereyi
       yeniden ekranin disina tasirdi. */
    frame_clamp_size_to_screen(core, frame);
    if (frame->client_width < 1) {
        frame->client_width = 1;
    }
    if (frame->client_height < 1) {
        frame->client_height = 1;
    }
    /* Now that the size can no longer exceed the desktop, pulling the right
       and bottom edges inside actually moves the frame instead of being
       refused: frame_clamp_to_workarea() declines to move a window too big to
       fit, and after the clamp above there is none. A window the program
       opened larger than the screen therefore lands with all four edges
       visible rather than with its bottom-right off the screen. Done before
       the natural size is recorded, so the two agree — a scaling press later
       must put the window back to a size the desktop can show. */
    frame_clamp_to_workarea(core, frame);
    frame->natural_width = frame->client_width;
    frame->natural_height = frame->client_height;

    frame->frame = XCreateSimpleWindow(
        core->display, core->root, frame->x, frame->y,
        (unsigned int)frame_width(frame),
        (unsigned int)frame_height(frame),
        0, 0, core->style.panel);
    if (frame->frame == None) {
        return NULL;
    }

    /* The frame is override-redirect, and this has to be set before it is
       mapped. The root redirects structure, so a normal window's map comes
       back to us as a MapRequest; without this the manager would receive a
       request for its own frame and wrap the frame in another frame, for
       ever. An override-redirect window is what the server maps without
       asking, which is exactly what the manager's own chrome has to be. */
    XSetWindowAttributes override_attributes;
    memset(&override_attributes, 0, sizeof(override_attributes));
    override_attributes.override_redirect = True;
    /* The bar is not cleared between redraws: every pixel of it is painted in
       wm_frame_draw, into the copy, before the copy is put up. Letting the
       server clear it first would defeat that and bring the flicker back. */
    override_attributes.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(core->display, frame->frame,
                            CWOverrideRedirect | CWBitGravity,
                            &override_attributes);

    /* The bar is drawn on the frame, so the frame is what is clicked, dragged
       and exposed. It also watches for the pointer entering it, because moving
       onto a title bar is how a user chooses a window, and focus follows the
       pointer. EnterNotify does not propagate up the tree, so each window that
       should react to the pointer has to ask for it itself. */
    XSelectInput(core->display, frame->frame,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | EnterWindowMask);

    /* The client is watched for its name, its size, and the pointer arriving
       on it. Without EnterWindowMask here the pointer would focus nothing when
       it moved straight onto the program's own window. */
    XSelectInput(core->display, client,
                 PropertyChangeMask | StructureNotifyMask | EnterWindowMask);

    /* A click on the program's own pixels is taken with a passive grab rather
       than by asking to be told about button presses through the event mask.
     *
     * The mask is the obvious way and it does not work: a program that handles
     * the mouse itself — a terminal with mouse reporting, which is what this
     * session opens — already has the button selected on its window, and
     * asking for it too is refused with BadAccess, which takes the whole mask
     * down with it. The manager then hears nothing at all from that window,
     * clicks included, and that is exactly the window a user clicks.
     *
     * A grab does not collide. It is held per button and per window, so the
     * program's own interest in the button does not stop this from registering,
     * and AnyModifier means the click is taken whether or not a modifier is
     * held. owner_events is False so the grab is the manager's, not the
     * program's, and the press waits here (GrabModeSync) until it is decided
     * what to do with it — see the ButtonPress case below, where it is replayed
     * to the program so a click meant for the program still reaches it. */
    XGrabButton(core->display, Button1, AnyModifier, client, False,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);

    /* Alt+left and Alt+right are the manager's — move and resize — and they
       need grabs of their own on the program's window, because without one the
       press goes straight to the program and the manager never hears it.
     *
     * The grab is taken for the modifier combination and not for the button
     * alone, which is what keeps the plain click on the program: a passive
     * grab only activates when exactly its modifiers are down, so a Button1
     * press with no Alt held does not match this grab and falls through to the
     * any-modifier one above. Where both could match, the press is handled the
     * same way either way — the handler tests the modifiers itself — so it does
     * not matter which of the two the server picks.
     *
     * The event mask is not just the press. Once a passive button grab has
     * activated, its event mask is what is reported for the whole grab, and
     * the grab lasts until the button comes back up. So asking for motion and
     * release here is what makes the window follow the pointer: without them
     * the grab exists but delivers nothing after the first press, and the
     * window only moves as far as the first motion event before the pointer
     * leaves it.
     *
     * Every lock combination is grabbed beside the bare one, so a session with
     * CapsLock or NumLock on still moves windows. */
    for (unsigned int locks = 0; locks < 4; locks++) {
        unsigned int modifiers = Mod1Mask;
        if (locks & 1) modifiers |= LockMask;
        if (locks & 2) modifiers |= Mod2Mask;

        XGrabButton(core->display, Button1, modifiers, client, False,
                    ButtonPressMask | PointerMotionMask | ButtonReleaseMask,
                    GrabModeSync, GrabModeAsync, None, None);
        XGrabButton(core->display, Button3, modifiers, client, False,
                    ButtonPressMask | PointerMotionMask | ButtonReleaseMask,
                    GrabModeSync, GrabModeAsync, None, None);
    }

    /* A safety net: if this window manager dies, the X server puts the client
       back on the root instead of leaving it inside a window nobody owns. */
    XAddToSaveSet(core->display, client);

    /* Reparenting a mapped window makes the server unmap it first, and that
       unmap comes back to us as an UnmapNotify. Counting it here is what tells
       it apart from the user's program hiding itself. */
    if (attributes.map_state == IsViewable) {
        core->ignore_unmaps++;
    }

    /* WM_STATE says "this window is managed", and it is the test a task list,
       a pager or any other watcher uses to tell a client from the chrome.
       The protocol requires it of a reparenting manager, and without it
       _NET_CLIENT_LIST stays empty however many windows are open. */
    long wm_state[2] = { 1 /* NormalState */, None };
    XChangeProperty(core->display, client, core->wm_state, core->wm_state,
                    32, PropModeReplace, (unsigned char *)wm_state, 2);

    XSetWindowBorderWidth(core->display, client, 0);
    XReparentWindow(core->display, client, frame->frame,
                    frame_border_of(frame), frame_title_of(frame));
    XMapWindow(core->display, client);
    XMapWindow(core->display, frame->frame);

    core->frame_count++;

    frame_read_name(core, frame);
    wm_frame_read_icon(core, frame);
    wm_frame_draw(core, frame);
    return frame;
}

void wm_frame_destroy(WmCore *core, WmFrame *frame, int client_gone) {
    /* A window that was being drawn scaled is given back to the server before
       anything else. Leaving it redirected would leave it in the one state
       this module exists to avoid: a window nobody draws. It is done here and
       not left for the compositor's own cleanup because the client may outlive
       the frame — a reparented window is put back on the root below, and it
       has to be the server's again when it gets there. */
    if (frame->scaled) {
        wm_compositor_unscale(core, frame->client);
        frame->scaled = 0;
    }

    /* A window that was fullscreen is told it is not, before it is given back
       or destroyed. The state lives in the CLIENT's own property, and a client
       that outlives this manager — one that is put back on the root below —
       would otherwise still be carrying "I am fullscreen" and would ask the
       next manager for a screen it already gave back, or draw itself at the
       screen's size inside whatever frame it was put in. */
    if (frame->fullscreen) {
        frame_publish_fullscreen_state(core, frame, 0);
        frame->fullscreen = 0;
    }

    /* The icon is this process's pixmap, not the client's, so it does not go
       away with the client and is freed here by the code that made it. */
    wm_frame_free_icon(core, frame);
    if (!client_gone) {
        /* Put the client back where the desktop found it, so a program that
           outlives this window manager is not left fatherless. */
        XRemoveFromSaveSet(core->display, frame->client);
        XReparentWindow(core->display, frame->client, core->root,
                        frame->x, frame->y);
    }
    if (frame->buffer != None) {
        XFreePixmap(core->display, frame->buffer);
        frame->buffer = None;
    }
    if (frame->frame != None) {
        XDestroyWindow(core->display, frame->frame);
    }
    if (core->focused == frame->client) {
        core->focused = 0;
    }
    /* A window that is gone must leave the focus order too, or the switcher
       would try to activate a window the server no longer has. */
    wm_focus_forget(core, frame->client);

    /* Take the frame out of the table by moving the last one into its place.
       Order does not matter here, and this keeps the array packed. */
    int index = (int)(frame - core->frames);
    core->frames[index] = core->frames[core->frame_count - 1];
    core->frame_count--;
    XFlush(core->display);
}

void wm_frame_close(WmCore *core, WmFrame *frame) {
    Atom *protocols = NULL;
    int count = 0;
    int polite = 0;

    if (XGetWMProtocols(core->display, frame->client, &protocols, &count)) {
        for (int i = 0; i < count; i++) {
            if (protocols[i] == core->wm_delete_window) {
                polite = 1;
                break;
            }
        }
        if (protocols) {
            XFree(protocols);
        }
    }

    if (polite) {
        /* Ask, so the program can save its work and its own prompt appears. */
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.xclient.type = ClientMessage;
        event.xclient.window = frame->client;
        event.xclient.message_type = core->wm_protocols;
        event.xclient.format = 32;
        event.xclient.data.l[0] = (long)core->wm_delete_window;
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(core->display, frame->client, False, NoEventMask, &event);
    } else {
        /* A program that cannot be asked has no other way to be stopped. */
        XKillClient(core->display, frame->client);
    }
    XFlush(core->display);
}

/* --- the double click ------------------------------------------------------
 *
 * Two presses close together in time and place are a double click, and X has
 * no event for one: it reports every press the same way. Nor is the interval
 * the desktop is configured with readable from Xlib — it lives in XSETTINGS —
 * so it is a constant here, at the value every desktop uses by default. The
 * window must not have moved between the presses either: a press that dragged
 * the window is not the first half of a double click. */
#define WM_DOUBLE_CLICK_MS 400
#define WM_DOUBLE_CLICK_SLOP 6

static Window click_client = None;
static Time click_time = 0;
static int click_x = 0;
static int click_y = 0;
static int click_frame_x = 0;
static int click_frame_y = 0;

static void click_forget(void) {
    click_client = None;
    click_time = 0;
}

static void click_remember(WmFrame *frame, XButtonEvent *press) {
    click_client = frame->client;
    click_time = press->time;
    click_x = press->x;
    click_y = press->y;
    click_frame_x = frame->x;
    click_frame_y = frame->y;
}

static int frame_is_double_click(WmFrame *frame, XButtonEvent *press) {
    if (click_client != frame->client || click_time == 0) {
        return 0;
    }
    if (press->time < click_time ||
        press->time - click_time > WM_DOUBLE_CLICK_MS) {
        return 0;
    }
    if (frame->x != click_frame_x || frame->y != click_frame_y) {
        return 0;
    }
    int dx = press->x - click_x;
    int dy = press->y - click_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= WM_DOUBLE_CLICK_SLOP && dy <= WM_DOUBLE_CLICK_SLOP;
}

/* --- the module: the bar's clicks, drags and exposures -------------------- */

/* The frame a press or motion names.
 *
 * It can name two different windows. A grab this module took on the frame
 * reports the frame; a grab it took on the client — the Alt+move and Alt+resize
 * ones — reports the client, because that is the window the grab is on. Both
 * are the same window from the user's side, so both are resolved here and
 * nothing below has to know which grab answered. */
static WmFrame *frame_for_event(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find_by_frame(core, window);
    if (!frame) {
        frame = wm_frame_find(core, window);
    }
    return frame;
}

static void frame_begin_drag(WmCore *core, WmFrame *frame, XButtonEvent *press) {
    frame->dragging = 1;
    frame->drag_pointer_x = press->x_root;
    frame->drag_pointer_y = press->y_root;
    frame->drag_frame_x = frame->x;
    frame->drag_frame_y = frame->y;

    wm_focus_set(core, frame->client);
    wm_frame_raise(core, frame);

    /* The pointer is taken for the length of the drag. Without this a fast
       drag leaves the frame and the motion stops arriving, and the window
       stops following the hand. */
    XGrabPointer(core->display, frame->frame, False,
                 PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

static void frame_end_drag(WmCore *core, WmFrame *frame) {
    frame->dragging = 0;
    XUngrabPointer(core->display, CurrentTime);
    XFlush(core->display);
}

/* --- resizing --------------------------------------------------------------
 *
 * Alt+right-button resizes, and which sides move is decided by where the press
 * landed rather than by which key was held: a press near an edge takes that
 * edge, a press near a corner takes both, and a press in the body takes the
 * bottom and the right — the diagonal a hand pulls when it wants something
 * bigger. One mechanism, three ways of aiming it. */

/* Which sides a press at this point takes. The point is in the frame's own
   coordinates: the press may have arrived on the frame or on the client inside
   it, and the caller puts it into the same space either way, so the borders
   that can be grabbed are in one place. */
static int resize_edges_at(const WmFrame *frame, int x, int y) {
    int width = frame_width(frame);
    int height = frame_height(frame);
    int edges = 0;

    if (x <= WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_LEFT;
    } else if (x >= width - WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_RIGHT;
    }
    if (y <= WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_TOP;
    } else if (y >= height - WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_BOTTOM;
    }

    if (edges == 0) {
        edges = WM_RESIZE_BOTTOM | WM_RESIZE_RIGHT;
    }
    return edges;
}

static void frame_begin_resize(WmCore *core, WmFrame *frame,
                               XButtonEvent *press) {
    frame->resizing = 1;
    frame->resize_edges = resize_edges_at(frame,
                                          press->x_root - frame->x,
                                          press->y_root - frame->y);
    frame->resize_pointer_x = press->x_root;
    frame->resize_pointer_y = press->y_root;
    frame->resize_x = frame->x;
    frame->resize_y = frame->y;
    frame->resize_width = frame->client_width;
    frame->resize_height = frame->client_height;

    /* A resize is measured from where it began, not from the last motion, so
       the numbers above are the ones every later motion is applied to. That is
       what makes a drag that is taken back undo itself exactly. */
    XGrabPointer(core->display, frame->frame, False,
                 PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

/* One motion of a resize, from the pointer's absolute place. */
static void frame_resize_drag(WmCore *core, WmFrame *frame,
                              int pointer_x, int pointer_y) {
    WmSizeHints hints;
    int dx = pointer_x - frame->resize_pointer_x;
    int dy = pointer_y - frame->resize_pointer_y;

    int x = frame->resize_x;
    int y = frame->resize_y;
    int width = frame->resize_width;
    int height = frame->resize_height;
    int moving_left = (frame->resize_edges & WM_RESIZE_LEFT) != 0;
    int moving_top = (frame->resize_edges & WM_RESIZE_TOP) != 0;

    /* A side that is moving takes its share of the motion; the opposite side
       stays where it was. A left or top edge therefore moves the frame's
       origin as well as its size, which is the whole difference between
       growing a window and growing it outwards. */
    if (moving_left) {
        width = frame->resize_width - dx;
    } else if (frame->resize_edges & WM_RESIZE_RIGHT) {
        width = frame->resize_width + dx;
    }
    if (moving_top) {
        height = frame->resize_height - dy;
    } else if (frame->resize_edges & WM_RESIZE_BOTTOM) {
        height = frame->resize_height + dy;
    }

    /* The pointer's size is snapped to the grid the client asked for before it
       is applied, so the window is never a fraction of a cell — see the note
       above WmSizeHints. A client that named no increments has one pixel as
       its step and passes through unchanged. */
    frame_read_size_hints(core, frame, &hints);
    width = frame_hint_size(width, hints.base_width, hints.width_inc,
                            hints.min_width > WM_RESIZE_MIN_WIDTH
                                ? hints.min_width : WM_RESIZE_MIN_WIDTH);
    height = frame_hint_size(height, hints.base_height, hints.height_inc,
                             hints.min_height > WM_RESIZE_MIN_HEIGHT
                                 ? hints.min_height : WM_RESIZE_MIN_HEIGHT);

    /* The drag stops at the desktop, like every other size path. Without this
       a window dragged bigger than the screen has its right and bottom edges
       past the screen's own, where X keeps no pixels and raises no Expose when
       they are dragged back — the blank edge this limit exists to prevent,
       made here by the hand rather than by the program. The size is clamped
       BEFORE the origin is computed, so a left or top drag that hits the limit
       keeps its opposite edge still. */
    frame->client_width = width;
    frame->client_height = height;
    frame_clamp_size_to_screen(core, frame);
    width = frame->client_width;
    height = frame->client_height;

    /* The origin follows from the size for an edge that is moving: the
       OPPOSITE edge is the one that has to stay still. Done after the snapping
       so a left or top edge cannot drift by whatever the snapping rounded
       away. */
    if (moving_left) {
        x = frame->resize_x + (frame->resize_width - width);
    }
    if (moving_top) {
        y = frame->resize_y + (frame->resize_height - height);
    }

    frame->x = x;
    frame->y = y;
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

static void frame_end_resize(WmCore *core, WmFrame *frame) {
    frame->resizing = 0;
    XUngrabPointer(core->display, CurrentTime);
    XFlush(core->display);
}

/* Answer the synchronous grab a press may have arrived under.
 *
 * The passive grabs this module takes on a client are GrabModeSync: the
 * server holds the pointer until the manager answers with XAllowEvents. A
 * path that decides the press is not for it and returns without answering
 * leaves the device held, and the freeze is the server's rather than this
 * window's — so the whole display stops responding, which from the user's
 * side is a session that has gone dead.
 *
 * Only a window this module manages can have one of those grabs on it, and
 * the grab reports the client it was taken on, so that is the test. A press
 * that arrived on the frame itself, on the bar or on the root was not grabbed
 * and is left alone: sending an XAllowEvents for a grab that is not active is
 * a no-op at best and is what makes the difference between answering a grab
 * and guessing at one. */
static void frame_allow_sync_grab(WmCore *core, XButtonEvent *press) {
    if (wm_frame_find(core, press->window)) {
        XAllowEvents(core->display, AsyncPointer, press->time);
    }
}

/* Put the pointer where a click on a scaled window actually is.
 *
 * A scaled window is drawn bigger than the frame showing it: the picture is
 * fitted to the frame, so the point the user aimed at is not the point inside
 * the window. The press arrives with the pointer where the hand left it, and
 * the program is about to be handed that coordinate — which is the wrong one,
 * by the same ratio the picture is scaled by. Half a window's width out at
 * half size.
 *
 * The pointer is therefore moved to the other end of the scale before the
 * press is replayed, so the program receives the coordinate it would have
 * received if it had been the size of the frame all along.
 *
 * Two things follow, and both are worth saying plainly rather than leaving to
 * be discovered: the pointer VISIBLY JUMPS, because it has to be somewhere and
 * the only place the program will look is its own coordinates; and the jump is
 * between clicks, so this is a feature for a game played with a pad or a
 * keyboard, not for one played by pointing. That is the honest cost of fitting
 * a window that does not want to be fitted, and it is why the key is a
 * deliberate press rather than something done to every window. */
static void frame_warp_click(WmCore *core, WmFrame *frame, XButtonEvent *press) {
    int client_x = press->x;
    int client_y = press->y;

    wm_compositor_unscale_point(frame->client,
                                frame->client_width, frame->client_height,
                                press->x, press->y, &client_x, &client_y);
    if (client_x == press->x && client_y == press->y) {
        return;     /* it is drawn at its own size; the point is the point */
    }

    /* Where that point is on the screen: the frame's corner, plus the frame's
       own chrome — the client sits at (border, WM_TITLE_HEIGHT) inside it —
       plus the point inside the window. */
    int root_x = frame->x + frame_border_of(frame) + client_x;
    int root_y = frame->y + frame_title_of(frame) + client_y;

    XWarpPointer(core->display, None, core->root, 0, 0, 0, 0, root_x, root_y);
    XSync(core->display, False);
}

static void frame_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case ButtonPress: {
        /* Alt+button is the manager's own, wherever it landed. The press may
           have arrived on the frame — the title bar and the border, which are
           the manager's own window — or on the client inside it, which the
           passive grabs route here. Both are resolved to the same frame so the
           two halves of a window behave the same way. */
        if ((event->xbutton.state & Mod1Mask) &&
            (event->xbutton.button == Button1 ||
             event->xbutton.button == Button3)) {
            int on_client = 0;
            WmFrame *frame = wm_frame_find(core, event->xbutton.window);
            if (frame) {
                on_client = 1;
            } else {
                frame = wm_frame_find_by_frame(core, event->xbutton.window);
            }
            if (!frame) {
                /* Neither a managed client nor one of this manager's frames,
                   but the grab was taken on a client and the press still has
                   to be answered or the pointer stays frozen. */
                frame_allow_sync_grab(core, &event->xbutton);
                break;
            }

            wm_frame_activate(core, frame);

            /* A frame that is maximised is put back before it is moved or
               resized: dragging a full-screen window is almost always meant as
               "take it out of full screen", and a resize of one has nothing to
               grab because its edges are the screen's. */
            if (frame->maximized) {
                wm_frame_maximize(core, frame);
            }

            /* The press on the client is held by a synchronous passive grab;
               releasing it without replaying is what makes the press the
               manager's rather than the program's. The frame's own window has
               no such grab, so nothing has to be released there. */
            if (on_client) {
                XAllowEvents(core->display, AsyncPointer, event->xbutton.time);
            }

            if (event->xbutton.button == Button1) {
                frame_begin_drag(core, frame, &event->xbutton);
            } else {
                frame_begin_resize(core, frame, &event->xbutton);
            }
            XFlush(core->display);
            break;
        }

        if (event->xbutton.button != Button1) {
            /* Not a button this manager acts on. A grab may still be holding
               the pointer, so it is released before the press is dropped. */
            frame_allow_sync_grab(core, &event->xbutton);
            break;
        }
        /* A press the passive grab took on a program's own window. The click
           was aimed at the program, so it is not answered here: the window is
           focused and raised, and the press is then replayed. Replaying is both
           what releases the synchronous grab and what hands the event on, so a
           click meant for the program still reaches it. */
        WmFrame *clicked = wm_frame_find(core, event->xbutton.window);
        if (clicked) {
            wm_focus_set(core, clicked->client);
            wm_frame_raise(core, clicked);
            /* A scaled window is not where it looks, so the pointer is put
               where the program will look before the press is replayed. It is
               done here and not earlier because this is the only path that
               hands a press to the program: a click the manager answers itself
               never reaches the program and needs no correction. */
            if (clicked->scaled) {
                frame_warp_click(core, clicked, &event->xbutton);
            }
            XAllowEvents(core->display, ReplayPointer, event->xbutton.time);
            XFlush(core->display);
            break;
        }

        WmFrame *frame = wm_frame_find_by_frame(core, event->xbutton.window);
        if (!frame) {
            frame_allow_sync_grab(core, &event->xbutton);
            break;
        }
        WmFrameButton button = button_at(frame, event->xbutton.x,
                                         event->xbutton.y);
        if (button == WM_BUTTON_CLOSE) {
            wm_frame_close(core, frame);
        } else if (button == WM_BUTTON_MAXIMIZE) {
            wm_frame_maximize(core, frame);
        } else if (button == WM_BUTTON_MINIMIZE) {
            wm_frame_minimize(core, frame);
        } else if (event->xbutton.y < WM_TITLE_HEIGHT) {
            /* One click on the bar starts a drag; two fill the screen or put
               it back. Every desktop fills the screen on a double click, and
               the box is a small target to ask a hand to find. */
            if (frame_is_double_click(frame, &event->xbutton)) {
                click_forget();
                wm_frame_maximize(core, frame);
            } else {
                click_remember(frame, &event->xbutton);
                frame_begin_drag(core, frame, &event->xbutton);
            }
        }
        break;
    }
    case MotionNotify: {
        /* The window named here is whichever grab is reporting: the frame for a
           title-bar drag, the client for an Alt+move or Alt+resize. Both are
           resolved to the same frame, so the two paths are one. */
        WmFrame *frame = frame_for_event(core, event->xmotion.window);
        if (!frame) {
            break;
        }
        if (frame->dragging || frame->resizing) {
            /* COALESCE. The pointer reports every step it takes and the server
               queues them all, so one drag across the screen arrives as dozens
               of motion events, every one of them stale by the time it is read.
               Applying each one resizes the window, repaints the title bar, and
               — through the ConfigureNotify — makes the program inside redraw
               its whole content: dozens of times over for one visible movement,
               with the window trailing the hand by the whole backlog. That
               backlog is the lag of a resize drag. Only the newest position
               matters, so the queued ones are taken off the queue and dropped
               here and the work is done once.

               Matched by WINDOW, so another window's motion events are left
               alone: a drag owns the pointer, and the events it owns are the
               ones for the window its grab was taken on. */
            XEvent newer;
            while (XCheckTypedWindowEvent(core->display, event->xmotion.window,
                                          MotionNotify, &newer)) {
                *event = newer;
            }
        }
        if (frame->dragging) {
            int x = frame->drag_frame_x +
                    (event->xmotion.x_root - frame->drag_pointer_x);
            int y = frame->drag_frame_y +
                    (event->xmotion.y_root - frame->drag_pointer_y);
            /* The frame is kept clear of the bar at both edges, so a window
               can always be put somewhere it can be grabbed again. */
            wm_frame_move(core, frame, x, y);
        } else if (frame->resizing) {
            frame_resize_drag(core, frame,
                              event->xmotion.x_root, event->xmotion.y_root);
        }
        break;
    }
    case ButtonRelease: {
        WmFrame *frame = frame_for_event(core, event->xbutton.window);
        if (frame && frame->dragging) {
            frame_end_drag(core, frame);
        } else if (frame && frame->resizing) {
            frame_end_resize(core, frame);
        }
        break;
    }
    case Expose: {
        /* Exposes arrive in a RUN — a resize or a raise queues one per exposed
           rectangle — and `count` is how many more are already waiting for
           this window. Drawing for each of them draws the whole frame once per
           rectangle, and only the last one covers them all, so only that one
           draws. During a resize drag this is the difference between one
           repaint and a dozen per motion. */
        if (event->xexpose.count == 0) {
            WmFrame *frame =
                wm_frame_find_by_frame(core, event->xexpose.window);
            if (frame) {
                wm_frame_draw(core, frame);
            }
        }
        break;
    }
    case ConfigureNotify: {
        /* The screen changed size. A maximised window was fitted to the old
           size, so it is fitted again: leaving it is a window that no longer
           fills the screen it is on. */
        if (event->xconfigure.window == core->root) {
            core->width = event->xconfigure.width;
            core->height = event->xconfigure.height;
            for (int i = 0; i < core->frame_count; i++) {
                WmFrame *frame = &core->frames[i];
                if (frame->maximized && !frame->minimized) {
                    frame->maximized = 0;   /* so maximize() fits it again */
                    wm_frame_maximize(core, frame);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

const WmModule wm_frame_module = {
    .name = "frame",
    .init = NULL,
    .event = frame_event,
    .cleanup = NULL,
};
