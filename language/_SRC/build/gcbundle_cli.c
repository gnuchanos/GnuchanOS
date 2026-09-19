/*
 * gcbundle_cli.c — gcBundle test CLI.
 *
 * Usage:
 *   gcbmod pack <dir> <name> <out.gcBundle>
 *   gcbmod info <module.gcBundle>
 *   gcbmod read <module.gcBundle> <path>
 */

#include "gcbundle.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>      /* _setmode, _fileno */
#include <fcntl.h>   /* _O_BINARY */
#endif

static void print_usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  gcbmod pack <dir> <name> <out.gcBundle>\n"
        "  gcbmod info <module.gcBundle>\n"
        "  gcbmod read <module.gcBundle> <path>\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { print_usage(); return 1; }

    if (strcmp(argv[1], "pack") == 0 && argc == 5) {
        return gcb_pack_dir(argv[2], argv[3], argv[4]) == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "info") == 0 && argc == 3) {
        GcbBundle *b = gcb_open(argv[2]);
        if (!b) { fprintf(stderr, "gcb error: %s\n", gcb_last_error()); return 1; }
        printf("name: %s\n", gcb_name(b));
        printf("entries: %u\n", (unsigned)gcb_entry_count(b));
        for (uint32_t i = 0; i < gcb_entry_count(b); i++) {
            printf("  [%u] %s (%u bytes)\n", i, gcb_entry_path(b, i),
                   (unsigned)gcb_entry_size(b, i));
        }
        gcb_close(b);
        return 0;
    }

    if (strcmp(argv[1], "read") == 0 && argc == 4) {
        GcbBundle *b = gcb_open(argv[2]);
        if (!b) { fprintf(stderr, "gcb error: %s\n", gcb_last_error()); return 1; }
        uint32_t size = 0;
        const void *data = gcb_read_path(b, argv[3], &size);
        if (!data) { fprintf(stderr, "gcb error: %s\n", gcb_last_error()); gcb_close(b); return 1; }
#ifdef _WIN32
        /* On Windows stdout defaults to TEXT mode: '\n' → "\r\n" is translated
           and the 0x1A (EOF) byte is processed — this CORRUPTS binary output.
           Switch to binary mode. */
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        fwrite(data, 1, size, stdout);
        gcb_close(b);
        return 0;
    }

    print_usage();
    return 1;
}
