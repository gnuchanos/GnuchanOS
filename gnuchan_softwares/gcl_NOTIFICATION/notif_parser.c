/*
 * notif_parser.c — a small reader for the settings script.
 *
 * This is not a Python interpreter and must not become one. It understands the
 * two statement shapes a settings file uses — a name given a value, and a call
 * that passes named arguments — and the handful of value kinds those contain.
 * It produces a flat list of statements; notif_config.c is what gives them
 * meaning.
 *
 * This is the same parser GnuChanWM, GnuChanDock, GnuChanSS, GnuChanTerm,
 * GnuChanRunner and GnuChanFetch use, under its own name.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "notif_parser.h"

static char s_last_error[NOTIF_TEXT_LENGTH * 2];
static int s_have_error = 0;

const char *notif_parse_last_error(void) {
    return s_have_error ? s_last_error : "";
}

typedef enum TokenKind {
    TOKEN_END,
    TOKEN_NAME,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_EQUALS,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_COMMA,
    TOKEN_NEWLINE,
    TOKEN_UNKNOWN,
} TokenKind;

typedef struct Parser {
    const char *text;
    size_t length;
    size_t position;
    TokenKind kind;
    char text_value[NOTIF_TEXT_LENGTH];
    int number_value;
    double real_value;
    int depth;
    int failed;
} Parser;

static void free_value(NotifValue *value);

static void free_statement(NotifStatement *statement) {
    free_value(&statement->value);
    for (int i = 0; i < statement->arg_count; i++) {
        free_value(&statement->args[i].value);
    }
    statement->arg_count = 0;
}

static void free_value(NotifValue *value) {
    if (value->kind == NOTIF_VALUE_LIST && value->items) {
        for (int i = 0; i < value->item_count; i++) {
            free_value(&value->items[i]);
        }
        free(value->items);
        value->items = NULL;
        value->item_count = 0;
    } else if (value->kind == NOTIF_VALUE_CALL && value->call) {
        free_statement(value->call);
        free(value->call);
        value->call = NULL;
    }
}

void notif_parse_free(NotifStatement *statements, int count) {
    if (!statements) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free_statement(&statements[i]);
    }
    free(statements);
}

static void parser_error(Parser *parser, const char *message) {
    if (parser->failed) {
        return;
    }
    parser->failed = 1;
    if (!s_have_error) {
        s_have_error = 1;
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

static int is_name_start(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int is_name_char(int c) {
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '.';
}

static int is_digit(int c) {
    return c >= '0' && c <= '9';
}

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
        parser->position++;
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

static int parse_value(Parser *parser, NotifValue *value);
static int parse_call_body(Parser *parser, NotifStatement *statement);

static int parse_list(Parser *parser, NotifValue *value) {
    value->kind = NOTIF_VALUE_LIST;
    value->items = NULL;
    value->item_count = 0;

    parser_next(parser);
    if (parser->kind == TOKEN_RBRACKET) {
        parser_next(parser);
        return 1;
    }

    int capacity = 0;
    for (;;) {
        if (value->item_count == capacity) {
            int grown = capacity ? capacity * 2 : 4;
            NotifValue *items = realloc(value->items,
                                        sizeof(NotifValue) * (size_t)grown);
            if (!items) {
                parser_error(parser, "out of memory");
                return 0;
            }
            value->items = items;
            capacity = grown;
        }
        NotifValue *item = &value->items[value->item_count];
        memset(item, 0, sizeof(*item));
        if (!parse_value(parser, item)) {
            return 0;
        }
        value->item_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_next(parser);
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

static int parse_value(Parser *parser, NotifValue *value) {
    memset(value, 0, sizeof(*value));
    switch (parser->kind) {
    case TOKEN_STRING:
        value->kind = NOTIF_VALUE_STRING;
        snprintf(value->text, sizeof(value->text), "%s", parser->text_value);
        parser_next(parser);
        return 1;
    case TOKEN_NUMBER:
        value->number = parser->number_value;
        value->real = parser->real_value;
        value->kind = (strchr(parser->text_value, '.') != NULL)
                          ? NOTIF_VALUE_REAL : NOTIF_VALUE_NUMBER;
        snprintf(value->text, sizeof(value->text), "%s", parser->text_value);
        parser_next(parser);
        return 1;
    case TOKEN_TRUE:
        value->kind = NOTIF_VALUE_BOOL;
        value->boolean = 1;
        parser_next(parser);
        return 1;
    case TOKEN_FALSE:
        value->kind = NOTIF_VALUE_BOOL;
        value->boolean = 0;
        parser_next(parser);
        return 1;
    case TOKEN_NAME: {
        char name[NOTIF_TEXT_LENGTH];
        snprintf(name, sizeof(name), "%s", parser->text_value);
        parser_next(parser);
        if (parser->kind == TOKEN_LPAREN) {
            value->kind = NOTIF_VALUE_CALL;
            value->call = calloc(1, sizeof(NotifStatement));
            if (!value->call) {
                parser_error(parser, "out of memory");
                return 0;
            }
            snprintf(value->call->target, sizeof(value->call->target), "%s",
                     name);
            value->call->kind = NOTIF_STMT_CALL;
            return parse_call_body(parser, value->call);
        }
        value->kind = NOTIF_VALUE_NAME;
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

static int parse_argument(Parser *parser, NotifArgument *argument) {
    memset(argument, 0, sizeof(*argument));

    if (parser->kind == TOKEN_NAME) {
        char saved[NOTIF_TEXT_LENGTH];
        snprintf(saved, sizeof(saved), "%s", parser->text_value);
        parser_next(parser);
        if (parser->kind == TOKEN_EQUALS) {
            snprintf(argument->name, sizeof(argument->name), "%s", saved);
            parser_next(parser);
            return parse_value(parser, &argument->value);
        }
        if (parser->kind == TOKEN_LPAREN) {
            argument->value.kind = NOTIF_VALUE_CALL;
            argument->value.call = calloc(1, sizeof(NotifStatement));
            if (!argument->value.call) {
                parser_error(parser, "out of memory");
                return 0;
            }
            snprintf(argument->value.call->target,
                     sizeof(argument->value.call->target), "%s", saved);
            argument->value.call->kind = NOTIF_STMT_CALL;
            return parse_call_body(parser, argument->value.call);
        }
        argument->value.kind = NOTIF_VALUE_NAME;
        snprintf(argument->value.text, sizeof(argument->value.text), "%s",
                 saved);
        return 1;
    }

    return parse_value(parser, &argument->value);
}

static int parse_call_body(Parser *parser, NotifStatement *statement) {
    parser_next(parser);

    if (parser->kind == TOKEN_RPAREN) {
        parser_next(parser);
        return 1;
    }

    for (;;) {
        if (statement->arg_count >= NOTIF_MAX_ARGS) {
            parser_error(parser, "a call passed more arguments than are read");
            return 0;
        }
        NotifArgument *argument = &statement->args[statement->arg_count];
        if (!parse_argument(parser, argument)) {
            return 0;
        }
        statement->arg_count++;

        if (parser->kind == TOKEN_COMMA) {
            parser_next(parser);
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

static int parse_statement(Parser *parser, NotifStatement *statement) {
    memset(statement, 0, sizeof(*statement));

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
        statement->kind = NOTIF_STMT_ASSIGN;
        parser_next(parser);
        if (!parse_value(parser, &statement->value)) {
            return -1;
        }
    } else if (parser->kind == TOKEN_LPAREN) {
        statement->kind = NOTIF_STMT_CALL;
        if (!parse_call_body(parser, statement)) {
            return -1;
        }
    } else {
        parser_error(parser, "a statement was not = or ( after its name");
        return -1;
    }

    while (parser->kind == TOKEN_NEWLINE) {
        parser_next(parser);
    }
    return 1;
}

int notif_parse_file(const char *path, NotifStatement **out, int *out_count) {
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

    NotifStatement *statements =
        calloc(NOTIF_PARSER_MAX_STATEMENTS, sizeof(NotifStatement));
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
    while (count < NOTIF_PARSER_MAX_STATEMENTS) {
        int result = parse_statement(&parser, &statements[count]);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            notif_parse_free(statements, count);
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

const NotifValue *notif_argument(const NotifStatement *statement,
                                 const char *name) {
    if (!statement || statement->kind != NOTIF_STMT_CALL) {
        return NULL;
    }
    for (int i = 0; i < statement->arg_count; i++) {
        if (strcmp(statement->args[i].name, name) == 0) {
            return &statement->args[i].value;
        }
    }
    return NULL;
}

void notif_value_text(const NotifValue *value, char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    switch (value->kind) {
    case NOTIF_VALUE_STRING:
    case NOTIF_VALUE_NAME:
        snprintf(out, size, "%s", value->text);
        break;
    case NOTIF_VALUE_NUMBER:
        snprintf(out, size, "%d", value->number);
        break;
    case NOTIF_VALUE_REAL:
        snprintf(out, size, "%g", value->real);
        break;
    case NOTIF_VALUE_BOOL:
        snprintf(out, size, "%s", value->boolean ? "True" : "False");
        break;
    default:
        out[0] = '\0';
        break;
    }
}

int notif_value_number(const NotifValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == NOTIF_VALUE_NUMBER) {
        return value->number;
    }
    if (value->kind == NOTIF_VALUE_REAL) {
        return (int)value->real;
    }
    if (value->kind == NOTIF_VALUE_BOOL) {
        return value->boolean;
    }
    if ((value->kind == NOTIF_VALUE_STRING ||
         value->kind == NOTIF_VALUE_NAME) && value->text[0]) {
        char *end = NULL;
        long parsed = strtol(value->text, &end, 10);
        if (end && *end == '\0') {
            return (int)parsed;
        }
    }
    return fallback;
}

int notif_value_bool(const NotifValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == NOTIF_VALUE_BOOL) {
        return value->boolean;
    }
    if (value->kind == NOTIF_VALUE_NUMBER) {
        return value->number != 0;
    }
    if (value->kind == NOTIF_VALUE_NAME) {
        if (strcmp(value->text, "True") == 0 ||
            strcmp(value->text, "true") == 0) {
            return 1;
        }
        if (strcmp(value->text, "False") == 0 ||
            strcmp(value->text, "false") == 0) {
            return 0;
        }
    }
    return fallback;
}
