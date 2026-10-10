/*
 * settings_types.h — the shape GnuChanSettings is built from.
 *
 * GnuChanSettings is the desktop's control panel: the one place a person who
 * does not want to edit a script opens, changes a colour, a size or a switch,
 * and saves. Every Gnuchan program is configured by a settings file under
 * ~/.config that is written like a Python script, and this program is the GUI
 * over those files.
 *
 * Nothing here knows about X. This header is plain data: WHAT a setting is,
 * what KIND of value it holds, and HOW the value is written in the file — the
 * three things the reader, the writer and the window all have to agree on.
 */
#ifndef GNUCHANSETTINGS_TYPES_H
#define GNUCHANSETTINGS_TYPES_H

/* A written value is long enough for a colour, a command line, a font
   description or a whole prompt string. One length governs the reader and the
   window both. */
#define SETTINGS_TEXT_LENGTH 256

/* How many setting rows one program may show. */
#define SETTINGS_MAX_ROWS 48

/* How many programs the panel knows how to configure. */
#define SETTINGS_MAX_APPS 16

/* What a value IS, which decides the control the window draws for it and the
   check the writer makes before it writes. */
typedef enum SettingType {
    SETTING_COLOR,   /* "#1a0b2e" — a colour, drawn as a swatch           */
    SETTING_TEXT,    /* a command, a font, a path — a line of text        */
    SETTING_INT,     /* 48 — a whole number                              */
    SETTING_REAL,    /* 1.0 — a fraction, written bare like an int        */
    SETTING_BOOL,    /* True / False — a switch                          */
} SettingType;

/* HOW the value is written in the file. SETTING_STYLE_INHERIT means "whatever
   the program's own style is" and is NEGATIVE so a zeroed row inherits. */
typedef enum SettingStyle {
    SETTING_STYLE_INHERIT = -1, /* use the program's own style           */
    SETTING_CALL,    /* gcl_X.Main(Key="value") — the argument of a call  */
    SETTING_FLAT,    /* Key = value            — a bare line             */
    SETTING_ASSIGN,  /* gcl_Switcher.name = "#..."  — a dotted line        */
    SETTING_FUNC,    /* gcl_Window.set_name("#...")  — a function's first
                        argument                                          */
} SettingStyle;

/* One setting: what to call it, where to find it, and what it holds. */
typedef struct SettingDef {
    const char  *key;        /* the token to find                           */
    const char  *label;      /* what a person reads in the window           */
    const char  *group;      /* the heading this row sits under, or ""       */
    SettingType  type;
    SettingStyle style;      /* SETTING_STYLE_INHERIT = the program's        */
    const char  *call;       /* NULL = the program's call prefix            */
    const char  *fallback;   /* the value to show when the file names none  */
} SettingDef;

/* One program: where its settings live, how they are written, and the rows the
   window shows for it. */
typedef struct AppDef {
    const char       *name;       /* the category's title in the sidebar   */
    const char       *subtitle;   /* one line under it                    */
    const char       *dir;        /* ~/.config/<dir>/                     */
    const char       *file;       /* <file> inside that directory         */
    const char       *call;       /* the call to add a missing argument to */
    SettingStyle      style;      /* how every row of this program is
                                     written unless a row says otherwise  */
    int               capital_bools; /* True/False or true/false         */
    const SettingDef *settings;
    int               setting_count;
} AppDef;

/* The table of programs the panel can configure, and how many there are. This
   is defined in settings_apps.c. */
const AppDef *settings_apps(int *count);

#endif /* GNUCHANSETTINGS_TYPES_H */
