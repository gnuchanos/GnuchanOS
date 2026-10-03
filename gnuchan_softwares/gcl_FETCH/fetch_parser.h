/*
 * fetch_parser.h — enough of the Python grammar to read the settings script,
 * and nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would use
 * to describe a fetch program: a call that passes named arguments, and lists of
 * names.
 *
 *     gcl_Fetch.Main(Image="~/.config/GnuChanFetch/logo.png",
 *                    Fields=["os", "kernel", "cpu"])
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements, calls with named arguments, and the values a person writes —
 * strings, numbers, flags, lists — and produces a list for the config module to
 * walk.
 *
 * This is the same parser GnuChanWM, GnuChanSS, GnuChanTerm and GnuChanRunner
 * use, under its own name. They were written as one and split so that none has
 * to include another's config header; what is here knows nothing about fetch
 * programs, and the module that gives the statements meaning knows nothing
 * about parsing.
 */
#ifndef GNUCHANFETCH_PARSER_H
#define GNUCHANFETCH_PARSER_H

#include "fetch_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a fetch program's settings need, and both
   exist so a malformed file cannot make the parser allocate without end. */
#define FETCH_MAX_ARGS              32
#define FETCH_PARSER_MAX_STATEMENTS 128

typedef enum FetchValueKind {
    FETCH_VALUE_STRING,   /* "pipe"             */
    FETCH_VALUE_NUMBER,   /* 300                */
    FETCH_VALUE_REAL,     /* 0.5                */
    FETCH_VALUE_BOOL,     /* True, False        */
    FETCH_VALUE_NAME,     /* a bare name        */
    FETCH_VALUE_LIST,     /* [a, b]             */
    FETCH_VALUE_CALL,     /* gcl_Fetch.Main(...) */
} FetchValueKind;

typedef struct FetchStatement FetchStatement;
typedef struct FetchValue {
    FetchValueKind kind;
    char text[FETCH_TEXT_LENGTH];
    int number;
    double real;
    int boolean;

    struct FetchValue *items;   /* FETCH_VALUE_LIST owns its items */
    int item_count;
    FetchStatement *call;       /* FETCH_VALUE_CALL */
} FetchValue;

/* One named argument of a call. A positional argument has an empty name. */
typedef struct FetchArgument {
    char name[FETCH_TEXT_LENGTH];
    FetchValue value;
} FetchArgument;

typedef enum FetchStatementKind {
    FETCH_STMT_ASSIGN,   /* name = value                   */
    FETCH_STMT_CALL,     /* object.method(name=value, ...) */
} FetchStatementKind;

struct FetchStatement {
    FetchStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. Left as written — deciding that
       "gcl_Fetch.Main" is the main call is the config module's job. */
    char target[FETCH_TEXT_LENGTH];

    FetchValue value;                     /* FETCH_STMT_ASSIGN */
    FetchArgument args[FETCH_MAX_ARGS];   /* FETCH_STMT_CALL  */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with fetch_parse_free().
   Returns 0 on success, -1 when the file cannot be read or contains a
   statement this grammar does not accept. */
int fetch_parse_file(const char *path, FetchStatement **out, int *out_count);

void fetch_parse_free(FetchStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const FetchValue *fetch_argument(const FetchStatement *statement,
                                 const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void fetch_value_text(const FetchValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int fetch_value_number(const FetchValue *value, int fallback);
int fetch_value_bool(const FetchValue *value, int fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. Only the first error is kept. */
const char *fetch_parse_last_error(void);

#endif /* GNUCHANFETCH_PARSER_H */
