/*
 * gcl_raygui.c — GCL Raygui modülü (.dll/.so) — TAM wrapper.
 *
 * #native <Raygui> ile yüklenir; Raygui.* çağrıları raygui fonksiyonlarına bağlanır.
 *
 * GCL modül sistemi: fn(int argc, const char **argv) -> double.
 *   - Rectangle -> 4 arg (x, y, w, h)
 *   - Color     -> 1 arg (packed uint R|G<<8|B<<16|A<<24)
 *   - int/float/bool çıktıları g_last_* global'lerine yazılır.
 */

#include "gcl_module.h"

#include <raygui.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

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
static double fn_GuiLoadStyleFromMemory(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
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
static double fn_GuiGetIcons(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GuiLoadIcons(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GuiLoadIconsFromMemory(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GuiDrawIcon(int argc,const char**argv){ GuiDrawIcon(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }

static double fn_GuiGetTextWidth(int argc,const char**argv){ return (double)GuiGetTextWidth(ss(argv[0])); }

/* =========================================================================
   Container / separator controls
   ========================================================================= */
static double fn_GuiWindowBox(int argc,const char**argv){ return (double)GuiWindowBox(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiGroupBox(int argc,const char**argv){ return (double)GuiGroupBox(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiLine(int argc,const char**argv){ return (double)GuiLine(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiPanel(int argc,const char**argv){ return (double)GuiPanel(rect_arg(argv,0),ss(argv[4])); }
static double fn_GuiScrollPanel(int argc,const char**argv){
    Rectangle content=rect_arg(argv,5);
    Vector2 scroll=v2_arg_in(argv,9); Rectangle view=g_last_rect;
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
static double fn_GuiListViewEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GuiTabBar(int argc,const char**argv){
    int hscroll=ii(argv[5]); int active=ii(argv[6]);
    int r=GuiTabBar(rect_arg(argv,0),ss(argv[4]),&hscroll,&active);
    g_last_int=active; g_last_v2=(Vector2){(float)hscroll,0}; return (double)r;
}
static double fn_GuiTabBarEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
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
    Color color=ci(argv,0==0?0:0);
    (void)color;
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
    E(GuiSetFont),E(GuiGetFont),
    /* Style */
    E(GuiSetStyle),E(GuiGetStyle),
    E(GuiLoadStyle),E(GuiLoadStyleFromMemory),E(GuiLoadStyleDefault),
    /* Tooltips */
    E(GuiEnableTooltip),E(GuiDisableTooltip),E(GuiSetTooltip),
    /* Icons */
    E(GuiIconText),E(GuiSetIconScale),E(GuiGetIcons),E(GuiLoadIcons),
    E(GuiLoadIconsFromMemory),E(GuiDrawIcon),
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
};

GCL_EXPORT const GclNativeEntry *gcl_raygui_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
