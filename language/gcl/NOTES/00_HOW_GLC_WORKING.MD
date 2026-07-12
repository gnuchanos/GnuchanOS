=======================================================================
=== GCLANG WORKING PIPELINE
=======================================================================

This document explains how gcLang works: pipelines, file types,
and library management.

only for gnu/linux and windolf

═══════════════════════════════════════════════════════════════════════
1.  FILE TYPES
═══════════════════════════════════════════════════════════════════════

  .gcsf   → gnuchan script file  (source code)
  .gclib  → gnuchan library      (library module)
  .gctf   → gnuchan text file    (document / data)
  .gcdata → JSON-style config    (project settings)

  @lib <module_name>      → loads module_name.gclib
  @include <file_name>    → imports file_name.gcsf
  @extern <library>       → links external C library .dll / .so / .a
          extern void my_c_function(); → specifies C ABI usage

# -run shared pipe line (lexer -> ast/parser -> IR) -> jit -> run
# -build shared pipe line (lexer -> ast/parser -> IR) -> .c, .o/raylib.h -> wasm/emsdk
# -build shared pipe line (lexer -> ast/parser -> IR) -> .c, .o/raylib.h -> .exe or gnu/linux

═══════════════════════════════════════════════════════════════════════
2.  PROJECT CONFIG
═══════════════════════════════════════════════════════════════════════

  gcLang works in two modes:  gcl -run  and  gcl -build

  Each mode uses its own .gcdata file:

      gcl -run   → run.gcdata     (execution via IR executor)
      gcl -build → build.gcdata   (compilation: wasm / native)

  Example run.gcdata / build.gcdata:

  {
      "project_name": "my first program",
      "developer": "gnuchanos",
      "version": 0.1,

      "main": "src/main.gcsf",
      "output_name": "gcl_default_program",

      "emsdk": "full_path", -> empty skip
      "assets_dir": "full_path", -> empty skip

      "deps": {
          "gclib": {
              "stdio":  "Library/stdio.gclib",
              "memory": "Library/memory.gclib",
              "string": "Library/string.gclib",
              "raylib": "Library/raylib.gclib"
          },
          "extern": {
              "raylib": "Library/External/raylib.dll"
          },
          "include": {
              "script_name": "path/to/script.gcsf"
          }
      }
  }

## default main.gcsf
#lib <stdio> --> Library/stdio.gclib

int main(int argc, char **argv) {
    printf("welcome to gnuchan language \n");

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    return 0;
}

## flags
# gcl -there_is_no_order_for_flag

gcl -> interactive shell
gcl -build wasm raylib
gcl -build wasm binding
gcl -build wasm export

gcl -debug run
gcl -version
gcl -help


# extra future
gcl -luarun -> lua55.dll, lua55.so
    gcl -luarun file.lua -dll path/lua55.dll or -so path/lua55.so
        -dll -> missing path or file
        -so  -> missing path or file


═══════════════════════════════════════════════════════════════════════
3.  PIPELINE: gcl -run  (IR Executor — Fast Development)
═══════════════════════════════════════════════════════════════════════

  project.gcdata
        ↓
  ① PREPROCESSOR  → resolves @lib, @include, @extern
        ↓
  ② LEXER         → token sequence
        ↓
  ③ PARSER        → AST (Abstract Syntax Tree)
        ↓
  ④ SEMANTIC / TYPECHECK
        ↓
  ⑤ IR LOWERING   → AST converted to linear IR instruction list
        ↓
  ⑥ LINKER        → extern DLLs loaded, symbols resolved
        ↓
  ⑦ IR EXECUTOR   → IR instruction array executed with pc

  IR Components:

  ir_build.c         → IR instruction generation from AST
  ir_instr.h         → instruction set definitions
  ir_execute.c       → interpreter loop (switch-case dispatch)
  ir_frame.c         → stack frame + slot management
  ir_value.c         → runtime value system
  ir_control_flow.c  → jump/label resolution
  ir_extern_call.c   → DLL calls (LoadLibrary/dlopen)
  ir_debug.c         → trace / debug

  IR Instruction Set:

  Data:        LOAD_CONST, LOAD_LOCAL, STORE_LOCAL, LOAD_GLOBAL, STORE_GLOBAL
  Arithmetic:  ADD, SUB, MUL, DIV, MOD
  Comparison:  CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE
  Logic:       NOT, NEG
  Flow:        JMP, JMP_IF_FALSE, RET
  Call:        CALL, CALL_EXTERN
  Pointer:     ADDR_OF, LOAD_INDIRECT, STORE_INDIRECT, MEMBER_PTR, INDEX_PTR
  Conversion:  CAST, SIZEOF_CONST

═══════════════════════════════════════════════════════════════════════
4.  PIPELINE: gcl -build --wasm  (WASM Output)
═══════════════════════════════════════════════════════════════════════

  project.gcdata
        ↓
  ① PREPROCESSOR  → resolves @lib, @include, @extern
        ↓
  ② LEXER         → token sequence
        ↓
  ③ PARSER        → AST
        ↓
  ④ C_CODEGEN     → generates C code from AST
        ↓
  ⑤ Postprocess:
       - __declspec(dllexport) is cleaned
       - "raylib." prefix is removed (direct function name in C)
        ↓
  ⑥ main_gclib.h is generated:
       - Each .gclib → converted to separate C header
       - @extern, extern "c" is cleaned
       - All are merged into a single header
        ↓
  ⑦ shell.html is copied (template/shell.html → target/)
        ↓
  ⑧ emcc -c compiles .c → .o
        ↓
  ⑨ emcc link:
       .o + all .a's + WASM=1 + USE_GLFW=3 + ASYNCIFY
       + --shell-file shell.html
       + --preload-file asset (if any)
        ↓
  ⑩ Output:
       target/debug/index.html
       target/debug/<output_name>.wasm
       target/debug/<output_name>.js

═══════════════════════════════════════════════════════════════════════
5.  PIPELINE: gcl -build --native  (Native Output — FUTURE)
═══════════════════════════════════════════════════════════════════════

  Not yet implemented — plan:

  project.gcdata
        ↓
  ... (PREPROCESSOR → LEXER → PARSER → C_CODEGEN same)
        ↓
  C_CODEGEN → generates C code from AST
        ↓
  compile with gcc/clang:
       .c → .o
        ↓
  link with linker for native binary:
       .o + .a + .dll/.so
        ↓
  Output:
       target/debug/<output_name>.exe  (Windows)
       target/debug/<output_name>       (Linux)

═══════════════════════════════════════════════════════════════════════
6.  # C MACROS
═══════════════════════════════════════════════════════════════════════

  In gcLang, the # character has two different roles:

    #define      → defines a macro
    #if          → opens conditional compilation
    #ifdef       → compiles if defined
    #ifndef      → compiles if not defined
    #undef       → removes a macro
    #error       → prints error message
    #pragma      → compiler directive

  Rule:
      # + directive_name (define, if, ifdef, etc.) = PREPROCESSOR

═══════════════════════════════════════════════════════════════════════
7.  @extern DYNAMIC LOADING — IMPORTANT NOTE
═══════════════════════════════════════════════════════════════════════

  In gcLang, @extern specifies how a C library is called.
  HOW it is loaded (dynamic/static) changes depending on the EXECUTION MODE.

  @extern <raylib.dll>   → means "link the raylib library"
  @extern <raylib.web.a> → same meaning, different file

  gcl -run  (IR Executor):
      @extern <raylib.dll>  → LoadLibrary("raylib.dll")  [dynamic]
      @extern <raylib.so>   → dlopen("raylib.so")        [dynamic]
      Extern functions are resolved symbol by symbol at IR runtime.
      The library file is read from the extern path in run.gcdata.
      No hardcode — gcl just opens the file at the given path.

  gcl -build --wasm (C_CODEGEN + emcc):
      @extern <raylib.web.a>  → .a is added during emcc link phase
      @extern declaration is cleaned by C_CODEGEN,
      because on the C side, raylib.h already knows the prototypes.
      The .a file is taken from the extern path in build.gcdata.
      No hardcode — gcl only uses what is written in .gcdata.

  IMPORTANT:
      @extern itself only says "I want this library".
      Which file is used (raylib.dll / .so / .web.a)
      is specified in the .gcdata config. There is NO hardcoded/statically
      defined library path in gcl.

  Exception — gcl sees its own Library/ folder:
      The Library/ folder in the directory where gcl runs contains built-in
      .gclib files (stdio.gclib, stdlib.gclib, etc.).
      Everything else is read from .gcdata.

═══════════════════════════════════════════════════════════════════════
11. gcLang ≠ C — IMPORTANT DIFFERENCES
═══════════════════════════════════════════════════════════════════════

  #include  → NOT AVAILABLE. Instead:   @lib <module>  (loads .gclib)
                             @include <file> (imports .gcsf)
  .h header → NOT AVAILABLE. Instead:   .gclib (gnuchan library)
  C library → linked with @extern <raylib.dll>

  gcLang is based on gnu99 but has its own syntax:
  - @lib <raylib> → raylib.gclib
  - @extern <raylib.dll> → C DLL
  - @include <script> → script.gcsf

═══════════════════════════════════════════════════════════════════════
12. MEMORY MANAGEMENT
═══════════════════════════════════════════════════════════════════════

  📍 Manual Pointer (current)
  ----------------------------
  type *p = malloc(sizeof(type));
  *p = 42;
  free(p);

  type *arr = calloc(5, sizeof(type));
  arr[0] = 1;
  free(arr);

  int *p = malloc(2 * sizeof(int));
  p[0] = 1;
  p[1] = 2;
  p = realloc(p, 4 * sizeof(int));
  p[2] = 3;
  p[3] = 4;
  free(p);

  📍 Automatic Pointer (future)
  -------------------------------
  - GC (Garbage Collection) will be added
  - malloc's will be collected in an arena
  - Before program close, it will check if user freed memory

═══════════════════════════════════════════════════════════════════════
13. RUNTIME TYPES (External Libraries)
═══════════════════════════════════════════════════════════════════════

  Windows:  .dll
  Linux:    .so
  Web:      .a  (emscripten with wasm)

═══════════════════════════════════════════════════════════════════════
14. LINK TIME TYPES (Compile Time)
═══════════════════════════════════════════════════════════════════════

  .a   → static library
  .o   → object file