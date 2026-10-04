/*
 * wm_config_apply.c — what the read script does to the desktop.
 *
 * wm_config_file.c reads the settings script into a WmConfig; this file is the
 * other half: the questions the rest of the manager asks of that config once
 * it is read, and the one place it is pushed out into the running desktop.
 *
 * It is a separate file because the two jobs have nothing in common but the
 * struct they share. The reader walks statements and knows what gcl_BAR means;
 * nothing here has ever seen a statement. What lives here is asked of the
 * config by name — which edges a bar occupies, what a mouse button does, where
 * a bar is on a screen this size, what a reload changes — and answers from the
 * plain data the reader left behind.
 *
 * It also owns the two windows the config module draws: the message window a
 * fault is shown in, and the module entry point itself.
 *
 * The reload is asked for by hand — the reload key — and never watched for: a
 * person presses Ctrl+Alt+R when they want the script read again, and the
 * desktop does not re-read the file behind their back. That is what lets a
 * read be all-or-nothing: the whole file is parsed into a copy and swapped in
 * only when it parsed, a mistake leaves the desktop it already had, and the
 * mistake is put in a window rather than silently applied half-read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "wm_config_parser.h"
#include "wm_core.h"
#include "wm_frame.h"
#include "wm_spawn.h"
#include "wm_theme.h"

unsigned int wm_config_bar_edges(const WmConfig *config) {
    if (!config) {
        return 0;
    }
    /* Every bar is asked, and the answer is the union of them: a session with
       a top bar and a bottom bar takes room at both edges, and a window is
       kept clear of each. A bar that placed itself with X and Y rather than
       naming an edge is not an edge bar and contributes nothing here — it is
       floating somewhere on the screen rather than against a side. */
    unsigned int edges = 0;
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        if (!bar->present || bar->size <= 0) {
            continue;
        }
        if (strcmp(bar->position, "bottom") == 0) {
            edges |= 1u << WM_EDGE_BOTTOM;
        } else if (strcmp(bar->position, "left") == 0) {
            edges |= 1u << WM_EDGE_LEFT;
        } else if (strcmp(bar->position, "right") == 0) {
            edges |= 1u << WM_EDGE_RIGHT;
        } else {
            edges |= 1u << WM_EDGE_TOP;
        }
    }
    return edges;
}

WmMouseAction wm_config_mouse_action(const WmConfig *config,
                                     unsigned int button) {
    if (!config) {
        return WM_MOUSE_NONE;
    }
    switch (button) {
    case 1: return config->mouse_left;
    case 2: return config->mouse_middle;
    case 3: return config->mouse_right;
    case 4: return config->mouse_scroll_up;
    case 5: return config->mouse_scroll_down;
    default: return WM_MOUSE_NONE;
    }
}

void wm_config_bar_rect(const WmBar *bar, int screen_width, int screen_height,
                        int *x, int *y, int *width, int *height) {
    int vertical;
    int size;
    int bx = 0;
    int by = 0;
    int bw = 0;
    int bh = 0;

    if (!bar) {
        *x = 0; *y = 0; *width = 0; *height = 0;
        return;
    }
    vertical = strcmp(bar->pose, "vertical") == 0;
    size = bar->size;
    if (size <= 0) {
        *x = 0; *y = 0; *width = 0; *height = 0;
        return;
    }

    /* Pose decides which way the bar runs and which way it is thick: a
       horizontal bar is as wide as the screen and as thick as Size, a vertical
       one is the other way round. Position then says which edge it hugs, so a
       right bar is pushed to the right edge and a bottom one down to the
       bottom edge. */
    if (vertical) {
        bw = size;
        bh = screen_height;
        if (strcmp(bar->position, "right") == 0) {
            bx = screen_width - size;
        }
    } else {
        bw = screen_width;
        bh = size;
        if (strcmp(bar->position, "bottom") == 0) {
            by = screen_height - size;
        }
    }

    /* X and Y, when written, are the bar's own corner and win over the edge
       Position chose. -1 is "not written" and leaves the edge where it is;
       zero is the very corner of the screen and is kept. */
    if (bar->x >= 0) {
        bx = bar->x;
    }
    if (bar->y >= 0) {
        by = bar->y;
    }

    /* The empty spaces pull the two ends of the bar in along its own axis —
       Left/Right for a horizontal bar, Up/Down for a vertical one — which is
       also why a bar with Left_EmptySpace=5 no longer starts at x=0. A pair
       that would leave nothing is dropped rather than making the bar negative
       wide. */
    if (vertical) {
        if (bar->up_empty + bar->down_empty < bh) {
            by += bar->up_empty;
            bh -= bar->up_empty + bar->down_empty;
        }
    } else {
        if (bar->left_empty + bar->right_empty < bw) {
            bx += bar->left_empty;
            bw -= bar->left_empty + bar->right_empty;
        }
    }

    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;
    *x = bx;
    *y = by;
    *width = bw;
    *height = bh;
}

void wm_config_workarea(const WmConfig *config, int screen_width,
                        int screen_height, int *x, int *y,
                        int *width, int *height) {
    int wx = 0;
    int wy = 0;
    int ww = screen_width;
    int wh = screen_height;

    if (!config) {
        *x = wx; *y = wy; *width = ww; *height = wh;
        return;
    }

    /* Every bar takes the strip it ACTUALLY covers out of the workarea, one
       after another — a top bar and a bottom bar both come off the height, a
       left bar off the width. The strip is the bar's real rectangle, not
       "the edge, Size thick": a bar with Y=10 and empty spaces five pixels in
       at each end sits at x=5, y=10 and is 24 tall, so the workarea has to
       begin at 34 and not at 24. Deriving it from wm_config_bar_rect() is what
       keeps the space a window is kept out of the same as the space the bar
       covers — the two used to be separate rules and disagreed exactly for a
       bar that named an X, a Y or an empty space. A strip that would leave
       nothing is held so at least one pixel of screen is left, which is what
       stops a script asking for a bar thicker than the screen from producing a
       negative workarea. */
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        int bx, by, bw, bh;

        if (!bar->present || bar->size <= 0) {
            continue;
        }
        wm_config_bar_rect(bar, screen_width, screen_height, &bx, &by, &bw, &bh);

        if (strcmp(bar->position, "bottom") == 0) {
            int room = by - wy;              /* bar's top edge is the new floor */
            if (room < 1) room = 1;
            wh = room;
        } else if (strcmp(bar->position, "left") == 0) {
            int right = bx + bw;             /* bar's right edge */
            int room = (wx + ww) - right;
            if (right < wx) right = wx;
            if (room < 1) room = 1;
            wx = right;
            ww = room;
        } else if (strcmp(bar->position, "right") == 0) {
            int room = bx - wx;              /* bar's left edge */
            if (room < 1) room = 1;
            ww = room;
        } else {                             /* "top", or no position written */
            int bottom = by + bh;            /* bar's bottom edge */
            int room = (wy + wh) - bottom;
            if (bottom < wy) bottom = wy;
            if (room < 1) room = 1;
            wy = bottom;
            wh = room;
        }
    }

    *x = wx;
    *y = wy;
    *width = ww;
    *height = wh;
}

/* --- reporting what was read ----------------------------------------------
 *
 * A script that parsed but asked for nothing visible looks exactly like a
 * script that never loaded: the only trace either leaves is one line saying a
 * file was read. The summary below is what tells the two apart. It is written
 * to the log, which is where a session with no terminal can be read back from.
 */

static void config_report(const WmConfig *config, const char *origin) {
    fprintf(stderr,
            "gnuchanwm: config %s: border %s / %s, %d binding(s), "
            "%d bar(s), %d workspace(s), terminal '%s'\n",
            origin,
            config->active_border[0] ? config->active_border : "(default)",
            config->inactive_border[0] ? config->inactive_border : "(default)",
            config->binding_count,
            config->bar_count,
            config->workspace_count,
            config->terminal[0] ? config->terminal : "(from $TERMINAL)");
    /* One line per bar, so a wrong pose or a bar that did not appear can be
       told apart from a bar that was never read. The empty spaces named are
       the pair the bar's own pose uses, so the line never shows a number that
       was not applied. */
    for (int i = 0; i < config->bar_count; i++) {
        const WmBar *bar = &config->bars[i];
        int vertical = strcmp(bar->pose, "vertical") == 0;
        fprintf(stderr,
                "gnuchanwm: config %s: bar %d %s %s size %d, %d widget(s), "
                "at (%d,%d), space %d/%d\n",
                origin, i, bar->position[0] ? bar->position : "(top)",
                bar->pose[0] ? bar->pose : "horizontal",
                bar->size, bar->widget_count, bar->x, bar->y,
                vertical ? bar->up_empty : bar->left_empty,
                vertical ? bar->down_empty : bar->right_empty);
    }
    fprintf(stderr,
            "gnuchanwm: config %s: theme %s, icons %s, cursor %s\n",
            origin,
            config->gtk_theme[0] ? config->gtk_theme : "(default)",
            config->icon_theme[0] ? config->icon_theme : "(default)",
            config->cursor_theme[0] ? config->cursor_theme : "(default)");
    /* The programs the session will start, one line each, so a daemon that
       did not come up can be told apart from one that was never asked for:
       the log says whether the script named it and whether it was started. */
    for (int i = 0; i < config->autostart_count; i++) {
        fprintf(stderr, "gnuchanwm: config %s: autostart %d: %s\n",
                origin, i, config->autostart[i]);
    }
    if (config->autostart_count == 0) {
        fprintf(stderr, "gnuchanwm: config %s: no autostart programs\n",
                origin);
    }
}

/* --- applying it to the desktop ------------------------------------------- */

void wm_config_apply(WmCore *core) {
    if (!core || !core->display) {
        return;
    }

    /* The script's terminal is published to the spawn module, which Alt+Enter
       and the first window both go through. A name the machine cannot run is
       discarded there, so a typo falls back to $TERMINAL rather than leaving
       the key opening nothing. */
    wm_spawn_set_terminal(core->config.terminal);

    /* The two frame colours are the ones the script sets through gcl_Window;
       the rest of the palette stays what wm_style.c resolved. */
    if (core->config.active_border[0]) {
        core->style.border = wm_style_colour(core->display, core->screen,
                                             core->config.active_border,
                                             core->style.border);
    }
    if (core->config.inactive_border[0]) {
        core->style.border_unfocused =
            wm_style_colour(core->display, core->screen,
                            core->config.inactive_border,
                            core->style.border_unfocused);
    }
    /* The third colour, and the reason it is resolved here with the other two:
       a frame is drawn with all three of these at draw time, so a script that
       changed only this one has to reach the windows already on screen the
       same way a change to either of the others does. The fallback is the
       palette's own orange (wm_style.c), which is what a script that never
       names this colour keeps. */
    if (core->config.moved_border[0]) {
        core->style.border_moved =
            wm_style_colour(core->display, core->screen,
                            core->config.moved_border,
                            core->style.border_moved);
    }

    /* The switcher's seven, resolved the same way and for the same reason: the
       overlay is drawn from the style at draw time, so a script that changed
       one has to reach the running desktop. Each falls back to the palette's
       own value (wm_style.c), which is what a script that names none of them
       keeps — so the overlay a person gets is the one they had before any of
       this was configurable. */
    {
        struct {
            const char *written;
            unsigned long *resolved;
        } switcher[] = {
            { core->config.switcher_background,    &core->style.switcher_background    },
            { core->config.switcher_panel,         &core->style.switcher_panel         },
            { core->config.switcher_cell_border,   &core->style.switcher_cell_border   },
            { core->config.switcher_select_border, &core->style.switcher_select_border },
            { core->config.switcher_text,          &core->style.switcher_text          },
            { core->config.switcher_select_text,   &core->style.switcher_select_text   },
            { core->config.switcher_field,         &core->style.switcher_field         },
        };
        for (unsigned int i = 0; i < sizeof(switcher) / sizeof(switcher[0]); i++) {
            if (switcher[i].written[0]) {
                *switcher[i].resolved =
                    wm_style_colour(core->display, core->screen,
                                    switcher[i].written,
                                    *switcher[i].resolved);
            }
        }
    }
    if (core->config.border_width > 0) {
        core->style.border_width = core->config.border_width;
    }

    /* The border width is a number every open frame's geometry is computed
       from, so a script that changed it has to reach the windows that are
       already open. wm_frame_apply_border() puts each frame back together at
       the new width; a screen with no windows is a no-op. */
    wm_frame_apply_border(core);

    /* And every frame is drawn again, whatever the width did. The two colours
       above are what a frame's border and its title line are drawn in, and
       they are read at draw time — so a script that changed only a colour
       would otherwise leave the windows already on screen showing the old one
       until something else happened to repaint them. Doing it here, in the one
       place both reload paths go through, is what makes a colour change appear
       the moment the script is read. */
    wm_frame_draw_all(core);

    /* The theme names are published the same three ways the first run published
       them: the GTK settings files, the resource manager where the cursor is
       named, and the environment. This is not only setenv: a reload that
       changed gcl_themes.Theme_icon(...) has to reach ~/.config/gtk-3.0, and a
       reload that changed the cursor has to reach Xcursor.theme, or half the
       desktop keeps the theme the session started with. wm_theme_apply() is
       the whole of that, and it is the same call the module's init makes. */
    wm_theme_apply(core);

    /* The touchpad settings are pushed into the running X server again when
       they change. wm_input.c applies them at start, but a script whose
       TapToClick was just edited has to take effect on the next save rather
       than at the next login — which is what the whole reload path exists
       for, and a setting that waited would be the one part of the script that
       did not follow it. */
    wm_input_apply(core);

    /* Whatever the script asked for that could not be given — a bar with an
       empty-space setting that belongs to the other pose — is shown once here,
       because this is the one place both reads pass through and both have a
       core to draw on. The reader that found the mistake only had the file: it
       recorded the notes on the config (see WmConfig.notes and set_bar()) and
       the message goes up now, where the person who wrote the script will see
       it. The log has every note whether or not a message can be drawn. */
    if (core->config.notes[0]) {
        fprintf(stderr, "gnuchanwm: config notes: %s\n", core->config.notes);
        wm_config_show_message(core,
                               "GnuChanWM: the config was applied, with notes",
                               core->config.notes,
                               "the log has the same list");
    }
}

/* --- the window a config mistake is shown in ------------------------------ */

/* When the reload key asks for the script and it cannot be read, the reason is
   put on screen and not only in the log.
 *
 * A person who pressed the key is looking at the desktop, not at a file: a
 * change that silently did not happen reads as a key that does nothing, and
 * the log is somewhere a running session has no terminal to read. So the
 * mistake is shown where the person already is — the statement that could not
 * be understood, and the file it came from. The window is override-redirect so
 * the manager does not try to manage its own message, and it is closed by a
 * click or a key, which is the one gesture a reader will try. */
#define WM_ERROR_WINDOW_WIDTH  640
#define WM_ERROR_WINDOW_HEIGHT 168
#define WM_ERROR_PADDING       16
#define WM_ERROR_LINE_HEIGHT   22

static Window error_window = None;

/* The message window, or None when no message is up. Read by the menu's logout
   walk, which has to leave the windows this manager owns alone: killing one
   closes this manager's own connection, and the kills queued behind it are
   discarded with it. */
Window wm_config_error_window(void) {
    return error_window;
}
static char error_title[WM_CONFIG_TEXT_LENGTH];
static char error_detail[WM_CONFIG_TEXT_LENGTH * 2];
/* Room for "in " and a path: the path buffer the reload builds is four text
   lengths, so the line that names the file is one length larger than that. A
   short buffer here would silently clip the path of the very file the message
   exists to name. */
static char error_where[WM_CONFIG_TEXT_LENGTH * 5];

static void config_draw_error(WmCore *core) {
    if (error_window == None) {
        return;
    }
    Display *display = core->display;
    XftFont *font = core->style.font;

    XSetForeground(display, core->gc, core->style.panel);
    XFillRectangle(display, error_window, core->gc, 0, 0,
                   WM_ERROR_WINDOW_WIDTH, WM_ERROR_WINDOW_HEIGHT);
    XSetForeground(display, core->gc, core->style.accent);
    XDrawRectangle(display, error_window, core->gc, 0, 0,
                   WM_ERROR_WINDOW_WIDTH - 1, WM_ERROR_WINDOW_HEIGHT - 1);

    if (!font) {
        return;
    }
    int baseline = WM_ERROR_PADDING + font->ascent;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_title, core->style.accent);
    baseline += WM_ERROR_LINE_HEIGHT;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_detail, core->style.text);
    baseline += WM_ERROR_LINE_HEIGHT;
    wm_style_text(display, core->screen, error_window, font,
                  WM_ERROR_PADDING, baseline, error_where,
                  core->style.text_muted);
    XFlush(display);
}

/* Show a message on the screen. Public because the problems a session can have
   are not all the config module's: a settings picture that will not load, a
   key that cannot be bound, no terminal to open. Every one of them reaches the
   log — a session started by a display manager has no terminal, so the log is
   the record — but a log is where a person looks after they have already
   noticed something is wrong, and the whole point of a message is to be the
   thing they notice. So the same message that goes to the log is put in a
   window here.
 *
 * The window is override-redirect so the manager does not try to manage its own
 * message, and it is closed by a click or a key. A message already up is
 * replaced rather than stacked: two windows, one behind the other, is one
 * message nobody can read. */
void wm_config_show_message(WmCore *core, const char *title,
                            const char *detail, const char *where) {
    if (!core || !core->display) {
        return;
    }

    snprintf(error_title, sizeof(error_title), "%s",
             (title && title[0]) ? title : "GnuChanWM");
    snprintf(error_detail, sizeof(error_detail), "%s",
             (detail && detail[0]) ? detail : "something went wrong");
    snprintf(error_where, sizeof(error_where), "%s",
             (where && where[0]) ? where : "");

    fprintf(stderr, "gnuchanwm: message shown: %s (%s)\n",
            error_detail, error_where);

    if (error_window != None) {
        XDestroyWindow(core->display, error_window);
        error_window = None;
    }

    int x = (core->width - WM_ERROR_WINDOW_WIDTH) / 2;
    if (x < 0) {
        x = 0;
    }
    int y = (core->height - WM_ERROR_WINDOW_HEIGHT) / 3;
    if (y < 0) {
        y = 0;
    }

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.panel;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    error_window = XCreateWindow(core->display, core->root,
                                 x, y,
                                 WM_ERROR_WINDOW_WIDTH, WM_ERROR_WINDOW_HEIGHT,
                                 1, CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask,
                                 &attributes);
    if (error_window == None) {
        return;
    }
    XStoreName(core->display, error_window, "GnuChanWM");
    XMapRaised(core->display, error_window);
    XFlush(core->display);
    config_draw_error(core);
}

/* Show the reason the script could not be applied. Called from the forced
   reload, which is the path a person took by pressing the key, so it is the
   path where a message on screen is what they asked for. */
static void config_show_error(WmCore *core, const char *path,
                              const char *reason) {
    char where[WM_CONFIG_TEXT_LENGTH * 4 + 8];
    snprintf(where, sizeof(where), "in %s", path ? path : "");
    wm_config_show_message(core,
                           "GnuChanWM: the config was not applied",
                           (reason && reason[0]) ? reason
                                                 : "the file could not be read",
                           where);
}

int wm_config_reload_forced(WmCore *core) {
    if (!core) {
        return 0;
    }
    char path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(path, sizeof(path));
    if (!path[0]) {
        return 0;
    }

    /* The unchanged-file shortcut is deliberately skipped: the key means
       "read it again now", and a person who saved a change between two ticks
       would otherwise press it and see nothing. */
    WmConfig fresh = core->config;
    if (wm_config_load(&fresh, path) != 0) {
        /* A script that does not parse is not a half-desktop: the copy above
           is thrown away, the desktop keeps what it had, and the reason is
           put on screen. */
        config_show_error(core, path, wm_config_last_error());
        return 0;
    }

    core->config = fresh;
    wm_config_apply(core);
    config_report(&core->config, "reloaded");
    fprintf(stderr, "gnuchanwm: config reloaded from %s\n", path);
    return 1;
}

/* The module's event callback: the error window's own two gestures. Every
   other event is passed over, so adding this callback costs the rest of the
   loop one comparison. */
static void config_event(WmCore *core, XEvent *event) {
    if (error_window == None || event->xany.window != error_window) {
        return;
    }
    switch (event->type) {
    case Expose:
        if (event->xexpose.count == 0) {
            config_draw_error(core);
        }
        break;
    case ButtonPress:
    case KeyPress:
        XDestroyWindow(core->display, error_window);
        error_window = None;
        XFlush(core->display);
        break;
    default:
        break;
    }
}

/* --- the module ----------------------------------------------------------- */

static int config_init(WmCore *core) {
    if (!core) {
        return -1;
    }
    wm_config_defaults(&core->config);

    char path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(path, sizeof(path));

    struct stat info;
    if (path[0] && stat(path, &info) == 0 &&
        wm_config_load(&core->config, path) == 0) {
        fprintf(stderr, "gnuchanwm: config read from %s\n", path);
    } else {
        /* No script is not a failure: it is a machine that never wrote one,
           and it gets the built-in desktop — including the built-in bar. */
        fprintf(stderr, "gnuchanwm: no config at %s; using the defaults\n",
                path[0] ? path : "(no home directory)");
    }

    wm_config_apply(core);
    config_report(&core->config, "in use");
    return 0;
}

const WmModule wm_config_module = {
    .name = "config",
    .init = config_init,
    .event = config_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};
