/*
 * test_parser.c — GCL AST parser icin BASLIKSIZ (headless) regresyon testi.
 *
 * Parser raylib/UI'ya bagimli DEGILDIR: lexer + parser + free zinciri
 * dogrudan cagrilir.
 *
 *   gcc -std=c99 -I language/_SRC/SharedPipeline -o _temp/ptest.exe \
 *       language/tests/parser/test_parser.c \
 *       language/_SRC/SharedPipeline/gcl_lexer.c \
 *       language/_SRC/SharedPipeline/gcl_parser.c \
 *       language/_SRC/SharedPipeline/gcl_error.c -lm
 *
 * Kapsam (todo "language full bug hunting" #1/#2/#3/#4 + #21):
 *   - ornek .gcsf dosyalarinin TAMAMI ayristirilir,
 *   - parse → gcl_program_free dongusu (ZINCIR serbest birakma),
 *   - `typedef struct {...} X;` zinciri gercekten kuruluyor mu,
 *   - `global a, b;` zinciri kuruluyor mu,
 *   - switch govdesinde atilan ifade (free_stmt ile serbest birakilir),
 *   - operator alias'lari (AND/OR/XOR/NOT) ve bilesik literal.
 */
#include "gcl_parser.h"
#include "gcl_lexer.h"

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

int main(void) {
    printf("GCL parser tests\n");

    test_examples();
    test_statement_chains();
    test_switch_discard();
    test_operator_aliases();
    test_compound_and_chains();
    test_unary_precedence();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
