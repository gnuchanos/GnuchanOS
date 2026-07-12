# FUNCTIONS
# gcLang / C99 / gnu99

## Functions in gcLang
=======================
Functions in gcLang use the same syntax as C.
Difference: gcLang uses `@extern` to call C library functions,
and runs its own functions in the IR executor.

### Basic Function
```c
int topla(int a, int b) {
    return a + b;
}

void selamla() {
    printf("merhaba\n");
}
```

### Prototype (Declaration)
```c
int topla(int a, int b);     // forward declaration

int main() {
    return topla(3, 4);
}

int topla(int a, int b) {
    return a + b;
}
```

---

## Parameter Passing
====================

### Call by Value (default)
```c
void increment(int x) {
    x++;                       // original unchanged
}
```

### Call by Reference (with pointer)
```c
void increment(int* x) {
    (*x)++;
}

int a = 5;
increment(&a);                 // a = 6
```

### Array Parameter
```c
int sum(int arr[], int n) {    // arr is actually a pointer
    int s = 0;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}
```

---

## return
==========

```c
int square(int x) {
    return x * x;
}

void early(int x) {
    if (x < 0) return;
    printf("%d\n", x);
}
```

WARNING — returning a local pointer:
```c
int* bad() {
    int x = 42;
    return &x;                 // ERROR: local variable is gone!
}

int* good() {
    int* p = malloc(sizeof(int));
    *p = 42;
    return p;                  // correct: lives on the heap
}
```

---

## static — File-Scope Function
=================================

```c
static int yardimci(int x) {   // visible only in this file
    return x * 2;
}
```

---

## Function Pointer
=====================

```c
int topla(int a, int b) { return a + b; }
int carp(int a, int b)  { return a * b; }

int (*islem)(int, int);
islem = topla;
printf("%d\n", islem(3, 4));   // 7

typedef int (*op_func)(int, int);
op_func f = carp;
printf("%d\n", f(3, 4));       // 12
```

---

## Calling C Functions with @extern (gcLang-Specific)
===================================================

In gcLang, functions in C DLL/.so/.a are called via `@extern`:

```c
@extern <raylib.dll>
extern "c" {
    void InitWindow(int width, int height, const char* title);
    void CloseWindow();
    int WindowShouldClose();
    void BeginDrawing();
    void EndDrawing();
    void ClearBackground(int color);
}
```

Usage:
```c
int main() {
    InitWindow(800, 600, "gcLang");
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(0xFFFFFF);
        EndDrawing();
    }
    CloseWindow();
}
```

---

## Functions in gcLang IR
=============================

- Function body is converted from AST to a linear IR instruction list
- Each function call opens a new frame (ir_frame.c)
- Parameters are placed into frame slots
- On return, the old frame is restored
- In IR: `CALL` (gcLang function), `CALL_EXTERN` (C DLL function)