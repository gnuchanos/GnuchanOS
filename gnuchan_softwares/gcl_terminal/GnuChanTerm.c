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
 * TERM is set to the name this terminal answers to, and it is set to something
 * plain rather than to `xterm-256color`: this is not an xterm and a program
 * that reads the name and then uses an xterm-only feature would be reading a
 * promise that is not kept. `gcl-256color` is a name no program has special
 * cases for, which means every program falls back to reading the terminfo
 * entry — and the entry is installed to match this terminal exactly.
 *
 * COLORTERM is what tells a program it may use truecolor, which is the one
 * thing btop looks at and the one thing the name cannot carry.
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

/* --- the shell ------------------------------------------------------------
 *
 * The command run is the user's shell if SHELL says so and /bin/sh otherwise.
 * It is run as a LOGIN shell only when no `-e` command was given on the
 * command line: a terminal opened to run one program must not read the user's
 * profile first, and a terminal opened for typing must.
 *
 * The environment is built rather than inherited, because TERM is exactly what
 * this terminal has to say about itself and an inherited one from the shell
 * that started us would be another terminal's name.
 */
/* --- LS_COLORS, built from the palette --------------------------------------
 *
 * `ls` colours from LS_COLORS and from nothing else, so this is the one place
 * the terminal has a say in what a file looks like. Debian's default paints a
 * directory BLUE and an executable GREEN — dircolors' choices, nothing to do
 * with this theme — and on a desktop where every other window is violet that
 * made `ls` the one program showing two foreign colours.
 *
 * Every colour below is READ FROM THE PALETTE and not one is written here. The
 * settings file is the single place a colour is chosen, so changing Colors[5]
 * changes the directory with it; a literal here would be a second copy of the
 * theme, and the two would drift the first time either was edited.
 *
 * Which entry goes where is decided by LIGHTNESS, because hue is the one thing
 * this palette has none of to spare — see gcl_palette.h. A directory, a link
 * and an executable are three steps apart, which is what keeps them tellable
 * from one another.
 */

/* The palette's own order, so a line below can name an entry instead of
   counting to it. */
enum {
    LS_BLACK = 0, LS_RED, LS_GREEN, LS_YELLOW,
    LS_BLUE, LS_MAGENTA, LS_CYAN, LS_WHITE,
    LS_BRIGHT_BLACK, LS_BRIGHT_RED, LS_BRIGHT_GREEN, LS_BRIGHT_YELLOW,
    LS_BRIGHT_BLUE, LS_BRIGHT_MAGENTA, LS_BRIGHT_CYAN, LS_BRIGHT_WHITE,
    LS_TEXT, LS_BACKGROUND,
};

/* One packed colour as the three decimal numbers an SGR wants: `38;2;R;G;B`.
   A macro and not a function because it expands to THREE arguments, and a
   function returning a struct could not be handed to snprintf as three. */
#define RGB_ARG(rgb)                                            \
    (unsigned)(((rgb) >> 16) & 0xFFu),                          \
    (unsigned)(((rgb) >> 8) & 0xFFu),                           \
    (unsigned)((rgb) & 0xFFu)

/* A buffer being filled with LS_COLORS entries, and how much of it is written.
   It always ends with a NUL while it is being built. */
typedef struct LsColorsBuilder {
    char *text;
    unsigned int size;
    unsigned int used;
} LsColorsBuilder;

/* One `key=<colour><extra>:` appended.
 *
 * An entry that will not fit is DROPPED rather than truncated into the tail:
 * half of `di=38;2;199;125` is not a colour and would be read as one, which is
 * `ls` drawing a directory in whatever the cut number happened to come to.
 * Dropping it leaves that one kind uncoloured — visible and harmless — which is
 * the same choice term_config.c makes with a colour it cannot read. */
static void ls_colors_add(LsColorsBuilder *builder, const char *key,
                          uint32_t rgb, const char *extra) {
    unsigned int room = builder->size - builder->used;
    if (room < 48) {
        return;
    }
    int written = snprintf(builder->text + builder->used, room,
                           "%s=38;2;%u;%u;%u%s:", key,
                           (unsigned)((rgb >> 16) & 0xFFu),
                           (unsigned)((rgb >> 8) & 0xFFu),
                           (unsigned)(rgb & 0xFFu), extra);
    if (written > 0 && (unsigned int)written < room) {
        builder->used += (unsigned int)written;
    }
}

/* Every key in a NULL-terminated list, in one colour. */
static void ls_colors_add_all(LsColorsBuilder *builder,
                              const char *const *keys, uint32_t rgb) {
    for (int i = 0; keys[i] != NULL; i++) {
        ls_colors_add(builder, keys[i], rgb, "");
    }
}

/* Build the whole value from the palette. `out` is the caller's buffer. */
static void build_ls_colors(const TermStyle *style, char *out,
                            unsigned int size) {
    if (out == NULL || size == 0) {
        return;
    }
    out[0] = '\0';

    LsColorsBuilder builder;
    builder.text = out;
    builder.size = size;
    builder.used = 0;

    /* The colours, read once each. Which entry is which is the settings file's
       business and not this file's — see the note above. */
    uint32_t dir    = term_style_palette_entry(style, LS_MAGENTA);
    uint32_t link   = term_style_palette_entry(style, LS_CYAN);
    uint32_t exec   = term_style_palette_entry(style, LS_RED);
    uint32_t device = term_style_palette_entry(style, LS_BRIGHT_YELLOW);
    uint32_t plain  = term_style_palette_entry(style, LS_TEXT);
    uint32_t quiet  = term_style_palette_entry(style, LS_YELLOW);
    uint32_t media  = term_style_palette_entry(style, LS_BRIGHT_CYAN);
    uint32_t sound  = term_style_palette_entry(style, LS_CYAN);
    uint32_t code   = term_style_palette_entry(style, LS_TEXT);

    /* The kinds of file. `;4` is underline and marks the three that change what
       a file DOES when it is run rather than what it is. */
    ls_colors_add(&builder, "di", dir, "");
    ls_colors_add(&builder, "ln", link, "");
    ls_colors_add(&builder, "ex", exec, "");
    ls_colors_add(&builder, "pi", quiet, "");
    ls_colors_add(&builder, "so", quiet, "");
    ls_colors_add(&builder, "bd", device, "");
    ls_colors_add(&builder, "cd", device, "");
    ls_colors_add(&builder, "or", plain, "");
    ls_colors_add(&builder, "mi", plain, "");
    ls_colors_add(&builder, "su", exec, ";4");
    ls_colors_add(&builder, "sg", exec, ";4");
    ls_colors_add(&builder, "tw", dir, ";4");
    ls_colors_add(&builder, "ow", dir, ";4");
    ls_colors_add(&builder, "st", dir, ";4");

    /* And the extensions, by what they hold. These matter as much as the kinds:
       a plain `ls` is themed by the block above, and `ls` on a directory full of
       archives is themed by this one. */
    static const char *const ARCHIVES[] = {
        "*.tar", "*.tgz", "*.zip", "*.gz", "*.bz2", "*.xz", "*.zst", "*.7z",
        NULL,
    };
    static const char *const IMAGES[] = {
        "*.jpg", "*.jpeg", "*.png", "*.gif", "*.svg", "*.webp", "*.bmp",
        "*.ico", NULL,
    };
    static const char *const SOUNDS[] = {
        "*.mp3", "*.flac", "*.ogg", "*.wav", "*.m4a", "*.opus", NULL,
    };
    static const char *const VIDEOS[] = {
        "*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov", NULL,
    };
    static const char *const DOCUMENTS[] = {
        "*.pdf", "*.odt", "*.ods", "*.odp", "*.doc", "*.docx", "*.xls",
        "*.xlsx", "*.ppt", "*.pptx", NULL,
    };
    static const char *const SOURCES[] = {
        "*.c", "*.h", "*.cc", "*.cpp", "*.hpp", "*.rs", "*.go", "*.py",
        "*.lua", "*.js", "*.ts", "*.sh", "*.gcsf", NULL,
    };
    static const char *const TEXTS[] = {
        "*.txt", "*.md", "*.rst", "*.log", "*.json", "*.toml", "*.yaml",
        "*.yml", "*.xml", "*.ini", "*.cfg", "*.conf", NULL,
    };

    ls_colors_add_all(&builder, ARCHIVES,  media);
    ls_colors_add_all(&builder, IMAGES,    media);
    ls_colors_add_all(&builder, SOUNDS,    sound);
    ls_colors_add_all(&builder, VIDEOS,    sound);
    ls_colors_add_all(&builder, DOCUMENTS, quiet);
    ls_colors_add_all(&builder, SOURCES,   code);
    ls_colors_add_all(&builder, TEXTS,     quiet);
}

static void build_env(const TermStyle *style, char **env, int *env_count) {
    extern char **environ;
    int count = 0;

    /* Everything the session already had, minus the four this terminal owns.
       Taking the rest keeps PATH, HOME, LANG and everything else a program
       expects, without this file having to know what they are. */
    /* Five of this terminal's own are added below, and one slot is the NULL
       that ends the array — so the copies stop six short of the end and not
       five. It was five, and five was one too many: a session with a full
       environment put the five in the last five slots and the terminator one
       past the array. */
    for (char **it = environ; *it != NULL && count < TERM_ENV_MAX - 6; it++) {
        if (strncmp(*it, "TERM=", 5) == 0 ||
            strncmp(*it, "COLORTERM=", 10) == 0 ||
            strncmp(*it, "TERM_PROGRAM=", 13) == 0 ||
            strncmp(*it, "PROMPT_COMMAND=", 15) == 0 ||
            strncmp(*it, "LS_COLORS=", 10) == 0) {
            continue;
        }
        env[count++] = *it;
    }

    /* The four this terminal is. They are static strings and not copied:
       execvp() reads them once and the process image is replaced after.
     *
     * PS1 is the fourth and it is the terminal's own for one reason: a shell
     * that has not been configured prints `user@host:/the/whole/path$`, which
     * is a line of things the user already knows — their own name, their own
     * machine — with the one part that matters (where they are) buried in the
     * middle, and a `$` on the end, which is the character Windows shows and
     * the one this user asked not to see.
     *
     * `\w` is the working directory with the home directory folded to `~`, and
     * it is understood by bash, dash and zsh alike — the shells a log-in
     * reaches here. The mark is `>`, and the line is short enough to read at a
     * glance. A user who wants their own prompt back writes PS1 in their
     * shell's own startup file, which runs AFTER this and wins. */
    static char term_var[] = "TERM=" TERM_NAME;
    static char color_var[] = "COLORTERM=truecolor";
    static char program_var[] = "TERM_PROGRAM=GnuChanTerm";

    /* LS_COLORS, in the theme, because `ls` colours from this and nothing else.
     *
     * The default Debian one is the reason a directory is BLUE and an
     * executable is GREEN: those are dircolors' choices and they have nothing
     * to do with this terminal. On a desktop where every other window is
     * violet, `ls` was the one place two foreign colours appeared, and a
     * terminal that leaves them there is a terminal that does not match its own
     * theme.
     *
     * Every value is a truecolor SGR — `38;2;R;G;B` — which this terminal reads
     * directly, and the numbers are the theme's own: a directory the bright
     * violet, a symlink the mid violet, an executable the light violet. The
     * three step apart in LIGHTNESS, which is what keeps them tellable from one
     * another now that they share a hue — the same argument gcl_palette.h makes
     * about the sixteen.
     *
     * The trailing entries matter as much as the leading ones: without them a
     * plain `ls` is themed and `ls --color` on a directory full of archives
     * shows them in whatever the system default was. */
    /* The value is BUILT FROM THE PALETTE at every start, and not written out
       here: the settings file is where a colour is decided, and a table of
       hexadecimals in this file would be a second copy of the theme that stops
       agreeing with the first the moment either is edited. See
       build_ls_colors() above for how the entries are chosen. */
    static char ls_colors_value[TERM_CONFIG_TEXT_LENGTH * 8];
    build_ls_colors(style, ls_colors_value, sizeof(ls_colors_value));

    /* The prompt, in the same colours and for the same reason: the settings
       file decides, and nothing here is written twice.
     *
     * The shape is two lines — the machine above, the place below — and the
     * colours step from the bright violet at the marks, through the text
     * colour on the name, to the mid violet on the path. Nothing on the line
     * is a colour a program can produce, so the prompt cannot be mistaken for
     * output. */
    static char ps1_value[TERM_CONFIG_TEXT_LENGTH * 4];
    snprintf(ps1_value, sizeof(ps1_value),
             "\\[\\e[38;2;%u;%u;%um\\]\\342\\225\\255\\342\\224\\200 "
             "\\[\\e[38;2;%u;%u;%um\\]\\u@\\h "
             "\\[\\e[38;2;%u;%u;%um\\]\\342\\224\\200\\n"
             "\\[\\e[38;2;%u;%u;%um\\]\\342\\225\\260\\342\\224\\200 "
             "\\[\\e[38;2;%u;%u;%um\\][ \\w ] "
             "\\[\\e[38;2;%u;%u;%um\\]\\342\\235\\257 "
             "\\[\\e[0m\\]",
             RGB_ARG(term_style_palette_entry(style, LS_MAGENTA)),
             RGB_ARG(term_style_palette_entry(style, LS_TEXT)),
             RGB_ARG(term_style_palette_entry(style, LS_CYAN)),
             RGB_ARG(term_style_palette_entry(style, LS_MAGENTA)),
             RGB_ARG(term_style_palette_entry(style, LS_YELLOW)),
             RGB_ARG(term_style_palette_entry(style, LS_RED)));

    /* The prompt goes through PROMPT_COMMAND and NOT through PS1, and that is
       the whole of why it is visible at all.
     *
     * A PS1 handed over in the environment is overwritten before the first
     * prompt is ever drawn: Debian's /etc/bash.bashrc assigns a plain
     * `\u@\h:\w\$ ` and ~/.bashrc assigns it again, both unconditionally, so
     * the shell ends up with the `$` this terminal exists to not show — however
     * carefully the variable was set here. That is a fact about those files and
     * not about this one, and fighting them would mean editing the user's own
     * shell configuration, which is theirs.
     *
     * PROMPT_COMMAND is run by bash immediately BEFORE every prompt is drawn
     * and neither of those files touches it. Setting PS1 there therefore
     * happens after they have had their say, every single time, and the prompt
     * below is what the user sees.
     *
     * It is a command and not a value, which is why the inner quotes are part
     * of the string: bash evaluates this as `PS1="..."` before each prompt. The
     * escapes inside are single-quoted for the same reason — they are bash's to
     * expand at draw time, not this program's. */
    /* The prompt and the file colours, both written in the theme's purples.
     *
     * They are ONE setting because they fail the same way. ~/.bashrc on this
     * system ends with `eval "$(dircolors -b)"`, which sets LS_COLORS to
     * Debian's defaults — a BLUE directory and a GREEN executable — and the
     * same file assigns PS1 over the top of whatever was handed over. Both are
     * therefore gone by the time the first prompt is drawn, however carefully
     * they were put in the environment.
     *
     * PROMPT_COMMAND is run by bash immediately BEFORE every prompt, and not
     * one of those files touches it. Everything set here lands AFTER they have
     * had their say — on the first prompt and on every one after it — so the
     * prompt is the theme's and `ls` is the theme's, and there is no colour on
     * the screen that is not.
     *
     * The value inside is built with snprintf and not written out twice: the
     * sixteen colours are a kilobyte of text, and two copies of a kilobyte is
     * one copy that will be changed and one that will not.
     *
     * PS1 goes inside double quotes and LS_COLORS inside single ones, and the
     * difference is not style: an LS_COLORS value is full of semicolons
     * ("di=38;2;199;125;255"), so an unquoted one would end the command at the
     * first of them. It contains no single quote of its own, which is what
     * makes single quotes the safe pair. */
    static char ls_colors_var[sizeof(ls_colors_value) + 16];
    static char prompt_var[sizeof(ps1_value) + sizeof(ls_colors_value) + 64];
    snprintf(ls_colors_var, sizeof(ls_colors_var),
             "LS_COLORS=%s", ls_colors_value);
    snprintf(prompt_var, sizeof(prompt_var),
             "PROMPT_COMMAND=PS1=\"%s\"; export LS_COLORS='%s'",
             ps1_value, ls_colors_value);

    env[count++] = term_var;
    env[count++] = color_var;
    env[count++] = program_var;
    env[count++] = prompt_var;
    env[count++] = ls_colors_var;
    env[count] = NULL;
    *env_count = count;
}

/* Start the shell on a new PTY sized to the grid.
 *
 * The size is passed to forkpty() and not applied afterwards, which is the one
 * ordering that matters for a full-screen program — see term_pty.h. */
static int start_shell(TermCore *core, int argc, char **argv) {
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
    build_env(core->style, env, &env_count);

    if (term_pty_spawn(pty, shell_argv, env, core->cols, core->rows) != 0) {
        fprintf(stderr, "gcl_terminal: cannot start a shell\n");
        free(pty);
        core->pty = NULL;
        return -1;
    }

    /* The parser's answers to the program's queries go back down the same PTY.
       It is wired here because this is where the PTY comes into existence. */
    core->vt_host.write = term_pty_vt_write;
    core->vt_host.user = pty;
    core->vt.host = core->vt_host;
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
     configured, and it leaves the built-in defaults. A script that will not
     parse leaves them too, and says why on stderr — see term_config.c. */
    TermConfig config;
    term_config_load_default(&config);

    /* The modules. The order is the order they are given events and the order
       they are asked to draw, and it is deliberate:
       
         render   draws, and it is first so a frame is ready before anything
                  else acts on the event
         input    writes to the child, and it is last so a key that changes
                  what is drawn does not draw before it is sent
       
       The cleanup runs in reverse, so input is finished with before the
       renderer frees the buffer its events might have drawn into. */
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

    if (start_shell(&core, argc, argv) != 0) {
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
