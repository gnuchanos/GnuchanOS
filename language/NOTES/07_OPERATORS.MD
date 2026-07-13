# OPERATORS
# gcLang / C99 / gnu99

## Arithmetic
=============

```c
a + b   // addition
a - b   // subtraction
a * b   // multiplication
a / b   // division
a % b   // modulus (remainder)
```

## Comparison
=================

```c
a == b   // equal to
a != b   // not equal to
a < b    // less than
a > b    // greater than
a <= b   // less than or equal to
a >= b   // greater than or equal to
```

## Logical
=============

```c
a && b   // AND (short-circuit)
a || b   // OR  (short-circuit)
!a       // NOT
```

## Bitwise
==============

```c
a & b    // AND
a | b    // OR
a ^ b    // XOR
~a       // NOT (bitwise complement)
a << 2   // left shift
a >> 2   // right shift
```

## Assignment
=========

```c
a = b    // assignment
a += b   // a = a + b
a -= b   // a = a - b
a *= b   // a = a * b
a /= b   // a = a / b
a %= b   // a = a % b
a &= b   // a = a & b
a |= b   // a = a | b
a ^= b   // a = a ^ b
```

## Increment/Decrement
==================

```c
a++   // post-increment
++a   // pre-increment
a--   // post-decrement
--a   // pre-decrement
```

## Ternary
===========

```c
int max = (a > b) ? a : b;
```

## sizeof
==========

```c
sizeof(int)                     // 4
sizeof(arr) / sizeof(arr[0])    // number of elements
```

## Address/Content
================

```c
&x         // address-of
*p         // dereference
p->member  // struct pointer member access
arr[i]     // index (same as *(arr + i))
```

---

## Operators in gcLang IR
============================

```
Arithmetic:     ADD, SUB, MUL, DIV, MOD
Comparison:     CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE
Logical:        NOT, NEG
Pointer:        ADDR_OF, LOAD_INDIRECT, STORE_INDIRECT, MEMBER_PTR, INDEX_PTR
Conversion:     CAST, SIZEOF_CONST