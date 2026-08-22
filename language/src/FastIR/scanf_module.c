#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

typedef struct {
    int kind;
    int64_t i_val;
    double f_val;
    const char *s_val;
} Val;

typedef struct {
    const char *name;
    Val value;
} VarSlot;

typedef struct {
    VarSlot slots[256];
    int count;
} VarTable;


/* =========================================================
   INTERPRETER
   ========================================================= */

extern void store_var(void *st, const char *name, Val val);

extern Val val_int(int64_t v);
extern Val val_float(double v);
extern Val val_string(const char *s);


/* =========================================================
   INPUT BUFFER
   ========================================================= */

static char g_input[4096];

static char *g_ptr = NULL;

static int g_has_line = 0;


/* =========================================================
   READ LINE
   ========================================================= */

static int read_input_line(void)
{
    fflush(stdout);

    if (!fgets(g_input, sizeof(g_input), stdin)) {
        g_input[0] = '\0';
        g_ptr = g_input;
        g_has_line = 0;
        return 0;
    }

    /* Security: validate line length */
    size_t line_len = strlen(g_input);
    if (line_len >= sizeof(g_input) - 1) {
        fprintf(stderr, "gcl: scanf: input line truncated (max %zu bytes)\n", 
                sizeof(g_input) - 1);
    }

    g_ptr = g_input;
    g_has_line = 1;

    return 1;
}


/* =========================================================
   REMOVE CR / LF
   ========================================================= */

static void remove_line_end(void)
{
    size_t len = strlen(g_input);

    while (len > 0) {
        if (g_input[len - 1] == '\n' ||
            g_input[len - 1] == '\r') {

            g_input[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}


/* =========================================================
   SKIP WHITESPACE
   ========================================================= */

static void skip_spaces(void)
{
    while (g_ptr &&
           *g_ptr &&
           isspace((unsigned char)*g_ptr)) {

        g_ptr++;
    }
}


/* =========================================================
   GET TOKEN
   ========================================================= */

static int get_token(
    char *out,
    size_t size
)
{
    if (!out || size < 2)
        return 0;

    /* Security: cap maximum token size */
    if (size > 512)
        size = 512;

    size_t n = 0;
    size_t safe_max = size - 1;

    while (1) {

        if (!g_has_line) {

            if (!read_input_line()) {
                out[n] = '\0';
                return n > 0;  /* Return accumulated token if any */
            }

            remove_line_end();
        }

        skip_spaces();

        /*
         * Satır bitti, henüz token başlamadı.
         * Yeni satır oku ve devam et.
         */
        if (!*g_ptr) {

            /* Eğer token biriktirilmişse döndür */
            if (n > 0) {
                out[n] = '\0';
                g_has_line = 0;
                return 1;
            }

            g_has_line = 0;
            continue;
        }

        /* Token karakteri - satır başlamışsa toplama başla */
        while (*g_ptr &&
               !isspace((unsigned char)*g_ptr)) {

            if (n < safe_max) {
                out[n++] = *g_ptr;
            } else {
                fprintf(stderr, "gcl: scanf: token truncated (max %zu bytes)\n", safe_max);
            }

            g_ptr++;
        }

        out[n] = '\0';

        return n > 0;
    }
}


/* =========================================================
   GET CHAR
   ========================================================= */

static int get_char_value(char *out)
{
    if (!out)
        return 0;

    while (1) {

        if (!g_has_line) {

            if (!read_input_line())
                return 0;

            remove_line_end();
        }

        /*
         * scanf(" %c") mantığı:
         * whitespace atla.
         */
        skip_spaces();

        if (!*g_ptr) {
            g_has_line = 0;
            continue;
        }

        *out = *g_ptr;

        /*
         * Karakteri tüket.
         */
        g_ptr++;

        /*
         * ÖNEMLİ:
         *
         * Kullanıcı "50" girdiyse %c
         * sadece '5'i almamalı.
         *
         * Scanner çağrısının geri kalanını
         * temizliyoruz.
         */
        while (*g_ptr)
            g_ptr++;

        g_has_line = 0;

        return 1;
    }
}


/* =========================================================
   STRING
   ========================================================= */

static void store_string(
    void *st,
    const char *name,
    const char *value
)
{
    if (!st || !name || !value)
        return;

    char buffer[4096];

    snprintf(
        buffer,
        sizeof(buffer),
        "\"%s\"",
        value
    );

    store_var(
        st,
        name,
        val_string(buffer)
    );
}


/* =========================================================
   SIGNED INTEGER
   ========================================================= */

static int parse_signed(
    const char *text,
    int base,
    int64_t *result
)
{
    if (!text || !result)
        return 0;

    /* Security: validate text pointer and length */
    if (strlen(text) > 256)
        return 0;

    errno = 0;

    char *end = NULL;

    long long value =
        strtoll(text, &end, base);

    if (end == text)
        return 0;

    if (errno == ERANGE)
        return 0;

    while (*end) {

        if (!isspace((unsigned char)*end))
            return 0;

        end++;
    }

    *result = (int64_t)value;

    return 1;
}


/* =========================================================
   UNSIGNED INTEGER
   ========================================================= */

static int parse_unsigned(
    const char *text,
    int base,
    uint64_t *result
)
{
    if (!text || !result)
        return 0;

    /* Security: validate text pointer and length */
    if (strlen(text) > 256)
        return 0;

    errno = 0;

    char *end = NULL;

    unsigned long long value =
        strtoull(text, &end, base);

    if (end == text)
        return 0;

    if (errno == ERANGE)
        return 0;

    while (*end) {

        if (!isspace((unsigned char)*end))
            return 0;

        end++;
    }

    *result = (uint64_t)value;

    return 1;
}


/* =========================================================
   FLOAT
   ========================================================= */

static int parse_float(
    const char *text,
    double *result
)
{
    if (!text || !result)
        return 0;

    /* Security: validate text pointer and length */
    if (strlen(text) > 256)
        return 0;

    errno = 0;

    char *end = NULL;

    double value =
        strtod(text, &end);

    if (end == text)
        return 0;

    if (errno == ERANGE)
        return 0;

    while (*end) {

        if (!isspace((unsigned char)*end))
            return 0;

        end++;
    }

    *result = value;

    return 1;
}


/* =========================================================
   STORE SIGNED
   ========================================================= */

static void store_signed(
    void *st,
    const char *name,
    const char *token
)
{
    int64_t value;

    if (!parse_signed(token, 10, &value))
        return;

    store_var(
        st,
        name,
        val_int(value)
    );
}


/* =========================================================
   STORE UNSIGNED
   ========================================================= */

static void store_unsigned(
    void *st,
    const char *name,
    const char *token
)
{
    uint64_t value;

    if (!parse_unsigned(token, 10, &value))
        return;

    /*
     * Val şu anda int64 tuttuğu için
     * INT64_MAX üzerindeki değerleri doğrudan
     * temsil edemiyoruz.
     */
    if (value > INT64_MAX)
        return;

    store_var(
        st,
        name,
        val_int((int64_t)value)
    );
}


/* =========================================================
   MAIN SCANF MODULE
   ========================================================= */

void interp_scanf_module(
    void *st,
    const char *name,
    int64_t kind
)
{
    if (!st || !name)
        return;


    /* =====================================================
       STRING
       ===================================================== */

    if (kind == 0) {

        char token[4096];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_string(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       SIGNED INT
       ===================================================== */

    if (kind == 1) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_signed(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       FLOAT
       ===================================================== */

    if (kind == 2) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        double value;

        if (!parse_float(
                token,
                &value)) {

            return;
        }

        store_var(
            st,
            name,
            val_float(value)
        );

        return;
    }


    /* =====================================================
       CHAR
       ===================================================== */

    if (kind == 3) {

        char c;

        if (!get_char_value(&c))
            return;

        char buffer[8];

        snprintf(
            buffer,
            sizeof(buffer),
            "'%c'",
            c
        );

        store_var(
            st,
            name,
            val_string(buffer)
        );

        return;
    }


    /* =====================================================
       SHORT
       ===================================================== */

    if (kind == 4) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        int64_t value;

        if (!parse_signed(
                token,
                10,
                &value)) {

            return;
        }

        if (value < SHRT_MIN ||
            value > SHRT_MAX) {

            return;
        }

        store_var(
            st,
            name,
            val_int(value)
        );

        return;
    }


    /* =====================================================
       UNSIGNED INT
       ===================================================== */

    if (kind == 5) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        uint64_t value;

        if (!parse_unsigned(
                token,
                10,
                &value)) {

            return;
        }

        if (value > UINT_MAX)
            return;

        store_var(
            st,
            name,
            val_int((int64_t)value)
        );

        return;
    }


    /* =====================================================
       LONG
       ===================================================== */

    if (kind == 6) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_signed(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       LONG LONG
       ===================================================== */

    if (kind == 7) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_signed(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       UNSIGNED LONG
       ===================================================== */

    if (kind == 8) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_unsigned(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       OCTAL
       ===================================================== */

    if (kind == 9) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        int64_t value;

        if (!parse_signed(
                token,
                8,
                &value)) {

            return;
        }

        store_var(
            st,
            name,
            val_int(value)
        );

        return;
    }


    /* =====================================================
       HEX
       ===================================================== */

    if (kind == 10) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        int64_t value;

        if (!parse_signed(
                token,
                16,
                &value)) {

            return;
        }

        store_var(
            st,
            name,
            val_int(value)
        );

        return;
    }


    /* =====================================================
       UNSIGNED SHORT
       ===================================================== */

    if (kind == 11) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        uint64_t value;

        if (!parse_unsigned(
                token,
                10,
                &value)) {

            return;
        }

        if (value > USHRT_MAX)
            return;

        store_var(
            st,
            name,
            val_int((int64_t)value)
        );

        return;
    }


    /* =====================================================
       UNSIGNED LONG LONG
       ===================================================== */

    if (kind == 12) {

        char token[128];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_unsigned(
            st,
            name,
            token
        );

        return;
    }


    /* =====================================================
       GCCHAR / STRING
       ===================================================== */

    if (kind == 13) {

        char token[4096];

        if (!get_token(
                token,
                sizeof(token))) {

            return;
        }

        store_string(
            st,
            name,
            token
        );

        return;
    }
}