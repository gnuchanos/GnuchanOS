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
│   │   ├── lexer/           # Lexer (tokenizer)
│   │   ├── parser/          # Parser → AST
│   │   ├── type/            # Type system
│   │   ├── semantic/        # Semantic analysis
│   │   ├── ir/              # Intermediate Representation
│   │   ├── codegen/         # Code generation (IR → C)
│   │   └── runtime/         # Runtime (debug, crash analyzer)
│   ├── NOTES/               # Geliştirme notları
│   ├── _output_test/        # Build çıktıları (git ignore)
│   └── build.py             # Build script
├── os/                      # OS ile ilgili dosyalar
├── .gitignore
├── README.md
└── road_map_todo.md
```

## 🚀 GCL — Gnuchan C-Like Language Compiler

**Pipeline:**
```
Source → Lexer → Parser → AST → Semantic → IR → Codegen → C → GCC → Executable
```

### Kullanım

```bash
# Derle ve çalıştır (language/ içinde)
cd language
python build.py
.\_output_test\gcl.exe test.gcsf -run

# Sadece IR dökümü
.\_output_test\gcl.exe test.gcsf -emit-ir

# Debug mod (runtime monitoring)
.\_output_test\gcl.exe test.gcsf -debug -run
```

### Seçenekler

| Seçenek         | Açıklama                                |
|-----------------|-----------------------------------------|
| `-o <file>`     | Çıktı dosyası (varsayılan: a.out)       |
| `-run`          | Derle ve çalıştır                       |
| `-debug`        | Debug mod (runtime monitor + crash analyzer) |
| `-emit-c`       | Sadece C kodu üret (derleme yapma)      |
| `-emit-ir`      | IR dökümünü göster                       |
| `-help`         | Yardım                                  |

## 🛠 Build

```bash
cd language
python build.py
# Çıktı: language/_output_test/gcl.exe
```

## 📜 Lisans

GNU Affero General Public License v3.0 — bkz. [LICENSE](./LICENSE)

---

<div align="center"><i>maybe one day but not today</i></div>
