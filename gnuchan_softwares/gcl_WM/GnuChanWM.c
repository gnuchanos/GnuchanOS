/*
 * GnuChanWM.c — the entry point.
 *
 * This is the only file that decides which modules make up the WM. The core
 * knows nothing about window management, focus or keys; it knows how to run a
 * list of modules. So the whole WM, as a feature set, is this list — and
 * adding a feature is adding a line here and a file beside it.
 *
 *     GnuChanWM            start (as a session's window manager)
 *     GnuChanWM --version  print the version and exit
 *
 * Everything the WM prints goes to a log file as well as to stderr, because a
 * window manager started from a display manager has no terminal to print to:
 * when something goes wrong the log is the only record, and the log is what a
 * terminal is opened to show.
 */
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_spawn.h"

#define GNUCHANWM_VERSION "0.1.0"

/* Where the WM writes what it has to say. /tmp is used rather than the home
   directory because it is the one place always writable, whatever user the
   session runs as, and it survives only until the next boot — which is the
   right lifetime for a start-up log. */
static const char *LOG_PATH = "/tmp/gnuchanwm.log";

/* Set by the signal handler and read by the loop. Only a flag of this exact
   type may be touched from a handler. */
static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signum) {
    (void)signum;
    stop_requested = 1;
}

/* The modules, in the order they start. Order matters where one module reads
   state another sets up; key grabs come last so a key that cannot be bound
   cannot stop the window manager itself from running. */
static void register_modules(WmCore *core) {
    /* Order matters where a module reads state another sets up: the theme is
       published before anything is drawn or started, so every window that
       opens afterwards is drawn by a toolkit that already knows the answer;
       the desktop paints the background next; windows are managed on top of
       it; focus names the current one; and keys come last so a key that
       cannot be bound cannot stop the window manager itself from running. */
    wm_register(core, &wm_config_module);
    wm_register(core, &wm_workspace_module);
    wm_register(core, &wm_theme_module);
    wm_register(core, &wm_desktop_module);
    /* The compositor, before the frame and before manage, and the order is not
       arbitrary: it is what is told a scaled window changed size. That notice
       throws away the pixmap the window used to be, and it has to happen
       BEFORE anything draws — manage's ConfigureNotify calls wm_frame_sync,
       which draws. Registered after either of them, the first draw after a
       resize would be from a pixmap the server has already thrown away, and
       the window would show a torn or missing picture for a frame. */
    wm_register(core, &wm_compositor_module);
    /* The display guard, before the frame and manage: it is what stops a game
       from resizing the whole desktop under them. It remembers the CRTC mode
       the session started on and puts it back if a client changes it — the
       fallback a Direct3D game reaches for when it cannot get fullscreen the
       EWMH way. Registered here so the mode is remembered before the first
       window can open and ask for the screen. */
    wm_register(core, &wm_randr_module);
    wm_register(core, &wm_manage_module);
    wm_register(core, &wm_frame_module);
    wm_register(core, &wm_focus_module);
    /* The switcher: it reads the frame table and the focus order the two
       modules above fill, so it comes after them. It is registered before the
       keys module because both grab on the root, and the switcher's key is a
       gesture rather than a binding — see wm_switcher.c — so it takes its grab
       first and the table below cannot take the same combination away from
       it. */
    wm_register(core, &wm_switcher_module);
    wm_register(core, &wm_keys_module);
    /* The menu takes the desktop's own button, so it comes after the desktop
       has claimed the root and before the first terminal is opened: the menu
       is part of the desktop a user lands on, and a session whose first
       terminal is still starting should still answer a right click. */
    wm_register(core, &wm_menu_module);
    /* The pointer: what the wheel does over the desktop, and the touchpad
       settings pushed into the running X server. After the menu, because both
       answer the same button and the menu has to see its press first. */
    wm_register(core, &wm_input_module);
    /* The tray, before the first program starts: claiming the tray selection
       is what tells a program where to put its icon, and a program that looked
       before the claim found no tray and does not look again — it is found by
       the MANAGER announcement the claim sends. Registering it here, ahead of
       autostart, is what makes the session's own first program able to dock
       rather than having to be told to look a second time. */
    wm_register(core, &wm_tray_module);
    /* Last: it opens the first terminal, and by then the keys are already
       grabbed and the desktop is already painted, so the window it opens is
       managed by a session that is completely up rather than one still
       arranging itself. */
    wm_register(core, &wm_autostart_module);
    /* The laptop lid: sleep on close, screen saver / lock on open. A desktop
       finds no lid on the first tick and does nothing, so it costs nothing to
       always register. */
    wm_register(core, &wm_lid_module);
    /* The idle desk: turn the X server's own blanking and DPMS off, and run
       the session's screen saver (and lock) when the keyboard and pointer have
       been still for the configured time. Registered last because it is the
       one module that acts on a quiet desk rather than on an event: its init
       runs `xset s off -dpms` once the display is up, and its tick then keeps
       the desk from going black behind the session's back. It is a companion
       to the lid module and shares its command lines, but the two are separate
       because a desk can be idle with the lid open and shut with it idle. */
    wm_register(core, &wm_idle_module);
}

/* Send everything the WM prints to the log as well as to stderr. Called
   before anything can print, so no message is ever missing from the log. */
static void open_log(void) {
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    dup2(fd, STDERR_FILENO);
    close(fd);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

/* When start-up fails after the display is open — a module could not run —
   the log is opened in a terminal so the failure is seen instead of leaving
   a bare screen. If no terminal can be opened, the messages have already
   gone to stderr and the log file, which is what a display manager keeps. */
static void show_failure(void) {
    fprintf(stderr, "gnuchanwm: start-up failed; opening a terminal with the log.\n");
    fflush(stderr);
    if (wm_spawn_terminal_displaying(LOG_PATH) != 0) {
        fprintf(stderr, "gnuchanwm: see %s for what went wrong.\n", LOG_PATH);
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanWM %s\n", GNUCHANWM_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanWM [--version] [--help]\n"
                   "\n"
                   "Starts the GnuchanOS window manager. Alt+Enter opens the\n"
                   "default terminal, and Alt+F4 closes the focused one.\n"
                   "\n"
                   "Hold Alt and press ` to switch windows: every open window\n"
                   "appears as a picture with its name under it, the arrow\n"
                   "keys walk the grid while the key is held, and letting the\n"
                   "key go brings the chosen window forward. A left click on a\n"
                   "picture chooses it as well, and Escape leaves the choice\n"
                   "unmade.\n"
                   "\n"
                   "Super+S fits the focused window's content into it: a\n"
                   "program that ignores being resized is drawn smaller rather\n"
                   "than cut off, so nothing of it is hidden. Super+S again\n"
                   "puts it back. Clicks in a fitted window move the pointer\n"
                   "to where the program expects it, which is visible; the key\n"
                   "is for a game played with a pad or a keyboard.\n"
                   "\n"
                   "Super+1..N goes to a workspace and Super+Shift+1..N sends\n"
                   "the focused window to one and stays where you are. How\n"
                   "many there are is what the settings script's layout widget\n"
                   "asks for.\n"
                   "\n"
                   "The title bar's buttons minimise, maximise and close, and\n"
                   "the desktop's own button opens the session menu. It is\n"
                   "meant to be started by a display manager session, not from\n"
                   "inside another session.\n");
            return 0;
        }
    }

    open_log();

    /* The first line, before anything can fail: it is what tells a log that
       the binary was started at all from a log that shows how far it got. */
    fprintf(stderr,
            "gnuchanwm: started (pid %ld, DISPLAY=%s)\n",
            (long)getpid(),
            getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    /* SA_RESTART is deliberately left off: the signal has to interrupt the
       blocking XNextEvent so the loop can see the flag. */
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    WmCore core;

    /* Claim the display first: a module's init needs the connection and the
       atom table that this opens, so it cannot be registered onto a core that
       does not have them yet. */
    if (wm_core_init(&core) != 0) {
        /* No display: a terminal cannot be opened either, because there is no
           X server to open it on. The message has gone to the log. */
        fprintf(stderr, "gnuchanwm: cannot start: no usable X display.\n");
        return 1;
    }

    register_modules(&core);

    if (wm_core_start(&core) != 0) {
        show_failure();
        wm_core_shutdown(&core);
        return 1;
    }

    /* The loop stops on a signal by clearing the flag it checks, so the core
       is given one last chance to close the display cleanly. */
    while (core.running && !stop_requested) {
        wm_core_step(&core);
    }
    core.running = 0;

    wm_core_shutdown(&core);
    return 0;
}
