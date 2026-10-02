/*
 * ss_parser.c — a small reader for the settings script.
 *
 * This is not a Python interpreter and must not become one. It understands the
 * two statement shapes a settings file uses — a name given a value, and a call
 * that passes named arguments — and the handful of value kinds those contain.
 * It produces a flat list of statements; ss_config.c is what gives them
 * meaning. The split is deliberate: only ss_config.c needs to know that
 * "gcl_SS.Effect" is the effect, and this file needs to know none of it.
 *
 * The reader walks the text directly rather than building a token list first.
 * The grammar is small enough that a token list would be more code than the
 * reader it would feed, and a setting is a few hundred bytes at most.
 *
 * On the first statement it cannot read it stops and remembers why — see
 * ss_parse_last_error() — because a half-read settings file is one nobody can
 * trust, and the read as a whole is refused rather than half applied.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ss_parser.h"

/* The message for the first statement a read refused, and whether the last read
   refused one. Kept as file-local state because there is one settings file and
   one read at a time. */
static char s_last_error[SS_TEXT_LENGTH * 2];
static int s_have_error = 0;

const char *ss_parse_last_error(void) {
    return s_have_error ? s_last_error : "";
}

typedef enum TokenKind {
    TOKEN_END,
    TOKEN_NAME,      /* an identifier, possibly dotted */
    TOKEN_STRING,    /* "..." */
    TOKEN_NUMBER,    /* 123, -4, 0.5 */
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_EQUALS,    /* = */
    TOKEN_LPAREN,    /* ( */
    TOKEN_RPAREN,    /* ) */
    TOKEN_LBRACKET,  /* [ */
    TOKEN_RBRACKET,  /* ] */
    TOKEN_COMMA,     /* , */
    TOKEN_NEWLINE,
    TOKEN_UNKNOWN,
} TokenKind;

typedef struct Parser {
    const char *text;
    size_t length;
    size_t position;

    TokenKind kind;
    char text_value[SS_TEXT_LENGTH];
    int number_value;
    double real_value;

    /* How many ( and [ are open. While it is above zero a newline is not the
       end of anything: a call laid out across many lines — every argument on
       its own line, which is how a settings file is written — is one statement
       like any other. See parser_next(). */
    int depth;

    int failed;
} Parser;

/* --- value freeing -------------------------------------------------------- */

static void free_value(SsValue *value);

static void free_statement(SsStatement *statement) {
    free_value(&statement->value);
    for (int i = 0; i < statement->arg_count; i++) {
        free_value(&statement->args[i].value);
    }
    statement->arg_count = 0;
}

static void free_value(SsValue *value) {
    if (value->kind == SS_VALUE_LIST && value->items) {
        for (int i = 0; i < value->item_count; i++) {
            free_value(&value->items[i]);
        }
        free(value->items);
        value->items = NULL;
        value->item_count = 0;
    } else if (value->kind == SS_VALUE_CALL && value->call) {
        free_statement(value->call);
        free(value->call);
        value->call = NULL;
    }
}

void ss_parse_free(SsStatement *statements, int count) {
    if (!statements) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free_statement(&statements[i]);
    }
    free(statements);
}

/* --- errors --------------------------------------------------------------- */

static void parser_error(Parser *parser, const char *message) {
    if (parser->failed) {
        return;
    }
    parser->failed = 1;
    if (!s_have_error) {
        s_have_error = 1;
        /* Where it happened is as useful as what happened: a line number turns
           "unexpected token" into a place to look. */
        int line = 1;
        for (size_t i = 0; i < parser->position && i < parser->length; i++) {
            if (parser->text[i] == '\n') {
                line++;
            }
        }
        snprintf(s_last_error, sizeof(s_last_error), "line %d: %s",
                 line, message);
    }
}

/* --- the tokeniser -------------------------------------------------------- */

static int is_name_start(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int is_name_char(int c) {
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '.';
}

static int is_digit(int c) {
    return c >= '0' && c <= '9';
}

/* Move past spaces and comments. A comment runs to the end of the line.
 *
 * A newline is skipped here only while a bracket is open: inside a call laid
 * out across lines — every argument on its own line, which is how a settings
 * file is written — the newline is not the end of anything. Between
 * statements depth is zero and the newline is kept, so the statement reader
 * still sees where one ends. */
static void skip_space(Parser *parser) {
    while (parser->position < parser->length) {
        char c = parser->text[parser->position];
        if (c == ' ' || c == '\t' || c == '\r') {
            parser->position++;
        } else if (c == '#') {
            while (parser->position < parser->length &&
                   parser->text[parser->position] != '\n') {
                parser->position++;
            }
        } else if (c == '\n' && parser->depth > 0) {
            parser->position++;
        } else {
            break;
        }
    }
}

static void parser_next(Parser *parser) {
    parser->text_value[0] = '\0';
    parser->number_value = 0;
    parser->real_value = 0.0;

    skip_space(parser);
    if (parser->position >= parser->length) {
        parser->kind = TOKEN_END;
        return;
    }

    char c = parser->text[parser->position];

    if (c == '\n') {
        parser->position++;
        parser->kind = TOKEN_NEWLINE;
        return;
    }

    if (is_name_start(c)) {
        size_t start = parser->position;
        while (parser->position < parser->length &&
               is_name_char(parser->text[parser->position])) {
            parser->position++;
        }
        size_t length = parser->position - start;
        if (length >= sizeof(parser->text_value)) {
            length = sizeof(parser->text_value) - 1;
        }
        memcpy(parser->text_value, parser->text + start, length);
        parser->text_value[length] = '\0';

        if (strcmp(parser->text_value, "True") == 0) {
            parser->kind = TOKEN_TRUE;
        } else if (strcmp(parser->text_value, "False") == 0) {
            parser->kind = TOKEN_FALSE;
        } else {
            parser->kind = TOKEN_NAME;
        }
        return;
    }

    if (c == '"' || c == '\'') {
        char quote = c;
        parser->position++;
        size_t out = 0;
        while (parser->position < parser->length &&
               parser->text[parser->position] != quote) {
            char ch = parser->text[parser->position];
            if (ch == '\\' && parser->position + 1 < parser->length) {
                parser->position++;
                ch = parser->text[parser->position];
                if (ch == 'n') {
                    ch = '\n';
                } else if (ch == 't') {
                    ch = '\t';
                }
            }
            if (out + 1 < sizeof(parser->text_value)) {
                parser->text_value[out++] = ch;
            }
            parser->position++;
        }
        if (parser->position >= parser->length) {
            parser_error(parser, "a string was not closed before the end of "
                                 "the file");
            parser->kind = TOKEN_UNKNOWN;
            return;
        }
        parser->position++;   /* closing quote */
        parser->text_value[out] = '\0';
        parser->kind = TOKEN_STRING;
        return;
    }

    if (is_digit(c) || (c == '-' && parser->position + 1 < parser->length &&
                        is_digit(parser->text[parser->position + 1]))) {
        size_t start = parser->position;
        if (parser->text[parser->position] == '-') {
            parser->position++;
        }
        while (parser->position < parser->length &&
               is_digit(parser->text[parser->position])) {
            parser->position++;
        }
        if (parser->position < parser->length &&
            parser->text[parser->position] == '.') {
            parser->position++;
            while (parser->position < parser->length &&
                   is_digit(parser->text[parser->position])) {
                parser->position++;
            }
        }
        size_t length = parser->position - start;
        if (length >= sizeof(parser->text_value)) {
            length = sizeof(parser->text_value) - 1;
        }
        memcpy(parser->text_value, parser->text + start, length);
        parser->text_value[length] = '\0';
        parser->number_value = atoi(parser->text_value);
        parser->real_value = atof(parser->text_value);
        parser->kind = TOKEN_NUMBER;
        return;
    }

    parser->position++;
    switch (c) {
    /* The brackets set the depth skip_space() counts, so it knows whether a
       newline is the end of a statement or just how a call was laid out. */
    case '=': parser->kind = TOKEN_EQUALS;   return;
    case '(': parser->depth++; parser->kind = TOKEN_LPAREN;   return;
    case ')': if (parser->depth > 0) parser->depth--;
              parser->kind = TOKEN_RPAREN;   return;
    case '[': parser->depth++; parser->kind = TOKEN_LBRACKET; return;
    case ']': if (parser->depth > 0) parser->depth--;
              parser->kind = TOKEN_RBRACKET; return;
    case ',': parser->kind = TOKEN_COMMA;    return;
    default:
        parser_error(parser, "a character that is not part of the settings "
                             "grammar");
        parser->kind = TOKEN_UNKNOWN;
        return;
    }
}

/* --- the grammar ---------------------------------------------------------- */

static int parse_value(Parser *parser, SsValue *value);
static int parse_call_body(Parser *parser, SsStatement *statement);

/* A list: [a, b, c], possibly nested. */
static int parse_list(Parser *parser, SsValue *value) {
    value->kind = SS_VALUE_LIST;
    value->items = NULL;
    value->item_count = 0;

    parser_next(parser);   /* past [ */
    if (parser->kind == TOKEN_RBRACKET) {
        parser_next(parser);
        return 1;
    }

    int capacity = 0;
    for (;;) {
        if (value->item_count == capacity) {
            int grown = capacity ? capacity * 2 : 4;
            SsValue *items = realloc(value->items,
                                     sizeof(SsValue) * (size_t)grown);
            if (!items) {
                parser_error(parser, "out of memory");
                return 0;
            }
            value->items = items;
            capacity = grown;
        }
        SsValue *item = &value->items[value->item_count];
        memset(item, 0, sizeof(*item));
        if (!parse_value(parser, item)) {
            return 0;
        }
        value->item_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_next(parser);
            /* A comma right before the ] is a trailing comma: a list written
               one item per line ends with one, and it is not an error. */
            if (parser->kind == TOKEN_RBRACKET) {
                break;
            }
            continue;
        }
        break;
    }

    if (parser->kind != TOKEN_RBRACKET) {
        parser_error(parser, "a list was not closed with ]");
        return 0;
    }
    parser_next(parser);
    return 1;
}

static int parse_value(Parser *parser, SsValue *value) {
    memset(value, 0, sizeof(*value));
    switch (parser->kind) {
    case TOKEN_STRING:
        value->kind = SS_VALUE_STRING;
        snprintf(value->text, sizeof(value->text), "%s", parser->text_value);
        parser_next(parser);
        return 1;
    case TOKEN_NUMBER:
        value->number = parser->number_value;
        value->real = parser->real_value;
        /* A fraction is a real; a whole number is an int. The dot in the
           written text is what tells them apart. */
        value->kind = (strchr(parser->text_value, '.') != NULL)
                          ? SS_VALUE_REAL : SS_VALUE_NUMBER;
        snprintf(value->text, sizeof(value->text), "%s", parser->text_value);
        parser_next(parser);
        return 1;
    case TOKEN_TRUE:
        value->kind = SS_VALUE_BOOL;
        value->boolean = 1;
        parser_next(parser);
        return 1;
    case TOKEN_FALSE:
        value->kind = SS_VALUE_BOOL;
        value->boolean = 0;
        parser_next(parser);
        return 1;
    case TOKEN_NAME: {
        char name[SS_TEXT_LENGTH];
        snprintf(name, sizeof(name), "%s", parser->text_value);
        parser_next(parser);
        if (parser->kind == TOKEN_LPAREN) {
            /* A name followed by ( is a call: obj.method(args...) or a bare
               function(args...). */
            value->kind = SS_VALUE_CALL;
            value->call = calloc(1, sizeof(SsStatement));
            if (!value->call) {
                parser_error(parser, "out of memory");
                return 0;
            }
            snprintf(value->call->target, sizeof(value->call->target), "%s",
                     name);
            value->call->kind = SS_STMT_CALL;
            return parse_call_body(parser, value->call);
        }
        value->kind = SS_VALUE_NAME;
        snprintf(value->text, sizeof(value->text), "%s", name);
        return 1;
    }
    case TOKEN_LBRACKET:
        return parse_list(parser, value);
    default:
        parser_error(parser, "a value was expected");
        return 0;
    }
}

/* One argument of a call: name=value, or a bare positional value. */
static int parse_argument(Parser *parser, SsArgument *argument) {
    memset(argument, 0, sizeof(*argument));

    /* A named argument is NAME = value. The lookahead is one token: read the
       name, and if the next token is = it was a name; otherwise it was the
       first token of a positional value. */
    if (parser->kind == TOKEN_NAME) {
        char saved[SS_TEXT_LENGTH];
        snprintf(saved, sizeof(saved), "%s", parser->text_value);
        parser_next(parser);
        if (parser->kind == TOKEN_EQUALS) {
            snprintf(argument->name, sizeof(argument->name), "%s", saved);
            parser_next(parser);
            return parse_value(parser, &argument->value);
        }
        /* Not a name after all: the name becomes the value, and what follows
           decides whether that value is done. */
        if (parser->kind == TOKEN_LPAREN) {
            /* It was a call: name(args...). */
            argument->value.kind = SS_VALUE_CALL;
            argument->value.call = calloc(1, sizeof(SsStatement));
            if (!argument->value.call) {
                parser_error(parser, "out of memory");
                return 0;
            }
            snprintf(argument->value.call->target,
                     sizeof(argument->value.call->target), "%s", saved);
            argument->value.call->kind = SS_STMT_CALL;
            return parse_call_body(parser, argument->value.call);
        }
        argument->value.kind = SS_VALUE_NAME;
        snprintf(argument->value.text, sizeof(argument->value.text), "%s",
                 saved);
        return 1;
    }

    return parse_value(parser, &argument->value);
}

/* The body of a call, after the target and before the closing ). */
static int parse_call_body(Parser *parser, SsStatement *statement) {
    parser_next(parser);   /* step past ( */

    if (parser->kind == TOKEN_RPAREN) {
        parser_next(parser);
        return 1;
    }

    for (;;) {
        if (statement->arg_count >= SS_MAX_ARGS) {
            parser_error(parser, "a call passed more arguments than are read");
            return 0;
        }
        SsArgument *argument = &statement->args[statement->arg_count];
        if (!parse_argument(parser, argument)) {
            return 0;
        }
        statement->arg_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_next(parser);
            /* A comma right before the ) is a trailing comma. Every settings
               file is written one argument per line and ends the call with
               one, so refusing it would refuse the shape the file is in. */
            if (parser->kind == TOKEN_RPAREN) {
                break;
            }
            continue;
        }
        break;
    }

    if (parser->kind != TOKEN_RPAREN) {
        parser_error(parser, "a call was not closed with )");
        return 0;
    }
    parser_next(parser);
    return 1;
}

/* One statement: an assignment (NAME = value) or a call (obj.method(args)).
   Returns 1 when one was read, 0 at the end of the file, -1 on a mistake. */
static int parse_statement(Parser *parser, SsStatement *statement) {
    memset(statement, 0, sizeof(*statement));

    /* Newlines between statements carry no meaning; step over any. */
    for (;;) {
        skip_space(parser);
        if (parser->position < parser->length &&
            parser->text[parser->position] == '\n') {
            parser->position++;
            continue;
        }
        break;
    }
    if (parser->position >= parser->length) {
        parser->kind = TOKEN_END;
        return 0;
    }

    parser_next(parser);
    if (parser->kind != TOKEN_NAME) {
        parser_error(parser, "a statement was expected");
        return -1;
    }
    snprintf(statement->target, sizeof(statement->target), "%s",
             parser->text_value);

    parser_next(parser);
    if (parser->kind == TOKEN_EQUALS) {
        statement->kind = SS_STMT_ASSIGN;
        parser_next(parser);
        if (!parse_value(parser, &statement->value)) {
            return -1;
        }
    } else if (parser->kind == TOKEN_LPAREN) {
        statement->kind = SS_STMT_CALL;
        if (!parse_call_body(parser, statement)) {
            return -1;
        }
    } else {
        parser_error(parser, "a statement was not = or ( after its name");
        return -1;
    }

    /* The end of a statement is a newline or the end of the file. */
    while (parser->kind == TOKEN_NEWLINE) {
        parser_next(parser);
    }
    return 1;
}

int ss_parse_file(const char *path, SsStatement **out, int *out_count) {
    *out = NULL;
    *out_count = 0;
    s_have_error = 0;
    s_last_error[0] = '\0';

    if (!path || !path[0]) {
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error), "no settings file");
        return -1;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error),
                 "could not read %s", path);
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error), "could not read %s", path);
        return -1;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error), "could not read %s", path);
        return -1;
    }
    rewind(file);

    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(file);
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error), "out of memory");
        return -1;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    text[read] = '\0';
    fclose(file);

    SsStatement *statements = calloc(SS_PARSER_MAX_STATEMENTS,
                                     sizeof(SsStatement));
    if (!statements) {
        free(text);
        s_have_error = 1;
        snprintf(s_last_error, sizeof(s_last_error), "out of memory");
        return -1;
    }

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.text = text;
    parser.length = read;
    parser.position = 0;
    parser.kind = TOKEN_END;

    int count = 0;
    while (count < SS_PARSER_MAX_STATEMENTS) {
        int result = parse_statement(&parser, &statements[count]);
        if (result == 0) {
            break;              /* the end of the file */
        }
        if (result < 0) {
            ss_parse_free(statements, count);
            free(text);
            return -1;
        }
        count++;
    }

    free(text);
    if (count == 0) {
        free(statements);
        *out = NULL;
        *out_count = 0;
        return 0;
    }
    *out = statements;
    *out_count = count;
    return 0;
}

/* --- reading values ------------------------------------------------------- */

const SsValue *ss_argument(const SsStatement *statement, const char *name) {
    if (!statement || statement->kind != SS_STMT_CALL) {
        return NULL;
    }
    for (int i = 0; i < statement->arg_count; i++) {
        if (strcmp(statement->args[i].name, name) == 0) {
            return &statement->args[i].value;
        }
    }
    return NULL;
}

void ss_value_text(const SsValue *value, char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    switch (value->kind) {
    case SS_VALUE_STRING:
    case SS_VALUE_NAME:
        snprintf(out, size, "%s", value->text);
        break;
    case SS_VALUE_NUMBER:
        snprintf(out, size, "%d", value->number);
        break;
    case SS_VALUE_REAL:
        snprintf(out, size, "%g", value->real);
        break;
    case SS_VALUE_BOOL:
        snprintf(out, size, "%s", value->boolean ? "True" : "False");
        break;
    default:
        out[0] = '\0';
        break;
    }
}

int ss_value_number(const SsValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == SS_VALUE_NUMBER) {
        return value->number;
    }
    if (value->kind == SS_VALUE_REAL) {
        return (int)value->real;
    }
    if (value->kind == SS_VALUE_BOOL) {
        return value->boolean;
    }
    /* A number written as a string — the script wrote "300" in quotes — is
       still a number to a person reading it, so it is read as one. */
    if ((value->kind == SS_VALUE_STRING || value->kind == SS_VALUE_NAME) &&
        value->text[0]) {
        char *end = NULL;
        long parsed = strtol(value->text, &end, 10);
        if (end && *end == '\0') {
            return (int)parsed;
        }
    }
    return fallback;
}

int ss_value_bool(const SsValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == SS_VALUE_BOOL) {
        return value->boolean;
    }
    if (value->kind == SS_VALUE_NUMBER) {
        return value->number != 0;
    }
    if (value->kind == SS_VALUE_NAME) {
        if (strcmp(value->text, "True") == 0 || strcmp(value->text, "true") == 0) {
            return 1;
        }
        if (strcmp(value->text, "False") == 0 || strcmp(value->text, "false") == 0) {
            return 0;
        }
    }
    return fallback;
}
