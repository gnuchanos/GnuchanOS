<div align="center">
  <img src="./assets/logo.png" alt="Gnuchanos" width="120" />
</div>

<div align="center"><strong>GnuchanOS</strong> — kişisel GNU/Linux tabanlı işletim sistemi denemesi (rolling release).</div>

<div align="center">Bu dil, Guix dağıtımında yazılım geliştirmek için kullanılacak.</div>

<hr>

# GCL — Gnuchan C-Like Language (GNU99)

# Gnuchan Language System — Before Start

- **Güvenli C** — runtime safety eklenmiş, debug her zaman net
- **Sıfır maliyet** — debug kapalıyken hiçbir overhead yok

---

## CLI Parameters

```bash
gcl -flag value            # veya
gcl -flag value value value
```

| Komut                     | Açıklama                              |
|---------------------------|---------------------------------------|
| `gcl`                     | Interactive shell                     |
| `gcl -version`, `-v`      | Version info                          |
| `gcl -help`, `-h`         | Help                                  |

### Path / Library

| Flag              | Açıklama                 |
|-------------------|--------------------------|
| `-linclude path`  | Extra include path       |
| `-llib path`      | Extra library path       |
| `-lextend path`   | Extension module path    |

### Debug / Pipeline

| Flag                    | Açıklama           |
|-------------------------|--------------------|
| `-lexer file.gcsf`      | Token stream       |
| `-parser file.gcsf`     | Parse tree         |
| `-ast file.gcsf`        | AST dump           |
| `-ir file.gcsf`         | IR dump            |
| `-codegen file.gcsf`    | C code dump        |
| `-debug`                | Debug mode         |

### Export

```bash
# Full pipeline → executable (istediğin kadar flag girebilirsin)
gcl -istedigin_kadar_flag_girebilirsin file.gcsf -o out

# Manuel path
gcl -linclude path -llib path -lextend path .\main.gcsf -o out_test

# Same folder search
gcl.exe .\main.gcsf -o out_test
```

---

## Compiler Pipeline

```
         #extern Resolution
                │
        ┌───────┴────────┐
        ▼                ▼
      .gclib          .gcsf
        │                │
        ├── #extern ─────┤
        │                │
        ▼                ▼
   Resolve Recursively
                │
                ▼
      Merged Source File
                │
                ▼
       Compiler Frontend
  (Lexer → Parser → AST → Semantic)
                │
                ▼
         Code Generation
                │
                ▼
            output.c
                │
                ▼
        GCC Compilation
                │
                ▼
            output.o
                │
                ▼
             Linker
                │
      ┌─────────┴─────────┐
      ▼                   ▼
 output.exe        output.gcdebug
```

### Dosya Formatları

| Format     | Amaç                                                 |
|------------|------------------------------------------------------|
| `.gcsf`    | Ana kaynak dosyası                                   |
| `.gclib`   | Kütüphane (#extern DLL, extern "c" blokları)         |
| `.gcdebug` | Export debug dump (IR + defines + externs + errors)  |

---

## Syntax

### Headers

```gcsf
#include <script> or "script"    # → .gcsf
#lib     <library> or "library"  # → .gclib
#extern  <raylib.dll> or "raylib.dll"
```

### Comments

```gcsf
#| yorumlar
devam
ediyorlar
|#

// c yorumu

/* c
yorumu
bu
*/
```

### Defines

```gcsf
#undef MAX
#define MAX 100
#define name "string"
#define number 31.690000
```

### Extern C Block

```gclib
#extern <out.dll>

extern "c" {
    #define _anan anan
    #define _baban baban
}
```

### Debug

```gcsf
#debug "Library: " library._anan " : " library._baban
#debug "math: " PI " : " TAU " : " AUTHOR
#debug "local define: " your_mother
```

Namespace.member notasyonu (`library._anan`) kullanılabilir.

---

## Clang / Rust Style Error Handling

### Mevcut Durum (v0.1)

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
- Lokasyon: `dosya:satır:kolon` doğru
- Error/warning log'lar `.gcdebug` dosyasına yazılır

---

## Örnek Tam Pipeline

### math.gcsf

```gcsf
// math.gcsf — standard math library (simulated)
#define PI 3.14159
#define TAU 6
#define AUTHOR "GnuchanOS"
```

### library.gclib

```gclib
#extern <out.dll>

extern "c" {
    #define _anan anan
    #define _baban baban
}
```

### main.gcsf

```gcsf
#lib <library>
#include <math>

#define your_mother "my biggest WAIFU"

#debug "Library: " library._anan " : " library._baban
#debug "math: " PI " : " TAU " : " AUTHOR
#debug "local define: " your_mother
```

### IR Dump (`-ir`)

```
; ── IR Dump ──
; Dependency chain:
  #lib       library
  #include   math
  #extern    out.dll

; Resolved defines:
  your_mother          = "my biggest WAIFU"
  PI                   = 3.14159
  TAU                  = 6
  AUTHOR               = "GnuchanOS"

; Extern symbols:
  baban
  anan

; #debug statements:
  #debug  "Library: " anan " : " baban
  #debug  "math: " 3.14159 " : " 6 " : " "GnuchanOS"
  #debug  "local define: " "my biggest WAIFU"
```

### Codegen Çıktısı (`-codegen`)

```c
#include <stdio.h>

const char *baban = "baban";
const char *anan = "anan";
#define your_mother "my biggest WAIFU"
#define PI 3.14159
#define TAU 6
#define AUTHOR "GnuchanOS"

int main(void) {
    printf("Library: %s : %s\n", anan, baban);
    printf("math: %s : %s : %s\n", "3.14159", "6", "GnuchanOS");
    printf("local define: %s\n", "my biggest WAIFU");
    return 0;
}
```

### Runtime Çıktısı

```
Library: anan : baban
math: 3.14159 : 6 : GnuchanOS
local define: my biggest WAIFU
```

---

## Proje Yapısı

```
GnuchanOS/
├── assets/                  # Görseller (logo, icon, bg)
├── dotfile/                 # Dotfile'lar
├── fun_things/              # Denemeler, C module test vs.
├── language/                # GCL Derleyicisi
│   ├── src/
│   │   ├── main.c           # CLI + preprocessor (resolve, merge, export)
│   │   ├── types/types.h    # Token, AST node türleri
│   │   ├── common/          # defines (define tablosu + extern iterator), error
│   │   ├── frontend/        # lexer (tokenizer), parser (recursive descent)
│   │   └── backend/         # codegen (IR dump, AST dump, C codegen)
│   ├── build/               # gcl.exe + test .gcsf/.gclib dosyaları
│   └── makefile.py          # build script
├── os/                      # OS ile ilgili dosyalar
├── tutorials/               # C/Lua/Python eğitimleri
├── .github/workflows/       # CI (GitHub Actions)
├── .gitignore
├── README.md
└── LICENSE
```

## 🛠 Build

```bash
cd language
python makefile.py          # derle → build/gcl.exe
python makefile.py clean    # temizle

# veya direkt gcc ile:
gcc -std=c11 -Wall -Wextra -O2 \
  -I src -I src/types -I src/common -I src/frontend -I src/backend \
  src/main.c src/common/*.c src/frontend/*.c src/backend/*.c \
  -o build/gcl.exe
```

## 🔄 CI (GitHub Actions)

Her push'ta otomatik build + test (Ubuntu + Windows). Detaylar: [`.github/workflows/build.yml`](.github/workflows/build.yml)

## 📜 Lisans

GNU Affero General Public License v3.0 — bkz. [LICENSE](./LICENSE)

---

<div align="center"><i>maybe one day but not today</i></div>
