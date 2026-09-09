/*
 * gcl_ide_internal.h — GCL IDE iç yapıları.
 *
 * ridicullous_coding (Godot) referans alınarak iyileştirildi:
 *   - pitch-based typewriter sesi (monotonluğu kırar)
 *   - ease-out screenshake (düzgün sönümleme)
 *   - pixel-art patlama parçacıkları
 *   - font okunabilirliğini koruyan CRT/VHS/Bloom
 */

#ifndef GCL_IDE_INTERNAL_H
#define GCL_IDE_INTERNAL_H

#include "gcl_ide.h"
#include "gcl_ide_settings.h"
#include "gcl_settings_panel.h"
#include "gcl_lsp_internal.h"
#include "raylib.h"
#include "rlgl.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef _WIN32
#include <pthread.h>
#endif

/* Asenkron dış süreç çalıştırma soyutlaması.
   "Run" komutu senkron popen/fgets ile yapıldığında IDE kilitleniyor ve
   IDE kapanınca child süreç açık kalıyor. Bu yapı süreci asenkron başlatır,
   çıktıyı her frame'de toplar ve IDE kapanınca süreci sonlandırır.
   Windows: CreateProcess + pipe;  POSIX : fork/exec + pipe. */
typedef struct Editor Editor;
typedef struct GclIdeProcess GclIdeProcess;
void gcl_ide_proc_start(Editor *ed, const char *cmd, const char *label, FILE *logf);
void gcl_ide_proc_poll(Editor *ed);
void gcl_ide_proc_kill(Editor *ed);

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

/* GNU FreeFont — FreeMono.ttf gömülü byte array (makefile.py embed_font üretir). */
extern const unsigned char gcl_embed_freemono_ttf[];
extern const unsigned int gcl_embed_freemono_ttf_size;

/* Font render globals (ide_font.c). */
extern Font g_font;
extern int g_bloom;
extern int g_line_h;

/* FreeFont wrapper — DrawText/MeasureText çağrılarını özel fontla çizer. */
void gcl_draw_text(const char *text, int posX, int posY, int fontSize, Color color);
void gcl_draw_text_f(const char *text, float posX, int posY, int fontSize, Color color); /* float konum — syntax highlight chunk'larında truncate kaymasını önler */
int gcl_measure_text(const char *text, int fontSize);
float gcl_measure_text_f(const char *text, int fontSize); /* gerçek float genişlik — chunk çiziminde kayma birikmesini önler */

/* raylib'in DrawText/MeasureText fonksiyonlarını kendi wrapper'ımızla değiştir. */
#undef DrawText
#undef MeasureText
#define DrawText gcl_draw_text
#define MeasureText gcl_measure_text

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    float size;      /* pixel-art parçacık boyutu (px) */
    Color color;
} Particle;

#define MENU_H 28
#define TAB_H 28
#define STATUS_H 26
#define SIDEBAR_W 250
#define LINE_H (g_line_h)
#define GUTTER_W 44
#define MAX_EXPANDED 256
#define MAX_TABS 32

typedef enum {
    ACT_NONE = 0,
    ACT_SETTINGS,
    ACT_ABOUT,
    ACT_TOGGLE_EXPLORER,
    ACT_OPEN_FOLDER,
    ACT_RUN,
    ACT_BUILD,
    ACT_STOP
} Act;

typedef enum {
    MACT_NONE = 0,
    MACT_OPEN_FILE,
    MACT_OPEN_DIRECTORY,
    MACT_SAVE,
    MACT_SAVE_AS,
    MACT_RUN,
    MACT_TOGGLE_EXPLORER,
    MACT_TOGGLE_OUTPUT,
    MACT_SETTINGS,
    MACT_ABOUT,
    MACT_NEW_PROJECT,
    MACT_OPEN_PROJECT,
    MACT_RUN_PROJECT,
    MACT_BUILD_PROJECT,
    MACT_HELP
} MenuAction;

typedef enum {
    CTX_NEW_FILE = 0,
    CTX_NEW_FOLDER,
    CTX_RENAME,
    CTX_COPY,
    CTX_PASTE,
    CTX_DELETE
} CtxItem;

typedef struct {
    char *path;
    int is_dir;
    int depth;
} TreeNode;

typedef struct {
    TreeNode *nodes;
    int count;
    int cap;
} TreeModel;

typedef struct Editor {
    GclIdeBuffer *tabs;
    int tab_count;
    int active_tab;

    int about;
    int settings_open;
    int run_mode;

    char cwd[2048];

    /* Otomatik tamamlama (LSP) — LspSymbol listesi */
    LspSymbol *completions;
    int completion_count;
    int completion_selected;
    int completion_visible;
    int completion_private;        /* private erişim uyarısı göster */
    int completion_empty;          /* eşleşme yok — "there is no" göster */
    int completion_no_match;       /* "there is no '<word>'" gösteriliyor */
    double completion_debounce_until; /* yazma durduktan sonra otomatik açılma (Faz 4 debounce) */
    char completion_error[256];    /* private erişim mesajı */
    char completion_message[256];  /* "there is no '<word>'" mesajı */

    TreeModel tree;
    int tree_scroll;
    int tree_selected;

    GclIdeSettings settings;
    GclIdeTheme theme;
    int settings_font;
    int settings_theme;

    int ctx_open;
    int ctx_x, ctx_y;
    int ctx_item;
    int ctx_target;

    int name_dialog_open;
    int name_dialog_mode;
    char name_dialog_input[256];

    int folder_picker_open;
    char folder_picker_path[2048];

    char clipboard_path[4096];
    int clipboard_has;

    char status_msg[512];
    double status_msg_time;

    char expanded_dirs[MAX_EXPANDED][2048];
    int expanded_count;

    int active_textbox;
    int theme_dropdown_open;
    int settings_scroll;

    int sidebar_visible;
    int menu_open;

    size_t sel_anchor;

    char *clip_text;
    size_t clip_len;

    size_t text_caret_name;
    size_t text_caret_folder;
    size_t text_sel_name;
    size_t text_sel_folder;

    int sidebar_width;
    int quit;
    double last_tree_scan;

    /* editor navigation key repeat (ok tuşları basılı tutma) */
    int nav_repeat_key;
    double nav_repeat_time;

    /* Son imleç konumu: cursor-follow (imleci görünür tutma) yalnızca imleç
       GERÇEKTEN değiştiğinde çalışır. Fare tekerleği imleci değiştirmez, bu
       yüzden manuel scroll korunur. */
    size_t last_cursor;

    /* output paneli */
    char output[16384];
    size_t output_len;
    int output_visible;
    int output_scroll;   /* output panel dikey kaydırma (satır offset) */
    int running;

    /* blank tab (no file open) */
    GclIdeBuffer blank_tab;

    /* efekt/ayar kopyaları */
    int syntax_highlight;
    int vhs;
    int crt;
    int screen_shake;
    int typewriter;
    int particles;
    int blink_cursor;
    int text_bloom;
    int bloom_strength;     /* 0-100 */
    int shake_strength;     /* 0-100 */
    int particle_strength;  /* 0-100 */
    int vhs_strength;       /* 0-100 */
    int crt_strength;       /* 0-100 */
    int sound;              /* daktilo sesi */
    int editor_font;        /* kod editörü font boyutu (zoom sadece editör) */

    /* efekt state */
    double shake_time;      /* eski — geriye dönük uyumluluk (kullanılmayacak) */
    float shake_x, shake_y;
    float shake_remaining;  /* kalan shake süresi */
    float shake_duration;   /* shake başlangıç süresi */
    float shake_intensity;  /* shake genliği (px) */
    Particle parts[256];
    int part_count;
    double last_typed_key;
    int edited_this_frame;

    /* daktilo pitch (ridiculous_coding pitch_increase) */
    float sound_pitch;      /* anlık çalınacak pitch */
    float pitch_increase;   /* her tuşta artar, zamanla düşer */

    Sound click_snd;
    Sound enter_snd;   /* Enter: güçlü düşük "patlama" */
    Sound delete_snd;  /* Delete/Backspace: farklı "patlama" */

    /* Project sekmesi */
    char current_project[2048];
    int project_dialog_open;      /* New Project: proje adı girilecek */
    char project_name_input[256];
    size_t text_caret_projname;
    size_t text_sel_projname;

    /* New Project: konum seçimi */
    char new_project_path[2048];
    size_t text_caret_projpath;
    size_t text_sel_projpath;

    /* New Project: Lua / Python + Raylib(e) ayrı seçenekler */
    int new_project_open_lua;
    int new_project_open_luaraylib;
    int new_project_open_python;
    int new_project_open_pyraylib;

    /* Yeni Proje Paneli (ide_new_project.c)
       GCL her zaman ana çekirdek; Lua ve Python bağımsız embed toggle'ları.
       Her dil için kendi scene template'i seçilir (0=Empty, 1=2D, 2=3D). */
    int new_project_gcl_scene;       /* GCL (Core) scene */
    int new_project_gcl_raylib;      /* GCL tarafında #native <Raylib> */
    int new_project_lua_scene;       /* Embedded Lua scene */
    int new_project_python_scene;    /* Embedded Python scene */
    int new_project_scroll;          /* panel scroll offset */

    /* Help dialog */
    int help_open;

    /* Uyarı penceresi (proje açılmadan proje işlemi denendiğinde) */
    int warning_open;
    char warning_msg[512];

    /* Asenkron dış süreç (Run) — IDE kilitlenmesin diye popen senkron kullanılmaz */
    GclIdeProcess *run_proc;

    /* Build thread — IDE build'i KENDİ İÇİNDE arka planda yapıyor (GUI kilitlenmez).
       Build fonksiyonu thread'de gcb_build_project_runtime + exe kopyalama yapar;
       main döngü her frame'de build_output'u akıtıp status'u günceller. */
    int build_running;
    int build_done;
    int build_fail;
    char build_output[16384];
    size_t build_output_len;
    char build_step[512];
    char build_base_dir[4096];
    char build_runtime_dir[4096];
    char build_name[256];
    char build_out_dir[2048];
} Editor;

char *gcl_strdup(const char *s);
GclIdeBuffer *editor_cur(Editor *ed);

#define CUR (*editor_cur(&ed))
#define CURP (*editor_cur(ed))

extern const char *gcl_keywords[];
int gcl_is_keyword(const char *w);
int gcl_is_keyword_for(const char *w, const char *lang);
int gcl_is_digit(char c);
int gcl_is_alpha(char c);
int gcl_ident_char(char c);
int gcl_token_type(const char *line, size_t len, size_t start, size_t *tok_len, const char *lang);
Color gcl_token_color(int type, const GclIdeTheme *t);

void editor_typewriter_sound(Editor *ed, int kind); /* 0=normal, 1=enter, 2=delete */
void editor_spawn_particles(Editor *ed, float px, float py, int intensity);
/* İmleç konumunda partikül patlat (Enter/Delete/Backspace efektleri için). */
void editor_spawn_particles_at_cursor(Editor *ed, Rectangle editor_rect);
void editor_update_particles(Editor *ed, float dt);

/* fs */
int fs_is_dir(const char *p);
int fs_mkdir(const char *p);
void fs_copy_file(const char *src, const char *dst);
void fs_copy_dir_rec(const char *src, const char *dst);
void fs_remove_rec(const char *p);
void fs_parent_dir(const char *path, char *out, size_t outsz);
const char *path_basename(const char *p);

/* tab */
void tab_add(Editor *ed, const char *path);
void tab_close(Editor *ed, int idx);
void tab_activate(Editor *ed, int idx);

/* tree */
void tree_clear(TreeModel *tm);
void tree_add(TreeModel *tm, const char *path, int is_dir, int depth);
int is_supported_ext(const char *name);
int is_expanded(Editor *ed, const char *path);
void set_expanded(Editor *ed, const char *path, int expand);
void tree_scan_node(Editor *ed, const char *dir, int depth, int max_depth);
void tree_rescan(Editor *ed);

/* buffer helpers */
char *editor_buffer_text_cstr(const GclIdeBuffer *b);
void buffer_delete_range(GclIdeBuffer *b, size_t start, size_t end);
int editor_is_ident_char(char c);
int editor_is_word_char(char c);
void editor_handle_bracket_close(Editor *ed, char open_char, char close_char);

/* otomatik tamamlama (LSP) */
void editor_show_completion(Editor *ed);
void editor_accept_completion(Editor *ed);

/* GCL workspace-aware sembol tarayıcı (gcl_lsp_scan.c) */
int gcl_lsp_scan(const char *path, const char *text, size_t len,
                 const char *workspace, LspSymbol **out, int *out_count);

/* selection / clipboard */
size_t sel_start(Editor *ed);
size_t sel_end(Editor *ed);
int selection_active(Editor *ed);
void editor_copy(Editor *ed);
void editor_cut(Editor *ed);
void editor_paste(Editor *ed);
void editor_select_all(Editor *ed);

/* typing */
void editor_process_typing(Editor *ed, int ctrl, int shift);

/* widgets */
void textbox_process(char *buf, size_t bufsz, size_t *caret, size_t *sel);
int ui_button(Rectangle r, const char *label, const GclIdeTheme *t, int font_sz);
int ui_checkbox(Rectangle r, const char *label, int checked, const GclIdeTheme *t, int font_sz);
int ui_slider(Rectangle r, const char *label, int *val, int min, int max, const GclIdeTheme *t, int font_sz);
int ui_label_textbox(Rectangle r, char *buf, size_t bufsz, int focused, size_t *caret, size_t *sel, const GclIdeTheme *t, int font_sz);
void draw_panel_frame(Rectangle r, const char *title, const GclIdeTheme *t, int font_sz);

/* context menu */
void run_name_dialog(Editor *ed);
void ctx_execute(Editor *ed);

/* menu */
extern const char *g_menu_titles[];
extern const char *g_menu_items[][8];
extern MenuAction g_menu_actions[][8];
int menu_item_count(int mi);
void editor_get_active_dir(Editor *ed, char *out, size_t outsz);
void editor_run_program(Editor *ed);

/* project */
void project_find_gcdata(Editor *ed, char *out, size_t outsz);
void editor_run_project(Editor *ed);
void run_project_dialog(Editor *ed);
void editor_build_project(Editor *ed);
void editor_build_poll(Editor *ed);

/* Yeni Proje Paneli (ide_new_project.c) */
void ide_new_project_open(Editor *ed);
void ide_new_project_input(Editor *ed, int ctrl);
void ide_new_project_draw(Editor *ed, int w, int h, int font_sz, const GclIdeTheme *t);

void menu_action_run(Editor *ed, MenuAction a);

/* native dialog (ide_native_dialog.c) */
char *gcl_ide_native_dialog(int save, const char *initial_dir);
char *gcl_ide_native_dialog_folder(char *out, size_t outsz);

/* IDE ana döngü */
int gcl_ide_run(const char *path);

/* Settings/About modal çizimleri — gcl_settings_panel.c (bağımsız modül) */

/* gcBundle build — IDE build'i kendi içinde yapar (dış süreç çağırmaz).
   build_project gcl.exe -build'i çağırıyordu; IDE'de GCL_EXE_PATH/argv farklı
   olduğu için gcb_build_project_runtime başarısız oluyordu. Bu yüzden IDE
   doğrudan bu fonksiyonları çağırır. */
int gcb_build_project_runtime(const char *project_dir, const char *runtime_dir,
                              const char *project_name, const char *out_dir);
const char *gcb_last_error(void);

/* gcl_os.c — dizin oluştur (IDE build'inde out_dir için gerekli) */
int gcl_ensure_dir(const char *dir);

#endif
