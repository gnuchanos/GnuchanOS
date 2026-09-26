/*
 * wm_config_parser.c — read the settings script into statements.
 *
 * The tokeniser is the whole of the difficulty, and it is small on purpose.
 * It has to know two things Python knows and a naive read would get wrong:
 *
 *   - a '#' begins a comment, but only outside a string, so a colour written
 *     as "#27022b" survives;
 *   - whitespace — newlines included — separates tokens and nothing more.
 *
 * Everything after that is a recursive descent over the tokens. A statement
 * is a name, then either '=' and a value or '(' and a list of arguments. A
 * value may itself be a call, because that is how a bar's widgets are
 * written:
 *
 *     gcl_BAR.call(Widgets=[gcl_Widgets.Clock(format="%H:%M")])
 *
 * Anything else — an `if`, a `for`, a function definition — is rejected: a
 * script that computes its own desktop cannot be known without running it.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_config_parser.h"

/* --- tokens --------------------------------------------------------------- */

typedef enum TokenKind {
    TOKEN_END,
    TOKEN_NAME,      /* super_key1, gcl_BAR, Position */
    TOKEN_STRING,    /* "xterm"                       */
    TOKEN_NUMBER,    /* 24, -3                        */
    TOKEN_ASSIGN,    /* =                             */
    TOKEN_DOT,       /* .                             */
    TOKEN_COMMA,     /* ,                             */
    TOKEN_OPEN,      /* (  or  [                      */
    TOKEN_CLOSE,     /* )  or  ]                      */
} TokenKind;

typedef struct Parser {
    const char *cursor;
    TokenKind kind;
    char text[WM_CONFIG_TEXT_LENGTH];
} Parser;

static void free_value(WmValue *value);

/* The first message of the last read. A caller that only learns "the file did
   not parse" needs the reason as well as the fact, and a config saved with a
   mistake has to say which statement could not be read — otherwise the person
   who saved it has one line in a log and a desktop that did not change. It is
   a plain buffer and not allocated storage: the parser reads one file at a
   time, and the message lives until the next read. */
static char last_error[WM_CONFIG_TEXT_LENGTH];

const char *wm_config_last_error(void) {
    return last_error;
}

static void parser_error(const Parser *parser, const char *message) {
    /* Only the first error is kept. Everything after it is a consequence —
       one unclosed bracket makes the rest of the file nonsense — so reporting
       the first is reporting the mistake rather than its echoes. */
    if (last_error[0] == '\0') {
        snprintf(last_error, sizeof(last_error), "%s near '%.24s'", message,
                 parser->cursor ? parser->cursor : "");
    }
    fprintf(stderr, "gnuchanwm: config: %s near '%.24s'\n", message,
            parser->cursor ? parser->cursor : "");
}

static int is_name_start(int c) {
    return isalpha(c) || c == '_';
}

static int is_name_char(int c) {
    return isalnum(c) || c == '_';
}

static void parser_advance(Parser *parser) {
    const char *c = parser->cursor;
    parser->text[0] = '\0';

    for (;;) {
        while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') {
            c++;
        }
        if (*c == '#') {
            while (*c && *c != '\n') {
                c++;
            }
            continue;
        }
        break;
    }

    if (*c == '\0') {
        parser->kind = TOKEN_END;
        parser->cursor = c;
        return;
    }

    if (is_name_start((unsigned char)*c)) {
        unsigned int i = 0;
        while (is_name_char((unsigned char)*c)) {
            if (i + 1 < sizeof(parser->text)) {
                parser->text[i++] = *c;
            }
            c++;
        }
        parser->text[i] = '\0';
        parser->kind = TOKEN_NAME;
        parser->cursor = c;
        return;
    }

    if (*c == '"' || *c == '\'') {
        char quote = *c++;
        unsigned int i = 0;
        while (*c && *c != quote) {
            if (*c == '\\' && c[1]) {
                c++;
            }
            if (i + 1 < sizeof(parser->text)) {
                parser->text[i++] = *c;
            }
            c++;
        }
        parser->text[i] = '\0';
        if (*c == quote) {
            c++;
        } else {
            parser_error(parser, "a string is not closed");
        }
        parser->kind = TOKEN_STRING;
        parser->cursor = c;
        return;
    }

    if (isdigit((unsigned char)*c) || *c == '-') {
        unsigned int i = 0;
        if (*c == '-') {
            parser->text[i++] = *c++;
        }
        while (isdigit((unsigned char)*c)) {
            if (i + 1 < sizeof(parser->text)) {
                parser->text[i++] = *c;
            }
            c++;
        }
        parser->text[i] = '\0';
        parser->kind = TOKEN_NUMBER;
        parser->cursor = c;
        return;
    }

    parser->cursor = c + 1;
    switch (*c) {
    case '=': parser->kind = TOKEN_ASSIGN; return;
    case '.': parser->kind = TOKEN_DOT;    return;
    case ',': parser->kind = TOKEN_COMMA;  return;
    case '(':
    case '[': parser->kind = TOKEN_OPEN;   return;
    case ')':
    case ']': parser->kind = TOKEN_CLOSE;  return;
    default:
        parser_error(parser, "a character that means nothing here");
        parser->kind = TOKEN_END;
        return;
    }
}

/* --- values and calls ----------------------------------------------------- */

static int parse_value(Parser *parser, WmValue *value);
static int parse_call_body(Parser *parser, WmStatement *statement);

/* A dotted name: "gcl_Widgets.Clock" or "super_key1". The tokens are already
   being read; this joins the parts with dots. */
static int parse_dotted_name(Parser *parser, char *out, unsigned int size) {
    unsigned int length = 0;
    for (;;) {
        unsigned int i = 0;
        while (parser->text[i]) {
            if (length + 1 < size) {
                out[length++] = parser->text[i];
            }
            i++;
        }
        parser_advance(parser);
        if (parser->kind == TOKEN_DOT) {
            if (length + 1 < size) {
                out[length++] = '.';
            }
            parser_advance(parser);
            if (parser->kind != TOKEN_NAME) {
                parser_error(parser, "a name is missing after the dot");
                return -1;
            }
            continue;
        }
        break;
    }
    out[length] = '\0';
    return 0;
}

static int parse_list(Parser *parser, WmValue *value) {
    value->kind = WM_VALUE_LIST;
    value->items = NULL;
    value->item_count = 0;

    parser_advance(parser);   /* past '[' */
    if (parser->kind == TOKEN_CLOSE) {
        parser_advance(parser);
        return 0;
    }

    int capacity = 0;
    for (;;) {
        if (value->item_count == capacity) {
            int grown = capacity ? capacity * 2 : 8;
            WmValue *items = realloc(value->items,
                                     (size_t)grown * sizeof(WmValue));
            if (!items) {
                return -1;
            }
            value->items = items;
            capacity = grown;
        }
        if (parse_value(parser, &value->items[value->item_count]) != 0) {
            return -1;
        }
        value->item_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_advance(parser);
            if (parser->kind == TOKEN_CLOSE) { break; }
            continue;
        }
        break;
    }

    if (parser->kind != TOKEN_CLOSE) {
        parser_error(parser, "a list is not closed");
        return -1;
    }
    parser_advance(parser);
    return 0;
}

/* A call where a value may stand: "gcl_Widgets.Clock(...)". The name has not
   been read yet; this reads it, then the arguments, and stores the whole call
   in value->call. A name with no call after it is kept as a plain name. */
static int parse_call_value(Parser *parser, WmValue *value) {
    WmStatement *statement = calloc(1, sizeof(WmStatement));
    if (!statement) {
        return -1;
    }
    statement->kind = WM_STMT_CALL;

    if (parse_dotted_name(parser, statement->target,
                          sizeof(statement->target)) != 0) {
        free(statement);
        return -1;
    }
    if (parser->kind != TOKEN_OPEN) {
        /* A dotted name with no call after it: a plain value that happens to
           contain a dot, so it is kept as a name. */
        value->kind = WM_VALUE_NAME;
        snprintf(value->text, sizeof(value->text), "%s", statement->target);
        free(statement);
        return 0;
    }

    if (parse_call_body(parser, statement) != 0) {
        wm_config_statements_free(statement, 1);
        return -1;
    }

    value->kind = WM_VALUE_CALL;
    value->call = statement;
    return 0;
}

static int parse_value(Parser *parser, WmValue *value) {
    memset(value, 0, sizeof(*value));

    if (parser->kind == TOKEN_STRING) {
        value->kind = WM_VALUE_STRING;
        snprintf(value->text, sizeof(value->text), "%s", parser->text);
        parser_advance(parser);
        return 0;
    }

    if (parser->kind == TOKEN_NUMBER) {
        value->kind = WM_VALUE_NUMBER;
        value->number = atoi(parser->text);
        parser_advance(parser);
        return 0;
    }

    if (parser->kind == TOKEN_OPEN) {
        return parse_list(parser, value);
    }

    if (parser->kind == TOKEN_NAME) {
        if (strcmp(parser->text, "True") == 0 ||
            strcmp(parser->text, "False") == 0) {
            value->kind = WM_VALUE_BOOL;
            value->boolean = parser->text[0] == 'T';
            parser_advance(parser);
            return 0;
        }
        /* Any other name begins either a plain name or a call; the call form
           is what a list of widgets is written with. */
        return parse_call_value(parser, value);
    }

    parser_error(parser, "a value is missing");
    return -1;
}

/* --- statements ----------------------------------------------------------- */

static int parse_argument(Parser *parser, WmArgument *argument) {
    argument->name[0] = '\0';

    if (parser->kind == TOKEN_NAME) {
        char name[WM_CONFIG_TEXT_LENGTH];
        snprintf(name, sizeof(name), "%s", parser->text);
        parser_advance(parser);

        if (parser->kind == TOKEN_ASSIGN) {
            snprintf(argument->name, sizeof(argument->name), "%s", name);
            parser_advance(parser);
            return parse_value(parser, &argument->value);
        }

        /* Not followed by '=', so the name was the value. It may still be the
           start of a call, so a call is built from the name that was read and
           whatever follows it. */
        if (parser->kind == TOKEN_OPEN || parser->kind == TOKEN_DOT) {
            WmStatement *statement = calloc(1, sizeof(WmStatement));
            if (!statement) {
                return -1;
            }
            statement->kind = WM_STMT_CALL;
            snprintf(statement->target, sizeof(statement->target), "%s", name);
            unsigned int length = (unsigned int)strlen(statement->target);

            while (parser->kind == TOKEN_DOT) {
                if (length + 1 < sizeof(statement->target)) {
                    statement->target[length++] = '.';
                }
                parser_advance(parser);
                if (parser->kind != TOKEN_NAME) {
                    parser_error(parser, "a name is missing after the dot");
                    wm_config_statements_free(statement, 1);
                    return -1;
                }
                unsigned int i = 0;
                while (parser->text[i] &&
                       length + 1 < sizeof(statement->target)) {
                    statement->target[length++] = parser->text[i++];
                }
                parser_advance(parser);
            }
            statement->target[length] = '\0';

            if (parser->kind == TOKEN_OPEN) {
                if (parse_call_body(parser, statement) != 0) {
                    wm_config_statements_free(statement, 1);
                    return -1;
                }
                argument->value.kind = WM_VALUE_CALL;
                argument->value.call = statement;
                return 0;
            }
            argument->value.kind = WM_VALUE_NAME;
            snprintf(argument->value.text, sizeof(argument->value.text),
                     "%s", statement->target);
            free(statement);
            return 0;
        }

        argument->value.kind = WM_VALUE_NAME;
        snprintf(argument->value.text, sizeof(argument->value.text),
                 "%s", name);
        return 0;
    }

    return parse_value(parser, &argument->value);
}

static int parse_call_body(Parser *parser, WmStatement *statement) {
    statement->arg_count = 0;
    parser_advance(parser);   /* the first token inside the brackets */

    if (parser->kind == TOKEN_CLOSE) {
        parser_advance(parser);
        return 0;
    }

    for (;;) {
        if (statement->arg_count >= WM_CONFIG_MAX_ARGS) {
            parser_error(parser, "too many arguments");
            return -1;
        }
        if (parse_argument(parser, &statement->args[statement->arg_count]) != 0) {
            return -1;
        }
        statement->arg_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_advance(parser);
            if (parser->kind == TOKEN_CLOSE) { break; }
            continue;
        }
        break;
    }

    if (parser->kind != TOKEN_CLOSE) {
        parser_error(parser, "a call is not closed");
        return -1;
    }
    parser_advance(parser);
    return 0;
}

static int parse_statement(Parser *parser, WmStatement *statement) {
    memset(statement, 0, sizeof(*statement));

    if (parser->kind == TOKEN_END) {
        return 1;
    }
    if (parser->kind != TOKEN_NAME) {
        parser_error(parser, "a statement must begin with a name");
        return -1;
    }

    if (parse_dotted_name(parser, statement->target,
                          sizeof(statement->target)) != 0) {
        return -1;
    }

    if (parser->kind == TOKEN_ASSIGN) {
        statement->kind = WM_STMT_ASSIGN;
        parser_advance(parser);
        return parse_value(parser, &statement->value) == 0 ? 0 : -1;
    }
    if (parser->kind == TOKEN_OPEN) {
        statement->kind = WM_STMT_CALL;
        return parse_call_body(parser, statement) == 0 ? 0 : -1;
    }

    /* A bare expression such as gcl_keys.all without an assignment: nothing
       to store, but not a mistake either. It is kept as a name assignment of
       itself so the walker sees it and moves on. */
    statement->kind = WM_STMT_ASSIGN;
    statement->value.kind = WM_VALUE_NAME;
    snprintf(statement->value.text, sizeof(statement->value.text), "%s",
             statement->target);
    return 0;
}

/* --- the file ------------------------------------------------------------- */

int wm_config_parse_text(const char *text, WmStatement **out, int *out_count) {
    last_error[0] = '\0';
    if (!out || !out_count) {
        return -1;
    }
    *out = NULL;
    *out_count = 0;
    if (!text) {
        return -1;
    }

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.cursor = text;
    parser_advance(&parser);

    int capacity = 0;
    WmStatement *statements = NULL;
    int count = 0;

    for (;;) {
        if (count == capacity) {
            int grown = capacity ? capacity * 2 : 16;
            WmStatement *bigger = realloc(statements,
                                          (size_t)grown * sizeof(WmStatement));
            if (!bigger) {
                wm_config_statements_free(statements, count);
                return -1;
            }
            statements = bigger;
            capacity = grown;
        }

        int result = parse_statement(&parser, &statements[count]);
        if (result == 1) {
            break;
        }
        if (result != 0) {
            /* A statement this grammar cannot read means the file was not
               understood, and a file that was only half understood is not a
               desktop worth switching to. The whole read fails, the caller
               keeps the config it had, and the error above names what could
               not be read. */
            wm_config_statements_free(statements, count);
            return -1;
        }
        count++;
        if (count >= WM_CONFIG_PARSER_MAX_STATEMENTS) {
            parser_error(&parser, "the script has too many statements");
            wm_config_statements_free(statements, count);
            return -1;
        }
    }

    *out = statements;
    *out_count = count;
    return 0;
}

int wm_config_parse_file(const char *path, WmStatement **out, int *out_count) {
    if (!path || !out || !out_count) {
        return -1;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return -1;
    }
    rewind(file);

    char *text = malloc((size_t)length + 1);
    if (!text) {
        fclose(file);
        return -1;
    }
    size_t read = fread(text, 1, (size_t)length, file);
    text[read] = '\0';
    fclose(file);

    int result = wm_config_parse_text(text, out, out_count);
    free(text);
    return result;
}

/* Release a value and everything it holds. A list holds values of its own and
   a call holds a statement, so the release follows the shape the parser
   built, all the way down. */
static void free_value(WmValue *value) {
    if (!value) {
        return;
    }
    for (int i = 0; i < value->item_count; i++) {
        free_value(&value->items[i]);
    }
    free(value->items);
    value->items = NULL;
    value->item_count = 0;

    if (value->call) {
        wm_config_statements_free(value->call, 1);
        value->call = NULL;
    }
}

void wm_config_statements_free(WmStatement *statements, int count) {
    if (!statements) {
        return;
    }
    for (int i = 0; i < count; i++) {
        WmStatement *statement = &statements[i];
        free_value(&statement->value);
        for (int j = 0; j < statement->arg_count; j++) {
            free_value(&statement->args[j].value);
        }
    }
    free(statements);
}

/* --- reading a parsed statement ------------------------------------------- */

const WmValue *wm_config_argument(const WmStatement *statement,
                                  const char *name) {
    if (!statement || !name) {
        return NULL;
    }
    for (int i = 0; i < statement->arg_count; i++) {
        if (strcmp(statement->args[i].name, name) == 0) {
            return &statement->args[i].value;
        }
    }
    return NULL;
}

void wm_config_value_text(const WmValue *value, char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    out[0] = '\0';
    if (!value) {
        return;
    }
    switch (value->kind) {
    case WM_VALUE_STRING:
    case WM_VALUE_NAME:
        snprintf(out, size, "%s", value->text);
        break;
    case WM_VALUE_NUMBER:
        snprintf(out, size, "%d", value->number);
        break;
    case WM_VALUE_BOOL:
        snprintf(out, size, "%s", value->boolean ? "true" : "false");
        break;
    case WM_VALUE_LIST:
    case WM_VALUE_CALL:
        break;
    }
}

int wm_config_value_number(const WmValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == WM_VALUE_NUMBER) {
        return value->number;
    }
    if (value->kind == WM_VALUE_STRING || value->kind == WM_VALUE_NAME) {
        char *end = NULL;
        long parsed = strtol(value->text, &end, 10);
        if (end && end != value->text) {
            return (int)parsed;
        }
    }
    return fallback;
}

int wm_config_value_bool(const WmValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == WM_VALUE_BOOL) {
        return value->boolean;
    }
    if (value->kind == WM_VALUE_STRING || value->kind == WM_VALUE_NAME) {
        if (strcmp(value->text, "true") == 0 || strcmp(value->text, "True") == 0 ||
            strcmp(value->text, "yes") == 0 || strcmp(value->text, "on") == 0 ||
            strcmp(value->text, "1") == 0) {
            return 1;
        }
        if (strcmp(value->text, "false") == 0 || strcmp(value->text, "False") == 0 ||
            strcmp(value->text, "no") == 0 || strcmp(value->text, "off") == 0 ||
            strcmp(value->text, "0") == 0) {
            return 0;
        }
    }
    if (value->kind == WM_VALUE_NUMBER) {
        return value->number != 0;
    }
    return fallback;
}
