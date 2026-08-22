#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

typedef struct {
    int kind;
    int64_t i_val;
    double f_val;
    const char *s_val;
} Val;

extern int64_t val_to_int(Val v);
extern Val pop(void *st);


/* =========================================================
   STRING ESCAPE OUTPUT
   ========================================================= */

static void print_string_value(const char *s)
{
    if (!s) {
        printf("(null)");
        return;
    }

    size_t len = strlen(s);

    /*
     * GnuChan string value:
     *
     * "hello\nworld"
     *
     * şeklinde tutuluyorsa dış tırnakları kaldır.
     */
    if (len >= 2 &&
        s[0] == '"' &&
        s[len - 1] == '"') {

        s++;
        len -= 2;
    }

    for (size_t i = 0; i < len; i++) {

        if (s[i] == '\\' && i + 1 < len) {

            i++;

            switch (s[i]) {

            case 'n':
                putchar('\n');
                break;

            case 't':
                putchar('\t');
                break;

            case 'r':
                putchar('\r');
                break;

            case '\\':
                putchar('\\');
                break;

            case '"':
                putchar('"');
                break;

            case '\'':
                putchar('\'');
                break;

            default:
                putchar('\\');
                putchar(s[i]);
                break;
            }

        } else {

            putchar(s[i]);
        }
    }
}


/* =========================================================
   CHAR OUTPUT
   ========================================================= */

static void print_char_value(Val v)
{
    /*
     * GnuChan char:
     *
     * 'A'
     *
     * şeklinde string olarak tutuluyorsa.
     */
    if (v.s_val) {

        const char *s = v.s_val;
        size_t len = strlen(s);

        if (len >= 3 &&
            s[0] == '\'' &&
            s[len - 1] == '\'') {

            putchar(s[1]);
            return;
        }

        if (len > 0) {
            putchar(s[0]);
            return;
        }
    }

    putchar((char)val_to_int(v));
}


/* =========================================================
   FORMAT PARSER
   ========================================================= */

static const char *skip_printf_flags(const char *p)
{
    while (*p == '-' ||
           *p == '+' ||
           *p == ' ' ||
           *p == '#' ||
           *p == '0') {

        p++;
    }

    return p;
}


/* =========================================================
   PRINTF MODULE
   ========================================================= */

void interp_printf_module(void *st, int argc)
{
    if (!st || argc < 1)
        return;


    /* =====================================================
       ARGUMENTLARI STACK'TEN AL
       ===================================================== */

    Val args[64];

    /* Security: cap arguments at 64 */
    int n = argc < 64 ? argc : 64;

    /*
     * Stack:
     *
     * format
     * arg1
     * arg2
     * arg3
     *
     * Pop reverse order.
     */
    for (int i = n - 1; i >= 0; i--) {
        args[i] = pop(st);
    }


    /* =====================================================
       FORMAT
       ===================================================== */

    const char *fmt = args[0].s_val;

    if (!fmt) {
        printf("(null)");
        return;
    }


    /*
     * GnuChan string literal:
     *
     * "value = %d\n"
     *
     * şeklinde geliyorsa dış tırnakları kaldır.
     */
    char fmtbuf[4096];

    size_t flen = strlen(fmt);

    /* Security: validate format string length */
    if (flen > 2048) {
        fprintf(stderr, "gcl: printf: format string too long (%zu bytes, max 2048)\n", flen);
        return;
    }

    if (flen >= 2 &&
        fmt[0] == '"' &&
        fmt[flen - 1] == '"') {

        size_t copy_len = flen - 2;

        if (copy_len >= sizeof(fmtbuf))
            copy_len = sizeof(fmtbuf) - 1;

        memcpy(
            fmtbuf,
            fmt + 1,
            copy_len
        );

        fmtbuf[copy_len] = '\0';

        fmt = fmtbuf;
    }


    int ai = 1;


    /* =====================================================
       FORMAT'I GEZ
       ===================================================== */

    for (const char *p = fmt; *p; p++) {


        /* =================================================
           ESCAPE
           ================================================= */

        if (*p == '\\' && *(p + 1)) {

            p++;

            switch (*p) {

            case 'n':
                putchar('\n');
                break;

            case 't':
                putchar('\t');
                break;

            case 'r':
                putchar('\r');
                break;

            case '\\':
                putchar('\\');
                break;

            case '"':
                putchar('"');
                break;

            case '\'':
                putchar('\'');
                break;

            default:
                putchar('\\');
                putchar(*p);
                break;
            }

            continue;
        }


        /* =================================================
           NORMAL CHARACTER
           ================================================= */

        if (*p != '%') {

            putchar(*p);
            continue;
        }


        /* =================================================
           %% 
           ================================================= */

        if (*(p + 1) == '%') {

            putchar('%');
            p++;

            continue;
        }


        /*
         * Argüman yoksa.
         */
        if (ai >= n) {

            putchar('%');
            continue;
        }


        /* =================================================
           FORMAT PARAMETRELERİ
           ================================================= */

        p++;

        /*
         * flags
         */
        p = skip_printf_flags(p);


        /*
         * width - Security: limit to 256
         */
        int width = 0;
        while (isdigit((unsigned char)*p) && width < 256) {
            width = width * 10 + (*p - '0');
            p++;
        }
        if (isdigit((unsigned char)*p))
            while (isdigit((unsigned char)*p)) p++;


        /*
         * precision - Security: limit to 256
         */
        if (*p == '.') {

            p++;
            int prec = 0;
            while (isdigit((unsigned char)*p) && prec < 256) {
                prec = prec * 10 + (*p - '0');
                p++;
            }
            if (isdigit((unsigned char)*p))
                while (isdigit((unsigned char)*p)) p++;
        }


        /*
         * length modifier
         *
         * h
         * hh
         * l
         * ll
         * L
         * z
         *
         */
        if (*p == 'h') {

            p++;

            if (*p == 'h')
                p++;

        } else if (*p == 'l') {

            p++;

            if (*p == 'l')
                p++;

        } else if (*p == 'L') {

            p++;

        } else if (*p == 'z') {

            p++;
        }


        /* =================================================
           CONVERSION
           ================================================= */

        switch (*p) {


        /* =================================================
           SIGNED INTEGER
           ================================================= */

        case 'd':
        case 'i':
        {
            printf(
                "%lld",
                (long long)val_to_int(args[ai])
            );

            ai++;

            break;
        }


        /* =================================================
           UNSIGNED INTEGER
           ================================================= */

        case 'u':
        {
            uint64_t value =
                (uint64_t)val_to_int(args[ai]);

            printf(
                "%llu",
                (unsigned long long)value
            );

            ai++;

            break;
        }


        /* =================================================
           FLOAT
           ================================================= */

        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        {
            if (args[ai].kind == 2) {

                printf(
                    "%f",
                    args[ai].f_val
                );

            } else {

                printf(
                    "%f",
                    (double)val_to_int(args[ai])
                );
            }

            ai++;

            break;
        }


        /* =================================================
           STRING
           ================================================= */

        case 's':
        {
            print_string_value(
                args[ai].s_val
            );

            ai++;

            break;
        }


        /* =================================================
           CHAR
           ================================================= */

        case 'c':
        {
            print_char_value(
                args[ai]
            );

            ai++;

            break;
        }


        /* =================================================
           HEX
           ================================================= */

        case 'x':
        {
            uint64_t value =
                (uint64_t)val_to_int(args[ai]);

            printf(
                "%llx",
                (unsigned long long)value
            );

            ai++;

            break;
        }


        /* =================================================
           HEX UPPERCASE
           ================================================= */

        case 'X':
        {
            uint64_t value =
                (uint64_t)val_to_int(args[ai]);

            printf(
                "%llX",
                (unsigned long long)value
            );

            ai++;

            break;
        }


        /* =================================================
           OCTAL
           ================================================= */

        case 'o':
        {
            uint64_t value =
                (uint64_t)val_to_int(args[ai]);

            printf(
                "%llo",
                (unsigned long long)value
            );

            ai++;

            break;
        }


        /* =================================================
           POINTER
           ================================================= */

        case 'p':
        {
            uint64_t value =
                (uint64_t)val_to_int(args[ai]);

            printf(
                "0x%llx",
                (unsigned long long)value
            );

            ai++;

            break;
        }


        /* =================================================
           UNKNOWN
           ================================================= */

        default:

            putchar('%');
            putchar(*p);

            break;
        }
    }
}