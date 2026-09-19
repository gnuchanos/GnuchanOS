/*
 * gcl_ide_settings.h — IDE ayar ve tema tipleri.
 */

#ifndef GCL_IDE_SETTINGS_H
#define GCL_IDE_SETTINGS_H

#include "raylib.h"

typedef struct {
    Color bg;
    Color text;
    Color line_bg;
    Color gutter;
    Color cursor;
    Color accent;
    Color menu_bg;
    Color status_bg;
    Color status_text;
    Color panel_bg;
    Color popup_bg;
    Color selection;
    Color error;
    Color ok;
    Color tree_bg;
} GclIdeTheme;

typedef struct {
    int font_size;
    int theme_index;
    int syntax_highlight;
    int vhs;
    int crt;
    int screen_shake;
    int typewriter;
    int particles;
    int blink_cursor;
    int text_bloom;
    int bloom_strength;
    int shake_strength;
    int particle_strength;
    int vhs_strength;
    int crt_strength;
    int sound;
} GclIdeSettings;

typedef struct {
    const char *name;
    GclIdeTheme theme;
} GclIdeThemeEntry;

const GclIdeThemeEntry *gcl_ide_theme_entry(int index);
int gcl_ide_theme_count(void);
GclIdeTheme gcl_ide_theme_get(int index);

int gcl_ide_settings_load(GclIdeSettings *s);
int gcl_ide_settings_save(const GclIdeSettings *s);

#endif /* GCL_IDE_SETTINGS_H */
