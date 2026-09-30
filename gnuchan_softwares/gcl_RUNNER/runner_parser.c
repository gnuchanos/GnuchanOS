/*
 * runner_parser.c — the tokeniser and the tiny recursive-descent reader that
 * turn the settings script into a list of statements.
 *
 * The grammar, whole:
 *
 *     file      := (statement (NEWLINE | EOF))*
 *     statement := NAME '=' value
 *                | NAME '.' NAME '(' arguments ')'
 *     arguments := empty
 *                | argument (',' argument)* ','?
 *     argument  := value | NAME '=' value
 *     value     := STRING | NUMBER | 'True' | 'False' | NAME
 *                | '[' (value (',' value)* ','?)? ']'
 *                | NAME '.' NAME '(' arguments ')'
 *
 * A call is the only thing that spans lines, and it is read as one statement
 * however it is laid out: the reader counts the brackets it has opened, so a
 * call written across eight lines is the same statement as one written on
 * one. That is the whole of the file's structure — a name and a value, or a
 * name and a call — and nothing here needs to know what any name means.
 *
 * Reading is kept apart from understanding on purpose. This file knows a
 * string from a number; it does not know that add_program means a program or
 * that Prompt is a word on the screen. That is runner_config.c's job, and
 * keeping the two apart is what lets this file be read on its own.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_parser.h"

/* The reason the last read refused, kept here because the statements it was
   read into may be freed before anyone asks. One buffer, because only the
   most recent read is ever reported. */
static char parser_error[RUNNER_TEXT_LENGTH * 2];

static void set_error(const char *message) {
    snprintf(parser_error, sizeof(parser_error), "%s", message);
}

const char *runner_parse_last_error(void) {
    return parser_error;
}

/* --- tokens --------------------------------------------------------------- */

typedef enum TokenKind {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_IDENT,     /* a bare word: a name, True, False            */
    TOK_STRING,    /* "...", with its escapes resolved            */
    TOK_NUMBER,    /* 12, 24.5                                    */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA,
    TOK_EQUALS,
    TOK_DOT,
    TOK_OTHER,     /* anything else: a mistake, reported by position */
} TokenKind;

typedef struct Token {
    TokenKind kind;
    char text[RUNNER_TEXT_LENGTH];
    int number;
    int line;
} Token;

/* The reader's whole state: where it is in the text, which line it is on, the
   brackets it has opened, the token it has looked at but not yet consumed,
   and the first reason it refused. One token of lookahead is all this grammar
   needs — a call is told from an assignment by the one character after the
   name. */
typedef struct Reader {
    const char *cursor;
    int line;
    int depth;
    Token token;
    char error[RUNNER_TEXT_LENGTH * 2];
    int failed;
} Reader;

static void reader_fail(Reader *reader, const char *message) {
    if (reader->failed) {
        return;             /* the first reason is the one kept */
    }
    reader->failed = 1;
    snprintf(reader->error, sizeof(reader->error), "line %d: %s",
             reader->line, message);
}

static void token_clear(Token *token) {
    token->kind = TOK_EOF;
    token->text[0] = '\0';
    token->number = 0;
}

/* Skip the spaces that are not part of a token, and every comment. A '#'
   starts a comment that runs to the end of the line — the settings file is
   meant to be written with notes in it, and every note in the shipped script
   is one. */
static void reader_skip_space(Reader *reader) {
    for (;;) {
        char c = *reader->cursor;
        if (c == ' ' || c == '\t' || c == '\r') {
            reader->cursor++;
            continue;
        }
        if (c == '#') {
            while (*reader->cursor && *reader->cursor != '\n') {
                reader->cursor++;
            }
            continue;
        }
        /* Inside brackets a newline is not the end of anything: a call is
           allowed to be laid out across as many lines as it likes, and every
           argument on its own line is how the shipped script is written. */
        if (c == '\n' && reader->depth > 0) {
            reader->cursor++;
            reader->line++;
            continue;
        }
        return;
    }
}

/* Read a quoted string, resolving the escapes a script can contain: the quote
   itself, a backslash, and the newline and tab a command may be written
   with. Anything else after a backslash keeps the backslash, because a
   settings file is not a place to lose a character silently. */
static void reader_read_string(Reader *reader, Token *token) {
    char quote = *reader->cursor;
    reader->cursor++;
    int written = 0;

    while (*reader->cursor && *reader->cursor != quote) {
        char c = *reader->cursor++;
        if (c == '\n') {
            reader->line++;     /* a string may be written across lines */
        }
        if (c == '\\') {
            char next = *reader->cursor;
            if (next == '\0') {
                break;
            }
            reader->cursor++;
            switch (next) {
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '"':  c = '"';  break;
            default:
                if (written < RUNNER_TEXT_LENGTH - 1) {
                    token->text[written++] = '\\';
                }
                c = next;
                break;
            }
        }
        if (written < RUNNER_TEXT_LENGTH - 1) {
            token->text[written++] = c;
        }
    }

    if (*reader->cursor != quote) {
        reader_fail(reader, "a string is missing its closing quote");
        token->text[written] = '\0';
        return;
    }
    reader->cursor++;           /* the closing quote */
    token->text[written] = '\0';
    token->kind = TOK_STRING;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* Read a bare word: a letter or underscore first, then letters, digits and
   underscores. A name is what a script writes for True, for a variable it
   defined earlier, and for the object half of a call. */
static void reader_read_ident(Reader *reader, Token *token) {
    int written = 0;
    while (is_ident_char(*reader->cursor)) {
        if (written < RUNNER_TEXT_LENGTH - 1) {
            token->text[written++] = *reader->cursor;
        }
        reader->cursor++;
    }
    token->text[written] = '\0';
    token->kind = TOK_IDENT;
}

/* Read a number. Only whole numbers and a fractional part are accepted; a
   script describing a launcher has no use for an exponent, and accepting one
   would mean a float parser here for nothing. */
static void reader_read_number(Reader *reader, Token *token) {
    int written = 0;
    while (isdigit((unsigned char)*reader->cursor) ||
           *reader->cursor == '.') {
        if (written < RUNNER_TEXT_LENGTH - 1) {
            token->text[written++] = *reader->cursor;
        }
        reader->cursor++;
    }
    token->text[written] = '\0';
    token->kind = TOK_NUMBER;
    token->number = atoi(token->text);
}

static void reader_next(Reader *reader) {
    reader_skip_space(reader);
    token_clear(&reader->token);
    reader->token.line = reader->line;

    char c = *reader->cursor;
    if (c == '\0') {
        reader->token.kind = TOK_EOF;
        return;
    }
    if (c == '\n') {
        reader->cursor++;
        reader->token.kind = TOK_NEWLINE;
        reader->line++;
        return;
    }
    if (c == '"' || c == '\'') {
        reader_read_string(reader, &reader->token);
        return;
    }
    if (is_ident_start(c)) {
        reader_read_ident(reader, &reader->token);
        return;
    }
    if (isdigit((unsigned char)c)) {
        reader_read_number(reader, &reader->token);
        return;
    }

    reader->cursor++;
    switch (c) {
    case '(': reader->depth++; reader->token.kind = TOK_LPAREN;   return;
    case ')': reader->depth--; reader->token.kind = TOK_RPAREN;   return;
    case '[': reader->depth++; reader->token.kind = TOK_LBRACKET; return;
    case ']': reader->depth--; reader->token.kind = TOK_RBRACKET; return;
    case ',': reader->token.kind = TOK_COMMA;  return;
    case '=': reader->token.kind = TOK_EQUALS; return;
    case '.': reader->token.kind = TOK_DOT;    return;
    default:
        reader->token.kind = TOK_OTHER;
        reader->token.text[0] = c;
        reader->token.text[1] = '\0';
        return;
    }
}

/* --- values --------------------------------------------------------------- */

/* Free a value and everything it owns. A list frees its items, each of which
   may be a list of its own; a call frees its arguments the same way. */
static void value_free(RunnerValue *value) {
    for (int i = 0; i < value->item_count; i++) {
        value_free(&value->items[i]);
    }
    free(value->items);
    value->items = NULL;
    value->item_count = 0;

    if (value->call) {
        for (int i = 0; i < value->call->arg_count; i++) {
            value_free(&value->call->args[i].value);
        }
        free(value->call);
        value->call = NULL;
    }
}

/* Forward: a value may be a list of values, which may be calls, which hold
   arguments, which are values again. */
static void parse_value(Reader *reader, RunnerValue *value);
static void parse_arguments(Reader *reader, RunnerStatement *call);

/* Read `object.method(...)` as a value, so a call written inside a list is
   kept whole. The name has already been read. */
static void parse_call_value(Reader *reader, RunnerValue *value,
                             const char *target) {
    value->kind = RUNNER_VALUE_CALL;
    value->call = calloc(1, sizeof(RunnerStatement));
    if (!value->call) {
        reader_fail(reader, "out of memory reading a call");
        return;
    }
    value->call->kind = RUNNER_STMT_CALL;
    snprintf(value->call->target, sizeof(value->call->target), "%s", target);

    if (reader->token.kind == TOK_LPAREN) {
        reader_next(reader);
        parse_arguments(reader, value->call);
    }
}

static void parse_list(Reader *reader, RunnerValue *value) {
    value->kind = RUNNER_VALUE_LIST;
    value->items = calloc(RUNNER_MAX_LIST_ITEMS, sizeof(RunnerValue));
    if (!value->items) {
        reader_fail(reader, "out of memory reading a list");
        return;
    }

    reader_next(reader);           /* the '[' */
    if (reader->token.kind == TOK_RBRACKET) {
        reader_next(reader);
        return;
    }

    while (!reader->failed) {
        if (value->item_count >= RUNNER_MAX_LIST_ITEMS) {
            reader_fail(reader, "a list holds too many items");
            return;
        }
        RunnerValue *item = &value->items[value->item_count++];
        memset(item, 0, sizeof(*item));
        parse_value(reader, item);
        if (reader->failed) {
            return;
        }
        if (reader->token.kind == TOK_COMMA) {
            reader_next(reader);
            /* A trailing comma before the bracket is allowed: a list written
               one item per line usually has one. */
            if (reader->token.kind == TOK_RBRACKET) {
                break;
            }
            continue;
        }
        break;
    }

    if (reader->token.kind != TOK_RBRACKET) {
        reader_fail(reader, "a list is missing its closing bracket");
        return;
    }
    reader_next(reader);
}

/* A qualified name — `GnuChanRunner.add_program` — is read as one string by
   joining the parts, because nothing downstream cares which half of it was
   which: the reader hands the whole "object.method" to the config module,
   whose job it is to decide what that means. */
static void reader_read_qualified(Reader *reader, char *out,
                                  unsigned int size) {
    snprintf(out, size, "%s", reader->token.text);
    reader_next(reader);
    while (reader->token.kind == TOK_DOT) {
        reader_next(reader);
        if (reader->token.kind != TOK_IDENT) {
            reader_fail(reader, "a '.' is not followed by a name");
            return;
        }
        unsigned int used = (unsigned int)strlen(out);
        /* Room for the dot and at least one character of the name: what is
           left is handed to snprintf, which truncates rather than overruns. */
        if (used + 2u < size) {
            snprintf(out + used, size - used, ".%s", reader->token.text);
        }
        reader_next(reader);
    }
}

static void parse_value(Reader *reader, RunnerValue *value) {
    memset(value, 0, sizeof(*value));

    switch (reader->token.kind) {
    case TOK_STRING:
        value->kind = RUNNER_VALUE_STRING;
        snprintf(value->text, sizeof(value->text), "%s", reader->token.text);
        reader_next(reader);
        return;

    case TOK_NUMBER:
        value->kind = RUNNER_VALUE_NUMBER;
        value->number = reader->token.number;
        snprintf(value->text, sizeof(value->text), "%s", reader->token.text);
        reader_next(reader);
        return;

    case TOK_LBRACKET:
        parse_list(reader, value);
        return;

    case TOK_IDENT: {
        /* Before anything else: is this a name, or the object half of a
           call? One dotted name is read either way, and the bracket after it
           is what decides. */
        char qualified[RUNNER_TEXT_LENGTH];
        reader_read_qualified(reader, qualified, sizeof(qualified));
        if (reader->failed) {
            return;
        }
        if (reader->token.kind == TOK_LPAREN) {
            parse_call_value(reader, value, qualified);
            return;
        }
        if (strcmp(qualified, "True") == 0) {
            value->kind = RUNNER_VALUE_BOOL;
            value->boolean = 1;
            return;
        }
        if (strcmp(qualified, "False") == 0) {
            value->kind = RUNNER_VALUE_BOOL;
            value->boolean = 0;
            return;
        }
        value->kind = RUNNER_VALUE_NAME;
        snprintf(value->text, sizeof(value->text), "%s", qualified);
        return;
    }

    default:
        reader_fail(reader, "a value was expected here");
        return;
    }
}

/* --- calls ---------------------------------------------------------------- */

static void parse_arguments(Reader *reader, RunnerStatement *call) {
    if (reader->token.kind == TOK_RPAREN) {
        reader_next(reader);
        return;
    }

    while (!reader->failed) {
        if (call->arg_count >= RUNNER_MAX_ARGS) {
            reader_fail(reader, "a call passes too many arguments");
            return;
        }
        RunnerArgument *argument = &call->args[call->arg_count];
        int have_value = 0;

        /* A named argument is a name and then '='. Anything else is a
           positional one, whose name is left empty — which is how a caller
           tells "the first thing passed" from "the thing called Name". */
        if (reader->token.kind == TOK_IDENT) {
            Token save = reader->token;
            reader_next(reader);
            if (reader->token.kind == TOK_EQUALS) {
                snprintf(argument->name, sizeof(argument->name), "%s",
                         save.text);
                reader_next(reader);
            } else {
                /* Not named after all: the word that was consumed to look
                   ahead is the value, and it is written in here rather than
                   re-read, because the reader has already moved past it. */
                argument->name[0] = '\0';
                memset(&argument->value, 0, sizeof(argument->value));
                if (strcmp(save.text, "True") == 0) {
                    argument->value.kind = RUNNER_VALUE_BOOL;
                    argument->value.boolean = 1;
                } else if (strcmp(save.text, "False") == 0) {
                    argument->value.kind = RUNNER_VALUE_BOOL;
                    argument->value.boolean = 0;
                } else {
                    argument->value.kind = RUNNER_VALUE_NAME;
                    snprintf(argument->value.text,
                             sizeof(argument->value.text), "%s", save.text);
                }
                have_value = 1;
            }
        } else {
            argument->name[0] = '\0';
        }

        if (!have_value) {
            parse_value(reader, &argument->value);
            if (reader->failed) {
                return;
            }
        }
        call->arg_count++;

        if (reader->token.kind == TOK_COMMA) {
            reader_next(reader);
            if (reader->token.kind == TOK_RPAREN) {
                break;      /* a trailing comma is allowed */
            }
            continue;
        }
        break;
    }

    if (reader->token.kind != TOK_RPAREN) {
        reader_fail(reader, "a call is missing its closing bracket");
        return;
    }
    reader_next(reader);
}

/* --- statements ----------------------------------------------------------- */

/* Read one statement. The name is read first and the token after it decides
   which shape it is: '=' makes it an assignment and '(' a call. Anything else
   is a statement this grammar does not have, and saying so is better than
   skipping it. */
static void parse_statement(Reader *reader, RunnerStatement *statement) {
    memset(statement, 0, sizeof(*statement));

    if (reader->token.kind == TOK_EOF ||
        reader->token.kind == TOK_NEWLINE) {
        return;             /* a blank line: the caller skips it */
    }

    if (reader->token.kind != TOK_IDENT) {
        reader_fail(reader, "a statement begins with a name");
        return;
    }

    char qualified[RUNNER_TEXT_LENGTH];
    reader_read_qualified(reader, qualified, sizeof(qualified));
    if (reader->failed) {
        return;
    }
    snprintf(statement->target, sizeof(statement->target), "%s", qualified);

    if (reader->token.kind == TOK_EQUALS) {
        statement->kind = RUNNER_STMT_ASSIGN;
        reader_next(reader);
        parse_value(reader, &statement->value);
        return;
    }

    if (reader->token.kind == TOK_LPAREN) {
        statement->kind = RUNNER_STMT_CALL;
        reader_next(reader);
        parse_arguments(reader, statement);
        return;
    }

    reader_fail(reader, "'=' or '(' was expected after the name");
}

/* --- the public entry points ---------------------------------------------- */

int runner_parse_file(const char *path, RunnerStatement **out, int *out_count) {
    *out = NULL;
    *out_count = 0;

    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error("the settings file could not be opened");
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error("the settings file could not be read");
        return -1;
    }
    long size = ftell(file);
    if (size < 0 || size > 1024 * 1024) {
        /* A settings file larger than a megabyte is not a settings file, and
           the ceiling is what stops a wrong path from reading a disk. */
        fclose(file);
        set_error("the settings file is far too large to be one");
        return -1;
    }
    rewind(file);

    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(file);
        set_error("out of memory reading the settings file");
        return -1;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    text[got] = '\0';
    fclose(file);

    RunnerStatement *statements = calloc(RUNNER_MAX_STATEMENTS,
                                         sizeof(RunnerStatement));
    if (!statements) {
        free(text);
        set_error("out of memory reading the settings file");
        return -1;
    }

    Reader reader;
    memset(&reader, 0, sizeof(reader));
    reader.cursor = text;
    reader.line = 1;
    reader_next(&reader);

    int count = 0;
    while (!reader.failed) {
        /* Every newline between statements is skipped here, so a blank line,
           a comment-only line and the line after a call are all the same
           thing: nothing to read. */
        while (reader.token.kind == TOK_NEWLINE) {
            reader_next(&reader);
        }
        if (reader.token.kind == TOK_EOF) {
            break;
        }
        if (count >= RUNNER_MAX_STATEMENTS) {
            reader_fail(&reader, "the file holds too many statements");
            break;
        }
        parse_statement(&reader, &statements[count]);
        if (reader.failed) {
            break;
        }
        count++;
    }

    if (reader.failed) {
        set_error(reader.error);
        runner_parse_free(statements, count);
        free(text);
        return -1;
    }

    set_error("");
    free(text);
    *out = statements;
    *out_count = count;
    return 0;
}

void runner_parse_free(RunnerStatement *statements, int count) {
    if (!statements) {
        return;
    }
    for (int i = 0; i < count; i++) {
        value_free(&statements[i].value);
        for (int a = 0; a < statements[i].arg_count; a++) {
            value_free(&statements[i].args[a].value);
        }
    }
    free(statements);
}

const RunnerValue *runner_argument(const RunnerStatement *statement,
                                   const char *name) {
    for (int i = 0; i < statement->arg_count; i++) {
        if (strcmp(statement->args[i].name, name) == 0) {
            return &statement->args[i].value;
        }
    }
    return NULL;
}

const RunnerValue *runner_argument_first(const RunnerStatement *statement) {
    for (int i = 0; i < statement->arg_count; i++) {
        if (statement->args[i].name[0] == '\0') {
            return &statement->args[i].value;
        }
    }
    return NULL;
}

void runner_value_text(const RunnerValue *value, char *out,
                       unsigned int size) {
    out[0] = '\0';
    if (!value) {
        return;
    }
    switch (value->kind) {
    case RUNNER_VALUE_STRING:
    case RUNNER_VALUE_NAME:
        snprintf(out, size, "%s", value->text);
        break;
    case RUNNER_VALUE_NUMBER:
        snprintf(out, size, "%d", value->number);
        break;
    case RUNNER_VALUE_BOOL:
        snprintf(out, size, "%s", value->boolean ? "True" : "False");
        break;
    default:
        /* A list or a call has no single text, and a caller that asked for
           one asked the wrong question: it gets nothing. */
        break;
    }
}

int runner_value_number(const RunnerValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == RUNNER_VALUE_NUMBER) {
        return value->number;
    }
    if (value->kind == RUNNER_VALUE_STRING) {
        return atoi(value->text);
    }
    return fallback;
}

int runner_value_bool(const RunnerValue *value, int fallback) {
    if (!value) {
        return fallback;
    }
    if (value->kind == RUNNER_VALUE_BOOL) {
        return value->boolean;
    }
    if (value->kind == RUNNER_VALUE_STRING) {
        /* A string that says yes or no is a flag someone wrote with quotes
           round it by mistake, and reading it as the flag is kinder than
           making the script not work. */
        if (strcmp(value->text, "True") == 0 ||
            strcmp(value->text, "true") == 0 ||
            strcmp(value->text, "1") == 0) {
            return 1;
        }
        if (strcmp(value->text, "False") == 0 ||
            strcmp(value->text, "false") == 0 ||
            strcmp(value->text, "0") == 0) {
            return 0;
        }
    }
    return fallback;
}
