/*
 * dock_parser.h — enough of the Python grammar to read the settings script,
 * and nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a dock: a call that passes named arguments, and lists of
 * names and calls.
 *
 *     gcl_Dock.Main(Background="#1a0b2e", IconSize=48)
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements, calls with named arguments, and the values a person writes —
 * strings, numbers, flags, lists — and produces a list for the config module to
 * walk.
 *
 * This is the same parser GnuChanWM, GnuChanSS, GnuChanTerm, GnuChanRunner and
 * GnuChanFetch use, under its own name. They were written as one and split so
 * that none has to include another's config header; what is here knows nothing
 * about docks, and the module that gives the statements meaning knows nothing
 * about parsing.
 */
#ifndef GNUCHANDOCK_PARSER_H
#define GNUCHANDOCK_PARSER_H

#include "dock_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a dock's settings need, and both exist so a
   malformed file cannot make the parser allocate without end. */
#define DOCK_MAX_ARGS              32
#define DOCK_PARSER_MAX_STATEMENTS 128

typedef enum DockValueKind {
    DOCK_VALUE_STRING,   /* "#1a0b2e"           */
    DOCK_VALUE_NUMBER,   /* 48                  */
    DOCK_VALUE_REAL,     /* 0.5                 */
    DOCK_VALUE_BOOL,     /* True, False         */
    DOCK_VALUE_NAME,     /* a bare name         */
    DOCK_VALUE_LIST,     /* [a, b]              */
    DOCK_VALUE_CALL,     /* gcl_Item.Terminal() */
} DockValueKind;

typedef struct DockStatement DockStatement;
typedef struct DockValue {
    DockValueKind kind;
    char text[DOCK_TEXT_LENGTH];
    int number;
    double real;
    int boolean;

    struct DockValue *items;   /* DOCK_VALUE_LIST owns its items */
    int item_count;
    DockStatement *call;       /* DOCK_VALUE_CALL */
} DockValue;

/* One named argument of a call. A positional argument has an empty name. */
typedef struct DockArgument {
    char name[DOCK_TEXT_LENGTH];
    DockValue value;
} DockArgument;

typedef enum DockStatementKind {
    DOCK_STMT_ASSIGN,   /* name = value                   */
    DOCK_STMT_CALL,     /* object.method(name=value, ...) */
} DockStatementKind;

struct DockStatement {
    DockStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. Left as written — deciding that
       "gcl_Dock.Main" is the main call is the config module's job. */
    char target[DOCK_TEXT_LENGTH];

    DockValue value;                     /* DOCK_STMT_ASSIGN */
    DockArgument args[DOCK_MAX_ARGS];    /* DOCK_STMT_CALL  */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with dock_parse_free().
   Returns 0 on success, -1 when the file cannot be read or contains a
   statement this grammar does not accept. */
int dock_parse_file(const char *path, DockStatement **out, int *out_count);

void dock_parse_free(DockStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const DockValue *dock_argument(const DockStatement *statement,
                               const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void dock_value_text(const DockValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int dock_value_number(const DockValue *value, int fallback);
int dock_value_bool(const DockValue *value, int fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. Only the first error is kept. */
const char *dock_parse_last_error(void);

#endif /* GNUCHANDOCK_PARSER_H */
