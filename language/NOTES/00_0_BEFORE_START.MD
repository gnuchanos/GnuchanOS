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

gcl -flags file.gcsf -o path/output_name
gcl -flags -run file.gcsf

-flags butun flaglari tek tek yazmamak icin ve bu  flag ismi degil flag falan cagirmiyor placeholder isim bu kadar

```


