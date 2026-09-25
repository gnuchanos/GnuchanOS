/*
 * wm_config_parser.h — enough of the Python grammar to read the settings
 * script, and nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a desktop: a name given a value, and a call that passes
 * named arguments.
 *
 *     default_terminal = "xterm"
 *     gcl_BAR.call(Position="top", Size=24)
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements of exactly two shapes — assignment and call — and produces a
 * list of them for the config module to walk.
 *
 * The parser is deliberately separate from the module that gives the
 * statements meaning. Reading the file and understanding it are two
 * different jobs, and only the second one knows what gcl_BAR is.
 */
#ifndef GNUCHANWM_CONFIG_PARSER_H
#define GNUCHANWM_CONFIG_PARSER_H

#include "wm_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a desktop script needs, and both exist so
   that a malformed file cannot make the parser allocate without end. */
#define WM_CONFIG_MAX_ARGS              32
#define WM_CONFIG_PARSER_MAX_STATEMENTS 512

typedef enum WmValueKind {
    WM_VALUE_STRING,   /* "xterm"                                         */
    WM_VALUE_NUMBER,   /* 24                                              */
    WM_VALUE_BOOL,     /* True, False                                     */
    WM_VALUE_NAME,     /* default_terminal - a name, resolved by the user */
    WM_VALUE_LIST,     /* [super_key1, "return"]                          */
    WM_VALUE_CALL,
} WmValueKind;

typedef struct WmStatement WmStatement;
typedef struct WmValue {
    WmValueKind kind;
    char text[WM_CONFIG_TEXT_LENGTH];
    int number;
    int boolean;

    /* WM_VALUE_LIST owns its items; wm_config_statements_free() releases
       them, recursively, because a list may hold lists of its own. */
    struct WmValue *items;
    int item_count;
    WmStatement *call;
} WmValue;

/* One named argument of a call. A positional argument has an empty name,
   which is how the walker can tell "the first thing passed" from "the thing
   called Position". */
typedef struct WmArgument {
    char name[WM_CONFIG_TEXT_LENGTH];
    WmValue value;
} WmArgument;

typedef enum WmStatementKind {
    WM_STMT_ASSIGN,   /* name = value                   */
    WM_STMT_CALL,     /* object.method(name=value, ...) */
} WmStatementKind;

struct WmStatement {
    WmStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. It is left as written - the parser
       has no table of objects, so deciding that "gcl_BAR.call" is the bar is
       the config module's job, not this one's. */
    char target[WM_CONFIG_TEXT_LENGTH];

    WmValue value;                        /* WM_STMT_ASSIGN */
    WmArgument args[WM_CONFIG_MAX_ARGS];  /* WM_STMT_CALL   */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with
   wm_config_statements_free(). Returns 0 on success, -1 when the file cannot
   be read or contains a statement this grammar does not accept. */
int wm_config_parse_file(const char *path, WmStatement **out, int *out_count);

/* The same, over text already in memory. */
int wm_config_parse_text(const char *text, WmStatement **out, int *out_count);

void wm_config_statements_free(WmStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const WmValue *wm_config_argument(const WmStatement *statement,
                                  const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void wm_config_value_text(const WmValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int wm_config_value_number(const WmValue *value, int fallback);
int wm_config_value_bool(const WmValue *value, int fallback);

#endif /* GNUCHANWM_CONFIG_PARSER_H */
