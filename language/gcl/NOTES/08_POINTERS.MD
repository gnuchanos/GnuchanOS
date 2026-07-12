# POINTERS
# gcLang / C99 / gnu99

## Basic Pointer
=================

```c
int x = 42;
int* p = &x;        // p holds the address of x

printf("%d\n", *p); // dereference → 42
*p = 100;           // x is now 100
```

## Pointer and const
====================

```c
const int* p1 = &x;   // value is const, pointer can change
int* const p2 = &x;   // pointer is const, value can change
```

## Pointer Arithmetic
======================

```c
int arr[5] = {10, 20, 30, 40, 50};
int* p = arr;

p + 1          // next int (address + 4)
*(p + 2)       // same as p[2]
p[3]           // same as *(p + 3)
```

## NULL Pointer
================

```c
int* p = NULL;
if (p != NULL) {
    *p = 42;    // safe access
}
```

## void* — Generic Pointer
===========================

```c
void* p = malloc(64);
int* ip = (int*)p;           // cast required
*ip = 42;
```

## Function Pointer
=====================

```c
int topla(int a, int b) { return a + b; }
int (*op)(int, int) = topla;
int r = op(3, 4);            // 7
```

---

## Pointers in gcLang
=====================
Pointers in gcLang work the same as in C:

```c
type* p = malloc(sizeof(type));
*p = 42;
free(p);

type* arr = calloc(5, sizeof(type));
free(arr);
```

Currently gcLang only has manual pointers.
Automatic pointers (GC) will be added in the future.

Pointer operations in IR:
- `ADDR_OF` → get address (&x)
- `LOAD_INDIRECT` → dereference read (*p)
- `STORE_INDIRECT` → dereference write (*p = x)
- `MEMBER_PTR` → struct member address (p->x)
- `INDEX_PTR` → array index (arr[i])