/*
 * shell_gnuLinux.c — GCL interactive shell (GNU/Linux)
 *
 * NOTE: This shell will later become the foundation of GnuchanOS's own
 * shell system. Unlike bash it aims to be fish-like and more advanced.
 * CURRENTLY KEPT SIMPLE.
 *
 * Usage:
 *   gcl            -> starts the interactive shell
 *   gcl <file.gcsf> -> compiles a file (main.c)
 *
 * Commands (for now):
 *   ls, pwd, cd, echo, clear, help, exit, version
 */

#ifndef GCL_SHELL_GNU_LINUX_C
#define GCL_SHELL_GNU_LINUX_C

/* Needed for glibc to expose PATH_MAX under strict -std=c11 */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>

#define SHELL_PROMPT "gcl> "
#define SHELL_MAX_LINE 2048

static void shell_print_help(void) {
    printf(GCL_COLOR_MAGENTA_DIM "GCL Shell - basic commands (will become fish-like later):\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  ls              list files in the current directory\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  pwd             show the working directory\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  cd <path>       change directory\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  echo <text>     print text\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  clear           clear the screen\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  version         show gcl version\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  help            show this help\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  exit            leave the shell\n" GCL_COLOR_RESET);
}

static void shell_print_version(void) {
    printf(GCL_COLOR_MAGENTA_BRIGHT "gcl %s - Gnuchan C-Like Language shell (GNU/Linux)\n" GCL_COLOR_RESET, GCL_VERSION);
}

static int shell_cmd_ls(void) {
    DIR *dir;
    struct dirent *ent;
    dir = opendir(".");
    if (dir == NULL) {
        fprintf(stderr, GCL_COLOR_ERROR "gcl: error: cannot open directory\n" GCL_COLOR_RESET);
        return 1;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        printf(GCL_COLOR_MAGENTA_DIM "  %s\n" GCL_COLOR_RESET, ent->d_name);
    }
    closedir(dir);
    return 0;
}

static int shell_cmd_cd(const char *path) {
    if (path == NULL || path[0] == '\0') {
        fprintf(stderr, GCL_COLOR_ERROR "gcl: error: expected 'cd <path>'\n" GCL_COLOR_RESET);
        return 1;
    }
    if (chdir(path) != 0) {
        fprintf(stderr, GCL_COLOR_ERROR "gcl: error: cannot change directory: %s\n" GCL_COLOR_RESET, path);
        return 1;
    }
    return 0;
}

static int shell_cmd_echo(const char *text) {
    if (text == NULL)
        text = "";
    printf(GCL_COLOR_MAGENTA_DIM "%s\n" GCL_COLOR_RESET, text);
    return 0;
}

/* Read an input line (no malloc — fixed buffer, bounds-checked) */
static int shell_read_line(char *buf, size_t cap) {
    if (fgets(buf, (int)cap, stdin) == NULL)
        return -1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';
    if (len > 0 && buf[len - 1] == '\r')
        buf[--len] = '\0';
    return (int)len;
}

/* Split a command line (simple: by whitespace, no quote support for now) */
static int shell_split(char *line, char **argv, int max_args) {
    int n = 0;
    char *p = line;
    while (*p != '\0' && n < max_args) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        argv[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p != '\0')
            *p++ = '\0';
    }
    return n;
}

/* Join echo args in a separate buffer:
 * "echo a b c" -> "a b c"
 * NOTE: we build into a separate empty buffer instead of strcat'ing onto the
 * pointer (text); otherwise source and destination overlap (UB) — Linux gcc
 * -Wstringop-overflow warns. */
static void shell_cmd_echo_args(char **argv, int argc) {
    char out[SHELL_MAX_LINE];
    size_t pos = 0;

    out[0] = '\0';
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (pos + len + (i > 1 ? 1 : 0) + 1 >= sizeof out) {
            fprintf(stderr, GCL_COLOR_ERROR "gcl: error: echo too long\n" GCL_COLOR_RESET);
            break;
        }
        if (i > 1)
            out[pos++] = ' ';
        memcpy(out + pos, argv[i], len);
        pos += len;
        out[pos] = '\0';
    }
    shell_cmd_echo(out);
}

/* Interactive shell main loop (will become FISH-like — CURRENTLY SIMPLE) */
static int gcl_shell_run(void) {
    char line[SHELL_MAX_LINE];

    printf(GCL_COLOR_MAGENTA_BRIGHT "gcl %s - interactive shell (GNU/Linux)\n" GCL_COLOR_RESET, GCL_VERSION);
    printf(GCL_COLOR_MAGENTA_DIM "Type 'help' to see commands, 'exit' to leave.\n" GCL_COLOR_RESET);

    for (;;) {
        char *argv[64];
        int argc;
        int len;

        printf(GCL_COLOR_MAGENTA SHELL_PROMPT GCL_COLOR_RESET);
        fflush(stdout);

        len = shell_read_line(line, sizeof line);
        if (len < 0)
            break;
        argc = shell_split(line, argv, 64);
        if (argc <= 0)
            continue;

        if (strcmp(argv[0], "exit") == 0)
            break;
        else if (strcmp(argv[0], "help") == 0)
            shell_print_help();
        else if (strcmp(argv[0], "version") == 0)
            shell_print_version();
        else if (strcmp(argv[0], "ls") == 0)
            shell_cmd_ls();
        else if (strcmp(argv[0], "pwd") == 0) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof cwd) != NULL)
                printf(GCL_COLOR_MAGENTA_DIM "%s\n" GCL_COLOR_RESET, cwd);
            else
                fprintf(stderr, GCL_COLOR_ERROR "gcl: error: cannot get working directory\n" GCL_COLOR_RESET);
        } else if (strcmp(argv[0], "cd") == 0)
            shell_cmd_cd(argc > 1 ? argv[1] : NULL);
        else if (strcmp(argv[0], "echo") == 0)
            shell_cmd_echo_args(argv, argc);
        else if (strcmp(argv[0], "clear") == 0) {
            printf("\033[2J\033[H");
        } else {
            fprintf(stderr, GCL_COLOR_ERROR "gcl: error: unknown command: %s (type 'help')\n" GCL_COLOR_RESET, argv[0]);
        }
    }
    return 0;
}

#endif /* GCL_SHELL_GNU_LINUX_C */
