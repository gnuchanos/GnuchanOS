/*
 * gcl_embed_lua.h — GCL Embed Lua backend.
 *
 * simple_doc.md:
 *   Embed.Run(type="lua") → luaL_newstate + açık kütüphaneler
 *   Embed.Stop(type="lua") → lua_close
 *   Embed.GetValue(type="lua", value) → Lua global değişkenini oku
 *   Embed.SendValue(type="lua", value) → Lua global değişkenine yaz
 *
 * Destek: int, float, char (ileride gcChar)
 */

#ifndef GCL_EMBED_LUA_H
#define GCL_EMBED_LUA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lua state başlat. Daha önce başlatılmışsa no-op. */
int gcl_lua_runtime_init(void);

/* Lua state kapat. Çalışmıyorsa no-op. */
void gcl_lua_runtime_shutdown(void);

/* Çalışıyor mu? */
int gcl_lua_runtime_is_active(void);

/* Lua'da <name> global değişkenine değer yaz.
   value: "42", "3.14", "'merhaba'" ... — tam ifade olarak (kaynak string). */
int gcl_lua_set_global(const char *name, const char *value);

/* Lua'da <name> global değişkeninin değerini oku.
   out: string olarak (sayı ise "42" / "3.14", string ise içerik). */
int gcl_lua_get_global(const char *name, char *out, size_t out_size);

/* Lua kod string çalıştır (luaL_dostring wrapper). */
int gcl_lua_run_simple(const char *code);

/* Lua dosya çalıştır — LuaRaylib/LuaRaygui binding'lerini yükler + gcl table kurulumu.
   Embed.dll'den export edilir. döner: 0=başarı, 1=hata. */
int gcl_lua_run_file(const char *path);

/* Aktif lua_State döndür (binding kaydı için). Yoksa NULL. */
void *gcl_lua_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* GCL_EMBED_LUA_H */
