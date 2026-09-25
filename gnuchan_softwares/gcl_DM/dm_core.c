/*
 * dm_core.c — the display, the window, the event loop, and the module list.
 *
 * The greeter draws every frame into an off-screen pixmap and then puts that
 * pixmap on the window in one XCopyArea. Drawing straight to the window would
 * clear it and rebuild it in full view of the user, which is what turns a held
 * and repeating Tab key into a flicker.
 *
 * There is one greeter per machine, so the list of modules is a file-static
 * here rather than a field of the core: a second core would share it, and a
 * second core is not a thing that can exist.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/cursorfont.h>

#include "dm_core.h"

static DmModuleList g_modules;

static int core_x_error(Display *display, XErrorEvent *error) {
    char text[256];
    text[0] = '\0';
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr, "gnuchandm: X error: %s\n", text);
    return 0;
}

/* The arrow, one string per row: 'X' is a pixel of the arrow, '.' is not.
 *
 * The shape is written out here rather than asked for from the font, because
 * the font's cursor is a name the pointer theme answers — and a theme is free
 * to ignore the colour it is given. Every modern theme draws its cursors as
 * ARGB images, and XRecolorCursor does nothing to an image, which is exactly
 * why recolouring the font cursor left the pointer the theme's own colour. A
 * shape made at this level has no theme behind it to disagree. */
static const char *const CURSOR_ARROW[15] = {
    "X..............",
    "XX.............",
    "X.X............",
    "X..X...........",
    "X...X..........",
    "X....X.........",
    "X.....X........",
    "X......X.......",
    "X.......X......",
    "X........X.....",
    "X.....XXXXX....",
    "X..X...X.......",
    "X.X.X...X......",
    "XX...X...X.....",
    "X.....X...X....",
};

#define CURSOR_SIDE 15

/* One 1-bit pixmap holding the arrow. With `grow` set the shape is fattened by
   a pixel to make the mask: the mask is what the server draws the two colours
   through, so a mask larger than the source puts the background colour as an
   outline around every purple pixel — which is what keeps the pointer visible
   over a light patch as well as a dark one. */
static Pixmap core_cursor_shape(DmCore *core, int grow) {
    Pixmap pixmap = XCreatePixmap(core->display, core->root,
                                  CURSOR_SIDE, CURSOR_SIDE, 1);
    if (pixmap == None) {
        return None;
    }
    GC gc = XCreateGC(core->display, pixmap, 0, NULL);
    XSetForeground(core->display, gc, 0);
    XFillRectangle(core->display, pixmap, gc, 0, 0, CURSOR_SIDE, CURSOR_SIDE);
    XSetForeground(core->display, gc, 1);

    for (int y = 0; y < CURSOR_SIDE; y++) {
        for (int x = 0; x < CURSOR_SIDE; x++) {
            int set = CURSOR_ARROW[y][x] == 'X';
            if (!set && grow) {
                for (int dy = -1; dy <= 1 && !set; dy++) {
                    for (int dx = -1; dx <= 1 && !set; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny >= 0 && ny < CURSOR_SIDE &&
                            nx >= 0 && nx < CURSOR_SIDE &&
                            CURSOR_ARROW[ny][nx] == 'X') {
                            set = 1;
                        }
                    }
                }
            }
            if (set) {
                XDrawPoint(core->display, pixmap, gc, x, y);
            }
        }
    }

    XFreeGC(core->display, gc);
    return pixmap;
}

/* The pointer over the login screen: the arrow above, in the greeter's own
   purple on its own background. Built from a bitmap so the colour is the
   greeter's rather than the pointer theme's, which is what makes it read as
   part of this screen instead of something left over from the machine. */
static Cursor core_make_cursor(DmCore *core) {
    Pixmap source = core_cursor_shape(core, 0);
    Pixmap mask = core_cursor_shape(core, 1);
    if (source == None || mask == None) {
        if (source != None) XFreePixmap(core->display, source);
        if (mask != None) XFreePixmap(core->display, mask);
        return XCreateFontCursor(core->display, XC_left_ptr);
    }

    XColor foreground;   /* the accent purple #c77dff */
    XColor background;   /* the login background #1a0b2e */
    Colormap cmap = DefaultColormap(core->display, core->screen);
    if (!XParseColor(core->display, cmap, "#c77dff", &foreground) ||
        !XParseColor(core->display, cmap, "#1a0b2e", &background) ||
        !XAllocColor(core->display, cmap, &foreground) ||
        !XAllocColor(core->display, cmap, &background)) {
        XFreePixmap(core->display, source);
        XFreePixmap(core->display, mask);
        return XCreateFontCursor(core->display, XC_left_ptr);
    }

    /* The hotspot is the tip of the arrow, so the point the user aims at is
       the point that lands. */
    Cursor cursor = XCreatePixmapCursor(core->display, source, mask,
                                        &foreground, &background, 0, 0);
    XFreePixmap(core->display, source);
    XFreePixmap(core->display, mask);
    return cursor;
}

static int core_create_buffer(DmCore *core) {
    core->buffer = XCreatePixmap(core->display, core->window,
                                 (unsigned int)core->width,
                                 (unsigned int)core->height,
                                 (unsigned int)DefaultDepth(core->display, core->screen));
    return core->buffer != None ? 0 : -1;
}

int dm_register(DmCore *core, const DmModule *module) {
    (void)core;
    if (!module || !module->name) return -1;
    if (g_modules.count >= DM_MAX_MODULES) return -1;
    g_modules.items[g_modules.count++] = module;
    return 0;
}

int dm_core_init(DmCore *core) {
    memset(core, 0, sizeof(*core));

    core->display = XOpenDisplay(NULL);
    if (!core->display) {
        fprintf(stderr, "gnuchandm: cannot open the X display\n");
        return -1;
    }

    XSetErrorHandler(core_x_error);

    core->screen = DefaultScreen(core->display);
    core->root = RootWindow(core->display, core->screen);
    core->width = DisplayWidth(core->display, core->screen);
    core->height = DisplayHeight(core->display, core->screen);
    core->running = 1;
    core->focus = DM_FOCUS_USERNAME;

    if (dm_style_load(&core->style, core->display, core->screen) != 0) return -1;

    /* The sessions are read once, here, rather than at every redraw: what the
       machine offers does not change while the greeter is running, and a
       directory read per frame would be a file system walk per keystroke.
       GnuChanWM is put first by the scan, so index 0 is the default. */
    core->session_count = dm_sessions_scan(core->sessions, DM_MAX_SESSIONS);
    core->session_selected = core->session_count > 0 ? 0 : -1;
    core->session_open = 0;
    core->session_hover = -1;

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.background;
    /* PointerMotionMask is what makes the dropped-down session list highlight
       the row under the pointer: without it the list is a set of names with no
       sign of which one a click would land on. */
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            PointerMotionMask |
                            StructureNotifyMask | FocusChangeMask;

    core->cursor = core_make_cursor(core);
    if (core->cursor != None) {
        attributes.cursor = core->cursor;
    }

    core->window = XCreateWindow(
        core->display, core->root,
        0, 0, (unsigned int)core->width, (unsigned int)core->height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask | CWCursor, &attributes);
    if (core->window == None) return -1;

    core->gc = XCreateGC(core->display, core->window, 0, NULL);
    if (core->gc == NULL) return -1;

    if (core_create_buffer(core) != 0) return -1;

    /* Map first, then take the keyboard. XSetInputFocus on a window that is not
       viewable is a BadMatch: the request is refused, the error handler prints
       it, and the keyboard ends up nowhere — so the user types a password into
       a screen that never receives a key. The focus request is only meaningful
       once the window is on the screen, which is why it comes second. */
    XMapRaised(core->display, core->window);
    XSetInputFocus(core->display, core->window, RevertToPointerRoot, CurrentTime);
    XSync(core->display, False);

    /* A login screen should not blank while it waits: the machine has only
       just booted and nobody has touched the pointer yet. */
    dm_core_wake_screen(core);
    return 0;
}

int dm_core_start(DmCore *core) {
    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->init && module->init(core) != 0) {
            fprintf(stderr, "gnuchandm: module '%s' failed\n", module->name);
            return -1;
        }
    }
    dm_core_redraw(core);
    return 0;
}

void dm_core_redraw(DmCore *core) {
    if (!core->display || core->window == None || core->buffer == None) return;
    /* While a session is being started the greeter is handing the screen over
       and must not draw over it. */
    if (core->starting_session) return;

    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->draw) module->draw(core);
    }

    /* The frame is complete in the pixmap: show it in one operation. */
    XCopyArea(core->display, core->buffer, core->window, core->gc,
              0, 0, (unsigned int)core->width, (unsigned int)core->height, 0, 0);
    XFlush(core->display);
}

/* Run a program and wait for it, for the screen commands below. Best-effort:
   a machine without xset still gets the Xlib half of dm_core_wake_screen,
   which is most of the fix, and a screen that could not be woken is not a
   reason to stop the greeter from running. */
static void core_run(const char *program, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(program, argv);
        _exit(127);
    }
    if (pid > 0) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { /* retry */ }
    }
}

void dm_core_wake_screen(DmCore *core) {
    if (!core->display) {
        return;
    }

    /* The screensaver is the server's own idea of "the user has stopped
       typing", and it outlives a session: a desktop that armed it leaves it
       armed, so the login screen that comes back is blanked on the same timer.
       Zero disarms it; the reset call wakes it if it is already blanked. */
    XSetScreenSaver(core->display, 0, 0, DontPreferBlanking, DontAllowExposures);
    XForceScreenSaver(core->display, ScreenSaverReset);

    /* The monitor itself is the other half. A session that powered it down
       leaves the X server holding it down, and a login screen drawn into a
       monitor that is off looks exactly like a greeter that never started.
       This is the DPMS extension, which Xlib does not expose, so it is asked
       for through xset — the one program on every Debian that speaks to it. */
    char *screensaver_off[] = { "xset", "s", "off", NULL };
    char *screensaver_reset[] = { "xset", "s", "reset", NULL };
    char *dpms_on[] = { "xset", "-dpms", NULL };
    char *dpms_force[] = { "xset", "dpms", "force", "on", NULL };
    core_run("xset", screensaver_off);
    core_run("xset", screensaver_reset);
    core_run("xset", dpms_on);
    core_run("xset", dpms_force);

    XSync(core->display, False);
}

void dm_core_step(DmCore *core) {
    XEvent event;
    XNextEvent(core->display, &event);

    if (event.type == ConfigureNotify) {
        core->width = event.xconfigure.width;
        core->height = event.xconfigure.height;
        if (core->buffer != None) XFreePixmap(core->display, core->buffer);
        if (core_create_buffer(core) != 0) return;
    }

    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->event) module->event(core, &event);
    }

    switch (event.type) {
    case Expose:
    case ConfigureNotify:
    case KeyPress:
    case ButtonPress:
    case FocusIn:
        dm_core_redraw(core);
        break;
    case MotionNotify:
        /* Only a redraw is needed here, and only if the input module changed
           which session row is highlighted; it asks for one itself when it
           does. Redrawing on every motion event would redraw for a pointer
           that moved a pixel with the list closed. */
        break;
    default:
        break;
    }
}

void dm_core_shutdown(DmCore *core) {
    for (int i = g_modules.count - 1; i >= 0; i--) {
        const DmModule *module = g_modules.items[i];
        if (module->cleanup) module->cleanup(core);
    }
    g_modules.count = 0;
    dm_form_clear_password(core);

    if (core->buffer != None) {
        XFreePixmap(core->display, core->buffer);
        core->buffer = None;
    }
    if (core->cursor != None) {
        XFreeCursor(core->display, core->cursor);
        core->cursor = None;
    }
    if (core->gc != NULL) {
        XFreeGC(core->display, core->gc);
        core->gc = NULL;
    }
    if (core->window != None) {
        XDestroyWindow(core->display, core->window);
        core->window = None;
    }
    if (core->display) {
        dm_style_free(&core->style, core->display);
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
