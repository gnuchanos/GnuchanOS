/*
 * python_syntax.h — Python dilinin kendi kelimeleri (keywords + builtins).
 *
 * gcl-lsp, workspace taramasi + BUZDOLABI (gercek Python) yaninda bu
 * statik tablodan da tamamlama uretir: "impo" -> import, "pri" -> print
 * gibi. Boylece Python'un KENDI dili de LSP'den tamamlanir.
 *
 * Derleme: makefile.py lsp_BUILD icinde gcl_lsp.c ile birlikte linklenir.
 */

#ifndef GCL_PYTHON_SYNTAX_H
#define GCL_PYTHON_SYNTAX_H

#include <stddef.h>

/* Python 3.14 keywords (tam liste) */
extern const char *const PY_KEYWORDS[];
extern const int PY_KEYWORDS_COUNT;

/* Python builtins */
extern const char *const PY_BUILTINS[];
extern const int PY_BUILTINS_COUNT;

/* prefix ile eslesen keyword + builtin sayisi (prefix bos ise hepsi) */
int py_syntax_count(const char *prefix);

/* i. eslesmeyi doldurur (keywords once, sonra builtins).
 * label: kelime, kind: "keyword" | "fn", detail: aciklama. */
void py_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                  char *kind, size_t kind_cap, char *detail, size_t detail_cap);

#endif /* GCL_PYTHON_SYNTAX_H */
