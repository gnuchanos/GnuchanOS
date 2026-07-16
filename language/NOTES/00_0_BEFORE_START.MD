# Gnuchan Language System — Before Start

## File Types

| Extension  | Type                    | Description                      |
|------------|-------------------------|----------------------------------|
| `.gcsf`    | Gnuchan Script File     | Kaynak kod (source)              |
| `.gclib`   | Gnuchan Library         | Kütüphane modülü                 |
| `.gctf`    | Gnuchan Text File       | Doküman / data                   |
| `.gcdata`  | Gnuchan Config          | JSON-style proje ayarları        |

## Import / Link Direktifleri

### GCL (C tarafı) — `#` prefix
```
#extern <file.dll>      # .dll, .so, .a — external C library
#lib    <file>          # .gclib textfile
#include <file>         # .gcsf source file
```

### GCPY (Python tarafı) — direkt
```
import file, file, file
import folder in file, file, file
from file import things, things, things
from folder import file, file, file
import file as alias
from module import thing as alias
```

---

## CLI Parameters

```
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
gcl -all_flags file.gcsf -o out → full pipeline to executable

# Run / Build
gcl -run file.gcsf               → run with executor
gcl -build file.gcsf             → build executable
gcl -build file.gcsf -o out      → custom output name
gcl -debug run file.gcsf         → run with full debug

# WASM
gcl -wasm raylib
gcl -wasm binding
gcl -wasm export

# External Runtimes
gcl -luarun file.lua                  → run Lua via lua55.dll/so
gcl -luarun file.lua -dll path/lua55.dll
gcl -luarun file.lua -so  path/lua55.so

gcl -pyrun file.py                    → run Python via python3xx.dll/so
gcl -dll mylib.dll                    → load Windows DLL
gcl -so  libmylib.so                  → load Linux SO
```

---

## Runtime Architecture

```
Program Başlat
    │
    ▼
Runtime Başlat
    ├── Memory Manager       ← allocation/free tracking
    ├── Metadata Manager     ← instruction ID → file:line:func
    ├── Error Reporter       ← hata raporu formatlama
    └── Debug Thread (opt)   ← crash dump, file log, IDE notify
    │
    ▼
Compiler (her kaynak dosyası için)
    │
    ├── Her fonksiyon için
    │   └── Her satıra benzersiz InstructionID
    │       Örnek:
    │         ID 1001 → main.gcsf:25
    │         ID 1002 → main.gcsf:26
    │         ID 1003 → player.gcsf:81
    │
    ▼
Memory Allocation
    │
    ├── ptr = malloc(size)
    ├── Runtime → Allocation Table'a kaydet:
    │   {
    │       pointer,
    │       size,
    │       owner_file,
    │       owner_line,
    │       allocation_id
    │   }
    │
    ▼
Memory Access *(ptr + offset)
    │
    ├── Compiler'ın eklediği kontrol:
    │   if (offset >= allocation_size) {
    │       Runtime_Error(InstructionID, pointer, offset);
    │   }
    │
    ▼
Runtime_Error
    │
    ├── InstructionID'den → file, line, function bilgileri
    ├── Allocation Table'dan → allocation size, allocation location
    │
    ▼
Rapor Oluştur
    ┌─────────────────────────────────────┐
    │        MEMORY ERROR                │
    ├─────────────────────────────────────┤
    │ File:            player.gc         │
    │ Function:        UpdatePlayer()    │
    │ Line:            81                │
    │ Operation:       Write             │
    │ Pointer:         0x12345678        │
    │ Allocation Size: 64 bytes          │
    │ Attempted Offset: 72 bytes         │
    │ Reason:          Buffer Overflow   │
    │ Allocated At:    memory.gc:35      │
    └─────────────────────────────────────┘
    │
    ▼
Debug Thread (opsiyonel)
    ├── Hata raporunu dosyaya yaz
    ├── Hata raporunu konsola yaz
    ├── Hata raporunu IDE'ye gönder
    └── Crash dump oluştur
    │
    ▼
Programı Sonlandır / Exception Fırlat / Devam Et (ayarına göre)
```

---

## Güvenlik Katmanları

| Katman | Aşama           | Ne Yapar                                              |
|--------|-----------------|-------------------------------------------------------|
| 1      | Derleme Zamanı  | type checking, const correctness, attribute validation |
| 2      | Debug Runtime   | bounds, null, overflow, use-after-free, leak check    |
| 3      | Release         | opsiyonel lightweight guards, error path only         |

### Performans

| Build      | gc_malloc        | gc_free        | Kontroller                | Overhead |
|------------|------------------|----------------|---------------------------|----------|
| Release    | malloc           | free           | hiçbiri (derlenmez)       | %0       |
| Debug      | tracked_malloc   | tracked_free   | bounds + null + overflow  | ~%5-15   |
| Hybrid     | malloc           | free           | sadece error path         | <%1      |

---

## Instruction ID Format

```
GCLID:<dosya>:<satır>:<sütun>:<fonksiyon>
```

Örnek: `GCLID:player.gcsf:81:5:TakeDamage`

## Error Type Codes

| Kod | Sabit                     | Açıklama                  |
|-----|---------------------------|---------------------------|
| 0   | GCL_ERR_NONE              | Hata yok                  |
| 1   | GCL_ERR_NULL_POINTER      | NULL dereference          |
| 2   | GCL_ERR_BOUNDS            | Array index out of bounds |
| 3   | GCL_ERR_USE_AFTER_FREE    | Freed memory access       |
| 4   | GCL_ERR_BUFFER_OVERFLOW   | Stack buffer overflow     |
| 5   | GCL_ERR_INTEGER_OVERFLOW  | Arithmetic overflow       |
| 6   | GCL_ERR_DIV_BY_ZERO       | Division by zero          |
| 7   | GCL_ERR_INVALID_CAST      | Invalid type cast         |
| 8   | GCL_ERR_MEMORY_LEAK       | Unfreed allocation (debug)|
| 9   | GCL_ERR_DOUBLE_FREE       | Freeing freed pointer     |
| 10  | GCL_ERR_STACK_OVERFLOW    | Recursion too deep        |
