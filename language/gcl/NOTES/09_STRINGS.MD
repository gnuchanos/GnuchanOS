# STRINGS
# gcLang / C99 / gnu99

## Strings in gcLang
===================
In gcLang, strings are null-character (`\0`) terminated char arrays, just like in C.

```c
"hello"    // char[6]: 'h','e','l','l','o','\0'

char s1[] = "hello";              // stack (automatic size)
char* s2 = "hello";               // string literal (READ ONLY!)
char* s3 = malloc(6);             // heap
       strcpy(s3, "hello");       //   → copy
```

## gcLang String Types
=========================
```c
// dynamic stack string
char string[] = "";
// runtime safety required (to prevent program crashes)

// dynamic heap string
char* string = "";

// static stack string
char string[64] = "welcome to gnuchan language";
```

## String Operations
====================
`string.h` functions provided by `#lib <stdlib>`:

```c
strlen("hello");                  // 5

char dst[10];
strcpy(dst, "hello");             // copy

char buf[20] = "hello ";
strcat(buf, "world");             // concatenation

strcmp("abc", "abc");             // 0 (equal)

strchr("hello", 'e');             // pointer to "ello"
strstr("hello world", "wor");     // pointer to "world"
```

## String Safety
====================

```c
char buf[5];
strncpy(buf, "hello world", sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';

// or snprintf (recommended)
char buf[64];
snprintf(buf, sizeof(buf), "deger: %d", 42);
```

## Note
========
In gcLang, the `str(v)` macro casts to `(char*)(v)`.
There is no `#include <string.h>`, instead `string.h` functions
come from the `.gclib` loaded via `#lib <stdlib>`.