/*
 * fetch_info.c — the machine, read a piece at a time.
 *
 * Each field is a small function that answers one question about the machine,
 * and fetch_field_line() is the table that puts a label in front of the answer.
 * The reading is done from the files the kernel and the distribution keep —
 * /proc, /etc/os-release, the package database — rather than from a command, so
 * a field is answered the same way whether or not a shell is on the PATH. The
 * two that are not a file, the user and the shell, come from the environment and
 * from the password database, which is where a login puts them.
 *
 * Nothing here is required to succeed. A field whose source cannot be read gives
 * an empty line, and the caller prints the fields that did have an answer; a
 * machine without a package database simply has no Packages line. That is the
 * whole contract, and it is why every reader has a quiet failure path instead of
 * an error to report.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <pwd.h>

#include "fetch_info.h"

/* --- small helpers --------------------------------------------------------- */

/* The first line of a file, trimmed, into out. Returns 1 when something was
   read. Used for the one-line files under /proc. */
static int first_line(const char *path, char *out, unsigned int size) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }
    if (!fgets(out, (int)size, file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    size_t length = strlen(out);
    while (length > 0 && (out[length - 1] == '\n' || out[length - 1] == '\r' ||
                          out[length - 1] == ' ')) {
        out[--length] = '\0';
    }
    return out[0] != '\0';
}

/* A "Key: value" line of a file like /etc/os-release or /proc/meminfo. The key
   is matched at the start of the line; the value is trimmed of the quotes a
   shell fragment wraps its values in. Returns 1 when the key was found. */
static int keyed_line(const char *path, const char *key, char *out,
                      unsigned int size) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }
    size_t key_length = strlen(key);
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, key, key_length) != 0) {
            continue;
        }
        char *value = line + key_length;
        while (*value == ' ' || *value == '\t' || *value == '=') {
            value++;
        }
        size_t length = strlen(value);
        while (length > 0 && (value[length - 1] == '\n' ||
                              value[length - 1] == '\r' ||
                              value[length - 1] == ' ')) {
            value[--length] = '\0';
        }
        if (length >= 2 && (value[0] == '"' || value[0] == '\'')) {
            value++;
            length -= 2;
            value[length] = '\0';
        }
        snprintf(out, size, "%s", value);
        found = out[0] != '\0';
        break;
    }
    fclose(file);
    return found;
}

/* The last path component of a name: "/bin/bash" gives "bash". */
static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* --- the individual fields ------------------------------------------------- */

static void read_user(char *out, unsigned int size) {
    const char *name = getenv("USER");
    if (!name || !name[0]) {
        name = getenv("LOGNAME");
    }
    if (!name || !name[0]) {
        struct passwd *entry = getpwuid(getuid());
        name = entry ? entry->pw_name : NULL;
    }
    snprintf(out, size, "%s", (name && name[0]) ? name : "");
}

static void read_host(char *out, unsigned int size) {
    /* uname reports the name the kernel was given, which is what the "host" of
       a fetch line means; the whole node name is used, up to the first dot, so
       a machine that keeps its search domain in the host name reads as just the
       host. */
    struct utsname info;
    if (uname(&info) != 0) {
        out[0] = '\0';
        return;
    }
    snprintf(out, size, "%s", info.nodename);
    char *dot = strchr(out, '.');
    if (dot) {
        *dot = '\0';
    }
}

static void read_os(char *out, unsigned int size) {
    char value[FETCH_TEXT_LENGTH];
    if (keyed_line("/etc/os-release", "PRETTY_NAME", value, sizeof(value)) ||
        keyed_line("/etc/os-release", "NAME", value, sizeof(value))) {
        snprintf(out, size, "%s", value);
        return;
    }
    out[0] = '\0';
}

static void read_kernel(char *out, unsigned int size) {
    struct utsname info;
    if (uname(&info) != 0) {
        out[0] = '\0';
        return;
    }
    snprintf(out, size, "%s", info.release);
}

/* The uptime, made into "3 hours, 12 minutes". /proc/uptime is seconds with a
   fraction; only the whole seconds are used. */
static void read_uptime(char *out, unsigned int size) {
    char raw[128];
    if (!first_line("/proc/uptime", raw, sizeof(raw))) {
        out[0] = '\0';
        return;
    }
    long total = atol(raw);
    if (total < 0) {
        out[0] = '\0';
        return;
    }
    long days = total / 86400;
    long hours = (total % 86400) / 3600;
    long minutes = (total % 3600) / 60;

    if (days > 0) {
        snprintf(out, size, "%ld day%s, %ld hour%s, %ld min%s",
                 days, days == 1 ? "" : "s",
                 hours, hours == 1 ? "" : "s",
                 minutes, minutes == 1 ? "" : "s");
    } else if (hours > 0) {
        snprintf(out, size, "%ld hour%s, %ld min%s",
                 hours, hours == 1 ? "" : "s",
                 minutes, minutes == 1 ? "" : "s");
    } else {
        snprintf(out, size, "%ld min%s", minutes, minutes == 1 ? "" : "s");
    }
}

/* How many packages dpkg has installed: one "Package:" line per installed
   package in its status file. A machine without it has no Packages line. */
static void read_packages(char *out, unsigned int size) {
    FILE *file = fopen("/var/lib/dpkg/status", "r");
    if (!file) {
        out[0] = '\0';
        return;
    }
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "Package:", 8) == 0) {
            count++;
        }
    }
    fclose(file);
    if (count > 0) {
        snprintf(out, size, "%d (dpkg)", count);
    } else {
        out[0] = '\0';
    }
}

static void read_shell(char *out, unsigned int size) {
    const char *shell = getenv("SHELL");
    if (shell && shell[0]) {
        snprintf(out, size, "%s", base_name(shell));
        return;
    }
    out[0] = '\0';
}

/* Whether a process with exactly this name is running.
 *
 * `ps -e -o comm=` prints one bare command name per line and no header, which
 * is the smallest output that answers the question. The name is compared WHOLE
 * rather than as a substring: a search for "i3" that matched "pid3" or a match
 * of "wm" inside "xfwm4" would name the wrong thing entirely.
 *
 * This is how the window manager and the display manager are found when the
 * session set no variable for them — a bare session started from a script, or
 * this desktop's own WM, which exports nothing about itself. */
static int process_running(const char *name) {
    FILE *file = popen("ps -e -o comm= 2>/dev/null", "r");
    if (!file) {
        return 0;
    }
    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' ||
                              line[length - 1] == '\r' ||
                              line[length - 1] == ' ')) {
            line[--length] = '\0';
        }
        if (strcmp(line, name) == 0) {
            found = 1;
            break;
        }
    }
    pclose(file);
    return found;
}

/* The first of `names` that is running, written into out. Used to pick one name
   out of the list of well known programs. Returns 1 when one was found. */
static int first_running(const char *const *names, unsigned int count,
                         char *out, unsigned int size) {
    for (unsigned int i = 0; i < count; i++) {
        if (process_running(names[i])) {
            snprintf(out, size, "%s", names[i]);
            return 1;
        }
    }
    return 0;
}

/* The window manager, or the desktop when a full session is running.
 *
 * The environment is asked first and is believed when it answers: a session
 * that NAMES itself is the authority on its own identity. When nothing in the
 * environment says — which is the case for a bare window manager on this
 * desktop, and for a session started from a script — the window manager is
 * found by which of the well known ones is actually RUNNING. The order is most
 * specific first, so a session that also runs a compositor still names the
 * window manager. */
static void read_wm(char *out, unsigned int size) {
    const char *value = getenv("XDG_CURRENT_DESKTOP");
    if (!value || !value[0]) {
        value = getenv("DESKTOP_SESSION");
    }
    if (!value || !value[0]) {
        value = getenv("XDG_SESSION_DESKTOP");
    }
    if (!value || !value[0]) {
        value = getenv("WINDOWMANAGER");
    }
    if (value && value[0]) {
        snprintf(out, size, "%s", value);
        return;
    }

    static const char *const KNOWN[] = {
        "GnuChanWM", "labwc", "sway", "hyprland", "wayfire", "river",
        "xfwm4", "openbox", "i3", "bspwm", "dwm", "marco", "metacity",
        "mutter", "kwin_x11", "kwin_wayland", "fluxbox", "icewm", "jwm",
        "fvwm", "awesome", "herbstluftwm", "spectrwm", "cwm",
    };
    if (!first_running(KNOWN, sizeof(KNOWN) / sizeof(KNOWN[0]), out, size)) {
        out[0] = '\0';
    }
}

/* The display manager: the one that is RUNNING first, and the configured one
   as the fallback.
 *
 * The running greeter is what the person is actually logging in through, and
 * the file `/etc/X11/default-display-manager` can be stale — it names the
 * package that was configured, not the program that was started, so a desktop
 * that ships its own greeter still finds the distribution's name there. The
 * process is therefore believed over the file. */
static void read_dm(char *out, unsigned int size) {
    static const char *const KNOWN[] = {
        "GnuChanDM", "lightdm", "gdm3", "gdm", "sddm", "lxdm",
        "lxdm-binary", "xdm", "slim", "greetd",
    };
    if (first_running(KNOWN, sizeof(KNOWN) / sizeof(KNOWN[0]), out, size)) {
        return;
    }

    char value[FETCH_TEXT_LENGTH];
    if (first_line("/etc/X11/default-display-manager", value, sizeof(value)) &&
        value[0]) {
        snprintf(out, size, "%s", base_name(value));
        return;
    }
    out[0] = '\0';
}

/* The GTK 3 settings file, which is where a theme selection is written on a
   session with no XSettings daemon — which is every bare window manager. */
static int gtk3_settings_path(char *out, unsigned int size) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return 0;
    }
    snprintf(out, size, "%s/.config/gtk-3.0/settings.ini", home);
    return 1;
}

/* The GTK theme's name, from the settings file. Empty when none is set, so the
   line is left out rather than printing a label with nothing after it. */
static void read_theme(char *out, unsigned int size) {
    char path[512];
    char value[FETCH_TEXT_LENGTH];
    if (gtk3_settings_path(path, sizeof(path)) &&
        keyed_line(path, "gtk-theme-name", value, sizeof(value))) {
        snprintf(out, size, "%s", value);
        return;
    }
    out[0] = '\0';
}

/* The icon theme's name, from the same file. */
static void read_icons(char *out, unsigned int size) {
    char path[512];
    char value[FETCH_TEXT_LENGTH];
    if (gtk3_settings_path(path, sizeof(path)) &&
        keyed_line(path, "gtk-icon-theme-name", value, sizeof(value))) {
        snprintf(out, size, "%s", value);
        return;
    }
    out[0] = '\0';
}

/* The cursor theme's name, from the same file. */
static void read_cursor(char *out, unsigned int size) {
    char path[512];
    char value[FETCH_TEXT_LENGTH];
    if (gtk3_settings_path(path, sizeof(path)) &&
        keyed_line(path, "gtk-cursor-theme-name", value, sizeof(value))) {
        snprintf(out, size, "%s", value);
        return;
    }
    out[0] = '\0';
}

static void read_terminal(char *out, unsigned int size) {
    const char *program = getenv("TERM_PROGRAM");
    if (program && program[0]) {
        snprintf(out, size, "%s", program);
        return;
    }
    const char *term = getenv("TERM");
    if (term && term[0]) {
        snprintf(out, size, "%s", term);
        return;
    }
    out[0] = '\0';
}

/* The CPU's model name, from /proc/cpuinfo. The first "model name" line is the
   first core, which is every core on a machine whose cores are the same. */
static void read_cpu(char *out, unsigned int size) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file) {
        out[0] = '\0';
        return;
    }
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "model name", 10) != 0 &&
            strncmp(line, "Model", 5) != 0 &&
            strncmp(line, "Hardware", 8) != 0) {
            continue;
        }
        char *colon = strchr(line, ':');
        if (!colon) {
            continue;
        }
        char *value = colon + 1;
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        size_t length = strlen(value);
        while (length > 0 && (value[length - 1] == '\n' ||
                              value[length - 1] == '\r' ||
                              value[length - 1] == ' ')) {
            value[--length] = '\0';
        }
        if (value[0]) {
            /* The trailing clock in parentheses is noise on a fetch line. */
            char *paren = strstr(value, " @ ");
            if (paren) {
                *paren = '\0';
            }
            snprintf(out, size, "%s", value);
            found = 1;
            break;
        }
    }
    fclose(file);
    if (!found) {
        out[0] = '\0';
    }
}

/* A DMI string the firmware filled in, or an empty line when the firmware left
   the placeholder its own vendor put there. The placeholders are the ones
   boards actually ship: an unset field holds "System manufacturer", "To Be
   Filled By O.E.M." or "Default string", and printing one of those would be
   worse than printing nothing. */
static int read_dmi(const char *file, char *out, unsigned int size) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/dmi/id/%s", file);
    char value[FETCH_TEXT_LENGTH];
    if (!first_line(path, value, sizeof(value))) {
        out[0] = '\0';
        return 0;
    }
    static const char *const PLACEHOLDERS[] = {
        "System manufacturer", "System Product Name", "System Version",
        "To Be Filled By O.E.M.", "To be filled by O.E.M.", "Default string",
        "Not Specified", "Not Applicable", "OEM", "None", "Unknown",
    };
    for (unsigned int i = 0;
         i < sizeof(PLACEHOLDERS) / sizeof(PLACEHOLDERS[0]); i++) {
        if (strcmp(value, PLACEHOLDERS[i]) == 0) {
            out[0] = '\0';
            return 0;
        }
    }
    snprintf(out, size, "%s", value);
    return 1;
}

/* The machine's model: the vendor and the product name from the firmware, as
   "Dell Inc. Vostro A860". The vendor is joined to the product because a bare
   model number is ambiguous — 3000 is a Dell and a Lenovo both — and the pair
   is what `DMI` is for. When only one of the two could be read, that one is
   used; when neither could, the line is left out. */
static void read_model(char *out, unsigned int size) {
    char vendor[FETCH_TEXT_LENGTH];
    char product[FETCH_TEXT_LENGTH];
    char version[FETCH_TEXT_LENGTH];
    vendor[0] = '\0';
    product[0] = '\0';
    version[0] = '\0';
    read_dmi("sys_vendor", vendor, sizeof(vendor));
    if (!read_dmi("product_name", product, sizeof(product))) {
        read_dmi("product_version", product, sizeof(product));
    }
    read_dmi("product_version", version, sizeof(version));

    /* The version is only worth showing when it is not the product name again
       and not a number the product name already carries — "Vostro A860" with a
       version of "A860" is one machine named twice. */
    int show_version = version[0] && strcmp(version, product) != 0 &&
                       strstr(product, version) == NULL;

    if (vendor[0] && product[0]) {
        if (show_version) {
            snprintf(out, size, "%s %s %s", vendor, product, version);
        } else {
            snprintf(out, size, "%s %s", vendor, product);
        }
    } else if (product[0]) {
        snprintf(out, size, "%s", product);
    } else if (vendor[0]) {
        snprintf(out, size, "%s", vendor);
    } else {
        out[0] = '\0';
    }
}

/* The graphics adapter.
 *
 * lspci is what names the chip the way a person knows it — "Intel Corporation
 * Mobile GM965/GL960 Integrated Graphics Controller" — and it is believed
 * first, because only its database can turn a pair of PCI ids into a name. The
 * output line is cut down to the part after the class, so the bus address and
 * the words "VGA compatible controller:" are dropped and the revision with
 * them.
 *
 * A machine without pciutils is not left with nothing: the KERNEL DRIVER that
 * is bound to the first DRM card is read from sysfs instead, which gives
 * "i915", "amdgpu", "nouveau" or "nvidia" — the family the adapter belongs to,
 * which is the useful half of the answer and the half that is always there on
 * a machine that is drawing anything at all. */
static void read_gpu(char *out, unsigned int size) {
    FILE *file = popen("lspci 2>/dev/null | grep -iE 'vga|3d|display' | head -n 1",
                       "r");
    if (file) {
        char line[512];
        if (fgets(line, sizeof(line), file) && line[0]) {
            /* "00:02.0 VGA compatible controller: Intel Corporation ..." —
               the first colon ends the bus address, the second ends the class,
               and the name is what is left. */
            char *first = strchr(line, ':');
            char *second = first ? strchr(first + 1, ':') : NULL;
            char *name = second ? second + 1 : NULL;
            if (name) {
                while (*name == ' ' || *name == '\t') {
                    name++;
                }
                size_t length = strlen(name);
                while (length > 0 && (name[length - 1] == '\n' ||
                                      name[length - 1] == '\r' ||
                                      name[length - 1] == ' ')) {
                    name[--length] = '\0';
                }
                /* The trailing "(rev 0c)" is noise on a fetch line. */
                char *rev = strstr(name, " (rev ");
                if (rev) {
                    *rev = '\0';
                }
                snprintf(out, size, "%s", name);
                pclose(file);
                if (out[0]) {
                    return;
                }
            }
        }
        pclose(file);
    }

    /* The DRM driver, which is the family the adapter belongs to. */
    const char *cards[] = {"/sys/class/drm/card0", "/sys/class/drm/card1"};
    for (unsigned int i = 0; i < sizeof(cards) / sizeof(cards[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/device/driver", cards[i]);
        char target[512];
        ssize_t got = readlink(path, target, sizeof(target) - 1);
        if (got > 0) {
            target[got] = '\0';
            snprintf(out, size, "%s", base_name(target));
            return;
        }
    }
    out[0] = '\0';
}

/* The memory in use and total, as GiB with one decimal, from /proc/meminfo. The
   "used" a person means is total minus available, which is the number the free
   command shows and the one that counts the page cache as free. */
static void read_memory(char *out, unsigned int size) {
    char raw[128];
    if (!keyed_line("/proc/meminfo", "MemTotal:", raw, sizeof(raw))) {
        out[0] = '\0';
        return;
    }
    long total_kib = atol(raw);
    long available_kib = 0;
    char available[128];
    if (keyed_line("/proc/meminfo", "MemAvailable:", available,
                   sizeof(available))) {
        available_kib = atol(available);
    } else if (keyed_line("/proc/meminfo", "MemFree:", available,
                          sizeof(available))) {
        available_kib = atol(available);
    }
    if (total_kib <= 0) {
        out[0] = '\0';
        return;
    }
    long used_kib = total_kib - available_kib;
    if (used_kib < 0) {
        used_kib = 0;
    }
    /* Values are in KiB, so a GiB is 1024 * 1024 KiB. */
    double total_gib = (double)total_kib / (1024.0 * 1024.0);
    double used_gib = (double)used_kib / (1024.0 * 1024.0);
    snprintf(out, size, "%.1f GiB / %.1f GiB", used_gib, total_gib);
}

/* The disk the root filesystem is on: used and total, as GiB. The total of a
   filesystem is its size, and f_blocks * f_frsize is it in bytes. */
static void read_disk(char *out, unsigned int size) {
    struct statvfs info;
    if (statvfs("/", &info) != 0 || info.f_blocks == 0) {
        out[0] = '\0';
        return;
    }
    double block = (double)info.f_frsize;
    double total = (double)info.f_blocks * block / (1024.0 * 1024.0 * 1024.0);
    double free = (double)info.f_bavail * block / (1024.0 * 1024.0 * 1024.0);
    double used = total - free;
    if (used < 0.0) {
        used = 0.0;
    }
    snprintf(out, size, "%.1f GiB / %.1f GiB", used, total);
}

/* --- the table ------------------------------------------------------------- */

/* One row of the table: the name a settings file writes, the label that goes on
   the line, and the reader. */
typedef struct FieldReader {
    const char *name;
    const char *label;
    FetchField field;
    void (*read)(char *out, unsigned int size);
} FieldReader;

static const FieldReader READERS[] = {
    {"user",     "User",     FETCH_FIELD_USER,     read_user},
    {"host",     "Host",     FETCH_FIELD_HOST,     read_host},
    {"os",       "OS",       FETCH_FIELD_OS,       read_os},
    {"kernel",   "Kernel",   FETCH_FIELD_KERNEL,   read_kernel},
    {"uptime",   "Uptime",   FETCH_FIELD_UPTIME,   read_uptime},
    {"packages", "Packages", FETCH_FIELD_PACKAGES, read_packages},
    {"shell",    "Shell",    FETCH_FIELD_SHELL,    read_shell},
    {"wm",       "WM",       FETCH_FIELD_WM,       read_wm},
    {"dm",       "DM",       FETCH_FIELD_DM,       read_dm},
    {"theme",    "Theme",    FETCH_FIELD_THEME,    read_theme},
    {"icons",    "Icons",    FETCH_FIELD_ICONS,    read_icons},
    {"cursor",   "Cursor",   FETCH_FIELD_CURSOR,   read_cursor},
    {"terminal", "Terminal", FETCH_FIELD_TERMINAL, read_terminal},
    {"model",    "Model",    FETCH_FIELD_MODEL,    read_model},
    {"cpu",      "CPU",      FETCH_FIELD_CPU,      read_cpu},
    {"gpu",      "GPU",      FETCH_FIELD_GPU,      read_gpu},
    {"memory",   "Memory",   FETCH_FIELD_MEMORY,   read_memory},
    {"disk",     "Disk",     FETCH_FIELD_DISK,     read_disk},
};

static const unsigned int READER_COUNT =
    sizeof(READERS) / sizeof(READERS[0]);

/* --- the public entry points ---------------------------------------------- */

FetchField fetch_field_from_name(const char *name) {
    if (!name) {
        return FETCH_FIELD_UNKNOWN;
    }
    for (unsigned int i = 0; i < READER_COUNT; i++) {
        if (strcmp(READERS[i].name, name) == 0) {
            return READERS[i].field;
        }
    }
    return FETCH_FIELD_UNKNOWN;
}

void fetch_field_line(FetchField field, char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    out[0] = '\0';
    if (field == FETCH_FIELD_UNKNOWN) {
        return;
    }
    for (unsigned int i = 0; i < READER_COUNT; i++) {
        if (READERS[i].field != field) {
            continue;
        }
        char value[FETCH_TEXT_LENGTH];
        value[0] = '\0';
        READERS[i].read(value, sizeof(value));
        if (value[0]) {
            snprintf(out, size, "%s: %s", READERS[i].label, value);
        }
        return;
    }
}

void fetch_title_line(char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    char user[FETCH_TEXT_LENGTH];
    char host[FETCH_TEXT_LENGTH];
    user[0] = '\0';
    host[0] = '\0';
    read_user(user, sizeof(user));
    read_host(host, sizeof(host));

    if (user[0] && host[0]) {
        snprintf(out, size, "%s@%s", user, host);
    } else if (user[0]) {
        snprintf(out, size, "%s", user);
    } else if (host[0]) {
        snprintf(out, size, "%s", host);
    } else {
        out[0] = '\0';
    }
}
