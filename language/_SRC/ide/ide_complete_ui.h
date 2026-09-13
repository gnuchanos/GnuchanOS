/*
 * ide_complete_ui.h — Tamamlama popup'i + imza yardimi cizimi (todo.md §13).
 *
 * Motor (complete/) ve editor karar verir; bu modul YALNIZCA cizer:
 *   - popup listesi (tur ikonlari ile, emoji DEGIL — vektor glifler)
 *   - secili ogelerin imza/detay paneli (doc paneli)
 *   - aktif cagrinin imza serididi (signature help, §11.2)
 */
#ifndef GCL_IDE_COMPLETE_UI_H
#define GCL_IDE_COMPLETE_UI_H

#include "raylib.h"
#include "gcl_ide_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Editor Editor;

/* Tamamlama penceresini ve (varsa) imza seridini cizer.
   editor_rect : kod alani dikdortgeni (popup bu alana sigdirilir)
   efont       : editor font boyutu */
void ide_complete_ui_draw(Editor *ed, Rectangle editor_rect, int efont,
                          const GclIdeTheme *t);

#ifdef __cplusplus
}
#endif

#endif /* GCL_IDE_COMPLETE_UI_H */
