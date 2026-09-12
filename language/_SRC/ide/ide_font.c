#include "gcl_ide_internal.h"

/* gcl_draw_text/gcl_measure_text tanımları raylib'in orijinal DrawText/MeasureText'ini kullanır;
   header'daki makro eşlemesini sadece burada devre dışı bırak (recursion'ı önle). */
#undef DrawText
#undef MeasureText

Font g_font = { 0 };
int g_bloom = 0;
int g_line_h = 16;

/* Monospace sabit adım: her karakter "M" genişliğinde çizilir.
   DrawTextEx tek parça çağrıldığında glyph advance birikimi ile MeasureTextEx
   arasında kümülatif sapma oluşur (özellikle `,` gibi kesirli advance'lı sembollerde).
   Karakter karakter çizim + "M" genişliği kullanmak ölçümü çizimle birebir eşleştirir. */
static int utf8_char_len2(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8_char_count2(const char *s) {
    int count = 0;
    for (const char *p = s; *p; ) {
        p += utf8_char_len2((unsigned char)*p);
        count++;
    }
    return count;
}

static float mono_cw(int fontSize) {
    if (g_font.texture.id > 0) {
        Vector2 sz = MeasureTextEx(g_font, "M", (float)fontSize, 0.0f);
        return sz.x;
    }
    return (float)MeasureText("M", fontSize);
}

/* Bir karakterin kac hucre ilerlettigi. '\t' bir sonraki TAB duragina
   (GCL_TAB_SIZE) kadar ilerletir. Python dosyalari girinti icin sekme
   karakteri kullanabilir; cizim ve olcum ayni kurali kullanmazsa
   imlec metinden kayar. */
static int gcl_tab_advance(char c, int col) {
    if (c != '\t') return 1;
    int step = GCL_TAB_SIZE - (col % GCL_TAB_SIZE);
    return step > 0 ? step : GCL_TAB_SIZE;
}

/* 'text'in 'start_col' hucre kolonundan itibaren kapladigi hucre sayisi. */
int gcl_text_cells(const char *text, int start_col) {
    if (!text) return 0;
    int col = start_col;
    for (const char *p = text; *p; ) {
        col += gcl_tab_advance(*p, col);
        p += utf8_char_len2((unsigned char)*p);
    }
    return col - start_col;
}

/* Tek karakteri x + i*cw konumuna çiz (UTF-8 güvenli). */
static void draw_mono_char(const char *s, float x, float y, int fontSize, Color color) {
    char tmp[5];
    int len = utf8_char_len2((unsigned char)s[0]);
    memcpy(tmp, s, (size_t)len); tmp[len] = '\0';
    Vector2 pos = { x, y };
    if (g_font.texture.id > 0) DrawTextEx(g_font, tmp, pos, (float)fontSize, 0.0f, color);
    else DrawText(tmp, (int)x, (int)y, fontSize, color);
}

/* Sekme duyarli cizim: 'start_col' = metnin basladigi hucre kolonu.
   '\t' ekrana cizilmez; bir sonraki TAB duragina kadar bosluk birakir.
   Boylece sekme ile girintilenmis Python dosyalari hizali gorunur. */
void gcl_draw_text_col(const char *text, float posX, int posY, int fontSize, Color color, int start_col) {
    if (!text) return;
    float cw = mono_cw(fontSize);
    float y0 = (float)posY;
    int col = start_col;
    int bloom = (g_bloom > 0);
    Color soft = color;
    if (bloom) soft.a = (unsigned char)(color.a * (g_bloom / 100.0f) * 0.6f);
    float x = posX;
    for (const char *p = text; *p; ) {
        int adv = gcl_tab_advance(*p, col);
        if (*p != '\t') {
            if (bloom) {
                draw_mono_char(p, x - 1, y0, fontSize, soft);
                draw_mono_char(p, x + 1, y0, fontSize, soft);
                draw_mono_char(p, x, y0 - 1, fontSize, soft);
                draw_mono_char(p, x, y0 + 1, fontSize, soft);
            }
            draw_mono_char(p, x, y0, fontSize, color);
        }
        col += adv;
        x += cw * (float)adv;
        p += utf8_char_len2((unsigned char)*p);
    }
}

void gcl_draw_text_f(const char *text, float posX, int posY, int fontSize, Color color) {
    gcl_draw_text_col(text, posX, posY, fontSize, color, 0);
}

void gcl_draw_text(const char *text, int posX, int posY, int fontSize, Color color) {
    gcl_draw_text_col(text, (float)posX, posY, fontSize, color, 0);
}

/* Ölçüm: monospace sabit adım (cw * karakter sayısı).
   DrawTextEx'in tek parça advance birikimi ile MeasureTextEx arasındaki kümülatif
   sapmayı tamamen ortadan kaldırır; ölçüm artık çizimle birebir eşleşir. */
static float measure_mono(const char *text, int fontSize, int start_col) {
    if (!text) return 0.0f;
    float cw = mono_cw(fontSize);
    return cw * (float)gcl_text_cells(text, start_col);
}

int gcl_measure_text(const char *text, int fontSize) {
    float w = measure_mono(text, fontSize, 0);
    return (int)(w + 0.5f);
}

/* Gerçek float genişlik — syntax highlight chunk çiziminde int truncate/round hatası
   birikmesin diye kullanılır. Monospace sabit adımla çizimle birebir eşleşir. */
float gcl_measure_text_f(const char *text, int fontSize) {
    return measure_mono(text, fontSize, 0);
}

/* Sekme duyarli olcum: chunk'lar satir basindan farkli kolonlarda baslayabilir. */
float gcl_measure_text_col(const char *text, int fontSize, int start_col) {
    return measure_mono(text, fontSize, start_col);
}

char *gcl_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

GclIdeBuffer *editor_cur(Editor *ed) {
    return ed->active_tab >= 0 ? &ed->tabs[ed->active_tab] : &ed->blank_tab;
}

const char *gcl_keywords[] = {
    /* veri tipleri */
    "int","short","long","float","double","void","char","bool",
    /* kontrol akışı */
    "return","if","else","while","for","break","continue",
    "switch","case","default","do","goto",
    /* yapılar */
    "struct","enum","typedef","const","sizeof",
    /* görünürlük / yaşam */
    "global","local","inline","public","private",
    /* yerleşik fonksiyonlar / kavramlar */
    "printf","scanf","strlen","true","false","null",
    NULL
};

const char *lua_keywords[] = {
    "and","break","do","else","elseif","end","false","for","function","goto",
    "if","in","local","nil","not","or","repeat","return","then","true","until","while",
    NULL
};

const char *python_keywords[] = {
    "and","as","assert","async","await","break","class","continue","def","del","elif",
    "else","except","finally","for","from","global","if","import","in","is","lambda",
    "nonlocal","not","or","pass","raise","return","try","while","with","yield",
    "None","True","False","self","cls",
    NULL
};

int gcl_is_keyword_for(const char *w, const char *lang) {
    if (!w) return 0;
    const char **list;
    if (lang && strcmp(lang, "python") == 0) list = python_keywords;
    else if (lang && strcmp(lang, "lua") == 0) list = lua_keywords;
    else list = gcl_keywords;
    for (int i = 0; list[i]; i++) {
        if (strcmp(w, list[i]) == 0) return 1;
    }
    return 0;
}

int gcl_is_keyword(const char *w) { return gcl_is_keyword_for(w, "gcl"); }

int gcl_is_digit(char c) { return c >= '0' && c <= '9'; }
int gcl_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int gcl_ident_char(char c) { return gcl_is_alpha(c) || gcl_is_digit(c); }

/* Token türleri: 0=plain, 1=keyword, 2=string, 3=char, 4=number, 5=comment, 6=preproc, 7=ident,
                   8=function, 9=class, 10=self/cls */
int gcl_token_type(const char *line, size_t len, size_t start, size_t *tok_len, const char *lang) {
    (void)len;
    char c = line[start];
    int is_py = lang && strcmp(lang, "python") == 0;
    int is_lua = lang && strcmp(lang, "lua") == 0;

    /* Python: # yorum (GCL'de preproc değil) */
    if (is_py && c == '#') {
        size_t i = start;
        while (i < len && line[i] != '\n' && line[i] != '\r') i++;
        *tok_len = i - start;
        return 5;
    }
    /* Lua: -- yorum */
    if (is_lua && c == '-' && start + 1 < len && line[start+1] == '-') {
        size_t i = start;
        while (i < len && line[i] != '\n' && line[i] != '\r') i++;
        *tok_len = i - start;
        return 5;
    }
    /* GCL preproc / include */
    if (c == '#') {
        size_t i = start;
        while (i < len && line[i] != '\n' && line[i] != '\r') i++;
        *tok_len = i - start;
        return 6;
    }
    /* Triple-quote string: """ ... """ veya ''' ... ''' (Python) */
    if ((c == '"' || c == '\'') && start + 2 < len && line[start+1] == c && line[start+2] == c) {
        size_t i = start + 3;
        while (i + 2 < len) {
            if (line[i] == c && line[i+1] == c && line[i+2] == c) { i += 3; break; }
            i++;
        }
        *tok_len = i - start;
        return 2;
    }
    /* string */
    if (c == '"' || c == '\'') {
        char q = c;
        size_t i = start + 1;
        while (i < len) {
            if (line[i] == '\\' && i + 1 < len) { i += 2; continue; }
            if (line[i] == q) { i++; break; }
            i++;
        }
        *tok_len = i - start;
        return c == '"' ? 2 : 3;
    }
    /* number */
    if (gcl_is_digit(c)) {
        size_t i = start;
        while (i < len && (gcl_is_digit(line[i]) || line[i] == '.' || line[i] == 'x' ||
                           line[i] == 'X' || (line[i] >= 'a' && line[i] <= 'f') ||
                           (line[i] >= 'A' && line[i] <= 'F'))) i++;
        *tok_len = i - start;
        return 4;
    }
    /* identifier / keyword */
    if (gcl_is_alpha(line[start]) || c == '_') {
        size_t i = start;
        while (i < len && gcl_ident_char(line[i])) i++;
        *tok_len = i - start;
        char word[256];
        size_t wl = i - start;
        if (wl >= sizeof(word)) wl = sizeof(word) - 1;
        memcpy(word, line + start, wl);
        word[wl] = '\0';

        /* Önceki anlamlı kelime def/class ise, bu identifier function/class adıdır */
        if ((is_py || is_lua) && start > 0) {
            size_t p = start;
            while (p > 0 && (line[p-1] == ' ' || line[p-1] == '\t')) p--;
            if (p > 0 && gcl_ident_char(line[p-1])) {
                size_t q = p;
                while (q > 0 && gcl_ident_char(line[q-1])) q--;
                char pword[256];
                size_t pw = p - q;
                if (pw < sizeof(pword)) {
                    memcpy(pword, line + q, pw); pword[pw] = '\0';
                    if (strcmp(pword, "def") == 0) return 8;
                    if (strcmp(pword, "class") == 0) return 9;
                }
            }
        }
        /* __init__ gibi dunder metod adları → function */
        if (is_py && wl >= 4 && word[0] == '_' && word[1] == '_' &&
            word[wl-1] == '_' && word[wl-2] == '_') {
            return 8;
        }
        return gcl_is_keyword_for(word, lang) ? 1 : 7;
    }
    *tok_len = 1;
    return 0;
}

/* syntax renkleri — VSCode mor/purple temasına uygun palet.
   Bu renkler kullanıcının settings.json'ındaki editor.foreground (#ffd5d5) ve
   mor tema tonlarıyla uyumludur; syntax highlight burada gerçekten görünür. */
Color gcl_token_color(int type, const GclIdeTheme *t) {
    (void)t;
    switch (type) {
        case 1:  return (Color){ 197, 134, 192, 255 };  /* keyword — VSCode mor (#c586c0) */
        case 2:  return (Color){ 206, 145, 120, 255 };  /* string — VSCode turuncu (#ce9178) */
        case 3:  return (Color){ 206, 145, 120, 255 };  /* char — string ile aynı */
        case 4:  return (Color){ 181, 206, 168, 255 };  /* number — VSCode açık yeşil (#b5cea8) */
        case 5:  return (Color){ 106, 153, 85, 255 };   /* comment — VSCode yeşil (#6a9955) */
        case 6:  return (Color){ 200, 160, 255, 255 };  /* preproc — mor (%c8a0ff) */
        case 8:  return (Color){ 220, 220, 170, 255 };  /* function — VSCode sarı (#dcdcaa) */
        case 9:  return (Color){ 78, 201, 176, 255 };   /* class — VSCode turkuaz (#4ec9b0) */
        case 10: return (Color){ 156, 220, 254, 255 };  /* self/cls — VSCode açık mavi (#9cdcfe) */
        default: return (Color){ 255, 213, 213, 255 };  /* plain/ident — kullanıcı foreground'u (#ffd5d5) */
    }
}

void editor_typewriter_sound(Editor *ed, int kind) {
    if (!ed->sound) return;

    float pitch = 1.0f;
    if (kind == 1 && ed->enter_snd.frameCount > 0) {
        pitch = 1.0f;
    } else if (kind == 2 && ed->delete_snd.frameCount > 0) {
        pitch = 1.0f + ed->pitch_increase * 0.5f;
    } else if (ed->click_snd.frameCount > 0) {
        pitch = 1.0f + ed->pitch_increase;
    }

    Sound *snd = NULL;
    if (kind == 1) snd = &ed->enter_snd;
    else if (kind == 2) snd = &ed->delete_snd;
    else snd = &ed->click_snd;

    if (snd && snd->frameCount > 0) {
        SetSoundPitch(*snd, pitch);
        PlaySound(*snd);
    }

    ed->sound_pitch = pitch;

    /* Typing: her tuşta hafif pitch artışı; sönümleme ana döngüde (main loop) yapılır */
    if (kind != 1) {
        ed->pitch_increase += 0.10f;
        if (ed->pitch_increase > 0.8f) ed->pitch_increase = 0.8f;
    }
    ed->last_typed_key = GetTime();
}

void editor_spawn_particles(Editor *ed, float px, float py, int intensity) {
    if (ed->part_count >= 256) return;
    int count = 1 + (int)((intensity / 100.0f) * 5.0f);
    if (count < 1) count = 1;
    if (count > 8) count = 8;
    float spd_scale = 0.4f + (intensity / 100.0f) * 0.6f;
    for (int i = 0; i < count; i++) {
        if (ed->part_count >= 256) break;
        Particle *p = &ed->parts[ed->part_count++];
        float ang = (float)(rand() % 360) * 3.14159f / 180.0f;
        float spd = (float)(rand() % 80 + 40) / 100.0f * spd_scale;
        p->x = px;
        p->y = py;
        p->vx = cosf(ang) * spd;
        p->vy = sinf(ang) * spd;
        p->life = 0.4f + (float)(rand() % 40) / 100.0f;
        /* pixel-art: kare parçacık boyutu (1-3px) */
        p->size = 1.0f + (float)(rand() % 3);
        switch (rand() % 4) {
            case 0: p->color = (Color){ 255, 90, 90, 255 }; break;
            case 1: p->color = (Color){ 90, 220, 110, 255 }; break;
            case 2: p->color = (Color){ 120, 180, 255, 255 }; break;
            default: p->color = (Color){ 255, 220, 90, 255 }; break;
        }
    }
}

/* İmlecin görsel konumunu hesapla ve orada partikül patlat.
   editor_rect: kod editörü alanı. Enter/Delete/Backspace efektleri için kullanılır. */
void editor_spawn_particles_at_cursor(Editor *ed, Rectangle editor_rect) {
    if (!ed->particles || ed->part_count >= 256) return;
    size_t cl = gcl_ide_buffer_line_of_cursor(&CURP);
    size_t ccol = gcl_ide_buffer_cursor_in_line(&CURP);
    int efont = ed->editor_font;
    float fcw = gcl_measure_text_f("M", efont);
    if (fcw < 1.0f) fcw = 1.0f;
    float sx_f = fcw * (float)CURP.scroll_x;
    size_t l = 0;
    const char *cline = gcl_ide_buffer_line_at(&CURP, cl, &l);
    if (ccol > l) ccol = l;
    char prefix[4096];
    if (ccol > sizeof(prefix) - 1) ccol = sizeof(prefix) - 1;
    memcpy(prefix, cline, ccol); prefix[ccol] = '\0';
    float px = editor_rect.x + GUTTER_W + gcl_measure_text_f(prefix, efont) - sx_f;
    float py = editor_rect.y + 4 + (float)(cl - CURP.scroll_y) * LINE_H + (float)LINE_H / 2;
    editor_spawn_particles(ed, px, py, ed->particle_strength);
}

void editor_update_particles(Editor *ed, float dt) {
    for (int i = 0; i < ed->part_count; i++) {
        Particle *p = &ed->parts[i];
        p->x += p->vx * dt * 60.0f;
        p->y += p->vy * dt * 60.0f;
        p->vy += 0.03f; /* gravity */
        p->life -= dt;
    }
    int w = 0;
    for (int i = 0; i < ed->part_count; i++) {
        if (ed->parts[i].life > 0) ed->parts[w++] = ed->parts[i];
    }
    ed->part_count = w;
}
