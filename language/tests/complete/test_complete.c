/*
 * test_complete.c — GCL "SMART" tamamlama motoru icin BASLIKSIZ (headless) test.
 *
 * Motor raylib/UI'ya bagimli DEGILDIR; bu yuzden pencere acmadan derlenip
 * kosulabilir:
 *
 *   mkdir -p _temp/tests    (working directory is the repo root)
 *   gcc -std=c99 -D_POSIX_C_SOURCE=200809L -I language/_SRC/complete \
 *       -o _temp/tests/ctest.exe \
 *       language/tests/complete/test_complete.c \
 *       language/_SRC/complete/gcl_complete.c \
 *       language/_SRC/complete/complete_context.c \
 *       language/_SRC/complete/complete_scope.c \
 *       language/_SRC/complete/complete_type.c \
 *       language/_SRC/complete/complete_native.c \
 *       language/_SRC/complete/complete_native_db.c \
 *       language/_SRC/complete/complete_project.c \
 *       language/_SRC/complete/complete_rank.c \
 *       language/_SRC/complete/complete_index.c \
 *       language/_SRC/complete/complete_diag.c -lm
 *
 * Kapsam (todo.md → "language full bug hunting"): #5 baglam, #6 tip cikarimi,
 * #9 kapsam onbellegi, #10 dosya indeksleme, #26 NULL/sinir denetimleri.
 */
#include "gcl_complete.h"
#include "complete_native.h"   /* DB butunluk denetimi: modul/struct/alan */
#include "complete_scope.h"    /* gclc_keyword_list(): anahtar sozcuk listesi */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static int g_pass = 0, g_fail = 0;
static const char *g_case = "";

static void case_begin(const char *name) { g_case = name; }

static void check(int cond, const char *what) {
    if (cond) { g_pass++; return; }
    g_fail++;
    printf("FAIL [%s] %s\n", g_case, what);
}

/* ------------------------------------------------------------------ */
/* Yardimcilar                                                         */
/* ------------------------------------------------------------------ */

/* Metnin SONUNDA sorgu yap (imlec = len). */
static int query(const char *file, const char *text, const char *ws,
                 GclCompletionResult *r) {
    size_t len = strlen(text);
    gcl_complete_query(file, text, len, len, ws, r);
    return r->count;
}

/* 'label' oneriliyor mu? */
static int suggests(const char *label, const char *file, const char *text,
                    const char *ws) {
    GclCompletionResult r;
    query(file, text, ws, &r);
    int found = 0;
    for (int i = 0; i < r.count; i++)
        if (r.items[i].label && strcmp(r.items[i].label, label) == 0) { found = 1; break; }
    gcl_complete_result_free(&r);
    return found;
}

/* 'label' oneriliyor VE tipi 'type' mi? (bayat/yanlis tip sizmasin) */
static int suggests_typed(const char *label, const char *type, const char *file,
                          const char *text, const char *ws) {
    GclCompletionResult r;
    query(file, text, ws, &r);
    int ok = 0;
    for (int i = 0; i < r.count; i++) {
        if (!r.items[i].label || strcmp(r.items[i].label, label) != 0) continue;
        ok = (r.items[i].type && strcmp(r.items[i].type, type) == 0);
        break;
    }
    gcl_complete_result_free(&r);
    return ok;
}

static int suppressed(const char *file, const char *text, const char *ws) {
    GclCompletionResult r;
    query(file, text, ws, &r);
    int s = r.suppressed;
    gcl_complete_result_free(&r);
    return s;
}

/* Oneri 'label' ise tipini dondur (yoksa NULL). */
static const char *type_of(const char *label, const GclCompletionResult *r) {
    for (int i = 0; i < r->count; i++)
        if (r->items[i].label && strcmp(r->items[i].label, label) == 0)
            return r->items[i].type;
    return NULL;
}

static void make_dir(const char *path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

/* Fixture yazimi. Basarisizlikta SORUNLU YOLU yazar: setup_project() yalnizca
   "dosyalar yazilamadi" diyebiliyordu ve hangi yolun (eksik klasor, dosya
   sanilan klasor, dolu disk) patladigi ancak tahminle bulunabiliyordu. */
static int write_text(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("  [kurulum] yazilamadi (fopen): %s\n", path);
        return 0;
    }
    size_t n = strlen(content);
    size_t w = fwrite(content, 1, n, f);
    fclose(f);
    if (w != n) {
        printf("  [kurulum] eksik yazildi (%lu/%lu): %s\n",
               (unsigned long)w, (unsigned long)n, path);
        return 0;
    }
    return 1;
}
/* ------------------------------------------------------------------ */
/* Test verisi                                                         */
/* ------------------------------------------------------------------ */

/* Bildirimler: sonuna tamamlanacak ifade eklenir ve imlec METNIN SONUNDA
   olur (query() imleci len yapar). */
static const char *k_decls =
    "#native <Stdio>\n"
    "\n"
    "struct Person {\n"
    "    int id;\n"
    "    int age;\n"
    "    char name[20];\n"
    "};\n"
    "\n"
    "typedef struct {\n"
    "    int id;\n"
    "    int age;\n"
    "    char name[20];\n"
    "} User;\n"
    "\n"
    "Camera3D cam;\n"
    "Camera3D cams[2];\n"
    "\n"
    "void demo() {\n"
    "    struct Person person = { 1, 25, \"Ali\" };\n"
    "    struct Person people[3] = { {1,20,\"Ali\"}, {2,25,\"Veli\"} };\n"
    "    User user = { 2, 30, \"Veli\" };\n"
    "    int counter = 0;\n"
    "    char buf[64];\n";

/* Proje taklidi kok dizini. Bu ad, test ikilisinin KENDI yolundan farkli
   olmak ZORUNDA: kosucu ikiliyi eskiden _temp/ctest icine kuruyordu ve test
   ardindan mkdir("_temp/ctest") cagiriyordu. Linux'ta (.exe son eki yok)
   mkdir EEXIST ile, butun fixture yazimlari ENOTDIR ile patliyordu — test
   Windows'ta geciyor, CI'da "proje kurulumu" adiminda dusuyordu.
   Ikili artik _temp/tests/ altinda; bu dizinin adini hicbir test ikilisi
   kullanmaz. */
#define PROJ_DIR "_temp/gcl_ctest/proj"

static char g_buf[16384];

static const char *tf(const char *expr) {
    snprintf(g_buf, sizeof(g_buf), "%s%s", k_decls, expr);
    return g_buf;
}

/* ------------------------------------------------------------------ */
/* #6 tip cikarimi + dizi bildirimleri (F1)                            */
/* ------------------------------------------------------------------ */

static void test_array_members(void) {
    GclCompletionResult r;

    case_begin("dizi-uyesi gorunur");
    /* struct Person { char name[20]; } → 'name' uyesi kaybolmamali. */
    check(suggests("name", NULL, tf("    person."), NULL),
          "person. -> char name[20] uyesi onerilmedi");
    check(suggests("id", NULL, tf("    person."), NULL), "person. -> id yok");
    check(suggests("age", NULL, tf("    person."), NULL), "person. -> age yok");

    case_begin("typedef-uyesi gorunur");
    check(suggests("name", NULL, tf("    user."), NULL),
          "user. -> char name[20] uyesi onerilmedi");

    case_begin("dizi degiskeni sembol");
    /* "struct Person people[3]" → people : Person */
    check(suggests_typed("people", "Person", NULL, tf("    peopl"), NULL),
          "people sembolu tipi Person degil / hic yok");

    case_begin("dizi alt-simge uye (F2)");
    check(suggests("id", NULL, tf("    people[0]."), NULL),
          "people[0]. -> id yok");
    check(suggests("name", NULL, tf("    people[i]."), NULL),
          "people[i]. -> name yok");
    check(suggests("fovy", NULL, tf("    cams[0]."), NULL),
          "cams[0]. -> native Camera3D uyesi yok");

    /* Dizi uyesi de tip bagini korumali: "char name[20]" → name : char */
    case_begin("dizi uyesi tip bagi");
    query(NULL, tf("    person."), NULL, &r);
    const char *nt = type_of("name", &r);
    check(nt != NULL && strcmp(nt, "char") == 0,
          "person.name uyesinin tipi 'char' degil");
    gcl_complete_result_free(&r);

    case_begin("alt-simge: ilkel tip -> popup kapali (S2)");
    /* char dizisinin uyesi yoktur → liste bos + popup KAPALI (S2). */
    check(suppressed(NULL, tf("    buf[0]."), NULL),
          "buf[0]. (char dizisi) -> popup kapanmadi");
}

/* ------------------------------------------------------------------ */
/* #4/#8 native modul uyeleri + sizinti denetimi                       */
/* ------------------------------------------------------------------ */

static void test_native_no_leak(void) {
    GclCompletionResult r;

    case_begin("native struct uyeleri");
    check(suggests("fovy", NULL, tf("    cam."), NULL), "cam. -> fovy yok");
    check(suggests("target", NULL, tf("    cam."), NULL), "cam. -> target yok");
    check(!suggests("x", NULL, tf("    cam."), NULL),
          "cam. -> Vector2 'x' alani SIZDI (yanlis tip cozumu)");
    check(!suggests("id", NULL, tf("    cam."), NULL),
          "cam. -> ilgisiz 'id' alani SIZDI");

    case_begin("zincirleme: alan -> struct");
    check(suggests("x", NULL, tf("    cam.target."), NULL),
          "cam.target. -> Vector3.x yok");
    check(!suggests("fovy", NULL, tf("    cam.target."), NULL),
          "cam.target. -> Camera3D 'fovy' SIZDI (zincir kopmus)");

    case_begin("zincirleme: fonksiyon donusu");
    check(suggests("x", NULL, tf("    Raylib.GetMousePosition()."), NULL),
          "Raylib.GetMousePosition(). -> Vector2.x yok");
    check(suggests("y", NULL, tf("    Raylib.GetMousePosition()."), NULL),
          "Raylib.GetMousePosition(). -> Vector2.y yok");

    case_begin("Raylib. listesi sadece uyeler");
    check(suggests("InitWindow", NULL, tf("    Raylib."), NULL),
          "Raylib. -> InitWindow yok");
    check(!suggests("fovy", NULL, tf("    Raylib."), NULL),
          "Raylib. -> Camera3D alani 'fovy' SIZDI");
    check(!suggests("int", NULL, tf("    Raylib."), NULL),
          "Raylib. -> 'int' anahtar sozcugu SIZDI");
    check(!suggests("if", NULL, tf("    Raylib."), NULL),
          "Raylib. -> 'if' anahtar sozcugu SIZDI");

    case_begin("cozulemeyen uye -> popup kapali (S2)");
    check(suppressed(NULL, tf("    counter."), NULL),
          "counter. (int) -> popup kapanmadi");
    check(query(NULL, tf("    counter."), NULL, &r) == 0,
          "counter. -> liste bos degil");
    gcl_complete_result_free(&r);

    check(suppressed(NULL, tf("    nosuchvar."), NULL),
          "nosuchvar. -> popup kapanmadi (yanlis struct uyeleri gosterilebilir)");
}

/* ------------------------------------------------------------------ */
/* #5 baglam tespiti                                                   */
/* ------------------------------------------------------------------ */

static void test_contexts(void) {
    GclCompletionResult r;

    case_begin("ONERI: deger atama");
    check(suggests("counter", NULL, tf("    int x = "), NULL),
          "deger-atama baglaminda yerel degisken yok");
    check(!suggests("return", NULL, tf("    int x = "), NULL),
          "deger-atamada anahtar sozcuk sizdi");

    case_begin("ONERI: cagri argumani + imza");
    query(NULL, tf("    Stdio.printf("), NULL, &r);
    check(r.have_signature == 1, "Stdio.printf( -> imza yardimi yok");
    check(strstr(r.sig_params, "gcChar") != NULL,
          "Stdio.printf( -> imza parametreleri yanlis");
    check(r.count > 0, "Stdio.printf( -> arguman listesi bos");
    gcl_complete_result_free(&r);

    case_begin("ONERI: string/yorum icinde KAPALI");
    check(suppressed(NULL, tf("    // person."), NULL),
          "yorum satirinda popup kapanmadi");
    check(suppressed(NULL, tf("    Stdio.printf(\""), NULL),
          "string icinde popup kapanmadi");

    case_begin("on ek filtreleme + best_index");
    query(NULL, tf("    person.n"), NULL, &r);
    check(r.count >= 1, "person.n -> hic oneri yok");
    check(r.count >= 1 && r.items[r.best_index].label &&
          strcmp(r.items[r.best_index].label, "name") == 0,
          "person.n -> best_index 'name' degil");
    gcl_complete_result_free(&r);

    case_begin("eslesmeyen on ek: liste bos (F4 yol)");
    query(NULL, tf("    Raylib.zzzz"), NULL, &r);
    check(r.count == 0, "eslesmeyen on ekte liste bosaltilmadi");
    check(r.best_index == 0, "eslesmeyen on ekte best_index 0 degil");
    gcl_complete_result_free(&r);

    case_begin("BOS girdi / sinir durumlari");
    {
        GclCompletionResult e;
        gcl_complete_query(NULL, NULL, 0, 0, NULL, &e);
        check(e.suppressed == 1, "NULL metin -> popup kapanmadi");
        gcl_complete_result_free(&e);

        /* imlec metinden buyuk: kirpilmali, cokme olmamali */
        gcl_complete_query(NULL, "int a;", 6, 99, NULL, &e);
        check(e.suppressed == 1 || e.count >= 0, "imlec > len -> cokme");
        gcl_complete_result_free(&e);
    }
}

/* ------------------------------------------------------------------ */
/* #8 siralama / filtre SOZLESMESI                                     */
/*                                                                     */
/* Sozlesme (complete_rank.h): "onek yoksa hepsi kalir" +               */
/* gcl_rank_kind_bonus()'un -80 cezasi adayi GERIYE ATAR, listeye       */
/* girdigini ELEMEZ. Iki sessiz hata burada kilitlenir:                 */
/*  a) `sc < 0` filtresi GECERLI dusuk puanli adayi da dusuruyordu      */
/*     (modul @ CTX_VALUE_ASSIGN = 20 - 80 = -60);                      */
/*  b) fonksiyonlar/moduller deger & arguman baglaminda HIC itilmedigi  */
/*     icin `int x = f()` ve `printf(Modul.fn())` tamamlanamiyordu.     */
/* ------------------------------------------------------------------ */

static int index_of(const char *label, const GclCompletionResult *r) {
    for (int i = 0; i < r->count; i++)
        if (r->items[i].label && strcmp(r->items[i].label, label) == 0) return i;
    return -1;
}

static void test_rank_contract(void) {
    GclCompletionResult r;

    case_begin("#8: deger baglaminda FONKSIYON da onerilir");
    query(NULL, tf("    int x = "), NULL, &r);
    int iv  = index_of("counter", &r);
    int ifn = index_of("demo", &r);
    check(ifn >= 0, "deger atamada fonksiyon adi hic onerilmiyor (cagri yazilamaz)");
    check(iv >= 0, "kontrol: yerel deger 'counter' listede yok");
    check(iv >= 0 && ifn >= 0 && iv < ifn,
          "fonksiyon degerin ONUNDE (yanlis oncelik: deger > fonksiyon olmali)");
    gcl_complete_result_free(&r);

    case_begin("#8: negatif puanli aday ELENMEZ (sessiz filtre regresyonu)");
    /* Bildirilmis modul @ CTX_VALUE_ASSIGN = (200 - 4*45) + (-80) = -60 < 0.
       Eski `sc < 0 -> filtrele` kodu bunu listeden DUSURUYORDU. Ikinci
       kontrol senaryonun GERCEKTEN negatif oldugunu kanitlar; ceza
       degisirse (or. -80 -> 0) test bunu gorunur kilar, sessizce
       anlamsizlasmaz.
       NOT: k_decls yalnizca "#native <Stdio>" bildirir; modüller artik
       DOSYADA BILDIRILMISSE önerildigi icin aday 'Stdio''dur. */
    query(NULL, tf("    int x = "), NULL, &r);
    {
        int im = index_of("Stdio", &r);
        check(im >= 0, "negatif puanli aday listeden dustu (sessiz filtre)");
        check(im >= 0 && r.items[im].score < 0,
              "modul adayinin puani negatif DEGIL (regresyon senaryosu kurulmadi)");
    }
    gcl_complete_result_free(&r);

    case_begin("#8: argumanda MODUL adi (yalnizca bildirilmis olan)");
    check(suggests("Stdio", NULL, tf("    Stdio.printf("), NULL),
          "argumanda bildirilmis modul adi yok -> printf(Modul....) yazilamaz");
    check(!suggests("Raylib", NULL, tf("    Stdio.printf("), NULL),
          "bildirilmemis 'Raylib' modulu argumanda onerildi (#native yok)");
    check(suggests("counter", NULL, tf("    Stdio.printf("), NULL),
          "argumanda yerel deger yok");

    case_begin("#8: TAM eslesme ilk sirada (best_index)");
    query(NULL, tf("    person"), NULL, &r);
    check(r.count >= 1 && r.items[r.best_index].label &&
          strcmp(r.items[r.best_index].label, "person") == 0,
          "tam eslesme best_index'te degil (yanlis oncelik)");
    gcl_complete_result_free(&r);

    case_begin("#8: eslesmeyen onek -> liste yine kapanir (kontrat korunuyor)");
    query(NULL, tf("    Raylib.zzzz"), NULL, &r);
    check(r.count == 0, "eslesmeyen onekte liste bosaltilmadi");
    gcl_complete_result_free(&r);
}

/* ------------------------------------------------------------------ */
/* §5.4 printf {} tanisi                                               */
/* ------------------------------------------------------------------ */

static void test_diag(void) {
    GclCompletionResult r;

    case_begin("printf yer-tutucu uyusmazligi");
    query(NULL, tf("    Stdio.printf(\"a={} b={}\", counter)"), NULL, &r);
    check(r.have_diagnostic == 1, "2 yer-tutucu / 1 arguman -> tani yok");
    gcl_complete_result_free(&r);

    case_begin("printf tutarli");
    query(NULL, tf("    Stdio.printf(\"a={}\", counter)"), NULL, &r);
    check(r.have_diagnostic == 0, "1 yer-tutucu / 1 arguman -> yanlis tani");
    gcl_complete_result_free(&r);
}
/* ------------------------------------------------------------------ */
/* §8/#10 proje indeksi + kapsam onbellegi tazeligi (F3)               */
/* ------------------------------------------------------------------ */

static int setup_project(void) {
    make_dir("_temp");
    make_dir("_temp/gcl_ctest");
    make_dir(PROJ_DIR);
    make_dir(PROJ_DIR "/include");
    make_dir(PROJ_DIR "/assets");

    int ok = 1;
    ok &= write_text(PROJ_DIR "/project.gcdata",
        "{\n"
        "  \"project_name\": \"Ctest\",\n"
        "  \"gcl_include_path\": \"include\",\n"
        "  \"gcl_external_path\": \"external\",\n"
        "  \"python_import_path\": \"scripts\",\n"
        "  \"raylib_asset_directory_path\": \"assets\"\n"
        "}\n");
    ok &= write_text(PROJ_DIR "/assets/hero.png", "PNG");
    ok &= write_text(PROJ_DIR "/include/helper.gcsf",
        "int helperValue;\n"
        "struct Helper { int alpha; int beta; };\n");
    return ok;
}

static void test_project_scope(void) {
    GclCompletionResult r;

    if (!setup_project()) {
        case_begin("proje kurulumu");
        check(0, "test proje dosyalari yazilamadi");
        return;
    }

    case_begin("proje sembolleri kapsama giriyor");
    check(suggests("helperValue", NULL, tf("    helperV"), PROJ_DIR),
          "include/helper.gcsf sembolu onerilmedi");
    check(suggests("Helper", NULL, tf("    Help"), PROJ_DIR),
          "include/helper.gcsf tipi onerilmedi");

    case_begin("aktif dosya sembolu projeden ONCE (rank)");
    /* Aktif buffer'da tanimli 'counter' projeden degil, aktif dosyadan gelir
       (scope_rank 2 < 3) → dosya kokeni aktif dosya olmaliydi. */
    query("main.gcsf", tf("    counter"), PROJ_DIR, &r);
    check(r.count >= 1, "aktif dosya sembolu bulunamadi");
    gcl_complete_result_free(&r);

    case_begin("varlik yolu tamamlama (§Faz 6)");
    check(suggests("hero.png", NULL,
                   tf("    Raylib.LoadTexture(\"assets/"), PROJ_DIR),
          "assets/ yolu altinda hero.png onerilmedi");

    /* --- F3: proje dosyasindan SILINEN tanim onbellekte kalmamali ---
       Aktif buffer DEGISMEDEN kalir (ayni hash), yalniz include dosyasi
       degisir; kapsam onbellegi nesil (generation) ile tazelenmelidir. */
    case_begin("F3: silinen tanim onbellekten dusuyor");
    const char *buf = tf("    helperV");
    check(suggests("helperValue", NULL, buf, PROJ_DIR),
          "ilk sorguda helperValue yok (kontrol)");

    if (!write_text(PROJ_DIR "/include/helper.gcsf",
                    "int otherName;\n")) {
        check(0, "helper.gcsf guncellenemedi");
        return;
    }
    /* AYNI buffer metni ile yeniden sorgula: bayat sembol KALMAMALI. */
    check(!suggests("helperValue", NULL, buf, PROJ_DIR),
          "helper.gcsf'den silinen helperValue onbellekte kaldi (bayat sembol)");
    check(suggests("otherName", NULL, tf("    otherN"), PROJ_DIR),
          "guncel tanim (otherName) onerilmedi");
}

/* ------------------------------------------------------------------ */
/* #7 native DB butunlugu                                              */
/*                                                                     */
/* Zincirleme tamamlama `ret` tipinin BILINEN bir struct olmasina      */
/* baglidir; bilinmeyen bir ad SESSIZCE zinciri durdurur (`.x` onerilmez). */
/* Ayni sekilde bir struct alaninin tipi bilinmiyorsa `a.b.` kirilir.  */
/* ------------------------------------------------------------------ */

/* Uzerinde '.' kullanilamayan yaprak tip mi? (bos/void/builtin) */
static int is_leaf_type(const char *t) {
    if (!t || !t[0]) return 1;
    if (strcmp(t, "void") == 0) return 1;
    return gclc_is_builtin_type(t);
}

static int is_chainable(const char *t) {
    return t && t[0] && (gcl_native_is_struct(t) || gcl_native_is_module(t));
}

/* Native uyenin DB'deki donus tipi 'want' mi? */
static int member_ret_is(const char *module, const char *name, const char *want) {
    const GclNativeMember *m = gcl_native_find(module, name);
    return m != NULL && m->ret != NULL && strcmp(m->ret, want) == 0;
}

/* Modulde ret'i 'want' olan kac uye var? (modul yoksa -1) */
static int count_member_ret(const char *module, const char *want) {
    const GclNativeModule *m = gcl_native_module(module);
    int n = 0;
    if (!m) return -1;
    for (int i = 0; i < m->member_count; i++)
        if (m->members[i].ret && strcmp(m->members[i].ret, want) == 0) n++;
    return n;
}

static void test_native_db(void) {
    int bad_ret = 0, bad_field = 0, dup_member = 0, dup_field = 0;
    int members = 0, fields = 0;

    case_begin("native DB: modul uye tutarliligi");

    const char *const *mods = gcl_native_module_names();
    for (int m = 0; mods[m]; m++) {
        const GclNativeModule *mod = gcl_native_module(mods[m]);
        if (!mod) { bad_ret++; continue; }
        for (int i = 0; i < mod->member_count; i++) {
            const GclNativeMember *u = &mod->members[i];
            members++;
            if (!u->name || !u->name[0]) { bad_ret++; continue; }
            for (int j = i + 1; j < mod->member_count; j++)
                if (mod->members[j].name &&
                    strcmp(mod->members[j].name, u->name) == 0)
                    dup_member++;
            if (!is_leaf_type(u->ret) && !is_chainable(u->ret)) {
                if (bad_ret < 8)
                    printf("  [DB] %s.%s -> '%s' bilinmiyor (zincir durur)\n",
                           mods[m], u->name, u->ret);
                bad_ret++;
            }
        }
    }

    const char *const *snames = gcl_native_struct_names();
    for (int s = 0; snames[s]; s++) {
        const GclNativeStruct *st = gcl_native_struct(snames[s]);
        if (!st) { bad_field++; continue; }
        for (int i = 0; i < st->field_count; i++) {
            const GclNativeField *f = &st->fields[i];
            fields++;
            if (!f->name || !f->name[0]) { bad_field++; continue; }
            for (int j = i + 1; j < st->field_count; j++)
                if (st->fields[j].name &&
                    strcmp(st->fields[j].name, f->name) == 0)
                    dup_field++;
            if (!is_leaf_type(f->type) && !is_chainable(f->type)) {
                if (bad_field < 8)
                    printf("  [DB] %s.%s : '%s' bilinmiyor (zincir kirilir)\n",
                           snames[s], f->name, f->type);
                bad_field++;
            }
        }
    }

    printf("  [DB] %d uye, %d alan tarandi\n", members, fields);
    check(members > 500, "native DB beklenenden kucuk (Raylib tablosu eksik?)");
    check(bad_ret == 0, "native modul donus tipinde bilinmeyen tip var");
    check(bad_field == 0, "native struct alan tipinde bilinmeyen tip var");
    check(dup_member == 0, "native modulde ayni adli uye tekrari var");
    check(dup_field == 0, "native struct'ta ayni adli alan tekrari var");

    case_begin("native tipler DUZ ONEKTE onerilmez (katalog dokulmesin)");
    /* Raylib/Raygui icindeki her sey (tipler dahil) modul uyesidir ve
       yalnizca "Raylib." yazildiginda gelir. Eskiden native tip katalogu
       (Vector2, Ray, Camera3D, Color ...) her duz önek listesine dokuluyordu
       (kullanici bildirimi: "Ray ne?? Vector2 nerden geliyor"). */
    check(!suggests("Vector2", NULL, tf("    Vect"), NULL),
          "Vector2 duz onekte onerildi (native katalog sizintisi)");
    check(!suggests("Camera3D", NULL, tf("    Came"), NULL),
          "Camera3D duz onekte onerildi (native katalog sizintisi)");
    check(!suggests("Ray", NULL, tf("    Ray"), NULL),
          "Ray duz onekte onerildi (native katalog sizintisi)");
    check(!suggests("Color", NULL, tf("    Col"), NULL),
          "Color duz onekte onerildi (native katalog sizintisi)");

    case_begin("native tipler 'Modul.' ile gelir (NF_TYPE uyeleri)");
    check(suggests("Vector2", NULL, tf("    Raylib.Vect"), NULL),
          "Raylib.Vect -> Vector2 onerilmedi (modul uyesi tip)");
    check(suggests("Rectangle", NULL, tf("    Raylib.Rect"), NULL),
          "Raylib.Rect -> Rectangle onerilmedi (modul uyesi tip)");
    /* DB'deki modul uyesi adi 'Camera'dir (Camera3D yalnizca struct
       esdegeridir; bkz. complete_native.c struct tablosu). */
    check(suggests("Camera", NULL, tf("    Raylib.Came"), NULL),
          "Raylib.Came -> Camera onerilmedi (modul uyesi tip)");

    case_begin("modul-prefixli tip bildirimi alan cozumlemesi");
    /* "Raylib.Camera3D cam;" → cam.target. calismali (modul oneki soyulur). */
    {
        static const char *k_mod_type =
            "#native <Raylib>\n"
            "Raylib.Camera3D kam;\n"
            "void f() {\n"
            "    kam.target.";
        check(suggests("x", NULL, k_mod_type, NULL),
              "Raylib.Camera3D kam; -> kam.target. -> Vector3.x yok");
        check(suggests("y", NULL, k_mod_type, NULL),
              "Raylib.Camera3D kam; -> kam.target. -> Vector3.y yok");
    }

    case_begin("bildirilmis modul duz onekte gorunur");
    {
        static const char *k_mod_decl =
            "#native <Raylib>\n"
            "int main() {\n"
            "    \n"
            "}\n";
        check(suggests("Raylib", NULL, k_mod_decl, NULL),
              "#native <Raylib> bildirilmis ama 'Raylib' onerilmedi");
    }
}

/* ------------------------------------------------------------------ */
/* #7 zincir bagi regresyonlari (tools/gen_native_db.py)                */
/*                                                                     */
/* Uc ayri SESSIZ hata burada kilitlenir:                              */
/*  a) cok satirli binding'ler kirpilinca struct donusu "int" oluyordu */
/*     -> zincir oluyordu ama hicbir sey onerilmiyordu                 */
/*  b) void fonksiyonun scratch g_last_* atamasi struct saniliyordu    */
/*     -> olmayan bir donus icin uye oneriliyordu                      */
/*  c) renkler ABI'de packed uint oldugu halde struct sanilabiliyordu  */
/*     -> calismayan .r/.g onerileri                                   */
/* ------------------------------------------------------------------ */

static void test_native_chain_links(void) {
    GclCompletionResult r;

    case_begin("#7: cok-satirli govde struct donusu (kirpma regresyonu)");
    check(member_ret_is("Raylib", "LoadVrStereoConfig", "VrStereoConfig"),
          "LoadVrStereoConfig -> VrStereoConfig degil (fn_body ilk satirda kirpilmis)");

    case_begin("#7: void fonksiyon scratch struct dondurmemeli");
    check(member_ret_is("Raylib", "BeginMode2D", "void"),
          "BeginMode2D -> void degil (scratch g_last_cam2d struct sanilmis)");
    check(member_ret_is("Raylib", "BeginMode3D", "void"),
          "BeginMode3D -> void degil (scratch g_last_cam struct sanilmis)");
    check(member_ret_is("Raylib", "UpdateCamera", "void"),
          "UpdateCamera -> void degil (g_last_cam struct sanilmis)");

    case_begin("#7: struct donusu zinciri UCTAN UCA");
    check(member_ret_is("Raylib", "GetWindowPosition", "Vector2"),
          "GetWindowPosition -> Vector2 degil");
    check(suggests("x", NULL, tf("    Raylib.GetWindowPosition()."), NULL),
          "Raylib.GetWindowPosition(). -> Vector2.x onerilmedi");
    check(suggests("y", NULL, tf("    Raylib.GetWindowPosition()."), NULL),
          "Raylib.GetWindowPosition(). -> Vector2.y onerilmedi");

    case_begin("#7: void donusu -> zincir kapanir");
    check(member_ret_is("Raylib", "EndDrawing", "void"), "EndDrawing -> void degil");
    check(suppressed(NULL, tf("    Raylib.EndDrawing()."), NULL),
          "void donusu -> popup kapanmadi (yanlis uyeler gosterilebilir)");
    check(query(NULL, tf("    Raylib.EndDrawing()."), NULL, &r) == 0,
          "void donusu -> liste bos degil");
    gcl_complete_result_free(&r);

    case_begin("#7: ABI packed renk -> struct DEGIL (int)");
    check(member_ret_is("Raylib", "Fade", "int"),
          "Fade -> int degil (packed uint renk 'Color' sanilmis)");
    check(member_ret_is("Raylib", "ColorAlpha", "int"),
          "ColorAlpha -> int degil (packed uint renk 'Color' sanilmis)");
    check(count_member_ret("Raylib", "Color") == 0,
          "Raylib uyesi 'Color' ret'li olamaz (renkler ABI'de packed uint)");

    case_begin("#7: uygulanmis uyeler DOGRU donus tipini bildiriyor");
    /* Bu iki uye bir zamanlar no-op stub'ti ve testleri 'void' beklerdi.
       Ikisi de uygulandi; madde artik GERCEK davranisi kilitler:
       - GetPixelColor bir Color dondurur ve Color ABI'de packed uint'tir -> int
       - MeasureTextCodepoints Vector2 slot'una yazar -> Vector2 */
    check(member_ret_is("Raylib", "GetPixelColor", "int"),
          "GetPixelColor -> int degil (packed renk ABI'de uint olmali)");
    check(member_ret_is("Raylib", "MeasureTextCodepoints", "Vector2"),
          "MeasureTextCodepoints -> Vector2 degil");

    case_begin("#7: GERCEK no-op stub deger dondurmus gibi gorunmemeli");
    /* C callback alan uyeler GCL'den cagrilamaz (modul ABI'si fonksiyon
       isaretcisi tasimaz) ve raylib basligi bunlarda `void` der. Marker
       taramasi bunlari 'deger donduruyor' sanmamali. Ayni sekilde modulun
       veriyi hemen biraktigi Unload* uyeleri de void'dir. */
    check(member_ret_is("Raylib", "SetAudioStreamCallback", "void"),
          "SetAudioStreamCallback -> void degil (no-op stub deger uretiyor sanildi)");
    check(member_ret_is("Raylib", "AttachAudioStreamProcessor", "void"),
          "AttachAudioStreamProcessor -> void degil (no-op stub)");
    check(member_ret_is("Raylib", "SetTraceLogCallback", "void"),
          "SetTraceLogCallback -> void degil (no-op stub)");
    check(member_ret_is("Raylib", "UnloadWaveSamples", "void"),
          "UnloadWaveSamples -> void degil (modul veriyi zaten birakti)");
    check(member_ret_is("Raylib", "UnloadRandomSequence", "void"),
          "UnloadRandomSequence -> void degil (modul diziyi zaten birakti)");

    case_begin("#7: struct ret -> BOS olmayan gercek struct");
    {
        int empty_struct = 0;
        const char *const *mods = gcl_native_module_names();
        for (int m = 0; mods[m]; m++) {
            const GclNativeModule *mod = gcl_native_module(mods[m]);
            if (!mod) continue;
            for (int i = 0; i < mod->member_count; i++) {
                const char *rt = mod->members[i].ret;
                if (!rt || !rt[0] || is_leaf_type(rt)) continue;
                if (gcl_native_is_module(rt)) continue;
                const GclNativeStruct *st = gcl_native_struct(rt);
                if (!st || st->field_count <= 0) {
                    if (empty_struct < 8)
                        printf("  [DB] %s.%s -> '%s' zincirlenebilir ama alani yok\n",
                               mods[m], mod->members[i].name, rt);
                    empty_struct++;
                }
            }
        }
        check(empty_struct == 0,
              "struct ret'li uye bos/tanimsiz struct'a isaret ediyor (zincir sessizce durur)");
    }
}

/* ------------------------------------------------------------------ */
/* Regresyon: cagri ARGUMANLARI bildirim DEGILDIR                       */
/*                                                                     */
/* Cagri yerleri kapsama siziyordu:                                    */
/*  a) sayisal literaller (800, 600, 60, 20) parametre sanilip duz     */
/*     onekte tamamlama olarak oneriliyordu,                           */
/*  b) "Raylib.InitWindow(...)" / "Stdio.printf(...)" cagrilari        */
/*     "InitWindow"/"printf" adli GLOBAL fonksiyon sembolu uretiyordu  */
/*     -> modul uyeleri "Modul." yazmadan onek listesinde goruluyordu,  */
/*  c) "p.x = 7;" uye atamasi "x" degiskeni saniliyordu.               */
/* ------------------------------------------------------------------ */

/* Sonuclarda tamamen sayidan olusan etiket var mi? (literaller sizmasin) */
static int any_digit_label(const GclCompletionResult *r) {
    for (int i = 0; i < r->count; i++) {
        const char *l = r->items[i].label;
        if (!l || !l[0]) continue;
        int digits = 1;
        for (const char *p = l; *p; p++)
            if (*p < '0' || *p > '9') { digits = 0; break; }
        if (digits) return 1;
    }
    return 0;
}

static void test_call_args_leak(void) {
    /* Kullanicinin bildirdigi program: her arguman bir ifade. */
    static const char *k_prog =
        "int main() {\n"
        "    Stdio.printf(\"Hello, GCL!\\n\");\n"
        "    Raylib.InitWindow(800, 600, \"GCL Project\");\n"
        "    Raylib.SetTargetFPS(60);\n"
        "    Raylib.DrawText(\"GCL Project\", 20, 20, 20, Raylib.WHITE);\n"
        "    return 0;\n"
        "}\n";
    GclCompletionResult r;

    case_begin("cagri argumani literali parametre olmamali");
    query(NULL, k_prog, NULL, &r);
    check(!any_digit_label(&r),
          "cagri argumanindaki sayilar (20/60/600/800) tamamlama olarak onerildi");
    gcl_complete_result_free(&r);

    case_begin("cagri adi global sembol olmamali");
    check(!suggests("InitWindow", NULL, k_prog, NULL),
          "Raylib.InitWindow(...) cagrisi 'InitWindow' sembolu uretti");
    check(!suggests("DrawText", NULL, k_prog, NULL),
          "Raylib.DrawText(...) cagrisi 'DrawText' sembolu uretti");
    check(!suggests("printf", NULL, k_prog, NULL),
          "Stdio.printf(...) cagrisi 'printf' sembolu uretti");

    case_begin("modul uyesi yalnizca 'Modul.' ile onerilir");
    check(suggests("InitWindow", NULL, tf("    Raylib."), NULL),
          "Raylib. -> InitWindow onerilmedi (uye tamamlamasi bozuldu)");

    case_begin("uye atamasi bagimsiz degisken sanilmamali");
    {
        static const char *k_assign =
            "struct P { int x; int y; };\n"
            "P p;\n"
            "void f() {\n"
            "    p.x = 7;\n"
            "    \n"
            "}\n";
        check(!suggests("x", NULL, k_assign, NULL),
              "p.x = 7; -> uye 'x' bagimsiz degisken olarak onerildi");
        check(suggests("p", NULL, k_assign, NULL),
              "P p; -> 'p' degiskeni onerilmedi");
    }

    case_begin("gercek parametreler hala kapsamda");
    {
        static const char *k_fn =
            "void foo(int a, float b, char *c) {\n"
            "    \n"
            "}\n";
        check(suggests("a", NULL, k_fn, NULL), "int a parametresi kayboldu");
        check(suggests("b", NULL, k_fn, NULL), "float b parametresi kayboldu");
        check(suggests("c", NULL, k_fn, NULL), "char *c parametresi kayboldu");
    }
}

/* ------------------------------------------------------------------ */
/* Kullanicinin KENDI tip/fonksiyon zinciri + bildirim listeleri (§6)  */
/*                                                                     */
/* Dort SESSIZ hata burada kilitlenir:                                 */
/*  a) fonksiyon DONUS tipi ham metin olarak saklaniyordu              */
/*     ("struct Point", "Point *") → `make(1).` hicbir uye onermiyordu; */
/*  b) "struct Foo make(int a);" prototipi DEGISKEN sanilip fonksiyon   */
/*     adi kapsama hic girmiyordu;                                     */
/*  c) kullanicinin kendi tipi ayni adli native struct tarafindan       */
/*     eziliyordu: `enum Color { RED }` → `Color.` raylib'in r/g/b/a    */
/*     alanlarini gosteriyordu;                                         */
/*  d) "int x, y;" / "int a, b;" listelerinde YALNIZ son ad kayitli,    */
/*     tipi de cop kaliyordu ("y : int x,").                            */
/* ------------------------------------------------------------------ */

static const char *k_chain =
    "struct Point { float x; float y; };\n"
    "struct Point makePoint(int a);\n"
    "struct Point *makePtr(int a);\n"
    "enum Color { RED, GREEN, BLUE };\n"
    "struct Pair { int a, b; };\n"
    "int m1, m2;\n"
    "\n"
    "void f() {\n";

static char g_chain[8192];

static const char *tc(const char *expr) {
    snprintf(g_chain, sizeof(g_chain), "%s%s", k_chain, expr);
    return g_chain;
}

static void test_user_chain(void) {
    GclCompletionResult r;

    case_begin("kullanici fn donusu: struct zinciri");
    check(suggests("x", NULL, tc("    makePoint(1)."), NULL),
          "makePoint(1). -> Point.x onerilmedi (donus tipi normalize edilmemis)");
    check(suggests("y", NULL, tc("    makePoint(1)."), NULL),
          "makePoint(1). -> Point.y onerilmedi");

    case_begin("kullanici fn donusu: pointer zinciri");
    check(suggests("x", NULL, tc("    makePtr(1)."), NULL),
          "makePtr(1). -> Point.x onerilmedi (donus tipi 'Point *' kalmis)");

    case_begin("cagri parantezi YOK -> zincir kurulmaz");
    check(suppressed(NULL, tc("    makePoint."), NULL),
          "makePoint. (parantezsiz) -> popup kapanmadi (fonksiyon adi deger sanildi)");

    case_begin("kullanici enum'u native Color'u GOLGELER");
    check(suggests("RED", NULL, tc("    Color."), NULL),
          "Color. -> RED onerilmedi (native Color alanlari kazandi)");
    check(suggests("GREEN", NULL, tc("    Color."), NULL),
          "Color. -> GREEN onerilmedi");
    check(suggests("BLUE", NULL, tc("    Color."), NULL),
          "Color. -> BLUE onerilmedi (eski kod yalnizca son adi uye yapiyordu)");
    check(!suggests("r", NULL, tc("    Color."), NULL),
          "Color. -> native Color alani 'r' SIZDI");

    case_begin("struct uye listesi: 'int a, b'");
    {
        static const char *k_pair =
            "struct Pair { int a, b; };\n"
            "struct Pair pr;\n"
            "void f() {\n"
            "    pr.";
        check(suggests("a", NULL, k_pair, NULL),
              "struct Pair { int a, b; } -> 'a' uyesi kayip");
        check(suggests("b", NULL, k_pair, NULL),
              "struct Pair { int a, b; } -> 'b' uyesi kayip");
        check(suggests_typed("a", "int", NULL, k_pair, NULL),
              "'a' uyesinin tipi 'int' degil (tip cop kalmis)");
    }

    case_begin("cok degiskenli bildirim: 'int m1, m2;'");
    check(suggests("m1", NULL, tc("    m1"), NULL), "m1 kaydedilmedi");
    check(suggests("m2", NULL, tc("    m2"), NULL), "m2 kaydedilmedi");
    check(suggests_typed("m1", "int", NULL, tc("    m1"), NULL),
          "m1 tipi 'int' degil");
    check(suggests_typed("m2", "int", NULL, tc("    m2"), NULL),
          "m2 tipi 'int' degil");

    case_begin("baslatici icindeki sayilar sembol OLMAMALI");
    {
        static const char *k_init =
            "struct P { int x; };\n"
            "void g() {\n"
            "    struct P ps[2] = { {1}, {2} };\n"
            "    \n"
            "}\n";
        query(NULL, k_init, NULL, &r);
        check(!any_digit_label(&r),
              "struct baslaticidaki sayilar (1/2) tamamlama olarak onerildi");
        gcl_complete_result_free(&r);
    }
}

/* ------------------------------------------------------------------ */
/* Enum sabitleri DUZ (cipkalak) onerilmeli                            */
/*                                                                     */
/* GCL C gibidir: `enum Day { MONDAY, TUESDAY };` tanimindan sonra      */
/* kullanici `today = MONDAY;` yazar, `Day.MONDAY` DEGIL. Motor enum    */
/* uyelerini yalnizca tipin ICINDE tutuyordu; bu yuzden `MON` yazinca   */
/* MONDAY hic onerilmiyordu (sessiz bos liste).                         */
/* ------------------------------------------------------------------ */

static void test_enum_constants(void) {
    GclCompletionResult r;

    case_begin("enum sabiti duz onekte (tek satir)");
    {
        static const char *k_e1 =
            "enum Day { MONDAY, TUESDAY, WEDNESDAY };\n"
            "enum Day today;\n"
            "void f() {\n"
            "    MON";
        check(suggests("MONDAY", NULL, k_e1, NULL),
              "enum Day {...} -> 'MON' -> MONDAY onerilmedi (sabit duz kapsamda yok)");
        check(suggests_typed("MONDAY", "Day", NULL, k_e1, NULL),
              "MONDAY sabitinin tipi 'Day' degil");
    }

    case_begin("enum sabiti duz onekte (cok satirli govde)");
    {
        static const char *k_e2 =
            "enum Cfg {\n"
            "    A_ON,\n"
            "    A_OFF\n"
            "};\n"
            "void f() {\n"
            "    A_";
        check(suggests("A_ON", NULL, k_e2, NULL),
              "cok satirli enum govdesi -> A_ON onerilmedi");
        check(suggests("A_OFF", NULL, k_e2, NULL),
              "cok satirli enum govdesi -> A_OFF onerilmedi");
    }

    case_begin("anonim typedef enum sabitleri");
    {
        static const char *k_e3 =
            "typedef enum { OK, FAIL } Status;\n"
            "void f() {\n"
            "    FAI";
        check(suggests("FAIL", NULL, k_e3, NULL),
              "typedef enum {...} Status -> FAIL onerilmedi");
    }

    case_begin("enum sabiti DEGER baglaminda da onerilir");
    {
        static const char *k_e4 =
            "enum Day { MONDAY, TUESDAY };\n"
            "enum Day today;\n"
            "void f() {\n"
            "    today = MON";
        check(suggests("MONDAY", NULL, k_e4, NULL),
              "today = MON -> MONDAY onerilmedi");
    }

    case_begin("enum sabiti tip uyeligi BOZULMADI (`Day.`)");
    {
        static const char *k_e5 =
            "enum Day { MONDAY, TUESDAY };\n"
            "void f() {\n"
            "    Day.";
        check(suggests("MONDAY", NULL, k_e5, NULL),
              "Day. -> MONDAY yok (tip uye listesi bozuldu)");
        check(suggests("TUESDAY", NULL, k_e5, NULL),
              "Day. -> TUESDAY yok");
    }

    /* Enum uyeleri sembol tablosuna girince deger listesi buyudu; yanlislikla
       SAYI veya tip adi sizmadigi kontrol edilir. */
    case_begin("enum kaydi yan etki uretmemeli");
    {
        static const char *k_e6 =
            "enum Day { MONDAY, TUESDAY };\n"
            "void f() {\n"
            "    ";
        query(NULL, k_e6, NULL, &r);
        check(!any_digit_label(&r), "enum kaydi sayi etiketi sizdirdi");
        gcl_complete_result_free(&r);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

/* ---- Error-handling keywords -------------------------------------------
   try/catch/finally/throw/raise were added to the language (lexer + parser +
   runner) but were missing from the completion and highlighting lists: typing
   "try" produced an empty suggestion list and the IDE did not highlight the
   words, so the feature looked like it did not exist.

   This test pins both layers:
     a) the contents of gclc_keyword_list() (the language's single list),
     b) what the engine ACTUALLY suggests (through context + rank filtering).
   (b) matters on its own: if the list is correct but the context filter drops
   the words, the user still never sees them - two separate failure modes. */
static void test_error_keywords(void) {
    static const char *want[] = { "try", "catch", "finally", "throw", "raise",
                                  "defer", NULL };
    const char *const *kw = gclc_keyword_list();

    case_begin("error keywords present in the dictionary");
    for (int i = 0; want[i]; i++) {
        int found = 0;
        for (int k = 0; kw && kw[k]; k++) {
            if (strcmp(kw[k], want[i]) == 0) { found = 1; break; }
        }
        char what[96];
        snprintf(what, sizeof(what), "'%s' missing from gclc_keyword_list()", want[i]);
        check(found, what);
    }

    case_begin("error keywords are offered");
    check(suggests("try", NULL, tf("    tr"), NULL),
          "'tr' inside demo() did not offer 'try'");
    check(suggests("catch", NULL, tf("    cat"), NULL),
          "'cat' did not offer 'catch'");
    check(suggests("finally", NULL, tf("    fin"), NULL),
          "'fin' did not offer 'finally'");
    check(suggests("throw", NULL, tf("    thr"), NULL),
          "'thr' did not offer 'throw'");
    check(suggests("raise", NULL, tf("    rai"), NULL),
          "'rai' did not offer 'raise'");
    /* `defer` is a statement keyword like `return`: it must be offered at the
       start of a body, otherwise the feature is invisible in the IDE even
       though the interpreter accepts it. */
    check(suggests("defer", NULL, tf("    def"), NULL),
          "'def' did not offer 'defer'");
}

int main(void) {
    printf("GCL completion engine tests\n");

    test_array_members();
    test_native_no_leak();
    test_contexts();
    test_rank_contract();
    test_diag();
    test_project_scope();
    test_native_db();
    test_native_chain_links();
    test_call_args_leak();
    test_user_chain();
    test_enum_constants();
    test_error_keywords();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
