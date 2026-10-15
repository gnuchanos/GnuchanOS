/*
 * inst.h - the GnuchanOS installer's shared pieces.
 *
 * The installer is three parts that agree through this header:
 *
 *   inst_ui.c      the window: the fields, the button, the output pane. It is
 *                  Xlib and Xft, the same toolkit every other Gnuchan program
 *                  is written in (see GnuChanSL, GnuChanDM, GnuChanDock).
 *   inst_engine.c  the install: it runs gnuchanos-install.sh and hands its
 *                  output lines to the window as they come.
 *   inst_disks.c   the disks: it asks lsblk which ones there are.
 *
 * The window does NOT touch a disk. Every step that writes to one is the shell
 * script, run by the engine, printing what it prints; the window only collects
 * the four answers the person has to give and shows that output. That is the
 * split that keeps a failure visible - the script says why in its own words.
 */

#ifndef GNUCHANOS_INST_H
#define GNUCHANOS_INST_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

/* --- the palette ----------------------------------------------------------
 *
 * The dark purple the desktop and the lock screen already use, so the installer
 * looks like the rest of the system. Kept here rather than in inst_ui.c because
 * GnuChanInstall.c loads them into app->colours before any drawing.
 */
#define COL_BG        "#0e0614"
#define COL_PANEL     "#180a22"
#define COL_EDGE      "#32143f"
#define COL_FIELD     "#0b0410"
#define COL_FIELD_ON  "#24102f"
#define COL_TEXT      "#ead7ff"
#define COL_MUTED     "#b78ed6"
#define COL_ACCENT    "#a855f7"
#define COL_DANGER    "#d8a4ff"
#define COL_GOOD      "#b76eff"

/* --- limits ---------------------------------------------------------------- */

#define INST_MAX_FIELDS 24
#define INST_FIELD_TEXT 256
#define INST_OUTPUT_LINES 2000
#define INST_OUTPUT_WIDTH 512
#define INST_MAX_DISKS 16
#define INST_DISK_DROPDOWN_ROWS 4

/* --- the colours the window is drawn in ----------------------------------- */

typedef struct InstColours {
    XftColor background;
    XftColor panel;
    XftColor panel_edge;
    XftColor field;
    XftColor field_active;
    XftColor text;
    XftColor text_muted;
    XftColor accent;
    XftColor danger;
    XftColor good;
} InstColours;

/* --- one form field -------------------------------------------------------- */

typedef enum InstFieldKind {
    INST_FIELD_PLAIN,     /* ordinary text (login name, host name)            */
    INST_FIELD_SECRET     /* shown as dots (the password and its repeat)      */
} InstFieldKind;

typedef struct InstField {
    const char *label;             /* what is drawn to the left of the box     */
    InstFieldKind kind;
    char text[INST_FIELD_TEXT];
    int length;
    int box_x, box_y, box_w, box_h;   /* set while laying the window out       */
} InstField;

/* --- the disk list --------------------------------------------------------- */

typedef struct InstDisk {
    char device[64];               /* /dev/sda                                  */
    char description[160];         /* size, model - what the person reads       */
} InstDisk;

/* --- the whole window ------------------------------------------------------ */

typedef struct InstApp {
    Display *display;
    int screen;
    Window window;
    Pixmap buffer;                 /* one frame is built here, then copied     */
    int width, height;
    int depth;

    XftFont *font;
    XftFont *font_bold;
    XftDraw *draw;
    GC gc;
    InstColours colours;

    /* the disks, and which one is chosen (-1 = none) */
    InstDisk disks[INST_MAX_DISKS];
    int disk_count;
    int disk_choice;
    int disk_dropdown_open;
    int disk_dropdown_cursor;
    int disk_dropdown_scroll;

    /* the fields, and which one has the keyboard (-1 = none) */
    InstField fields[INST_MAX_FIELDS];
    int field_count;
    int field_active;

    /* the button */
    int button_x, button_y, button_w, button_h;
    int button_hot;                /* the pointer is over it                    */
    int confirming_erase;
    int erase_confirmed;
    char erase_confirmation[6];
    int erase_box_x, erase_box_y, erase_box_w, erase_box_h;

    /* the output pane: a ring of the last lines the script printed */
    char *lines[INST_OUTPUT_LINES];
    int line_count;                /* how many slots are used                   */
    int line_head;                 /* the oldest of them                        */

    /* the engine: the shell script as a child process on a pipe */
    int running;                   /* an install is in progress                 */
    int finished;                  /* and it has ended                          */
    int exit_code;                 /* the engine's own exit status              */
    int engine_fd;                 /* read end of its output pipe, or -1        */
    int engine_pid;                /* its process id, or -1                     */
    char engine_buf[8192];         /* bytes read but not yet split into lines   */
    int engine_buf_len;

    /* a one-line note under the button: what went wrong, or what is happening */
    char note[256];
    int note_is_error;

    int should_close;
} InstApp;

/* --- colours (inst_ui.c) --------------------------------------------------- */

int inst_load_colour(InstApp *app, const char *name, const char *fallback,
                     XftColor *out);
void inst_free_colours(InstApp *app);

/* --- disks (inst_disks.c) -------------------------------------------------- */

/* Fill app->disks with the disks lsblk reports, leaving out the one the live
   system is running from. Returns the number found. */
int inst_list_disks(InstApp *app);

/* --- the engine (inst_engine.c) -------------------------------------------- */

/* Start gnuchanos-install.sh with the form's answers in its environment. It
   runs as a CHILD PROCESS with a pipe on its output; lines arrive through
   inst_engine_poll(). No thread is used, so every X call stays on the one
   thread that owns the display. Returns 1 if it started, 0 if it could not
   (the reason is in app->note). */
int inst_engine_start(InstApp *app);

/* Read whatever the child has written since the last call and append it to the
   output pane, splitting on newlines. Returns 1 while the child is running,
   0 once it has exited. The pipe is non-blocking, so this returns at once. */
int inst_engine_poll(InstApp *app);

/* The file descriptor to watch with select() while the engine runs, or -1. */
int inst_engine_fd(InstApp *app);

/* --- the window (inst_ui.c) ------------------------------------------------ */

void inst_layout(InstApp *app);
void inst_draw(InstApp *app);
void inst_add_line(InstApp *app, const char *text);
void inst_set_note(InstApp *app, int is_error, const char *fmt, ...);
int inst_validate(InstApp *app);

/* Which field the pointer is over, or the button, for the hover highlight. */
void inst_pointer_motion(InstApp *app, int x, int y);
/* A click: choose a disk, focus a field, or press the button. Starts the
   install when the button is pressed and the answers are valid. */
void inst_button_press(InstApp *app, int x, int y);

#endif /* GNUCHANOS_INST_H */
