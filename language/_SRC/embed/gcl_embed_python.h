/*
 * gcl_embed_python.h — GCL Embed Python backend.
 *
 * simple_doc.md:
 *   Embed.Run(type="python") → Py_Initialize + değişken hazırla
 *   Embed.Stop(type="python") → Py_Finalize
 *   Embed.GetValue(type="python", value) → Python global değişkenini oku
 *   Embed.SendValue(type="python", value) → Python global değişkenine yaz
 *
 * Destek: int, float, char (ileride gcChar)
 *
 * NOT: MSVC .lib import library'leri MinGW ile uyumsuz olduğu için
 * Python C-API'si **dinamik yükleme** (LoadLibrary/dlsym) ile çözülür.
 */

#ifndef GCL_EMBED_PYTHON_H
#define GCL_EMBED_PYTHON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Python interpreter'ı başlat. Daha önce başlatılmışsa no-op. */
int gcl_py_runtime_init(void);

/* Python interpreter'ı bitir. Çalışmıyorsa no-op. */
void gcl_py_runtime_shutdown(void);

/* Çalışıyor mu? */
int gcl_py_runtime_is_active(void);

/* Python'da <name> global değişkenine değer yaz.
   value: "42", "3.14", "'merhaba'" ... — tam ifade olarak.
   Başarılıysa 1, değilse 0. */
int gcl_py_set_global(const char *name, const char *value);

/* Python'da <name> global değişkeninin değerini oku.
   out: string olarak (sayı ise "42" / "3.14", string ise tırnaksız içerik).
   Başarılıysa 1, değilse 0. */
int gcl_py_get_global(const char *name, char *out, size_t out_size);

/* Python kod string çalıştır (PyRun_SimpleString wrapper). */
int gcl_py_run_simple(const char *code);

/* Python dosya çalıştır — sys.path'e script dizinini + embed library dizinini ekler,
   raylib.pyd/raygui.pyd bağımlılıklarını çözer. Embed.dll'den export edilir.
   döner: 0=başarı, 1=hata. */
int gcl_py_run_file(const char *path);

/* Python sys.path'e dizin ekle (gömülü .pyd modüllerini bulmak için). */
int gcl_py_add_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* GCL_EMBED_PYTHON_H */
