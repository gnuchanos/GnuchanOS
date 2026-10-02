/*
 * ss_parser.h — enough of the Python grammar to read the settings script, and
 * nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a screen saver: a call that passes named arguments.
 *
 *     gcl_SS.Effect(Name="pipe", PrimaryColor="#d400ff", IdleSeconds=300)
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements, calls with named arguments, and the values a person writes —
 * strings, numbers, flags, lists — and produces a list for the config module to
 * walk.
 *
 * This is the same parser GnuChanWM, GnuChanTerm and GnuChanRunner use, under
 * its own name. The four were written as one and split so that none has to
 * include another's config header; what is here knows nothing about screen
 * savers, and the module that gives the statements meaning knows nothing about
 * parsing.
 */
#ifndef GNUCHANSS_PARSER_H
#define GNUCHANSS_PARSER_H

#include "ss_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a screen saver's settings need, and both
   exist so a malformed file cannot make the parser allocate without end. */
#define SS_MAX_ARGS              32
#define SS_PARSER_MAX_STATEMENTS 128

typedef enum SsValueKind {
    SS_VALUE_STRING,   /* "pipe"             */
    SS_VALUE_NUMBER,   /* 300                */
    SS_VALUE_REAL,     /* 0.5                */
    SS_VALUE_BOOL,     /* True, False        */
    SS_VALUE_NAME,     /* a bare name        */
    SS_VALUE_LIST,     /* [a, b]             */
    SS_VALUE_CALL,     /* gcl_SS.Effect(...) */
} SsValueKind;

typedef struct SsStatement SsStatement;
typedef struct SsValue {
    SsValueKind kind;
    char text[SS_TEXT_LENGTH];
    int number;
    double real;
    int boolean;

    struct SsValue *items;   /* SS_VALUE_LIST owns its items */
    int item_count;
    SsStatement *call;       /* SS_VALUE_CALL */
} SsValue;

/* One named argument of a call. A positional argument has an empty name. */
typedef struct SsArgument {
    char name[SS_TEXT_LENGTH];
    SsValue value;
} SsArgument;

typedef enum SsStatementKind {
    SS_STMT_ASSIGN,   /* name = value                   */
    SS_STMT_CALL,     /* object.method(name=value, ...) */
} SsStatementKind;

struct SsStatement {
    SsStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. Left as written — deciding that
       "gcl_SS.Effect" is the effect call is the config module's job. */
    char target[SS_TEXT_LENGTH];

    SsValue value;                     /* SS_STMT_ASSIGN */
    SsArgument args[SS_MAX_ARGS];      /* SS_STMT_CALL  */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with ss_parse_free().
   Returns 0 on success, -1 when the file cannot be read or contains a
   statement this grammar does not accept. */
int ss_parse_file(const char *path, SsStatement **out, int *out_count);

void ss_parse_free(SsStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const SsValue *ss_argument(const SsStatement *statement, const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void ss_value_text(const SsValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int ss_value_number(const SsValue *value, int fallback);
int ss_value_bool(const SsValue *value, int fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. Only the first error is kept. */
const char *ss_parse_last_error(void);

#endif /* GNUCHANSS_PARSER_H */
