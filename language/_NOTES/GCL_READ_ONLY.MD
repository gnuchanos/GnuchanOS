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
      [JIT Compiler]              [Code Generation]
            │                           │
            ▼                           ▼
   [Machine Code (RAM)]           [Assembler]
            │                           │
            ▼                           ▼
     [Execute Immediately]       [Object File]
                                        │
                                        ▼
                                     [Linker]
                                        │
                                        ▼
                                   [Executable]
                                        │
                                        ▼
                                    [Run Later]


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
    extern "c" {}
    extern type func
    direk cagirma
#lib    <file>          # .gclib textfile
    file. python class gibi cagirma
    custome modules
    ready variable
    extern
#include <file>         # .gcsf source file
    .c+.h birlesimi include sistemi direk cagirma

```

---

# ==========================================================
## CLI Parameters
# ==========================================================

```

# burada olmayan seyleri dile ekleme

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

###### HEADERS #################################################

#include <script> or "script" --> .gcsf
#lib     <library> or "library" --> .gclib
#extern  <raylib.dll> or "raylib.dll"

# yorum 
#| yorumlar
devam 
ediyorlar
|#

#include <stdio.h>

#define MAX 100
#undef MAX

#ifdef DEBUG
    printf("Debug");
#endif

#ifndef DEBUG
    #define DEBUG
#endif

#if VERSION == 1
    #define TEXT "v1"
#elif VERSION == 2
    #define TEXT "v2"
#else
    #error VERSION tanımlı değil veya geçersiz.
#endif

#pragma message("Derleniyor...")

#line 200 "example.gcsf"
###### HEADERS #################################################


###### MAIN ENTRY #################################################
int main(int argc, char *argv[]) {

    return 0;
}

int main(int argc, char **argv) {

    return 0;
}

int main() {

    return 0;
}

#// her .gcsf icinde main olmasina gerek yok sadece
gcc -run file.gcsf burada olmasi yeterli  main
gcc file.gcsf (burada olsun yeter main) -o path/output
###### MAIN ENTRY #################################################


###### comment #################################################
c comment
// 
/* .... */ 

# gcl comment
#| ... |#
#//
###### comment #################################################


---

###### 101 — Types #################################################

```
// ── Primitive Types ──────────────────────────────────────────
# INT
```
int a = 5;              // Tam sayı sabiti
int a = -5;             // Negatif tam sayı
int a = +5;             // Pozitif işaretli tam sayı
int a = 'A';            // Karakter sabiti ('A' ASCII/Unicode kodu)
int a = '\n';           // Kaçış karakteri (yeni satır)
int a = 0xFF;           // Hexadecimal (16 tabanlı) sayı
int a = 077;            // Octal (8 tabanlı) sayı
int a = 0b1010;         // Binary (2 tabanlı) sayı (GNU Extension / C23)

int a = b;              // Başka bir değişkenin değerini kopyala
int a = func();         // Fonksiyonun döndürdüğü değeri ata

int a = x + y;          // Toplama
int a = x - y;          // Çıkarma
int a = x * y;          // Çarpma
int a = x / y;          // Bölme
int a = x % y;          // Mod (kalan)

int a = x & y;          // Bit düzeyinde AND
int a = x | y;          // Bit düzeyinde OR
int a = x ^ y;          // Bit düzeyinde XOR
int a = ~x;             // Bit düzeyinde NOT (tüm bitleri ters çevir)

int a = x << 2;         // Bitleri sola kaydır
int a = x >> 2;         // Bitleri sağa kaydır

int a = x > y;          // x, y'den büyük mü? (1 veya 0)
int a = x < y;          // x, y'den küçük mü? (1 veya 0)
int a = x >= y;         // x, y'den büyük veya eşit mi? (1 veya 0)
int a = x <= y;         // x, y'den küçük veya eşit mi? (1 veya 0)
int a = x == y;         // x ve y eşit mi? (1 veya 0)
int a = x != y;         // x ve y farklı mı? (1 veya 0)

int a = x && y;         // Mantıksal AND (ikisi de true ise 1)
int a = x || y;         // Mantıksal OR (biri true ise 1)
int a = !x;             // Mantıksal NOT (true ↔ false)

int a = x ? y : z;      // Koşul doğruysa y, yanlışsa z

int a = ++x;            // Önce artır, sonra değeri kullan
int a = --x;            // Önce azalt, sonra değeri kullan
int a = x++;            // Önce değeri kullan, sonra artır
int a = x--;            // Önce değeri kullan, sonra azalt

int a = (int)3.14;      // Tür dönüşümü (cast)
int a = sizeof(int);    // Türün boyutu (byte cinsinden)

int a = x = 5;          // Önce x'e 5 ata, sonra sonucu a'ya ata

int a = x += 5;         // x = x + 5; sonucunu a'ya ata
int a = x -= 5;         // x = x - 5; sonucunu a'ya ata
int a = x *= 5;         // x = x * 5; sonucunu a'ya ata
int a = x /= 5;         // x = x / 5; sonucunu a'ya ata
int a = x %= 5;         // x = x % 5; sonucunu a'ya ata

int a = x &= 5;         // x = x & 5; sonucunu a'ya ata
int a = x |= 5;         // x = x | 5; sonucunu a'ya ata
int a = x ^= 5;         // x = x ^ 5; sonucunu a'ya ata

int a = x <<= 1;        // x = x << 1; sonucunu a'ya ata
int a = x >>= 1;        // x = x >> 1; sonucunu a'ya ata

```

// ───────────────── Primitive Types ─────────────────────────

char               // Unicode codepoint (32-bit)
short              // 16-bit signed integer
long               // 32-bit or 64-bit signed integer (platform dependent)
long long          // 64-bit signed integer
float              // 32-bit IEEE 754 floating point
double             // 64-bit IEEE 754 floating point
long double        // Extended precision floating point (80-bit / 128-bit)
void               // No value / no type

// ───────────────── Fixed-Width Signed Integers ─────────────

int8               // Signed 8-bit integer
int16              // Signed 16-bit integer
int32              // Signed 32-bit integer
int64              // Signed 64-bit integer
int128             // Signed 128-bit integer

// ───────────────── Unsigned Integers ───────────────────────

unsigned char      // Unsigned Unicode codepoint
unsigned short     // Unsigned 16-bit integer
unsigned int       // Unsigned integer
unsigned long      // Unsigned long integer
unsigned long long // Unsigned 64-bit integer

uint8              // Unsigned 8-bit integer
uint16             // Unsigned 16-bit integer
uint32             // Unsigned 32-bit integer
uint64             // Unsigned 64-bit integer

// ───────────────── Unsigned Floating Types ─────────────────

unsigned float     // Float with NaN / Infinity runtime guard
unsigned double    // Double with NaN / Infinity runtime guard

// ───────────────── Boolean ─────────────────────────────────

bool               // Boolean type (true / false)


// ───────────────── Character & String Types ────────────────

char               // Single Unicode codepoint (char32)
char[]             // UTF-8 string stored directly in an array
char*              // Raw byte pointer (not a string type)


// ───────────────── Examples ────────────────────────────────

// String stored on the stack (modifiable)
char name[] = "Gordon Freeman";

// Pointer to a string literal (read-only memory)
char *game = "HALF LIFE 3";

// Fixed-size 2D string array (32 strings × 32 characters)
char stack_string_array[32][32] = {"a", "abc"};

// Compiler automatically determines the number of rows
char stack_string_array[][32] = {"a", "abc"};

// INVALID: Last array dimension cannot be omitted in C
// char stack_string_array[32][] = {"a", "abc"};

// Array of pointers to string literals
char *string_array[] = {"a", "ABC"};

// Dynamically allocated array of string pointers
char **ready_now = malloc(2 * sizeof(*ready_now));

ready_now[0] = "Half Life 3";
ready_now[1] = "Left 4 Dead 3";

// ───────────────── typedef ─────────────────

// Mevcut bir tipe yeni isim (alias) verir.
typedef unsigned int uint;


// ───────────────── struct ─────────────────

// Yeni bir yapı tipi tanımlar.
struct Player {
    int hp;
    int armor;
};


// ───────────────── enum ───────────────────

// İsimlendirilmiş sabitler tanımlar.
enum Weapon {
    PISTOL,
    SHOTGUN,
    RPG
};

// ───────────────── union ───────────────────

// Aynı belleği paylaşan üyelerden oluşan yeni bir tip tanımlar.
union Value {
    int i;
    float f;
    char *str;
};

// ───────────────── typedef struct ─────────

// struct anahtar kelimesini kullanmadan erişmeyi sağlar.
typedef struct Player {
    int hp;
    int armor;
} Player;


// ───────────────── typedef enum ───────────

// enum anahtar kelimesini kullanmadan erişmeyi sağlar.
typedef enum Weapon {
    PISTOL,
    SHOTGUN,
    RPG
} Weapon;

// ───────────────── typedef union ───────────

// union anahtar kelimesini kaldırır.
typedef union Value {
    int i;
    float f;
    char *str;
} Value;

// ─── MULTI VALUE SIGNE ───────────────────────────────────────────
type variable0, variable1, variable2 = value;
type variable0, variable1, variable2 = value0, value1, value2;

###### 101 — Types ##############################################

###### PRINTF — Types ##############################################

int printf(const char *format, ...);

/*
FORMAT:

%[flags][width][.precision][length]specifier

FLAGS:
-    Left align
+    Always show sign
' '  Leading space for positive
#    Alternate form
0    Zero padding

WIDTH:
<number>
*

PRECISION:
.<number>
.*

LENGTH:
hh
h
l
ll
j
z
t
L

SPECIFIERS:
%d  signed int
%i  signed int
%u  unsigned int
%o  unsigned octal
%x  hexadecimal (lowercase)
%X  hexadecimal (uppercase)
%f  double
%F  double
%e  scientific
%E  scientific
%g  shortest float
%G  shortest float
%a  hexadecimal float
%A  hexadecimal float
%c  char
%s  string
%p  pointer
%n  write character count
%%  percent sign
*/

###### PRINTF — Types ##############################################


###### function #################################################

// Function declaration
int add(int a, int b);

// Function definition
int add(int a, int b) {
    return a + b;
}

// No parameters
void hello(void) {
}

// No return value
void print_number(int value) {
}

// Return primitive type
int get_score(void) {
    return 100;
}

// Return pointer
char *get_name(void) {
    return "Gordon";
}

// Return struct
Player create_player(void) {
    Player p = {100, 50};
    return p;
}

// Function pointer
int (*operation)(int, int);

// Function pointer declaration
typedef int (*MathFunc)(int, int);

MathFunc func = add;

// Variadic function
int printf(const char *fmt, ...);

###### function #################################################


###### if / else / else if ######################################

if (x > 0) {
}

if (x > 0) {
}
else {
}

if (x > 0) {
}
else if (x == 0) {
}
else {
}

###### if / else / else if ######################################


###### switch / case ############################################

switch (value) {

case 0:
    break;
case 1:
    break;
case 2:
    break;
case 3:
    break;
default:
    break;
}

###### switch / case ############################################

###### memory (malloc / calloc / realloc / free) #################

// Allocate memory
int *ptr = malloc(10 * sizeof(int));

// Allocate zero-initialized memory
int *ptr = calloc(10, sizeof(int));

// Resize allocated memory
ptr = realloc(ptr, 20 * sizeof(int));

// Release memory
free(ptr);

// Allocate a single object
Player *player = malloc(sizeof(Player));

// Allocate an array of objects
Player *players = malloc(100 * sizeof(Player));

###### memory (malloc / calloc / realloc / free) #################


###### for #######################################################

// Classic for loop
for (int i = 0; i < 10; i++) {
}

// Multiple variables
for (int i = 0, j = 9; i < j; i++, j--) {
}

// Infinite loop
for (;;) {
}

// Empty body
for (int i = 0; i < 10; i++);

###### for #######################################################


###### while #####################################################

// Basic while loop
while (running) {
}

// Infinite loop
while (1) {
}

// Empty body
while (condition);

###### while #####################################################


###### do / while ################################################

// Execute at least once
do {
} while (running);

// Empty body
do {
} while (condition);

###### do / while ################################################


##### GELICEKDE SUANDA GEREK YOK
==========================================================
## networking -> built-in networking library
# ==========================================================
--- daha degil
==========================================================
## http request -> https client functionality
# ==========================================================
--- daha degil
==========================================================
## json support -> native json parsing/serialization
# ==========================================================
--- daha degil
==========================================================
## modern I/O async I/O operations
# ==========================================================
--- daha degil
==========================================================
## unicode support full unicode text handling
# ==========================================================
--- daha degil
==========================================================
## CLI arguments command line argument parsin
# ==========================================================
--- daha degil