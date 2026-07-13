# COMMENT LINES — COMMENT SYNTAX
# C99 / gnu99 examples — for gcLang base

## // — C99 single-line (gcLang supports)
```c
// this is a comment
int x = 5; // end of line comment
```

## /* */ — C89 multi-line (gcLang supports)
```c
/* single line block */
/*
   multi-line
   comment
*/
int y = /* comment in the middle */ 10;
```


## gcLang additional comment styles
```c
--[[ Lua style comment ]]
{- ML/Haskell style comment -}
<!-- HTML style comment -->
{ Pascal style comment }
(* Pascal/ML style comment *)
""" Python/docstring style comment """
```

## C99 — #if 0 block commenting (alternative comment)
```c
#if 0
    this whole section is
    disabled
    // even comments here don't work
#endif
```

## C99 — __func__ (function name, debug-like comment)
```c
#include <stdio.h>
void test(void) {
    printf("this function: %s\n", __func__); // "test"
}
```

## gnu99 — \ multi-line string with comment
```c
// \ continuing comment at end of line
// this also \
    continues like
```

## comparison: comment types
```
gcLang    C/gnu99     description
------    -------     --------
//        //          C99 single-line
/* */     /* */       C89 multi-line
#         —           gcLang specific
--[[ ]]   —           gcLang specific
{- -}     —           gcLang specific
%         —           gcLang specific
""" """   —           gcLang specific