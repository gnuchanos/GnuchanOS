#ifndef GCL_RAYLIB_SYNTAX_H
#define GCL_RAYLIB_SYNTAX_H

#include <stddef.h>

/* raylib binding uyeleri (rl.): Lua "local rl = gcl.raylib" ve Python
 * "import pyRaylib as rl" tamamlamasi icin statik tablo.
 *
 * lua_raylib.gcDL / python_raylib.gcDL NATIVE (.gcDL) modullerdir; LSP
 * workspace taramasi .lua/.py dosyalarindan bu sembolleri CIKARAMAZ.
 * Tablo, gen_reference.py'deki LUA_RAYLIB_FUNCS / PY_RAYLIB_FUNCS ve
 * CONSTANTS listelerinin birebir karşılığıdır.
 *
 * raylib_syntax_count: prefix ile eslesen fn + const sayisi.
 * raylib_syntax_at  : idx. eslesmeyi doldurur (label/kind/detail). */
int raylib_syntax_count(const char *prefix);
void raylib_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                      char *kind, size_t kind_cap, char *detail, size_t detail_cap);

#endif /* GCL_RAYLIB_SYNTAX_H */
