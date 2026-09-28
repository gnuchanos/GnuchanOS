/*
 * runner_ui.c — the window, the grab, and the keys.
 *
 * This is where the launcher meets the display. Everything it is made of has
 * already been read and resolved by the time this runs: the config, the
 * palette, the font, the list of programs. What is left is a window, a grab,
 * and a loop that turns key presses into a query, a query into matches, and a
 * press of Enter into a choice.
 *
 * The grab is the part worth stating. The launcher takes the keyboard and the
 * pointer for as long as it is up:
 *
 *   The keyboard, because the first letter typed has to reach the launcher and
 *   not whatever had the focus before it. Without it a person opens the
 *   launcher, types "fire", and watches a letter appear in the terminal
 *   underneath.
 *
 *   The pointer, because a click anywhere has to be the launcher's — either on
 *   a row, or somewhere that means "go away". Without it a click on the
 *   desktop would go to the desktop and the launcher would sit there.
 *
 * Escape dismisses it, Enter chooses, the arrows move, and a printable letter
 * is appended to the query. That is the whole of the input, and it is small
 * because a launcher's input should be: anything more elaborate is a program
 * with a window, not a thing that starts one.
 */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "runner_draw.h"
#include "runner_ui.h"

/* Where the window sits on the screen: centred, hugging the top, or hugging
   the bottom, each with a margin so it is not flush against the edge. The
   position names come from the config, and anything the launcher does not know
   is centred — a misspelled position should put the window somewhere, not
   nowhere. */
static void work_out_geometry(RunnerUi *ui) {
    int rows = ui->config.rows;
    int visible = ui->matches.count < rows ? ui->matches.count : rows;
    if (visible < 1) {
        visible = 1;
    }

    ui->width = ui->config.width;
    if (ui->width > ui->screen_width) {
        ui->width = ui->screen_width;
    }

    /* The height is the query line, the gap under it, the rows, and the room
       the border takes. It grows with the number of matches only up to the
       configured number of rows, so the launcher is the same size whether the
       query found two programs or two hundred — which is what makes it
       readable while a person is typing. */
    ui->height = ui->style.row_height + 2
               + visible * ui->style.row_height;

    int x = (ui->screen_width - ui->width) / 2;
    int y;

    if (strcmp(ui->config.position, "top") == 0) {
        y = 60;
    } else if (strcmp(ui->config.position, "bottom") == 0) {
        y = ui->screen_height - ui->height - 60;
        if (y < 0) {
            y = 0;
        }
    } else {
        y = (ui->screen_height - ui->height) / 3;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    XMoveResizeWindow(ui->display, ui->window, x, y,
                      (unsigned int)ui->width, (unsigned int)ui->height);
}

/* Work out where the command line belongs in the matches, and whether it is
   offered at all.
 *
 * There is one command line and it is always either at the end of the list or
 * absent. It is absent when the config turned it off, and when the config
 * offers it only for a query that matched nothing and the query did match
 * something. It is present otherwise — after the last match, so choosing the
 * first program is still one press of Enter with nothing typed.
 */
static void work_out_command_row(RunnerUi *ui) {
    ui->command_row = -1;
    if (ui->config.command_mode == RUNNER_COMMAND_OFF) {
        return;
    }
    if (ui->query_length == 0) {
        /* With nothing typed there is no command line: a row that would run an
           empty command is a row that does nothing. */
        return;
    }
    if (ui->config.command_mode == RUNNER_COMMAND_TYPED &&
        ui->matches.count > 0) {
        /* Offered only once nothing matches — see the config's own note. */
        return;
    }
    ui->command_row = ui->matches.count;
}

/* Re-run the query and put the list back at a sane place.
 *
 * The chosen row and the scroll are set here rather than by each key: every
 * key that changes the query makes the old chosen index mean a different
 * program, so a list that kept it would jump to an unrelated entry on the
 * first letter typed. Choosing the first match is what a launcher does, and it
 * is what makes Enter work with nothing selected by hand.
 */
static void refresh(RunnerUi *ui) {
    runner_match_query(&ui->matches, &ui->programs, &ui->config, ui->query);
    work_out_command_row(ui);

    /* The first row is chosen: the best match, or — when nothing matched and a
       command line is offered — the command line itself, so typing a command
       and pressing Enter runs it without a second keystroke. */
    if (ui->matches.count > 0) {
        ui->selected = 0;
    } else if (ui->command_row >= 0) {
        ui->selected = ui->command_row;
    } else {
        ui->selected = -1;
    }
    ui->scroll = 0;
    work_out_geometry(ui);
}

/* How many rows fit in the window, which is what the scroll is kept inside. */
static int visible_rows(const RunnerUi *ui) {
    int rows = ui->config.rows;
    int room = (ui->height - ui->style.row_height - 2) / ui->style.row_height;
    if (room < rows) {
        rows = room;
    }
    return rows < 1 ? 1 : rows;
}

/* The number of rows the list has: the matches, plus the command line when it
   is offered. */
static int total_rows(const RunnerUi *ui) {
    return ui->matches.count + (ui->command_row >= 0 ? 1 : 0);
}

/* Keep the chosen row inside the visible window, moving the scroll under it
   rather than the row. This is what makes the arrows work at the end of a long
   list: the chosen row walks off the bottom and the list follows. */
static void keep_selected_visible(RunnerUi *ui) {
    int visible = visible_rows(ui);
    if (ui->selected < ui->scroll) {
        ui->scroll = ui->selected;
    }
    if (ui->selected >= ui->scroll + visible) {
        ui->scroll = ui->selected - visible + 1;
    }
    if (ui->scroll < 0) {
        ui->scroll = 0;
    }
}

/* Move the chosen row by `step`, wrapping round the ends. Wrapping rather than
   stopping is what a launcher does — the list is short and the two ends are
   next to each other in a person's mind — and it is one line of arithmetic. */
static void move_selection(RunnerUi *ui, int step) {
    int rows = total_rows(ui);
    if (rows <= 0) {
        ui->selected = -1;
        return;
    }
    if (ui->selected < 0) {
        ui->selected = 0;
    } else {
        ui->selected += step;
        while (ui->selected < 0) {
            ui->selected += rows;
        }
        while (ui->selected >= rows) {
            ui->selected -= rows;
        }
    }
    keep_selected_visible(ui);
}

/* Append one typed character to the query, if it is printable and there is
   room. A control character is not printable and has no place in a program's
   name; the ceiling is what keeps a key held down from growing the buffer. */
static int query_append(RunnerUi *ui, const char *text) {
    if (!text || !text[0]) {
        return 0;
    }
    size_t length = strlen(text);
    if (ui->query_length + (int)length >= RUNNER_MAX_QUERY) {
        return 0;
    }
    memcpy(ui->query + ui->query_length, text, length);
    ui->query_length += (int)length;
    ui->query[ui->query_length] = '\0';
    return 1;
}

static int query_backspace(RunnerUi *ui) {
    if (ui->query_length == 0) {
        return 0;
    }
    ui->query_length--;
    ui->query[ui->query_length] = '\0';
    return 1;
}

/* Choose whatever row is selected. A program row sets launch_selected to the
   program's index; the command row puts what was typed into launch_command.
   The two are told apart here and nowhere else, which is what keeps "what does
   Enter do" one question with one answer. */
static void choose_selected(RunnerUi *ui) {
    if (ui->selected < 0) {
        return;
    }
    if (ui->selected == ui->command_row) {
        snprintf(ui->launch_command, sizeof(ui->launch_command), "%s",
                 ui->query);
        ui->launch_selected = -1;
        ui->running = 0;
        return;
    }
    if (ui->selected < ui->matches.count) {
        ui->launch_selected = ui->matches.items[ui->selected].program_index;
        ui->launch_command[0] = '\0';
        ui->running = 0;
    }
}

/* Choose the nth row of the visible window, for a click: the row a click
   landed on, not the one the keyboard was on. */
static void choose_clicked_row(RunnerUi *ui, int y) {
    int top = ui->style.row_height + 2;
    if (y < top) {
        return;         /* the click was on the query line */
    }
    int row = (y - top) / ui->style.row_height;
    int index = ui->scroll + row;
    if (index < 0 || index >= total_rows(ui)) {
        return;
    }
    ui->selected = index;
    choose_selected(ui);
}

/* --- the keys ------------------------------------------------------------- */

static void handle_key(RunnerUi *ui, XKeyEvent *key) {
    /* The keysym is looked up with the shift and caps state the event carries,
       so a capital letter and the digit above it are told apart. A keysym of
       NoSymbol is a key with nothing on it, which is nothing to do. */
    KeySym symbol = XLookupKeysym(key, 0);
    if (symbol == NoSymbol) {
        return;
    }

    switch (symbol) {
    case XK_Escape:
        ui->running = 0;
        ui->launch_selected = -1;
        ui->launch_command[0] = '\0';
        return;

    case XK_Return:
    case XK_KP_Enter:
        choose_selected(ui);
        return;

    case XK_BackSpace:
        if (query_backspace(ui)) {
            refresh(ui);
            runner_draw(ui);
        }
        return;

    case XK_Up:
    case XK_KP_Up:
        move_selection(ui, -1);
        runner_draw(ui);
        return;

    case XK_Down:
    case XK_KP_Down:
        move_selection(ui, 1);
        runner_draw(ui);
        return;

    case XK_Page_Up:
        move_selection(ui, -visible_rows(ui));
        runner_draw(ui);
        return;

    case XK_Page_Down:
        move_selection(ui, visible_rows(ui));
        runner_draw(ui);
        return;

    case XK_Home:
        ui->selected = 0;
        keep_selected_visible(ui);
        runner_draw(ui);
        return;

    case XK_End:
        ui->selected = total_rows(ui) - 1;
        keep_selected_visible(ui);
        runner_draw(ui);
        return;

    default:
        break;
    }

    /* A letter, a digit, a space, a punctuation mark: whatever the key
       produced as text. XLookupString writes the character the key stands for
       in the current layout, which is why it is asked rather than the keysym
       being turned into a character by hand — a keyboard that is not the one
       the launcher was written on still types what it says it types. */
    char text[8];
    int length = XLookupString(key, text, sizeof(text) - 1, NULL, NULL);
    if (length <= 0) {
        return;
    }
    text[length] = '\0';

    /* Only characters that show on a line: a control character is not part of
       a program's name, and appending one would put an invisible byte in the
       query that a later backspace would have to remove one at a time. */
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 || c == 0x7f) {
            return;
        }
    }

    if (query_append(ui, text)) {
        refresh(ui);
        runner_draw(ui);
    }
}

/* --- the window ----------------------------------------------------------- */

/* Take the keyboard and the pointer, and say whether the keyboard was taken.
   A keyboard that could not be grabbed is not fatal — the launcher still works
   with the pointer, and the keys go where they were going — but it is worth
   knowing about, because it is the one failure that makes typing do something
   else. */
static int grab_input(RunnerUi *ui) {
    XGrabPointer(ui->display, ui->window, False,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    int status = XGrabKeyboard(ui->display, ui->window, False,
                               GrabModeAsync, GrabModeAsync, CurrentTime);
    return status == GrabSuccess;
}

static void ungrab_input(RunnerUi *ui) {
    XUngrabKeyboard(ui->display, CurrentTime);
    XUngrabPointer(ui->display, CurrentTime);
}

int runner_ui_open(RunnerUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui));
    ui->launch_selected = -1;
    ui->command_row = -1;
    ui->selected = -1;

    runner_apps_init(&ui->programs);
    runner_match_init(&ui->matches);

    /* The locale first, so XLookupString returns the characters the keyboard
       actually types rather than assuming Latin-1. */
    setlocale(LC_ALL, "");

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "gnuchanrunner: cannot open the X display\n");
        return -1;
    }
    ui->screen = DefaultScreen(ui->display);
    ui->root = RootWindow(ui->display, ui->screen);
    ui->screen_width = DisplayWidth(ui->display, ui->screen);
    ui->screen_height = DisplayHeight(ui->display, ui->screen);

    /* The settings, then the palette, then the programs. In that order: the
       palette is built from the config's colour names, and the scan is given
       the config's own list of directories. */
    char found_path[RUNNER_TEXT_LENGTH * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = runner_config_path(found_path, sizeof(found_path));
    }
    if (runner_config_load(&ui->config, path) != 0) {
        /* A settings file that could not be read is not fatal: the defaults
           are already in place — runner_config_load lays them down first — and
           the launcher starts with them rather than not starting. The reason
           is written down where someone can read it. */
        fprintf(stderr, "gnuchanrunner: %s; using the defaults\n",
                ui->config.error);
    }

    runner_style_load(&ui->style, ui->display, ui->screen, &ui->config);
    runner_apps_load(&ui->programs, &ui->config);

    ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
    if (!ui->gc) {
        fprintf(stderr, "gnuchanrunner: cannot make a graphics context\n");
        return -1;
    }

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    /* Override-redirect: the window manager never frames this window. A
       launcher with a title bar is not a launcher, and the window is up for a
       few seconds — there is nothing to manage. */
    attributes.override_redirect = True;
    attributes.background_pixel = ui->style.background;
    attributes.border_pixel = 0;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            ButtonReleaseMask | PointerMotionMask;

    ui->window = XCreateWindow(ui->display, ui->root, 0, 0,
                               (unsigned int)ui->config.width, 100, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel |
                               CWBorderPixel | CWEventMask, &attributes);
    if (ui->window == None) {
        fprintf(stderr, "gnuchanrunner: cannot make the window\n");
        return -1;
    }

    /* The query is run once before the window is placed, so the window opens
       at the size of what it is going to show rather than at the size of
       nothing and then growing. */
    refresh(ui);
    XMapRaised(ui->display, ui->window);
    XFlush(ui->display);

    grab_input(ui);
    runner_draw(ui);
    ui->running = 1;
    return 0;
}

int runner_ui_run(RunnerUi *ui) {
    if (!ui->display || ui->window == None) {
        return -1;
    }

    while (ui->running) {
        XEvent event;
        XNextEvent(ui->display, &event);

        switch (event.type) {
        case KeyPress:
            handle_key(ui, &event.xkey);
            break;

        case ButtonPress:
            /* A click on a row chooses it and runs it; a click anywhere else
               dismisses the launcher. This is what makes it feel like a
               launcher rather than a window that has to be closed: a click on
               the desktop is "I did not mean it". */
            if (event.xbutton.x >= 0 && event.xbutton.x < ui->width &&
                event.xbutton.y >= 0 && event.xbutton.y < ui->height &&
                event.xbutton.y >= ui->style.row_height) {
                choose_clicked_row(ui, event.xbutton.y);
            } else {
                ui->running = 0;
                ui->launch_selected = -1;
                ui->launch_command[0] = '\0';
            }
            break;

        case Expose:
            if (event.xexpose.window == ui->window) {
                runner_draw(ui);
            }
            break;

        default:
            break;
        }
    }

    ungrab_input(ui);
    XFlush(ui->display);

    /* What was chosen, kept across the close. The caller runs it — see
       runner_ui.h — because starting a program is not the window's job and a
       window that starts programs cannot be tried without starting them. */
    if (ui->launch_selected >= 0 || ui->launch_command[0]) {
        return 0;
    }
    return -1;
}

void runner_ui_close(RunnerUi *ui) {
    if (ui->display) {
        ungrab_input(ui);
        if (ui->gc) {
            XFreeGC(ui->display, ui->gc);
            ui->gc = NULL;
        }
        if (ui->window != None) {
            XDestroyWindow(ui->display, ui->window);
            ui->window = None;
        }
        runner_style_free(&ui->style, ui->display);
        XCloseDisplay(ui->display);
        ui->display = NULL;
    }

    runner_apps_free(&ui->programs);
    runner_match_free(&ui->matches);
}
