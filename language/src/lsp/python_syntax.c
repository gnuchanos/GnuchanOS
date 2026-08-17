/*
 * python_syntax.c — Python dilinin kendi kelimeleri (keywords + builtins).
 *
 * gcl-lsp, workspace taramasi + BUZDOLABI (gercek Python) yaninda bu
 * statik tablodan da tamamlama uretir: "impo" -> import, "pri" -> print.
 * Boylece Python'un KENDI dili de LSP'den tamamlanir.
 *
 * Derleme: makefile.py lsp_BUILD icinde gcl_lsp.c ile birlikte linklenir.
 */

#include "python_syntax.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* Python 3.14 keywords (tam liste) */
const char *const PY_KEYWORDS[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield",
};
const int PY_KEYWORDS_COUNT = (int)(sizeof PY_KEYWORDS / sizeof PY_KEYWORDS[0]);

/* Python builtins (yerlesik islevler + tipler + sabitler). */
const char *const PY_BUILTINS[] = {
    "abs", "aiter", "all", "anext", "any", "ascii", "bin", "bool",
    "breakpoint", "bytearray", "bytes", "callable", "chr", "classmethod",
    "compile", "complex", "delattr", "dict", "dir", "divmod", "enumerate",
    "eval", "exec", "filter", "float", "format", "frozenset", "getattr",
    "globals", "hasattr", "hash", "help", "hex", "id", "input", "int",
    "isinstance", "issubclass", "iter", "len", "list", "locals", "map",
    "max", "memoryview", "min", "next", "object", "oct", "open", "ord",
    "pow", "print", "property", "range", "repr", "reversed", "round",
    "set", "setattr", "slice", "sorted", "staticmethod", "str", "sum",
    "super", "tuple", "type", "vars", "zip",
    "ArithmeticError", "AssertionError", "AttributeError", "BaseException",
    "BaseExceptionGroup", "BlockingIOError", "BrokenPipeError", "BufferError",
    "BytesWarning", "ChildProcessError", "ConnectionAbortedError",
    "ConnectionError", "ConnectionRefusedError", "ConnectionResetError",
    "DeprecationWarning", "EOFError", "Ellipsis", "EncodingWarning",
    "EnvironmentError", "Exception", "ExceptionGroup", "FloatingPointError",
    "FutureWarning", "GeneratorExit", "ImportError", "ImportWarning",
    "IndentationError", "IndexError", "InterruptedError", "IsADirectoryError",
    "KeyError", "KeyboardInterrupt", "LookupError", "MemoryError", "ModuleNotFoundError",
    "NotADirectoryError", "NotImplemented", "NotImplementedError", "OSError",
    "OverflowError", "PendingDeprecationWarning", "PermissionError",
    "ProcessLookupError", "RecursionError", "ReferenceError", "ResourceWarning",
    "RuntimeError", "RuntimeWarning", "StopAsyncIteration", "StopIteration",
    "SyntaxError", "SyntaxWarning", "SystemError", "SystemExit", "TabError",
    "TimeoutError", "TypeError", "UnboundLocalError", "UnicodeDecodeError",
    "UnicodeEncodeError", "UnicodeError", "UnicodeTranslateError", "UnicodeWarning",
    "UserWarning", "ValueError", "Warning", "ZeroDivisionError",
};
const int PY_BUILTINS_COUNT = (int)(sizeof PY_BUILTINS / sizeof PY_BUILTINS[0]);

/* prefix'e gore eslesme sayisi. prefix "" ise hepsi sayilir. */
int py_syntax_count(const char *prefix) {
  const size_t plen = prefix ? strlen(prefix) : 0;
  int n = 0;
  for (int i = 0; i < PY_KEYWORDS_COUNT; i++)
    if (plen == 0 || strncmp(PY_KEYWORDS[i], prefix, plen) == 0) n++;
  for (int i = 0; i < PY_BUILTINS_COUNT; i++)
    if (plen == 0 || strncmp(PY_BUILTINS[i], prefix, plen) == 0) n++;
  return n;
}

/* i. eslesmeyi doldurur: keywords once, sonra builtins. */
void py_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                  char *kind, size_t kind_cap, char *detail, size_t detail_cap) {
  const size_t plen = prefix ? strlen(prefix) : 0;
  int k = 0;
  /* keywords */
  for (int i = 0; i < PY_KEYWORDS_COUNT; i++) {
    if (plen == 0 || strncmp(PY_KEYWORDS[i], prefix, plen) == 0) {
      if (k == idx) {
        snprintf(label, label_cap, "%s", PY_KEYWORDS[i]);
        snprintf(kind, kind_cap, "keyword");
        snprintf(detail, detail_cap, "keyword");
        return;
      }
      k++;
    }
  }
  /* builtins */
  for (int i = 0; i < PY_BUILTINS_COUNT; i++) {
    if (plen == 0 || strncmp(PY_BUILTINS[i], prefix, plen) == 0) {
      if (k == idx) {
        snprintf(label, label_cap, "%s", PY_BUILTINS[i]);
        /* buyuk harfle baslayanlar tip/sinif, digerleri islev */
        snprintf(kind, kind_cap, "%s",
                 (PY_BUILTINS[i][0] >= 'A' && PY_BUILTINS[i][0] <= 'Z')
                     ? "class"
                     : "fn");
        snprintf(detail, detail_cap, "built-in %s", PY_BUILTINS[i]);
        return;
      }
      k++;
    }
  }
  /* bulunamadi: bos */
  label[0] = 0;
  kind[0] = 0;
  detail[0] = 0;
}
