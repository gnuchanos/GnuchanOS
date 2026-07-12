# gcLang CLI — COMMAND LINE USAGE
# gcl — gcLang compiler/runner

═══════════════════════════════════════════════════════════════════════
USAGE
═══════════════════════════════════════════════════════════════════════

  gcl -run                → Run with IR Executor (run.gcdata)
  gcl -run --debug-ir     → Run with IR instruction trace

  gcl -build --wasm       → Generate WASM output (build.gcdata)
  gcl -build --native     → Generate native binary (future)

  gcl -version            → version info
  gcl -help               → help

═══════════════════════════════════════════════════════════════════════
WORKING DIRECTORY STRUCTURE
═══════════════════════════════════════════════════════════════════════

  project/
    ├── src/
    │   ├── main.gcsf         # main source
    │   ├── utils.gcsf        # utilities (optional)
    │   └── ...
    ├── Library/
    │   ├── stdio.gclib       # gcLang std libs
    │   ├── stdlib.gclib
    │   ├── string.gclib
    │   ├── memory.gclib
    │   ├── raylib.gclib
    │   └── External/
    │       ├── raylib.dll     # Windows runtime
    │       ├── raylib.so      # Linux runtime
    │       └── raylib.web.a   # WASM link
    ├── run.gcdata             # -run config
    ├── build.gcdata           # -build config
    ├── template/
    │   └── shell.html         # WASM shell template
    └── target/
        └── debug/             # outputs

═══════════════════════════════════════════════════════════════════════
gcl -run FLOW
═══════════════════════════════════════════════════════════════════════

  1. Read run.gcdata
  2. Get main.gcsf, resolve @lib / @include / @extern
  3. Lex → Parse → AST
  4. IR Lower → instruction list
  5. LINKER: Load external DLLs (LoadLibrary / dlopen)
  6. IR Executor: start from main(), execute with pc

═══════════════════════════════════════════════════════════════════════
gcl -build --wasm FLOW
═══════════════════════════════════════════════════════════════════════

  1. Read build.gcdata
  2. Get main.gcsf, resolve @lib / @include / @extern
  3. Lex → Parse → AST
  4. C_CODEGEN → Generate C code
  5. Postprocess (export/prefix cleanup)
  6. Create main_gclib.h (.gclib → header)
  7. emcc .c → .o → wasm link
  8. Output: index.html, .wasm, .js