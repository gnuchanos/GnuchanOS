/*
 * gcl_native_types.h — native modul struct tiplerinin alan duzeni.
 *
 * Raylib/Raygui struct'lari (Vector2, Rectangle, Color, Camera3D ...) GCL'de
 * birer DEGERE sahiptir; her iki taraf da ayni alan listesine ihtiyac duyar:
 *
 *   - tamamlama motoru (complete/complete_type.c): `v.` zincirini alanlara
 *     cozer, alan tipini bir sonraki adim icin kullanir,
 *   - yorumlayici (GCL/SimpleRunner/gcl_runner.c): `Raylib.Rectangle r;`
 *     bildirimiyle alanlari olan bir degisken yaratir; `r.x` okuma/yazma
 *     bu tablo olmadan SESSIZCE 0 donerdi (en kotu sonuc: hata yok, yanlis
 *     deger var).
 *
 * Tablo bu yuzden SharedPipeline'da durur: lexer/parser gibi cekirdek katman,
 * ne UI'ya ne de tamamlama motoruna bagimlidir.
 *
 * Alan adlari raylib basliklariyla birebir aynidir (x, y, width, height).
 * Alan tipi bir baska native struct ise ("Vector3"), yorumlayici alani
 * ozyinelemeli olarak kurar.
 */
#ifndef GCL_NATIVE_TYPES_H
#define GCL_NATIVE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Tek alan: ad + bildirilen tip. */
typedef struct {
    const char *name;
    const char *type;
} GclNativeField;

/* Tek struct tipi: ad + alan listesi. */
typedef struct {
    const char *type;                 /* "Vector2", "Camera3D" ... */
    const GclNativeField *fields;
    int field_count;
} GclNativeStruct;

/* Native bir struct tipinin alanlari (yoksa NULL). Camera ve Camera3D aynidir. */
const GclNativeStruct *gcl_native_struct(const char *type);

/* 'type' native bir struct mi? */
int gcl_native_is_struct(const char *type);

/* Tüm native struct tip adlari (NULL ile biten; "Vector2", "Camera3D", ...).
   Liste motora aittir, serbest birakilmaz. */
const char *const *gcl_native_struct_names(void);

/* "Raylib.Rectangle" -> "Rectangle". Modul oneki YALNIZCA bilinen bir modul
   adiysa soyulur; `Vector2` gibi duz yazimlar ve kullanici tipleri dokunulmaz
   kalir. `is_module` cagirana birakilir: cekirdek katman modul tablosunu
   bilmez, bu yuzden onek adayini kendisi dogrular. */
const char *gcl_native_strip_module_prefix(const char *name,
                                           int (*is_module)(const char *));

#ifdef __cplusplus
}
#endif

#endif /* GCL_NATIVE_TYPES_H */
