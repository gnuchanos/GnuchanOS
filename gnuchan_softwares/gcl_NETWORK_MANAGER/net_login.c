/*
 * net_login.c — the password window.
 *
 * See net_login.h for why it is a window. What is here is the smallest window
 * that can ask one question: a title, a line of dots for what has been typed,
 * a hint, and keys read until Enter or Escape.
 *
 * It is drawn with the manager's own NetStyle — the same palette and font — so
 * the dialog is not a grey box in the middle of a purple desktop. The style is
 * loaded and freed here because the manager's window does not exist yet: this
 * is the thing that runs first.
 *
 * It is override-redirect and it grabs the keyboard. That is deliberate and it
 * is the opposite of the manager's own window: a password prompt must not be
 * moved off-screen, must not be behind anything, and must have every key typed
 * while it is up. The manager's window is the ordinary one; this is the
 * exception, and it earns it.
 */
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "net_login.h"
#include "net_style.h"

typedef struct LoginDialog {
    Display *display;
    int screen;
    Window window;
    GC gc;
    NetStyle style;

    /* The off-screen buffer every frame is drawn into before it is put up in
       one copy, so typing does not flicker. */
    Pixmap buffer;

    int width;
    int height;

    char password[NET_LOGIN_MAX];
    int length;

    int accepted;
} LoginDialog;

static Drawable target(const LoginDialog *dialog) {
    return dialog->buffer ? dialog->buffer : dialog->window;
}

static void fill(LoginDialog *dialog, unsigned long colour,
                 int x, int y, int width, int height) {
    XSetForeground(dialog->display, dialog->gc, colour);
    XFillRectangle(dialog->display, target(dialog), dialog->gc,
                   x, y, (unsigned int)width, (unsigned int)height);
}

static void text(LoginDialog *dialog, int x, int baseline,
                 const char *string, unsigned long colour) {
    net_style_text(dialog->display, dialog->screen, target(dialog),
                   dialog->style.font, x, baseline, string, colour);
}

static void draw(LoginDialog *dialog) {
    int pad = dialog->style.padding;
    int row = dialog->style.row_height;
    int ascent = dialog->style.font ? dialog->style.font->ascent : 12;

    fill(dialog, dialog->style.background, 0, 0,
         dialog->width, dialog->height);

    int y = pad;
    text(dialog, pad, y + ascent, "Administrator password needed",
         dialog->style.accent);
    y += row;

    text(dialog, pad, y + ascent,
         "Changing the DNS or an interface needs root.",
         dialog->style.text_muted);
    y += row;

    int field_h = row;
    fill(dialog, dialog->style.field, pad, y, dialog->width - 2 * pad,
         field_h);
    XSetForeground(dialog->display, dialog->gc, dialog->style.panel_edge);
    XDrawRectangle(dialog->display, target(dialog), dialog->gc,
                   pad, y, (unsigned)(dialog->width - 2 * pad - 1),
                   (unsigned)(field_h - 1));

    int dot_r = 3;
    int dot_gap = 14;
    int cx = pad + pad / 2 + dot_r;
    int cy = y + field_h / 2;
    XSetForeground(dialog->display, dialog->gc, dialog->style.text);
    for (int i = 0; i < dialog->length; i++) {
        XFillArc(dialog->display, target(dialog), dialog->gc,
                 cx + i * dot_gap - dot_r, cy - dot_r,
                 (unsigned)(2 * dot_r), (unsigned)(2 * dot_r), 0, 360 * 64);
    }
    fill(dialog, dialog->style.accent, cx + dialog->length * dot_gap,
         y + pad / 2, 2, field_h - pad);
    y += field_h + pad;

    text(dialog, pad, y + ascent, "Enter accepts, Escape cancels",
         dialog->style.text_muted);

    if (dialog->buffer) {
        XCopyArea(dialog->display, dialog->buffer, dialog->window, dialog->gc,
                  0, 0, (unsigned)dialog->width, (unsigned)dialog->height,
                  0, 0);
    }
    XFlush(dialog->display);
}

static int append(LoginDialog *dialog, const char *data) {
    size_t length = strlen(data);
    if (dialog->length + (int)length >= NET_LOGIN_MAX) {
        return 0;
    }
    memcpy(dialog->password + dialog->length, data, length);
    dialog->length += (int)length;
    dialog->password[dialog->length] = '\0';
    return 1;
}

static int backspace(LoginDialog *dialog) {
    if (dialog->length == 0) {
        return 0;
    }
    dialog->password[--dialog->length] = '\0';
    return 1;
}

/* Returns 1 when Enter or Escape was pressed — the dialog is finished. */
static int handle_key(LoginDialog *dialog, XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);

    if (symbol == XK_Escape) {
        return 1;
    }
    if (symbol == XK_Return || symbol == XK_KP_Enter) {
        dialog->accepted = 1;
        return 1;
    }
    if (symbol == XK_BackSpace) {
        if (backspace(dialog)) {
            draw(dialog);
        }
        return 0;
    }

    char buffer[8];
    int got = XLookupString(key, buffer, sizeof(buffer) - 1, NULL, NULL);
    if (got <= 0) {
        return 0;
    }
    buffer[got] = '\0';
    for (int i = 0; i < got; i++) {
        unsigned char c = (unsigned char)buffer[i];
        if (c < 0x20 || c == 0x7f) {
            return 0;
        }
    }
    if (append(dialog, buffer)) {
        draw(dialog);
    }
    return 0;
}

int net_login_prompt(const NetConfig *config, char *password,
                     unsigned int size) {
    if (password && size) {
        password[0] = '\0';
    }

    LoginDialog dialog;
    memset(&dialog, 0, sizeof(dialog));

    setlocale(LC_ALL, "");

    dialog.display = XOpenDisplay(NULL);
    if (!dialog.display) {
        return -1;
    }
    dialog.screen = DefaultScreen(dialog.display);

    net_style_load(&dialog.style, dialog.display, dialog.screen, config);

    int screen_w = DisplayWidth(dialog.display, dialog.screen);
    int screen_h = DisplayHeight(dialog.display, dialog.screen);

    int row = dialog.style.row_height;
    dialog.width = 420;
    if (dialog.width > screen_w - 40) {
        dialog.width = screen_w - 40;
    }
    dialog.height = 2 * dialog.style.padding + 4 * row;
    int x = (screen_w - dialog.width) / 2;
    int y = (screen_h - dialog.height) / 2;

    Window root = RootWindow(dialog.display, dialog.screen);
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.background_pixel = dialog.style.background;
    attributes.border_pixel = dialog.style.panel_edge;
    attributes.override_redirect = True;
    attributes.event_mask = KeyPressMask | ExposureMask | StructureNotifyMask;

    dialog.window = XCreateWindow(dialog.display, root, x, y,
                                  (unsigned)dialog.width,
                                  (unsigned)dialog.height, 1, CopyFromParent,
                                  InputOutput, CopyFromParent,
                                  CWBackPixel | CWBorderPixel |
                                  CWOverrideRedirect | CWEventMask,
                                  &attributes);
    if (dialog.window == None) {
        net_style_free(&dialog.style, dialog.display);
        XCloseDisplay(dialog.display);
        return -1;
    }

    dialog.gc = XCreateGC(dialog.display, dialog.window, 0, NULL);

    dialog.buffer = XCreatePixmap(dialog.display, dialog.window,
                                  (unsigned)dialog.width,
                                  (unsigned)dialog.height,
                                  (unsigned)DefaultDepth(dialog.display,
                                                         dialog.screen));

    XStoreName(dialog.display, dialog.window,
               "GnuChanNetworkManager — authentication");

    XMapRaised(dialog.display, dialog.window);
    XSetInputFocus(dialog.display, dialog.window, RevertToParent, CurrentTime);
    XGrabKeyboard(dialog.display, dialog.window, True, GrabModeAsync,
                  GrabModeAsync, CurrentTime);
    XFlush(dialog.display);

    draw(&dialog);

    while (1) {
        XEvent event;
        XNextEvent(dialog.display, &event);
        if (event.type == KeyPress) {
            if (handle_key(&dialog, &event.xkey)) {
                break;
            }
        } else if (event.type == Expose && event.xexpose.count == 0) {
            draw(&dialog);
        }
    }

    XUngrabKeyboard(dialog.display, CurrentTime);
    if (dialog.buffer) {
        XFreePixmap(dialog.display, dialog.buffer);
        dialog.buffer = None;
    }
    if (dialog.gc) {
        XFreeGC(dialog.display, dialog.gc);
    }
    XDestroyWindow(dialog.display, dialog.window);
    net_style_free(&dialog.style, dialog.display);
    XCloseDisplay(dialog.display);

    if (!dialog.accepted) {
        return -1;
    }
    if (password && size) {
        snprintf(password, size, "%s", dialog.password);
    }
    return 0;
}
