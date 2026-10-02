/*
 * sl_parser.h — enough of the Python grammar to read the settings script, and
 * nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a lock screen: a call that passes named arguments.
 *
 *     gcl_SL.Lock(Background="#0d0512", Title="...", ClockFormat="%H:%M")
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements, calls with named arguments, and the values a person writes —
 * strings, numbers, flags, lists — and produces a list for the config module to
 * walk.
 *
 * This is the same parser GnuChanWM, GnuChanTerm, GnuChanRunner and GnuChanSS
 * use, under its own name. They were written as one and split so that none has
 * to include another's config header; what is here knows nothing about lock
 * screens, and the module that gives the statements meaning knows nothing about
 * parsing.
 */
#ifndef GNUCHANSL_PARSER_H
#define GNUCHANSL_PARSER_H

#include "sl_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a lock screen's settings need, and both
   exist so a malformed file cannot make the parser allocate without end. */
#define SL_MAX_ARGS              32
#define SL_PARSER_MAX_STATEMENTS 128

typedef enum SlValueKind {
    SL_VALUE_STRING,   /* "..."              */
    SL_VALUE_NUMBER,   /* 300                */
    SL_VALUE_REAL,     /* 0.5                */
    SL_VALUE_BOOL,     /* True, False        */
    SL_VALUE_NAME,     /* a bare name        */
    SL_VALUE_LIST,     /* [a, b]             */
    SL_VALUE_CALL,     /* gcl_SL.Lock(...)   */
} SlValueKind;

typedef struct SlStatement SlStatement;
typedef struct SlValue {
    SlValueKind kind;
    char text[SL_TEXT_LENGTH];
    int number;
    double real;
    int boolean;

    struct SlValue *items;   /* SL_VALUE_LIST owns its items */
    int item_count;
    SlStatement *call;       /* SL_VALUE_CALL */
} SlValue;

/* One named argument of a call. A positional argument has an empty name. */
typedef struct SlArgument {
    char name[SL_TEXT_LENGTH];
    SlValue value;
} SlArgument;

typedef enum SlStatementKind {
    SL_STMT_ASSIGN,   /* name = value                   */
    SL_STMT_CALL,     /* object.method(name=value, ...) */
} SlStatementKind;

struct SlStatement {
    SlStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. Left as written — deciding that
       "gcl_SL.Lock" is the lock call is the config module's job. */
    char target[SL_TEXT_LENGTH];

    SlValue value;                     /* SL_STMT_ASSIGN */
    SlArgument args[SL_MAX_ARGS];      /* SL_STMT_CALL  */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with sl_parse_free().
   Returns 0 on success, -1 when the file cannot be read or contains a
   statement this grammar does not accept. */
int sl_parse_file(const char *path, SlStatement **out, int *out_count);

void sl_parse_free(SlStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const SlValue *sl_argument(const SlStatement *statement, const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void sl_value_text(const SlValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int sl_value_number(const SlValue *value, int fallback);
int sl_value_bool(const SlValue *value, int fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. Only the first error is kept. */
const char *sl_parse_last_error(void);

#endif /* GNUCHANSL_PARSER_H */
