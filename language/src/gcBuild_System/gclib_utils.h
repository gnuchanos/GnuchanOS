/*
 * gclib_utils.h — .gcLib / .gcDL modul sorgulama aracı
 *
 *   gcl -libs                      -> Library icindeki tum modulleri listele
 *   gcl -libcheck <modul>          -> modul var mi / yok mu (exit 0/1)
 *   gcl -lib <modul> -luarun ...   -> modul VARSA istenen calismayi yap
 */

#ifndef GCLIB_UTILS_H
#define GCLIB_UTILS_H

#include <stddef.h>

/* exe'nin YANINDAKI Library dizininin tam yolu (out/cap). NULL -> hata. */
const char *gclib_library_dir(char *out, size_t cap);

/* Modul adini (lua, lua_raylib, raylib, python, python_raylib veya
 * LuaModules/PythonModules icinde <name>.gcDL) tam yola cevirir.
 * 1 = bulundu (out'a yol yazilir), 0 = yok. */
int gclib_find_module(const char *name, char *out, size_t cap);

/* Library altindaki tum modulleri listeler. 0 basarili, -1 hata. */
int gclib_list_all(char *err, size_t err_cap);

#endif /* GCLIB_UTILS_H */
