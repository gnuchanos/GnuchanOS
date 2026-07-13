# C TIPLERI ve gcLang'de KULLANIMI
# gcLang / C99 / gnu99

## gcLang'de Tipler
====================
gcLang, gnu99 temelli olduğu için C tiplerini aynen kullanır.
Fark: gcLang'de tipleri `#include <stdint.h>` ile değil, kendi built-in
makrolarıyla kullanırsın. C kütüphane tipleri (ör: raylib.h'deki Color)
#extern + C_CODEGEN ile otomatik gelir.

### Temel Tipler (C'den devralınan, gcLang'de aynen)
```c
char c = 'A';
short s = -123;
int i = 42;
long l = 1000L;
long long ll = 999999LL;

unsigned char uc = 255;
unsigned short us = 123;
unsigned int ui = 42U;
unsigned long ul = 1000UL;

float f = 3.14f;
double d = 3.141592653589793;

_Bool b = 1;
void* p = 0;
```

### gcLang Tür Dönüşüm Makroları (built-in, #lib gerekmez)
```c
i8(v)   → (signed char)(v)
u8(v)   → (unsigned char)(v)
i16(v)  → (short)(v)
u16(v)  → (unsigned short)(v)
i32(v)  → (int)(v)
u32(v)  → (unsigned int)(v)
i64(v)  → (long long)(v)
u64(v)  → (unsigned long long)(v)
f32(v)  → (float)(v)
f64(v)  → (double)(v)
ch(v)   → (char)(v)
str(v)  → (char*)(v)
b8(v)   → (bool)(v)
```

---

## struct — Yapı
=================

gcLang'de struct C ile aynı:

```c
struct Point { int x; int y; };
struct Point p = {10, 20};
p.x = 30;
```

---

## raylib.h Tiplerini Direk Kullanma (ÖNEMLİ)
==============================================

raylib.h'deki tipleri gcLang'de TEKRAR YAZMAZSIN.
C_CODEGEN sayesinde raylib.h'deki struct'lar otomatik kullanılır.

Örnek — raylib.h'de şu var:
```c
typedef struct Color {
    unsigned char r, g, b, a;
} Color;
```

gcLang'de bunu şöyle kullanırsın:

### Yöntem 1 — C compound literal (C99, gcLang tarafından üretilir)
```c
// gcLang'de:
ClearBackground((colo){255, 255, 255, 255});

// C_CODEGEN çıktısı:
ClearBackground((Color){255, 255, 255, 255});
```

`colo` → raylib.h'deki `Color` struct'ının gcLang'deki kısa adıdır.
(r, g, b, a) sırasıyla — hex ezberlemek gerekmez!

### Yöntem 2 — #define ile sabit renkler
```c
// raylib.gclib içinde:
#define RAYWHITE  (Color){245, 245, 245, 255}
#define RED       (Color){230, 41, 55, 255}
#define GREEN     (Color){0, 228, 48, 255}
#define BLUE      (Color){0, 121, 241, 255}
#define BLACK     (Color){0, 0, 0, 255}
#define WHITE     (Color){255, 255, 255, 255}

// kullanım:
ClearBackground(RAYWHITE);
DrawRectangle(10, 10, 100, 50, RED);
```

Neden hex değil de (r,g,b,a)?
- `0xFFFBF0FF` okuyamazsın, hangisi hangi kanal belli değil
- `(colo){245, 245, 245, 255}` okunur, anlaşılır, düzenlenebilir
- raylib.h zaten Color struct'ını biliyor, C_CODEGEN direkt kullanır

### Yöntem 3 — İsimlendirilmiş sabitler
```c
#define RENK_KIRMIZI   (colo){255, 0, 0, 255}
#define RENK_MAVI      (colo){0, 0, 255, 255}
#define RENK_YESIL     (colo){0, 255, 0, 255}
#define RENK_SARI      (colo){255, 255, 0, 255}
```

---

## typedef — Tip Takma Adı
============================

```c
typedef unsigned long ulong;
typedef struct { int x, y; } Point;
```

---

## union — Birlik
==================

```c
union Data { int i; float f; char str[4]; };
```

---

## enum — Sıralı Sabitler
===========================

```c
enum Color { RED, GREEN, BLUE };
enum { MAX_BUF = 4096 };
```

---

## sizeof
==========

```c
sizeof(int)                    // 4
sizeof(arr) / sizeof(arr[0])   // eleman sayısı
```

---

## gcLang'de Tip Kullanımı — Özet
===================================

- `#include <stdint.h>` gerekmez → gcLang makroları var (`i32(v)` vs.)
- `#include <stdbool.h>` gerekmez → `bool`, `true`, `false` built-in
- `#include <raylib.h>` gerekmez → `#extern <raylib.web.a>` + C_CODEGEN
- raylib.h'deki struct'lar (Color, Vector2, Rectangle) aynen kullanılır
- Hex renk ezberlemek yok → `(colo){r, g, b, a}` veya isimlendirilmiş sabitler
