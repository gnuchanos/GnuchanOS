# GCL — Gnuchan C-Like Language (GNU99)

## Temel Felsefe

- **GNU99 tabanlı** — C99 standartı + GNU extensions
- **Güvenli C** — runtime safety eklenmiş, debug her zaman net
- **Sıfır maliyet** — debug kapalıyken hiçbir overhead yok

---

###### import ######################################################

```
#extern <file.dll>    // .dll, .so, .a — external C library
#lib    <file>        // .gclib textfile — GCL library module
#include <file>       // .gcsf — GCL source file

extern void my_c_func(int);     // C ABI declaration
```

```
import file, file, file            // single file link
import file as alias
from module import thing as alias
```

###### import ######################################################

---

###### 101 — Types #################################################

```
// ── Primitive Types ──────────────────────────────────────────
char               // 1 byte
short              // 2 byte
int                // 4 byte
long               // 4/8 byte (platform)
long long          // 8 byte
float              // 4 byte
double             // 8 byte
long double        // 10/16 byte (platform)

void               // no type

// ── GCL-Specific Fixed-Width ────────────────────────────────
int8               // signed 8-bit
int16              // signed 16-bit
int32              // signed 32-bit
int64              // signed 64-bit
int128             // signed 128-bit (GCC __int128)

// ── Unsigned ────────────────────────────────────────────────
unsigned char
unsigned short
unsigned int
unsigned long
unsigned long long

uint8              // unsigned 8-bit
uint16             // unsigned 16-bit
uint32             // unsigned 32-bit
uint64             // unsigned 64-bit

// ── GCL-Specific Unsigned Float/Double ─────────────────────
// Behaves like standard float/double but signals on NaN/Inf
unsigned float     // float with overflow guard
unsigned double    // double with overflow guard

// ── Boolean ────────────────────────────────────────────────
bool               // stdbool.h, values: true / false

// ── Size / Pointer ─────────────────────────────────────────
size_t             // sizeof result type
ssize_t            // signed size
intptr_t           // integer capable of holding pointer
uintptr_t          // unsigned variant
```

###### 101 — Types #################################################

---

###### 101 — Literals & Variables ##################################

```
// ── Characters ──────────────────────────────────────────────
char c = 'A';
char newline = '\n';
char tab = '\t';
char hex = '\x41';       // 'A'
char oct = '\101';       // 'A'

// ── Integers ────────────────────────────────────────────────
int dec = 42;
int hex = 0x2A;
int oct = 052;
int bin = 0b101010;      // GCC extension
unsigned long big = 1000000UL;

// ── Floats ──────────────────────────────────────────────────
float f = 3.14f;
double d = 3.14159;
long double ld = 3.141592653589793L;

// ── Strings (Stack) ─────────────────────────────────────────
char STR[32] = "hello world";        // 32 byte buffer, auto + \0
char str[] = "auto-sized";           // size = 12 (11 + \0)

// ── Strings (Heap) ──────────────────────────────────────────
char *s = strdup("heap string");     // POSIX, free() gerek
char *s2 = gc_malloc(64);            // GCL safe malloc (tracked)
strcpy(s2, "safe heap string");

// ── Constants ───────────────────────────────────────────────
const int MAX_PLAYERS = 4;
const char *NAME = "Gnuchan";

// ── Enums ───────────────────────────────────────────────────
enum Color { RED, GREEN, BLUE };
enum State { ON = 1, OFF = 0, UNKNOWN = -1 };
```

###### 101 — Literals & Variables ##################################

---

###### arrays ######################################################

```
// ── Stack Arrays ────────────────────────────────────────────
int numbers[5] = {10, 20, 30, 40, 50};
int matrix[3][4] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12}
};

char names[3][32] = {
    "Half Life 3",
    "Left 4 Dead 3",
    "Team Fortress 3"
};

// ── Heap Arrays ─────────────────────────────────────────────
int *list = gc_malloc(3 * sizeof(int));
list[0] = 10;
list[1] = 20;
list[2] = 30;

char **string_list = gc_malloc(3 * sizeof(char*));
string_list[0] = strdup("Half Life 3");
string_list[1] = strdup("Left 4 Dead 3");
string_list[2] = strdup("Team Fortress 3");

// ── Multi-Dimensional (Heap) ────────────────────────────────
int (*mat)[4] = gc_malloc(3 * sizeof(*mat));
mat[0][0] = 1;

// ── Variable Length Array (GNU99) ───────────────────────────
int n = 10;
int vla[n];                    // VLA, stack allocated
for (int i = 0; i < n; i++) {
    vla[i] = i * 2;
}

// ── Designated Initializers (GNU99) ─────────────────────────
int arr[10] = { [0] = 1, [5] = 2, [9] = 3 };
struct Point p = { .x = 10, .y = 20 };
```

###### arrays ######################################################

---

###### pointers ####################################################

```
// ── Basic Pointers ──────────────────────────────────────────
int x = 42;
int *p = &x;
*p = 99;               // x is now 99

// ── Null Protection (GCL Safe) ─────────────────────────────
int *ptr = gc_new(int);         // tracked allocation
gc_set(ptr, 42);                // safe write (null-checked)
int val = gc_get(ptr);          // safe read  (null-checked)

// ── Pointer Arithmetic (GCL Safe) ──────────────────────────
int *arr = gc_array(int, 10);   // tracked array
int *elem = gc_offset(arr, 5);  // bounds-checked offset
gc_set(elem, 123);

// ── Function Pointers ───────────────────────────────────────
int add(int a, int b) { return a + b; }
int (*op)(int, int) = &add;
int result = op(3, 4);          // 7

// ── void* ──────────────────────────────────────────────────
void *data = gc_malloc(64);
int *ip = (int*)data;           // explicit cast required
float *fp = (float*)data;

// ── Null Checks ────────────────────────────────────────────
if (!ptr) {
    gc_error("null pointer detected at %s:%d", __FILE__, __LINE__);
}

// ── Restrict (for optimization) ─────────────────────────────
void vector_add(int *restrict a, int *restrict b, int n) {
    for (int i = 0; i < n; i++) a[i] += b[i];
}
```

###### pointers ####################################################

---

###### operators ###################################################

```
// ── Arithmetic ──────────────────────────────────────────────
+  -  *  /       // standard
//              // integer division (floor for positive)
%               // modulo
**              // GCC extension: pow(a, b) via builtin
++ --           // pre/post increment/decrement

// ── Compound Assignment ─────────────────────────────────────
+=  -=  *=  /=  %=
<<=  >>=  &=  |=  ^=

// ── Comparison ──────────────────────────────────────────────
==  !=  <  >  <=  >=

// ── Logical ─────────────────────────────────────────────────
&&  ||  !       // short-circuit

// ── Bitwise ─────────────────────────────────────────────────
&  |  ^  ~  <<  >>

// ── Pointer ─────────────────────────────────────────────────
&  *             // address-of / dereference
->              // struct member via pointer
.               // struct member direct

// ── Ternary ─────────────────────────────────────────────────
int max = (a > b) ? a : b;

// ── Comma ───────────────────────────────────────────────────
for (int i = 0, j = 10; i < j; i++, j--) { }

// ── Type Cast ───────────────────────────────────────────────
(type)value
    float f = (float)42;
    int i = (int)3.14;
    void *p = (void*)0x1234;

// ── Alignment / Offset (GNU99) ─────────────────────────────
int off = __builtin_offsetof(struct S, member);
type *aligned = (type*)__builtin_assume_aligned(ptr, 16);
```

###### operators ###################################################

---

###### if / elif / else #############################################

```
if (x > 0) {
    printf("positive\n");
}

// ── GNU99 allows declaration in condition ───────────────────
if (int err = func()) {
    printf("error: %d\n", err);
}

// ── else if ─────────────────────────────────────────────────
if (score >= 90) {
    grade = 'A';
} else if (score >= 80) {
    grade = 'B';
} else if (score >= 70) {
    grade = 'C';
} else {
    grade = 'F';
}

// ── Ternary ─────────────────────────────────────────────────
result = (hp > 0) ? "alive" : "dead";
```

###### if / elif / else #############################################

---

###### switch / case ################################################

```
switch (value) {
    case 1:
        printf("one\n");
        break;
    case 2:
    case 3:
        printf("two or three\n");
        break;
    case 4 ... 8:               // GNU99 range extension
        printf("between 4 and 8\n");
        break;
    default:
        printf("other\n");
        break;
}

// ── GNU99 switch on any integer type ─────────────────────────
switch (enum_val) {
    case RED:   printf("red");   break;
    case GREEN: printf("green"); break;
    case BLUE:  printf("blue");  break;
}
```

###### switch / case ################################################

---

###### while ########################################################

```
int i = 0;
while (i < 10) {
    printf("%d\n", i);
    i++;
}

// ── Infinite loop ──────────────────────────────────────────
while (true) {
    // break to exit
}

// ── Do-While ────────────────────────────────────────────────
int j = 0;
do {
    printf("%d\n", j);
    j++;
} while (j < 10);
```

###### while ########################################################

---

###### for ##########################################################

```
// ── Classic ─────────────────────────────────────────────────
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}

// ── GNU99: declare in init ──────────────────────────────────
for (int i = 0; i < n; i++) { }     // i scoped to loop

// ── Multiple variables ──────────────────────────────────────
for (int i = 0, j = 10; i < j; i++, j--) { }

// ── Infinite (no condition) ─────────────────────────────────
for (;;) {
    // break to exit
}

// ── Array iteration (GCL extension) ─────────────────────────
for (int i = 0; i < gc_len(arr); i++) {
    printf("%d\n", gc_get(arr, i));
}

// ── Pointer iteration ───────────────────────────────────────
int arr[] = {1, 2, 3, 4, 5};
for (int *p = arr; p < arr + 5; p++) {
    printf("%d\n", *p);
}
```

###### for ##########################################################

---

###### loop control #################################################

```
for (int i = 0; i < 10; i++) {
    if (i == 3) {
        continue;           // skip iteration
    }
    if (i == 7) {
        break;              // exit loop
    }
    printf("%d\n", i);
}

// ── Nested loops, labeled break (GNU99) ─────────────────────
outer:
for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
        if (i + j > 15) {
            break outer;    // breaks both loops
        }
        printf("(%d,%d) ", i, j);
    }
}

// ── goto (use sparingly) ────────────────────────────────────
if (error) {
    goto cleanup;
}
cleanup:
    free(ptr);
    return -1;
```

###### loop control #################################################

---

###### Functions ###################################################

```
// ── Basic Function ─────────────────────────────────────────
int add(int a, int b) {
    return a + b;
}

// ── Void ────────────────────────────────────────────────────
void greet(const char *name) {
    printf("Hello, %s!\n", name);
}

// ── Multiple Returns via Out Parameters ─────────────────────
void divide(int a, int b, int *quot, int *rem) {
    *quot = a / b;
    *rem  = a % b;
}

// ── Static (file-scoped) ────────────────────────────────────
static int counter = 0;

static void helper(void) {
    // only visible in this file
}

// ── Inline (GNU99) ──────────────────────────────────────────
static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

// ── Nested Functions (GNU extension) ────────────────────────
void outer(void) {
    int inner(int x) {
        return x * 2;
    }
    printf("%d\n", inner(5));   // 10
}

// ── GCC Attributes ──────────────────────────────────────────
int __attribute__((pure)) compute(int x) {
    return x * x;
}

void __attribute__((noreturn)) fatal_error(void) {
    printf("FATAL\n");
    exit(1);
}

// ── Variadic ────────────────────────────────────────────────
#include <stdarg.h>
void logf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// ── GCL Safe Arithmetic ─────────────────────────────────────
int checked_add(int a, int b) {
    int result;
    if (__builtin_add_overflow(a, b, &result)) {
        gc_raise("integer overflow");
        return 0;
    }
    return result;
}

int checked_mul(int a, int b) {
    int result;
    if (__builtin_mul_overflow(a, b, &result)) {
        gc_raise("integer overflow");
        return 0;
    }
    return result;
}
```

###### Functions ###################################################

---

###### struct #######################################################

```
// ── Definition ──────────────────────────────────────────────
struct Player {
    char name[32];
    int hp;
    int max_hp;
    float speed;
    bool alive;
};

// ── Usage ───────────────────────────────────────────────────
struct Player p1 = { "Gordon", 100, 100, 5.0f, true };

// designated initializer (GNU99)
struct Player p2 = {
    .name = "Freeman",
    .hp = 50,
    .max_hp = 100,
    .speed = 3.5f,
    .alive = true
};

p1.hp = 75;
strcpy(p1.name, "Gordon Freeman");

// ── Pointer to struct ───────────────────────────────────────
struct Player *pp = &p1;
pp->hp = 90;                    // equivalent to (*pp).hp
(*pp).speed = 6.0f;

// ── Heap allocated struct (GCL safe) ────────────────────────
struct Player *gp = gc_new(struct Player);
gc_set_field(gp, hp, 100);      // bounds & null checked

// ── Nested structs ──────────────────────────────────────────
struct Vector2 { float x, y; };
struct Entity {
    struct Vector2 pos;
    struct Vector2 vel;
};

// ── Anonymous struct (GNU99) ────────────────────────────────
struct { int x; int y; } point = { 10, 20 };
typedef struct { int x, y; } Point;

// ── Flexible Array Member (GNU99) ───────────────────────────
struct Buffer {
    size_t len;
    char data[];                // flexible array (must be last)
};

struct Buffer *buf = gc_malloc(sizeof(struct Buffer) + 100);
buf->len = 100;
buf->data[0] = 'A';            // GCC ensures alignment
```

###### struct #######################################################

---

###### enum #########################################################

```
enum Color { RED, GREEN, BLUE };         // RED=0, GREEN=1, BLUE=2
enum State { ON = 1, OFF = 0, UNKNOWN = -1 };
enum Flags {
    FLAG_A = 0x01,
    FLAG_B = 0x02,
    FLAG_C = 0x04
};

// ── Anonymous + typedef ─────────────────────────────────────
typedef enum { SUNDAY, MONDAY, TUESDAY } Day;
Day today = MONDAY;

// ── GCL Type-Checked Enum ───────────────────────────────────
// GCL adds runtime tag-checking for enums (when debug enabled)
enum GCL_SAFE Color c = RED;
if (!gcl_enum_is_valid(c, COLOR_RED, COLOR_BLUE)) {
    gc_raise("invalid enum value");
}
```

###### enum #########################################################

---

###### typedef ######################################################

```
typedef unsigned long ulong;
typedef struct Point { int x, y; } Point;

typedef int (*MathOp)(int, int);    // function pointer type

typedef struct {
    char name[32];
    int age;
} Person;

// ── GCL safe typedef patterns ───────────────────────────────
typedef struct {
    GCL_HEADER;                     // magic + size metadata
    int x, y;
} GCL_Point;

// GCL_HEADER provides:
//   uint32_t magic;       // 0x47434C (ASCII "GCL")
//   size_t   alloc_size;  // total allocation size
//   const char *file;     // source file of allocation
//   int       line;       // source line of allocation
```

###### typedef ######################################################

---

###### sizeof / offsetof ############################################

```
size_t sz = sizeof(int);                // 4
size_t sa = sizeof(struct Player);      // struct size incl padding
size_t s = sizeof(p1);                  // variable

int off = __builtin_offsetof(struct Player, hp);

// ── GCL safe sizeof (tracked) ───────────────────────────────
size_t actual = gc_alloc_size(ptr);     // returns actual allocation size
// gc_alloc_size(ptr) >= sizeof(*ptr) for valid allocations
```

###### sizeof / offsetof ############################################

---

###### string functions #############################################

```
// ── Standard C ──────────────────────────────────────────────
#include <string.h>

char dst[32];
strcpy(dst, src);               // unsafe — GCL provides safe version
strncpy(dst, src, sizeof(dst)); // safer, but no null guarantee
strlcpy(dst, src, sizeof(dst)); // BSD, always null-terminates

size_t len = strlen(str);
int cmp = strcmp(a, b);
char *pos = strstr(haystack, needle);
char *token = strtok(str, delim);

// ── GCL Safe Strings ───────────────────────────────────────
char *s = gc_strdup("hello");           // tracked allocation
gc_strcpy_s(dst, sizeof(dst), src);     // bounds-checked copy
gc_strcat_s(dst, sizeof(dst), src);     // bounds-checked concat

// ── String Format ──────────────────────────────────────────
char buf[128];
snprintf(buf, sizeof(buf), "hp=%d score=%d", hp, score);

// ── String Length (safe) ───────────────────────────────────
size_t len = gc_strlen(s);              // null-checked strlen
if (gc_strlen(s) > 0) {
    printf("string: %s\n", s);
}
```

###### string functions #############################################

---

###### memory management ############################################

```
// ── Standard C (raw, minimal tracking) ──────────────────────
void *ptr = malloc(64);
ptr = realloc(ptr, 128);
free(ptr);

void *zero = calloc(1, 64);             // zero-initialized

// ── GCL Safe Memory System ─────────────────────────────────
// Her allocation kaydedilir: pointer, size, file, line, id
// Her free silinir / invalidate edilir

void *p = gc_malloc(64);                // tracked allocation
void *z = gc_calloc(1, 64);             // tracked + zeroed
void *r = gc_realloc(p, 128);           // tracked realloc
gc_free(r);                             // tracked free

// ── Type-safe helpers ──────────────────────────────────────
int *ip = gc_new(int);                  // gc_malloc(sizeof(int))
int *arr = gc_array(int, 10);           // gc_malloc(10 * sizeof(int))

int *clone = gc_clone_array(arr, 10 * sizeof(int));  // + check

// ── Ownership Transfer ─────────────────────────────────────
// gc_free() sonrası pointer NULL atanır (use-after-free koruması)
// Debug modunda: freed pointer kullanımı => detaylı rapor

// ── GC_MALLOC_DEBUG wrapper ────────────────────────────────
// Normal build:     gc_malloc = malloc
// Debug build:       gc_malloc = tracked_malloc + kayıt
// Release build:    gc_malloc = malloc (sıfır overhead)
```

###### memory management ############################################

---

###### runtime safety system ########################################

```
// ── GCL Runtime Safety ─────────────────────────────────────
// Üç katmanlı güvenlik:
//   1. Derleme zamanı: type checking, const correctness
//   2. Debug runtime:  bounds, null, overflow, use-after-free
//   3. Release:        opsiyonel lightweight guards

// ═══════════════════════════════════════════════════════════════
// MEMORY TRACKING TABLE
// ═══════════════════════════════════════════════════════════════
//
// struct GCL_AllocEntry {
//     void       *ptr;        // allocation address
//     size_t      size;       // allocation size
//     const char *file;       // source file
//     int         line;       // source line
//     int         id;         // unique allocation id
//     bool        freed;      // marked freed?
//     uint32_t    magic;      // integrity check
// };
//
// GCL_AllocEntry g_alloc_table[GCL_MAX_ALLOCS];  // global table
// int g_alloc_count;
//
// ═══════════════════════════════════════════════════════════════

// ── GCL Safe API ─────────────────────────────────────────────

// Allocation
void *gc_malloc(size_t size);               // tracked malloc
void *gc_calloc(size_t count, size_t size);  // tracked calloc
void *gc_realloc(void *ptr, size_t size);    // tracked realloc
void  gc_free(void *ptr);                   // tracked free + invalidate

// Dereference (bounds-checked)
int   gc_get_int(int *ptr);                 // null-checked read
void  gc_set_int(int *ptr, int val);        // null-checked write

void *gc_offset(void *base, size_t index, size_t elem_size);
    // Returns base + (index * elem_size), with bounds check.
    // Eğer index >= alloc_size / elem_size => gc_error()

// Length / Size
size_t gc_len(void *ptr);                   // alloc_size / elem_size
size_t gc_alloc_size(void *ptr);            // raw allocation size

// Null check
bool gc_is_valid(void *ptr);                // in table + not freed
bool gc_is_null(void *ptr);                 // pointer is NULL

// ── Conditional Compilation ───────────────────────────────────
// #ifdef GCL_DEBUG
//   #define gc_new(T)       gcl_debug_new(sizeof(T), __FILE__, __LINE__)
//   #define gc_malloc(s)    gcl_debug_malloc(s, __FILE__, __LINE__)
//   #define gc_free(p)      gcl_debug_free(p, __FILE__, __LINE__)
// #else
//   #define gc_new(T)       malloc(sizeof(T))
//   #define gc_malloc(s)    malloc(s)
//   #define gc_free(p)      free(p)
// #endif
```

###### runtime safety system ########################################

---

###### debug system #################################################

```
// ═══════════════════════════════════════════════════════════════
// DEBUG SYSTEM — Tam Anlaşılır Hata Raporları
// ═══════════════════════════════════════════════════════════════
//
// Amaç: ASLA segfault, ASLA anlamsız crash.
// Her hata => program adı, dosya, satır, fonksiyon, çağrı yığını
//
// ═══════════════════════════════════════════════════════════════

// ── Instruction ID Sistemi ───────────────────────────────────
// Her satıra derleme sırasında benzersiz InstructionID verilir:
//
//     ID 00001  →  main.gcsf:12       int x = foo();
//     ID 00002  →  main.gcsf:13       x += 10;
//     ID 00003  →  player.gcsf:81     hp -= damage;
//     ID 00004  →  player.gcsf:82     if (hp <= 0) ...
//
// Format: GCLID:<dosya>:<satır>:<sütun>:<fonksiyon>
// Örn:   GCLID:player.gcsf:81:5:TakeDamage

// ── Error Types ──────────────────────────────────────────────
//
// GCL_ERR_NONE              = 0
// GCL_ERR_NULL_POINTER      = 1    // NULL dereference
// GCL_ERR_BOUNDS            = 2    // array index out of bounds
// GCL_ERR_USE_AFTER_FREE    = 3    // freed memory access
// GCL_ERR_BUFFER_OVERFLOW   = 4    // stack buffer overflow
// GCL_ERR_INTEGER_OVERFLOW  = 5    // arithmetic overflow
// GCL_ERR_DIV_BY_ZERO       = 6    // division by zero
// GCL_ERR_INVALID_CAST      = 7    // invalid type cast
// GCL_ERR_MEMORY_LEAK       = 8    // unfreed allocation on exit (debug)
// GCL_ERR_DOUBLE_FREE       = 9    // freeing already freed pointer
// GCL_ERR_STACK_OVERFLOW    = 10   // recursion too deep

// ── Error Report Format ──────────────────────────────────────
//
// ╔══════════════════════════════════════════════════════════════╗
// ║                    GCL RUNTIME ERROR                        ║
// ╠══════════════════════════════════════════════════════════════╣
// ║  Type:    NULL POINTER DEREFERENCE                          ║
// ║  Code:    GCL_ERR_NULL_POINTER (1)                         ║
// ║                                                            ║
// ║  Location:                                                 ║
// ║    File:       player.gcsf                                  ║
// ║    Line:       81                                          ║
// ║    Column:     5                                           ║
// ║    Function:   TakeDamage                                  ║
// ║    Instruction ID: GCLID:player.gcsf:81:5:TakeDamage      ║
// ║                                                            ║
// ║  Pointer:     0x0000000000000000 (NULL)                    ║
// ║                                                            ║
// ║  Call Stack:                                               ║
// ║    0: main.gcsf:120        main()                          ║
// ║    1: player.gcsf:81       TakeDamage(Player*, int)        ║
// ║    2: player.gcsf:45       ApplyDamage(Player*, int)       ║
// ║                                                            ║
// ║  Allocation Info:                                          ║
// ║    (pointer is NULL — no allocation to display)            ║
// ║                                                            ║
// ║  Hint: Check if player pointer was initialized before      ║
// ║        calling TakeDamage().                               ║
// ╚══════════════════════════════════════════════════════════════╝

// ── Bounds Error Example ─────────────────────────────────────
//
// ╔══════════════════════════════════════════════════════════════╗
// ║                    GCL RUNTIME ERROR                        ║
// ╠══════════════════════════════════════════════════════════════╣
// ║  Type:    BUFFER OVERFLOW                                   ║
// ║  Code:    GCL_ERR_BOUNDS (2)                               ║
// ║                                                            ║
// ║  Location:                                                 ║
// ║    File:       inventory.gcsf                               ║
// ║    Line:       34                                          ║
// ║    Function:   AddItem                                     ║
// ║                                                            ║
// ║  Pointer:     0x00A3F7B0                                   ║
// ║  Allocated Size:  80 bytes  (5 items * 16 bytes)           ║
// ║  Attempted Offset: 96 bytes  (index 6, item size 16)      ║
// ║                                                             ║
// ║  Allocation Origin:                                        ║
// ║    File:    main.gcsf:22                                   ║
// ║    Function: InitInventory                                 ║
// ║                                                            ║
// ║  Call Stack:                                               ║
// ║    0: main.gcsf:55        main()                           ║
// ║    1: inventory.gcsf:34   AddItem(Inventory*, Item)        ║
// ║                                                            ║
// ║  Hint: Inventory was allocated for 5 items, but you tried  ║
// ║        to access index 6. Use gc_resize() to grow.         ║
// ╚══════════════════════════════════════════════════════════════╝

// ── Debug API ────────────────────────────────────────────────

// Hata fırlatma
void gc_raise(const char *fmt, ...);            // error + stack trace
void gc_assert(bool cond, const char *msg);     // assertion
void gc_panic(const char *msg);                 // unrecoverable error

// Debug log
void gc_debug(const char *fmt, ...);            // debug message
void gc_trace(const char *fmt, ...);            // trace message
void gc_warn(const char *fmt, ...);             // warning

// Debug modu kontrolü
bool gc_debug_enabled(void);                    // is GCL_DEBUG set?
void gc_set_debug_level(int level);             // 0=off, 1=error, 2=warn, 3=info, 4=trace

// Haşat raporu yazdırma
void gc_print_error_report(GCL_Error *err);     // formatted error
void gc_print_alloc_table(void);                // dump all allocations
void gc_print_stack_trace(void);                // current call stack

// ── Debug Thread ─────────────────────────────────────────────
// Opsiyonel: ayrı bir thread hataları dinler
// gcl_debug_thread_start()   → arka planda hata monitörü başlatır
// gcl_debug_thread_stop()    → durdurur
// gcl_debug_write_dump()     → crash dump dosyası oluşturur

// ── Debug Mode CLI Commands ──────────────────────────────────
// gcl -debug run                 Full debug mode (tüm kontroller açık)
// gcl -debug tokens              Lexer token çıktısı
// gcl -debug ast                 AST dump
// gcl -debug ir                  IR dump
// gcl -debug mem                 Memory allocation trace
// gcl -debug profile             Performance profiling
```

###### debug system #################################################

---

###### error reporting ##############################################

```
// ═══════════════════════════════════════════════════════════════
// ERROR HANDLING PATTERNS
// ═══════════════════════════════════════════════════════════════

// ── Pattern 1: Return Code (C classic) ──────────────────────
int result = do_something();
if (result < 0) {
    gc_error("do_something failed: %s", strerror(errno));
    return -1;
}

// ── Pattern 2: GCL Assert ───────────────────────────────────
gc_assert(ptr != NULL, "ptr must not be null");
gc_assert(index < gc_len(arr), "index out of bounds");

// ── Pattern 3: Checked Allocations ──────────────────────────
int *data = gc_array(int, 100);
if (gc_is_null(data)) {
    gc_panic("failed to allocate 100 ints");
}

// ── Pattern 4: Safe Dereference ─────────────────────────────
gc_set_int(ptr, 42);            // null-checked
int val = gc_get_int(ptr);      // null-checked

// ── Pattern 5: Bounds-Checked Access ───────────────────────
gc_array_set(data, 5, 42);      // bounds + null checked
int v = gc_array_get(data, 5);  // bounds + null checked

// ── Pattern 6: Overflow-Checked Arithmetic ─────────────────
int sum = gc_safe_add(a, b);    // overflow => error
int diff = gc_safe_sub(a, b);   // underflow => error
int prod = gc_safe_mul(a, b);   // overflow => error
int quot = gc_safe_div(a, b);   // div-by-zero => error
```

###### error reporting ##############################################

---

###### attribute & annotations ######################################

```
// ── GCC Attributes ──────────────────────────────────────────
// GCL uses GCC attributes to add safety info at compile time

// Non-null parameter
void process(int *ptr) __attribute__((nonnull(1)));

// Pure function (no side effects)
int square(int x) __attribute__((pure));

// Never returns
void abort(void) __attribute__((noreturn));

// Alignment
int aligned_arr[16] __attribute__((aligned(64)));

// Deprecated
int old_func(void) __attribute__((deprecated("use new_func instead")));

// ── GCL Extended Attributes ─────────────────────────────────
// GCL adds custom attributes for runtime safety

// __gcl_safe — adds runtime checks to function
int __gcl_safe get_element(int *arr, size_t idx);

// __gcl_checked — ensures bounds checking
void __gcl_checked array_set(int *arr, size_t idx, int val);

// __gcl_debug_only — only compiled in debug mode
void __gcl_debug_only dump_stats(void);

// __gcl_no_debug — never instrumented (hot paths)
void __gcl_no_debug fast_path(void);
```

###### attribute & annotations ######################################

---

###### preprocessor #################################################

```
// ── Standard C Preprocessor ─────────────────────────────────
#include <stdio.h>
#include "myheader.h"

#define MAX_BUFFER 1024
#define SQUARE(x) ((x) * (x))

#ifdef GCL_DEBUG
    #define SAFE_FREE(p) do { \
        gc_free(p);           \
        (p) = NULL;           \
    } while(0)
#else
    #define SAFE_FREE(p) free(p)
#endif

// ── GCL-Specific Defines ────────────────────────────────────
// GCL_DEBUG          — enable runtime safety checks
// GCL_DEBUG_MEM      — enable memory tracing
// GCL_DEBUG_PROFILE  — enable performance profiling
// GCL_RELEASE        — disable all checks (maximum speed)
// GCL_THREAD_SAFE    — enable thread-safe allocation tracking

// ── GNU99 Specific ──────────────────────────────────────────
// __STDC_VERSION__ >= 199901L  — C99 guaranteed
// __GNUC__                     — GCC extensions available
// __builtin_expect(cond, 1)    — branch prediction
// __builtin_trap()             — immediate crash (debug only)
// __builtin_unreachable()      — optimize unreachable paths

#if __STDC_VERSION__ >= 199901L
    #define C99_OR_LATER
#endif

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
    #define GNU_EXTENSIONS_AVAILABLE
#endif
```

###### preprocessor #################################################

---

###### gcl -debug CLI output examples ###############################

```
// ── gcl -debug run file.gcsf ────────────────────────────────
//
// ════════════════════════════════════════════════════════════
//  GCL DEBUG: SESSION START
// ════════════════════════════════════════════════════════════
//  File:     file.gcsf
//  Mode:     Full Debug
//  Memory:   0 allocations (0 bytes)
// ════════════════════════════════════════════════════════════
//
// [EXEC] GCLID:main.gcsf:1:1:main → int main(void)
// [EXEC] GCLID:main.gcsf:2:5:main →     int x = 42;
// [MEM ] alloc  ptr=0x00A3F7B0  size=4   file=main.gcsf:3  id=1
// [EXEC] GCLID:main.gcsf:3:5:main →     int *p = gc_new(int);
// [EXEC] GCLID:main.gcsf:4:5:main →     *p = x;
// [EXEC] GCLID:main.gcsf:5:5:main →     return 0;
//
// ════════════════════════════════════════════════════════════
//  GCL DEBUG: SESSION END
// ════════════════════════════════════════════════════════════
//  Memory Leak Check:
//    Allocations:  1
//    Freed:        1
//    Leaked:       0   ✓
// ════════════════════════════════════════════════════════════

// ── gcl -debug mem ───────────────────────────────────────────
//
// ════════════════════════════════════════════════════════════
//  GCL MEMORY TABLE
// ════════════════════════════════════════════════════════════
//  ID │ Ptr          │ Size  │ File:Line        │ Status
// ────┼──────────────┼───────┼──────────────────┼────────
//   1 │ 0x00A3F7B0  │    4  │ main.gcsf:3      │ active
//   2 │ 0x00A3F7C0  │  100  │ player.gcsf:12   │ active
//   3 │ 0x00A3F830  │   32  │ weapon.gcsf:45   │ freed
// ────┴──────────────┴───────┴──────────────────┴────────
//  Total: 136 bytes allocated, 104 bytes active
// ════════════════════════════════════════════════════════════
```

###### gcl -debug CLI output examples ###############################

---

###### performance ##################################################

```
// ═══════════════════════════════════════════════════════════════
// PERFORMANCE — Zero Overhead Design
// ═══════════════════════════════════════════════════════════════

// 1. RELEASE BUILD (gcl -build -o release)
//    gc_malloc = malloc  (tek bir fonksiyon çağrısı)
//    gc_free   = free    (tek bir fonksiyon çağrısı)
//    gc_assert = if() abort() veya tamamen boş
//    Debug kontrolleri: KOMPİLE EDİLMEZ
//    Overhead: %0 (sıfır)

// 2. DEBUG BUILD (gcl -debug run)
//    gc_malloc = tracked_malloc  (tabloya ekle)
//    gc_free   = tracked_free    (tablodan sil + invalidate)
//    gc_assert = if() gc_raise() (full report)
//    Her erişimde bounds/null check
//    Overhead: ~%5-15 (amaç debug, performans değil)

// 3. HYBRID BUILD (default)
//    gc_malloc = malloc (normal)
//    gc_free   = free   (normal)
//    gc_assert = if() fprintf(stderr) (minimal)
//    Sadece error path'lerde check
//    Overhead: <%1

// ── Makrolar ile koşullu derleme ─────────────────────────────
// #ifdef GCL_DEBUG
//     #define GC_CHECK(cond, msg) \
//         do { if (!(cond)) gc_raise(msg, __FILE__, __LINE__); } while(0)
// #else
//     #define GC_CHECK(cond, msg) ((void)0)   // nothing
// #endif

// #ifdef GCL_DEBUG_MEM
//     #define GC_TRACE_ALLOC(p, size) \
//         gcl_track_alloc(p, size, __FILE__, __LINE__)
// #else
//     #define GC_TRACE_ALLOC(p, size) ((void)0)  // nothing
// #endif
```

###### performance ##################################################

