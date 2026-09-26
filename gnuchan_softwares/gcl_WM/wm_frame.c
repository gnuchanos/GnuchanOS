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
#include "wm_desktop.h"
#include "wm_frame.h"
#include "wm_workspace.h"

/* --- geometry -------------------------------------------------------------
 * All of it is computed from the client's size, so there is one place that
 * decides what a frame looks like and every other function asks it. */

static int frame_width(const WmFrame *frame) {
    return frame->client_width + 2 * WM_FRAME_BORDER;
}

static int frame_height(const WmFrame *frame) {
    return frame->client_height + WM_TITLE_HEIGHT + WM_FRAME_BORDER;
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

    frame_ensure_buffer(core, frame, width, height);
    Drawable target = frame->buffer != None ? frame->buffer : frame->frame;

    /* The bar. */
    XSetForeground(display, core->gc, style->panel);
    XFillRectangle(display, target, core->gc, 0, 0,
                   (unsigned int)width, WM_TITLE_HEIGHT);

    /* The line under it that says which window is active. */
    XSetForeground(display, core->gc,
                   focused ? style->accent : style->accent_dim);
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

    /* The outline around the whole frame, in the focus colour. */
    XSetForeground(display, core->gc,
                   focused ? style->border : style->border_unfocused);
    XDrawRectangle(display, target, core->gc, 0, 0,
                   (unsigned int)(width - 1), (unsigned int)(height - 1));

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
   change draws nothing until it hears about it. */
static void frame_notify_configure(WmCore *core, WmFrame *frame) {
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xconfigure.type = ConfigureNotify;
    event.xconfigure.display = core->display;
    event.xconfigure.event = frame->client;
    event.xconfigure.window = frame->client;
    event.xconfigure.x = WM_FRAME_BORDER;
    event.xconfigure.y = WM_TITLE_HEIGHT;
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
    XMoveResizeWindow(core->display, frame->client,
                      WM_FRAME_BORDER, WM_TITLE_HEIGHT,
                      (unsigned int)frame->client_width,
                      (unsigned int)frame->client_height);
    wm_frame_draw(core, frame);
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
    /* The bar is a sibling of the frame on the root, so raising the frame has
       just put it above the bar. Putting the bar back is this module's job
       because this is the moment it happened, and it is what lets the bar stay
       on top without a timer that restacked the desktop several times a second
       — a restack under the pointer also fires Enter and Leave events for
       whatever the pointer is over, and those redraw title bars, which is what
       made a still window flicker. */
    wm_desktop_raise_bar(core);
    XFlush(core->display);
}

/* Keep a frame inside the workarea while it is being dragged.
 *
 * The bar is a window of its own and is kept above the windows, so a frame
 * dragged under it would have its title bar hidden behind something the user
 * cannot move — and the title bar is the only part of a frame that can be
 * grabbed, so the window could not be got back. The workarea is the screen
 * less that strip, so clamping to it is what keeps every reachable window
 * reachable.
 *
 * Only the top and the bottom are clamped. Moving a window off the left or
 * right edge is how a window is parked half out of the way, and the bar takes
 * nothing from those edges, so there is nothing there to hide behind. */
static int frame_clamp_y(WmCore *core, const WmFrame *frame, int y) {
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    wm_config_workarea(&core->config, core->width, core->height,
                       &area_x, &area_y, &area_width, &area_height);

    if (y < area_y) {
        y = area_y;
    }
    /* The bottom edge is the workarea's, less the frame's own height: the
       workarea says where the desktop ends, and a frame is inside it. */
    int lowest = area_y + area_height - frame_height(frame);
    if (lowest < area_y) {
        lowest = area_y;
    }
    if (y > lowest) {
        y = lowest;
    }
    return y;
}

void wm_frame_resize(WmCore *core, WmFrame *frame, int width, int height) {
    if (width > 1) {
        frame->client_width = width;
    }
    if (height > 1) {
        frame->client_height = height;
    }
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

/* A client that moved or resized itself. Its position is meaningless inside a
   frame — the bar lives above it — so only the size is taken, and the position
   is put back to where the frame wants it.
 *
 * Nothing is redrawn when nothing changed. This is called from every
 * ConfigureNotify the client sends, and the server sends one for reasons that
 * are not a resize — a restack above it, for instance — so repainting
 * unconditionally here is a title bar that repaints whenever anything in the
 * stack moves. The size is the only thing this frame takes from the client,
 * so the size differing is the whole test. */
void wm_frame_sync(WmCore *core, WmFrame *frame) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, frame->client, &attributes)) {
        return;
    }
    if (attributes.width == frame->client_width &&
        attributes.height == frame->client_height) {
        return;
    }
    if (attributes.width > 1) {
        frame->client_width = attributes.width;
    }
    if (attributes.height > 1) {
        frame->client_height = attributes.height;
    }
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
    frame->client_width = area_width - 2 * WM_FRAME_BORDER;
    frame->client_height = area_height - WM_TITLE_HEIGHT - WM_FRAME_BORDER;
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

    /* A window is kept clear of the bar and inside the desktop. The screen's
       top-left corner is not where a window belongs when a bar is along the
       top: the frame's title bar would open underneath the strip and the window
       could not be dragged out from under it. Nor should a window open larger
       than the area it is being put in — a client that asks for the whole
       screen is asking for something the bar has already taken.

       Only a window that would land outside is moved. A window that fits where
       it asked to be is left exactly there, because a manager that repositions
       a window the user's program placed deliberately is a manager that makes
       every program's own saved geometry meaningless. */
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

    /* The frame's own chrome is part of what has to fit, so the room left for
       the client is the area less the title bar and the two side borders. */
    int room_width = area_width - 2 * WM_FRAME_BORDER;
    int room_height = area_height - WM_TITLE_HEIGHT - WM_FRAME_BORDER;
    if (room_width > 1 && frame->client_width > room_width) {
        frame->client_width = room_width;
    }
    if (room_height > 1 && frame->client_height > room_height) {
        frame->client_height = room_height;
    }

    frame->frame = XCreateSimpleWindow(
        core->display, core->root, frame->x, frame->y,
        (unsigned int)frame_width(frame), (unsigned int)frame_height(frame),
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
                    WM_FRAME_BORDER, WM_TITLE_HEIGHT);
    XMapWindow(core->display, client);
    XMapWindow(core->display, frame->frame);

    core->frame_count++;

    frame_read_name(core, frame);
    wm_frame_draw(core, frame);
    return frame;
}

void wm_frame_destroy(WmCore *core, WmFrame *frame, int client_gone) {
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
    XFlush(core->display);
}

/* One motion of a resize, from the pointer's absolute place. */
static void frame_resize_drag(WmCore *core, WmFrame *frame,
                              int pointer_x, int pointer_y) {
    int dx = pointer_x - frame->resize_pointer_x;
    int dy = pointer_y - frame->resize_pointer_y;

    int x = frame->resize_x;
    int y = frame->resize_y;
    int width = frame->resize_width;
    int height = frame->resize_height;

    /* A side that is moving takes its share of the motion; the opposite side
       stays where it was. A left or top edge therefore moves the frame's
       origin as well as its size, which is the whole difference between
       growing a window and growing it outwards. */
    if (frame->resize_edges & WM_RESIZE_LEFT) {
        int wanted = frame->resize_width - dx;
        if (wanted < WM_RESIZE_MIN_WIDTH) {
            dx = frame->resize_width - WM_RESIZE_MIN_WIDTH;
            wanted = WM_RESIZE_MIN_WIDTH;
        }
        x = frame->resize_x + dx;
        width = wanted;
    } else if (frame->resize_edges & WM_RESIZE_RIGHT) {
        width = frame->resize_width + dx;
        if (width < WM_RESIZE_MIN_WIDTH) {
            width = WM_RESIZE_MIN_WIDTH;
        }
    }

    if (frame->resize_edges & WM_RESIZE_TOP) {
        int wanted = frame->resize_height - dy;
        if (wanted < WM_RESIZE_MIN_HEIGHT) {
            dy = frame->resize_height - WM_RESIZE_MIN_HEIGHT;
            wanted = WM_RESIZE_MIN_HEIGHT;
        }
        y = frame->resize_y + dy;
        height = wanted;
    } else if (frame->resize_edges & WM_RESIZE_BOTTOM) {
        height = frame->resize_height + dy;
        if (height < WM_RESIZE_MIN_HEIGHT) {
            height = WM_RESIZE_MIN_HEIGHT;
        }
    }

    /* A left or top edge dragged past the bar would push the frame under it,
       and the frame is the only part of a window the hand can take hold of. It
       is stopped at the workarea instead, and the size gives rather than the
       origin — so the opposite edge stays exactly where the user left it. */
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    wm_config_workarea(&core->config, core->width, core->height,
                       &area_x, &area_y, &area_width, &area_height);

    if (x < area_x) {
        width -= area_x - x;
        x = area_x;
        if (width < WM_RESIZE_MIN_WIDTH) {
            width = WM_RESIZE_MIN_WIDTH;
        }
    }
    if (y < area_y) {
        height -= area_y - y;
        y = area_y;
        if (height < WM_RESIZE_MIN_HEIGHT) {
            height = WM_RESIZE_MIN_HEIGHT;
        }
    }

    frame->x = x;
    frame->y = y;
    frame->client_width = width;
    frame->client_height = height;
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

static void frame_end_resize(WmCore *core, WmFrame *frame) {
    frame->resizing = 0;
    XUngrabPointer(core->display, CurrentTime);
    XFlush(core->display);
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
            XAllowEvents(core->display, ReplayPointer, event->xbutton.time);
            XFlush(core->display);
            break;
        }

        WmFrame *frame = wm_frame_find_by_frame(core, event->xbutton.window);
        if (!frame) {
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
        if (frame->dragging) {
            int x = frame->drag_frame_x +
                    (event->xmotion.x_root - frame->drag_pointer_x);
            int y = frame->drag_frame_y +
                    (event->xmotion.y_root - frame->drag_pointer_y);
            /* The frame is kept clear of the bar at both edges, so a window
               can always be put somewhere it can be grabbed again. */
            wm_frame_move(core, frame, x, frame_clamp_y(core, frame, y));
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
        WmFrame *frame = wm_frame_find_by_frame(core, event->xexpose.window);
        if (frame) {
            wm_frame_draw(core, frame);
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
