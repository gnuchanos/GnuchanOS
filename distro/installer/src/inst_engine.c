/*
 * inst_engine.c - running the install.
 *
 * The installer's real work is the shell script gnuchanos-install.sh. This file
 * starts it, hands it the answers the person gave (as environment variables, so
 * it asks nothing and runs unattended), and reads its output a line at a time
 * so the window can show it as it arrives.
 *
 * --- why a process and not the work in C -----------------------------------
 *
 * Everything that touches the DISK is the script, and on purpose: the commands
 * it runs - sfdisk, mkfs, unsquashfs, grub-install - each print their own
 * errors, and running them from a script means those errors reach the screen in
 * their own words. A C reimplementation would have to reproduce every message
 * and would hide the ones it forgot. The script is the engine; this file is the
 * pipe that carries what it says.
 *
 * --- why no thread ---------------------------------------------------------
 *
 * The window is Xlib, and Xlib is not safe to call from two threads. So there is
 * one thread: it starts the child, and then WAITS on the child's output with
 * select() in the same loop that reads X events (see GnuChanInstall.c). Reading
 * the pipe is non-blocking, so nothing here ever stalls the window.
 *
 * License: GPL3.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/wait.h>

#include "inst.h"

/* Where the hook installs the script. */
#define ENGINE_PATH "/usr/local/sbin/gnuchanos-install"

/* Set the answers in the child only: the privileged GUI process should not
   expose the password through its own environment. */
static int set_env_if_set(const char *name, const char *value) {
    if (value != NULL) {
        return setenv(name, value, 1);
    }
    return 0;
}

int inst_engine_start(InstApp *app) {
    if (app->running) {
        return 0;
    }
    if (app->disk_choice < 0 || app->disk_choice >= app->disk_count) {
        inst_set_note(app, 1, "Choose the disk to install onto.");
        return 0;
    }
    if (!app->erase_confirmed) {
        inst_set_note(app, 1, "Confirm the disk erase before installing.");
        return 0;
    }
    app->erase_confirmed = 0;

    int fds[2];
    if (pipe(fds) != 0) {
        inst_set_note(app, 1, "Could not create a pipe: %s", strerror(errno));
        return 0;
    }
    int flags = fcntl(fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        close(fds[0]);
        close(fds[1]);
        inst_set_note(app, 1, "Could not configure installer output: %s",
                      strerror(saved_errno));
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        int saved_errno = errno;
        close(fds[0]);
        close(fds[1]);
        inst_set_note(app, 1, "Could not start the installer: %s",
                      strerror(saved_errno));
        return 0;
    }
    if (pid == 0) {
        /* The child: its output - both stdout and stderr - goes down the pipe,
           because a step that fails usually says why on stderr and that line
           must reach the window too. */
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0 ||
            dup2(fds[1], STDERR_FILENO) < 0) {
            fprintf(stderr, "could not connect installer output: %s\n",
                    strerror(errno));
            _exit(126);
        }
        close(fds[1]);
        if (set_env_if_set("GNUCHANOS_DISK",
                           app->disks[app->disk_choice].device) != 0 ||
            set_env_if_set("GNUCHANOS_USER", app->fields[1].text) != 0 ||
            set_env_if_set("GNUCHANOS_FULLNAME", app->fields[2].text) != 0 ||
            set_env_if_set("GNUCHANOS_PASSWORD", app->fields[3].text) != 0 ||
            set_env_if_set("GNUCHANOS_HOSTNAME", app->fields[5].text) != 0 ||
            setenv("GNUCHANOS_YES", "1", 1) != 0) {
            fprintf(stderr, "could not prepare installer answers: %s\n",
                    strerror(errno));
            _exit(126);
        }
        execl("/bin/bash", "bash", ENGINE_PATH, (char *)NULL);
        /* execl only returns on failure; say so in the pipe we still hold. */
        fprintf(stderr, "could not run %s\n", ENGINE_PATH);
        _exit(127);
    }

    /* The parent. The read end is non-blocking so the window's loop can poll it
       and never wait on a step that takes minutes. */
    close(fds[1]);

    app->engine_fd = fds[0];
    app->engine_pid = pid;
    app->engine_buf_len = 0;
    app->running = 1;
    app->finished = 0;
    app->exit_code = 0;
    inst_set_note(app, 0, "Installing to %s ...",
                  app->disks[app->disk_choice].device);
    return 1;
}

int inst_engine_fd(InstApp *app) {
    return app->running ? app->engine_fd : -1;
}

/* Hand one finished line to the window. Called only for whole lines, so the
   output pane never shows half a line that is still being written. */
static void emit_line(InstApp *app, char *line) {
    inst_add_line(app, line);
}

/* Split app->engine_buf on newlines and emit each complete line; the tail after
   the last newline stays in the buffer for the next read. */
static void drain_lines(InstApp *app) {
    int start = 0;
    for (int i = 0; i < app->engine_buf_len; i++) {
        if (app->engine_buf[i] == '\n') {
            app->engine_buf[i] = '\0';
            emit_line(app, app->engine_buf + start);
            start = i + 1;
        }
    }
    int remaining = app->engine_buf_len - start;
    if (remaining > 0) {
        memmove(app->engine_buf, app->engine_buf + start, (size_t)remaining);
    }
    app->engine_buf_len = remaining;
}

int inst_engine_poll(InstApp *app) {
    if (!app->running) {
        return 0;
    }

    /* Read whatever is there. A non-blocking read returns -1/EAGAIN when the
       child has written nothing since the last look, which is the common case
       while a step runs. */
    int read_failed = 0;
    for (;;) {
        int space = (int)sizeof(app->engine_buf) - app->engine_buf_len - 1;
        if (space <= 0) {
            /* A single line longer than the buffer: emit what is held so the
               pane keeps moving rather than stalling and losing output. */
            app->engine_buf[app->engine_buf_len] = '\0';
            emit_line(app, app->engine_buf);
            app->engine_buf_len = 0;
            space = (int)sizeof(app->engine_buf) - 1;
        }
        ssize_t n = read(app->engine_fd, app->engine_buf + app->engine_buf_len,
                         (size_t)space);
        if (n > 0) {
            app->engine_buf_len += (int)n;
            drain_lines(app);
            continue;
        }
        if (n == 0) {
            /* end of file: the child is done writing */
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* nothing more right now - not an error */
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        inst_add_line(app, strerror(errno));
        read_failed = 1;
        break;
    }

    /* The pipe is closed. Flush any last line left without a newline, then
       collect the exit status. */
    if (app->engine_buf_len > 0) {
        app->engine_buf[app->engine_buf_len] = '\0';
        emit_line(app, app->engine_buf);
        app->engine_buf_len = 0;
    }

    int status = 0;
    pid_t waited;
    if (app->engine_pid > 0) {
        do {
            waited = waitpid(app->engine_pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
    } else {
        waited = -1;
    }
    if (waited < 0) {
        inst_add_line(app, "Could not collect the installer process status.");
        app->exit_code = 1;
    } else if (WIFEXITED(status)) {
        app->exit_code = WEXITSTATUS(status);
    } else {
        app->exit_code = 1;
    }
    if (read_failed) {
        app->exit_code = 1;
    }

    close(app->engine_fd);
    app->engine_fd = -1;
    app->engine_pid = -1;
    app->running = 0;
    app->finished = 1;

    if (app->exit_code == 0) {
        inst_set_note(app, 0, "Done. Remove the medium and reboot.");
    } else {
        inst_set_note(app, 1, "The install stopped (exit %d). See the output "
                              "above.", app->exit_code);
    }
    return 0;
}
