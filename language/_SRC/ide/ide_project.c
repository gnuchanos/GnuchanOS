#include "gcl_ide_internal.h"
#include <sys/stat.h>

#ifdef _WIN32
/* windows.h'in CloseWindow/ShowCursor fonksiyonları raylib.h ile çakışır.
   Aynı gizleme tekniği ide_proc.c'de kullanılır. */
#ifndef NOGDI
#define NOGDI
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define CloseWindow CloseWindow_
#define ShowCursor ShowCursor_
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef Rectangle
#include <process.h> /* _beginthreadex */
#endif

/* Güvenli sabit-boyut string kopyalama - truncation uyarılarını giderir. */
static void str_copy_fixed(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    size_t n = src ? strlen(src) : 0;
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ---------------------------------------------
   Project helpers
   --------------------------------------------- */

/* Build thread — IDE build'i KENDİ İÇİNDE yapar (GUI kilitlenmesin).
   gcb_build_project_runtime + exe kopyalama thread'de çalışır; main döngü
   editor_build_poll ile build_output'u akıtır. */
#ifdef _WIN32
static unsigned __stdcall build_thread_fn(void *arg) {
#else
static void *build_thread_fn(void *arg) {
#endif
    Editor *ed = (Editor *)arg;
    char step[512];

    snprintf(step, sizeof(step), "[Build] 1/5 Proje + runtime paketleniyor...\n");
    strncpy(ed->build_step, step, sizeof(ed->build_step) - 1);
    ed->build_step[sizeof(ed->build_step) - 1] = '\0';

    int rc = gcb_build_project_runtime(ed->build_base_dir, ed->build_runtime_dir,
                                       ed->build_name, ed->build_out_dir);
    if (rc != 0) {
        const char *err = gcb_last_error();
        snprintf(ed->build_step, sizeof(ed->build_step), "[Build] Error: could not build project (%s)\n",
                 err && err[0] ? err : "unknown");
        ed->build_fail = 1;
        ed->build_done = 1;
        return 0;
    }

    snprintf(step, sizeof(step), "[Build] 2/5 Çalıştırılabilir kopyalanıyor...\n");
    strncpy(ed->build_step, step, sizeof(ed->build_step) - 1);
    ed->build_step[sizeof(ed->build_step) - 1] = '\0';

    const char *src_exe = getenv("GCL_EXE_PATH");
    if (src_exe && src_exe[0]) {
#ifdef _WIN32
        char dest[4096];
        snprintf(dest, sizeof(dest), "%s\\%s.exe", ed->build_out_dir, ed->build_name);
        if (CopyFileA(src_exe, dest, FALSE)) {
            char ok[512];
            snprintf(ok, sizeof(ok), "Built: %s\n", dest);
            strncpy(ed->build_step, ok, sizeof(ed->build_step) - 1);
            ed->build_step[sizeof(ed->build_step) - 1] = '\0';
        }
#else
        char dest[4096];
        snprintf(dest, sizeof(dest), "%s/%s", ed->build_out_dir, ed->build_name);
        char cp_cmd[8192];
        snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", src_exe, dest);
        if (system(cp_cmd) == 0) {
            char ok[512];
            snprintf(ok, sizeof(ok), "Built: %s\n", dest);
            strncpy(ed->build_step, ok, sizeof(ed->build_step) - 1);
            ed->build_step[sizeof(ed->build_step) - 1] = '\0';
        }
#endif
    }

    snprintf(step, sizeof(step), "[Build] 3/5 Tamamlandı.\n");
    strncpy(ed->build_step, step, sizeof(ed->build_step) - 1);
    ed->build_step[sizeof(ed->build_step) - 1] = '\0';
    ed->build_done = 1;
    ed->build_fail = 0;
    return 0;
}

void editor_build_poll(Editor *ed) {
    if (!ed->build_running) return;
    if (ed->build_step[0]) {
        snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", ed->build_step);
        ed->status_msg_time = GetTime();
        if (ed->output_len + strlen(ed->build_step) < sizeof(ed->output) - 1) {
            memcpy(ed->output + ed->output_len, ed->build_step, strlen(ed->build_step));
            ed->output_len += strlen(ed->build_step);
            ed->output[ed->output_len] = '\0';
        }
    }
    if (ed->build_done) {
        ed->build_running = 0;
        ed->build_done = 0;
        char done[512];
        if (ed->build_fail) {
            snprintf(done, sizeof(done), "[Build] Error: could not build project\n");
        } else {
            snprintf(done, sizeof(done), "[Build] Done. Built: %s.gcBundle -> %s\n",
                     ed->build_name, ed->build_out_dir);
        }
        snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", done);
        ed->status_msg_time = GetTime();
        if (ed->output_len + strlen(done) < sizeof(ed->output) - 1) {
            memcpy(ed->output + ed->output_len, done, strlen(done));
            ed->output_len += strlen(done);
            ed->output[ed->output_len] = '\0';
        }
        ed->build_fail = 0;
    }
}

/* Default proje klasör/dosyası eksik mi? (IDE run/build öncesi uyarı) */
static int defaults_missing(const char *base, char *missing_out, size_t outsz) {
    const char *req[] = { "scripts", "assets", "include", "external", "main.gcsf" };
    for (int i = 0; i < 5; i++) {
        char p[4096];
        snprintf(p, sizeof(p), "%s/%s", base, req[i]);
        struct stat st;
        if (stat(p, &st) != 0) {
            if (missing_out && outsz) snprintf(missing_out, outsz, "%s", req[i]);
            return 1;
        }
    }
    return 0;
}

void project_find_gcdata(Editor *ed, char *out, size_t outsz) {
    /* current_project > cwd içinde project.gcdata ara */
    const char *bases[2];
    bases[0] = ed->current_project;
    bases[1] = ed->cwd;
    for (int i = 0; i < 2; i++) {
        if (!bases[i] || !bases[i][0]) continue;
        if (fs_is_dir(bases[i])) {
            snprintf(out, outsz, "%s/project.gcdata", bases[i]);
            if (fopen(out, "rb")) { fclose(fopen(out, "rb")); return; }
        }
    }
    out[0] = '\0';
}

void editor_run_project(Editor *ed) {
    char gcdata[4096];
    project_find_gcdata(ed, gcdata, sizeof(gcdata));
    if (!gcdata[0]) {
        snprintf(ed->output, sizeof(ed->output), "Error: no project.gcdata found (open/create a project first)\n");
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
        return;
    }
    char base_dir[4096];
    fs_parent_dir(gcdata, base_dir, sizeof(base_dir));
    /* Doğru çalıştırılacak dosya main.gcsf'tir (gcdata değil!) */
    char main_gcsf[8192];
    snprintf(main_gcsf, sizeof(main_gcsf), "%s/main.gcsf", base_dir);
    struct stat st;
    if (stat(main_gcsf, &st) != 0) {
        snprintf(ed->output, sizeof(ed->output), "Error: main.gcsf not found (open/create a project first)\n");
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
        return;
    }
    /* Default klasörler eksikse uyar */
    char miss[256] = "";
    if (defaults_missing(base_dir, miss, sizeof(miss))) {
        snprintf(ed->output, sizeof(ed->output),
                 "Warning: default project item '%s' is missing.\n"
                 "Restore it or set new folder paths in project.gcdata.\n", miss);
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
    }
    /* .gclog dosyasına da yaz — asenkron process çıktısı buraya akar.
       gcl_ide_proc_start logf'u process'e bağlar; poll her frame'de hem
       ed->output'a hem logf'a yazar. Böylece project.gclog boş kalmaz. */
    char log_path[8192];
    snprintf(log_path, sizeof(log_path), "%s/project.gclog", base_dir);
    FILE *logf = fopen(log_path, "wb");
    if (logf) fputs("=== gcl -run ===\n", logf);

    /* Project Run: standalone -run'dan farklı olarak GCL_PROJECT_DIR'i
       PROJE KÖKÜNE zorla. Aksi halde Embed.Run alt süreçleri script yolunu
       workspace'e göre çözer (örn. D:\GnuchanOS/scripts/main.lua — yanlış).
       Bu env çocuk sürece miras kalır; run_gcl dışarıdan set edilmiş değeri korur. */
#ifdef _WIN32
    _putenv_s("GCL_PROJECT_DIR", base_dir);
    {
        char shared_file[8192];
        snprintf(shared_file, sizeof(shared_file), "%s\\gcl_shared.state", base_dir);
        _putenv_s("GCL_SHARED_FILE", shared_file);
    }
#else
    setenv("GCL_PROJECT_DIR", base_dir, 1);
    {
        char shared_file[8192];
        snprintf(shared_file, sizeof(shared_file), "%s/gcl_shared.state", base_dir);
        setenv("GCL_SHARED_FILE", shared_file, 1);
    }
#endif
    const char *self = getenv("GCL_EXE_PATH");
    if (!self || !self[0]) self = "gcl";
    char cmd[20480];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "cmd /c \"\"%s\" -run \"%s\"\" 2>&1", self, main_gcsf);
#else
    snprintf(cmd, sizeof(cmd), "\"%s\" -run \"%s\" 2>&1", self, main_gcsf);
#endif
    /* Senkron popen yerine asenkron süreç: IDE kilitlenmez, IDE kapanınca süreç sonlanır.
       logf, process'in log handle'ı olur (poll yazar, kill kapatır). */
    gcl_ide_proc_start(ed, cmd, main_gcsf, logf);
}

void run_project_dialog(Editor *ed) {
    if (!ed->project_name_input[0]) {
        return;
    }
    char base[2048];
    if (ed->new_project_path[0]) snprintf(base, sizeof(base), "%s", ed->new_project_path);
    else if (ed->current_project[0]) snprintf(base, sizeof(base), "%s", ed->current_project);
    else snprintf(base, sizeof(base), "%s", ed->cwd);
    /* Base yolun sonundaki ayraçları temizle — sürücü kökü gibi path seçicilerden
       gelen "D:\" artık "D:\_2" yerine "D:/_2" gibi çift ayraçlı kirli birleşimlere
       yol açmasın. "D:\" gibi kök dizinlerde tek ayraç korunur (normalize edici
       aşağıda). */
    {
        size_t blen = strlen(base);
        while (blen > 0 && (base[blen - 1] == '/' || base[blen - 1] == '\\')) {
            /* "D:\" gibi sürücü kökünde tek ayraç bırak */
            if (blen == 3 && base[0] && base[1] == ':' && (base[2] == '\\' || base[2] == '/')) break;
            base[--blen] = '\0';
        }
    }
    char full[8192];
    snprintf(full, sizeof(full), "%s/%s", base, ed->project_name_input);
    if (fs_is_dir(full)) {
        snprintf(ed->status_msg, sizeof(ed->status_msg), "Error: project '%s' already exists", ed->project_name_input);
        ed->status_msg_time = GetTime();
        return;
    }
    gcl_ensure_dir(full);
    /* IDE KENDİ İÇİNDE proje iskeleti oluşturur — gcl -new ÇAĞIRILMAZ.
       Dizinler + main.gcsf + project.gcdata + scripts/main.{lua,py} üretilir. */
    {
        const char *dirs[] = { "scripts", "assets", "include", "external", "out", NULL };
        for (int i = 0; dirs[i]; i++) {
            char dp[8192];
            snprintf(dp, sizeof(dp), "%s/%s", full, dirs[i]);
            gcl_ensure_dir(dp);
        }
        char main_path[8192];
        snprintf(main_path, sizeof(main_path), "%s/main.gcsf", full);
        FILE *mf = fopen(main_path, "wb");
        if (mf) {
            int gcl_scene = ed->new_project_gcl_scene;
            int gcl_raylib = ed->new_project_gcl_raylib;
            int lua_on = ed->new_project_open_lua;
            int py_on = ed->new_project_open_python;
            int lua_scene = ed->new_project_lua_scene;
            int py_scene = ed->new_project_python_scene;
            int want_embed = (lua_on || py_on);

            fputs("#native <Stdio>\n", mf);
            if (want_embed) fputs("#native <Embed>\n", mf);
            if (gcl_raylib) fputs("#native <Raylib>\n", mf);

            if (gcl_scene == 0 && !gcl_raylib && !want_embed) {
                /* Pure Empty — sadece hello world */
                fputs("\nint main() {\n", mf);
                fputs("    Stdio.printf(\"Hello, GCL!\\n\");\n", mf);
                if (lua_on) fputs("    Embed.Run(type=\"lua\");\n", mf);
                if (py_on)   fputs("    Embed.Run(type=\"python\");\n", mf);
                fputs("    return 0;\n", mf);
                fputs("}\n", mf);
                fclose(mf);
            } else {
                /* GCL typedef struct kamera tanımları — raylib Camera3D/Camera2D
                   karşılığı. Vector3/Vector2 nested struct'ları ile (kullanıcının
                   istediği raylib imzası): camera.position.x gibi erişim çalışır. */
                if (gcl_raylib && gcl_scene == 2) {
                    fputs("typedef struct {\n", mf);
                    fputs("    float x;\n", mf);
                    fputs("    float y;\n", mf);
                    fputs("    float z;\n", mf);
                    fputs("} Vector3;\n", mf);
                    fputs("typedef struct {\n", mf);
                    fputs("    Vector3 position;\n", mf);
                    fputs("    Vector3 target;\n", mf);
                    fputs("    Vector3 up;\n", mf);
                    fputs("    float fovy;\n", mf);
                    fputs("    int projection;\n", mf);
                    fputs("} Camera3D;\n", mf);
                } else if (gcl_raylib && gcl_scene == 1) {
                    fputs("typedef struct {\n", mf);
                    fputs("    float x;\n", mf);
                    fputs("    float y;\n", mf);
                    fputs("} Vector2;\n", mf);
                    fputs("typedef struct {\n", mf);
                    fputs("    Vector2 offset;\n", mf);
                    fputs("    Vector2 target;\n", mf);
                    fputs("    float rotation;\n", mf);
                    fputs("    float zoom;\n", mf);
                    fputs("} Camera2D;\n", mf);
                }
                fputs("\nint main() {\n", mf);
                fputs("    Stdio.printf(\"Hello, GCL!\\n\");\n", mf);
                if (lua_on) fputs("    Embed.Run(type=\"lua\");\n", mf);
                if (py_on)   fputs("    Embed.Run(type=\"python\");\n", mf);

                if (gcl_raylib) {
                    if (gcl_scene == 2) {
                        /* GCL 3D — C-örneğine yakın: Raylib.*, compound literal,
                           member-atama, &camera, CAMERA_FREE/CAMERA_PERSPECTIVE. */
                        fputs("    Raylib.InitWindow(800, 600, \"GCL 3D Project\");\n", mf);
                        fputs("    Raylib.SetTargetFPS(60);\n", mf);
                        fputs("    const int screenWidth = 800;\n", mf);
                        fputs("    const int screenHeight = 600;\n", mf);
                        fputs("    Camera3D camera = { 0 };\n", mf);
                        fputs("    camera.position = (Vector3){ 10.0, 10.0, 10.0 };\n", mf);
                        fputs("    camera.target = (Vector3){ 0.0, 0.0, 0.0 };\n", mf);
                        fputs("    camera.up = (Vector3){ 0.0, 1.0, 0.0 };\n", mf);
                        fputs("    camera.fovy = 45.0;\n", mf);
                        fputs("    camera.projection = Raylib.CAMERA_PERSPECTIVE;\n", mf);
                        fputs("    Vector3 cubePos = { 0.0, 0.0, 0.0 };\n", mf);
                        fputs("    while (!Raylib.WindowShouldClose()) {\n", mf);
                        fputs("        Raylib.UpdateCamera(&camera, Raylib.CAMERA_FREE);\n", mf);
                        fputs("        if (Raylib.IsKeyPressed(Raylib.KEY_Z)) {\n", mf);
                        fputs("            camera.target = (Vector3){ 0.0, 0.0, 0.0 };\n", mf);
                        fputs("        }\n", mf);
                        fputs("        Raylib.BeginDrawing();\n", mf);
                        fputs("        Raylib.ClearBackground(Raylib.RAYWHITE);\n", mf);
                        fputs("        Raylib.BeginMode3D(camera);\n", mf);
                        fputs("        Raylib.DrawCube(cubePos, 2.0, 2.0, 2.0, Raylib.RED);\n", mf);
                        fputs("        Raylib.DrawCubeWires(cubePos, 2.0, 2.0, 2.0, Raylib.MAROON);\n", mf);
                        fputs("        Raylib.DrawGrid(10, 1.0);\n", mf);
                        fputs("        Raylib.EndMode3D();\n", mf);
                        fputs("        Raylib.EndDrawing();\n", mf);
                        fputs("    }\n", mf);
                        fputs("    Raylib.CloseWindow();\n", mf);
                    } else if (gcl_scene == 1) {
                        /* GCL 2D — GERÇEK GCL typedef struct Camera2D (Vector2 nested).
                           camera.offset.x/y, target.x/y, rotation, zoom alan alan atanır
                           (GCL 3D ile aynı stil). Raylib.BeginMode2D(camera) struct
                           üyelerini genişletip raylib Camera2D'ye aktarır. */
                        fputs("    Raylib.InitWindow(800, 600, \"GCL 2D Project\");\n", mf);
                        fputs("    Raylib.SetTargetFPS(60);\n", mf);
                        fputs("    Camera2D camera;\n", mf);
                        fputs("    camera.offset = (Vector2){ 400.0, 300.0 };\n", mf);
                        fputs("    camera.target = (Vector2){ 0.0, 0.0 };\n", mf);
                        fputs("    camera.rotation = 0.0;\n", mf);
                        fputs("    camera.zoom = 1.0;\n", mf);
                        fputs("    while (!Raylib.WindowShouldClose()) {\n", mf);
                        fputs("        Raylib.BeginDrawing();\n", mf);
                        fputs("        Raylib.ClearBackground(Raylib.BLACK);\n", mf);
                        fputs("        Raylib.BeginMode2D(camera);\n", mf);
                        fputs("        Raylib.DrawCircle(400, 300, 50, Raylib.RED);\n", mf);
                        fputs("        Raylib.DrawText(\"GCL 2D Project\", 20, 20, 20, Raylib.WHITE);\n", mf);
                        fputs("        Raylib.EndMode2D();\n", mf);
                        fputs("        Raylib.EndDrawing();\n", mf);
                        fputs("    }\n", mf);
                        fputs("    Raylib.CloseWindow();\n", mf);
                    } else {
                        /* GCL Empty + Raylib */
                        fputs("    Raylib.InitWindow(800, 600, \"GCL Project\");\n", mf);
                        fputs("    Raylib.SetTargetFPS(60);\n", mf);
                        fputs("    while (!Raylib.WindowShouldClose()) {\n", mf);
                        fputs("        Raylib.BeginDrawing();\n", mf);
                        fputs("        Raylib.ClearBackground(Raylib.BLACK);\n", mf);
                        fputs("        Raylib.DrawText(\"GCL Project\", 20, 20, 20, Raylib.WHITE);\n", mf);
                        fputs("        Raylib.EndDrawing();\n", mf);
                        fputs("    }\n", mf);
                        fputs("    Raylib.CloseWindow();\n", mf);
                    }
                }
                fputs("    return 0;\n", mf);
                fputs("}\n", mf);
                fclose(mf);
            }
        }
        char gcdata_path[8192];
        snprintf(gcdata_path, sizeof(gcdata_path), "%s/project.gcdata", full);
        FILE *gf = fopen(gcdata_path, "wb");
        if (gf) {
            /* Lua/Python raylib: Enable açıksa Empty scene'de bile raylib
               pencere şablonu üretilir. "Empty" = boş pencere + hello,
               "2D"/"3D" = kamera içeren şablon. */
            int lua_raylib = ed->new_project_open_lua;
            int py_raylib = ed->new_project_open_python;
            fprintf(gf, "{\n");
            fprintf(gf, "  \"project_name\": \"%s\",\n", ed->project_name_input);
            fprintf(gf, "  \"developer_name\": \"developer\",\n");
            fprintf(gf, "  \"version\": 0.100,\n");
            fprintf(gf, "  \"open_lua\": %s,\n", ed->new_project_open_lua ? "true" : "false");
            fprintf(gf, "  \"open_lua_raylib\": %s,\n", lua_raylib ? "true" : "false");
            fprintf(gf, "  \"open_python\": %s,\n", ed->new_project_open_python ? "true" : "false");
            fprintf(gf, "  \"open_python_raylib\": %s,\n", py_raylib ? "true" : "false");
            fprintf(gf, "  \"single_bundle\": false,\n");
            fprintf(gf, "  \"lua_main_file\": \"main.lua\",\n");
            fprintf(gf, "  \"python_main_file\": \"main.py\",\n");
            fprintf(gf, "  \"gcl_include_path\": \"include\",\n");
            fprintf(gf, "  \"gcl_lib_path\": \"lib\",\n");
            fprintf(gf, "  \"gcl_external_path\": \"external\",\n");
            fprintf(gf, "  \"python_import_path\": \"scripts\",\n");
            fprintf(gf, "  \"lua_import_path\": \"scripts\",\n");
            fprintf(gf, "  \"raylib_asset_directory_path\": \"assets\"\n");
            fprintf(gf, "}\n");
            fclose(gf);
        }
        if (ed->new_project_open_lua) {
            char lp[8192];
            snprintf(lp, sizeof(lp), "%s/scripts/main.lua", full);
            FILE *lf = fopen(lp, "wb");
            if (lf) {
                /* Scene: 0=Empty, 1=2D, 2=3D */
                if (ed->new_project_lua_scene == 2) {
                    /* Lua 3D — gerçek Camera3D tablo nesnesi */
                    fputs(
                        "-- GCL Lua 3D example\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Lua 3D Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "local camera = raylib.Camera3D()\n"
                        "camera.position = { x = 10.0, y = 10.0, z = 10.0 }\n"
                        "camera.target = { x = 0.0, y = 0.0, z = 0.0 }\n"
                        "camera.up = { x = 0.0, y = 1.0, z = 0.0 }\n"
                        "camera.fovy = 45.0\n"
                        "camera.projection = raylib.CAMERA_PERSPECTIVE\n"
                        "\n"
                        "while not raylib.WindowShouldClose() do\n"
                        "    raylib.UpdateCamera(camera, raylib.CAMERA_FREE)\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.RAYWHITE)\n"
                        "    raylib.BeginMode3D(camera)\n"
                        "    raylib.DrawCube(raylib.Vector3(0.0, 0.0, 0.0), 2.0, 2.0, 2.0, raylib.RED)\n"
                        "    raylib.DrawCubeWires(raylib.Vector3(0.0, 0.0, 0.0), 2.0, 2.0, 2.0, raylib.MAROON)\n"
                        "    raylib.DrawGrid(10, 1.0)\n"
                        "    raylib.EndMode3D()\n"
                        "    raylib.EndDrawing()\n"
                        "end\n"
                        "\n"
                        "raylib.CloseWindow()\n", lf);
                } else if (ed->new_project_lua_scene == 1) {
                    /* Lua 2D — gerçek Camera2D tablo nesnesi */
                    fputs(
                        "-- GCL Lua 2D example\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Lua 2D Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "local camera = raylib.Camera2D()\n"
                        "camera.offset = { x = 400.0, y = 300.0 }\n"
                        "camera.target = { x = 0.0, y = 0.0 }\n"
                        "camera.rotation = 0.0\n"
                        "camera.zoom = 1.0\n"
                        "\n"
                        "while not raylib.WindowShouldClose() do\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.BLACK)\n"
                        "    raylib.BeginMode2D(camera)\n"
                        "    raylib.DrawCircle(400, 300, 50, raylib.RED)\n"
                        "    raylib.DrawText(\"GCL Lua 2D Project\", 20, 20, 20, raylib.WHITE)\n"
                        "    raylib.EndMode2D()\n"
                        "    raylib.EndDrawing()\n"
                        "end\n"
                        "\n"
                        "raylib.CloseWindow()\n", lf);
                } else {
                    /* Lua Empty — boş pencere + hello */
                    fputs(
                        "-- GCL Lua Empty example\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Lua Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "while not raylib.WindowShouldClose() do\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.BLACK)\n"
                        "    raylib.DrawText(\"Hello from Lua embedded in GCL!\", 20, 20, 20, raylib.WHITE)\n"
                        "    raylib.EndDrawing()\n"
                        "end\n"
                        "\n"
                        "raylib.CloseWindow()\n", lf);
                }
                fclose(lf);
            }
        }
        if (ed->new_project_open_python) {
            char pp2[8192];
            snprintf(pp2, sizeof(pp2), "%s/scripts/main.py", full);
            FILE *pf = fopen(pp2, "wb");
            if (pf) {
                /* Scene: 0=Empty, 1=2D, 2=3D */
                if (ed->new_project_python_scene == 2) {
                    /* Python 3D — gerçek Camera3D dict nesnesi */
                    fputs(
                        "# GCL Python 3D example\n"
                        "import raylib\n"
                        "import gcl\n"
                        "\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Python 3D Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "camera = raylib.Camera3D()\n"
                        "camera[\"position\"] = { \"x\": 10.0, \"y\": 10.0, \"z\": 10.0 }\n"
                        "camera[\"target\"] = { \"x\": 0.0, \"y\": 0.0, \"z\": 0.0 }\n"
                        "camera[\"up\"] = { \"x\": 0.0, \"y\": 1.0, \"z\": 0.0 }\n"
                        "camera[\"fovy\"] = 45.0\n"
                        "camera[\"projection\"] = raylib.CAMERA_PERSPECTIVE\n"
                        "\n"
                        "while not raylib.WindowShouldClose():\n"
                        "    raylib.UpdateCamera(camera, raylib.CAMERA_FREE)\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.RAYWHITE)\n"
                        "    raylib.BeginMode3D(camera)\n"
                        "    raylib.DrawCube(raylib.Vector3(0.0, 0.0, 0.0), 2.0, 2.0, 2.0, raylib.RED)\n"
                        "    raylib.DrawCubeWires(raylib.Vector3(0.0, 0.0, 0.0), 2.0, 2.0, 2.0, raylib.MAROON)\n"
                        "    raylib.DrawGrid(10, 1.0)\n"
                        "    raylib.EndMode3D()\n"
                        "    raylib.EndDrawing()\n"
                        "\n"
                        "raylib.CloseWindow()\n", pf);
                } else if (ed->new_project_python_scene == 1) {
                    /* Python 2D — gerçek Camera2D dict nesnesi */
                    fputs(
                        "# GCL Python 2D example\n"
                        "import raylib\n"
                        "import gcl\n"
                        "\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Python 2D Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "camera = raylib.Camera2D()\n"
                        "camera[\"offset\"] = { \"x\": 400.0, \"y\": 300.0 }\n"
                        "camera[\"target\"] = { \"x\": 0.0, \"y\": 0.0 }\n"
                        "camera[\"rotation\"] = 0.0\n"
                        "camera[\"zoom\"] = 1.0\n"
                        "\n"
                        "while not raylib.WindowShouldClose():\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.BLACK)\n"
                        "    raylib.BeginMode2D(camera)\n"
                        "    raylib.DrawCircle(400, 300, 50, raylib.RED)\n"
                        "    raylib.DrawText(\"GCL Python 2D Project\", 20, 20, 20, raylib.WHITE)\n"
                        "    raylib.EndMode2D()\n"
                        "    raylib.EndDrawing()\n"
                        "\n"
                        "raylib.CloseWindow()\n", pf);
                } else {
                    /* Python Empty — boş pencere + hello */
                    fputs(
                        "# GCL Python Empty example\n"
                        "import raylib\n"
                        "import gcl\n"
                        "\n"
                        "gcl.init()\n"
                        "\n"
                        "raylib.InitWindow(800, 600, \"GCL Python Project\")\n"
                        "raylib.SetTargetFPS(60)\n"
                        "\n"
                        "while not raylib.WindowShouldClose():\n"
                        "    raylib.BeginDrawing()\n"
                        "    raylib.ClearBackground(raylib.BLACK)\n"
                        "    raylib.DrawText(\"Hello from Python embedded in GCL!\", 20, 20, 20, raylib.WHITE)\n"
                        "    raylib.EndDrawing()\n"
                        "\n"
                        "raylib.CloseWindow()\n", pf);
                }
                fclose(pf);
            }
        }
    }
    /* Yeni projeyi aç */
    str_copy_fixed(ed->new_project_path, sizeof(ed->new_project_path), base);
    str_copy_fixed(ed->current_project, sizeof(ed->current_project), full);
    str_copy_fixed(ed->cwd, sizeof(ed->cwd), full);
    ed->expanded_count = 0;
    tree_rescan(ed);
    ed->project_dialog_open = 0;
    ed->active_textbox = -1;
    char status_tmp[8300];
    snprintf(status_tmp, sizeof(status_tmp), "Project created: %s", full);
    str_copy_fixed(ed->status_msg, sizeof(ed->status_msg), status_tmp);
    ed->status_msg_time = GetTime();
}

void editor_build_project(Editor *ed) {
    if (ed->build_running) {
        snprintf(ed->status_msg, sizeof(ed->status_msg), "Build already in progress...");
        ed->status_msg_time = GetTime();
        return;
    }
    char gcdata[4096];
    project_find_gcdata(ed, gcdata, sizeof(gcdata));
    if (!gcdata[0]) {
        snprintf(ed->output, sizeof(ed->output), "Error: no project.gcdata found (open/create a project first)\n");
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
        return;
    }
    char base_dir[4096];
    fs_parent_dir(gcdata, base_dir, sizeof(base_dir));
    /* Default klasörler eksikse uyar */
    char miss[256] = "";
    if (defaults_missing(base_dir, miss, sizeof(miss))) {
        snprintf(ed->output, sizeof(ed->output),
                 "Warning: default project item '%s' is missing.\n"
                 "Restore it or set new folder paths in project.gcdata.\n", miss);
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
    }

    /* project.gcdata'dan proje adını al (JSON: "project_name": "X") */
    char name[256] = "project";
    FILE *gf = fopen(gcdata, "rb");
    if (gf) {
        char gbuf[4096];
        size_t gn = fread(gbuf, 1, sizeof(gbuf) - 1, gf);
        fclose(gf);
        gbuf[gn] = '\0';
        char *p = strstr(gbuf, "project_name");
        if (p) {
            p = strchr(p, ':');
            if (p) p = strchr(p, '"');
            if (p) {
                p++;
                char *e = strchr(p, '"');
                if (e) {
                    size_t n = (size_t)(e - p);
                    if (n >= sizeof(name)) n = sizeof(name) - 1;
                    memcpy(name, p, n);
                    name[n] = '\0';
                }
            }
        }
    }

    /* çıktı dizini — HER ZAMAN proje kökündeki out/ klasörü (simple_doc.md: out/) */
    char out_dir[2048];
    snprintf(out_dir, sizeof(out_dir), "%s/out", base_dir);
    {
        size_t odl = strlen(out_dir);
        while (odl > 0 && (out_dir[odl - 1] == '/' || out_dir[odl - 1] == '\\'))
            out_dir[--odl] = '\0';
    }
    gcl_ensure_dir(out_dir);

    /* runtime dizini = GCL_EXE_PATH'ın parent'ı (build/<os>/) */
    char runtime_dir[4096] = "";
    const char *gcl_exe = getenv("GCL_EXE_PATH");
    if (gcl_exe && gcl_exe[0]) {
        snprintf(runtime_dir, sizeof(runtime_dir), "%s", gcl_exe);
        char *rs = strrchr(runtime_dir, '\\');
        if (!rs) rs = strrchr(runtime_dir, '/');
        if (rs) *rs = '\0';
    }
    if (!runtime_dir[0]) snprintf(runtime_dir, sizeof(runtime_dir), ".");

    /* .gclog — build çıktısı buraya da yazılır */
    char log_path[8192];
    snprintf(log_path, sizeof(log_path), "%s/project.gclog", base_dir);
    FILE *logf = fopen(log_path, "wb");
    if (logf) fputs("=== Build started ===\n", logf);

    /* Thread için alanları doldur (build_thread_fn kullanır) */
    str_copy_fixed(ed->build_base_dir, sizeof(ed->build_base_dir), base_dir);
    str_copy_fixed(ed->build_runtime_dir, sizeof(ed->build_runtime_dir), runtime_dir);
    str_copy_fixed(ed->build_name, sizeof(ed->build_name), name);
    str_copy_fixed(ed->build_out_dir, sizeof(ed->build_out_dir), out_dir);
    ed->build_running = 1;
    ed->build_done = 0;
    ed->build_fail = 0;
    ed->build_output_len = 0;
    ed->build_output[0] = '\0';
    ed->build_step[0] = '\0';

    snprintf(ed->output, sizeof(ed->output), "=== Build started ===\n");
    ed->output_len = strlen(ed->output);
    ed->output_visible = 1;

    /* IDE KENDİ build'ini thread'de yapar — dış süreç (gcl.exe -build) YOK.
       GUI kilitlenmez; main döngü editor_build_poll ile adımları akıtır. */
#ifdef _WIN32
    _beginthreadex(NULL, 0, build_thread_fn, ed, 0, NULL);
#else
    pthread_t thr;
    pthread_create(&thr, NULL, build_thread_fn, ed);
    pthread_detach(thr);
#endif
}
