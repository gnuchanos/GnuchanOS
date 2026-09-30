/*
 * dm_config_parser.h — enough of the Python grammar to read the settings
 * script, and nothing more.
 *
 * The settings file is not a config format with Python in it; it is a Python
 * script, and the parts of the language it uses are the parts a person would
 * use to describe a login screen: a name given a value, and a call that passes
 * named arguments.
 *
 *     hostname = "gnuchan"
 *     gcl_DM.call(Accent="#c77dff", Margin=16)
 *
 * So this is not a Python interpreter and must not become one. It reads
 * statements of exactly two shapes — assignment and call — and produces a list
 * of them for the config module to walk.
 *
 * This is the same grammar GnuChanWM and GnuChanTerm read their scripts with,
 * under this program's own names. The three are not shared code: the programs
 * install separately and none of them may depend on another being present. It
 * is the same grammar because a desktop's settings should be written the same
 * way whichever program they belong to, not because the files are the same.
 */
#ifndef GNUCHANDM_CONFIG_PARSER_H
#define GNUCHANDM_CONFIG_PARSER_H

#include "dm_config.h"

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a login screen's settings need, and both
   exist so that a malformed file cannot make the parser allocate without
   end. */
#define DM_CONFIG_MAX_ARGS              32
#define DM_CONFIG_PARSER_MAX_STATEMENTS 256

typedef enum DmValueKind {
    DM_VALUE_STRING,   /* "#c77dff"                       */
    DM_VALUE_NUMBER,   /* 16                              */
    DM_VALUE_REAL,     /* 0.5                             */
    DM_VALUE_BOOL,     /* True, False                     */
    DM_VALUE_NAME,     /* a bare name                     */
    DM_VALUE_LIST,     /* [a, b]                          */
    DM_VALUE_CALL,     /* gcl_Widgets.Clock(...)          */
} DmValueKind;

typedef struct DmStatement DmStatement;
typedef struct DmValue {
    DmValueKind kind;
    char text[DM_CONFIG_TEXT_LENGTH];
    int number;
    /* The same number kept whole, for a value written with a fraction. */
    double real;
    int boolean;

    /* DM_VALUE_LIST owns its items, recursively, because a list may hold
       lists of its own. */
    struct DmValue *items;
    int item_count;
    DmStatement *call;
} DmValue;

/* One named argument of a call. A positional argument has an empty name, which
   is how the walker can tell "the first thing passed" from "the thing called
   Accent". */
typedef struct DmArgument {
    char name[DM_CONFIG_TEXT_LENGTH];
    DmValue value;
} DmArgument;

typedef enum DmStatementKind {
    DM_STMT_ASSIGN,   /* name = value                    */
    DM_STMT_CALL,     /* object.method(name=value, ...)  */
} DmStatementKind;

struct DmStatement {
    DmStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. It is left as written — the parser
       has no table of objects, so deciding that "gcl_DM.call" is the login
       screen is the config module's job, not this one's. */
    char target[DM_CONFIG_TEXT_LENGTH];

    DmValue value;                        /* DM_STMT_ASSIGN */
    DmArgument args[DM_CONFIG_MAX_ARGS];  /* DM_STMT_CALL   */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with
   dm_config_statements_free(). Returns 0 on success, -1 when the file cannot
   be read or contains a statement this grammar does not accept. */
int dm_config_parse_file(const char *path, DmStatement **out, int *out_count);

/* The same, over text already in memory. */
int dm_config_parse_text(const char *text, DmStatement **out, int *out_count);

void dm_config_statements_free(DmStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const DmValue *dm_config_argument(const DmStatement *statement,
                                  const char *name);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void dm_config_value_text(const DmValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int dm_config_value_number(const DmValue *value, int fallback);
int dm_config_value_bool(const DmValue *value, int fallback);
double dm_config_value_real(const DmValue *value, double fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. The message names the statement that could
   not be read, which is what a person is shown when the settings script is
   saved with a mistake in it. Only the first error is kept — the rest are
   consequences of it. */
const char *dm_config_last_error(void);

#endif /* GNUCHANDM_CONFIG_PARSER_H */
