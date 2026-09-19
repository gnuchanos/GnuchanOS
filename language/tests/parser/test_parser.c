
#include "gcl_parser.h"
#include "gcl_lexer.h"
#include "gcl_diag.h"   /* TURN 46: the rich sink gcl_parse_diag() fills */
#include "gcl_source.h" /* TURN 48: the map that keeps a position honest */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
static const char *g_case = "";

static void case_begin(const char *name) { g_case = name; }

static void check(int cond, const char *what) {
    if (cond) { g_pass++; return; }
    g_fail++;
    printf("FAIL [%s] %s\n", g_case, what);
}

/* --- yardimcilar ------------------------------------------------- */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[r] = '\0';
    return buf;
}

/*
 * Kaynagi ayristir ve programı SERBEST BIRAK. `free_stmt` zinciri
 * (`stmt->next`) dolasmazsa sizinti; iki kez dolasirsa/heap bozulursa
 * bu dongu coker. Donus: ayristirma basarili mi?
 */
static int parse_and_free(const char *src, char **err_out) {
    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) {
        if (err_out) *err_out = err; else free(err);
        return 0;
    }
    GclProgram *prog = gcl_parse(tl, &err);
    int ok = (prog != NULL);
    if (prog) gcl_program_free(prog);
    if (!ok && err && err_out) *err_out = err; else free(err);
    gcl_token_free(tl);
    return ok;
}

/*
 * prog->stmts icinde `kind` turunden kac UST-SEVIYE ifade var?
 *
 * SOZLESME (gcl_parse): parse_stmt bir `next` ZINCIRI uretir ama gcl_parse
 * zinciri PARCALAR — her dugumu AYRI bir prog->stmts girdisi yapar ve
 * next'i NULL'lar. Yani `typedef struct {...} X;` TEK bir girdi degil,
 * IKI girdidir (STMT_TYPEDEF + STMT_STRUCT_DECL); `global a, b, c;` de
 * uc STMT_VAR_DECL uretir. Bu parcalama serbest birakma ve tamamlama
 * kapsaminin dayandigi davranistir; burada KILITLENIR.
 */
static int count_stmts_of_kind(const char *src, GclStmtKind kind) {
    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) { free(err); return -1; }
    GclProgram *prog = gcl_parse(tl, &err);
    int n = -1;
    if (prog) {
        n = 0;
        for (int i = 0; i < prog->count; i++)
            if (prog->stmts[i] && prog->stmts[i]->kind == kind) n++;
        gcl_program_free(prog);
    } else {
        free(err);
    }
    gcl_token_free(tl);
    return n;
}

/* --- testler ----------------------------------------------------- */

/* Ornek dosyalarin TAMAMI: ayristir + serbest birak. */
static void test_examples(void) {
    static const char *files[] = {
        "language/examples/gcl_simple_syntax/0_comments.gcsf",
        "language/examples/gcl_simple_syntax/1_argv_example.gcsf",
        "language/examples/gcl_simple_syntax/2_vanilla_c.gcsf",
        "language/examples/gcl_simple_syntax/3_typedef.gcsf",
        "language/examples/gcl_simple_syntax/4_array.gcsf",
        "language/examples/gcl_simple_syntax/5_char_array.gcsf",
        "language/examples/gcl_simple_syntax/6_gcl_types.gcsf",
        "language/examples/gcl_simple_syntax/7_gcl_types_array.gcsf",
        "language/examples/gcl_simple_syntax/8_typedef_struct.gcsf",
        "language/examples/gcl_simple_syntax/9_macros.gcsf",
        "language/examples/gcl_simple_syntax/10_include_test.gcsf",
        "language/examples/gcl_simple_syntax/11_scanf_commandline_test.gcsf",
    };

    case_begin("ornek .gcsf dosyalari ayristirilir + serbest birakilir");
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char *src = read_file(files[i]);
        if (!src) {
            printf("  [skip] okunamadi: %s\n", files[i]);
            continue;
        }
        char *err = NULL;
        int ok = parse_and_free(src, &err);
        if (!ok) {
            printf("FAIL [%s] %s: %s\n", g_case, files[i],
                   err ? err : "(hata yok)");
            g_fail++;
        } else {
            g_pass++;
        }
        free(err);
        free(src);
    }
}

/* Zincirli dugumler: typedef struct ve typeless global listesi.
   Bu iki yol `stmt->next` kullanir; gcl_program_free zinciri dolasmali. */
static void test_statement_chains(void) {
    case_begin("typedef struct: zincir PARCALANIP iki ust-seviye ifade olur");
    {
        const char *src =
            "typedef struct {\n"
            "    int id;\n"
            "    char name[20];\n"
            "} Person;\n";
        check(count_stmts_of_kind(src, STMT_TYPEDEF) == 1,
              "STMT_TYPEDEF sayisi 1 degil");
        /* Uye listesi ayri bir dugum olarak prog->stmts'te olmali; yoksa
           uye adlari/turleri de kapsamdan ve serbest birakmadan duserdi. */
        check(count_stmts_of_kind(src, STMT_STRUCT_DECL) == 1,
              "STMT_STRUCT_DECL sayisi 1 degil (uye listesi kayboldu)");
    }

    case_begin("typeless global listesi: 3 ayri ust-seviye bildirim olur");
    {
        const char *src = "global g_alpha, g_beta, g_gamma;\n";
        check(count_stmts_of_kind(src, STMT_VAR_DECL) == 3,
              "global listesi 3 STMT_VAR_DECL uretmedi (parcalama bozuk)");
    }

    /* Zinciri 200 kez kur + serbest birak: cift serbest/heap bozulmasi
       olsaydi bu dongu er ya da gec cokerdi. */
    case_begin("zincir kur/free dongusu (cift serbest regresyonu)");
    {
        const char *src =
            "typedef struct { int a; char b[8]; } X;\n"
            "global one, two, three;\n"
            "void f() { int i = 0; for (i = 0; i < 3; i++) { } }\n";
        int all_ok = 1;
        for (int i = 0; i < 200; i++)
            if (!parse_and_free(src, NULL)) { all_ok = 0; break; }
        check(all_ok == 1, "tekrarlanan parse+free dongusu basarisiz");
    }
}

/* switch icinde case/default DISINDA kalan ifade: ayristirilir ve
   free_stmt ile serbest birakilir (eskiden yalniz dugum birakilirdi). */
static void test_switch_discard(void) {
    case_begin("switch: case disi ifade ayristirilir + serbest birakilir");
    {
        const char *src =
            "void f() {\n"
            "    int x = 1;\n"
            "    switch (x) {\n"
            "        x = 2;\n"
            "        case 1:\n"
            "            x = 3;\n"
            "            break;\n"
            "        default:\n"
            "            break;\n"
            "    }\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1, "switch govdesi ayristirilamadi");
    }
}

/* Operator alias'lari: AND/OR/XOR/NOT + BIT_* ve kaydirma adlari.
   (Bu yol eskiden her denemede tok_str sizintisi uretiyordu.) */
static void test_operator_aliases(void) {
    case_begin("operator alias'lari (AND/OR/XOR/NOT/BIT_*/SHIFT)");
    {
        const char *src =
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 0;\n"
            "    int c = 0;\n"
            "    a = a AND b;\n"
            "    a = a OR b;\n"
            "    a = a XOR b;\n"
            "    a = NOT b;\n"
            "    a = a BIT_AND b;\n"
            "    a = a BIT_OR b;\n"
            "    a = a BIT_XOR b;\n"
            "    a = a LEFT_SHIFT 2;\n"
            "    a = a RIGHT_SHIFT 1;\n"
            "    c = 2 + 3 * 4 - 1;\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1, "operator alias ifadeleri ayristirilamadi");
    }

    case_begin("operator alias DEGIL: normal tanimlayici ifade bozmuyor");
    {
        const char *src = "void f() { int AND2 = 1; int x = AND2; }\n";
        check(parse_and_free(src, NULL) == 1,
              "AND2 gibi tanimlayici yanlislikla operator sayildi");
    }
}

/* Bilesik literal + ic ice uye/dizi zinciri (ifade agaci sahipligi). */
static void test_compound_and_chains(void) {
    case_begin("bilesik literal + zincirli erisim");
    {
        const char *src =
            "struct P { int x; int y[3]; };\n"
            "void f() {\n"
            "    struct P p = { 1, {2, 3, 4} };\n"
            "    int n = p.y[1];\n"
            "    p.x = p.y[0] + n;\n"
            "    int q = (2 + 3) * 4;\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1,
              "bilesik literal / zincirli erisim ayristirilamadi");
    }
}

/* Native modul struct tipleri nitelikli de yazilabilir: `Raylib.Rectangle r;`
   (bkz. complete_type.c: "hem `Vector2 v;` hem `Raylib.Vector2 v;` calisir").
   Bu yol eskiden hic ayristirilamiyordu: `Raylib.Rectangle Player = ...;`
   bir ifade ifadesi sayiliyor, `(Raylib.Rectangle){...}` ise parantezli uye
   erisimi olarak okunup `{` disarida birakiliyordu. Sonuc, ilk virgulde
   anlamsiz bir "expected expression at 7:53" hatasiydi. */
static void test_qualified_type_names(void) {
    case_begin("nitelikli native tip: `Raylib.Rectangle p = (Raylib.Rectangle){...};`");
    {
        const char *src =
            "int main() {\n"
            "    Raylib.Rectangle Player = (Raylib.Rectangle){ 400, 300, 200, 150 };\n"
            "    Player.x = Player.x + 1;\n"
            "    int w = Player.width;\n"
            "    return 0;\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1,
              "nitelikli bildirim + bilesik literal ayristirilamadi");
    }

    case_begin("nitelikli native tip: struct uyesi + donus tipi + dizi");
    {
        const char *src =
            "struct S { Raylib.Vector2 pos; int n; };\n"
            "Raylib.Rectangle make() {\n"
            "    Raylib.Rectangle r = { 0, 0, 10, 10 };\n"
            "    return r;\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1,
              "nitelikli struct uyesi / donus tipi ayristirilamadi");
    }

    /* Kilit: noktali zincir SADECE bir bildirim baslatirsa tip sayilir.
       `Player.x = 5;` bir uye atamasidir — bildirim sanilirsa ifade kaybolur. */
    case_begin("nitelikli tip DEGIL: uye atamasi ve modul cagrisi bozulmamali");
    {
        const char *src =
            "int main() {\n"
            "    Player.x = 5;\n"
            "    Player.rec.x = 7;\n"
            "    Raylib.InitWindow(800, 600, \"t\");\n"
            "    int n = Raylib.WindowShouldClose();\n"
            "    return n;\n"
            "}\n";
        check(parse_and_free(src, NULL) == 1,
              "uye atamasi / modul cagrisi bildirim sanildi");
    }
}

/* Top-level `TYPE name = ...;` bildiriminin init ifadesi (yoksa NULL). */
static GclExpr *find_top_init(GclProgram *prog, const char *varname) {
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (!s || s->kind != STMT_VAR_DECL) continue;
        if (s->u.var_decl.name && strcmp(s->u.var_decl.name, varname) == 0)
            return s->u.var_decl.init;
    }
    return NULL;
}

/* UNARY ONCELIK regresyonu.
   parse_expr(p, min_prec) eskiden min_prec'i YOK SAYIP parse_assign'e
   dusuyordu (taban 1 = en dusuk). Bu yuzden bir unary operatorun operand'i
   ifadenin GERI KALANINI yutuyordu:
       -3 + 5   ->  -(3 + 5)  = -8
       !a && b  ->  !(a && b)
   Ikincisi her `while (!WindowShouldClose() && ...)` oyun dongusunu SONSUZ
   donguye ceviriyordu (sessizce). Test agacin SEKLI'ni kilitler. */
static void test_unary_precedence(void) {
    static const char *src =
        "int a = 1;\n"
        "int b = 0;\n"
        "int sum = -3 + 5;\n"
        "int cond = !a && b;\n"
        "int prod = 2 * -3 + 1;\n";

    case_begin("unary operand tum ifadeyi yutmamali (min_prec)");

    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) {
        printf("FAIL [%s] lex: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); return;
    }
    GclProgram *prog = gcl_parse(tl, &err);
    if (!prog) {
        printf("FAIL [%s] parse: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); gcl_token_free(tl); return;
    }

    /* `-3 + 5`  ->  BINOP(ADD, UNOP(SUB, 3), 5) */
    GclExpr *sum = find_top_init(prog, "sum");
    check(sum && sum->kind == AST_EXPR_BINOP && sum->op == OP_ADD,
          "-3 + 5: kok BINOP(ADD) degil (unary ifadenin tamamini yuttu)");
    check(sum && sum->left && sum->left->kind == AST_EXPR_UNOP &&
          sum->left->op == OP_SUB,
          "-3 + 5: sol taraf UNOP(SUB) degil");
    check(sum && sum->right && sum->right->kind == AST_EXPR_FLOAT &&
          sum->right->num == 5,
          "-3 + 5: sag taraf 5 degil");

    /* `!a && b`  ->  BINOP(AND, UNOP(NOT, a), b) */
    GclExpr *cond = find_top_init(prog, "cond");
    check(cond && cond->kind == AST_EXPR_BINOP && cond->op == OP_AND,
          "!a && b: kok BINOP(AND) degil -> oyun dongusu sonsuz olur");
    check(cond && cond->left && cond->left->kind == AST_EXPR_UNOP &&
          cond->left->op == OP_NOT,
          "!a && b: sol taraf UNOP(NOT) degil");
    check(cond && cond->right && cond->right->kind == AST_EXPR_VAR,
          "!a && b: sag taraf degil");

    /* `2 * -3 + 1`  ->  BINOP(ADD, BINOP(MUL, 2, UNOP(SUB, 3)), 1) */
    GclExpr *prod = find_top_init(prog, "prod");
    check(prod && prod->kind == AST_EXPR_BINOP && prod->op == OP_ADD,
          "2 * -3 + 1: kok BINOP(ADD) degil");
    check(prod && prod->left && prod->left->kind == AST_EXPR_BINOP &&
          prod->left->op == OP_MUL,
          "2 * -3 + 1: sol taraf BINOP(MUL) degil");

    gcl_program_free(prog);
    gcl_token_free(tl);
}

/* STMT_FUNC_DECL sahipligi (ownership) — TURN 25.

   TURN 23'te 15 "alakasiz" testin hepsi TEK bir hatadan kirmiziydi:
   `free_stmt()` icinde `return_type` IKI KEZ serbest birakiliyordu. Hata
   yalnizca FONKSIYON TANIMLAYAN programlarda gorunuyordu ve semptom
   (STATUS_HEAP_CORRUPTION) testin KONUSUYLA ilgisiz goruntuyordu; bu yuzden
   teshis pahaliydi.

   Bu vaka o cagri zincirini (parse → gcl_program_free) DOGRUDAN kosturur:
   parametreli + govdeli fonksiyonlar (parametre tablosu: bkz. TURN 26),
   ve yakin zamanda
   eklenen `defer` / `try` / `catch` / `finally` dugumlerini iceren bir govde.
   Burada bir cift-free veya eksik free olursa ptest ya coker ya da
   (Linux'ta) `-fsanitize=address` altinda yakalanir — runtime'a hic
   ihtiyac duymadan. */
static GclStmt *find_func_decl(GclProgram *prog, const char *name) {
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (s && s->kind == STMT_FUNC_DECL && s->u.func_decl.name &&
            strcmp(s->u.func_decl.name, name) == 0)
            return s;
    }
    return NULL;
}

static void test_function_decl_teardown(void) {
    /* DIKKAT: `local` (ve `global`) bir ANAHTAR SOZCUKTUR — `local base;`
       baglama bildirimidir. `int local = ...;` bu yuzden GECERSIZDIR
       (parser "expected variable name" der). Vaka adi bunu bilerek
       belgelemez; degisken adi `total` secildi. */
    static const char *src =
        "int add(int a, int b) { int total = a + b; return total; }\n"
        "void nothing() { }\n"
        "int cleanup(int n) { defer cleanup_step(); try { return 1; } "
        "catch (err) { return 0; } finally { finish(); } }\n"
        "typedef struct { int id; } Person;\n"
        /* TURN 26 trigger shape: four `int` parameters fill the initial
           parameter capacity, so the FIFTH (a struct) lands on a slot created
           by `realloc` — the slot the old code never wrote. */
        "int pick(int a, int b, int c, int d, Person p) { return p.id; }\n";

    case_begin("free_stmt: STMT_FUNC_DECL ownership (params + body)");
    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) {
        printf("FAIL [%s] lex: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); return;
    }
    GclProgram *prog = gcl_parse(tl, &err);
    if (!prog) {
        printf("FAIL [%s] parse: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); gcl_token_free(tl); return;
    }

    GclStmt *add = find_func_decl(prog, "add");
    check(add != NULL, "`int add(int a, int b)` ayristirilmadi");
    check(add && add->u.func_decl.param_count == 2, "add: param_count 2 degil");
    check(add && add->u.func_decl.body != NULL, "add: govde yok");
    /* TURN 26: parameters are ONE array of GclParam {name, type}. Every entry
       must carry its own name, and the type must be the declared type — NULL
       only where the parser cannot resolve one (a struct/native type name is a
       single token). The old shape kept a SECOND, parallel array and grew it
       with `realloc` (whose new tail is NOT zeroed), so a struct parameter past
       a growth boundary read uninitialized memory; `pick` below is exactly that
       shape. */
    if (add && add->u.func_decl.params) {
        int all_named = 1;
        for (int i = 0; i < add->u.func_decl.param_count; i++) {
            if (!add->u.func_decl.params[i].name) { all_named = 0; break; }
        }
        check(all_named, "add: params[i].name is NULL for some i");
        check(all_named && strcmp(add->u.func_decl.params[0].name, "a") == 0 &&
              strcmp(add->u.func_decl.params[1].name, "b") == 0,
              "add: parameter names are not a/b");
        check(all_named && add->u.func_decl.params[0].type &&
              strcmp(add->u.func_decl.params[0].type, "int") == 0,
              "add: params[0].type is not 'int'");
    } else {
        check(0, "add: no parameter array (parser records no parameters)");
    }

    /* The growth boundary: 4 int params (initial capacity) + 1 struct param
       (grown slot). Both fields must be written for EVERY index. */
    GclStmt *pick = find_func_decl(prog, "pick");
    check(pick != NULL,
          "`int pick(int a, int b, int c, int d, Person p)` ayristirilmadi");
    check(pick && pick->u.func_decl.param_count == 5, "pick: param_count 5 degil");
    if (pick && pick->u.func_decl.params && pick->u.func_decl.param_count == 5) {
        int named = 1;
        for (int i = 0; i < 5; i++) {
            if (!pick->u.func_decl.params[i].name) { named = 0; break; }
        }
        check(named, "pick: an entry was appended WITHOUT a name (half-written parameter)");
        int typed = 1;
        for (int i = 0; i < 4; i++) {
            if (!pick->u.func_decl.params[i].type ||
                strcmp(pick->u.func_decl.params[i].type, "int") != 0) { typed = 0; break; }
        }
        check(typed, "pick: params[0..3].type is not 'int'");
        check(pick->u.func_decl.params[4].type == NULL,
              "pick: struct parameter's type is not NULL (uninitialized / garbage)");
        check(pick->u.func_decl.params[4].name &&
              strcmp(pick->u.func_decl.params[4].name, "p") == 0,
              "pick: struct parameter's name is not 'p'");
    } else {
        check(0, "pick: no parameter array / wrong param_count");
    }

    check(find_func_decl(prog, "nothing") != NULL, "`void nothing()` ayristirilmadi");
    check(find_func_decl(prog, "cleanup") != NULL,
          "defer/try/catch/finally iceren fonksiyon ayristirilmadi");

    /* TEK serbest birakma. Burada cift-free varsa ptest coker. */
    gcl_program_free(prog);
    gcl_token_free(tl);
}

/* --- TURN 28: STMT_TRY / STMT_THROW / STMT_DEFER ownership ---------------
 *
 * `test_function_decl_teardown` yukarida bu dugumleri KOSAR (ayristirir ve
 * serbest birakir) ama ICERIGINI hic OKUMAZ: tek iddiasi fonksiyonun
 * ayristirilmis OLMASI. TURN 26'nin hatasi (iki ayni tipli alanin yer
 * degistirmesi) tam olarak "kosturuldu ama okunmadi" sinifindandi: serbest
 * birakma cokme uretmeden once alanlar yanlis yazilmis olabilir.
 *
 * Bu vaka `u.try_` / `u.throw_` / `u.defer_` alanlarinin HER BIRINI okur.
 * Birlesim (union) alanlarindan biri digeriyle karistirilirsa ya da serbest
 * birakma yolu degisirse burada kirmizi olur. `throw` payload'i bir string
 * arguman tasir; `free_expr` onu dolasmazsa/bir kez fazla dolasirsa
 * (paylasilan pointer) cift-free olur ve ptest coker. */

/* Bir blogun DOGRUDAN cocuklari arasinda `kind` turunden ilk dugum. */
static GclStmt *block_child(GclStmt *blk, GclStmtKind kind) {
    if (!blk || blk->kind != STMT_BLOCK) return NULL;
    for (int i = 0; i < blk->u.block.count; i++) {
        GclStmt *s = blk->u.block.stmts[i];
        if (s && s->kind == kind) return s;
    }
    return NULL;
}

/* Bir CAGRI ifadesinin cagrilan adi.
   ONEMLI: parser cagrilani `call->left` icinde tutar (bkz. parse_primary:
   `GclExpr *call = new_expr(AST_EXPR_CALL); if (call) call->left = e;`).
   `call->name` bir cagri icin KULLANILMAZ — nitelikli cagrilarda ad MEMBER
   dugumunde kalir (`Raylib.InitWindow(...)` -> left = MEMBER(Raylib,
   InitWindow)). Bu vaka ilk yazildiginda `call->name` okunuyordu ve 4 iddia
   bu yuzden kirmizi oldu: yanlis olan TESTTI, parser degil. */
static const char *call_callee(const GclExpr *e) {
    if (!e || e->kind != AST_EXPR_CALL || !e->left) return NULL;
    if (e->left->kind == AST_EXPR_VAR) return e->left->name;
    if (e->left->kind == AST_EXPR_MEMBER) return e->left->member_name;
    return NULL;
}

/* `blk` icinde `name` adli bir cagri ifadesi var mi? */
static int block_has_call(GclStmt *blk, const char *name) {
    if (!blk || blk->kind != STMT_BLOCK) return 0;
    for (int i = 0; i < blk->u.block.count; i++) {
        GclStmt *s = blk->u.block.stmts[i];
        if (s && s->kind == STMT_EXPR && call_callee(s->u.expr) &&
            strcmp(call_callee(s->u.expr), name) == 0)
            return 1;
    }
    return 0;
}

static void test_try_throw_defer_fields(void) {
    /* `defer` hem ciplak ifade hem blok alir; `catch` hem adli hem ADSIZ
       olabilir; `throw` ifadesiz ("yeniden firlat") olabilir. Dort sekil de
       burada. */
    static const char *src =
        "int guarded(int n) {\n"
        "    defer close_thing(n);\n"
        "    try {\n"
        "        throw Err(7, \"boom\");\n"
        "    } catch (err) {\n"
        "        return 1;\n"
        "    } finally {\n"
        "        finish();\n"
        "    }\n"
        "}\n"
        "int braced(void) { defer { note(); } return 0; }\n"
        "int bare_throw(void) { throw; }\n"
        "int anon_catch(void) { try { return 1; } catch { return 2; } }\n";

    case_begin("free_stmt: STMT_TRY / STMT_THROW / STMT_DEFER fields");
    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) {
        printf("FAIL [%s] lex: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); return;
    }
    GclProgram *prog = gcl_parse(tl, &err);
    if (!prog) {
        printf("FAIL [%s] parse: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); gcl_token_free(tl); return;
    }

    GclStmt *g = find_func_decl(prog, "guarded");
    check(g != NULL, "`int guarded(int n)` ayristirilmadi");
    GclStmt *body = g ? g->u.func_decl.body : NULL;
    check(body && body->kind == STMT_BLOCK, "guarded: govde bir blok degil");

    /* defer close_thing(n);  — CIPLAK (bracesiz) yuk: yuk bir STMT_EXPR'dir,
       bloga SARILMAZ. Bu sozlesme runner'in run_cleanup_body'sinin dayandigi
       seydir; `defer { ... }` ile ayni alani paylasir. */
    GclStmt *df = block_child(body, STMT_DEFER);
    check(df != NULL, "guarded: defer dugumu yok");
    check(df && df->u.defer_.stmt != NULL, "defer: payload NULL");
    if (df && df->u.defer_.stmt) {
        GclStmt *pl = df->u.defer_.stmt;
        check(pl->kind == STMT_EXPR, "defer: ciplak payload STMT_EXPR degil");
        check(pl->u.expr && pl->u.expr->kind == AST_EXPR_CALL,
              "defer: payload bir CAGRI ifadesi degil");
        check(call_callee(pl->u.expr) &&
              strcmp(call_callee(pl->u.expr), "close_thing") == 0,
              "defer: cagri adi 'close_thing' degil (call->left VAR)");
        check(pl->u.expr && pl->u.expr->arg_count == 1,
              "defer: arg_count 1 degil");
        check(pl->u.expr && pl->u.expr->args && pl->u.expr->args[0] &&
              pl->u.expr->args[0]->kind == AST_EXPR_VAR &&
              pl->u.expr->args[0]->name &&
              strcmp(pl->u.expr->args[0]->name, "n") == 0,
              "defer: argumani `n` degiskeni degil");
    }

    GclStmt *tr = block_child(body, STMT_TRY);
    check(tr != NULL, "guarded: try dugumu yok");
    if (tr) {
        check(tr->u.try_.body != NULL, "try: govde NULL");
        check(tr->u.try_.body && tr->u.try_.body->kind == STMT_BLOCK,
              "try: govde bir blok degil");
        check(tr->u.try_.err_name && strcmp(tr->u.try_.err_name, "err") == 0,
              "try: catch baglamasi 'err' degil");
        check(tr->u.try_.catch_body != NULL, "try: catch govdesi NULL");
        check(tr->u.try_.finally_body != NULL, "try: finally govdesi NULL");

        GclStmt *th = block_child(tr->u.try_.body, STMT_THROW);
        check(th != NULL, "try govdesinde throw dugumu yok");
        if (th) {
            GclExpr *e = th->u.throw_.expr;
            check(e != NULL, "throw: ifade NULL (payload kayboldu)");
            check(e && e->kind == AST_EXPR_CALL, "throw: ifade bir cagri degil");
            check(call_callee(e) && strcmp(call_callee(e), "Err") == 0,
                  "throw: cagri adi 'Err' degil (call->left VAR)");
            check(e && e->arg_count == 2, "throw: arg_count 2 degil");
            check(e && e->args && e->args[1] &&
                  e->args[1]->kind == AST_EXPR_STRING && e->args[1]->str &&
                  strcmp(e->args[1]->str, "boom") == 0,
                  "throw: ikinci arguman \"boom\" string'i degil");
        }

        GclStmt *ret = block_child(tr->u.try_.catch_body, STMT_RETURN);
        check(ret != NULL && ret->u.ret.expr != NULL,
              "catch: `return 1` (ifadeli return) yok");
        check(ret && ret->u.ret.type_name == NULL,
              "catch: return.type_name beklenmedik sekilde dolu");
        check(block_has_call(tr->u.try_.finally_body, "finish"),
              "finally: finish() cagrisi yok");
    }

    /* defer { note(); }  — BRAKLI yuk bir STMT_BLOCK'tir. Ciplak hal ile
       ayni alani paylastigi icin ikisi birlikte kilitlenmeli. */
    GclStmt *bc = find_func_decl(prog, "braced");
    check(bc != NULL, "`int braced(void)` ayristirilmadi");
    GclStmt *bdf = bc ? block_child(bc->u.func_decl.body, STMT_DEFER) : NULL;
    check(bdf != NULL, "braced: defer dugumu yok");
    check(bdf && bdf->u.defer_.stmt &&
          bdf->u.defer_.stmt->kind == STMT_BLOCK,
          "defer { ... }: payload STMT_BLOCK degil (ciplak hal ile karisti)");
    if (bdf && bdf->u.defer_.stmt && bdf->u.defer_.stmt->kind == STMT_BLOCK)
        check(block_has_call(bdf->u.defer_.stmt, "note"),
              "defer { ... }: note() cagrisi blokta yok");

    /* throw;  — ifadesiz yeniden firlatma. free_expr(NULL) guvenli olmali. */
    GclStmt *bt = find_func_decl(prog, "bare_throw");
    check(bt != NULL, "`int bare_throw(void)` ayristirilmadi");
    GclStmt *bth = bt ? block_child(bt->u.func_decl.body, STMT_THROW) : NULL;
    check(bth != NULL, "bare_throw: throw dugumu yok");
    check(bth && bth->u.throw_.expr == NULL,
          "`throw;` ifadesi NULL olmali (yeniden firlatma)");

    /* catch { ... }  — ADSIZ baglama: err_name NULL olmali. */
    GclStmt *ac = find_func_decl(prog, "anon_catch");
    check(ac != NULL, "`int anon_catch(void)` ayristirilmadi");
    GclStmt *atr = ac ? block_child(ac->u.func_decl.body, STMT_TRY) : NULL;
    check(atr != NULL, "anon_catch: try dugumu yok");
    check(atr && atr->u.try_.err_name == NULL,
          "`catch {`: baglama adi NULL olmali (adsiz catch)");
    check(atr && atr->u.try_.catch_body != NULL, "`catch {`: govde NULL");
    check(atr && atr->u.try_.finally_body == NULL,
          "`catch {`: finally dalinin olmamasi gerekir (u.try_ karisti mi?)");

    /* TEK serbest birakma: yukaridaki tum alt agaclar burada birakilir.
       Paylasilan bir pointer varsa (orn. throw payload'i iki kez asiliysa)
       bu satir coker. */
    gcl_program_free(prog);
    gcl_token_free(tl);
}

/* --- TURN 46: the "never closed" diagnostic ------------------------------

   `unclosed_openers`, `span_make` and `callee_name` were written for these two
   diagnostics and then never called: a forgotten `}` was reported at the END OF
   FILE (the one place the mistake is not) and a forgotten `)` did not say which
   call it belonged to. The gcsf cases pin the legacy sentence the CLI prints,
   but they cannot see the RICH half - the legacy string has no code and no
   span. This case pins that half through gcl_parse_diag(): the diagnostic's
   CODE must come from the shared vocabulary and its SPAN must be the OPENER's
   (the `{`/`(` the user forgot), not the token the parser stopped on.

   It is also the first caller of gcl_parse_diag() in the tree: that function
   was a DECLARATION in gcl_parser.h with no definition anywhere, and gcl_parse
   hard-set `p.diags = NULL`, so every rich diagnostic this parser built was
   discarded before anyone could read it.

   Position expectations are COMPUTED from the source (the last opener, its own
   line/column), never hardcoded, so a change to the test text cannot silently
   invalidate the assertion. */
static void check_closer_diag(const char *label, const char *src, char opener,
                             GclDiagCode want_code, const char *msg_needle) {
    case_begin(label);

    const char *open = NULL;
    for (const char *q = src; *q; q++) if (*q == opener) open = q;
    check(open != NULL, "test source has no opener");
    if (!open) return;

    int want_line = 1, want_col = 1;
    for (const char *q = src; q < open; q++) {
        if (*q == '\n') { want_line++; want_col = 1; } else want_col++;
    }

    char *err = NULL;
    GclTokenList *tl = gcl_lex(src, strlen(src), &err);
    if (!tl) {
        printf("FAIL [%s] lex: %s\n", g_case, err ? err : "?");
        g_fail++; free(err); return;
    }

    GclDiagList dl;
    gcl_diag_list_init(&dl, "probe.gcsf");
    GclProgram *prog = gcl_parse_diag(tl, &dl);

    check(prog == NULL, "broken source did not fail the parse");
    const GclDiag *d = gcl_diag_first_error(&dl);
    check(d != NULL, "no diagnostic was recorded (rich sink unused)");
    if (d) {
        check(d->code == want_code,
              "diagnostic code is not the one reserved for this failure");
        check(d->span.line == want_line && d->span.col == want_col,
              "span does not point at the opener that is never closed");
        check(d->span.len == 1, "span length is not the 1-character opener");
        /* A reader with no span (a log, a CI diff) must still be sent to the
           opener: the sentence carries the same line and column. */
        char where[48];
        snprintf(where, sizeof(where), "opened at %d:%d", want_line, want_col);
        check(d->message && strstr(d->message, "never closed") &&
              strstr(d->message, where),
              "message does not name the opener's own position");
        if (msg_needle)
            check(d->message && strstr(d->message, msg_needle),
                  "message does not name the callee it belongs to");
    }

    /* And the diagnostic must RENDER. This half had no test at all before the
       CLI started printing through it, and it is where a latent crash lived:
       the header, the source line and the caret are produced by a two-pass
       sink (first count, then write into a heap buffer sized by that count),
       so a disagreement between the passes underflows the room calculation.
       Rendering into a local buffer here exercises exactly that path. */
    char frame[2048];
    size_t fn = gcl_diag_list_render(&dl, src, strlen(src), frame, sizeof(frame));
    check(fn > 0 && frame[0] != '\0', "the diagnostic rendered nothing");
    check(strstr(frame, "never closed") != NULL,
          "rendered frame does not carry the message");
    check(strstr(frame, "^") != NULL,
          "rendered frame has no caret under the offending span");

    gcl_diag_list_free(&dl);
    gcl_token_free(tl);
}

static void test_unclosed_closer_diag(void) {
    /* A block left open: the block code is 2006 and the span is main's `{`. */
    check_closer_diag("unclosed '}': span is the '{', code is GCL2006",
                      "int main() {\n    int x = 1;\n", '{',
                      GCL_E_PARSE_UNCLOSED_BLOCK, NULL);

    /* A call left open: the bracket code is 2018, the span is the call's `(`
       (the LAST opener, not main's), and the message names the qualified
       callee the way callee_name() builds it from the member chain. */
    check_closer_diag("unclosed ')': span is the call's '(', code is GCL2018",
                      "int main() {\n    Stdio.printf(\"hi\"\n}\n", '(',
                      GCL_E_PARSE_EXPECTED_CLOSE, "'Stdio.printf'");
}

/* --- TURN 48: the source map the preprocessor fills ----------------------
 *
 * The CLI can only report a position in the USER'S file because the
 * preprocessor records one origin per line of the buffer it builds, and the
 * renderer reads that back. The map is therefore load-bearing for every
 * diagnostic that comes out of a preprocessed program - and until this case
 * existed, no test called it either.
 *
 * What is pinned is exactly what the new production path relies on:
 *   * add_line() returns the origin's 1-based line number IN THE BUFFER, which
 *     is the number the lexer/parser will later report;
 *   * lookup() hands back the file NAME and that file's own TEXT - the map's
 *     own copy, so it outlives the preprocessed buffer;
 *   * line_expanded() separates a line the user typed from one a macro
 *     rewrote, which is what makes the caret warning honest instead of
 *     pretending an approximate column is exact;
 *   * truncate() drops the tail - the recursive-#include case where the child
 *     recorded lines the parent could NOT paste - and touches nothing when
 *     asked to keep everything, or when there is no map at all.
 */
static void test_source_map(void) {
    GclSourceMap m;
    gcl_smap_init(&m);

    static const char *frag_text = "one\ntwo\nthree\n";
    int top = gcl_smap_add_file(&m, "main.gcsf", "a\nb\n", 4);
    int frag = gcl_smap_add_file(&m, "frag.inc", frag_text, strlen(frag_text));

    case_begin("smap: add_file returns an index and COPIES the text");
    check(top == 0, "the first file did not get index 0");
    check(frag == 1, "the second file did not get index 1");
    check(gcl_smap_find_file(&m, "frag.inc") == frag,
          "find_file did not find a registered name");
    check(gcl_smap_find_file(&m, "never-added.inc") == -1,
          "find_file invented a file");
    {
        size_t blen = 0;
        const char *bt = gcl_smap_file_text(&m, top, &blen);
        check(bt != NULL && blen == 4 && strcmp(bt, "a\nb\n") == 0,
              "the file's own text was not kept");
    }

    case_begin("smap: add_line numbers the buffer, lookup resolves the origin");
    {
        /* Buffer: line 1 is main.gcsf:1; line 2 is frag.inc:2 and was rewritten
           by macro expansion; line 3 is frag.inc:3. If the map confused the
           buffer's numbering with the file's, line 2 would resolve to file
           line 2 of the WRONG file, which is the whole defect being fixed. */
        int n1 = gcl_smap_add_line(&m, top, 1, 0);
        int n2 = gcl_smap_add_line(&m, frag, 2, 1);
        int n3 = gcl_smap_add_line(&m, frag, 3, 0);
        check(n1 == 1 && n2 == 2 && n3 == 3,
              "add_line did not return the buffer's 1-based line number");
        check(gcl_smap_line_count(&m) == 3, "line_count is not 3");

        const char *fname = NULL, *text = NULL;
        size_t tlen = 0;
        int oline = 0;
        check(gcl_smap_lookup(&m, 2, &fname, &text, &tlen, &oline) == 1,
              "lookup refused a line it was given");
        check(fname && strcmp(fname, "frag.inc") == 0,
              "lookup named the wrong file (the include's own name is lost)");
        check(oline == 2, "lookup lost the file's own line number");
        check(text && tlen == strlen(frag_text) && strcmp(text, frag_text) == 0,
              "lookup did not hand back the file's own text");
        check(gcl_smap_line_expanded(&m, 2) == 1,
              "the macro-rewritten line is not flagged as expanded");
        check(gcl_smap_line_expanded(&m, 1) == 0,
              "a line the user typed was flagged as expanded");
        check(gcl_smap_lookup(&m, 99, NULL, NULL, NULL, NULL) == 0,
              "lookup accepted a line that was never recorded");
    }

    case_begin("smap: truncate drops the tail and tolerates everything else");
    {
        gcl_smap_truncate(&m, 2);
        check(gcl_smap_line_count(&m) == 2, "truncate(2) did not keep 2 lines");
        check(gcl_smap_lookup(&m, 2, NULL, NULL, NULL, NULL) == 1,
              "truncate dropped a line it was told to keep");
        check(gcl_smap_lookup(&m, 3, NULL, NULL, NULL, NULL) == 0,
              "truncate kept a line it was told to drop");
        /* Keeping MORE than exists must not grow anything, and a negative
           count must not wrap around into "keep nothing". */
        gcl_smap_truncate(&m, 1000);
        check(gcl_smap_line_count(&m) == 2, "truncate(1000) changed the count");
        gcl_smap_truncate(&m, -1);
        check(gcl_smap_line_count(&m) == 0, "truncate(-1) did not clear the map");
        /* NULL is the "nobody asked for a map" case, so neither call may
           dereference it: the preprocessor passes NULL by design. */
        gcl_smap_truncate(NULL, 5);
        check(gcl_smap_line_count(NULL) == 0, "line_count(NULL) is not 0");
    }

    gcl_smap_free(&m);
}

int main(void) {
    printf("GCL parser tests\n");

    test_examples();
    test_statement_chains();
    test_switch_discard();
    test_operator_aliases();
    test_compound_and_chains();
    test_unary_precedence();
    test_qualified_type_names();
    test_function_decl_teardown();
    test_try_throw_defer_fields();
    test_unclosed_closer_diag();
    test_source_map();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
