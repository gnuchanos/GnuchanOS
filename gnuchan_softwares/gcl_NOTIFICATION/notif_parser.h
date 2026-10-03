/*
 * notif_parser.h — enough of the Python grammar to read the settings script,
 * and nothing more.
 *
 * This is the same parser GnuChanWM, GnuChanDock, GnuChanSS, GnuChanTerm,
 * GnuChanRunner and GnuChanFetch use, under its own name. It knows nothing
 * about notifications, and the module that gives the statements meaning knows
 * nothing about parsing.
 */
#ifndef GNUCHANNOTIFICATION_PARSER_H
#define GNUCHANNOTIFICATION_PARSER_H

#include "notif_config.h"

#define NOTIF_MAX_ARGS              32
#define NOTIF_PARSER_MAX_STATEMENTS 128

typedef enum NotifValueKind {
    NOTIF_VALUE_STRING,
    NOTIF_VALUE_NUMBER,
    NOTIF_VALUE_REAL,
    NOTIF_VALUE_BOOL,
    NOTIF_VALUE_NAME,
    NOTIF_VALUE_LIST,
    NOTIF_VALUE_CALL,
} NotifValueKind;

typedef struct NotifStatement NotifStatement;
typedef struct NotifValue {
    NotifValueKind kind;
    char text[NOTIF_TEXT_LENGTH];
    int number;
    double real;
    int boolean;
    struct NotifValue *items;
    int item_count;
    NotifStatement *call;
} NotifValue;

typedef struct NotifArgument {
    char name[NOTIF_TEXT_LENGTH];
    NotifValue value;
} NotifArgument;

typedef enum NotifStatementKind {
    NOTIF_STMT_ASSIGN,
    NOTIF_STMT_CALL,
} NotifStatementKind;

struct NotifStatement {
    NotifStatementKind kind;
    char target[NOTIF_TEXT_LENGTH];
    NotifValue value;
    NotifArgument args[NOTIF_MAX_ARGS];
    int arg_count;
};

int notif_parse_file(const char *path, NotifStatement **out, int *out_count);
void notif_parse_free(NotifStatement *statements, int count);
const NotifValue *notif_argument(const NotifStatement *statement,
                                 const char *name);
void notif_value_text(const NotifValue *value, char *out, unsigned int size);
int notif_value_number(const NotifValue *value, int fallback);
int notif_value_bool(const NotifValue *value, int fallback);
const char *notif_parse_last_error(void);

#endif /* GNUCHANNOTIFICATION_PARSER_H */
