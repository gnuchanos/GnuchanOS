# GnuchanOS

> **GnuChanIDE** + **GCL** — one language, with Python and Lua built in.

## Mission

I love Lua and Python, but I want to make my own language. So I take it all.

- **GCL** — the main C-like language
- **Python** — embedded, a native part of GCL
- **Lua** — embedded, a native part of GCL

---

## Fridge Model (Buzdolabı Modeli)

Think of GCL as a fridge. Inside it there is tons of stuff — Python 3.14,
Lua 5.4.7, raylib, pip-installed packages, and (coming soon) GCL's own
standard library. You do not write Python code inside `.gcsf` files, and
GCL is not a shell that just runs `python script.py`. Instead:

- Python and Lua are **capabilities of GCL**, ready to use when GCL code asks for them.
- `gcl -pyrun script.py` and `gcl -luarun script.lua` are **only simple execution shortcuts** — not the real integration model.
- The LSP scans the **whole fridge**: stdlib, site-packages, C extension modules, and import chains — so the IDE knows everything the fridge contains.

```
             ┌─────────────────────────────────────────────┐
             │                  GCL (fridge)               │
             │                                             │
             │   Python 3.14      Lua 5.4.7      raylib    │
             │   (pyLibrary)      (lua.gcDL)     (+ pyd)   │
             │   + stdlib         + stdlib                 │
             │   + site-packages  + luaLibrary             │
             │   + C extensions                            │
             │                                             │
             │   GCL language  ──  native capabilities     │
             └───────────────────┬─────────────────────────┘
                                 │
                                 ▼
                      GnuChanIDE + gcl-LSP
                      (full fridge scan: imports, stdlib,
                       site-packages, C extensions)
```

---

## Language Pipeline

```
GCL Source ──► .gclib/extern ──► .gcsf/extern ──► main.gcsf
     │
     ▼
SharedPipeline ──► FastIR
```

```
src/
├── FastIR/
└── SharedPipeline/
    ├── Linker/
    ├── Lexer/
    ├── Parser/
    ├── Semantic/
    ├── TypeChecker/
    ├── Memory/
    ├── GarbageCollector/
    ├── AST/
    ├── Diagnostics/
    ├── Ir/
    └── Common/
```

---

## Execution Modes

| Mode | Purpose |
|------|---------|
| `gcl -run file.gcsf` | interpreted (first 1000 calls → JIT) |
| `gcl -frun file.gcsf` | bytecode + VM, hot functions → JIT |
| `gcl -pyrun script.py` | simple Python run (shortcut only) |
| `gcl -luarun script.lua` | simple Lua run (shortcut only) |
| `gcl -m <module>` | run Python module (e.g. `gcl -m pip`) |

---

## Memory Management

```
              Program
                 │
    ┌────────────┼─────────────┐
    │            │             │
   Stack        Heap      Static Data
    │            │
    │      ┌─────┼─────┐
    │      │     │     │
   Local  Arena Pool Custom
```

Everything is checked: OOM + null + bounds + overflow.

---

## Directory Structure

```
build/windows/
├─ gcl                        <- GCL compiler driver (hub)
├─ gcl-LSP                    <- GCL LSP (full system scan)
├─ GnuChanIDE                 <- GnuChanIDE launcher (10KB)
├─ GnuChanIDE_JUNKS/          <- Electron runtime (actual exe + dll, pak, locales, resources)
└─ Library/
   ├─ Lua/
   │  ├─ lua.gcDL             <- Lua 5.4.7 embed
   │  ├─ lua_raylib.gcDL      <- Lua raylib binding (gcl.raylib)
   │  ├─ lua.gcReference      <- Lua embed API (signed, with examples)
   │  ├─ lua_raylib.gcReference <- gcl.raylib API (signed, with examples)
   │  ├─ lua.doc              <- beginner Lua guide (English)
   │  └─ luaLibrary/          <- user Lua helpers (wrappers + docs)
   ├─ Python/
   │  ├─ python.gcDL          <- Python 3.14 embed
   │  ├─ python_raylib.gcDL   <- Python raylib binding (gcl_raylib)
   │  ├─ pyRaylib.py          <- Python raylib wrapper
   │  ├─ *.gcReference        <- signed API references
   │  ├─ py.doc               <- beginner Python guide (English)
   │  └─ pyLibrary/           <- embed runtime (python314.dll + Lib/ + stdlib)
   │      └─ Lib/site-packages <- pip-installed packages (gcl -m pip)
   └─ bridge/
      └─ bridge.gcDL          <- cross-language data bridge (Lua <-> Python)
```

---

## LSP — Python-First Full System Scan

The LSP is currently **Python-focused**. Lua and GCL will join later. It scans
the entire embedded Python system — not just the open project:

- **Workspace files** — every `.py` in the project (`import ossuruk` → `ossuruk.zamber` works).
- **Python stdlib** — the real `Lib/` + `python314.zip` contents (`import os` → `os.path.join`).
- **site-packages** — everything installed via `gcl -m pip`.
- **C extension modules** — `.pyd`/C extensions resolved by asking Python itself.
- **Import chains** — `from x import y`, aliases, submodules.

Because `pyLibrary`, `Lib`, and `site-packages` are part of the fridge, they are
scanned — not treated as junk — and unresolved symbols fall back to asking the
embedded Python directly.

```
GnuChanIDE (Electron)
      │
      │  NDJSON over stdio
      ▼
gcl-LSP  ──► static workspace index (fast)
      │
      ▼
embedded Python query  ──► real imports, dir(), inspect
      │                        (stdlib + site-packages + .pyd)
      ▼
            full completion: project + fridge
```

---



See [gnuchanos.md](gnuchanos.md) for the full architecture reference.
