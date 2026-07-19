# GCL — Gnuchan C-Like Language (GNU99)

# Gnuchan Language System — Before Start

- **GNU99 tabanlı** — C99 standartı + GNU extensions
- **Güvenli C** — runtime safety eklenmiş, debug her zaman net
- **Sıfır maliyet** — debug kapalıyken hiçbir overhead yok

#### FUTURES
Fast Smart LSP
UTF-8 SUPPORT


# ==========================================================
# COMPILER PIPELINE
# ==========================================================

                    [Source Code]
                          │
                          ▼
                      [Lexer]
                          │
                          ▼
                      [Parser]
                          │
                          ▼
                        [AST]
                          │
                          ▼
                [Semantic Analysis]
                          │
                          ▼
                     [Typed AST]
                          │
                          ▼
                         [IR]
                          │
                          ▼
                 [IR Optimization]
                          │
            ┌─────────────┴─────────────┐
            ▼                           ▼
      [JIT]                      [Code Generation]
                                        │
                                        ▼
                                    [Assembler]
                                        │
                                        ▼
                                    [Object File]
                                        │
                                        ▼
                                     [Linker]
                                        │
                                        ▼
                                   [Executable]


# ==========================================================
# MEMORY ALLOCATION
# ==========================================================

                      area(size)
                          │
                          ▼
                 size <= 64 Bytes ?
                 ├──────────────────┐
                 │ Yes              │ No
                 ▼                  ▼
          [Pool Allocator]     size <= 64 KB ?
                                    ├───────────────┐
                                    │ Yes           │ No
                                    ▼               ▼
                             [Free List]      [OS Page Allocator]
                                    │               │
                                    └───────┬───────┘
                                            ▼
                                Allocate Memory Block
                                            │
                                            ▼
                              Write Memory Header
                                            │
                                            ▼
                                  Return Pointer


# ==========================================================
# TEMPORARY MEMORY
# ==========================================================

                   arena.alloc(size)
                          │
                          ▼
                  [Arena Allocator]
                          │
                          ▼
                   Increase Offset
                          │
                          ▼
                    Return Pointer

                     arena.reset()
                          │
                          ▼
              Free Whole Arena At Once


# ==========================================================
# FREE
# ==========================================================

                      free(ptr)
                          │
                          ▼
                  Validate Header
                          │
                          ▼
                Detect Double Free
                          │
          ┌───────────────┴────────────────┐
          ▼                                ▼
      Invalid Pointer                 Valid Pointer
          │                                │
          ▼                                ▼
    Runtime Error               Read Allocator Type
                                           │
        ┌──────────────┬──────────────┬──────────────┐
        ▼              ▼              ▼              ▼
     Pool         Free List        Arena         OS Page
        │              │              │              │
        ▼              ▼              ▼              ▼
  Return To Pool   Merge Blocks   Ignore/Reset   Release Pages
        │              │              │              │
        └──────────────┴──────────────┴──────────────┘
                          │
                          ▼
                         Done


# ==========================================================
# MEMORY HEADER
# ==========================================================

struct MemoryHeader
{
    size_t   size;
    uint16_t allocator;    // Pool | Arena | FreeList | OS
    uint16_t flags;
    uint32_t checksum;
}


# ==========================================================
# PROGRAM EXIT
# ==========================================================

             Program Starts
                    │
                    ▼
      Create Global Allocation Table
                    │
                    ▼
             area(...) Called
                    │
                    ▼
        Save Pointer To Allocation Table
                    │
                    ▼
              Program Terminates
                    │
                    ▼
      Free All Remaining Allocations
                    │
                    ▼
              Return Memory To OS

# ==========================================================
# CLANG / RUST STYLE ERROR HANDLING
# ==========================================================

## Mevcut Durum (v0.1)
- Parser'da `error(E001, line, col, msg)` fonksiyonu
- Çıktı formatı:
  ```
  error[E001]: expected number after '='
   --> test.gcsf:1:8
    |
  1 | int a = ;
    |        ^ expected number after '='
  ```
- Hata kodu: `E001` (tekil, genel)
- Kaynak satırı: caret ile işaretleniyor
- Lokasyon: dosya:satır:kolon doğru

## Plan — v0.2 Hata Sistemi

### 1. Hata Kodları (her hata türüne özel kod)
```
E000 — internal compiler error
E001 — syntax error (general)
E002 — expected identifier
E003 — expected ';'
E004 — expected expression
E005 — invalid token / unexpected character
E010 — type error (general)
E011 — undeclared variable
E012 — redeclaration
E013 — type mismatch
E020 — memory error (runtime)
E021 — double free
E022 — null pointer dereference
E023 — out of bounds
E030 — I/O error
E031 — file not found
```

### 2. Her Stage Kendi Error Modülü
- `src/error/` — merkezi error dispatcher
- Her pipeline stage hata bildirimini tek bir API üzerinden yapar
- Stage adı otomatik eklenir: `error[E001] (lexer)`, `error[E003] (parser)`

### 3. Ekstra Hata Bilgileri
```
error[E001]: expected ';' after declaration
 --> test.gcsf:2:14
  |
2 | int a = 10
  |           ^ help: add ';' here
```
- `help:` öneri satırı (opsiyonel)
- `note:` ek bilgi satırı (opsiyonel)
- Color coding: kırmızı `error`, sarı `warning`, mavi `note`, yeşil `help`

### 4. Warning Sistemi
- Parser'da `warning(Wxxx)` fonksiyonu
- Aynı format ama sarı renk
- `-Werror` flag ile warning'ler error'a çevrilir
- Warning'ler varsayılan olarak stderr'a yazılır, çıktı üretilir

### 5. Error Recovery Geliştirmeleri
- Şu an skip-to-semicolon var
- v0.2: skip-to-next-declaration (token pattern matching)
- v0.3: bracket matching recovery (nested bloklarda)

### 6. JSON / Makine Formatı
- `-ferror-format=json` flag ile JSON çıktı
- IDE/tool entegrasyonu için
```json
{
  "level": "error",
  "code": "E001",
  "file": "test.gcsf",
  "line": 1,
  "col": 8,
  "message": "expected ';'",
  "suggestion": "add ';' here"
}
```

### 7. Uygulama Sırası
1. Hata kod tablosu → `src/include/errors.h`
2. Merkezi error API → `src/error/` modülü
3. Renk/format ayarları → config'e bağla
4. Warning sistemi
5. `help:` / `note:` satırları
6. JSON formatı (opsiyonel)

### 8. Mevcut Koda Etkisi
- `parser.c`'deki `error()` → `error_report(E001, loc, "msg")` olur
- Aynı satır gösterimi kalır, üstüne `help:` eklenebilir
- Lexer hataları da aynı API'yi kullanır (`E005`)

# ==========================================================
## File Types
# ==========================================================

| Extension  | Type                    | Description                      |
|------------|-------------------------|----------------------------------|
| `.gcsf`    | Gnuchan Script File     | Kaynak kod (source)              |
| `.gclib`   | Gnuchan Library         | Kütüphane modülü                 |
| `.gctf`    | Gnuchan Text File       | Doküman / data                   |
| `.gcdata`  | Gnuchan Config          | JSON-style proje ayarları        |

# ==========================================================
## Import / Link Direktifleri
# ==========================================================

```
#extern <file.dll>      # .dll, .so, .a — external C library
#lib    <file>          # .gclib textfile
#include <file>         # .gcsf source file
```

---

# ==========================================================
## CLI Parameters
# ==========================================================

```
# how flag and vlaue works
gcl -flag value or gcl -flag value value value

gcl                     → interactive shell
gcl -version, -v        → version info
gcl -help, -h           → help

# Path / Library
gcl -linclude path      → extra include path
gcl -llib path          → extra library path
gcl -lextend path       → extension module path

# Debug / Pipeline
gcl -lexer file.gcsf    → token stream
gcl -parser file.gcsf   → parse tree
gcl -ast file.gcsf      → AST dump
gcl -ir file.gcsf       → IR dump
gcl -codegen file.gcsf  → generated C code
gcl -istedigin_kadar_flag_girebilirsin file.gcsf -o out → full pipeline to executable


gcl -debug

gcl -istedigin_kadar_flag_girebilirsin file.gcsf -o path/output_name
gcl -istedigin_kadar_flag_girebilirsin -run file.gcsf



---

# ==========================================================
## comment
# ==========================================================
c comment
// 
/* .... */ 

# gcl comment
#| ... |#
#//


---

###### 101 — Types #################################################

```
// ── Primitive Types ──────────────────────────────────────────
# INT
```
X  int a = 5;              // basit tamsayi (MEVCUT)
O  int a = -5;             // negatif sayi
O  int a = +5;             // pozitif isareti
O  int a = 'A';            // karakter
O  int a = '\n';           // kacis karakteri
O  int a = 0xFF;           // hexadecimal
O  int a = 077;            // octal
O  int a = 0b1010;         // binary (GNU extension)
O  int a = b;              // degisken atama
O  int a = func();         // fonksiyon cagrisi
O  int a = x + y;          // toplama
O  int a = x - y;          // cikarma
O  int a = x * y;          // carpma
O  int a = x / y;          // bolme
O  int a = x % y;          // mod alma
O  int a = x & y;          // bitwise AND
O  int a = x | y;          // bitwise OR
O  int a = x ^ y;          // bitwise XOR
O  int a = ~x;             // bitwise NOT
O  int a = x << 2;         // sola kaydirma
O  int a = x >> 2;         // saga kaydirma
O  int a = x > y;          // buyuktur
O  int a = x < y;          // kucuktur
O  int a = x >= y;         // buyuk esit
O  int a = x <= y;         // kucuk esit
O  int a = x == y;         // esitlik
O  int a = x != y;         // esit degil
O  int a = x && y;         // mantiksal AND
O  int a = x || y;         // mantiksal OR
O  int a = !x;             // mantiksal NOT
O  int a = x ? y : z;      // ternary
O  int a = ++x;            // pre-increment
O  int a = --x;            // pre-decrement
O  int a = x++;            // post-increment
O  int a = x--;            // post-decrement
O  int a = (int)3.14;      // cast
O  int a = sizeof(int);    // sizeof
O  int a = (x = 5);        // atama ifadesi icinde
O  int a = (x += 5);       // bilesik atama
O  int a = (x -= 5);
O  int a = (x *= 5);
O  int a = (x /= 5);
O  int a = (x %= 5);
O  int a = (x &= 5);
O  int a = (x |= 5);
O  int a = (x ^= 5);
O  int a = (x <<= 1);
O  int a = (x >>= 1);
O  int a = (1, 2, 3);      // virgul operatoru
```

char               // 1 byte
short              // 2 byte
long               // 4/8 byte (platform)
long long          // 8 byte
float              // 4 byte
double             // 8 byte
long double        // 10/16 byte (platform)

void               // no type

// ── GCL-Specific Fixed-Width ────────────────────────────────
int8               // signed 8-bit
int16              // signed 16-bit
int32              // signed 32-bit
int64              // signed 64-bit
int128             // signed 128-bit

// ── Unsigned ────────────────────────────────────────────────
unsigned char
unsigned short
unsigned int
unsigned long
unsigned long long

uint8              // unsigned 8-bit
uint16             // unsigned 16-bit
uint32             // unsigned 32-bit
uint64             // unsigned 64-bit

// ── GCL-Specific Unsigned Float/Double ─────────────────────
// Behaves like standard float/double but signals on NaN/Inf
unsigned float     // float with overflow guard
unsigned double    // double with overflow guard

// ── Boolean ────────────────────────────────────────────────
bool               // stdbool, values: true / false
```

###### 101 — Types #################################################







==========================================================
## networking -> built-in networking library
# ==========================================================

==========================================================
## http request -> https client functionality
# ==========================================================

==========================================================
## json support -> native json parsing/serialization
# ==========================================================

==========================================================
## modern I/O async I/O operations
# ==========================================================

==========================================================
## unicode support full unicode text handling
# ==========================================================

==========================================================
## CLI arguments command line argument parsin
# ==========================================================
