/*
 * settings_apps.c — the programs the panel can configure.
 *
 * This is the one place that knows WHICH programs GnuChanSettings edits, where
 * each one keeps its settings, how that file is written, and which of its
 * settings are worth putting in front of a person. Every row here is a setting
 * that PROGRAM ACTUALLY READS — taken from its own config header and the .py
 * it ships — and nothing else. A row that named a token no program looks for
 * would be a field a person could fill in and watch do nothing.
 *
 * The rows are not every line a file may contain. The calls that carry whole
 * LISTS — a bar's widgets, the terminal's palette, the key bindings, a fetch's
 * field list — are left out: a list is not one value a panel can offer as a
 * field. What is here is the values that ARE one value: a colour, a size, a
 * switch, a name.
 *
 * A few settings are one of a short fixed set of NAMES (the screen saver's
 * effect, a window's corner, the sort column). Those are SETTING_CHOICE rows
 * and the window draws them as a dropdown, so nobody has to remember that
 * "3dwall" is spelled without a space.
 */
#include <stddef.h>
#include <string.h>

#include "settings_types.h"

/* Terse row builders. Each produces one SettingDef. */
#define ROW(key, label, group, type, fallback) \
    { key, label, group, type, SETTING_STYLE_INHERIT, NULL, fallback, NULL }
#define ROW_IN(key, label, group, type, style, call, fallback) \
    { key, label, group, type, style, call, fallback, NULL }
#define CHOICE(key, label, group, fallback, list) \
    { key, label, group, SETTING_CHOICE, SETTING_STYLE_INHERIT, NULL, \
      fallback, list }

#define COLOR(key, label, group, fallback) \
    ROW(key, label, group, SETTING_COLOR, fallback)
#define TEXT(key, label, group, fallback) \
    ROW(key, label, group, SETTING_TEXT, fallback)
#define INT(key, label, group, fallback) \
    ROW(key, label, group, SETTING_INT, fallback)
#define REAL(key, label, group, fallback) \
    ROW(key, label, group, SETTING_REAL, fallback)
#define BOOL(key, label, group, fallback) \
    ROW(key, label, group, SETTING_BOOL, fallback)

/* --- the fixed lists a choice row offers ----------------------------------- */

static const char *const effect_choices[] = { "pipe", "3dwall", NULL };
static const char *const corner_choices[] = {
    "top-right", "top-left", "bottom-right", "bottom-left",
    "top-center", "bottom-center", NULL
};
static const char *const place_choices[] = { "center", "top", "bottom", NULL };
static const char *const command_choices[] = { "off", "typed", "always", NULL };
static const char *const sort_choices[] = { "cpu", "mem", "pid", "name", NULL };

/* --- GnuChanDock ----------------------------------------------------------- */

static const SettingDef dock_settings[] = {
    COLOR("Background",     "Background",  "Palette", "#1a0b2e"),
    COLOR("BackgroundEdge", "Edge",        "Palette", "#7b2cbf"),
    COLOR("Field",          "Hover field", "Palette", "#241033"),
    COLOR("Text",           "Text",        "Palette", "#e0c3fc"),
    COLOR("Accent",         "Accent",      "Palette", "#c77dff"),
    COLOR("Badge",          "Group badge", "Palette", "#1a0b2e"),

    INT("IconSize",     "Icon size",         "Shape", "48"),
    INT("Gap",          "Gap between icons", "Shape", "30"),
    INT("Padding",      "Padding",           "Shape", "10"),
    INT("Margin",       "Margin from edge",  "Shape", "6"),
    INT("Corner",       "Corner radius",     "Shape", "16"),
    INT("Magnify",      "Magnify amount",    "Shape", "18"),
    INT("MagnifyReach", "Magnify reach",     "Shape", "1"),

    TEXT("Font",         "Font",        "Label", "monospace:pixelsize=15"),
    BOOL("LabelEnabled", "Show labels", "Label", "True"),

    BOOL("SettingsEnabled", "Show settings icon", "Icons", "True"),
    TEXT("SettingsIcon",    "Settings icon",      "Icons",
         "~/.config/GnuChanDock/settings.png"),
    TEXT("SettingsLabel",   "Settings label",     "Icons", "settings"),

    BOOL("TerminalEnabled", "Show terminal icon", "Icons", "True"),
    TEXT("TerminalIcon",    "Terminal icon",      "Icons",
         "~/.config/GnuChanDock/logo.png"),
    TEXT("TerminalLabel",   "Terminal label",     "Icons", "terminal"),
    TEXT("TerminalCommand", "Terminal command",   "Icons", "GnuChanTerm"),

    BOOL("ShowRunning", "Show running programs", "Behaviour", "True"),
};

/* --- GnuChanTerm ----------------------------------------------------------- */

static const SettingDef term_settings[] = {
    TEXT("Font",   "Font",   "Font",   "monospace-11"),
    TEXT("Prompt", "Prompt", "Prompt", ""),

    COLOR("BarBackground", "Bar background", "Bar",    "#09030d"),
    COLOR("BarForeground", "Bar text",       "Bar",    "#ddb3ff"),
    COLOR("Cursor",        "Cursor colour",  "Cursor", "#ddb3ff"),
};

/* --- GnuChanFetch ---------------------------------------------------------- */

static const SettingDef fetch_settings[] = {
    TEXT("Image",            "Image",            "Image",
         "~/.config/GnuChanFetch/logo.png"),
    INT("ImageRows",         "Image height",     "Image", "16"),
    INT("Gap",               "Gap",              "Image", "3"),
    COLOR("ImageBackground", "Image background", "Image", "#0b0310"),
};

/* --- GnuChanSS ------------------------------------------------------------- */

static const SettingDef ss_settings[] = {
    CHOICE("Name", "Effect", "Effect", "pipe", effect_choices),
    COLOR("PrimaryColor",    "Primary colour",    "Effect",    "#d400ff"),
    COLOR("BackgroundColor", "Background colour", "Effect",    "#27022b"),
    INT("IdleSeconds",       "Idle seconds",      "Behaviour", "300"),
    BOOL("Inhibit",          "Inhibit on video",  "Behaviour", "True"),
};

/* --- GnuChanSL ------------------------------------------------------------- */

static const SettingDef sl_settings[] = {
    COLOR("Background", "Background",     "Palette", "#0d0512"),
    COLOR("Panel",      "Panel",          "Palette", "#1b0c22"),
    COLOR("PanelEdge",  "Panel edge",     "Palette", "#7b2cbf"),
    COLOR("Field",      "Field",          "Palette", "#241033"),
    COLOR("Text",       "Text",           "Palette", "#e0c3fc"),
    COLOR("TextMuted",  "Muted text",     "Palette", "#9d7bba"),
    COLOR("Accent",     "Accent",         "Palette", "#d400ff"),
    COLOR("Wrong",      "Wrong password", "Palette", "#ff4d6d"),

    TEXT("FontFamily", "Font family", "Font", "monospace"),
    INT("FontSize",    "Font size",   "Font", "14"),

    TEXT("Title",       "Title",           "Text", ""),
    TEXT("Subtitle",    "Subtitle",        "Text", "Enter your password"),
    TEXT("Prompt",      "Password prompt", "Text", "Password:"),
    TEXT("ClockFormat", "Clock format",    "Text", "%H:%M"),

    BOOL("ShowPasswordDots", "Show password dots",   "Behaviour", "True"),
    INT("WrongSeconds",      "Wrong-password flash", "Behaviour", "2"),
};

/* --- GnuChanNotification --------------------------------------------------- */

static const SettingDef notif_settings[] = {
    COLOR("Background",     "Background",     "Palette", "#1a0b2e"),
    COLOR("BackgroundAlt",  "Second band",    "Palette", "#241033"),
    COLOR("Frame",          "Frame",          "Palette", "#7b2cbf"),
    COLOR("Title",          "Title",          "Palette", "#e0c3fc"),
    COLOR("Body",           "Body",           "Palette", "#c9b6e4"),
    COLOR("Accent",         "Accent",         "Palette", "#c77dff"),
    COLOR("Urgent",         "Urgent",         "Palette", "#ff5c8a"),
    COLOR("IconBackground", "Icon background", "Palette", "#1a0b2e"),

    INT("Width",    "Width",         "Shape", "360"),
    INT("Padding",  "Padding",       "Shape", "14"),
    INT("Margin",   "Margin",        "Shape", "14"),
    INT("Gap",      "Gap",           "Shape", "10"),
    INT("Corner",   "Corner radius", "Shape", "14"),
    INT("IconSize", "Icon size",     "Shape", "48"),

    TEXT("Font",     "Font",       "Text", "monospace:pixelsize=13"),
    INT("BodyScale", "Body scale", "Text", "90"),

    CHOICE("Position", "Position", "Behaviour", "top-right", corner_choices),
    INT("Timeout",         "Timeout (ms)",    "Behaviour", "5000"),
    INT("MaxVisible",      "Max visible",     "Behaviour", "5"),
    BOOL("ShowIcon",       "Show icon",       "Behaviour", "True"),
    BOOL("ShowBody",       "Show body",       "Behaviour", "True"),
    INT("TopOffset",       "Top offset",      "Behaviour", "50"),
    BOOL("ShowTimer",      "Show timer",      "Behaviour", "True"),
    INT("MaxSummaryLines", "Max title lines", "Behaviour", "3"),
    INT("MaxBodyLines",    "Max body lines",  "Behaviour", "12"),
};

/* --- GnuChanRunner --------------------------------------------------------- */

static const SettingDef runner_settings[] = {
    TEXT("Prompt",       "Prompt",        "Words", "Run:"),
    TEXT("Placeholder",  "Placeholder",   "Words", "Type to search programs"),
    TEXT("EmptyMessage", "Empty message", "Words", "Nothing matches"),

    TEXT("FontFamily", "Font family", "Window", "monospace"),
    INT("FontSize",    "Font size",   "Window", "14"),
    INT("Width",       "Width",       "Window", "520"),
    INT("Rows",        "Rows",        "Window", "8"),
    CHOICE("Position", "Position",    "Window", "center", place_choices),

    COLOR("Background", "Background", "Palette", "#1a0b2e"),
    COLOR("Panel",      "Panel",      "Palette", "#32143f"),
    COLOR("PanelEdge",  "Panel edge", "Palette", "#7b2cbf"),
    COLOR("Field",      "Field",      "Palette", "#241033"),
    COLOR("Text",       "Text",       "Palette", "#e0c3fc"),
    COLOR("TextMuted",  "Muted text", "Palette", "#9d7bba"),
    COLOR("Accent",     "Accent",     "Palette", "#c77dff"),

    BOOL("CaseSensitive", "Case sensitive", "Behaviour", "False"),
    BOOL("Fuzzy",         "Fuzzy match",    "Behaviour", "True"),
    CHOICE("CommandMode", "Command line",   "Behaviour", "typed",
           command_choices),
    BOOL("ShowNoDisplay", "Show NoDisplay", "Behaviour", "False"),
    BOOL("ShowHidden",    "Show hidden",    "Behaviour", "False"),
};

/* --- GnuChanWifi -----------------------------------------------------------
 *
 * Only the settings a person would actually change. The program's file also
 * has NmcliPath and RestartModule, and neither is a field a panel should put in
 * front of anybody: NmcliPath is "nmcli" on every machine that has the program
 * at all, and RestartModule is read off the interface by the manager itself and
 * named only when that read fails. Offering them is offering a way to break a
 * working manager.
 */

static const SettingDef wifi_settings[] = {
    TEXT("Title",         "Title",          "Window", "Wi-Fi"),

    TEXT("FontFamily", "Font family", "Window", "monospace"),
    INT("FontSize",    "Font size",   "Window", "14"),
    INT("Width",       "Width",       "Window", "520"),
    INT("Rows",        "Rows",        "Window", "10"),

    COLOR("Background", "Background",     "Palette", "#1a0b2e"),
    COLOR("Panel",      "Panel",          "Palette", "#32143f"),
    COLOR("PanelEdge",  "Panel edge",     "Palette", "#7b2cbf"),
    COLOR("Field",      "Field",          "Palette", "#241033"),
    COLOR("Text",       "Text",           "Palette", "#e0c3fc"),
    COLOR("TextMuted",  "Muted text",     "Palette", "#9d7bba"),
    COLOR("Accent",     "Accent",         "Palette", "#c77dff"),
    COLOR("Secured",    "Secured mark",   "Palette", "#ff9e64"),
    COLOR("Connected",  "Connected mark", "Palette", "#9ece6a"),
    COLOR("Selection",  "Selection",      "Palette", "#5a2a8f"),
};

/* --- GnuChanTop ------------------------------------------------------------ */

static const SettingDef top_settings[] = {
    REAL("UpdateTime", "Update time (s)", "Sampling", "1.0"),
    INT("ProcLimit",   "Process limit",   "Sampling", "200"),
    BOOL("ShowGPU",    "Show GPU",        "Display",  "true"),
    CHOICE("Sort",     "Sort by",         "Display",  "cpu", sort_choices),
};

/* --- GnuChanDM ------------------------------------------------------------- */

static const SettingDef dm_settings[] = {
    COLOR("Background", "Background",    "Palette",  "#1a0b2e"),
    COLOR("Panel",      "Panel",         "Palette",  "#32143f"),
    COLOR("PanelEdge",  "Panel edge",    "Palette",  "#7b2cbf"),
    COLOR("Field",      "Field",         "Palette",  "#241033"),
    COLOR("FieldFocus", "Focused field", "Palette",  "#3a1a52"),
    COLOR("Text",       "Text",          "Palette",  "#e0c3fc"),
    COLOR("TextMuted",  "Muted text",    "Palette",  "#9d7bba"),
    COLOR("Accent",     "Accent",        "Palette",  "#c77dff"),
    COLOR("AccentDim",  "Accent (idle)", "Palette",  "#7b2cbf"),
    COLOR("Danger",     "Danger",        "Palette",  "#ff5d8f"),

    TEXT("FontLabel", "Label font", "Fonts",
         "-*-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1"),
    TEXT("FontField", "Field font", "Fonts",
         "-*-helvetica-medium-r-normal--18-*-*-*-*-*-iso8859-1"),
    TEXT("FontTitle", "Title font", "Fonts",
         "-*-helvetica-bold-r-normal--28-*-*-*-*-*-iso8859-1"),

    INT("Margin",       "Margin",        "Measurements", "16"),
    INT("Gap",          "Gap",           "Measurements", "12"),
    INT("PanelWidth",   "Panel width",   "Measurements", "420"),
    INT("FieldHeight",  "Field height",  "Measurements", "40"),
    INT("ButtonHeight", "Button height", "Measurements", "40"),
};

/* --- GnuChanWM -------------------------------------------------------------
 *
 * The odd one: it writes its borders as gcl_Window.set_*(...) calls, the
 * switcher's colours as gcl_Switcher.* assignments, and its wallpaper as a
 * gcl_Window.* assignment — all in the same file. So every row here names its
 * own style and prefix rather than following the program's. */

static const SettingDef wm_settings[] = {
    ROW_IN("set_active_window_border_color", "Active border", "Borders",
           SETTING_COLOR, SETTING_FUNC, "gcl_Window", "#d400ff"),
    ROW_IN("set_inactive_window_border_color", "Inactive border", "Borders",
           SETTING_COLOR, SETTING_FUNC, "gcl_Window", "#450552"),
    ROW_IN("set_moved_window_border_color", "Moved border", "Borders",
           SETTING_COLOR, SETTING_FUNC, "gcl_Window", "#52024d"),
    ROW_IN("set_inactive_window_panel_color", "Inactive title bar", "Borders",
           SETTING_COLOR, SETTING_FUNC, "gcl_Window", "#241033"),
    ROW_IN("set_window_border_width", "Border width", "Borders",
           SETTING_INT, SETTING_FUNC, "gcl_Window", "2"),

    ROW_IN("background_color", "Desktop colour", "Desktop",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Window", "#27022b"),
    ROW_IN("BackgroundImage", "Wallpaper", "Desktop",
           SETTING_TEXT, SETTING_ASSIGN, "gcl_Window",
           "~/.config/GnuChanWM/bg.png"),

    ROW_IN("background", "Switcher background", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#0d0512"),
    ROW_IN("panel", "Switcher panel", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#1b0c22"),
    ROW_IN("cell_border", "Cell border", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#7b2cbf"),
    ROW_IN("select_border", "Selected border", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#d400ff"),
    ROW_IN("text", "Cell text", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#e0c3fc"),
    ROW_IN("select_text", "Selected text", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#d400ff"),
    ROW_IN("field", "Hover field", "Switcher",
           SETTING_COLOR, SETTING_ASSIGN, "gcl_Switcher", "#2a1035"),
};

/* --- the table ------------------------------------------------------------- */

static const AppDef app_table[] = {
    {
        "GnuChanDock", "The desktop dock",
        "GnuChanDock", "GnuChanDock.py", "gcl_Dock.Main",
        SETTING_CALL, 1,
        dock_settings, (int)(sizeof(dock_settings) / sizeof(dock_settings[0])),
    },
    {
        "GnuChanTerm", "The terminal",
        "GnuChanTerm", "GnuChanTerm.py", "gcl_Terminal.call",
        SETTING_CALL, 1,
        term_settings, (int)(sizeof(term_settings) / sizeof(term_settings[0])),
    },
    {
        "GnuChanFetch", "The system information program",
        "GnuChanFetch", "GnuChanFetch.py", "gcl_Fetch.Main",
        SETTING_CALL, 1,
        fetch_settings, (int)(sizeof(fetch_settings) / sizeof(fetch_settings[0])),
    },
    {
        "GnuChanSS", "The screen saver",
        "GnuChanSS", "GnuChanSS.py", "gcl_SS.Effect",
        SETTING_CALL, 1,
        ss_settings, (int)(sizeof(ss_settings) / sizeof(ss_settings[0])),
    },
    {
        "GnuChanSL", "The lock screen",
        "GnuChanSL", "GnuChanSL.py", "gcl_SL.Lock",
        SETTING_CALL, 1,
        sl_settings, (int)(sizeof(sl_settings) / sizeof(sl_settings[0])),
    },
    {
        "GnuChanNotification", "The notification bubbles",
        "GnuChanNotification", "GnuChanNotification.py", "gcl_Notification.Main",
        SETTING_CALL, 1,
        notif_settings, (int)(sizeof(notif_settings) / sizeof(notif_settings[0])),
    },
    {
        "GnuChanRunner", "The program launcher",
        "GnuChanRunner", "config.py", "",
        SETTING_FLAT, 0,
        runner_settings, (int)(sizeof(runner_settings) / sizeof(runner_settings[0])),
    },
    {
        "GnuChanWifi", "The Wi-Fi manager",
        "GnuChanWifi", "config.py", "",
        SETTING_FLAT, 0,
        wifi_settings, (int)(sizeof(wifi_settings) / sizeof(wifi_settings[0])),
    },
    {
        "GnuChanTop", "The system monitor",
        "GnuChanTop", "GnuChanTop.py", "",
        SETTING_FLAT, 0,
        top_settings, (int)(sizeof(top_settings) / sizeof(top_settings[0])),
    },
    {
        "GnuChanDM", "The login screen",
        "GnuChanDM", "GnuChanDM.py", "gcl_DM.call",
        SETTING_CALL, 1,
        dm_settings, (int)(sizeof(dm_settings) / sizeof(dm_settings[0])),
    },
    {
        "GnuChanWM", "The window manager",
        "GnuChanWM", "GnuChanWM.py", "gcl_Window",
        SETTING_FUNC, 1,
        wm_settings, (int)(sizeof(wm_settings) / sizeof(wm_settings[0])),
    },
};

const AppDef *settings_apps(int *count) {
    if (count) {
        *count = (int)(sizeof(app_table) / sizeof(app_table[0]));
    }
    return app_table;
}

/* --- choice rows ----------------------------------------------------------- */

const char *const *settings_choices(const SettingDef *def) {
    if (!def || def->type != SETTING_CHOICE) {
        return NULL;
    }
    return def->choices;
}

int settings_choice_count(const SettingDef *def) {
    const char *const *list = settings_choices(def);
    int count = 0;
    if (!list) {
        return -1;
    }
    while (list[count]) {
        count++;
    }
    return count;
}

int settings_choice_index(const char *const *choices, const char *text) {
    if (!choices || !text) {
        return -1;
    }
    for (int i = 0; choices[i]; i++) {
        if (strcmp(choices[i], text) == 0) {
            return i;
        }
    }
    return -1;
}
