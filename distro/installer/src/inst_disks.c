/*
 * inst_disks.c - the disks the installer offers.
 *
 * The list comes from lsblk, one line per block device; the DISKS are the ones
 * whose type is `disk` (partitions and loops are not, and a CD-ROM is `rom`).
 * The disk the live system is itself running from is left OUT: offering it
 * would be offering to erase the system the person is using, which is the one
 * choice an installer must not put in front of them.
 *
 * Only the reading is here. Choosing a disk and writing to it is the window
 * (inst_ui.c) and the script (gnuchanos-install.sh).
 *
 * License: GPL3.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <unistd.h>

#include "inst.h"

/* Run a command and read all of its standard output into a malloc'd string.
   Returns NULL on failure. The command must write no more than a few kilobytes
   - it is `lsblk` and `readlink`, not a stream - so a single growing buffer is
   enough and a pipe that fills is not a case to handle here.
 */
static char *capture(const char *const argv[]) {
    int fds[2];
    if (pipe(fds) != 0) {
        return NULL;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        /* The child's own stderr goes nowhere: a missing lsblk is reported by
           the caller as "no disks", and its grumbling is not the person's to
           read in the middle of the list. */
        freopen("/dev/null", "w", stderr);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(fds[1]);

    size_t cap = 4096, len = 0;
    char *out = malloc(cap);
    if (out == NULL) {
        close(fds[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    for (;;) {
        if (len + 1024 + 1 > cap) {
            cap *= 2;
            char *grown = realloc(out, cap);
            if (grown == NULL) {
                free(out);
                close(fds[0]);
                waitpid(pid, NULL, 0);
                return NULL;
            }
            out = grown;
        }
        ssize_t n = read(fds[0], out + len, 1024);
        if (n <= 0) {
            break;
        }
        len += (size_t)n;
    }
    out[len] = '\0';
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return out;
}

/* The whole disk the live system is running from, as "/dev/sda" (an empty
   string when it cannot be worked out). Something under /run/live or
   /lib/live is the live medium; its partition's name has the trailing number
   and any nvme "p" taken off to get the disk. */
static void live_disk(char *out, size_t size) {
    out[0] = '\0';
    FILE *mounts = fopen("/proc/mounts", "r");
    if (mounts == NULL) {
        return;
    }
    char line[512];
    char device[256] = "";
    while (fgets(line, sizeof(line), mounts) != NULL) {
        if (strstr(line, "/run/live") == NULL &&
            strstr(line, "/lib/live") == NULL) {
            continue;
        }
        if (sscanf(line, "%255s", device) == 1) {
            break;
        }
    }
    fclose(mounts);
    if (device[0] == '\0') {
        return;
    }
    size_t n = strlen(device);
    /* drop a trailing partition number, then a trailing "p" (nvme0n1p1) */
    while (n > 0 && device[n - 1] >= '0' && device[n - 1] <= '9') {
        n--;
    }
    if (n > 0 && device[n - 1] == 'p') {
        n--;
    }
    device[n] = '\0';
    snprintf(out, size, "%s", device);
}

int inst_list_disks(InstApp *app) {
    static const char *const argv[] = {
        "lsblk", "-dno", "NAME,SIZE,TYPE,MODEL", NULL
    };
    char *text = capture(argv);
    app->disk_count = 0;
    if (text == NULL) {
        return 0;
    }

    char live[256];
    live_disk(live, sizeof(live));

    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save);
         line != NULL && app->disk_count < INST_MAX_DISKS;
         line = strtok_r(NULL, "\n", &save)) {
        char name[64], size[64], type[32], model[128];
        model[0] = '\0';
        int fields = sscanf(line, "%63s %63s %31s %127[^\n]",
                            name, size, type, model);
        if (fields < 3) {
            continue;
        }
        if (strcmp(type, "disk") != 0) {
            continue;
        }
        InstDisk *disk = &app->disks[app->disk_count];
        /* The 58 bounds the name so "/dev/" plus it always fits in device[64];
           a kernel name is short in practice, and this only makes the bound
           explicit rather than trusting it. */
        snprintf(disk->device, sizeof(disk->device), "/dev/%.58s", name);
        /* the live system's own disk is not offered (see the file comment) */
        if (live[0] != '\0' && strcmp(disk->device, live) == 0) {
            continue;
        }
        /* The description is what the person reads: the device, its size, and
           the model when the disk gives one. It is built in a local buffer
           first and copied in, because writing into a struct field from a
           format that reads another field of the SAME struct is what makes gcc
           warn about overlapping objects. */
        char description[160];
        if (fields >= 4 && model[0] != '\0') {
            snprintf(description, sizeof(description), "%s   %s   %s",
                     disk->device, size, model);
        } else {
            snprintf(description, sizeof(description), "%s   %s",
                     disk->device, size);
        }
        snprintf(disk->description, sizeof(disk->description), "%s",
                 description);
        app->disk_count++;
    }
    free(text);
    app->disk_choice = app->disk_count > 0 ? 0 : -1;
    return app->disk_count;
}
