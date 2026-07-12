# gcLang vs C — QUICK DIFFERENCE GUIDE
# gcLang simplifies and modernizes C99/gnu99

═══════════════════════════════════════════════════════════════════════
WHAT C HAS BUT gcLang DOESN'T
═══════════════════════════════════════════════════════════════════════

  #include <...>        → NOT AVAILABLE. Instead @lib <module> (see: 10_MODULES)
  .h header file        → NOT AVAILABLE. Instead .gclib
  #include "..."        → NOT AVAILABLE. Instead @include <script>
  #ifndef header guard  → NOT NEEDED. @lib is not reloaded
  
  Pointer arithmetic    → EXISTS but GC will automate it later
  malloc/free           → EXISTS but will become safe with arena tracking

═══════════════════════════════════════════════════════════════════════
WHAT gcLang HAS BUT C DOESN'T
═══════════════════════════════════════════════════════════════════════

  @lib <module>         → loads .gclib (replaces C's #include)
  @include <script>     → imports .gcsf
  @extern <dll>         → binds external C library
  extern "c" { }        → C ABI block
  
  __GCLANG__            → built-in macro (1 = compiling in gcLang)
  __GCLANG_VER__        → version constant

  IR Executor           → -run mode, linear IR interpreter without bytecode VM
  .gcdata               → JSON-style config (build.gcdata / run.gcdata)
  gcl CLI               → gcl -run, gcl -build --wasm, gcl -build --native

═══════════════════════════════════════════════════════════════════════
WHAT IS SIMPLIFIED IN gcLang
═══════════════════════════════════════════════════════════════════════

  C'de                    gcLang'de
  ─────                   ────────
  int32_t from stdint.h   i32(v) macro (built-in)
  bool from stdbool.h     bool, true, false (built-in)
  #include <stdio.h>      @lib <stdio>
  #include <stdlib.h>     @lib <stdlib>
  writing header guards   not needed (@lib idempotent)
  makefile/cmake          .gcdata JSON config

═══════════════════════════════════════════════════════════════════════
SAME IN BOTH
═══════════════════════════════════════════════════════════════════════

  - Basic types: int, char, float, double, void, struct, union, enum
  - Control flow: if, for, while, do/while, switch, goto
  - Pointers: *, &, ->, []
  - Function definition and call
  - Operators: arithmetic, logical, bitwise, comparison
  - sizeof, typedef
  - Preprocessor: #define, #undef, #if, #ifdef, #error, #pragma
  - Arrays: static, VLA, multi-dimensional, designated init