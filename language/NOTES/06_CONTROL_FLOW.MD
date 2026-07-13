# CONTROL FLOW
# gcLang / C99 / gnu99

## if / else if / else
======================

```c
if (x > 0) {
    printf("pozitif\n");
} else if (x == 0) {
    printf("sifir\n");
} else {
    printf("negatif\n");
}

// single line
if (x > 0) printf("pozitif\n");
```

## switch / case / default
===========================

```c
switch (x) {
    case 1:
        printf("bir\n");
        break;
    case 2:
    case 3:
        printf("iki veya uc\n");
        break;
    default:
        printf("diger\n");
        break;
}
```

## for
======

```c
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}

for (;;) {
    // break to exit
}
```

## while
========

```c
int i = 0;
while (i < 10) {
    printf("%d\n", i);
    i++;
}

while (1) {
    // break to exit
}
```

## do / while
=============

```c
int i = 0;
do {
    printf("%d\n", i);
    i++;
} while (i < 10);
```

## break / continue
===================

```c
for (int i = 0; i < 10; i++) {
    if (i == 5) break;             // breaks the loop
    if (i % 2 == 0) continue;      // skip this iteration
}
```

## goto
=======

```c
int* p = malloc(100);
if (!p) goto cleanup;
// ...
cleanup:
    free(p);
    return -1;
```

---

## Control Flow in gcLang IR
===============================

```
if       → JMP_IF_FALSE + JMP
while    → JMP_IF_FALSE + JMP
for      → init + JMP_IF_FALSE + body + JMP
break    → JMP (to loop end label)
continue → JMP (to loop start label)
switch   → comparison chain (jump table in future)