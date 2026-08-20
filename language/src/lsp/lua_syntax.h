#ifndef GCL_LUA_SYNTAX_H
#define GCL_LUA_SYNTAX_H

/* Lua dilinin kendi kelimeleri (keywords + globals):
 * "pri" -> print, "ta" -> table gibi tamamlama bu tablodan gelir.
 * lua_syntax_count: prefix ile eslesen kac kelime var.
 * lua_syntax_at: idx ile ilgili kelimeyi doldurur (label/kind/detail). */
int lua_syntax_count(const char *prefix);
void lua_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                   char *kind, size_t kind_cap, char *detail, size_t detail_cap);

#endif
