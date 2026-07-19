<div align="center">
  <img src="./assets/logo.png" alt="Gnuchanos" width="120" />
</div>

<div align="center"><strong>Gnuchanos</strong> — kişisel GNU/Linux tabanlı işletim sistemi denemesi (rolling release).</div>

<div align="center">Bu dil, Guix dağıtımında yazılım geliştirmek için kullanılacak.</div>

<hr>

## 📁 Proje Yapısı

```
GnuchanOS/
├── assets/                  # Görseller (logo, icon, bg)
├── dotfile/                 # Dotfile'lar
├── fun_things/              # Denemeler, Lua binding vs.
├── language/                # GCL (Gnuchan C-Like) Dil Derleyicisi
│   ├── src/                 # Kaynak kod
│   │   ├── include/         # Header'lar (gcl.h, error.h, memory.h, semantic.h, version.h ...)
│   │   ├── cli/             # CLI flag parser
│   │   ├── lexer/           # Lexer — hex/oct/bin/char/tüm operatorler
│   │   ├── parser/          # Pratt parser, operator precedence
│   │   ├── ast/             # AST node helper'ları + dump
│   │   ├── codegen/         # C kodu üretimi
│   │   ├── error/           # Rust-stili hata + source line + caret
│   │   ├── runtime/         # Memory allocator (arena/pool/freelist/page)
│   │   ├── semantic/        # Sembol tablosu + scope yönetimi
│   │   └── version/         # Derleyici + dil versiyon bilgisi
│   ├── build/               # Build script + obj/ + gcl.exe
│   ├── NOTES/               # Geliştirme notları (SECURITY.MD)
│   └── output_test_area/    # .gcsf test dosyaları + run_tests.py
├── os/                      # OS ile ilgili dosyalar
├── tutorials/               # C/Lua/Python eğitimleri
├── .gitignore
├── README.md
└── LICENSE
```

## 🚀 GCL — Gnuchan C-Like Language Compiler

**Pipeline:**
```
Source → Lexer → Parser → AST → Semantic Analysis → Codegen → C → GCC → Executable
```

### Kullanım

```bash
# Derle (language/build/ icinde)
cd language/build
python build.py
.\gcl.exe input.gcsf -o out.c

# Token akisi
.\gcl.exe -lexer input.gcsf

# AST dökümü
.\gcl.exe -ast input.gcsf

# Versiyon bilgisi
.\gcl.exe -v
```

### Seçenekler

| Seçenek         | Açıklama                                |
|-----------------|-----------------------------------------|
| `-o <file>`     | Çıktı dosyası                           |
| `-lexer`        | Token akisini göster                    |
| `-ast` / `-parser` | AST dökümünü göster                  |
| `-debug`        | Debug mod                               |
| `-v` / `--version` | Versiyon bilgisi                     |

### Özellikler

- ✅ `int` değişken bildirimi: `int a = 5;`
- ✅ Hex / Octal / Binary literal: `0xFF`, `077`, `0b1010`
- ✅ Char literal + kaçış karakterleri: `'A'`, `'\n'`
- ✅ Tüm C operatörleri (Pratt parser, doğru öncelik)
- ✅ Rust-stili hata raporlama (source line + caret + hata kodu)
- ✅ Memory: arena (hızlı) + pool (≤64B) + freelist (≤64KB) + page (>64KB)
- ✅ Semantic analiz: bildirilmemiş/tekrar bildirilmiş değişken yakalama
- ✅ Versiyon: `gcl -v` → `0.1.0-dev`, `GCL'25` standardı
- 🔒 Runtime güvenlik katmanı debug-only (plan)

### Performans

Tüm güvenlik katmanları **debug-only**. Release modda:
- Sıfır runtime overhead → direkt C hızı
- Semantic kontroller derleme anında → runtime maliyeti yok

## 🛠 Build

```bash
cd language/build
python build.py         # derle
python build.py test    # derle + 7 test
python build.py clean   # obj + binary temizle
```

## 📜 Lisans

GNU Affero General Public License v3.0 — bkz. [LICENSE](./LICENSE)

---

<div align="center"><i>maybe one day but not today</i></div>
