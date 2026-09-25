/*
 * dm_core.h — the X connection, the window, and the shared state modules are
 * given.
 *
 * The core owns the display, the one window the greeter draws in, the graphics
 * context, the palette, and the login form's state. Modules never open their
 * own display and never draw to their own window.
 *
 * The form state is here rather than in the input module because three modules
 * read it: input fills it, the drawing reads it, and the session module reads
 * the finished user name after a successful login.
 */
#ifndef GNUCHANDM_CORE_H
#define GNUCHANDM_CORE_H

#include <X11/Xlib.h>

#include "dm_module.h"
#include "dm_style.h"

/* The longest user name and password accepted. The password is a fixed buffer
   so it can be wiped without the allocator keeping a copy of it. */
#define DM_MAX_USERNAME 256
#define DM_MAX_PASSWORD 256

/* Which control has the keyboard. Tab moves between them. */
typedef enum DmField {
    DM_FIELD_USERNAME = 0,
    DM_FIELD_PASSWORD,
    DM_FIELD_COUNT,
} DmField;

/* Every clickable thing on the screen, for hit-testing a click. */
typedef enum DmButton {
    DM_BUTTON_NONE = -1,
    DM_BUTTON_SIGNIN = 0,
    DM_BUTTON_REBOOT,
    DM_BUTTON_SHUTDOWN,
} DmButton;

/* What the user asked for, set by the input module and acted on by the entry
   point after the event that set it has been handled. It is an action and not
   a call because a module must not start a session from inside an event —
   the event loop has to finish first. */
typedef enum DmAction {
    DM_ACTION_NONE = 0,
    DM_ACTION_LOGIN,
    DM_ACTION_REBOOT,
    DM_ACTION_SHUTDOWN,
} DmAction;

typedef struct DmModuleList {
    const DmModule *items[DM_MAX_MODULES];
    int count;
} DmModuleList;

struct DmCore {
    Display *display;
    int screen;
    Window root;
    Window window;
    GC gc;

    int width;                /* the window's width and height; the greeter  */
    int height;               /* redraws from these, so a resize just works */

    DmStyle style;
    int running;

    /* --- the login form --------------------------------------------------- */

    char username[DM_MAX_USERNAME];
    char password[DM_MAX_PASSWORD];
    int  username_length;
    int  password_length;
    DmField focused;

    /* A message under the fields: an error after a failed login, or the host
       name when there is nothing to report. Empty means show nothing. */
    char message[256];
    int  message_is_error;

    /* Set by the input module, read and cleared by the entry point. */
    DmAction action;

    /* Set by the session module while a session is being started, so the
       greeter can stop drawing and stop reading the keyboard. */
    int starting_session;

    /* Where the layout put each control, so the input module can hit-test
       against the same rectangles the drawing used. Filled by the login
       module during its layout pass and read back by everything else. */
    struct DmRect {
        int x, y, width, height;
    } field_box[DM_FIELD_COUNT];
    struct DmRect button_box;
    struct DmRect panel_box;
    struct DmRect power_box[2];   /* power_box[0] reboot, power_box[1] poweroff */
};

int  dm_core_init(DmCore *core);
int  dm_core_start(DmCore *core);

/* Read and dispatch exactly one event. */
void dm_core_step(DmCore *core);

/* Redraw the whole window: every module's draw, then the result on screen.
   Called once at start, and by a module that has changed what is shown. */
void dm_core_redraw(DmCore *core);

void dm_core_shutdown(DmCore *core);

/* Register a module. Call between dm_core_init() and dm_core_start(). */
int  dm_register(DmCore *core, const DmModule *module);

/* --- the state every module reads and writes ------------------------------ */

/* Append a character to the focused field, ignoring a control character. */
void dm_form_type(DmCore *core, char character);

/* Remove the last character of the focused field, if any. */
void dm_form_backspace(DmCore *core);

/* Move the keyboard to the next field, wrapping around. */
void dm_form_next_field(DmCore *core);

/* Put a message under the fields and redraw. An empty message clears it. */
void dm_form_set_message(DmCore *core, const char *message, int is_error);

/* Wipe the password and its length. Called when a login fails or finishes, so
   the password does not sit in memory longer than it has to. */
void dm_form_clear_password(DmCore *core);

/* --- the module entry points, one per file -------------------------------- */

extern const DmModule dm_login_module;    /* dm_login.c   */
extern const DmModule dm_input_module;    /* dm_input.c   */

/* Which button a point is inside, or DM_BUTTON_NONE. The login module owns
   the layout, so it is the only thing that can answer this — and answering it
   from the same rectangles it drew means a click always lands on what the
   user saw. DM_FIELD_COUNT is also tested: it returns the field that was hit,
   or DM_BUTTON_NONE, through the out parameter. */
DmButton dm_login_button_at(DmCore *core, int x, int y);
int      dm_login_field_at(DmCore *core, int x, int y);

/* dm_draw.c — the three primitives every drawing module uses. */
void dm_draw_clear(DmCore *core, unsigned long colour);
void dm_draw_box(DmCore *core, int x, int y, int width, int height,
                 unsigned long fill, unsigned long edge, int edge_width);
void dm_draw_text(DmCore *core, int x, int y, const char *text,
                  XFontStruct *font, unsigned long colour);
int  dm_draw_text_width(XFontStruct *font, const char *text, int length);

/* dm_auth.c — check a user name and a password. Returns 1 when the pair is
   correct, 0 when it is not. */
int dm_auth_check(const char *username, const char *password);

/* dm_session.c — start the window manager as the given user, and wait for it.
   The greeter stays alive as the parent so that when the session ends the
   login screen comes back: the greeter only truly exits when it is asked to.
   Returns 0 when the session ended and -1 when it could not be started. */
int dm_session_start(DmCore *core, const char *username);

/* dm_power.c — ask the system to reboot or to power off. Never returns. */
void dm_power_reboot(void);
void dm_power_shutdown(void);

#endif /* GNUCHANDM_CORE_H */
