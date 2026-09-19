/*
 * gcl_raygui.c — GCL Raygui modülü (.dll/.so) — raygui'nin tam fonksiyon yüzeyi.
 *
 * #native <Raygui> ile yüklenir; Raygui.* çağrıları raygui fonksiyonlarına bağlanır.
 *
 * GCL modül sistemi: fn(int argc, const char **argv) -> double.
 *   - Rectangle -> 4 arg (x, y, w, h)
 *   - Color     -> 1 arg (packed uint R|G<<8|B<<16|A<<24)
 *   - int/float/bool çıktıları g_last_* global'lerine yazılır.
 *
 * Erişim yolları:
 *   - Fonksiyonlar : raygui.h'deki 60 RAYGUIAPI fonksiyonunun tamamı.
 *   - Sabitler     : GCL_RAYGUI_CONST_LIST (tools/gen_gui_consts.py üretir) —
 *                    GuiControl/GuiControlProperty/.../GuiIconName dahil.
 *   - Durum okuma  : LastInt/LastFloat/LastBool/LastV2X../LastColor,
 *                    LastRectX../LastFont..., LastStringLen/LastStringByte.
 *   - İkon verisi  : GuiGetIcons/GuiGetIconByte/GuiIconsName/GuiIconsNameCount.
 *   - İkili girdi  : GuiLoadStyleFromMemory / GuiLoadIconsFromMemory — .rgs ve
 *                    .rgi yükleri HEX metin olarak geçer (hex köprüsü).
 *
 * SINIR: GuiSetFont yalnızca texture + metrik taşır; glyph dizisi (recs/glyphs)
 * GCL'den geçirilemediği için çizim eksik kalır. Font yüklemek için
 * GuiLoadFont / GuiLoadFontEx kullanın — Font'un TAMAMI raygui'ye verilir.
 */

#include "gcl_module.h"

#include <raygui.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* raygui enum sabit listesi (GuiControl, GuiControlProperty, GuiIconName, ...).
   tools/gen_gui_consts.py tarafından üretilir; elle düzenlenmemeli. */
#include "gcl_raygui_consts.h"

/* ---------- Global çıktılar ---------- */
static int     g_last_int   = 0;
static float   g_last_float = 0.0f;
static bool    g_last_bool  = false;
static Vector2 g_last_v2    = {0};
static Vector3 g_last_v3    = {0};
static Color   g_last_color = {0};
static Rectangle g_last_rect = {0};
static Font    g_last_font  = {0};
static char    g_str[4096]  = {0};

/* ---------- Yardımcılar ---------- */
static int ii(const char *s){ return s? (int)atof(s) : 0; }
static float ff(const char *s){ return s? (float)atof(s) : 0.0f; }
static const char *ss(const char *s){ return s? s : ""; }
static Rectangle rect_arg(const char **a,int i){
    return (Rectangle){ ff(a[i]), ff(a[i+1]), ff(a[i+2]), ff(a[i+3]) };
}
static Vector2 v2_arg_in(const char **a,int i){ return (Vector2){ ff(a[i]), ff(a[i+1]) }; }
static unsigned int color_to_uint(Color c){
    return ((unsigned int)c.r)|(((unsigned int)c.g)<<8)|(((unsigned int)c.b)<<16)|(((unsigned int)c.a)<<24);
}
static Color uint_to_color(unsigned int v){
    Color c; c.r=(unsigned char)(v&0xFF); c.g=(unsigned char)((v>>8)&0xFF); c.b=(unsigned char)((v>>16)&0xFF); c.a=(unsigned char)((v>>24)&0xFF); return c;
}
static Color ci(const char **a,int i){ return (a&&a[i])? uint_to_color((unsigned int)strtoul(a[i],NULL,10)) : (Color){0,0,0,255}; }

/* ---------- Hex <-> byte bridge ----------
   GCL string'leri modül ABI'sini geçebilen TEK bayt kabıdır. raygui'nin ikili
   girdileri (.rgs stil dosyası, .rgi ikon dosyası) bu yüzden HEX metin olarak
   taşınır ve burada çözülür — Raylib modülündeki CompressData/LoadFileData ile
   aynı sözleşme. */
static int gui_hex_val(int c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* Hex metni yeni bir malloc tamponuna çözer (çağıran serbest bırakır). */
static unsigned char *gui_hex_to_bytes(const char *hex, int hex_len, int *out_size){
    int n = 0;
    unsigned char *out;
    if (out_size) *out_size = 0;
    if (!hex || hex_len <= 0) return NULL;
    out = (unsigned char *)malloc((size_t)(hex_len / 2) + 1);
    if (!out) return NULL;
    for (int i = 0; i + 1 < hex_len; i += 2) {
        int hi = gui_hex_val((unsigned char)hex[i]);
        int lo = gui_hex_val((unsigned char)hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
    }
    if (out_size) *out_size = n;
    return out;
}

/* ---------- .rgi (raygui ikon dosyası) başlığı ----------
   Yerleşim (raygui.h GuiLoadIconsFromMemory yorumu):
     "rGI " | version(2) | reserved(2) | iconCount(2) | iconSize(2)
   Sayı burada okunur, çünkü raygui yükleyicileri ikon SAYISINI değil isim
   tablosunu döndürür. */
static int gui_rgi_icon_count(const unsigned char *d, int size){
    if (!d || size < 12) return 0;
    if (!(d[0]=='r' && d[1]=='G' && d[2]=='I' && d[3]==' ')) return 0;
    return (int)(short)(d[8] | (d[9] << 8));
}
static int gui_rgi_icon_size(const unsigned char *d, int size){
    if (!d || size < 12) return 0;
    return (int)(short)(d[10] | (d[11] << 8));
}

/* ---------- İkon seti durumu ----------
   GuiLoadIconsFromMemory 512 slotluk bir isim tablosu döndürür; tabloyu
   ÇAĞIRAN serbest bırakmalıdır ve kaç ikon yüklendiğini bildirmez. Modül
   isimlerin kendi kopyasını ve ikon verisinin bayt boyutunu tutar, raygui
   tamponlarını hemen bırakır; böylece GCL tarafı yükleme sonrası hem isimleri
   hem veriyi okuyabilir. */
#ifndef RAYGUI_ICON_MAX_ICONS
#define RAYGUI_ICON_MAX_ICONS 512
#endif
#ifndef RAYGUI_ICON_DATA_ELEMENTS
#define RAYGUI_ICON_DATA_ELEMENTS 8
#endif
#define GCL_MAX_ICON_NAMES RAYGUI_ICON_MAX_ICONS

static char *g_icon_names[GCL_MAX_ICON_NAMES] = {0};
static int   g_icon_name_count = 0;
/* Yerleşik set: RAYGUI_ICON_MAX_ICONS * RAYGUI_ICON_DATA_ELEMENTS unsigned int. */
static int   g_icons_bytes = RAYGUI_ICON_MAX_ICONS * RAYGUI_ICON_DATA_ELEMENTS * (int)sizeof(unsigned int);

static void gui_icon_names_clear(void){
    for (int i = 0; i < GCL_MAX_ICON_NAMES; i++) {
        if (g_icon_names[i]) { free(g_icon_names[i]); g_icon_names[i] = NULL; }
    }
    g_icon_name_count = 0;
}
/* Yeni yüklenen ikon setini devralır ve raygui'nin isim tablosunu serbest bırakır. */
static void gui_icon_names_adopt(char **names, int count, int icon_size){
    int has_names = (names != NULL) ? 1 : 0;
    if (count < 0) count = 0;
    if (count > GCL_MAX_ICON_NAMES) count = GCL_MAX_ICON_NAMES;
    gui_icon_names_clear();
    for (int i = 0; i < count; i++) {
        if (names && names[i]) g_icon_names[i] = strdup(names[i]);
    }
    if (names) {
        for (int i = 0; i < GCL_MAX_ICON_NAMES; i++) if (names[i]) free(names[i]);
        free(names);
    }
    g_icon_name_count = has_names ? count : 0;
    /* Veri boyutu isim tablosundan bağımsızdır: loadIconsName=0 olsa da set yüklendi. */
    if (count > 0 && icon_size >= 16)
        g_icons_bytes = count * ((icon_size * icon_size / 32) * (int)sizeof(unsigned int));
}

/* =========================================================================
   CONSTANTS — raygui.h enum değerleri (Raygui.BUTTON, Raygui.TEXT_SIZE,
   Raygui.ICON_FILE_OPEN, ...). Liste tools/gen_gui_consts.py tarafından
   raygui.h'den üretilir; elle düzenlenmez.
   ========================================================================= */
#define CONST_FN(n) static double fn_##n(int argc,const char**argv){(void)argc;(void)argv;return (double)(n);}
GCL_RAYGUI_CONST_LIST(CONST_FN)
#undef CONST_FN

/* =========================================================================
   READ-BACK — kontrollerin out-parametre sonuçlarını GCL'e taşır.
   GuiSlider/GuiCheckBox/GuiTextBox/... sonucu g_last_* ve g_str global'lerine
   yazar (modül ABI'si yalnızca tek bir double döndürebilir). Bu üyeler o
   değerleri okunabilir hale getirir; önce yalnızca int dönüşü erişilebiliyordu.
   String çıktısı için: n = Raygui.LastStringLen(); byte'lar LastStringByte(i).
   ========================================================================= */
static double fn_LastInt(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_int;}
static double fn_LastFloat(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_float;}
static double fn_LastBool(int argc,const char**argv){(void)argc;(void)argv;return g_last_bool?1.0:0.0;}
static double fn_LastV2X(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_v2.x;}
static double fn_LastV2Y(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_v2.y;}
static double fn_LastV3X(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_v3.x;}
static double fn_LastV3Y(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_v3.y;}
static double fn_LastV3Z(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_v3.z;}
static double fn_LastColor(int argc,const char**argv){(void)argc;(void)argv;return (double)color_to_uint(g_last_color);}
static double fn_LastRectX(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_rect.x;}
static double fn_LastRectY(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_rect.y;}
static double fn_LastRectW(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_rect.width;}
static double fn_LastRectH(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_rect.height;}
static double fn_LastFontBaseSize(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_font.baseSize;}
static double fn_LastFontGlyphCount(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_font.glyphCount;}
static double fn_LastFontGlyphPadding(int argc,const char**argv){(void)argc;(void)argv;return (double)g_last_font.glyphPadding;}
/* g_str: uzunluk + byte indeksleme (GCL'den string okumanın ABI uyumlu yolu). */
static double fn_LastStringLen(int argc,const char**argv){(void)argc;(void)argv;return (double)strlen(g_str);}
static double fn_LastStringByte(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    int n = (int)strlen(g_str);
    if (i < 0 || i >= n) return 0.0;
    return (double)(unsigned char)g_str[i];
}
/* =========================================================================
   Global gui state
   ========================================================================= */
static double fn_GuiEnable(int argc,const char**argv){(void)argc;(void)argv;GuiEnable();return 0.0;}
static double fn_GuiDisable(int argc,const char**argv){(void)argc;(void)argv;GuiDisable();return 0.0;}
static double fn_GuiLock(int argc,const char**argv){(void)argc;(void)argv;GuiLock();return 0.0;}
static double fn_GuiUnlock(int argc,const char**argv){(void)argc;(void)argv;GuiUnlock();return 0.0;}
static double fn_GuiIsLocked(int argc,const char**argv){(void)argc;(void)argv;return GuiIsLocked()?1.0:0.0;}
static double fn_GuiSetAlpha(int argc,const char**argv){ GuiSetAlpha(ff(argv[0])); return 0.0; }
static double fn_GuiSetState(int argc,const char**argv){ GuiSetState(ii(argv[0])); return 0.0; }
static double fn_GuiGetState(int argc,const char**argv){(void)argc;(void)argv;return (double)GuiGetState();}

/* Font */
/* GuiSetFont(bounds-tabanlı 4 sayı) korunur, ama 4 sayıdan gerçek bir Font
   kurulamaz (glyph dizisi yok) → çizim bozuk olur. Gerçek kullanım için
   GuiLoadFont / GuiLoadFontEx eklendi: raylib'den yüklenen Font'un TAMAMI
   raygui'ye verilir. Yüklenen font modül ömrü boyunca tutulur, çünkü raygui
   yalnızca Font struct'ının kopyasını saklar. */
static Font g_gui_font_holder = {0};
static double fn_GuiLoadFont(int argc,const char**argv){
    Font nf = LoadFont(ss((argc > 0) ? argv[0] : ""));
    GuiSetFont(nf);
    if (g_gui_font_holder.texture.id > 0) UnloadFont(g_gui_font_holder);
    g_gui_font_holder = nf;
    g_last_font = nf;
    return (double)nf.texture.id;
}
static double fn_GuiLoadFontEx(int argc,const char**argv){
    Font nf = LoadFontEx(ss((argc > 0) ? argv[0] : ""),
                         (argc > 1) ? ii(argv[1]) : 16, NULL, 0);
    GuiSetFont(nf);
    if (g_gui_font_holder.texture.id > 0) UnloadFont(g_gui_font_holder);
    g_gui_font_holder = nf;
    g_last_font = nf;
    return (double)nf.texture.id;
}
static double fn_GuiSetFont(int argc,const char**argv){
    Font f; memset(&f,0,sizeof(f));
    f.baseSize=ii(argv[0]); f.glyphCount=ii(argv[1]); f.glyphPadding=ii(argv[2]);
    f.texture.id=(unsigned int)strtoul(ss(argv[3]),NULL,10);
    GuiSetFont(f); g_last_font=f; return 0.0;
}
static double fn_GuiGetFont(int argc,const char**argv){(void)argc;(void)argv;g_last_font=GuiGetFont();return 0.0;}

/* Style */
static double fn_GuiSetStyle(int argc,const char**argv){ GuiSetStyle(ii(argv[0]),ii(argv[1]),ii(argv[2])); return 0.0; }
static double fn_GuiGetStyle(int argc,const char**argv){ return (double)GuiGetStyle(ii(argv[0]),ii(argv[1])); }

/* Styles loading */
static double fn_GuiLoadStyle(int argc,const char**argv){ GuiLoadStyle(ss(argv[0])); return 0.0; }
/* GuiLoadStyleFromMemory(hexData[, dataSize]) — .rgs stilini bellekten yükler.
   raygui `const unsigned char *` ister; GCL'de ikili dizi olmadığı için yük HEX
   metin olarak geçer (hex köprüsü). Dönüş: kullanılan bayt sayısı. */
static double fn_GuiLoadStyleFromMemory(int argc,const char**argv){
    int size = 0;
    const char *hex = (argc > 0) ? ss(argv[0]) : "";
    unsigned char *data = gui_hex_to_bytes(hex, (int)strlen(hex), &size);
    if (!data) return 0.0;
    if (argc > 1 && argv[1]) {
        int want = ii(argv[1]);
        if (want > 0 && want < size) size = want;
    }
    GuiLoadStyleFromMemory(data, size);
    free(data);
    return (double)size;
}
static double fn_GuiLoadStyleDefault(int argc,const char**argv){(void)argc;(void)argv;GuiLoadStyleDefault();return 0.0;}

/* Tooltips */
static double fn_GuiEnableTooltip(int argc,const char**argv){(void)argc;(void)argv;GuiEnableTooltip();return 0.0;}
static double fn_GuiDisableTooltip(int argc,const char**argv){(void)argc;(void)argv;GuiDisableTooltip();return 0.0;}
static double fn_GuiSetTooltip(int argc,const char**argv){ GuiSetTooltip(ss(argv[0])); return 0.0; }

/* Icons */
static double fn_GuiIconText(int argc,const char**argv){
    const char *s=GuiIconText(ii(argv[0]),ss(argv[1]));
    if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0;
}
static double fn_GuiSetIconScale(int argc,const char**argv){ GuiSetIconScale(ii(argv[0])); return 0.0; }
/* GuiGetIcons() — ikon verisinin okunabilir BAYT sayısı (veri yoksa 0).
   raygui bir `unsigned int *` döndürür; GCL'de pointer taşınamadığı için uzunluk
   döndürülür, baytlar GuiGetIconByte(i) ile okunur. */
static double fn_GuiGetIcons(int argc,const char**argv){
    (void)argc;(void)argv;
    if (!GuiGetIcons()) return 0.0;
    return (double)g_icons_bytes;
}
/* GuiGetIconByte(index) — ikon verisinin tek baytı (aralık dışında 0).
   Bayt bayt okuma, g_str sınırına takılmadan tüm bitmap'i hex'e çevirmeyi sağlar. */
static double fn_GuiGetIconByte(int argc,const char**argv){
    const unsigned char *icons = (const unsigned char *)GuiGetIcons();
    int i = (argc > 0) ? ii(argv[0]) : -1;
    if (!icons || i < 0 || i >= g_icons_bytes) return 0.0;
    return (double)icons[i];
}
/* GuiIconsNameCount() — yüklenen setteki isim sayısı (yerleşik sette isim yoktur). */
static double fn_GuiIconsNameCount(int argc,const char**argv){
    (void)argc;(void)argv;return (double)g_icon_name_count;
}
/* GuiIconsName(index) — ikon adını g_str'ye yazar ve uzunluğunu döndürür
   (okuma: LastStringLen/LastStringByte). */
static double fn_GuiIconsName(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    const char *nm = (i >= 0 && i < g_icon_name_count && g_icon_names[i]) ? g_icon_names[i] : "";
    snprintf(g_str, sizeof(g_str), "%s", nm);
    return (double)strlen(g_str);
}
/* GuiLoadIcons(fileName[, loadIconsName]) — .rgi ikon dosyasını yükler.
   Dönüş: 1 (yüklendi) / 0. İkon sayısı g_last_int'te (Raygui.LastInt()), adlar
   GuiIconsName(i)/GuiIconsNameCount() ile okunur. Modül raygui'nin ayırdığı isim
   tablosunu devralıp kendi kopyasını tutar ve raygui tamponlarını serbest
   bırakır (aksi halde her yükleme sızıntı olurdu). */
static double fn_GuiLoadIcons(int argc,const char**argv){
    const char *path = ss((argc > 0) ? argv[0] : "");
    int load_names = (argc > 1) ? (ii(argv[1]) != 0) : 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0.0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0.0; }
    long size = ftell(f);
    if (size <= 12) { fclose(f); return 0.0; }
    rewind(f);
    unsigned char *data = (unsigned char *)malloc((size_t)size);
    if (!data) { fclose(f); return 0.0; }
    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);
    int count = gui_rgi_icon_count(data, (int)got);
    int icon_size = gui_rgi_icon_size(data, (int)got);
    if (count <= 0) { free(data); return 0.0; }
    char **names = GuiLoadIconsFromMemory(data, (int)got, load_names ? true : false);
    free(data);
    gui_icon_names_adopt(names, count, icon_size);
    g_last_int = count;
    return 1.0;
}
/* GuiLoadIconsFromMemory(hexData[, dataSize[, loadIconsName]]) — .rgi setini
   bellekten yükler (yük HEX metin olarak geçer). Dönüş ve çıktılar GuiLoadIcons
   ile aynıdır. */
static double fn_GuiLoadIconsFromMemory(int argc,const char**argv){
    int size = 0;
    const char *hex = ss((argc > 0) ? argv[0] : "");
    unsigned char *data = gui_hex_to_bytes(hex, (int)strlen(hex), &size);
    if (!data) return 0.0;
    if (argc > 1 && argv[1]) {
        int want = ii(argv[1]);
        if (want > 0 && want < size) size = want;
    }
    int load_names = (argc > 2) ? (ii(argv[2]) != 0) : 0;
    int count = gui_rgi_icon_count(data, size);
    int icon_size = gui_rgi_icon_size(data, size);
    if (count <= 0) { free(data); return 0.0; }
    char **names = GuiLoadIconsFromMemory(data, size, load_names ? true : false);
    free(data);
    gui_icon_names_adopt(names, count, icon_size);
    g_last_int = count;
    return 1.0;
}
static double fn_GuiDrawIcon(int argc,const char**argv){ GuiDrawIcon(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }

static double fn_GuiGetTextWidth(int argc,const char**argv){ return (double)GuiGetTextWidth(ss(argv[0])); }

/* =========================================================================
   Container / separator controls
   ========================================================================= */
static double fn_GuiWindowBox(int argc,const char**argv){ return (double)GuiWindowBox(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiGroupBox(int argc,const char**argv){ return (double)GuiGroupBox(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiLine(int argc,const char**argv){ return (double)GuiLine(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiPanel(int argc,const char**argv){ return (double)GuiPanel(rect_arg(argv,0),ss(argv[4])); }
/* Argümanlar: bounds(0-3), text(4), content(5-8), scroll(9-10), view(11-14).
   view verilmezse bounds kullanılır. Önceki sürüm view'i son g_last_rect'ten
   alıyordu; ilk çağrıda çöp değerle başlıyordu. */
static double fn_GuiScrollPanel(int argc,const char**argv){
    Rectangle content=rect_arg(argv,5);
    Vector2 scroll=v2_arg_in(argv,9);
    Rectangle view=(argc>=15)?rect_arg(argv,11):rect_arg(argv,0);
    int r=GuiScrollPanel(rect_arg(argv,0),ss(argv[4]),content,&scroll,&view);
    g_last_v2=scroll; g_last_rect=view; g_last_int=r; return (double)r;
}

/* Basic controls */
static double fn_GuiLabel(int argc,const char**argv){ return (double)GuiLabel(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiButton(int argc,const char**argv){ return (double)GuiButton(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiLabelButton(int argc,const char**argv){ return (double)GuiLabelButton(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiToggle(int argc,const char**argv){
    bool active=(argc>5)?(ii(argv[5])!=0):false;
    int r=GuiToggle(rect_arg(argv,0),ss(argv[4]),&active);
    g_last_bool=active; return (double)r;
}
static double fn_GuiToggleGroup(int argc,const char**argv){
    int active=ii(argv[5]);
    int r=GuiToggleGroup(rect_arg(argv,0),ss(argv[4]),&active);
    g_last_int=active; return (double)r;
}
static double fn_GuiToggleSlider(int argc,const char**argv){
    int active=ii(argv[5]);
    int r=GuiToggleSlider(rect_arg(argv,0),ss(argv[4]),&active);
    g_last_int=active; return (double)r;
}
static double fn_GuiCheckBox(int argc,const char**argv){
    bool checked=(argc>5)?(ii(argv[5])!=0):false;
    int r=GuiCheckBox(rect_arg(argv,0),ss(argv[4]),&checked);
    g_last_bool=checked; return (double)r;
}
static double fn_GuiComboBox(int argc,const char**argv){
    int active=ii(argv[5]);
    int r=GuiComboBox(rect_arg(argv,0),ss(argv[4]),&active);
    g_last_int=active; return (double)r;
}
static double fn_GuiDropdownBox(int argc,const char**argv){
    int active=ii(argv[5]);
    bool editMode=(argc>6)?(ii(argv[6])!=0):false;
    int r=GuiDropdownBox(rect_arg(argv,0),ss(argv[4]),&active,editMode);
    g_last_int=active; return (double)r;
}
static double fn_GuiSpinner(int argc,const char**argv){
    int value=ii(argv[5]);
    bool editMode=(argc>8)?(ii(argv[8])!=0):false;
    int r=GuiSpinner(rect_arg(argv,0),ss(argv[4]),&value,ii(argv[6]),ii(argv[7]),editMode);
    g_last_int=value; return (double)r;
}
static double fn_GuiValueBox(int argc,const char**argv){
    int value=ii(argv[5]);
    bool editMode=(argc>8)?(ii(argv[8])!=0):false;
    int r=GuiValueBox(rect_arg(argv,0),ss(argv[4]),&value,ii(argv[6]),ii(argv[7]),editMode);
    g_last_int=value; return (double)r;
}
static double fn_GuiValueBoxFloat(int argc,const char**argv){
    char textValue[64]={0}; if(argc>5&&argv[5])snprintf(textValue,sizeof(textValue),"%s",argv[5]);
    float value=(argc>6)?ff(argv[6]):0.0f;
    bool editMode=(argc>7)?(ii(argv[7])!=0):false;
    int r=GuiValueBoxFloat(rect_arg(argv,0),ss(argv[4]),textValue,&value,editMode);
    snprintf(g_str,sizeof(g_str),"%s",textValue);
    g_last_float=value; return (double)r;
}
static double fn_GuiTextBox(int argc,const char**argv){
    char textBuf[4096]={0}; if(argc>5&&argv[5])snprintf(textBuf,sizeof(textBuf),"%s",argv[5]);
    bool editMode=(argc>7)?(ii(argv[7])!=0):false;
    int r=GuiTextBox(rect_arg(argv,0),textBuf,ii(argv[6]),editMode);
    snprintf(g_str,sizeof(g_str),"%s",textBuf);
    return (double)r;
}
static double fn_GuiSlider(int argc,const char**argv){
    float value=(argc>6)?ff(argv[6]):0.0f;
    int r=GuiSlider(rect_arg(argv,0),ss(argv[4]),ss(argv[5]),&value,ff(argv[7]),ff(argv[8]));
    g_last_float=value; return (double)r;
}
static double fn_GuiSliderBar(int argc,const char**argv){
    float value=(argc>6)?ff(argv[6]):0.0f;
    int r=GuiSliderBar(rect_arg(argv,0),ss(argv[4]),ss(argv[5]),&value,ff(argv[7]),ff(argv[8]));
    g_last_float=value; return (double)r;
}
static double fn_GuiProgressBar(int argc,const char**argv){
    float value=(argc>6)?ff(argv[6]):0.0f;
    int r=GuiProgressBar(rect_arg(argv,0),ss(argv[4]),ss(argv[5]),&value,ff(argv[7]),ff(argv[8]));
    g_last_float=value; return (double)r;
}
static double fn_GuiStatusBar(int argc,const char**argv){ return (double)GuiStatusBar(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiDummyRec(int argc,const char**argv){ return (double)GuiDummyRec(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiGrid(int argc,const char**argv){
    Vector2 mouseCell={0};
    int r=GuiGrid(rect_arg(argv,0),ss(argv[4]),ff(argv[5]),ii(argv[6]),&mouseCell);
    g_last_v2=mouseCell; return (double)r;
}

/* Advanced controls */
static double fn_GuiListView(int argc,const char**argv){
    int scrollIndex=ii(argv[5]); int active=ii(argv[6]);
    int r=GuiListView(rect_arg(argv,0),ss(argv[4]),&scrollIndex,&active);
    g_last_int=active; g_last_v2=(Vector2){(float)scrollIndex,0}; return (double)r;
}
/* GuiListViewEx / GuiTabBarEx metin listesini `char **` olarak ister; GCL dizi
   taşıyamadığı için liste "adet + metinler" olarak geçer ve bu yardımcı onu
   argv'den kurar. Dönüş: ilk durum parametresinin argv indeksi. */
#define GCL_GUI_MAX_ITEMS 64

static int gui_text_list(const char **argv, int argc, int count, char **out){
    int base = 5;   /* bounds 0-3, count 4 */
    for (int i = 0; i < count; i++)
        out[i] = (char *)((base + i < argc) ? ss(argv[base + i]) : "");
    return base + count;
}

/* GuiListViewEx(x,y,w,h, count, text0..textN-1, scrollIndex, active[, focus]).
   raygui `char **text` ister; liste "adet + metinler" olarak geçer. Dönüş
   raygui kontrol sonucudur. Durum çıktıları: seçili öğe LastInt, kaydırma
   indeksi LastV2X, odaklı öğe LastV2Y. */
static double fn_GuiListViewEx(int argc,const char**argv){
    int count = (argc > 4) ? ii(argv[4]) : 0;
    if (count < 0) count = 0;
    if (count > GCL_GUI_MAX_ITEMS) count = GCL_GUI_MAX_ITEMS;
    char *texts[GCL_GUI_MAX_ITEMS];
    int base = gui_text_list(argv, argc, count, texts);
    int scrollIndex = (base     < argc) ? ii(argv[base])     : 0;
    int active      = (base + 1 < argc) ? ii(argv[base + 1]) : -1;
    int focus       = (base + 2 < argc) ? ii(argv[base + 2]) : -1;
    int r = GuiListViewEx(rect_arg(argv,0), (count > 0) ? texts : NULL, count,
                          &scrollIndex, &active, &focus);
    g_last_int = active;
    g_last_v2  = (Vector2){ (float)scrollIndex, (float)focus };
    return (double)r;
}
static double fn_GuiTabBar(int argc,const char**argv){
    int hscroll=ii(argv[5]); int active=ii(argv[6]);
    int r=GuiTabBar(rect_arg(argv,0),ss(argv[4]),&hscroll,&active);
    g_last_int=active; g_last_v2=(Vector2){(float)hscroll,0}; return (double)r;
}
/* GuiTabBarEx(x,y,w,h, count, tab0..tabN-1, hscroll, active[, focus]).
   raygui sekme listesini `char **` olarak ister; aktif sekme zorunlu bir out
   parametredir (raygui onu NULL'a karşı korumaz, bu yüzden daima geçerli bir
   pointer verilir). Durum çıktıları: aktif sekme LastInt, hscroll LastV2X,
   odak LastV2Y. */
static double fn_GuiTabBarEx(int argc,const char**argv){
    int count = (argc > 4) ? ii(argv[4]) : 0;
    if (count < 0) count = 0;
    if (count > GCL_GUI_MAX_ITEMS) count = GCL_GUI_MAX_ITEMS;
    char *texts[GCL_GUI_MAX_ITEMS];
    int base = gui_text_list(argv, argc, count, texts);
    int hscroll = (base     < argc) ? ii(argv[base])     : 0;
    int active  = (base + 1 < argc) ? ii(argv[base + 1]) : 0;
    if (active < 0) active = 0;
    int focus   = (base + 2 < argc) ? ii(argv[base + 2]) : -1;
    int r = GuiTabBarEx(rect_arg(argv,0), (count > 0) ? texts : NULL, count,
                        &hscroll, &active, &focus);
    g_last_int = active;
    g_last_v2  = (Vector2){ (float)hscroll, (float)focus };
    return (double)r;
}
static double fn_GuiMessageBox(int argc,const char**argv){
    int btnActive=0;
    int r=GuiMessageBox(rect_arg(argv,0),ss(argv[4]),ss(argv[5]),ss(argv[6]),&btnActive);
    g_last_int=btnActive; return (double)r;
}
static double fn_GuiTextInputBox(int argc,const char**argv){
    char textBuf[4096]={0}; if(argc>6&&argv[6])snprintf(textBuf,sizeof(textBuf),"%s",argv[6]);
    int btnActive=0; bool secret=(argc>9)?(ii(argv[9])!=0):false;
    int r=GuiTextInputBox(rect_arg(argv,0),ss(argv[4]),ss(argv[5]),textBuf,ii(argv[7]),ss(argv[8]),&btnActive,&secret);
    snprintf(g_str,sizeof(g_str),"%s",textBuf);
    g_last_int=btnActive; g_last_bool=secret; return (double)r;
}
static double fn_GuiColorPicker(int argc,const char**argv){
    /* bounds 0-3, text 4, color packed 5 */
    Color c=ci(argv,5);
    int r=GuiColorPicker(rect_arg(argv,0),ss(argv[4]),&c);
    g_last_color=c; return (double)r;
}
static double fn_GuiColorPanel(int argc,const char**argv){
    Color c=ci(argv,5);
    int r=GuiColorPanel(rect_arg(argv,0),ss(argv[4]),&c);
    g_last_color=c; return (double)r;
}
static double fn_GuiColorBarAlpha(int argc,const char**argv){
    float alpha=(argc>5)?ff(argv[5]):0.0f;
    int r=GuiColorBarAlpha(rect_arg(argv,0),ss(argv[4]),&alpha);
    g_last_float=alpha; return (double)r;
}
static double fn_GuiColorBarHue(int argc,const char**argv){
    float value=(argc>5)?ff(argv[5]):0.0f;
    int r=GuiColorBarHue(rect_arg(argv,0),ss(argv[4]),&value);
    g_last_float=value; return (double)r;
}
static double fn_GuiColorPickerHSV(int argc,const char**argv){
    Vector3 c={ff(argv[5]),ff(argv[6]),ff(argv[7])};
    int r=GuiColorPickerHSV(rect_arg(argv,0),ss(argv[4]),&c);
    g_last_v3=c; return (double)r;
}
static double fn_GuiColorPanelHSV(int argc,const char**argv){
    Vector3 c={ff(argv[5]),ff(argv[6]),ff(argv[7])};
    int r=GuiColorPanelHSV(rect_arg(argv,0),ss(argv[4]),&c);
    g_last_v3=c; return (double)r;
}

/* =========================================================================
   Fonksiyon tablosu
   ========================================================================= */
#define E(NAME) {#NAME, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    /* State */
    E(GuiEnable),E(GuiDisable),E(GuiLock),E(GuiUnlock),E(GuiIsLocked),
    E(GuiSetAlpha),E(GuiSetState),E(GuiGetState),
    /* Font */
    E(GuiSetFont),E(GuiGetFont),E(GuiLoadFont),E(GuiLoadFontEx),
    /* Style */
    E(GuiSetStyle),E(GuiGetStyle),
    E(GuiLoadStyle),E(GuiLoadStyleFromMemory),E(GuiLoadStyleDefault),
    /* Tooltips */
    E(GuiEnableTooltip),E(GuiDisableTooltip),E(GuiSetTooltip),
    /* Icons */
    E(GuiIconText),E(GuiSetIconScale),E(GuiGetIcons),E(GuiLoadIcons),
    E(GuiLoadIconsFromMemory),E(GuiDrawIcon),
    /* İkon verisi/isim okuma erişimcileri */
    E(GuiGetIconByte),E(GuiIconsName),E(GuiIconsNameCount),
    /* Utility */
    E(GuiGetTextWidth),
    /* Containers */
    E(GuiWindowBox),E(GuiGroupBox),E(GuiLine),E(GuiPanel),E(GuiScrollPanel),
    /* Basic */
    E(GuiLabel),E(GuiButton),E(GuiLabelButton),E(GuiToggle),E(GuiToggleGroup),
    E(GuiToggleSlider),E(GuiCheckBox),E(GuiComboBox),E(GuiDropdownBox),E(GuiSpinner),
    E(GuiValueBox),E(GuiValueBoxFloat),E(GuiTextBox),E(GuiSlider),E(GuiSliderBar),
    E(GuiProgressBar),E(GuiStatusBar),E(GuiDummyRec),E(GuiGrid),
    /* Advanced */
    E(GuiListView),E(GuiListViewEx),E(GuiTabBar),E(GuiTabBarEx),
    E(GuiMessageBox),E(GuiTextInputBox),E(GuiColorPicker),E(GuiColorPanel),
    E(GuiColorBarAlpha),E(GuiColorBarHue),E(GuiColorPickerHSV),E(GuiColorPanelHSV),

    /* Constants — raygui.h enum değerleri (üretilen liste) */
#define CONST_ENTRY(n) E(n),
    GCL_RAYGUI_CONST_LIST(CONST_ENTRY)
#undef CONST_ENTRY

    /* Read-back — out-parametre sonuçlarını okuma erişimcileri */
    E(LastInt),E(LastFloat),E(LastBool),
    E(LastV2X),E(LastV2Y),E(LastV3X),E(LastV3Y),E(LastV3Z),
    E(LastColor),E(LastRectX),E(LastRectY),E(LastRectW),E(LastRectH),
    E(LastFontBaseSize),E(LastFontGlyphCount),E(LastFontGlyphPadding),
    E(LastStringLen),E(LastStringByte),
};

GCL_EXPORT const GclNativeEntry *gcl_raygui_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
