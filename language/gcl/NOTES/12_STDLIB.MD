# gcLang STANDARD LIBRARY — STANDARD LIBRARY
# .gclib modules loaded with @lib <x> in gcLang

═══════════════════════════════════════════════════════════════════════
gcLang Standard .gclib Modules
═══════════════════════════════════════════════════════════════════════

  @lib <stdio>     → printf, sprintf, scanf, fopen, fclose, etc.
  @lib <stdlib>    → malloc, free, realloc, calloc, atoi, rand, etc.
  @lib <string>    → strlen, strcpy, strcat, strcmp, memcpy, memset
  @lib <memory>    → arena / memory management (gcLang specific)
  @lib <math>      → sin, cos, sqrt, pow, floor, ceil (C math.h)
  @lib <time>      → time, clock, sleep (C time.h)
  @lib <raylib>    → raylib graphics library binding

  Note: Each module corresponds to a .gclib file.
  Their paths are specified in build.gcdata / run.gcdata.

═══════════════════════════════════════════════════════════════════════
Usage Examples
═══════════════════════════════════════════════════════════════════════

  @lib <stdio>
  @lib <stdlib>

  int main() {
      printf("merhaba gcLang\n");

      int* p = malloc(sizeof(int) * 10);
      if (!p) return 1;

      p[0] = 42;
      printf("%d\n", p[0]);

      free(p);
      return 0;
  }

═══════════════════════════════════════════════════════════════════════
#include Not Needed — gcLang Built-ins
═══════════════════════════════════════════════════════════════════════

  #include <stdint.h> not needed:
      i8(v), u8(v), i16(v), u16(v), i32(v), u32(v), i64(v), u64(v)
      f32(v), f64(v), ch(v), str(v), b8(v)

  #include <stdbool.h> not needed:
      bool, true, false

  #include <stddef.h> not needed:
      NULL, null, sizeof