/*
 * dm_session.c — start the window manager as the user who logged in, and wait
 * for it to finish so the login screen comes back.
 *
 * Three things have to be right or the session starts and is not seen:
 *
 *   1. The X cookie. The greeter connected with root's cookie and the user does
 *      not have it. The cookie is merged into the user's own Xauthority file
 *      before the session starts, so the window manager — connecting as the
 *      user — is allowed in. That file is written by root, so it is then given
 *      to the user: a cookie the user cannot read is the same as no cookie,
 *      and the session dies in the first XOpenDisplay.
 *
 *   2. The identity. The child drops to the user's uid and gid.
 *
 *   3. The environment. The session is told who it is, where the server is and
 *      where the cookie is.
 *
 * The greeter's own window is unmapped for the duration: it is drawn
 * override-redirect and full-screen, so if it stayed mapped it would sit on top
 * of the very session it just started.
 */
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dm_core.h"

/* The window manager a session runs, found on PATH. */
static const char *SESSION_COMMAND = "GnuChanWM";

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

        execvp(SESSION_COMMAND, (char *const[]){ (char *)SESSION_COMMAND, NULL });
        fprintf(stderr, "gnuchandm: cannot run %s: %s\n",
                SESSION_COMMAND, strerror(errno));
        _exit(1);
    }

    /* The parent — the greeter, waiting for the session to end, then back to
       the login screen. */
    dm_form_clear_password(core);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) break;
    }

    /* Say how the session ended: a session that dies in a second is the
       difference between "the user logged out" and "the session never
       started", and the log is the only place to tell them apart. */
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0) {
            fprintf(stderr, "gnuchandm: %s exited with status %d\n",
                    SESSION_COMMAND, code);
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "gnuchandm: %s was killed by signal %d\n",
                SESSION_COMMAND, WTERMSIG(status));
    }

    /* The session is over: take the screen back and return to the login
       screen with the user name kept, so logging back in is one field. */
    core->starting_session = 0;
    core->focus = DM_FOCUS_PASSWORD;
    XMapRaised(core->display, core->window);
    XSetInputFocus(core->display, core->window, RevertToPointerRoot, CurrentTime);
    XSync(core->display, False);
    return 0;
}
