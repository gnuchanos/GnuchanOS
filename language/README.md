# GCL — Gnuchan C-Like Language Compiler

**GCL**, GNU99 C ile uyumlu, güvenli ve yüksek performanslı bir programlama dilidir.  
Bu derleyici, `.gcsf` kaynak dosyalarını C koduna transpile eder ve ardından GCC ile binary üretir.

## Hızlı Başlangıç

```bash
# Derleyiciyi build et
cd language/src
python makefile.py

# Bir .gcsf dosyasını compile et
python makefile.py run ../build/main.gcsf

# Veya direkt gcl binary ile:
../build/gcl ../build/main.gcsf -o ../build/main.c
gcc -std=gnu99 ../build/main.c -o ../build/main.exe -lm
```

## Özellikler

- **GNU99 uyumlu** — C99 standardı + GNU extensions
- **Güvenli C** — runtime safety (bellek yönetimi, double-free koruması)
- **Sıfır maliyet** — debug kapalıyken hiçbir overhead
- **Clang/Rust style hatalar** — renkli, caret'li, JSON format destekli
- **JIT desteği** — (ileride) anında makine kodu üretimi
- **UTF-8** — tam Unicode desteği (ileride)

## Pipeline

```
Source (.gcsf) → Lexer → Parser → AST → Semantic Analysis → Codegen → C code → GCC → Binary
```

## CLI Kullanımı

```
gcl                                    → interactive shell
gcl -version, -v                       → version info
gcl -help, -h                          → help
gcl -lexer file.gcsf                   → token stream dump
gcl -parser file.gcsf                  → parse tree dump
gcl -ast file.gcsf                     → AST dump
gcl -codegen file.gcsf                 → generated C code (stdout)
gcl file.gcsf -o output.c              → compile to C file
gcl -run file.gcsf                     → compile + run
gcl -debug                             → debug output
gcl -linclude path                     → extra include path
gcl -llib path                         → extra library path
```

## Desteklenen Tipler

| Tip | Açıklama |
|-----|----------|
| `int`, `char`, `short`, `long`, `long long` | Standart C tipleri |
| `float`, `double`, `long double` | Kayan noktalı tipler |
| `void`, `bool` | Özel tipler |
| `int8`–`int128`, `uint8`–`uint64` | Sabit genişlikli tipler |
| `unsigned int/char/short/long/float/double` | Unsigned varyantlar |
| `struct`, `enum`, `union`, `typedef` | Bileşik tipler |

## Desteklenen Sözdizimi

### Preprocessor
```c
#lib <stdio>          // → #include <stdio.h>
#include "file.gcsf"   // source include
#extern <raylib.dll>   // external library
#define MAX 100
#undef MAX
#ifdef / #ifndef / #if / #elif / #else / #endif
#error mesaj
#pragma mesaj
```

### Yorumlar
```c
// C-style yorum
/* ... */            // block yorum
# gcl comment         // GCL tek satır
#| ... |#            // GCL block yorum
#// ...              // GCL C++ style
```

### Değişkenler
```c
int x = 10;
char name[] = "Gordon Freeman";
char *game = "HALF LIFE 3";
char names[3][32] = { "Gordon", "Alyx", "Barney" };
char *games[] = { "Half Life", "Portal" };
char **dynamic = malloc(2 * sizeof(*dynamic));
```

### Operatörler
Tüm C operatörleri: aritmetik (`+`, `-`, `*`, `/`, `%`), bitsel (`&`, `|`, `^`, `~`, `<<`, `>>`), mantıksal (`&&`, `||`, `!`), karşılaştırma (`==`, `!=`, `<`, `>`, `<=`, `>=`), atama (`=`, `+=`, `-=`, vb.), ternary (`?:`), prefix/postfix (`++`, `--`)

### Kontrol Akışı
```c
if (x > 0) { ... } else { ... }
for (int i = 0; i < 10; i++) { ... }
while (condition) { ... }
do { ... } while (condition);
switch (value) { case 0: ... break; default: ... }
return expr;
```

### Fonksiyonlar
```c
int add(int a, int b) { return a + b; }
int printf(const char *fmt, ...);  // variadic
```

### Bellek Yönetimi
```c
int *ptr = malloc(10 * sizeof(int));
ptr = realloc(ptr, 20 * sizeof(int));
free(ptr);
```

## Hata Sistemi

Clang/Rust tarzı, her hataya özel kod:

```
error[E011]: undeclared variable 'x'
 --> test.gcsf:5:10
  |
5 | int y = x + 1;
  |         ^ undeclared variable 'x'
```

**Hata Kodları:** E000–E031 (internal, syntax, type, memory, I/O)  
**Warning Kodları:** W001–W004  
**Format:** Renkli terminal çıktısı veya `-ferror-format=json` ile JSON

## GitHub Actions CI/CD

Her push ve PR'da otomatik build + test:
- **Ubuntu** (GCC) — binary build + 4 örnek test
- **Windows** (MinGW) — cross-platform doğrulama
- Artifact upload (binary + generated C)

Workflow: `.github/workflows/build.yml`

## Proje Yapısı

```
language/
├── src/
│   ├── main.c              # Entry point
│   ├── makefile.py         # Build system
│   ├── include/            # Headers (ast, tokens, types, errors)
│   ├── cli/                # CLI arg parsing
│   ├── lexer/              # Tokenizer
│   ├── parser/             # Recursive descent parser
│   ├── ast/                # AST node management
│   ├── semantic/           # Symbol table + type checker
│   ├── codegen/            # C transpiler
│   ├── error/              # Error reporting
│   ├── type/               # Type system helpers
│   ├── runtime/            # Memory management (area/arena)
│   ├── version/            # Version info
│   ├── linker/             # Linker stub
│   ├── shell/              # Interactive shell stub
│   └── jit/                # JIT stub
├── build/                  # Build artifacts + test .gcsf files
├── _NOTES/                 # Spec dokümanları
├── todo_list.md            # Roadmap
└── README.md               # Bu dosya
```

## Geliştirme

```bash
cd language/src

# Build
python makefile.py

# Clean
python makefile.py clean

# Build + test
python makefile.py test

# Tek dosya compile + run
python makefile.py run ../build/main.gcsf
```

## Lisans

MIT License — GnuchanOS projesinin bir parçasıdır.
