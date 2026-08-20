#ifndef GCL_GCL_SYNTAX_H
#define GCL_GCL_SYNTAX_H

/* GCL dilinin kendi kelimeleri (keywords + tipler + printf/scanf):
 * "pri" -> printf, "int" -> int32 gibi tamamlama bu tablodan gelir.
 * gcl_syntax_count: prefix ile eslesen kac kelime var.
 * gcl_syntax_at: idx ile ilgili kelimeyi doldurur (label/kind/detail). */
int gcl_syntax_count(const char *prefix);
void gcl_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                   char *kind, size_t kind_cap, char *detail, size_t detail_cap);

#endif
