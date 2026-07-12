# MACROS & PREPROCESSOR
# gcLang / C99 / gnu99 — gcLang base

## IMPORTANT: gcLang ≠ C
======================
gcLang takes its foundation from gnu99 but has important differences from C:

- `#include` — NOT AVAILABLE in gcLang. Instead:
  - `#lib <module>` → loads `.gclib` (gnuchan library) file
  - `#include <script>` → imports `.gcsf` (gnuchan script file) file
- `.h` (header) file — NOT AVAILABLE in gcLang. Libraries use `.gclib` extension
- `#extern <dll>` → external C library (`raylib.dll`, `raylib.so`, `raylib.web.a`)
- `extern void my_c_function();` → specifies C ABI usage
- gcLang source files: `.gcsf`, `.gclib`, `.gctf`, `.gcdata`

---

## Preprocessor Directives Common to gcLang and C
=====================================================

```
#define      → macro definition
#undef       → macro removal
#if          → conditional compilation
#ifdef       → if defined
#ifndef      → if not defined
#elif        → else if
#else        → else
#endif       → block close
#error       → error message
#pragma      → compiler directive
#line        → line number declaration
```

---

## #define — Macro Definition
=============================

```c
// constant macro
#define PI 3.14159265358979323846

// parameterized macro
#define SQUARE(x) ((x) * (x))

// gcLang type conversion macros
#define i8(v)   ((signed char)(v))
#define u8(v)   ((unsigned char)(v))
#define i16(v)  ((short)(v))
#define u16(v)  ((unsigned short)(v))
#define i32(v)  ((int)(v))
#define u32(v)  ((unsigned int)(v))
#define i64(v)  ((long long)(v))
#define u64(v)  ((unsigned long long)(v))
#define f32(v)  ((float)(v))
#define f64(v)  ((double)(v))
#define ch(v)   ((char)(v))
#define str(v)  ((char*)(v))
#define b8(v)   ((bool)(v))
```

---

## #ifndef — Header Guard
=============================

```c
#ifndef PI
    #define PI 3.14159265358979323846
#endif
```

---

## Conditional Compilation Example
===========================

```c
#if gnulinux
    // linux-specific code
#elif windows
    // windows-specific code
#endif
```

---

## #undef — Macro Removal (Undefine)
=========================================

```c
#undef i8
#undef u8
#undef i16
#undef u16
#undef i32
#undef u32
#undef i64
#undef u64
#undef f32
#undef f64
#undef ch
#undef str
#undef b8
```

### Conflict Prevention
```c
#define i8(v) ((signed char)(v))
// ... code ...
#undef i8
#define i8(v) some_other_thing(v)
```

### Conditional Removal
```c
#ifdef i8
    #undef i8
#endif
```

---

## Variadic Macros (__VA_ARGS__)
===================================

```c
#define debug(fmt, ...) printf(fmt, __VA_ARGS__)
debug("x=%d, y=%d", 10, 20);

// GNU extension — __VA_OPT__ (C23)
#define debug(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
debug("hello");       // comma is swallowed
```

---

## # Operator — Stringification
===================================

```c
#define str(x) #x
str(hello)        → "hello"
str(x + y)        → "x + y"
```

---

## ## Operator — Token Concatenation
=====================================

```c
#define concat(a, b) a ## b
concat(var, 42)   → var42

#define MAKE_TYPE(name) \
    typedef struct concat(name, _t) { int x; } concat(name, _t);
MAKE_TYPE(point) → typedef struct point_t { int x; } point_t;
```

---

## Predefined Macros
============================

```c
__LINE__         → current line
__FILE__         → file name
__DATE__         → compilation date
__TIME__         → compilation time
__STDC__         → 1 (C compliant)
__STDC_VERSION__ → 199901L (C99), 201112L (C11)
__COUNTER__      → increments each use (GNU)
```

### gcLang Specific
```c
__GCLANG__       → 1 (if compiled with gcLang)
__GCLANG_VER__   → 100
```

Usage:
```c
#ifndef __GCLANG__
    #error "This code can only be compiled with gcLang"
#endif
```

---

## #pragma once
=================

```c
#pragma once   // file is included only once
// In gcLang, .gclib files are already loaded uniquely
```

---

## gcLang Constants
====================

```c
#define null  ((void*)0)
#define true  1
#define false 0
```

For constants like `NULL`, `EXIT_SUCCESS`, `EXIT_FAILURE`, `EOF` that come with C,
the relevant `.gclib` (stdio.gclib etc.) must be loaded.

---

## Macro or Function in gcLang?
=====================================

Macro advantage: type independent, inline
Function advantage: type safe, debuggable

```c
// macro — type independent but double evaluation risk
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// static inline — safe
static inline int max(int a, int b) { return a > b ? a : b; }