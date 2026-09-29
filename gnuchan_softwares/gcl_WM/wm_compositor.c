/*
 * wm_compositor.c — scale one window's content to fit the window it is in.
 *
 * See wm_compositor.h for what this is for and what it costs. What follows is
 * how it is done, and the two ideas that make it work.
 *
 * --- the window keeps its own size -----------------------------------------
 *
 * The obvious way to scale a window is to resize it and scale what comes back.
 * That cannot work: resizing the window is what made the problem. A program
 * that ignores the resize draws its 1600x900 interface into an 800x600 window,
 * and the window's pixels are then a clipped 800x600 — there is nothing left
 * to scale. The part that went off the edge is not in the pixmap at all.
 *
 * So while a window is scaled, the window is NOT resized. It keeps the size it
 * chose, the program goes on drawing the whole of its interface at that size,
 * and the whole of it lands in the window's pixmap. What is scaled is the
 * FRAME: the picture is drawn into the frame's client area, which is whatever
 * the user dragged it to. Shrinking the frame hides nothing, because the whole
 * picture is still there — it is drawn smaller.
 *
 * That is why frame_apply() in wm_frame.c skips the client's resize when the
 * compositor owns it, and why the source size is read from the client rather
 * than kept anywhere: the client is the only thing that knows how big it is.
 *
 * --- the pixmap is re-taken whenever the window changes size -----------------
 *
 * XCompositeNameWindowPixmap() gives a pixmap that is valid until the window
 * is resized or destroyed. A window that changes its own size — a game
 * changing resolution — leaves the old pixmap pointing at nothing, and drawing
 * from it is a protocol error. So every ConfigureNotify from a scaled client
 * throws the pixmap away, and the next draw takes a fresh one. Nothing is
 * cached across the change, which is what makes the change safe.
 *
 * --- what happens when it cannot work ---------------------------------------
 *
 * Every failure path unscales. A server without Composite, a window whose
 * visual has no render format, a pixmap the server refuses: each one puts the
 * window back under the server and answers -1, and the window looks exactly as
 * it did before this module existed. A window is never left redirected and
 * undrawn, which is the one state that would make it disappear.
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_compositor.h"

/* One scaled window. The client is the key; everything else is what has to be
   kept between draws so a window that is not changing costs nothing. */
typedef struct ScaleEntry {
    Window client;

    /* The server's change notice for this window. A redirect is what makes the
       window's drawing private, and damage is what keeps it from being private
       to nobody: without it the picture would be the first frame for ever,
       because nothing would ever say that it had moved on. */
    Damage damage;

    /* The window's own pixels, as the server last left them, and the picture
       it is drawn through. Both are thrown away when the window changes size —
       see the note above — and made again on the next draw. */
    Pixmap source;
    Picture picture;
    int source_width;
    int source_height;

    /* The render format the source pixmap was made with, kept so the pixmap is
       never taken with a different one: a picture whose format does not match
       its drawable is a BadMatch, and it is the failure that would take the
       whole session down at the least convenient moment. */
    XRenderPictFormat *format;
} ScaleEntry;

static ScaleEntry entries[WM_MAX_FRAMES];
static int entry_count = 0;

/* What the server can do. All three are needed and a server missing any of
   them cannot scale anything, which is a desktop that behaves exactly as it
   did before this file existed. */
static int extensions_ok = 0;
static int damage_event_base = 0;

/* Whether anything is scaled at all. When nothing is, the whole module is a
   few comparisons in the event loop — which is what keeps a session that does
   not use this from paying for it. */
static int enabled = 0;

/* While the switcher is taking its picture of the desktop, every scaled window
   is put back under the server: a redirected window is not in the root's
   pixels, so the picture would have a hole where every scaled window is. See
   wm_compositor_suspend(). */
static int suspended = 0;

/* The picture the target is drawn through, kept between draws because the
   target is the same pixmap for the whole life of a frame's buffer. It is
   rebuilt when the target changes, which is what a resize of the frame does. */
static Drawable target_drawable = None;
static Picture target_picture = None;
static int target_width = 0;
static int target_height = 0;

/* --- the table ------------------------------------------------------------ */

static ScaleEntry *entry_find(Window client) {
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].client == client) {
            return &entries[i];
        }
    }
    return NULL;
}

/* Throw the window's picture away. Called when the window changes size, and it
   is the whole of what makes a resize safe: the next draw asks for a fresh one
   rather than reading a pixmap the server has already invalidated. */
static void entry_drop_source(WmCore *core, ScaleEntry *entry) {
    if (entry->picture != None) {
        XRenderFreePicture(core->display, entry->picture);
        entry->picture = None;
    }
    if (entry->source != None) {
        XFreePixmap(core->display, entry->source);
        entry->source = None;
    }
    entry->source_width = 0;
    entry->source_height = 0;
}

/* Take the window's pixels again, and build the picture they are drawn
   through. Returns 1 when there is something to draw, 0 when the server
   would not give the pixmap — in which case the caller draws nothing and the
   frame's own colour shows, rather than drawing from a picture that is not
   there. */
static int entry_take_source(WmCore *core, ScaleEntry *entry) {
    XWindowAttributes attributes;

    if (!XGetWindowAttributes(core->display, entry->client, &attributes)) {
        return 0;
    }
    if (attributes.map_state != IsViewable) {
        return 0;       /* nothing is being drawn; there is nothing to take  */
    }
    if (attributes.width < 1 || attributes.height < 1) {
        return 0;
    }
    if (!entry->format) {
        entry->format = XRenderFindVisualFormat(core->display,
                                                attributes.visual);
        if (!entry->format) {
            return 0;
        }
    }

    entry->source = XCompositeNameWindowPixmap(core->display, entry->client);
    if (entry->source == None) {
        return 0;
    }
    entry->source_width = attributes.width;
    entry->source_height = attributes.height;

    XRenderPictureAttributes picture_attributes;
    memset(&picture_attributes, 0, sizeof(picture_attributes));
    /* The picture is clipped to the window's own rectangle, so a program that
       draws past its edge — which is allowed, and which is what the window's
       parent normally clips — cannot paint over the title bar through this
       path. The server would have clipped it; so does this. */
    picture_attributes.clip_x_origin = 0;
    picture_attributes.clip_y_origin = 0;
    entry->picture = XRenderCreatePicture(core->display, entry->source,
                                          entry->format, 0,
                                          &picture_attributes);
    if (entry->picture == None) {
        XFreePixmap(core->display, entry->source);
        entry->source = None;
        return 0;
    }
    return 1;
}

/* --- what the server can do ----------------------------------------------- */

static void probe_extensions(WmCore *core) {
    int composite_event = 0, composite_error = 0;
    int damage_error = 0;
    int render_event = 0, render_error = 0;

    int composite_ok = XCompositeQueryExtension(core->display,
                                                &composite_event,
                                                &composite_error);
    int damage_ok = XDamageQueryExtension(core->display, &damage_event_base,
                                          &damage_error);
    int render_ok = XRenderQueryExtension(core->display, &render_event,
                                          &render_error);

    extensions_ok = composite_ok && damage_ok && render_ok;
    if (!extensions_ok) {
        fprintf(stderr,
                "gnuchanwm: compositor: scaling is off — this server has no "
                "%s%s%s. Windows behave as they did before.\n",
                composite_ok ? "" : "Composite ",
                damage_ok ? "" : "Damage ",
                render_ok ? "" : "Render ");
    }
}

int wm_compositor_available(void) {
    return extensions_ok && enabled;
}

int wm_compositor_is_scaled(Window client) {
    return entry_find(client) != NULL;
}

/* --- taking and giving back a window -------------------------------------- */

int wm_compositor_scale(WmCore *core, Window client) {
    if (!extensions_ok || !core || client == None) {
        return -1;
    }
    if (entry_find(client)) {
        return 0;       /* already scaled; asking twice is not an error     */
    }
    if (entry_count >= WM_MAX_FRAMES) {
        return -1;
    }

    /* A window with no drawable of its own — an InputOnly window, which has no
       pixels by definition — cannot be scaled and is refused here rather than
       failing later inside XRender. */
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, client, &attributes)) {
        return -1;
    }
    if (attributes.class == InputOnly || !attributes.visual) {
        return -1;
    }
    if (!XRenderFindVisualFormat(core->display, attributes.visual)) {
        fprintf(stderr, "gnuchanwm: compositor: this window's visual cannot "
                        "be drawn by the render extension; it is left alone.\n");
        return -1;
    }

    ScaleEntry *entry = &entries[entry_count];
    memset(entry, 0, sizeof(*entry));
    entry->client = client;
    entry->source = None;
    entry->picture = None;

    /* Manual, not Automatic: the server is told to stop drawing this window
       altogether, because what is drawn instead is the scaled copy. Automatic
       would leave the server drawing it unscaled underneath, which is the
       window with its content hanging off the edge — the thing being fixed. */
    XCompositeRedirectWindow(core->display, client, CompositeRedirectManual);
    XSync(core->display, False);

    entry->damage = XDamageCreate(core->display, client,
                                  XDamageReportNonEmpty);
    if (entry->damage == None) {
        XCompositeUnredirectWindow(core->display, client,
                                   CompositeRedirectManual);
        XSync(core->display, False);
        return -1;
    }

    entry_count++;
    enabled = 1;

    fprintf(stderr, "gnuchanwm: compositor: scaling this window to its frame\n");
    return 0;
}

void wm_compositor_unscale(WmCore *core, Window client) {
    ScaleEntry *entry = entry_find(client);
    if (!entry || !core) {
        return;
    }

    entry_drop_source(core, entry);
    if (entry->damage != None) {
        XDamageDestroy(core->display, entry->damage);
        entry->damage = None;
    }
    /* Given back to the server, which draws it again from here on. This is the
       line that makes every failure path safe: whatever went wrong, the window
       is the server's again and looks as it did. */
    XCompositeUnredirectWindow(core->display, client,
                               CompositeRedirectManual);
    XSync(core->display, False);

    /* Taken out of the table by moving the last one into its place, and packed
       — the same as the frame table, and for the same reason: this is walked
       on every damage event and a hole in it would slow that down for ever. */
    int index = (int)(entry - entries);
    entries[index] = entries[entry_count - 1];
    entry_count--;

    if (entry_count == 0) {
        enabled = 0;
    }
}

/* --- drawing -------------------------------------------------------------- */

/* The picture the target is drawn through, made again when the target changes.
   A frame's buffer is a new pixmap every time the frame is resized, and a
   picture made for the old one draws into a window that is gone. */
static void target_ensure(WmCore *core, Drawable target, int width, int height) {
    if (target == target_drawable && target_width == width &&
        target_height == height && target_picture != None) {
        return;
    }
    if (target_picture != None) {
        XRenderFreePicture(core->display, target_picture);
        target_picture = None;
    }

    XRenderPictFormat *format =
        XRenderFindVisualFormat(core->display,
                                DefaultVisual(core->display, core->screen));
    if (!format) {
        target_drawable = None;
        return;
    }
    target_picture = XRenderCreatePicture(core->display, target, format, 0, NULL);
    target_drawable = target;
    target_width = width;
    target_height = height;
}

/* The transform that draws a `source` sized picture into a `width` sized hole.
 *
 * Render works in 16.16 fixed point, so a ratio of one is 65536 and the numbers
 * below are that ratio restated: how many source pixels go into one drawn
 * pixel. It is a plain scale about the origin — the source's (0,0) is the
 * drawn (0,0) — which is what puts the window's own top-left corner at the
 * frame's top-left corner, as if the window had been resized and the program
 * had listened. */
static XTransform scale_transform(int source, int target) {
    XTransform transform;
    memset(&transform, 0, sizeof(transform));

    long ratio = ((long)source << 16) / (target > 0 ? target : 1);

    transform.matrix[0][0] = ratio;   /* x is read from x               */
    transform.matrix[0][1] = 0;
    transform.matrix[0][2] = 0;
    transform.matrix[1][0] = 0;
    transform.matrix[1][1] = ratio;   /* y is read from y               */
    transform.matrix[1][2] = 0;
    transform.matrix[2][0] = 0;
    transform.matrix[2][1] = 0;
    transform.matrix[2][2] = 65536;   /* the constant term is one       */
    return transform;
}

int wm_compositor_draw(WmCore *core, Window client, Drawable target,
                       int x, int y, int width, int height) {
    ScaleEntry *entry;

    if (!core || width < 1 || height < 1) {
        return 0;
    }
    entry = entry_find(client);
    if (!entry || suspended) {
        return 0;
    }

    /* The picture is taken on the draw rather than when the window was taken,
       because the window may not have drawn anything yet at that point and a
       pixmap taken then is empty. Taken here, it is the last thing the program
       drew — and it is taken again after every resize, which is what
       entry_drop_source() has arranged by having thrown it away. */
    if (entry->picture == None) {
        if (!entry_take_source(core, entry)) {
            return 0;
        }
    }

    target_ensure(core, target, width, height);
    if (target_picture == None) {
        return 0;
    }

    XRenderSetPictureFilter(core->display, entry->picture, FilterBilinear,
                            NULL, 0);
    XTransform transform = scale_transform(entry->source_width, width);
    XRenderSetPictureTransform(core->display, entry->picture, &transform);

    /* The whole of the window into the whole of the hole. PictOpSrc because
       the picture is opaque — a window's pixels have no alpha of their own —
       so the source simply replaces what was there. */
    XRenderComposite(core->display, PictOpSrc,
                     entry->picture, None, target_picture,
                     0, 0,               /* the source's corner            */
                     0, 0,               /* the mask's corner, unused      */
                     x, y,               /* where in the target it goes    */
                     (unsigned int)width, (unsigned int)height);

    XFlush(core->display);
    return 1;
}

void wm_compositor_unscale_point(Window client, int target_width,
                                 int target_height, int x, int y,
                                 int *client_x, int *client_y) {
    ScaleEntry *entry = entry_find(client);

    /* The default is that nothing is scaled, and the point is passed through
       untouched. That is the answer for every window this module is not
       drawing, and it is what keeps a caller from having to ask first. */
    if (client_x) {
        *client_x = x;
    }
    if (client_y) {
        *client_y = y;
    }
    if (!entry || target_width < 1 || target_height < 1) {
        return;
    }
    if (entry->source_width < 1 || entry->source_height < 1) {
        /* Nothing has been taken yet, so the window is still its own size and
           the point is still the point. */
        return;
    }

    /* The other end of the transform above: the point the user aimed at in the
       frame, restated in the coordinates the program drew in. */
    if (client_x) {
        *client_x = (int)((long)x * entry->source_width / target_width);
    }
    if (client_y) {
        *client_y = (int)((long)y * entry->source_height / target_height);
    }
}

void wm_compositor_repaint(WmCore *core, Window client) {
    if (!core || suspended) {
        return;
    }
    WmFrame *frame = wm_frame_find(core, client);
    if (frame) {
        wm_frame_draw(core, frame);
    }
}

/* --- the switcher's picture ----------------------------------------------- */

/* Give every window back to the server for as long as the switcher is taking
   its picture, and take them again afterwards.
 *
 * The switcher reads the ROOT window's pixels to make the little pictures of
 * each window. A redirected window is not in the root's pixels at all — that
 * is what redirection means — so without this every scaled window would be a
 * hole in the switcher, which is worse than the window being the wrong size in
 * a picture that is about to be taken down anyway.
 *
 * The windows are drawn unscaled for that instant. It is a frame or two at
 * most, and the alternative — the switcher compositing every window itself —
 * is a second implementation of this file. */
void wm_compositor_suspend(WmCore *core) {
    if (!core || suspended || entry_count == 0) {
        return;
    }
    for (int i = 0; i < entry_count; i++) {
        XCompositeUnredirectWindow(core->display, entries[i].client,
                                   CompositeRedirectManual);
    }
    XSync(core->display, False);
    suspended = 1;
}

void wm_compositor_resume(WmCore *core) {
    if (!core || !suspended) {
        return;
    }
    suspended = 0;
    for (int i = 0; i < entry_count; i++) {
        XCompositeRedirectWindow(core->display, entries[i].client,
                                 CompositeRedirectManual);
        /* The pixmap the window had before the pause is not the one it has
           after it, so it is thrown away and taken again on the next draw. */
        entry_drop_source(core, &entries[i]);
    }
    XSync(core->display, False);

    for (int i = 0; i < entry_count; i++) {
        wm_compositor_repaint(core, entries[i].client);
    }
}

/* --- the module ------------------------------------------------------------ */

static void compositor_event(WmCore *core, XEvent *event) {
    if (!extensions_ok || entry_count == 0) {
        return;
    }

    if (event->type == damage_event_base + XDamageNotify) {
        XDamageNotifyEvent *damage = (XDamageNotifyEvent *)event;
        ScaleEntry *entry = entry_find(damage->drawable);

        /* Everything the server has counted up to now is cleared at once. The
           report is NonEmpty, so the next one comes when there is more to draw
           rather than for every pixel; a program redrawing at sixty frames a
           second then costs sixty of these, which is the rate it is drawing
           at and cannot be bettered. */
        XDamageSubtract(core->display, damage->damage, None, None);
        if (entry) {
            wm_compositor_repaint(core, entry->client);
        }
        return;
    }

    /* A scaled window that changed its own size has left the pixmap taken from
       it pointing at nothing, and drawing from one is a protocol error. The
       ConfigureNotify is the moment that is known, so the pixmap goes here
       rather than being discovered by a failure. */
    if (event->type == ConfigureNotify) {
        ScaleEntry *entry = entry_find(event->xconfigure.window);
        if (entry) {
            entry_drop_source(core, entry);
            wm_compositor_repaint(core, entry->client);
        }
        return;
    }
}

static int compositor_init(WmCore *core) {
    memset(entries, 0, sizeof(entries));
    entry_count = 0;
    enabled = 0;
    suspended = 0;
    target_drawable = None;
    target_picture = None;
    target_width = 0;
    target_height = 0;
    probe_extensions(core);
    return 0;
}

static void compositor_cleanup(WmCore *core) {
    if (target_picture != None) {
        XRenderFreePicture(core->display, target_picture);
        target_picture = None;
    }
    /* Every window is given back before the session ends. A client that
       outlives this manager — which one does when the manager is restarted —
       would otherwise be inside a redirect nobody is drawing, which is a
       window that has no picture at all. */
    for (int i = 0; i < entry_count; i++) {
        entry_drop_source(core, &entries[i]);
        if (entries[i].damage != None) {
            XDamageDestroy(core->display, entries[i].damage);
            entries[i].damage = None;
        }
        XCompositeUnredirectWindow(core->display, entries[i].client,
                                   CompositeRedirectManual);
    }
    entry_count = 0;
    enabled = 0;
    XSync(core->display, False);
}

const WmModule wm_compositor_module = {
    .name = "compositor",
    .init = compositor_init,
    .event = compositor_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = compositor_cleanup,
};
