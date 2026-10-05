/*
 * GnuChanTerm.c — the entry point: what this terminal is made of, and the
 * order it is put together in.
 *
 * A terminal is a window around a program. The window is the core's — the
 * display, the grid, the loop — and the program is the PTY's. Neither knows
 * about the other, and this file is where they are introduced.
 *
 * --- the order, and why it is this order ---
 *
 *   1. register the modules          the core is told what it will run before
 *                                    it exists, so a module's init can be
 *                                    called the moment there is a display
 *
 *   2. term_core_init                the display, the font, the window, and
 *                                    both cell grids come into existence
 *
 *   3. term_pty_spawn                the shell starts on a PTY whose size is
 *                                    already the grid's — before it can ask
 *
 *   4. term_core_start               every module's init runs, which is where
 *                                    the renderer builds its buffer
 *
 *   5. term_core_run                 the loop, until the shell exits
 *
 * The PTY comes BEFORE the modules start, and that is not tidiness: btop's
 * first act is to ask for the window size and draw, and the renderer's buffer
 * has to be there to draw into. A terminal that spawned the shell after the
 * renderer started would be racing the first frame against the first output,
 * and losing that race shows as one frame drawn onto a window with nothing
 * behind it.
 *
 * --- the environment the shell gets ---
 *
 * THREE variables and no more, and the small number is the point.
 *
 * TERM is the name this terminal answers to, and it is set to something plain
 * rather than to `xterm-256color`: this is not an xterm, and a program that
 * reads the name and then uses an xterm-only feature would be reading a promise
 * that is not kept. `gcl-256color` is a name no program has special cases for,
 * which means every program falls back to reading the terminfo entry — and the
 * entry is installed to match this terminal exactly.
 *
 * COLORTERM is what tells a program it may use truecolor, which is the one
 * thing btop looks at and the one thing the name cannot carry.
 *
 * TERM_PROGRAM names this terminal to a program that asks.
 *
 * --- what this file does NOT set, and why ---
 *
 * It does not set LS_COLORS. A terminal that sets it is a terminal that has
 * decided how the user's shell should look, and that is the shell's business
 * and the shell's files'. The colours a person sees in *this* terminal — the
 * background, the text, the sixteen a program names, the cursor, the bar —
 * come from the settings script and nothing else: see term_config.c. What `ls`
 * does with those colours is then its own business, in its own configuration,
 * and a terminal that reaches in and overrides it is one that has to be fought
 * with every time the user edits their own files.
 *
 * The PROMPT is the one exception, and it is a deliberately narrow one, but it
 * takes THREE environment variables and the reason is a hard fact about how a
 * shell reads its files:
 *
 *   - a shell reads the ENVIRONMENT first and its own startup files after;
 *   - /etc/profile and /etc/bash.bashrc both set PS1 on every Debian install,
 *     unconditionally and near the end of the file. `PS1='$ '` is in
 *     /etc/profile and `PS1='\\u@\\h:\\w\\$ '` in /etc/bash.bashrc.
 *
 * So a PS1 put in the environment is ALWAYS overwritten before a prompt is
 * ever drawn: the shell's own files are read later and win. That is the whole
 * of why a configured prompt looked exactly like the default one, and why
 * putting the text in PS1 is not enough on its own.
 *
 * PROMPT_COMMAND is the one hook that runs AFTER those files and before every
 * prompt, so the prompt is set there as well:
 *
 *     PS1               the text, for a shell that runs no PROMPT_COMMAND
 *     GCL_TERM_PROMPT   the same text, so the command never has to quote it —
 *                       an apostrophe or a semicolon in a prompt cannot then
 *                       change what the command does
 *     PROMPT_COMMAND    PS1="$GCL_TERM_PROMPT", re-asserted before each prompt
 *
 * Not one file of the user's is read or written, and a script that names no
 * Prompt sets none of the three, so a shell then looks exactly as it did. A
 * user whose own startup script sets PROMPT_COMMAND keeps theirs — it is read
 * later and replaces this one — and with it their own prompt, which is theirs
 * to choose.
 */
#define _POSIX_C_SOURCE 200809L

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "term_config.h"
#include "term_core.h"
#include "term_pty.h"
#include "term_render.h"
#include "term_input.h"
#include "term_style.h"

/* The name this terminal answers to. See the file comment for why it is not
   `xterm-256color`. */
#define TERM_NAME "gcl-256color"

/* The most arguments the default shell command is built from, and the most
   environment entries it is given. Both are small and fixed, and both are
   ceilings rather than plans: a command line longer than the first is cut off
   rather than overrunning the array, and an environment longer than the second
   loses its tail rather than the terminal failing to start.
 *
 * TERM_PTY_MAX_ARGS is here and not in term_pty.h because the PTY does not
 * impose a limit — it is handed an argv that execvp() reads — and the only
 * place an array of that size is built is this file. */
#define TERM_ENV_MAX 64
#define TERM_PTY_MAX_ARGS 64

/* The most slots this terminal fills itself: the three it is — TERM,
   COLORTERM, TERM_PROGRAM — plus the three a configured prompt takes (PS1,
   GCL_TERM_PROMPT and PROMPT_COMMAND), plus the NULL that ends the array.
   Seven. The copies of the session's own environment below are stopped that
   many short of the end, so the array can never be overrun however long the
   session's environment is, whether or not a prompt was configured. */
#define TERM_ENV_OWN 7

/* --- the shell ------------------------------------------------------------
 *
 * The command run is the user's shell if SHELL says so and the shell recorded
 * for this user in /etc/passwd otherwise, run as a LOGIN shell unless `-e` named
 * a program.
 *
 * The environment is built rather than inherited, because TERM is exactly what
 * this terminal has to say about itself and an inherited one from the shell
 * that started us would be another terminal's name.
 */
static void build_env(char **env, int *env_count, const char *prompt) {
    extern char **environ;
    int count = 0;
    int has_prompt = prompt != NULL && prompt[0] != '\0';

    /* Everything the session already had, minus the names this terminal owns.
       Taking the rest keeps PATH, HOME, LANG and everything else a program
       expects, without this file having to know what they are. */
    for (char **it = environ; *it != NULL && count < TERM_ENV_MAX - TERM_ENV_OWN; it++) {
        if (strncmp(*it, "TERM=", 5) == 0 ||
            strncmp(*it, "COLORTERM=", 10) == 0 ||
            strncmp(*it, "TERM_PROGRAM=", 13) == 0) {
            continue;
        }
        /* An inherited PS1 — and the two names that go with it — is dropped
           when this terminal was given a prompt. Two copies of the same name
           in an environment is a name whose value depends on which reader
           looks first, and the one the settings script named is the answer
           this terminal was asked for. When no prompt was named all three are
           passed through untouched, so a script that says nothing about the
           prompt changes nothing about it. */
        if (has_prompt &&
            (strncmp(*it, "PS1=", 4) == 0 ||
             strncmp(*it, "PROMPT_COMMAND=", 15) == 0 ||
             strncmp(*it, "GCL_TERM_PROMPT=", 16) == 0)) {
            continue;
        }
        env[count++] = *it;
    }

    /* The three. They are static strings and not copied: execvp() reads them
       once and the process image is replaced after. */
    static char term_var[] = "TERM=" TERM_NAME;
    static char color_var[] = "COLORTERM=truecolor";
    static char program_var[] = "TERM_PROGRAM=GnuChanTerm";

    env[count++] = term_var;
    env[count++] = color_var;
    env[count++] = program_var;

    /* The prompt, when the settings script named one. All three are static for
       the same reason the three above are: the pointers outlive this function
       in the child's image only long enough for execvp() to read them. The
       text is carried through exactly as written — the \u, \h and \w in it are
       the shell's own escapes, and it is the shell that expands them.
     *
     * The command is a FIXED string with no prompt text in it, and that is the
     * point: the shell reads it as a command, so a prompt holding a quote, a
     * space or a semicolon would otherwise be able to change what the command
     * does. Referring to the variable keeps the text data and the command
     * code, whatever a person writes in their prompt. */
    if (has_prompt) {
        static char prompt_ps1[TERM_CONFIG_TEXT_LENGTH * 3 + 64];
        static char prompt_value[TERM_CONFIG_TEXT_LENGTH * 3 + 64];
        static char prompt_command[] =
            "PROMPT_COMMAND=PS1=\"$GCL_TERM_PROMPT\"";

        /* The prompt goes in exactly as the settings script wrote it. It used
           to be wrapped in the OSC 133 semantic-prompt markers, which existed
           for the terminal's own command suggestion — reading the line off the
           grid needs to know where the prompt ended. That feature is gone, so
           the markers are gone with it, and the prompt is now simply the text
           the user asked for. The shell's own \u, \h and \w escapes are still
           left to the shell to expand, as they always were. */
        snprintf(prompt_ps1, sizeof(prompt_ps1), "PS1=%s", prompt);
        snprintf(prompt_value, sizeof(prompt_value),
                 "GCL_TERM_PROMPT=%s", prompt);

        env[count++] = prompt_ps1;
        env[count++] = prompt_value;
        env[count++] = prompt_command;
    }

    env[count] = NULL;
    *env_count = count;
}

/* Start the shell on a new PTY sized to the grid.
 *
 * The size is passed to forkpty() and not applied afterwards, which is the one
 * ordering that matters for a full-screen program — see term_pty.h. */
static int start_shell(TermCore *core, int argc, char **argv,
                       const char *prompt) {
    TermPty *pty = (TermPty *)calloc(1, sizeof(TermPty));
    if (pty == NULL) {
        return -1;
    }
    core->pty = pty;

    /* The arguments. `-e cmd args` runs one program; with none, the user's
       shell is run as a login shell. */
    char *shell_argv[TERM_PTY_MAX_ARGS];
    int shell_argc = 0;
    int c = 0;
    for (c = 1; c < argc - 1; c++) {
        if (strcmp(argv[c], "-e") == 0) {
            break;
        }
    }
    if (c < argc - 1 && strcmp(argv[c], "-e") == 0) {
        for (int i = c + 1; i < argc && shell_argc < TERM_PTY_MAX_ARGS - 1; i++) {
            shell_argv[shell_argc++] = argv[i];
        }
    }
    if (shell_argc == 0) {
        /* The user's own shell, which is not the same thing as "a shell that
           runs". `SHELL` is asked first, and when it is unset — a terminal
           started from a session that never exported one, or from a script —
           the shell recorded for this user in /etc/passwd.
         *
         * Falling back to `/bin/sh` was the fault behind a bare `$ ` prompt: on
         * Debian that is dash, whose whole default prompt is `$ `. A user who
         * runs bash was handed a shell they never use, in a session that looked
         * like nothing else on their desktop. The passwd entry is what `login`
         * itself uses, and it is the user's choice rather than ours. */
        const char *shell = getenv("SHELL");
        if (shell == NULL || *shell == '\0') {
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL && pw->pw_shell != NULL &&
                pw->pw_shell[0] != '\0') {
                shell = pw->pw_shell;
            } else {
                shell = "/bin/bash";
            }
        }
        /* A static slot: the pointer outlives this function in the child's
           image only long enough for execvp() to read it. */
        static char login_shell[512];
        snprintf(login_shell, sizeof(login_shell), "-%s",
                 strrchr(shell, '/') != NULL ? strrchr(shell, '/') + 1 : shell);
        shell_argv[shell_argc++] = (char *)shell;
        shell_argv[shell_argc++] = login_shell;
    }
    shell_argv[shell_argc] = NULL;

    char *env[TERM_ENV_MAX];
    int env_count = 0;
    build_env(env, &env_count, prompt);

    if (term_pty_spawn(pty, shell_argv, env, core->cols, core->rows) != 0) {
        fprintf(stderr, "gcl_terminal: cannot start a shell\n");
        free(pty);
        core->pty = NULL;
        return -1;
    }

    /* The parser's host is wired by term_core_init() with the CORE as its user,
       and its callbacks reach the PTY through core->pty. Nothing to do here. */
    return 0;
}

/* --- the entry point ------------------------------------------------------ */

int main(int argc, char **argv) {
    /* Zeroed before anything touches it. term_core_register() runs first and
       reads modules.count, so a core that arrived with stack garbage in it
       would either refuse the first module or write past the array — and the
       terminal would open on an empty screen with no explanation. */
    TermCore core;
    memset(&core, 0, sizeof(core));

    /* The settings script, read BEFORE the display is opened, and that order
       is the whole reason the config is plain data. The font it names decides
       how big a cell is, and the cell size decides how big the window is — so
       a terminal that opened the display first would have to open the window
       at the wrong size and resize it, which is visible as a window that
       starts wrong and corrects itself.
     *
     * A missing script is the ordinary case on a machine that has never been
     * configured, and it leaves the built-in defaults. A script that will not
     * parse leaves them too, and says why on stderr — see term_config.c. */
    TermConfig config;
    term_config_load_default(&config);

    /* The modules. The order is the order they are given events and the order
       they are asked to draw, and it is deliberate:
       
         render   draws, and it is first so a frame is ready before anything
                  else acts on the event
         input    writes to the child, and it is last so a key that changes
                  what is drawn does not draw before it is sent
       
       The cleanup runs in reverse. */
    if (term_core_register(&core, &term_render_module) != 0 ||
        term_core_register(&core, &term_input_module) != 0) {
        fprintf(stderr, "gcl_terminal: too many modules\n");
        return 1;
    }

    if (term_core_init(&core, "GnuChanTerm", config.font) != 0) {
        fprintf(stderr, "gcl_terminal: cannot open a display\n");
        return 1;
    }

    /* The colours, now that there is a style to put them on. The font was
       already dealt with — the core opened it, because it had to have it
       before it could size the window — and everything else in the script is
       applied here. */
    term_config_apply_style(&config, core.style);

    /* What a program's own colour changes go back to, taken HERE and nowhere
       else. The snapshot has to come after the theme is applied — so it holds
       the colours the user configured and not the ones the code shipped with —
       and before the shell starts, so it cannot capture a program's own OSC 4
       as if it were part of the theme. See term_style.h for what it is for. */
    term_style_snapshot(core.style);

    if (start_shell(&core, argc, argv, config.prompt) != 0) {
        term_core_shutdown(&core);
        return 1;
    }

    if (term_core_start(&core) != 0) {
        term_core_shutdown(&core);
        return 1;
    }

    term_core_run(&core);

    term_core_shutdown(&core);
    return 0;
}
