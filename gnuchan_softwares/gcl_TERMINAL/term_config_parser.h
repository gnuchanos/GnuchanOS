/*
 * term_config_parser.h — enough of the Python grammar to read the settings
 * script, and nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a terminal: a name given a value, and a call that passes
 * named arguments.
 *
 *     font = "monospace-11"
 *     gcl_Terminal.set_background("#09030d")
 *     gcl_Terminal.call(Font="monospace-11", BarRows=1)
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements of exactly two shapes — assignment and call — and produces a list
 * of them for the config module to walk.
 *
 * This is the same parser GnuChanWM uses, under its own name. The two were
 * written as one and split so that neither has to include the other's config
 * header; what is here knows nothing about terminals, and the module that
 * gives the statements meaning knows nothing about parsing.
 */
 
#ifndef GNUCHANTERM_CONFIG_PARSER_H
#define GNUCHANTERM_CONFIG_PARSER_H

#include "term_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a terminal's settings need, and both exist
   so that a malformed file cannot make the parser allocate without end. */
#define TERM_CONFIG_MAX_ARGS              32
#define TERM_CONFIG_PARSER_MAX_STATEMENTS 256

/* A plain C string buffer, because the parser has no business allocating one
   string at a time and a settings file holds a few hundred short values. The
   length is the same one the config uses for a written value. */
typedef enum TermValueKind {
    TERM_VALUE_STRING,   /* "monospace-11"            */
    TERM_VALUE_NUMBER,   /* 24                        */
    TERM_VALUE_REAL,     /* 0.5                       */
    TERM_VALUE_BOOL,     /* True, False               */
    TERM_VALUE_NAME,     /* super_key1 - a bare name  */
    TERM_VALUE_LIST,     /* [a, b]                    */
    TERM_VALUE_CALL,     /* gcl_Widgets.Clock(...)    */
} TermValueKind;

typedef struct TermStatement TermStatement;
typedef struct TermValue {
    TermValueKind kind;
    char text[TERM_CONFIG_TEXT_LENGTH];
    int number;
    /* The same number kept whole, for a value written with a fraction. */
    double real;
    int boolean;

    /* TERM_VALUE_LIST owns its items, recursively, because a list may hold
       lists of its own — which is how a palette is written:
       Colors=[["#000000", "#ffffff"], ...]. */
    struct TermValue *items;
    int item_count;
    TermStatement *call;
} TermValue;

/* One named argument of a call. A positional argument has an empty name, which
   is how the walker can tell "the first thing passed" from "the thing called
   Font". */
typedef struct TermArgument {
    char name[TERM_CONFIG_TEXT_LENGTH];
    TermValue value;
} TermArgument;

typedef enum TermStatementKind {
    TERM_STMT_ASSIGN,   /* name = value                     */
    TERM_STMT_CALL,     /* object.method(name=value, ...)   */
} TermStatementKind;

struct TermStatement {
    TermStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. It is left as written — the parser
       has no table of objects, so deciding that "gcl_Terminal.call" is the
       terminal is the config module's job, not this one's. */
    char target[TERM_CONFIG_TEXT_LENGTH];

    TermValue value;                        /* TERM_STMT_ASSIGN */
    TermArgument args[TERM_CONFIG_MAX_ARGS]; /* TERM_STMT_CALL  */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with
   term_config_statements_free(). Returns 0 on success, -1 when the file cannot
   be read or contains a statement this grammar does not accept. */
int term_config_parse_file(const char *path, TermStatement **out,
                           int *out_count);

/* The same, over text already in memory. */
int term_config_parse_text(const char *text, TermStatement **out,
                           int *out_count);

void term_config_statements_free(TermStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const TermValue *term_config_argument(const TermStatement *statement,
                                      const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void term_config_value_text(const TermValue *value, char *out,
                            unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int term_config_value_number(const TermValue *value, int fallback);
int term_config_value_bool(const TermValue *value, int fallback);
double term_config_value_real(const TermValue *value, double fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. The message names the statement that could not
   be read, which is what a person is shown when the settings script is saved
   with a mistake in it. Only the first error is kept — the rest are
   consequences of it. */
const char *term_config_last_error(void);

#endif /* GNUCHANTERM_CONFIG_PARSER_H */
