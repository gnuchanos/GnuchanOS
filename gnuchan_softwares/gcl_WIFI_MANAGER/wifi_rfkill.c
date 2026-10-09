/*
 * wifi_rfkill.c — reading the kill switches and reloading the driver.
 *
 * The state is read straight out of /sys/class/rfkill: one directory per
 * switch, each with a `type` (wlan, bluetooth, ...), a `hard` and a `soft`
 * file holding 0 or 1. Only the wlan ones matter here. Reading sysfs needs no
 * command and no privilege; changing anything does, which is what the sudo at
 * start-up is for.
 *
 * The unblock and the reload go through wifi_shell.c like every other command.
 * `rfkill` is named first as found on PATH and then at /usr/sbin, because on
 * Debian it lives in sbin and a PATH without sbin would otherwise report the
 * tool missing — the same trap the dotfiles' own wifi_fix.py documents.
 */
#include "wifi_rfkill.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Read a one-value file into a small buffer and return its first character, or
   '\0' when it could not be read. Used for both the switch files and the state
   of a driver read. */
static char read_first(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return '\0';
    }
    int c = fgetc(file);
    fclose(file);
    return (c == EOF) ? '\0' : (char)c;
}

/* Read a whole line (its newline trimmed) into `out`. Returns 1 on success. */
static int read_line(const char *path, char *out, unsigned int size) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }
    if (!fgets(out, (int)size, file)) {
        fclose(file);
        out[0] = '\0';
        return 0;
    }
    fclose(file);
    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

int wifi_rfkill_state(WifiBlock *block) {
    if (!block) {
        return -1;
    }
    block->hard = 0;
    block->soft = 0;

    DIR *dir = opendir("/sys/class/rfkill");
    if (!dir) {
        /* No rfkill at all: nothing is blocked by it, and that is a success —
           the answer is "not blocked", not "unknown". */
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "rfkill", 6) != 0) {
            continue;
        }
        char path[512];
        char type[64];

        snprintf(path, sizeof(path), "/sys/class/rfkill/%s/type",
                 entry->d_name);
        if (!read_line(path, type, sizeof(type))) {
            continue;
        }
        if (strcmp(type, "wlan") != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "/sys/class/rfkill/%s/hard",
                 entry->d_name);
        if (read_first(path) == '1') {
            block->hard = 1;
        }
        snprintf(path, sizeof(path), "/sys/class/rfkill/%s/soft",
                 entry->d_name);
        if (read_first(path) == '1') {
            block->soft = 1;
        }
    }
    closedir(dir);
    return 0;
}

int wifi_rfkill_unblock(char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    /* `rfkill` first as named, then at the two sbin paths Debian keeps it in.
       The first form that runs is the one used; a machine with none is told
       so. This is the same search the dotfiles' wifi_fix.py does by hand. */
    const char *candidates[] = {
        "rfkill unblock all",
        "/usr/sbin/rfkill unblock all",
        "/sbin/rfkill unblock all",
        NULL,
    };

    static char output[WIFI_TEXT * 4];
    int ran = 0;
    for (int i = 0; candidates[i]; i++) {
        if (wifi_shell_run(candidates[i], output, sizeof(output)) == 0) {
            ran = 1;
            break;
        }
    }

    if (!ran) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "rfkill could not be run");
        }
        return -1;
    }
    return 0;
}

void wifi_rfkill_driver(const char *device, char *out, unsigned int size) {
    if (size) {
        out[0] = '\0';
    }
    if (!device || !device[0]) {
        return;
    }

    /* /sys/class/net/<device>/device/driver is a symlink to the driver's own
       directory; its last path component is the module's name — "ath5k",
       "wl", and so on. That is the name modprobe wants, asked of the kernel
       rather than guessed at in a config. */
    char path[512];
    snprintf(path, sizeof(path), "/sys/class/net/%s/device/driver", device);

    char target[512];
    ssize_t got = readlink(path, target, sizeof(target) - 1);
    if (got <= 0) {
        return;
    }
    target[got] = '\0';

    const char *slash = strrchr(target, '/');
    snprintf(out, size, "%s", slash ? slash + 1 : target);
}

int wifi_rfkill_restart(const char *module, char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    /* 1. Clear the blocks. A hard block cannot be cleared by software, but the
          command tries and reports; a soft one goes, which is the common case. */
    if (wifi_rfkill_unblock(error, size) != 0) {
        return -1;
    }

    /* 2 and 3. The driver out and back, which is the piece the Vostro needs
          when its card has latched itself "radio off". Only done when a module
          name is known: with none, this is just an unblock, which is the honest
          smaller action rather than a blind guess at a module name. */
    if (!module || !module[0]) {
        return 0;
    }

    char quoted[WIFI_TEXT * 2];
    wifi_shell_quote(module, quoted, sizeof(quoted));

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "modprobe -r %s", quoted);

    static char output[WIFI_TEXT * 4];
    /* A failure to REMOVE is not fatal: a module that is not loaded is already
       in the state the removal wanted, and modprobe says so on stderr. The
       load below is the one that has to work. */
    wifi_shell_run(command, output, sizeof(output));

    /* A moment between, so the kernel has finished tearing the interface down
       before it is asked to build it again. This is exactly the `sleep 1` in
       the dotfiles' own recovery script. */
    wifi_shell_run("sleep 1", output, sizeof(output));

    snprintf(command, sizeof(command), "modprobe %s", quoted);
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "the driver could not be reloaded");
        }
        return -1;
    }

    return 0;
}
