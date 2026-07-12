# MODULE SYSTEM — MODULE SYSTEM
# gcLang specific — not in C99/gnu99

gcLang's biggest difference from C is the module system.
There are no `#include` and `.h` files; instead:

  @lib <module>     → loads .gclib (gnuchan library)
  @include <file>   → imports .gcsf (gnuchan script)
  @extern <dll>     → links external C library .dll/.so/.a

═══════════════════════════════════════════════════════════════════════
1.  @lib — Library Loading
═══════════════════════════════════════════════════════════════════════

  To load a .gclib file in gcLang source code:

      @lib <stdio>
      @lib <string>
      @lib <raylib>

  @lib is like C's #include but the difference is:
  - It loads a .gclib file, not a .h file
  - Each .gclib is translated to a separate C header (during C_CODEGEN phase)
  - All .gclibs are combined in main_gclib.h
  - Paths are specified in build.gcdata / run.gcdata

  Example build.gcdata:
  {
      "deps": {
          "gclib": {
              "stdio":  "Library/stdio.gclib",
              "memory": "Library/memory.gclib",
              "string": "Library/string.gclib",
              "raylib": "Library/raylib.gclib"
          }
      }
  }

═══════════════════════════════════════════════════════════════════════
2.  @include — Script Importing
═══════════════════════════════════════════════════════════════════════

  Adds another .gcsf file to the current source:

      @include <utils>
      @include <math_helpers>

  Path in build.gcdata:
  {
      "deps": {
          "include": {
              "utils":       "src/utils.gcsf",
              "math_helpers": "src/math_helpers.gcsf"
          }
      }
  }

  Files loaded with @include are merged into the same AST,
  they are not treated as a separate module.

═══════════════════════════════════════════════════════════════════════
3.  @extern — C Library Binding
═══════════════════════════════════════════════════════════════════════

  To call functions from a C DLL/.so/.a in gcLang:

      @extern <raylib.dll>
      extern "c" {
          void InitWindow(int w, int h, const char* title);
          void CloseWindow();
          int WindowShouldClose();
          void BeginDrawing();
          void EndDrawing();
          void ClearBackground(int color);
      }

  - Build (.exe/.wasm): resolved at link time with .a or .lib
  - Run (IR executor): loaded at runtime via LoadLibrary/dlopen

  @extern types:
      @extern <raylib.dll>     → Windows runtime
      @extern <raylib.so>      → Linux runtime
      @extern <raylib.web.a>   → WASM link time

═══════════════════════════════════════════════════════════════════════
4.  extern "c" — C ABI Declaration
═══════════════════════════════════════════════════════════════════════

  Specifies that functions inside an extern "c" block will use the C ABI:

      extern "c" {
          int printf(const char* fmt, ...);
          void* malloc(unsigned long size);
          void free(void* ptr);
      }

  During C_CODEGEN, the extern "c" tag is removed,
  because C already uses it as the default ABI.

═══════════════════════════════════════════════════════════════════════
5.  @lib vs @include — Difference
═══════════════════════════════════════════════════════════════════════

  @lib <module>        @include <script>
  ─────────            ────────────────
  Loads .gclib         Imports .gcsf
  Independent module   Added to AST
  Combined in          No separate header
  main_gclib.h
  Not reloaded         Each @include is processed
  (idempotent)

═══════════════════════════════════════════════════════════════════════
6.  gcLang Module Vs C Include
═══════════════════════════════════════════════════════════════════════

  C:       #include <stdio.h>         → inserts .h file as raw text
  gcLang:  @lib <stdio>              → processes .gclib, generates header
  gcLang:  @include <utils>          → merges .gcsf into AST

  Advantages:
  - No .h file → no header guard hassle
  - .gclibs are passed through the preprocessor, cleaned
  - Dependencies are centrally managed in .gcdata config
  - The same .gclib is not loaded again with multiple @lib