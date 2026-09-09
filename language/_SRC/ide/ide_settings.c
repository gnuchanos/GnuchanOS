/*
 * ide_settings.c — GCL IDE tema paletleri ve ide.gcsettings yükle/kaydet.
 *
 * Default tema: mor (purple). 8 tema.
 * Tüm renk değerleri index tabanlı saklanır; tema palet adı da kaydedilir.
 */

#include "gcl_ide_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_THEMES 16

static const GclIdeThemeEntry g_themes[] = {
    { "Mor (Purple)", {
        { 20, 14, 44, 255 },       /* bg          */
        { 234, 222, 255, 255 },    /* text        */
        { 28, 20, 58, 255 },       /* line_bg     */
        { 150, 120, 225, 255 },    /* gutter      */
        { 216, 150, 255, 255 },    /* cursor      */
        { 180, 122, 255, 255 },    /* accent      */
        { 26, 18, 56, 255 },       /* menu_bg     */
        { 16, 10, 38, 255 },       /* status_bg   */
        { 216, 198, 255, 255 },    /* status_text */
        { 34, 26, 72, 255 },       /* panel_bg    */
        { 62, 44, 124, 255 },      /* popup_bg    */
        { 142, 90, 252, 255 },     /* selection   */
        { 255, 90, 90, 255 },      /* error       */
        { 90, 220, 110, 255 },     /* ok          */
        { 30, 22, 66, 255 }        /* tree_bg     */
    }},
    { "Siyah (Black)", {
        { 8, 8, 8, 255 },
        { 220, 220, 220, 255 },
        { 14, 14, 14, 255 },
        { 100, 100, 100, 255 },
        { 255, 200, 60, 255 },
        { 61, 150, 245, 255 },
        { 12, 12, 12, 255 },
        { 6, 6, 6, 255 },
        { 200, 200, 200, 255 },
        { 22, 22, 22, 255 },
        { 32, 32, 32, 255 },
        { 40, 90, 180, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 16, 16, 16, 255 }
    }},
    { "Beyaz (White)", {
        { 240, 240, 240, 255 },
        { 25, 25, 25, 255 },
        { 232, 232, 232, 255 },
        { 120, 120, 120, 255 },
        { 200, 60, 60, 255 },
        { 39, 111, 209, 255 },
        { 226, 226, 226, 255 },
        { 218, 218, 218, 255 },
        { 25, 25, 25, 255 },
        { 218, 218, 218, 255 },
        { 232, 232, 232, 255 },
        { 150, 190, 240, 255 },
        { 220, 60, 60, 255 },
        { 50, 160, 70, 255 },
        { 228, 228, 228, 255 }
    }},
    { "Mavi (Blue)", {
        { 8, 16, 36, 255 },
        { 220, 230, 250, 255 },
        { 12, 22, 44, 255 },
        { 100, 130, 180, 255 },
        { 80, 180, 255, 255 },
        { 61, 150, 245, 255 },
        { 10, 18, 42, 255 },
        { 6, 12, 30, 255 },
        { 200, 210, 230, 255 },
        { 18, 30, 64, 255 },
        { 30, 48, 92, 255 },
        { 45, 100, 180, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 14, 24, 52, 255 }
    }},
    { "Turuncu (Orange)", {
        { 28, 18, 10, 255 },
        { 240, 225, 210, 255 },
        { 36, 24, 12, 255 },
        { 180, 130, 80, 255 },
        { 255, 160, 60, 255 },
        { 255, 130, 40, 255 },
        { 32, 20, 12, 255 },
        { 22, 14, 8, 255 },
        { 230, 200, 170, 255 },
        { 44, 30, 16, 255 },
        { 62, 42, 24, 255 },
        { 190, 110, 40, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 36, 24, 12, 255 }
    }},
    { "Yeşil (Green)", {
        { 12, 28, 18, 255 },
        { 220, 240, 225, 255 },
        { 16, 36, 24, 255 },
        { 110, 180, 130, 255 },
        { 90, 255, 140, 255 },
        { 60, 200, 110, 255 },
        { 14, 32, 20, 255 },
        { 8, 22, 14, 255 },
        { 200, 230, 205, 255 },
        { 22, 44, 30, 255 },
        { 36, 62, 42, 255 },
        { 50, 150, 90, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 18, 36, 24, 255 }
    }},
    { "Kırmızı (Red)", {
        { 30, 10, 10, 255 },
        { 245, 220, 220, 255 },
        { 38, 14, 14, 255 },
        { 190, 100, 100, 255 },
        { 255, 90, 90, 255 },
        { 230, 60, 60, 255 },
        { 34, 12, 12, 255 },
        { 24, 8, 8, 255 },
        { 230, 200, 200, 255 },
        { 48, 20, 20, 255 },
        { 68, 28, 28, 255 },
        { 180, 50, 50, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 40, 16, 16, 255 }
    }},
    { "Gri (Gray)", {
        { 24, 24, 24, 255 },
        { 220, 220, 220, 255 },
        { 30, 30, 30, 255 },
        { 130, 130, 130, 255 },
        { 255, 200, 60, 255 },
        { 180, 180, 180, 255 },
        { 28, 28, 28, 255 },
        { 18, 18, 18, 255 },
        { 200, 200, 200, 255 },
        { 36, 36, 36, 255 },
        { 48, 48, 48, 255 },
        { 90, 90, 90, 255 },
        { 255, 90, 90, 255 },
        { 90, 220, 110, 255 },
        { 30, 30, 30, 255 }
    }},
};

static int g_theme_count = (int)(sizeof(g_themes) / sizeof(g_themes[0]));

const GclIdeThemeEntry *gcl_ide_theme_entry(int index) {
    if (index < 0 || index >= g_theme_count) return NULL;
    return &g_themes[index];
}

int gcl_ide_theme_count(void) {
    return g_theme_count;
}

GclIdeTheme gcl_ide_theme_get(int index) {
    if (index < 0 || index >= g_theme_count) index = 0;
    return g_themes[index].theme;
}

/* ---------- settings load/save ---------- */

static const char *settings_path(void) {
    static char path[4096];
    const char *cwd = getenv("GCL_IDE_SETTINGS");
    if (cwd && cwd[0]) {
        snprintf(path, sizeof(path), "%s", cwd);
        return path;
    }
    snprintf(path, sizeof(path), "ide.gcsettings");
    return path;
}

int gcl_ide_settings_load(GclIdeSettings *s) {
    if (!s) return -1;
    s->font_size = 14;
    s->theme_index = 0;
    s->syntax_highlight = 1;
    /* Font kalitesini korumak için tüm heavy efektler default KAPALI (ridiculous_coding yaklaşımı) */
    s->vhs = 0;
    s->crt = 0;
    s->screen_shake = 0;
    s->typewriter = 0;
    s->particles = 0;
    s->blink_cursor = 1;
    s->text_bloom = 0;
    s->bloom_strength = 50;
    s->shake_strength = 30;        /* rahatsız edici olmasın: varsayılan düşük */
    s->particle_strength = 40;
    s->vhs_strength = 30;
    s->crt_strength = 30;
    s->sound = 0;                  /* daktilo sesi default kapalı (monotonluk) */
    FILE *f = fopen(settings_path(), "r");
    if (!f) return -1;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (strncmp(line, "font=", 5) == 0) s->font_size = atoi(line + 5);
        else if (strncmp(line, "theme=", 6) == 0) s->theme_index = atoi(line + 6);
        else if (strncmp(line, "syntax_highlight=", 17) == 0) s->syntax_highlight = atoi(line + 17);
        else if (strncmp(line, "vhs=", 4) == 0) s->vhs = atoi(line + 4);
        else if (strncmp(line, "crt=", 4) == 0) s->crt = atoi(line + 4);
        else if (strncmp(line, "screen_shake=", 13) == 0) s->screen_shake = atoi(line + 13);
        else if (strncmp(line, "typewriter=", 11) == 0) s->typewriter = atoi(line + 11);
        else if (strncmp(line, "particles=", 10) == 0) s->particles = atoi(line + 10);
        else if (strncmp(line, "blink_cursor=", 13) == 0) s->blink_cursor = atoi(line + 13);
        else if (strncmp(line, "text_bloom=", 11) == 0) s->text_bloom = atoi(line + 11);
        else if (strncmp(line, "bloom_strength=", 15) == 0) s->bloom_strength = atoi(line + 15);
        else if (strncmp(line, "shake_strength=", 15) == 0) s->shake_strength = atoi(line + 15);
        else if (strncmp(line, "particle_strength=", 18) == 0) s->particle_strength = atoi(line + 18);
        else if (strncmp(line, "vhs_strength=", 13) == 0) s->vhs_strength = atoi(line + 13);
        else if (strncmp(line, "crt_strength=", 13) == 0) s->crt_strength = atoi(line + 13);
        else if (strncmp(line, "sound=", 6) == 0) s->sound = atoi(line + 6);
    }
    fclose(f);
    if (s->theme_index < 0 || s->theme_index >= g_theme_count) s->theme_index = 0;
    if (s->font_size < 8) s->font_size = 8;
    if (s->font_size > 32) s->font_size = 32;
    if (s->shake_strength < 0) s->shake_strength = 0;
    if (s->shake_strength > 100) s->shake_strength = 100;
    if (s->particle_strength < 0) s->particle_strength = 0;
    if (s->particle_strength > 100) s->particle_strength = 100;
    if (s->vhs_strength < 0) s->vhs_strength = 0;
    if (s->vhs_strength > 100) s->vhs_strength = 100;
    if (s->crt_strength < 0) s->crt_strength = 0;
    if (s->crt_strength > 100) s->crt_strength = 100;
    return 0;
}

int gcl_ide_settings_save(const GclIdeSettings *s) {
    if (!s) return -1;
    FILE *f = fopen(settings_path(), "w");
    if (!f) return -1;
    fprintf(f, "font=%d\n", s->font_size);
    fprintf(f, "theme=%d\n", s->theme_index);
    fprintf(f, "syntax_highlight=%d\n", s->syntax_highlight);
    fprintf(f, "vhs=%d\n", s->vhs);
    fprintf(f, "crt=%d\n", s->crt);
    fprintf(f, "screen_shake=%d\n", s->screen_shake);
    fprintf(f, "typewriter=%d\n", s->typewriter);
    fprintf(f, "particles=%d\n", s->particles);
    fprintf(f, "blink_cursor=%d\n", s->blink_cursor);
    fprintf(f, "text_bloom=%d\n", s->text_bloom);
    fprintf(f, "bloom_strength=%d\n", s->bloom_strength);
    fprintf(f, "shake_strength=%d\n", s->shake_strength);
    fprintf(f, "particle_strength=%d\n", s->particle_strength);
    fprintf(f, "vhs_strength=%d\n", s->vhs_strength);
    fprintf(f, "crt_strength=%d\n", s->crt_strength);
    fprintf(f, "sound=%d\n", s->sound);
    fclose(f);
    return 0;
}
