/*
 * gcl_settings_panel.h — Bağımsız ayarlar paneli modülü.
 *
 * Panel kendi state'ini (seçili sekme, scroll, dropdown) BU modül içinde tutar;
 * Editor struct'ına hiçbir UI state'i yazılmaz. Bu, panelin defalarca
 * "bozulmasına" yol açan hardcode tasarım hatasını kökten çözer.
 */

#ifndef GCL_SETTINGS_PANEL_H
#define GCL_SETTINGS_PANEL_H

#include "gcl_ide_settings.h"
#include "raylib.h"

typedef struct Editor Editor;

/* Ayar paneli hayat döngüsü.
   open(): ed->settings'ten scratch kopyalar; ed->settings_open = 1.
   close(): paneli kapatır, scratch'ı atar.
   is_open(): panel görünür mü? */
void gcl_settings_panel_open(Editor *ed);
void gcl_settings_panel_close(Editor *ed);
int  gcl_settings_panel_is_open(void);

/* Panel açıkken klavye girişi (ESC / Ctrl+S).
   1 döndürürse panel kapatıldı; main loop state'i günceller. */
int  gcl_settings_panel_input(Editor *ed, int ctrl);

/* Paneli çizer — ed->settings_open true iken çağrılır. */
void gcl_settings_panel_draw(Editor *ed, int w, int h, int font_sz, GclIdeTheme *t);

/* About diyaloğu (aynı modülde tutulur). */
void draw_about_panel(Editor *ed, int w, int h, int font_sz, GclIdeTheme *t);

#endif /* GCL_SETTINGS_PANEL_H */
