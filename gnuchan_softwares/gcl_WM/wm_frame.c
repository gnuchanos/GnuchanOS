/*
 * wm_frame.c — the frame a managed window lives in, and everything done to it.
 *
 * The shape of a frame is fixed and simple:
 *
 *     +--------------------------------------+
 *     | title             [x]                |  WM_TITLE_HEIGHT
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_frame.h"

/* --- geometry -------------------------------------------------------------
 * All of it is computed from the client's size, so there is one place that
 * decides what a frame looks like and every other function asks it. */

static int frame_width(const WmFrame *frame) {
    return frame->client_width + 2 * WM_FRAME_BORDER;
}

static int frame_height(const WmFrame *frame) {
    return frame->client_height + WM_TITLE_HEIGHT + WM_FRAME_BORDER;
}

/* Where the close button sits inside the bar. */
static int close_x(const WmFrame *frame) {
    return frame_width(frame) - WM_CLOSE_SIZE - 6;
}

static int close_y(void) {
    return (WM_TITLE_HEIGHT - WM_CLOSE_SIZE) / 2;
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
static void frame_clip(XFontStruct *font, const char *text, int room,
                       char *out, unsigned int size) {
    out[0] = '\0';
    if (!font || !text || room <= 0) {
        return;
    }
    if (XTextWidth(font, text, (int)strlen(text)) <= room) {
        snprintf(out, size, "%s", text);
        return;
    }
    size_t length = strlen(text);
    while (length > 0) {
        length--;
        char attempt[192];
        snprintf(attempt, sizeof(attempt), "%.*s...", (int)length, text);
        if (XTextWidth(font, attempt, (int)strlen(attempt)) <= room) {
            snprintf(out, size, "%s", attempt);
            return;
        }
    }
    snprintf(out, size, "...");
}

void wm_frame_draw(WmCore *core, WmFrame *frame) {
    Display *display = core->display;
    const WmStyle *style = &core->style;
    int width = frame_width(frame);
    int height = frame_height(frame);
    int focused = (core->focused == frame->client);

    /* The bar, and the line under it that says which window is active. */
    XSetForeground(display, core->gc, style->panel);
    XFillRectangle(display, frame->frame, core->gc, 0, 0,
                   (unsigned int)width, WM_TITLE_HEIGHT);
    XSetForeground(display, core->gc,
                   focused ? style->accent : style->accent_dim);
    XFillRectangle(display, frame->frame, core->gc, 0, WM_TITLE_HEIGHT - 2,
                   (unsigned int)width, 2);

    /* The title. */
    int padding = 8;
    int room = close_x(frame) - padding - 8;
    if (room < 12) {
        room = 12;
    }
    char label[192];
    frame_clip(style->font, frame->name, room, label, sizeof(label));
    int ascent = style->font ? style->font->ascent : 8;
    int descent = style->font ? style->font->descent : 2;
    int baseline = (WM_TITLE_HEIGHT + ascent - descent) / 2;
    wm_style_text(display, frame->frame, core->gc, style->font, padding,
                  baseline, label, style->text);

    /* The close button: a box with a cross in it. */
    int cx = close_x(frame);
    int cy = close_y();
    XSetForeground(display, core->gc, style->field);
    XFillRectangle(display, frame->frame, core->gc, cx, cy,
                   WM_CLOSE_SIZE, WM_CLOSE_SIZE);
    XSetForeground(display, core->gc, style->accent);
    XDrawRectangle(display, frame->frame, core->gc, cx, cy,
                   WM_CLOSE_SIZE - 1, WM_CLOSE_SIZE - 1);
    XDrawLine(display, frame->frame, core->gc,
              cx + 5, cy + 5, cx + WM_CLOSE_SIZE - 5, cy + WM_CLOSE_SIZE - 5);
    XDrawLine(display, frame->frame, core->gc,
              cx + WM_CLOSE_SIZE - 5, cy + 5, cx + 5, cy + WM_CLOSE_SIZE - 5);

    /* The outline around the whole frame, in the focus colour. */
    XSetForeground(display, core->gc,
                   focused ? style->border : style->border_unfocused);
    XDrawRectangle(display, frame->frame, core->gc, 0, 0,
                   (unsigned int)(width - 1), (unsigned int)(height - 1));

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
    XMoveWindow(core->display, frame->frame, x, y);
    XFlush(core->display);
}

void wm_frame_raise(WmCore *core, WmFrame *frame) {
    XRaiseWindow(core->display, frame->frame);
    XFlush(core->display);
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

void wm_frame_sync(WmCore *core, WmFrame *frame) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, frame->client, &attributes)) {
        return;
    }
    /* The client moved or resized itself. Its position is meaningless inside a
       frame — the bar lives above it — so only the size is taken, and the
       position is put back to where the frame wants it. */
    if (attributes.width != frame->client_width ||
        attributes.height != frame->client_height) {
        frame->client_width = attributes.width;
        frame->client_height = attributes.height;
    }
    frame_apply(core, frame);
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
    frame->client_width = attributes.width > 1 ? attributes.width : 80;
    frame->client_height = attributes.height > 1 ? attributes.height : 24;
    frame->x = attributes.x;
    frame->y = attributes.y;
    if (frame->x < 0) {
        frame->x = 0;
    }
    if (frame->y < 0) {
        frame->y = 0;
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
    override_attributes.override_redirect = True;
    XChangeWindowAttributes(core->display, frame->frame,
                            CWOverrideRedirect, &override_attributes);

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
    if (frame->frame != None) {
        XDestroyWindow(core->display, frame->frame);
    }
    if (core->focused == frame->client) {
        core->focused = 0;
    }

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

/* --- the module: the bar's clicks, drags and exposures -------------------- */

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

static void frame_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case ButtonPress: {
        if (event->xbutton.button != Button1) {
            break;
        }
        WmFrame *frame = wm_frame_find_by_frame(core, event->xbutton.window);
        if (!frame) {
            break;
        }
        int cx = close_x(frame);
        int cy = close_y();
        int on_close = event->xbutton.x >= cx &&
                       event->xbutton.x < cx + WM_CLOSE_SIZE &&
                       event->xbutton.y >= cy &&
                       event->xbutton.y < cy + WM_CLOSE_SIZE;
        if (on_close) {
            wm_frame_close(core, frame);
        } else if (event->xbutton.y < WM_TITLE_HEIGHT) {
            frame_begin_drag(core, frame, &event->xbutton);
        }
        break;
    }
    case MotionNotify: {
        WmFrame *frame = wm_frame_find_by_frame(core, event->xmotion.window);
        if (frame && frame->dragging) {
            int x = frame->drag_frame_x +
                    (event->xmotion.x_root - frame->drag_pointer_x);
            int y = frame->drag_frame_y +
                    (event->xmotion.y_root - frame->drag_pointer_y);
            wm_frame_move(core, frame, x, y);
        }
        break;
    }
    case ButtonRelease: {
        WmFrame *frame = wm_frame_find_by_frame(core, event->xbutton.window);
        if (frame && frame->dragging) {
            frame_end_drag(core, frame);
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
