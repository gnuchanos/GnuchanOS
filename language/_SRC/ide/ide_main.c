#include "gcl_ide_internal.h"

/* Tamamlama seçili elemanı için imza/detay penceresi çizer.
   VSCode signature-help benzeri: ad(parametreler) + detay. Uzun imzayı sarar. */
static void draw_completion_signature(const LspSymbol *s, int px, int py, int pw,
                                      int efont, const GclIdeTheme *t, int *out_h) {
    if (!s || !s->name) { if (out_h) *out_h = 0; return; }
    char sig[512];
    if (s->params && s->params[0])
        snprintf(sig, sizeof(sig), "%s(%s)", s->name, s->params);
    else
        snprintf(sig, sizeof(sig), "%s", s->name);

    int cw = MeasureText("M", efont);
    if (cw < 1) cw = 1;
    int max_chars = (pw - 16) / cw;
    if (max_chars < 10) max_chars = 10;

    char lines[8][256];
    size_t line_offsets[8];
    int nlines = 0;
    size_t idx = 0;
    size_t slen = strlen(sig);
    while (idx < slen && nlines < 8) {
        int taken = 0;
        while (idx < slen && taken < max_chars) {
            lines[nlines][taken++] = sig[idx++];
        }
        /* kelime ortasında kesme — ayraç/boşluk bulana kadar geri al */
        if (idx < slen) {
            while (taken > 1 && lines[nlines][taken-1] != ' ' && lines[nlines][taken-1] != ','
                   && lines[nlines][taken-1] != '(' && lines[nlines][taken-1] != ')') {
                taken--; idx--;
            }
        }
        line_offsets[nlines] = idx - (size_t)taken;
        lines[nlines][taken] = '\0';
        nlines++;
    }
    if (nlines == 0) { lines[0][0] = '\0'; nlines = 1; }

    int line_h = efont + 2;
    int h = nlines * line_h + 8;
    if (s->detail && s->detail[0]) h += line_h;

    DrawRectangle(px, py, pw, h, t->popup_bg);
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)pw, (float)h }, 1.0f, t->accent);

    /* Parametre vurgusu: fonksiyon adı (s->name) normal, parametreler ve parantez
       vurgulu (t->accent) çizilir. Uzun imza satırlara bölünürken hangi karakterin
       parametreye denk geldiğini line_offsets ile takip ederiz. */
    size_t name_len = strlen(s->name);
    int y = py + 4;
    for (int i = 0; i < nlines; i++) {
        size_t lo = (i < 8) ? line_offsets[i] : 0;
        size_t ll = strlen(lines[i]);
        size_t name_end_in_line = (name_len > lo) ? name_len - lo : 0;
        char part[256];
        if (name_end_in_line < ll) {
            /* ad kısmı */
            memcpy(part, lines[i], name_end_in_line);
            part[name_end_in_line] = '\0';
            if (part[0]) DrawText(part, px + 8, y, efont - 1, t->accent);
            /* parametre kısmı */
            DrawText(lines[i] + name_end_in_line, px + 8 + MeasureText(part, efont - 1), y,
                     efont - 1, t->text);
        } else {
            DrawText(lines[i], px + 8, y, efont - 1, t->accent);
        }
        y += line_h;
    }
    if (s->detail && s->detail[0]) {
        DrawText(s->detail, px + 8, y, efont - 2, t->gutter);
    }
    if (out_h) *out_h = h;
}

int gcl_ide_run(const char *path) {
    Editor ed;
    memset(&ed, 0, sizeof(ed));

    gcl_ide_settings_load(&ed.settings);
    ed.settings_font = ed.settings.font_size;
    ed.settings_theme = ed.settings.theme_index;
    ed.theme = gcl_ide_theme_get(ed.settings.theme_index);

    /* efekt/ayar kopyaları */
    ed.syntax_highlight = ed.settings.syntax_highlight;
    ed.vhs = ed.settings.vhs;
    ed.crt = ed.settings.crt;
    ed.screen_shake = ed.settings.screen_shake;
    ed.typewriter = ed.settings.typewriter;
    ed.particles = ed.settings.particles;
    ed.blink_cursor = ed.settings.blink_cursor;
    ed.text_bloom = ed.settings.text_bloom;
    ed.bloom_strength = ed.settings.bloom_strength;
    g_bloom = ed.text_bloom ? ed.bloom_strength : 0;
    ed.shake_strength = ed.settings.shake_strength;
    ed.particle_strength = ed.settings.particle_strength;
    ed.vhs_strength = ed.settings.vhs_strength;
    ed.crt_strength = ed.settings.crt_strength;
    ed.sound = ed.settings.sound;
    ed.editor_font = ed.settings.font_size;
    g_line_h = ed.editor_font + 2;

    /* efekt başlangıç değerleri (ridiculous_coding: ease-out shake + pitch typewriter) */
    ed.sound_pitch = 1.0f;
    ed.pitch_increase = 0.0f;
    ed.shake_remaining = 0.0f;
    ed.shake_duration = 0.0f;
    ed.shake_intensity = 0.0f;

    ed.about = 0;
    ed.settings_open = 0;
    ed.run_mode = 1;
    ed.current_project[0] = '\0';
    ed.new_project_path[0] = '\0';
    ed.text_caret_projpath = 0;
    ed.text_sel_projpath = 0;
    ed.help_open = 0;
    ed.project_dialog_open = 0;
    ed.project_name_input[0] = '\0';
    ed.text_caret_projname = 0;
    ed.text_sel_projname = 0;
    ed.new_project_open_lua = 1;
    ed.new_project_open_luaraylib = 1;
    ed.new_project_open_python = 1;
    ed.new_project_open_pyraylib = 1;
    ed.nav_repeat_key = 0;
    ed.nav_repeat_time = 0.0;
    ed.last_cursor = 0;
    ed.ctx_open = 0;
    ed.ctx_x = 0; ed.ctx_y = 0;
    ed.ctx_item = 0;
    ed.ctx_target = -1;
    ed.name_dialog_open = 0;
    ed.name_dialog_mode = 0;
    ed.name_dialog_input[0] = '\0';
    ed.folder_picker_open = 0;
    ed.folder_picker_path[0] = '\0';
    ed.clipboard_has = 0;
    ed.clipboard_path[0] = '\0';
    ed.expanded_count = 0;
    ed.tree_scroll = 0;
    ed.tree_selected = -1;
    ed.active_textbox = -1;
    ed.theme_dropdown_open = 0;
    ed.sidebar_visible = 1;
    ed.menu_open = 0;
    ed.sel_anchor = 0;
    ed.clip_text = NULL;
    ed.clip_len = 0;
    ed.text_caret_name = 0;
    ed.text_caret_folder = 0;
    ed.text_sel_name = 0;
    ed.text_sel_folder = 0;
    ed.tabs = NULL;
    ed.tab_count = 0;
    ed.active_tab = -1;
    ed.sidebar_width = SIDEBAR_W;
    ed.quit = 0;
    ed.edited_this_frame = 0;
    ed.output[0] = '\0';
    ed.output_len = 0;
    ed.output_visible = 0;
    ed.output_scroll = 0;  /* 0 = en alt (follow); pozitif = yukarı kaydırılmış satır sayısı */
    ed.running = 0;

#ifndef _WIN32
    if (getcwd(ed.cwd, sizeof(ed.cwd)) == NULL) snprintf(ed.cwd, sizeof(ed.cwd), ".");
#else
    snprintf(ed.cwd, sizeof(ed.cwd), ".");
#endif

    gcl_ide_buffer_init(&ed.blank_tab);
    if (path && path[0]) {
        /* Doc: gcl -ide D:\path → dizin workspace olarak açılır, dosya tab olarak yüklenir. */
        if (fs_is_dir(path)) {
            snprintf(ed.cwd, sizeof(ed.cwd), "%s", path);
            ed.tab_count = 0;
            ed.active_tab = -1;
        } else {
            tab_add(&ed, path);
        }
    } else {
        ed.tab_count = 0;
        ed.active_tab = -1;
    }
    tree_rescan(&ed);
    ed.last_tree_scan = GetTime();

    InitWindow(1200, 760, "GCL IDE");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable default ESC exit

    /* Title bar icon - assets/logo.png */
    Image icon = LoadImage("assets/logo.png");
    if (icon.data != NULL) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    /* GNU FreeFont - FreeMono.ttf (monospace, SIL OFL 1.1 license).
       Byte array olarak gömüldü; LoadFontFromMemory dosya yolu sorununu kökten çözer.
       Texture filtresi BILINEAR: zoom out'ta (16px→8px) pikselleşme yerine yumuşak
       (yumuşatılmış) metin çizilir. Varsayılan POINT filtre küçük boyutta berbat görünüm verir. */
    {
        /* Font texture'ı YÜKSEK çözünürlükte (48px) üretilir; DrawTextEx(fontSize) ile
           8-32px arası ölçeklenirken BILINEAR filtre sayesinde zoom out'ta yumuşak,
           zoom in'de net görünür. Küçük (16px) texture'a ölçeklemek pikselleşmeyi artırırdı. */
        int tf = 48;
        Font f = LoadFontFromMemory(".ttf", gcl_embed_freemono_ttf,
                                    (int)gcl_embed_freemono_ttf_size, tf, NULL, 0);
        if (f.texture.id > 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            g_font = f;
        }
    }

    InitAudioDevice();
    {
        const unsigned int rate = 22050;
        const unsigned int samples = rate * 60 / 1000;

        /* Normal daktilo tık sesi (yazma karakter) */
        short *buf = (short *)malloc(sizeof(short) * samples);
        if (buf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float v = (sinf(2 * 3.14159f * 160.0f * t_) * 0.7f +
                           sinf(2 * 3.14159f * 90.0f * t_) * 0.4f +
                           noise * 0.3f) * env * 0.6f;
                buf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = buf;
            ed.click_snd = LoadSoundFromWave(w);
            free(buf);
        }

        /* Enter: güçlü düşük patlama (60Hz + sub) */
        short *ebuf = (short *)malloc(sizeof(short) * samples);
        if (ebuf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float v = (sinf(2 * 3.14159f * 60.0f * t_) * 0.8f +
                           sinf(2 * 3.14159f * 45.0f * t_) * 0.5f +
                           sinf(2 * 3.14159f * 130.0f * t_) * 0.3f +
                           noise * 0.4f) * env * 0.7f;
                ebuf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = ebuf;
            ed.enter_snd = LoadSoundFromWave(w);
            free(ebuf);
        }

        /* Delete/Backspace: farklı, daha tiz patlama (chirp 200→600Hz) */
        short *dbuf = (short *)malloc(sizeof(short) * samples);
        if (dbuf) {
            for (unsigned int i = 0; i < samples; i++) {
                float t_ = (float)i / (float)rate;
                float env = 1.0f - (float)i / (float)samples;
                float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                float freq = 200.0f + 400.0f * env;
                float v = (sinf(2 * 3.14159f * freq * t_) * 0.6f +
                           sinf(2 * 3.14159f * 950.0f * t_) * 0.25f +
                           noise * 0.35f) * env * 0.6f;
                dbuf[i] = (short)(v * 32767.0f);
            }
            Wave w = { 0 };
            w.frameCount = samples;
            w.sampleRate = rate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = dbuf;
            ed.delete_snd = LoadSoundFromWave(w);
            free(dbuf);
        }
    }

    while (!WindowShouldClose() && !ed.quit) {
        /* Her frame'de çalışan dış sürecin çıktısını topla (asenkron run) */
        gcl_ide_proc_poll(&ed);

        /* Build thread'i poll et — GUI kilitlenmeden build adımları akar */
        editor_build_poll(&ed);
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        int font_sz = ed.settings.font_size;
        GclIdeTheme t = ed.theme;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        int content_top = MENU_H;
        Rectangle menu_rect = { 0, 0, (float)w, MENU_H };
        Rectangle sidebar_rect = { 0, (float)content_top, (float)ed.sidebar_width, (float)(h - content_top - STATUS_H) };
        float right_x = ed.sidebar_visible ? (float)ed.sidebar_width : 0;
        float right_w = (float)w - right_x;
        Rectangle tabbar_rect = { right_x, (float)content_top, right_w, TAB_H };
        int output_h = ed.output_visible ? 160 : 0;
        Rectangle editor_rect = { right_x, (float)(content_top + TAB_H), right_w, (float)(h - content_top - TAB_H - STATUS_H - output_h) };
        Rectangle output_rect = { right_x, (float)(h - STATUS_H - output_h), right_w, (float)output_h };
        Rectangle status_rect = { 0, (float)(h - STATUS_H), (float)w, STATUS_H };

        bool blocked = ed.settings_open || ed.name_dialog_open || ed.folder_picker_open || ed.project_dialog_open || ed.about || ed.menu_open || ed.help_open || ed.warning_open;

        /* sidebar boyutlandırma (sağ kenardan sürüklenebilir) */
        if (ed.sidebar_visible && !blocked) {
            Vector2 mp = GetMousePosition();
            Rectangle handle = { (float)(ed.sidebar_width - 4), (float)content_top + TAB_H, 8, (float)(h - content_top - TAB_H - STATUS_H) };
            if (CheckCollisionPointRec(mp, handle) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                ed.sidebar_width = (int)mp.x;
                if (ed.sidebar_width < 120) ed.sidebar_width = 120;
                if (ed.sidebar_width > w / 2) ed.sidebar_width = w / 2;
            }
        }

        /* periyodik explorer yenileme — modal/name_dialog açıkken çalışma (GUI kilitlenmesin).
           tree_rescan senkron I/O yapar; büyük dizinde her 1 sn'de bir bloklayıcı olur.
           Modal açıkken (blocked) çağırmak "hiçbir şeye tıklanamıyor" durumuna yol açar. */
        if (!blocked && (GetTime() - ed.last_tree_scan) > 1.0) {
            ed.last_tree_scan = GetTime();
            tree_rescan(&ed);
            if (ed.tree_selected >= ed.tree.count) ed.tree_selected = -1;
        }

        /* ---- input ---- */
        ed.edited_this_frame = 0;
        if (ed.settings_open && !ed.about) {
            gcl_settings_panel_input(&ed, ctrl);
        } else if (ed.name_dialog_open) {
            if (ed.active_textbox == 1) textbox_process(ed.name_dialog_input, sizeof(ed.name_dialog_input), &ed.text_caret_name, &ed.text_sel_name);
            if (IsKeyPressed(KEY_ESCAPE)) { ed.name_dialog_open = 0; ed.ctx_open = 0; ed.active_textbox = -1; }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) run_name_dialog(&ed);
        } else if (ed.folder_picker_open) {
            if (ed.active_textbox == 2) textbox_process(ed.folder_picker_path, sizeof(ed.folder_picker_path), &ed.text_caret_folder, &ed.text_sel_folder);
            if (IsKeyPressed(KEY_ESCAPE)) { ed.folder_picker_open = 0; ed.active_textbox = -1; }
        } else if (ed.project_dialog_open) {
            ide_new_project_input(&ed, ctrl);
        } else if (ed.help_open) {
            if (IsKeyPressed(KEY_ESCAPE)) ed.help_open = 0;
        } else if (ed.warning_open) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                ed.warning_open = 0;
            }
        } else {
            editor_process_typing(&ed, ctrl, shift);

            /* Faz 4: debounce — yazma durduktan 200ms sonra otomatik tamamlama açılır.
               editor_process_typing yalnızca completion_debounce_until'ü set eder;
               burada süre dolduysa pencereyi açar. Ctrl+Space hâlâ anında zorlar. */
            if (!ed.completion_visible && ed.completion_debounce_until > 0.0 &&
                GetTime() >= ed.completion_debounce_until) {
                ed.completion_debounce_until = 0.0;
                editor_show_completion(&ed);
            }

            /* Ctrl+MouseWheel zoom (sadece kod editörü) */
            if (ctrl) {
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    ed.editor_font += (int)mw;
                    if (ed.editor_font < 8) ed.editor_font = 8;
                    if (ed.editor_font > 32) ed.editor_font = 32;
                    g_line_h = ed.editor_font + 2;
                }
            } else if (shift) {
                /* Shift+MouseWheel yatay kaydırma */
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    int step_chars = 8;
                    if (mw > 0) {
                        CUR.scroll_x = CUR.scroll_x > (size_t)step_chars ? CUR.scroll_x - (size_t)step_chars : 0;
                    } else {
                        CUR.scroll_x += (size_t)step_chars;
                    }
                }
            } else {
                /* Normal MouseWheel: dikey kaydırma (imleç değil, scroll_y değişir). */
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    /* Fare output panel üzerindeyken output'u kaydır */
                    if (ed.output_visible && CheckCollisionPointRec(GetMousePosition(), output_rect)) {
                        int out_line_h = ed.editor_font + 2;
                        int out_vis = (int)((output_rect.height - 28) / out_line_h);
                        if (out_vis < 1) out_vis = 1;
                        int total_out = 0;
                        for (char *s = ed.output; *s; ) {
                            char *nl = strchr(s, '\n');
                            if (!nl) { total_out++; break; }
                            total_out++;
                            s = nl + 1;
                        }
                        int max_out = total_out > out_vis ? total_out - out_vis : 0;
                        if (mw > 0) {
                            /* yukarı kaydır — bottom-relative: artır */
                            if (ed.output_scroll < max_out) ed.output_scroll++;
                        } else {
                            /* aşağı kaydır — bottom-relative: azalt */
                            if (ed.output_scroll > 0) ed.output_scroll--;
                        }
                    } else {
                        size_t total_lines = gcl_ide_buffer_line_count(&CUR);
                        size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
                        if (vis < 1) vis = 1;
                        if (mw > 0) {
                            if (CUR.scroll_y > 0) CUR.scroll_y -= (size_t)mw;
                        } else {
                            size_t max_s = total_lines > vis ? total_lines - vis : 0;
                            if (CUR.scroll_y + (size_t)(-mw) > max_s) CUR.scroll_y = max_s;
                            else CUR.scroll_y += (size_t)(-mw);
                        }
                    }
                }
            }

            if (ed.edited_this_frame) {
                size_t cl = gcl_ide_buffer_line_of_cursor(&CUR);
                size_t cc = gcl_ide_buffer_cursor_in_line(&CUR);
                int ppx = (int)editor_rect.x + GUTTER_W;
                int ppy = (int)editor_rect.y + 4 + (int)(cl - CUR.scroll_y) * LINE_H + LINE_H / 2;
                if (cc > 0) {
                    size_t cll = 0;
                    const char *cll_str = gcl_ide_buffer_line_at(&CUR, cl, &cll);
                    size_t cpc = cc;
                    if (cpc > cll) cpc = cll;
                    char cpre[4096];
                    if (cpc > sizeof(cpre)-1) cpc = sizeof(cpre)-1;
                    memcpy(cpre, cll_str, cpc); cpre[cpc] = '\0';
                    ppx += MeasureText(cpre, ed.editor_font);
                }
                if (ed.particles) editor_spawn_particles(&ed, (float)ppx, (float)ppy, ed.particle_strength);
                if (ed.screen_shake) {
                    float inten = (float)ed.shake_strength / 100.0f;
                    if (inten < 0.1f) inten = 0.1f;
                    ed.shake_remaining = 0.15f;
                    ed.shake_duration = 0.15f;
                    ed.shake_intensity = 6.0f * inten;
                }
                ed.edited_this_frame = 0;
            }

            if (ctrl && IsKeyPressed(KEY_N)) tab_add(&ed, NULL);
            if (ctrl && IsKeyPressed(KEY_O)) {
                char dir[2048];
                editor_get_active_dir(&ed, dir, sizeof(dir));
                char *p = gcl_ide_native_dialog(0, dir);
                if (p) { tab_add(&ed, p); free(p); }
            }
            if (ctrl && IsKeyPressed(KEY_S)) {
                GclIdeBuffer *b = &CUR;
                if (b->path) gcl_ide_buffer_save(b);
                else {
                    char dir[2048];
                    editor_get_active_dir(&ed, dir, sizeof(dir));
                    char *p = gcl_ide_native_dialog(1, dir);
                    if (p) { free(b->path); b->path = gcl_strdup(p); gcl_ide_buffer_save(b); free(p); }
                }
            }
            if (ctrl && IsKeyPressed(KEY_C)) editor_copy(&ed);
            if (ctrl && IsKeyPressed(KEY_X)) editor_cut(&ed);
            if (ctrl && IsKeyPressed(KEY_V)) editor_paste(&ed);
            if (ctrl && IsKeyPressed(KEY_A)) editor_select_all(&ed);
            if (ctrl && IsKeyPressed(KEY_J)) ed.output_visible = !ed.output_visible;
            if (ctrl && IsKeyPressed(KEY_E)) {
                gcl_settings_panel_open(&ed);
            }
            if (shift && IsKeyPressed(KEY_SPACE)) ed.sidebar_visible = !ed.sidebar_visible;

            /* Run: F5 veya Ctrl+R */
            if (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_R))) editor_run_program(&ed);

            /* Undo: Ctrl+Z  Redo: Ctrl+Y */
            if (ctrl && IsKeyPressed(KEY_Z)) gcl_ide_buffer_undo(&CUR);
            if (ctrl && IsKeyPressed(KEY_Y)) gcl_ide_buffer_redo(&CUR);

            /* Ctrl+Space: otomatik tamamlama penceresini ZORLA aç */
            if (ctrl && IsKeyPressed(KEY_SPACE)) {
                editor_show_completion(&ed);
            }

            if (!(ed.completion_visible && (ed.completion_count > 0 || ed.completion_no_match))) {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    gcl_ide_buffer_insert_newline(&CUR); ed.sel_anchor = CUR.cursor;
                    if (ed.typewriter) editor_typewriter_sound(&ed, 1); /* Enter: güçlü patlama */
                    editor_spawn_particles_at_cursor(&ed, editor_rect);
                }
                if (IsKeyPressed(KEY_TAB)) { for (int i=0;i<4;i++) gcl_ide_buffer_insert_char(&CUR,' '); ed.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); ed.sel_anchor=CUR.cursor; }
                    else { gcl_ide_buffer_backspace(&CUR); ed.sel_anchor = CUR.cursor; }
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2); /* Backspace: tiz patlama */
                    editor_spawn_particles_at_cursor(&ed, editor_rect);
                }
                if (IsKeyPressed(KEY_DELETE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); ed.sel_anchor=CUR.cursor; }
                    else { gcl_ide_buffer_delete(&CUR); }
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2); /* Delete: tiz patlama */
                    editor_spawn_particles_at_cursor(&ed, editor_rect);
                }
                /* Ok tuşları: basılı tutma desteği (key repeat). */
                int nav_key = 0;
                if (IsKeyPressed(KEY_LEFT) || IsKeyDown(KEY_LEFT)) nav_key = KEY_LEFT;
                else if (IsKeyPressed(KEY_RIGHT) || IsKeyDown(KEY_RIGHT)) nav_key = KEY_RIGHT;
                else if (IsKeyPressed(KEY_UP) || IsKeyDown(KEY_UP)) nav_key = KEY_UP;
                else if (IsKeyPressed(KEY_DOWN) || IsKeyDown(KEY_DOWN)) nav_key = KEY_DOWN;
                double now_t = GetTime();
                if (nav_key != 0) {
                    bool first_press = IsKeyPressed(nav_key);
                    if (first_press || ed.nav_repeat_key != nav_key) {
                        ed.nav_repeat_key = nav_key;
                        ed.nav_repeat_time = now_t;
                        if (nav_key == KEY_LEFT) { gcl_ide_buffer_cursor_left(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_RIGHT) { gcl_ide_buffer_cursor_right(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_UP) { gcl_ide_buffer_cursor_up(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_DOWN) { gcl_ide_buffer_cursor_down(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        ed.nav_repeat_time = now_t + 0.40;  /* 400ms sonra tekrar başla */
                    } else if (now_t >= ed.nav_repeat_time) {
                        if (nav_key == KEY_LEFT) { gcl_ide_buffer_cursor_left(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_RIGHT) { gcl_ide_buffer_cursor_right(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_UP) { gcl_ide_buffer_cursor_up(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        else if (nav_key == KEY_DOWN) { gcl_ide_buffer_cursor_down(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                        ed.nav_repeat_time = now_t + 0.03;  /* ~30ms hız */
                    }
                } else {
                    ed.nav_repeat_key = 0;
                    ed.nav_repeat_time = 0.0;
                }
                if (IsKeyPressed(KEY_HOME)) { gcl_ide_buffer_cursor_line_start(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_END)) { gcl_ide_buffer_cursor_line_end(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_PAGE_UP)) { for (int i=0;i<20;i++) gcl_ide_buffer_cursor_up(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
                if (IsKeyPressed(KEY_PAGE_DOWN)) { for (int i=0;i<20;i++) gcl_ide_buffer_cursor_down(&CUR); if (!shift) ed.sel_anchor = CUR.cursor; }
            } else {
                /* otomatik tamamlama navigasyonu */
                if (ctrl && IsKeyPressed(KEY_SPACE)) editor_show_completion(&ed);
                if (IsKeyPressed(KEY_UP)) { if (ed.completion_selected > 0) ed.completion_selected--; }
                if (IsKeyPressed(KEY_DOWN)) { if (ed.completion_selected < ed.completion_count - 1) ed.completion_selected++; }
                if (IsKeyPressed(KEY_ESCAPE)) ed.completion_visible = 0;
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_TAB)) editor_accept_completion(&ed);
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); ed.sel_anchor=CUR.cursor; }
                    else { gcl_ide_buffer_backspace(&CUR); ed.sel_anchor = CUR.cursor; }
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2);
                    editor_spawn_particles_at_cursor(&ed, editor_rect);
                    editor_show_completion(&ed);
                }
                if (IsKeyPressed(KEY_DELETE)) {
                    if (selection_active(&ed)) { size_t s0=sel_start(&ed),s1=sel_end(&ed); buffer_delete_range(&CUR,s0,s1); ed.sel_anchor=CUR.cursor; }
                    else { gcl_ide_buffer_delete(&CUR); }
                    if (ed.typewriter) editor_typewriter_sound(&ed, 2);
                    editor_spawn_particles_at_cursor(&ed, editor_rect);
                    editor_show_completion(&ed);
                }
            }

            if (IsFileDropped()) {
                FilePathList fpl = LoadDroppedFiles();
                for (unsigned int i = 0; i < fpl.count; i++) {
                    const char *src = fpl.paths[i];
                    char dst[8192];
                    snprintf(dst, sizeof(dst), "%s/%s", ed.cwd, path_basename(src));
                    if (fs_is_dir(src)) fs_copy_dir_rec(src, dst);
                    else fs_copy_file(src, dst);
                }
                UnloadDroppedFiles(fpl);
                ed.last_tree_scan = 0;  /* geciktirilmiş tarama */
            }
        }

        /* ---- draw ---- */
        BeginDrawing();

        /* screen shake — ease-out (ridiculous_coding): kalan süreyle genlik düşer */
        if (ed.screen_shake && ed.shake_remaining > 0.0f) {
            float t = ed.shake_remaining / (ed.shake_duration > 0.0f ? ed.shake_duration : 1.0f);
            float mag = ed.shake_intensity * t * t; /* ease-out quadratic */
            ed.shake_x = ((float)(rand() % 200) - 100.0f) / 100.0f * mag;
            ed.shake_y = ((float)(rand() % 200) - 100.0f) / 100.0f * mag;
            rlPushMatrix();
            rlTranslatef(ed.shake_x, ed.shake_y, 0.0f);
        }

        /* menü bar */
        DrawRectangleRec(menu_rect, t.menu_bg);
        {
            float mx0 = 8;
            for (int i = 0; g_menu_titles[i]; i++) {
                int tw = MeasureText(g_menu_titles[i], font_sz);
                Rectangle r = { mx0, 2, (float)(tw + 16), (float)(MENU_H - 4) };
                bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                DrawText(g_menu_titles[i], (int)(mx0 + 8), 6, font_sz,
                         (hover || ed.menu_open == i + 1) ? t.accent : t.text);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ed.settings_open && !ed.name_dialog_open && !ed.folder_picker_open && !ed.about) {
                    ed.menu_open = (ed.menu_open == i + 1) ? 0 : (i + 1);
                }
                mx0 += tw + 20;
            }
            /* proje adı top-right */
            if (ed.current_project[0]) {
                const char *pname = path_basename(ed.current_project);
                int pw = MeasureText(pname, font_sz);
                DrawText(pname, w - pw - 12, 6, font_sz, t.text);
            }
        }

        /* explorer */
        if (ed.sidebar_visible) {
            DrawRectangleRec(sidebar_rect, t.tree_bg);
            const char *hdr = "EXPLORER";
            DrawText(hdr, (int)(sidebar_rect.x + 8), (int)(sidebar_rect.y + 6), font_sz, t.accent);
            DrawRectangle((int)sidebar_rect.x, (int)(sidebar_rect.y + 28), (int)sidebar_rect.width, 1, t.accent);

            int item_h = 18;
            Rectangle list_rect = { sidebar_rect.x + 4, sidebar_rect.y + 32, sidebar_rect.width - 8, sidebar_rect.height - 40 };
            BeginScissorMode((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height);
            int total = ed.tree.count;
            for (int i = ed.tree_scroll; i < total; i++) {
                int y = (int)list_rect.y + (i - ed.tree_scroll) * item_h;
                if (y + item_h > (int)(list_rect.y + list_rect.height)) break;
                TreeNode *node = &ed.tree.nodes[i];
                const char *bn = path_basename(node->path);
                int indent = node->depth * 14;
                if (i == ed.tree_selected) DrawRectangle((int)list_rect.x, y, (int)list_rect.width, item_h, t.selection);
                if (node->is_dir) {
                    if (is_expanded(&ed, node->path)) DrawText("v", (int)(list_rect.x + 4 + indent), y + 2, font_sz - 2, t.text);
                    else DrawText(">", (int)(list_rect.x + 4 + indent), y + 2, font_sz - 2, t.text);
                    DrawText(bn, (int)(list_rect.x + 18 + indent), y + 2, font_sz - 2, t.text);
                } else {
                    DrawRectangle((int)(list_rect.x + 4 + indent), y + 6, 8, 6, t.accent);
                    DrawText(bn, (int)(list_rect.x + 18 + indent), y + 2, font_sz - 2, i == ed.tree_selected ? WHITE : t.text);
                }
            }
            EndScissorMode();

            /* tree click handling - context menu açıkken çalışmaz */
            if (!blocked && !ed.ctx_open) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    Vector2 mp = GetMousePosition();
                    if (CheckCollisionPointRec(mp, list_rect)) {
                        int row = ed.tree_scroll + (int)((mp.y - list_rect.y) / item_h);
                        if (row >= 0 && row < total) {
                            ed.tree_selected = row;
                            TreeNode *node = &ed.tree.nodes[row];
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                if (node->is_dir) {
                                    if (is_expanded(&ed, node->path)) set_expanded(&ed, node->path, 0);
                                    else set_expanded(&ed, node->path, 1);
                                    ed.last_tree_scan = 0;  /* geciktirilmiş tarama */
                                } else {
                                    tab_add(&ed, node->path);
                                }
                            } else {
                                ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                                ed.ctx_target = row; ed.ctx_item = CTX_NEW_FILE;
                            }
                        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                            ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                            ed.ctx_target = -1; ed.ctx_item = CTX_NEW_FILE;
                        }
                    } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, sidebar_rect)) {
                        ed.ctx_open = 1; ed.ctx_x = (int)mp.x; ed.ctx_y = (int)mp.y;
                        ed.ctx_target = -1; ed.ctx_item = CTX_NEW_FILE;
                    }
                }
            }

            /* explorer context menu */
            if (ed.ctx_open) {
                const char *items[] = { "New File", "New Directory", "Rename", "Copy", "Paste", "Delete", NULL };
                int iw = 160, ih = 24, n = 6;
                int mxm = ed.ctx_x, mym = ed.ctx_y;
                if (mxm + iw > w) mxm = w - iw;
                if (mym + n * ih > h) mym = h - n * ih;
                DrawRectangle(mxm, mym, iw, n * ih, t.popup_bg);
                DrawRectangleLines((float)mxm, (float)mym, (float)iw, (float)(n * ih), t.accent);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 mp = GetMousePosition();
                    if (!CheckCollisionPointRec(mp, (Rectangle){(float)mxm, (float)mym, (float)iw, (float)(n * ih)})) ed.ctx_open = 0;
                    else {
                        int idx = (int)((mp.y - mym) / ih);
                        if (idx >= 0 && idx < n) { ed.ctx_item = idx; ctx_execute(&ed); }
                    }
                }
                for (int i = 0; i < n; i++) {
                    int y = mym + i * ih;
                    Rectangle it = { (float)mxm, (float)y, (float)iw, (float)ih };
                    bool hover = CheckCollisionPointRec(GetMousePosition(), it);
                    if (hover && ed.ctx_item == i) DrawRectangle(mxm, y, iw, ih, t.accent);
                    DrawText(items[i], mxm + 6, y + 4, font_sz - 2, hover ? WHITE : t.text);
                }
            }
        }

        /* tab bar - editor alanının üstünde; menü popup açıkken tıklamaları yok say.
           Modal (settings, build, project vb.) açıkken tıklamalar yutulur — aksi halde
           settings dialogunun üstüne denk gelen tıklama alttaki tab butonunu da tetikler. */
        DrawRectangleRec(tabbar_rect, t.menu_bg);
        if (!ed.menu_open && !blocked) {
            float tx = tabbar_rect.x + 6;
            for (int i = 0; i < ed.tab_count; i++) {
                const char *name = ed.tabs[i].path ? path_basename(ed.tabs[i].path) : "untitled";
                int tw = MeasureText(name, font_sz - 2) + 36;
                Rectangle tr = { tx, tabbar_rect.y + 2, (float)tw, (float)(TAB_H - 4) };
                bool active = (i == ed.active_tab);
                DrawRectangleRec(tr, active ? t.panel_bg : t.menu_bg);
                DrawRectangleLinesEx(tr, 1, active ? t.accent : t.gutter);
                DrawText(name, (int)(tx + 8), (int)(tabbar_rect.y + 6), font_sz - 2, active ? t.text : t.gutter);
                Rectangle close = { tx + tw - 20, tabbar_rect.y + 6, 14, 14 };
                if (CheckCollisionPointRec(GetMousePosition(), close)) {
                    DrawText("x", (int)(close.x + 2), (int)(close.y), font_sz - 2, t.error);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { tab_close(&ed, i); break; }
                }
                if (CheckCollisionPointRec(GetMousePosition(), tr) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (!CheckCollisionPointRec(GetMousePosition(), close)) tab_activate(&ed, i);
                }
                tx += tw + 4;
            }
            Rectangle plus = { tx + 2, tabbar_rect.y + 2, 24, (float)(TAB_H - 4) };
            DrawText("+", (int)(plus.x + 6), (int)(plus.y + 6), font_sz, t.text);
            if (CheckCollisionPointRec(GetMousePosition(), plus) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) tab_add(&ed, NULL);
        }

        /* editor */
        int efont = ed.editor_font;
        float fcw = gcl_measure_text_f("M", efont);
        if (fcw < 1.0f) fcw = 1.0f;
        int cw = (int)(fcw + 0.5f);
        if (cw < 1) cw = 1;
        /* cursor X görünürlük: imleç ekran dışına çıkınca yatay kaydır.
           ccol_t artık UTF-8 KARAKTER sayısı; scroll_x da karakter sayısı olarak tutulur.
           Böylece bayt/karakter uyumsuzluğu "cursor burada görünüyor ama yazı oraya" hissini yok eder. */
        {
            size_t ccol_t = gcl_ide_buffer_cursor_in_line_chars(&CUR);
            int view_chars = ((int)editor_rect.width - GUTTER_W - 8) / cw;
            if (view_chars < 1) view_chars = 1;
            if ((int)ccol_t < (int)CUR.scroll_x) CUR.scroll_x = ccol_t;
            else if ((int)ccol_t >= (int)CUR.scroll_x + view_chars) CUR.scroll_x = (int)ccol_t - view_chars + 1;
        }
        /* cursor Y görünürlük: imleç ekran dışına çıkınca dikey kaydır (scroll_y cursor'ı takip eder).
           Yalnızca imleç GERÇEKTEN değiştiyse çalışır (klik, yazma, ok tuşu). Fare tekerleği
           imleci değiştirmediği için manuel scroll korunur ve "aşağı inince yukarı atma" olmaz. */
        if (CUR.cursor != ed.last_cursor) {
            size_t cline_t = gcl_ide_buffer_line_of_cursor(&CUR);
            size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
            if (vis < 1) vis = 1;
            if (cline_t < CUR.scroll_y) CUR.scroll_y = cline_t;
            else if (cline_t >= CUR.scroll_y + vis) CUR.scroll_y = cline_t - vis + 1;
        }
        ed.last_cursor = CUR.cursor;
        /* Yatay kaydırma offset'i: CUR.scroll_x UTF-8 KARAKTER sayısı.
           Çizim monospace sabit adım (cw) kullanıyor — sx = cw * scroll_x (float).
           Böylece ölçüm/çizim birebir eşleşir, `,` vb. sembollerde kümülatif kayma yok. */
        float sx_f = fcw * (float)CUR.scroll_x;
        int sx = (int)(sx_f + 0.5f);
        DrawRectangleRec(editor_rect, t.bg);
        size_t total_lines = gcl_ide_buffer_line_count(&CUR);
        size_t vis = (size_t)((editor_rect.height - 8) / LINE_H);
        if (vis < 1) vis = 1;
        if (total_lines > vis) {
            float bar_h = editor_rect.height * ((float)vis / (float)total_lines);
            float bar_y = editor_rect.y + (editor_rect.height - bar_h) * ((float)CUR.scroll_y / (float)(total_lines - vis));
            DrawRectangle((int)(editor_rect.x + editor_rect.width - 6), (int)bar_y, 6, (int)bar_h, t.gutter);
        }
        BeginScissorMode((int)editor_rect.x, (int)editor_rect.y, (int)editor_rect.width, (int)editor_rect.height);
        size_t cur_line = gcl_ide_buffer_line_of_cursor(&CUR);
        if (cur_line >= CUR.scroll_y) {
            int ly = (int)editor_rect.y + 4 + (int)(cur_line - CUR.scroll_y) * LINE_H;
            DrawRectangle((int)editor_rect.x, ly, (int)editor_rect.width, LINE_H, t.line_bg);
        }
        if (CUR.content && CUR.size > 0) {
            size_t s0 = sel_start(&ed), s1 = sel_end(&ed);
            if (s0 < s1) {
                for (size_t i = CUR.scroll_y; i < total_lines; i++) {
                    int y = (int)editor_rect.y + 4 + (int)(i - CUR.scroll_y) * LINE_H;
                    if (y > (int)(editor_rect.y + editor_rect.height)) break;
                    size_t l = 0;
                    const char *line = gcl_ide_buffer_line_at(&CUR, i, &l);
                    size_t off = (size_t)(line - CUR.content);
                    size_t le = off + l;
                    if (s0 <= le && s1 >= off) {
                        size_t cs = s0 > off ? s0 - off : 0;
                        size_t ce = s1 < le ? s1 - off : le - off;
                        if (cs < ce) {
                            char ps[4096], pe[4096];
                            if (cs > sizeof(ps) - 1) cs = sizeof(ps) - 1;
                            if (ce > sizeof(pe) - 1) ce = sizeof(pe) - 1;
                            memcpy(ps, line, cs); ps[cs] = '\0';
                            memcpy(pe, line, ce); pe[ce] = '\0';
                            int x0 = (int)editor_rect.x + GUTTER_W + MeasureText(ps, efont) - sx;
                            int x1 = (int)editor_rect.x + GUTTER_W + MeasureText(pe, efont) - sx;
                            if (x1 > x0) DrawRectangle(x0, y, x1 - x0, LINE_H, t.selection);
                        }
                    }
                }
            }
        }
        for (size_t i = CUR.scroll_y; i < total_lines; i++) {
            int y = (int)editor_rect.y + 4 + (int)(i - CUR.scroll_y) * LINE_H;
            if (y > (int)(editor_rect.y + editor_rect.height)) break;
            size_t l = 0;
            const char *line = gcl_ide_buffer_line_at(&CUR, i, &l);
            char num[24]; snprintf(num, sizeof(num), "%zu", i + 1);
            DrawText(num, (int)(editor_rect.x + GUTTER_W - 8 - MeasureText(num, efont)), y, efont, t.gutter);
            char linebuf[4096];
            if (l >= sizeof(linebuf)) l = sizeof(linebuf) - 1;
            memcpy(linebuf, line, l); linebuf[l] = '\0';
            if (ed.syntax_highlight) {
                /* chunk çizimi: FLOAT akümülatör. `,` gibi her sembol ayrı chunk olduğunda
                   px += MeasureText(chunk) int yuvarlaması kümülatif hata biriktirir ve
                   imleç/metin ayrışır. gcl_measure_text_f ile gerçek float genişlik toplamı
                   DrawTextEx'in çizim konumuyla birebir eşleşir. */
                float px = (float)editor_rect.x + GUTTER_W - (float)sx;
                size_t pos = 0;
                while (pos < l) {
                    size_t tl = 0;
                    /* Aktif dosyanın uzantısına göre dil belirle: .py → python, .lua → lua, diğer → gcl */
                    const char *ext = CUR.path ? strrchr(CUR.path, '.') : NULL;
                    const char *hl_lang = "gcl";
                    if (ext && strcmp(ext, ".py") == 0) hl_lang = "python";
                    else if (ext && strcmp(ext, ".lua") == 0) hl_lang = "lua";
                    int tt = gcl_token_type(linebuf, l, pos, &tl, hl_lang);
                    if (tl == 0) tl = 1;
                    if (tl > l - pos) tl = l - pos;
                    char chunk[4096];
                    if (tl > sizeof(chunk) - 1) tl = sizeof(chunk) - 1;
                    memcpy(chunk, linebuf + pos, tl); chunk[tl] = '\0';
                    if (chunk[0] != '\0') {
                        Color col = gcl_token_color(tt, &t);
                        gcl_draw_text_f(chunk, px, y, efont, col);
                        px += gcl_measure_text_f(chunk, efont);
                    }
                    pos += tl;
                }
            } else {
                DrawText(linebuf, (int)(editor_rect.x + GUTTER_W - sx), y, efont, t.text);
            }
        }
        if (cur_line >= CUR.scroll_y) {
            int cy = (int)editor_rect.y + 4 + (int)(cur_line - CUR.scroll_y) * LINE_H;
            size_t l = 0;
            const char *cline = gcl_ide_buffer_line_at(&CUR, cur_line, &l);
            size_t cc = gcl_ide_buffer_cursor_in_line(&CUR);
            if (cc > l) cc = l;
            char prefix[4096];
            if (cc > sizeof(prefix) - 1) cc = sizeof(prefix) - 1;
            memcpy(prefix, cline, cc); prefix[cc] = '\0';
            int cx = (int)editor_rect.x + GUTTER_W + MeasureText(prefix, efont) - sx;
            if (ed.blink_cursor) {
                if ((int)(GetTime() * 2.0) % 2 == 0) DrawRectangle(cx, cy, 2, LINE_H, t.cursor);
            } else {
                DrawRectangle(cx, cy, 2, LINE_H, t.cursor);
            }
        }
        EndScissorMode();

        /* editor mouse - modal/ctx açıkken çalışmaz */
        if (!blocked) {
            Vector2 mp = GetMousePosition();
            Rectangle inner = { editor_rect.x + GUTTER_W, editor_rect.y + 4, editor_rect.width - GUTTER_W, editor_rect.height - 8 };
            if (CheckCollisionPointRec(mp, inner)) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    int line = (int)(CUR.scroll_y + (int)((mp.y - inner.y) / LINE_H));
                    if (line >= 0 && line < (int)total_lines) {
                        size_t l = 0;
                        const char *line_p = gcl_ide_buffer_line_at(&CUR, (size_t)line, &l);
                        size_t off = (size_t)(line_p - CUR.content);
                        /* UTF-8 güvenli tıklama: karakter bazlı ilerle, çok baytlı karakterlerin
                           ortasına değil tam başına/sonuna konumlan. Bayt bazlı ilerleme
                           "cursor burada görünüyor ama yazı buraya gidiyor" hissini verir. */
                        size_t col = 0;
                        size_t boff = 0;
                        while (boff < l) {
                            unsigned char lc = (unsigned char)line_p[boff];
                            int ll = (lc < 0x80) ? 1 : (((lc & 0xE0) == 0xC0) ? 2 :
                                     (((lc & 0xF0) == 0xE0) ? 3 : 4));
                            if (boff + (size_t)ll > l) ll = (int)(l - boff);
                            char tmp[5];
                            memcpy(tmp, line_p + boff, (size_t)ll); tmp[ll] = '\0';
                            if ((int)(inner.x - sx + MeasureText(tmp, efont)) <= (int)mp.x) {
                                col = boff + (size_t)ll;
                                boff += (size_t)ll;
                            } else {
                                break;
                            }
                        }
                        CUR.cursor = off + col;
                        if (!shift) ed.sel_anchor = CUR.cursor;
                    }
                }
            }
        }

        /* welcome screen - proje açık değilken; modal açıkken tıklamalar yutulur */
        if (ed.tab_count == 0 && !blocked) {
            Rectangle wr = { editor_rect.x + GUTTER_W, editor_rect.y + 4, editor_rect.width - GUTTER_W, editor_rect.height - 8 };
            int wcx = (int)(wr.x + wr.width / 2);
            int wcy = (int)(wr.y + wr.height / 2);
            const char *wtitle = "GCL IDE";
            const char *wsub = "Open a project or create a new one";
            DrawText(wtitle, wcx - MeasureText(wtitle, 28) / 2, wcy - 70, 28, t.accent);
            DrawText(wsub, wcx - MeasureText(wsub, font_sz) / 2, wcy - 30, font_sz, t.text);
            Rectangle npb = { (float)(wcx - 200), (float)(wcy + 10), 180, 36 };
            Rectangle opb = { (float)(wcx + 20), (float)(wcy + 10), 180, 36 };
            if (ui_button(npb, "New Project", &t, font_sz)) {
                ide_new_project_open(&ed);
            }
            if (ui_button(opb, "Open Project", &t, font_sz)) {
                menu_action_run(&ed, MACT_OPEN_PROJECT);
            }
        }

        /* completion penceresi */
        if (ed.completion_visible && (ed.completion_count > 0 || ed.completion_no_match)) {
            int px = (int)editor_rect.x + GUTTER_W;
            int py = (int)editor_rect.y + 4 + (int)(gcl_ide_buffer_line_of_cursor(&CUR) - CUR.scroll_y) * LINE_H + LINE_H;
            int pw = 280, ph = 28;
            if (ed.completion_no_match) {
                pw = MeasureText(ed.completion_message, efont) + 24;
                if (pw < 240) pw = 240;
            } else {
                int max_w = 0;
                for (int i = 0; i < ed.completion_count; i++) {
                    const char *lb = ed.completions[i].name ? ed.completions[i].name : "";
                    int lw = MeasureText(lb, efont) + 48;
                    if (lw > max_w) max_w = lw;
                }
                pw = max_w > 280 ? max_w : 280;
                ph = ed.completion_count * 20 + 4;
                if (ph > 200) ph = 200;
            }
            /* Pencere imlecin HEMEN ALTINDA açılır; editor alanının içinde kalır.
               Uzun listenin ekran altına taşması durumunda yukarı kaydırılır —
               AMA imleç satırının üzerine binmemesi için editor_rect.y ofseti hesaba katılır. */
            /* X: editor içinde kal */
            if (px + pw > (int)(editor_rect.x + editor_rect.width)) px = (int)(editor_rect.x + editor_rect.width) - pw;
            if (px < (int)editor_rect.x) px = (int)editor_rect.x;
            /* Pencere önce imlecin ALTINDA açılır; alta sığmazsa imlecin ÜSTÜNE geç —
               yazılan satırın üzerine asla binmez. */
            if (py + ph > (int)(editor_rect.y + editor_rect.height)) {
                py = (int)editor_rect.y + 4 + (int)(gcl_ide_buffer_line_of_cursor(&CUR) - CUR.scroll_y) * LINE_H - ph - 4;
                if (py < (int)editor_rect.y) py = (int)editor_rect.y;
            }
            DrawRectangle(px, py, pw, ph, t.popup_bg);
            DrawRectangleLines((float)px, (float)py, (float)pw, (float)ph, t.accent);
            if (ed.completion_no_match) {
                DrawText(ed.completion_message, px + 8, py + 4, efont, t.error);
            } else {
                int start = 0;
                if (ed.completion_selected >= 10) start = ed.completion_selected - 9;
                for (int i = start; i < ed.completion_count && i < start + 10; i++) {
                    int y = py + 2 + (i - start) * 20;
                    bool sel = (i == ed.completion_selected);
                    if (sel) DrawRectangle(px + 1, y, pw - 2, 20, t.selection);
                    const char *lb = ed.completions[i].name ? ed.completions[i].name : "";
                    const char *dt = ed.completions[i].detail ? ed.completions[i].detail : "";
                    DrawText(lb, px + 6, y + 2, efont - 1, sel ? WHITE : t.text);
                    if (dt[0]) DrawText(dt, px + pw - MeasureText(dt, efont - 2) - 8, y + 4, efont - 2, t.gutter);
                }
            }
            /* signature/parametre penceresi — seçili elemanın imzası listenin ALTINDA (VSCode benzeri) */
            if (ed.completion_selected >= 0 && ed.completion_selected < ed.completion_count) {
                int sig_h = 0;
                draw_completion_signature(&ed.completions[ed.completion_selected],
                                          px, py + ph, pw, efont, &t, &sig_h);
                ph += sig_h;
            }
            /* Ekran dışına taşarsa popup'ı yukarı kaydır (VSCode davranışı) */
            if (py + ph > (int)editor_rect.y + (int)editor_rect.height - 4) {
                py = (int)editor_rect.y + 4 + (int)(cur_line - CUR.scroll_y) * LINE_H - ph;
                if (py < (int)editor_rect.y + 4) py = (int)editor_rect.y + 4;
            }
        }

        /* name dialog (New File / New Directory / Rename) — modal çizimi.
           DİKKAT: input handling var (satır 299-302) ama çizim kodu YOKTU.
           blocked=true olduğu için tab/editor da çizilmiyordu → ekran boşalıyordu. */
        if (ed.name_dialog_open) {
            /* karartma */
            DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 200 });
            int dw = 360, dh = 140;
            int dx = (w - dw) / 2, dy = (h - dh) / 2;
            if (dx < 0) dx = 0;
            if (dy < 0) dy = 0;
            DrawRectangleRec((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh }, t.popup_bg);
            DrawRectangleLinesEx((Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh }, 2.0f, t.accent);
            const char *ndt = (ed.name_dialog_mode == 2) ? "Rename" :
                              (ed.name_dialog_mode == 1 ? "New Directory" : "New File");
            DrawText(ndt, dx + 16, dy + 12, font_sz, t.accent);
            Rectangle nd_tb = { (float)(dx + 16), (float)(dy + 44), (float)(dw - 32), 30 };
            ui_label_textbox(nd_tb, ed.name_dialog_input, sizeof(ed.name_dialog_input),
                             ed.active_textbox == 1, &ed.text_caret_name, &ed.text_sel_name, &t, font_sz);
            Rectangle nd_ok = { (float)(dx + dw - 160), (float)(dy + dh - 48), 66, 30 };
            Rectangle nd_cc = { (float)(dx + dw - 86), (float)(dy + dh - 48), 66, 30 };
            if (ui_button(nd_ok, "OK", &t, font_sz)) {
                run_name_dialog(&ed);
            }
            if (ui_button(nd_cc, "Cancel", &t, font_sz)) {
                ed.name_dialog_open = 0;
                ed.ctx_open = 0;
                ed.active_textbox = -1;
            }
        }

        /* settings / about modalları — `blocked=true` olduğu için editor çizilmez;
           bu paneller olmadan ekran siyah kalır. */
        if (ed.project_dialog_open) ide_new_project_draw(&ed, w, h, font_sz, &t);
        if (ed.settings_open) gcl_settings_panel_draw(&ed, w, h, font_sz, &t);
        if (ed.about) draw_about_panel(&ed, w, h, font_sz, &t);

        /* output panel — modal (New Project vb.) açıkken gizle: modal her zaman üstte kalmalı.
           Aksi halde output paneli modalın üstüne çizilip altta kalmasına yol açar. */
        if (ed.output_visible && !blocked) {
            DrawRectangleRec(output_rect, t.popup_bg);
            DrawRectangleLinesEx(output_rect, 1.0f, t.accent);
            DrawText("Output", (int)(output_rect.x + 8), (int)(output_rect.y + 4), font_sz - 2, t.accent);
            BeginScissorMode((int)output_rect.x, (int)output_rect.y + 22, (int)output_rect.width, (int)output_rect.height - 22);
            int oy = (int)output_rect.y + 26;
            int line_h = efont + 2;
            int out_vis = (int)((output_rect.height - 28) / line_h);
            if (out_vis < 1) out_vis = 1;
            int total_out = 0;
            for (char *s = ed.output; *s; ) {
                char *nl = strchr(s, '\n');
                if (!nl) { total_out++; break; }
                total_out++;
                s = nl + 1;
            }
            int max_out = total_out > out_vis ? total_out - out_vis : 0;
            if (ed.output_scroll < 0) ed.output_scroll = 0;
            if (ed.output_scroll > max_out) ed.output_scroll = max_out;
            int start_line = max_out - ed.output_scroll;
            char *line_start = ed.output;
            int skip = start_line;
            while (skip > 0 && *line_start) {
                char *nl = strchr(line_start, '\n');
                if (!nl) break;
                line_start = nl + 1;
                skip--;
            }
            while (*line_start && oy < (int)(output_rect.y + output_rect.height - 6)) {
                char *nl = strchr(line_start, '\n');
                size_t llen = nl ? (size_t)(nl - line_start) : strlen(line_start);
                char linebuf[4096];
                if (llen > sizeof(linebuf) - 1) llen = sizeof(linebuf) - 1;
                memcpy(linebuf, line_start, llen); linebuf[llen] = '\0';
                DrawText(linebuf, (int)(output_rect.x + 8), oy, efont, t.text);
                oy += line_h;
                if (!nl) break;
                line_start = nl + 1;
            }
            EndScissorMode();
            if (max_out > 0) {
                float bar_h = (output_rect.height - 22) * ((float)out_vis / (float)total_out);
                float bar_y = (output_rect.y + 22) + ((output_rect.height - 22) - bar_h) * ((float)(max_out - ed.output_scroll) / (float)max_out);
                DrawRectangle((int)(output_rect.x + output_rect.width - 8), (int)bar_y, 6, (int)bar_h, t.gutter);
            }
        }

        /* status */
        DrawRectangleRec(status_rect, t.status_bg);
        char status[2300];
        if (ed.running) {
            const char *step_msg = ed.status_msg[0] ? ed.status_msg : "Building...";
            DrawText(step_msg, (int)(status_rect.x + 8), (int)(status_rect.y + 6), font_sz - 2, t.accent);
        } else {
            snprintf(status, sizeof(status), "GCL IDE | %s", ed.tab_count > 0 && ed.active_tab >= 0 ? (ed.tabs[ed.active_tab].path ? path_basename(ed.tabs[ed.active_tab].path) : "untitled") : "No file open");
            DrawText(status, (int)(status_rect.x + 8), (int)(status_rect.y + 6), font_sz - 2, t.text);
        }

        /* menü popup en üstte */
        if (ed.menu_open) {
            int mi = ed.menu_open - 1;
            int n = menu_item_count(mi);
            float menux = 8;
            for (int i = 0; i < mi; i++) menux += MeasureText(g_menu_titles[i], font_sz) + 20;
            int pw = 160;
            for (int i = 0; i < n; i++) {
                int tw = MeasureText(g_menu_items[mi][i], font_sz) + 16;
                if (tw > pw) pw = tw;
            }
            Rectangle pr = { menux, (float)MENU_H, (float)pw, (float)(n * 26 + 4) };
            DrawRectangleRec(pr, t.popup_bg);
            DrawRectangleLinesEx(pr, 1, t.accent);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mp = GetMousePosition();
                if (mp.y > MENU_H && !CheckCollisionPointRec(mp, pr)) ed.menu_open = 0;
            }
            for (int i = 0; i < n; i++) {
                int y = (int)pr.y + 2 + i * 26;
                Rectangle it = { pr.x + 2, (float)y, pr.width - 4, 26 };
                bool hover = CheckCollisionPointRec(GetMousePosition(), it);
                if (hover) DrawRectangleRec(it, t.selection);
                DrawText(g_menu_items[mi][i], (int)(it.x + 8), (int)(it.y + 4), font_sz, hover ? WHITE : t.text);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    MenuAction a = g_menu_actions[mi][i];
                    ed.menu_open = 0;
                    menu_action_run(&ed, a);
                }
            }
        }

        /* shake'i kapat */
        if (ed.screen_shake && ed.shake_remaining > 0.0f) {
            rlPopMatrix();
        }

        /* particles — pixel-art kare parçacıklar */
        {
            float pdt = GetFrameTime();
            editor_update_particles(&ed, pdt);
            for (int i = 0; i < ed.part_count; i++) {
                Particle *p = &ed.parts[i];
                if (p->life > 0) {
                    DrawRectangle((int)(p->x - p->size / 2.0f),
                                  (int)(p->y - p->size / 2.0f),
                                  (int)p->size, (int)p->size, p->color);
                }
            }
        }

        /* efekt sönümlemesi (ridiculous_coding): shake ve pitch düşüşü */
        {
            float pdt = GetFrameTime();
            if (ed.shake_remaining > 0.0f) {
                ed.shake_remaining -= pdt;
                if (ed.shake_remaining < 0.0f) ed.shake_remaining = 0.0f;
            }
            if (ed.pitch_increase > 0.0f) {
                ed.pitch_increase -= 2.0f * pdt;
                if (ed.pitch_increase < 0.0f) ed.pitch_increase = 0.0f;
            }
        }

        /* VHS bozukluk — font okunabilirliğini koruyan daha yumuşak versiyon */
        if (ed.vhs) {
            int base_a = 2 + (ed.vhs_strength * 18 / 100);
            int band_a = 4 + (ed.vhs_strength * 40 / 100);
            for (int i = 0; i < h / 6; i++) {
                int yy = i * 6;
                Color c = { 255, 255, 255, base_a };
                DrawRectangle(0, yy, w, 1, c);
            }
            int band_y = (int)(GetTime() * 60) % (h + 80) - 40;
            DrawRectangle(0, band_y, w, 8, (Color){ 255, 255, 255, band_a });
        }

        /* CRT — daha hafif scanline + köşe karartması (okunabilirlik öncelikli) */
        if (ed.crt) {
            int ca = 8 + (ed.crt_strength * 35 / 100);
            for (int yy = 2; yy < h; yy += 4) {
                DrawRectangle(0, yy, w, 1, (Color){ 0, 0, 0, ca });
            }
            int edge = 3 + (ed.crt_strength * 16 / 100);
            for (int i = 0; i < edge; i++) {
                int a = (int)(20 * (1.0f - (float)i / (float)edge));
                DrawRectangle(0, i, w, 1, (Color){ 0, 0, 0, a });
                DrawRectangle(0, h - 1 - i, w, 1, (Color){ 0, 0, 0, a });
                DrawRectangle(i, 0, 1, h, (Color){ 0, 0, 0, a });
                DrawRectangle(w - 1 - i, 0, 1, h, (Color){ 0, 0, 0, a });
            }
        }

        EndDrawing();
    }

    /* IDE kapanırken hâlâ çalışan dış süreci (Run/Project run) sonlandır */
    gcl_ide_proc_kill(&ed);

    free(ed.tabs);
    free(ed.clip_text);
    CloseWindow();
    return 0;
}
