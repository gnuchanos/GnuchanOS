/*
 * runner_parser.h — enough of the Python grammar to read the settings file,
 * and nothing more.
 *
 * GnuChanRunner is configured by a script, the way GnuChanWM is, and for the
 * same reason: a person who can move a bar can move a window. The file is not
 * a config format with Python in it; it is the smallest part of Python that a
 * person would use to describe a launcher, and this reads exactly that.
 *
 *     Prompt = "Run:"                                  a name given a value
 *     GnuChanRunner.add_program(name="x", command="y") a call, named
 *
 * So this is not an interpreter and must not become one. Two statement shapes
 * are read — assignment and call — and anything else is a mistake the file is
 * told about rather than something quietly skipped.
 *
 * It is a copy of GnuChanWM's own reader in spirit rather than in code:
 * GnuChanRunner installs on its own and must build on its own, so it carries
 * the part of that grammar it needs rather than linking the window manager.
 */
#ifndef GNUCHANRUNNER_PARSER_H
#define GNUCHANRUNNER_PARSER_H

/* A written value: a colour, a command, a font name. */
#define RUNNER_TEXT_LENGTH 256

/* The most arguments one call may pass, and the most statements one file may
   contain. Both are far above what a launcher's script needs, and both exist
   so a malformed file cannot make the parser allocate without end. */
#define RUNNER_MAX_ARGS       32
#define RUNNER_MAX_STATEMENTS 256

/* The most items one list may hold — the directories a script names, and
   nothing longer. */
#define RUNNER_MAX_LIST_ITEMS 16

typedef enum RunnerValueKind {
    RUNNER_VALUE_STRING,   /* "xterm"                                  */
    RUNNER_VALUE_NUMBER,   /* 12                                       */
    RUNNER_VALUE_BOOL,     /* True, False                              */
    RUNNER_VALUE_NAME,     /* a bare name, kept as written             */
    RUNNER_VALUE_LIST,     /* ["/usr/share/applications"]              */
    RUNNER_VALUE_CALL,     /* a call used as a value, kept whole       */
} RunnerValueKind;

/* Forward, because a list may hold calls and a call holds values: the two
   types are each other's members and cannot be declared one inside the
   other. */
typedef struct RunnerStatement RunnerStatement;

typedef struct RunnerValue {
    RunnerValueKind kind;
    char text[RUNNER_TEXT_LENGTH];
    int number;
    int boolean;

    /* RUNNER_VALUE_LIST owns its items; runner_parse_free() releases them,
       recursively, because a list may hold lists of its own. */
    struct RunnerValue *items;
    int item_count;

    /* RUNNER_VALUE_CALL: the call itself, so a call written inside a list is
       kept whole rather than flattened. NULL for every other kind. */
    RunnerStatement *call;
} RunnerValue;

/* One named argument of a call. A positional argument has an empty name,
   which is how the walker can tell "the first thing passed" from "the thing
   called Position". */
typedef struct RunnerArgument {
    char name[RUNNER_TEXT_LENGTH];
    RunnerValue value;
} RunnerArgument;

typedef enum RunnerStatementKind {
    RUNNER_STMT_ASSIGN,   /* name = value                   */
    RUNNER_STMT_CALL,     /* object.method(value, name=...) */
} RunnerStatementKind;

struct RunnerStatement {
    RunnerStatementKind kind;

    /* The whole name on the left: the variable for an assignment, and the
       qualified object.method for a call. It is left as written — the parser
       has no table of objects, so deciding that "GnuChanRunner.add_program"
       is a program is the config module's job, not this one's. */
    char target[RUNNER_TEXT_LENGTH];

    RunnerValue value;                       /* RUNNER_STMT_ASSIGN */
    RunnerArgument args[RUNNER_MAX_ARGS];    /* RUNNER_STMT_CALL   */
    int arg_count;
};

/* Parse the script at path. On success *out points at a newly allocated list
   of *out_count statements, which the caller frees with runner_parse_free().
   Returns 0 on success, -1 when the file cannot be read or contains a
   statement this grammar does not accept. */
int runner_parse_file(const char *path, RunnerStatement **out, int *out_count);

void runner_parse_free(RunnerStatement *statements, int count);

/* The argument called name, or NULL when the call did not pass it. */
const RunnerValue *runner_argument(const RunnerStatement *statement,
                                   const char *name);

/* The first positional argument, or NULL when the call passed none. */
const RunnerValue *runner_argument_first(const RunnerStatement *statement);

/* A value as text: a string is itself, a number is written out, a name is the
   name, and anything else leaves out empty. */
void runner_value_text(const RunnerValue *value, char *out, unsigned int size);

/* A value as a number or a flag, with the answer to use when the value is
   missing or is not of that kind. */
int runner_value_number(const RunnerValue *value, int fallback);
int runner_value_bool(const RunnerValue *value, int fallback);

/* The message for the first statement the last read refused, or an empty
   string when that read parsed. Reading the file and understanding it are two
   jobs, and this is how a caller that only learns "the file did not parse"
   says why: the message names the statement that could not be read, which is
   what a person is shown when the settings script is saved with a mistake in
   it. Only the first error is kept — the rest are consequences of it. */
const char *runner_parse_last_error(void);

#endif /* GNUCHANRUNNER_PARSER_H */
