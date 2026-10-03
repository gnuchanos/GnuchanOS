/*
 * dm_session.c — start the session the user chose, as the user who logged in,
 * and wait for it to finish so the login screen comes back.
 *
 * Three things have to be right or the session starts and is not seen:
 *
 *   1. The X cookie. The greeter connected with root's cookie and the user does
 *      not have it. The cookie is merged into the user's own Xauthority file
 *      before the session starts, so the session — connecting as the user — is
 *      allowed in. That file is written by root, so it is then given to the
 *      user: a cookie the user cannot read is the same as no cookie, and the
 *      session dies in the first XOpenDisplay.
 *
 *   2. The identity. The child drops to the user's uid and gid.
 *
 *   3. The environment. The session is told who it is, where the server is and
 *      where the cookie is.
 *
 * The greeter's own window is unmapped for the duration: it is drawn
 * override-redirect and full-screen, so if it stayed mapped it would sit on top
 * of the very session it just started.
 *
 * What is run is the session the user picked from the list under the sign-in
 * button, read from /usr/share/xsessions. It is not hard-coded to one window
 * manager: a machine with several sessions installed offers all of them, and
 * the one that starts is the one that was chosen.
 */
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dm_core.h"

/* The session the user chose to start, or NULL when this machine has none. */
static const DmSession *selected_session(DmCore *core) {
    if (!core || core->session_count <= 0) {
        return NULL;
    }
    if (core->session_selected < 0 || core->session_selected >= core->session_count) {
        core->session_selected = 0;
    }
    return &core->sessions[core->session_selected];
}

/* Split a session's command line into arguments, in place.
 *
 * Exec= is a command line, not a path: "gnome-session --session=gnome" names a
 * program and its argument, and running the whole string as one name finds
 * nothing. The split is on spaces only, because a quoting rule would be a
 * general shell-word parser for a case no session file on Debian actually
 * contains. The separators are overwritten with terminators. */
static void split_command(char *line, char *argv[], int max, int *count) {
    int n = 0;
    char *p = line;
    while (*p && n < max - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[n] = NULL;
    *count = n;
}

/* Where the greeter's own cookie is. libX11 does exactly this when XAUTHORITY
   is unset. */
static const char *greeter_auth_file(void) {
    const char *set = getenv("XAUTHORITY");
    if (set && set[0]) return set;

    static char path[4096];
    const char *home = getenv("HOME");
    if (!home) home = "/root";
    snprintf(path, sizeof(path), "%s/.Xauthority", home);
    return path;
}

/* Merge the greeter's cookie into the user's file, then hand the file to the
   user. `xauth merge` copies every cookie the greeter holds, which is exactly
   the set the user needs, and leaves the user's other entries alone.
 *
 * The chown is the part that matters: xauth ran as root, so the file it wrote
 * is root's, and a cookie the user may not read is a cookie the user does not
 * have. Without this the session starts, cannot open the display, and exits
 * immediately — which looks like a black screen and then the login screen
 * again. */
static void install_x_cookie(const struct passwd *user, const char *home,
                             const char *display) {
    if (!display || !display[0]) return;

    char auth_path[4096];
    snprintf(auth_path, sizeof(auth_path), "%s/.Xauthority", home);

    const char *xauth = access("/usr/bin/xauth", X_OK) == 0 ? "/usr/bin/xauth" : "xauth";

    pid_t pid = fork();
    if (pid == 0) {
        execlp(xauth, xauth, "-f", auth_path, "merge", greeter_auth_file(),
               (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { /* retry */ }
    }

    /* Give the file to the user, and nobody else. */
    if (chown(auth_path, user->pw_uid, user->pw_gid) != 0) {
        fprintf(stderr, "gnuchandm: cannot give %s to %s: %s\n",
                auth_path, user->pw_name, strerror(errno));
    }
    chmod(auth_path, S_IRUSR | S_IWUSR);
}

static void set_session_environment(const struct passwd *user, const char *home,
                                    const char *auth_path, const char *display) {
    setenv("HOME", home, 1);
    setenv("USER", user->pw_name, 1);
    setenv("LOGNAME", user->pw_name, 1);
    setenv("SHELL", user->pw_shell && user->pw_shell[0] ? user->pw_shell : "/bin/sh", 1);
    setenv("DISPLAY", display, 1);
    setenv("XAUTHORITY", auth_path, 1);
    setenv("XDG_SESSION_TYPE", "x11", 1);

    /* The session's runtime directory and its D-Bus address.
     *
     * These are what make the session a SESSION and not merely a program on
     * an X display, and without them the desktop's own daemons are crippled.
     * The notification server is the clearest case: it holds the name
     * org.freedesktop.Notifications on the session bus, and a browser, a mail
     * client and Steam all reach that name over the bus — not over the X
     * display. With no bus address in the environment, a program that looks
     * for one either finds nothing and gives up, or (worse) starts a second,
     * invisible bus of its own with dbus-launch that nothing else is on, so
     * every notification lands where no server is listening.
     *
     * systemd creates /run/user/$UID and the user's bus when a session opens,
     * and this greeter starts the session itself rather than through a login
     * that would do it. So the two are pointed at here when they exist. They
     * are looked for rather than assumed: a machine with no systemd user
     * session has no /run/user/$UID, and inventing a bus address that is not
     * there would be worse than leaving it unset — a program would try to
     * connect to a socket nobody is listening on and stop, where an unset
     * variable lets it fall back to its own dbus-launch. */
    /* The paths are short — /run/user/NNNN and its bus socket — so the buffers
       are sized for that and not for a general path. A larger buffer here
       would only move the room into the concatenations below, where the
       compiler can no longer prove the result fits. */
    char runtime[256];
    snprintf(runtime, sizeof(runtime), "/run/user/%u", (unsigned)user->pw_uid);
    struct stat runtime_info;
    if (stat(runtime, &runtime_info) == 0 && S_ISDIR(runtime_info.st_mode)) {
        setenv("XDG_RUNTIME_DIR", runtime, 1);

        char bus_path[280];
        snprintf(bus_path, sizeof(bus_path), "%s/bus", runtime);
        struct stat bus_info;
        if (stat(bus_path, &bus_info) == 0) {
            char bus[300];
            snprintf(bus, sizeof(bus), "unix:path=%s", bus_path);
            setenv("DBUS_SESSION_BUS_ADDRESS", bus, 1);
        }
    }

    const char *current = getenv("PATH");
    static char path[4096];
    snprintf(path, sizeof(path), "/usr/local/bin:/usr/bin:/bin:/usr/games%s%s",
             current && current[0] ? ":" : "", current && current[0] ? current : "");
    setenv("PATH", path, 1);
}

int dm_session_start(DmCore *core, const char *username) {
    struct passwd *user = getpwnam(username);
    if (!user) {
        fprintf(stderr, "gnuchandm: no such user: %s\n", username);
        return -1;
    }

    const DmSession *session = selected_session(core);
    if (!session || !session->exec[0]) {
        fprintf(stderr, "gnuchandm: no session is installed to start\n");
        return -1;
    }

    const char *display = getenv("DISPLAY");
    if (!display || !display[0]) {
        fprintf(stderr, "gnuchandm: DISPLAY is not set\n");
        return -1;
    }

    const char *home = user->pw_dir && user->pw_dir[0] ? user->pw_dir : "/";
    char auth_path[4096];
    snprintf(auth_path, sizeof(auth_path), "%s/.Xauthority", home);

    install_x_cookie(user, home, display);

    /* Nothing the greeter draws from here on belongs on the screen: it is
       handing the display over. */
    core->starting_session = 1;
    XUnmapWindow(core->display, core->window);
    XSync(core->display, False);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "gnuchandm: fork: %s\n", strerror(errno));
        core->starting_session = 0;
        XMapRaised(core->display, core->window);
        return -1;
    }

    if (pid == 0) {
        setsid();
        /* The child must not hold the greeter's connection open: the greeter
           owns this display, and a second client of it would keep it alive
           after the greeter exits. */
        XCloseDisplay(core->display);
        dm_form_clear_password(core);

        set_session_environment(user, home, auth_path, display);
        chdir(home);

        if (setgid(user->pw_gid) != 0 ||
            initgroups(user->pw_name, user->pw_gid) != 0 ||
            setuid(user->pw_uid) != 0) {
            fprintf(stderr, "gnuchandm: cannot become %s: %s\n",
                    username, strerror(errno));
            _exit(1);
        }

        char command[DM_SESSION_EXEC];
        snprintf(command, sizeof(command), "%s", session->exec);
        char *argv[64];
        int argc = 0;
        split_command(command, argv, 64, &argc);
        if (argc > 0) {
            execvp(argv[0], argv);
        }
        fprintf(stderr, "gnuchandm: cannot run %s: %s\n",
                session->exec, strerror(errno));
        _exit(1);
    }

    /* The parent — the greeter, waiting for the session to end, then back to
       the login screen.
     *
     * The wait has to be interruptible by the request to stop, and that is the
     * whole reason it is not a plain waitpid loop. While a session is running
     * the greeter is blocked here and nowhere else, so this is the ONLY place
     * a SIGTERM sent during a session can be noticed — and SIGTERM is exactly
     * what systemd sends at a reboot, a shutdown, and a logout. A loop that
     * only retried on EINTR swallowed it: the system asked the greeter to
     * stop, the greeter was inside a blocking wait, the signal came back as
     * EINTR, and the code waited again. systemd then waited out its whole
     * stop timeout on a unit that was never going to answer and only a reboot
     * the user ran by hand, followed by a logout, ever got the machine down.
     *
     * So on every EINTR the flag is read. When it is set the session is ended
     * here: the child is the leader of its own session (setsid in the child),
     * so signalling its process group reaches the session's whole tree, and
     * then the wait continues until it is actually gone. The loop is left the
     * moment the session is reaped, whether it ended on its own or was ended
     * for the machine's sake. */
    dm_form_clear_password(core);
    int status = 0;
    int ended = 0;
    int asked_to_stop = 0;
    while (!ended) {
        pid_t done = waitpid(pid, &status, 0);
        if (done == pid) {
            ended = 1;
        } else if (done < 0 && errno == EINTR) {
            if (dm_core_stop_requested() && !asked_to_stop) {
                asked_to_stop = 1;
                fprintf(stderr,
                        "gnuchandm: asked to stop; ending the session\n");
                kill(-pid, SIGTERM);
            }
        } else if (done < 0) {
            /* Not EINTR and not the child: there is nothing left to wait for. */
            break;
        }
    }

    /* Asked to stop means the greeter is going with the machine, so the screen
       is not taken back: there is no login screen to come back to, and the
       window is torn down by the shutdown below rather than shown. */
    if (dm_core_stop_requested()) {
        return 0;
    }

    /* Say how the session ended: a session that dies in a second is the
       difference between "the user logged out" and "the session never
       started", and the log is the only place to tell them apart. */
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0) {
            fprintf(stderr, "gnuchandm: %s exited with status %d\n",
                    session->name, code);
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "gnuchandm: %s was killed by signal %d\n",
                session->name, WTERMSIG(status));
    }

    /* The session is over: take the screen back and return to the login
       screen with the user name kept, so logging back in is one field. */
    core->starting_session = 0;
    core->focus = DM_FOCUS_PASSWORD;

    /* The desktop the user just left had its own ideas about the screen: a
       screensaver it armed, a monitor it powered down, a resolution it set.
       None of that is undone by the session exiting, because it is X server
       state and the server outlives the client. It is undone here, before the
       greeter draws itself, or the login screen comes back onto a monitor the
       session switched off — which is the black screen between a logout and
       the greeter reappearing. */
    dm_core_wake_screen(core);

    XMapRaised(core->display, core->window);
    XSetInputFocus(core->display, core->window, RevertToPointerRoot, CurrentTime);
    XSync(core->display, False);
    dm_core_redraw(core);
    return 0;
}
