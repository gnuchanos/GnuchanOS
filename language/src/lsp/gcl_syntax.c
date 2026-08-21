/* gcl_syntax.c — GCL dilinin kendi kelimeleri (keywords + tipler +
 * printf/scanf imzalari). gcl_doc.md'deki gercek GCL sozdizimi esas alinir.
 * prefix: "pri" -> printf, "int" -> int32 gibi filtreleme. */
#include <stdio.h>
#include <string.h>

#include "gcl_syntax.h"

typedef struct {
  const char *label;
  const char *kind;
  const char *detail;
} GclEntry;

static const GclEntry g_gcl[] = {
    /* keywords */
    {"if", "keyword", "if (...) { ... } else if (...) { ... }"},
    {"else", "keyword", "else { ... }"},
    {"elif", "keyword", "elif () { ... }"},
    {"while", "keyword", "while (...) { ... }"},
    {"do", "keyword", "do { ... } while(...)"},
    {"for", "keyword", "for (int i = 0; i < n; i++) { ... }"},
    {"switch", "keyword", "switch () { case: break; default: break; }"},
    {"case", "keyword", "case:"},
    {"break", "keyword", "break;"},
    {"continue", "keyword", "continue;"},
    {"return", "keyword", "return value;"},
    {"public", "keyword", "public type identifier = value;"},
    {"private", "keyword", "private type identifier = value;"},
    {"const", "keyword", "const type identifier = value;"},
    {"inline", "keyword", "inline identifier;"},
    {"global", "keyword", "global identifier;"},
    {"typedef", "keyword", "typedef ... ;"},
    {"sizeof", "fn", "sizeof(type) or sizeof(variable)"},
    {"struct", "keyword", "struct Name { ... };"},
    {"class", "keyword", "class Name() { void m() {} }"},
    {"enum", "keyword", "enum Name { A, B, C };"},
    {"tuple", "keyword", "tuple name = ('a', 10, 300.0);"},
    {"dict", "keyword", "dict name = { gcChar key : value };"},
    {"#include", "keyword", "#include \"script.gcsf\" or <script.gcsf>"},
    {"#extern", "keyword", "#extern \"raylib.dll\" or <raylib.so>"},
    {"#register", "keyword", "#register void InitWindow(int w, int h, const char *t);"},
    {"#define", "keyword", "#define NAME value"},
    {"#undef", "keyword", "#undef NAME"},
    {"#warning", "keyword", "#warning \"text\""},
    {"#error", "keyword", "#error \"ERROR!!\""},
    {"#debug", "keyword", "#debug \"message\""},
    {"#ifdef", "keyword", "#ifdef NAME"},
    {"#ifndef", "keyword", "#ifndef NAME"},
    {"#if", "keyword", "#if windows"},
    {"#elif", "keyword", "#elif linux"},
    {"#endif", "keyword", "#endif"},

    /* GCL tipleri */
    {"int8", "type", "int8"},
    {"int16", "type", "int16"},
    {"int32", "type", "int32"},
    {"int64", "type", "int64"},
    {"int128", "type", "int128"},
    {"uint8", "type", "uint8"},
    {"uint16", "type", "uint16"},
    {"uint32", "type", "uint32"},
    {"uint64", "type", "uint64"},
    {"uint128", "type", "uint128"},
    {"float16", "type", "float16"},
    {"float32", "type", "float32"},
    {"float64", "type", "float64"},
    {"float128", "type", "float128"},
    {"gcChar", "type", "utf-8 char/string"},
    {"bool", "type", "bool"},
    {"char", "type", "vanilla 1 byte char"},
    {"short", "type", "short"},
    {"int", "type", "int"},
    {"float", "type", "float"},
    {"double", "type", "double"},
    {"long", "type", "long"},
    {"unsigned", "type", "unsigned"},
    {"void", "type", "void"},

    /* printf / scanf */
    {"printf", "fn", "printf(\"format\", args...)"},
    {"scanf", "fn", "scanf(\"%type\", text)"},
    {"malloc", "fn", "malloc(reserve=count, extra=n)"},
    {"free", "fn", "free(ptr);"},
    {"gcMalloc", "fn", "gcMalloc(reserve=count, extra=n)"},
};

static int prefix_match_gcl(const char *label, const char *prefix) {
  size_t n = strlen(prefix);
  return strncmp(label, prefix, n) == 0;
}

int gcl_syntax_count(const char *prefix) {
  int n = 0;
  size_t i;
  for (i = 0; i < sizeof g_gcl / sizeof g_gcl[0]; i++) {
    if (prefix_match_gcl(g_gcl[i].label, prefix ? prefix : "")) n++;
  }
  return n;
}

void gcl_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                   char *kind, size_t kind_cap, char *detail, size_t detail_cap) {
  int n = 0;
  size_t i;
  if (label_cap > 0) label[0] = '\0';
  if (kind_cap > 0) kind[0] = '\0';
  if (detail_cap > 0) detail[0] = '\0';
  for (i = 0; i < sizeof g_gcl / sizeof g_gcl[0]; i++) {
    if (prefix_match_gcl(g_gcl[i].label, prefix ? prefix : "")) {
      if (n == idx) {
        if (label_cap > 0) snprintf(label, label_cap, "%s", g_gcl[i].label);
        if (kind_cap > 0) snprintf(kind, kind_cap, "%s", g_gcl[i].kind);
        if (detail_cap > 0) snprintf(detail, detail_cap, "%s", g_gcl[i].detail);
        return;
      }
      n++;
    }
  }
}
