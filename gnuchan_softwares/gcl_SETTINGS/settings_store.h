/*
 * settings_store.h — reading a program's settings file, and writing it back.
 *
 * This is the half of GnuChanSettings that touches the disk, and the whole of
 * its promise is in one sentence: it changes the VALUE of the settings it was
 * asked to change and leaves every other byte of the file exactly as it found
 * it. The comments a person wrote, the settings it does not know, the lines it
 * has no business touching — all of them survive a save untouched.
 *
 * The file is read as LINES and written as LINES. A setting is found by the
 * token that names it (see SettingDef.key) and the value between the token and
 * the end of its argument is the only span replaced. That is what makes a file
 * this program has edited still the file a person would want to edit by hand.
 *
 * Nothing here knows about X.
 */
#ifndef GNUCHANSETTINGS_STORE_H
#define GNUCHANSETTINGS_STORE_H

#include <stddef.h>

#include "settings_types.h"

/* One setting's value, as the window holds it and the file carries it. `text`
   is the value WITHOUT its quotes — what the person reads and types — and it
   is the one field the window edits. `present` says whether the file named the
   setting at all; a setting that is absent still has a value (the schema's
   fallback) so the window always has something to show. */
typedef struct SettingValue {
    char text[SETTINGS_TEXT_LENGTH];
    int  present;
} SettingValue;

/* The full path of a program's settings file: $XDG_CONFIG_HOME/<dir>/<file>,
   or ~/.config/<dir>/<file> when XDG_CONFIG_HOME is not set. Written into
   `out`, which is returned. An empty string when there is no home to build it
   from. */
char *settings_path(char *out, unsigned int size, const AppDef *app);

/* Fill `values` — one per app->settings row — from the program's file.
 *
 * Every row gets a value: the one the file writes, or the schema's fallback
 * when the file does not name it. A file that is missing is NOT an error here:
 * the panel must open on a fresh install, showing the defaults the program
 * itself would use, and the save is what creates the file. Returns 1 when a
 * file was read, 0 when there was none, -1 on an error worth reporting. */
int settings_load(const AppDef *app, SettingValue *values);

/* Write `values` back into the program's file.
 *
 * Only the values change: every line the panel did not touch is copied
 * through, byte for byte, and a file that did not exist is created with the
 * program's own call around the values so it is a file the program can read.
 * The write is atomic — a temporary file beside the target, then a rename — so
 * a crash mid-save leaves the old settings, never half of a new file.
 *
 * Returns 0 on success, -1 on an error, with `reason` filled in (it must hold
 * at least SETTINGS_TEXT_LENGTH bytes). */
int settings_save(const AppDef *app, const SettingValue *values,
                  char *reason);

/* The value of a row as a number or a switch. A text that is not the type the
   row wants becomes 0, which is what the reader would do with it. */
int setting_value_int(const SettingValue *value);
int setting_value_bool(const SettingValue *value);

/* Write `number`/`flag` into a value's text in the form the app's file uses —
   capital True/False or lower — so the writer and the reader agree. */
void setting_value_set_int(SettingValue *value, int number);
void setting_value_set_bool(SettingValue *value, int flag, int capital);

#endif /* GNUCHANSETTINGS_STORE_H */
