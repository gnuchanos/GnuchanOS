# GCL

GCL is GnuchanOS's own language. It is C with a few extra things bolted on: `gcChar`
strings that actually carry their length, sized types whose width is written in the name
(`int8`, `float32`), a printf that works with `{}`, and native modules pulled in with
`#native <Name>`.

There is no compiler backend. `SharedPipeline` (lexer + parser) turns the source into an
AST, and `GCL/SimpleRunner` walks that tree and executes it. So it is an interpreter. That
single decision explains most of the quirks below: values live in a `double` at runtime,
and the declared type only exists to clamp the value on the way in. Keep that in mind and
the overflow behaviour stops being surprising.

Source files use the `.gcsf` extension.

---

## Running it

The build chain lives in `language/makefile.py`. One command does everything:

```
gcl language
```

That pulls the dependencies into `_temp/<os>/` (Lua, the Python embed, Raylib, Raygui,
FreeFont) and produces this tree under `language/build/<os>/`:

```
build/<os>/
    gcl(.exe)
    Library/
        Math.(dll|so)
        Stdio.(dll|so)
        Embed.(dll|so)
        Raylib.(dll|so)
        Raygui.(dll|so)
        Embeded/
            Lua_Runtime/
                lua, LuaRaylib, LuaRaygui
            Python_Runtime/
                Python, PyRaylib, PyRaygui, Python/ (stdlib), gcl.pyd
    Programs/
        ide.(dll|so)
    Assets/
        FreeMono.ttf
```

`gcl.exe` is deliberately small — it only contains the GCL language itself. The IDE is a
separate library (`Programs/ide`) and the Lua/Python runtimes are loaded dynamically out
of `Library/Embed`, so neither of them inflates the CLI binary.

The command line:

```
gcl -ide [file]
gcl -debug -run file.gcsf
gcl -luarun file.lua
gcl -pyrun file.py
gcl -build project.gcdata [-o dir]
gcl -new path [--lua] [--luaraylib] [--python] [--pyraylib]
gcl -pyhost <port>
```

With no arguments it opens the IDE. Unless the executable finds a `.gcBundle` sitting next
to itself — that is a project produced by `-build`, and in that case it runs the
`main.gcsf` inside the bundle instead of opening the IDE.

`-run` forwards extra arguments to the script:

```
gcl -run game.gcsf first second
```

Those land in `main(argc, argv)`. Without `-debug` the runtime stays quiet; errors still go
to stderr, but the trace output is suppressed.

---

## Comments

Two styles, and you can mix them.

```
// C comment
/* multi
   line */

# GCL comment (to end of line)

#|
    GCL block comment
    |#
```

The `#| ... |#` form exists on purpose. The preprocessor treats any line starting with `#`
as a directive, so the block comment needed its own marker to stay out of that path. It can
be indented; the preprocessor recognises and skips it before the directive check.

---

## Preprocessor

Lines beginning with `#` are handled before the lexer ever sees them. The order is: read a
line, consult the conditional stack, apply `#define` expansion, write the result into a
buffer. That buffer is what finally gets lexed.

Supported directives:

| Directive | What it does |
| --- | --- |
| `#include <x.gcsf>` / `#include "x.gcsf"` | inlines the file's contents through the same preprocessor - **only when the `#if` branch it sits in is active**; an unresolvable include is FATAL (exit 1) |
| `#define NAME` / `#define NAME value` | object-like macro |
| `#define F(a,b) ...` | function-like macro, arguments substituted |
| `#undef NAME` | removes a macro |
| `#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif` | conditional compilation |
| `#warning ...` | yellow line on stderr; compilation continues |
| `#error ...` | red line on stderr and **compilation STOPS** (exit 1, nothing runs) |
| `#debug ...` | blue line on stderr; compilation continues |
| `#native <Name>` | adds a module to the load list |
| `#extern <file.dll>` | loads an external library |
| `#register <ret> <fn>(<params>);` | declares an external function signature |
| `#pragma commandline` | puts the program into terminal-app mode |
| `#pragma once` | in an INCLUDED file: include it at most once per run (opt-in guard) |

### #include

Both `<...>` and `"..."` work and there is no difference between them. The search order is:

1. the directory of the file being run,
2. `<project>/include`,
3. `<project>/lib`.

`#include` means "paste here", not "link". Including the same header from two files embeds
its contents twice — that is C's behaviour and it is deliberate (table generators and
X-macro style patterns depend on it). For an include-once guard, put `#pragma once` in the
INCLUDED file: the guard is keyed on the resolved path, is reset for every run, and has no
fixed limit on how many distinct guarded files it remembers.

An `#include` is resolved and pasted only when the `#if` branch it sits in is ACTIVE: a
disabled include is not looked up and its contents are not pasted, exactly as in C. When the
branch IS active and the file cannot be found - or nesting passes 32 levels - the build
STOPS: the diagnostic names the file (and the directories searched), a plain
`Error: #include failed, compilation stopped` line follows it, no statement runs (stdout
stays empty) and the process exits with code **1**. Golden tests:
`language/tests/gcsf/preproc/include_inactive.gcsf`, `include_inactive_missing.gcsf`,
`include_missing_active.gcsf`, `include_cycle.gcsf`.

### #define

```
#define VERSION 1
#define APP_NAME "GnuchanOS"
#define MAX(a, b) ((a) > (b) ? (a) : (b))
```

Macro expansion does not touch the inside of string literals, which is the sane behaviour.
In a function-like macro the arguments are split on top-level commas with paren/bracket/brace
depth tracked, so `F(g(1,2), 3)` splits correctly.

The `#if` expression is evaluated by a small Pratt parser of its own. It handles
`|| && == != < > <= >= + - * / %`, parentheses, `!`, `~` and unary `-`. Both `defined X`
and `defined(X)` work. A macro whose value is not a plain number counts as 1 in a condition,
and something never defined counts as 0.

```
#if VERSION == 1
    ...
#elif VERSION == 2
    ...
#else
    ...
#endif
```

`#error` **stops the build**, exactly like C: the red line is printed to stderr, a plain
`Error: #error directive stopped compilation` line follows it (greppable in CI), **no
statement runs** — stdout stays empty even for a `printf` that appears before the directive —
and the process exits with code **1**.

A `#error` inside a FALSE branch never fires, which is the whole point of guarding a
directive with `#if`: a platform-specific `#error` stays quiet everywhere else. An ACTIVE
`#error` inside an `#include`d file stops the WHOLE compilation, not just that fragment, and
reports the fragment's own message so you can see which file failed.

`#warning` and `#debug` are diagnostics only: they print and compilation continues.

Golden tests for all four behaviours: `language/tests/gcsf/preproc/error_directive.gcsf`,
`error_in_include.gcsf`, `error_under_true_if.gcsf`, `error_inactive_and_warning.gcsf`.

### #native

```
#native <Stdio>
#native <Math>
```

The module name is collected and loaded when the program starts. The lookup order is:

1. `<dir of gcl.exe>/Library/<Name>.(dll|so)`
2. `<script dir>/Library/<Name>.(dll|so)`
3. `Library/<Name>.(dll|so)` (working directory)
4. `PATH`

A module only has to export one symbol: `gcl_<lowercase-name>_get_functions`, returning an
array of `GclNativeEntry`. Writing your own `.so`/`.dll` therefore looks like this:

```c
#include "gcl_module.h"

static double fn_hello(int argc, const char **argv) {
    printf("hello from %s\n", argc > 0 ? argv[0] : "nobody");
    return 0.0;
}

static const GclNativeEntry g_entries[] = {
    {"hello", fn_hello},
};

GCL_EXPORT const GclNativeEntry *gcl_mymodule_get_functions(int *count) {
    *count = 1;
    return g_entries;
}
```

Every native function has the signature `double f(int argc, const char **argv)`. Arguments
arrive as **strings** — if you need a number, parse it yourself with `atof`/`strtod`. There
are no named arguments at this level, only position. `Stdio.writeFile(file="a", text="b")`
reaches the module as `"a"` then `"b"`; the `file=` labels are cosmetic.

### #extern and #register

This is how you call plain C functions from GCL. Two lines are needed:

```
#extern <raylib.dll>

#register void InitWindow(int width, int height, const char *title);
```

Then `InitWindow(800, 600, "Window");` works.

Honestly, the support here is **limited**. Parameter types are resolved by a rough string
match (`char` → string, `int`/`float`/`long` → int, `double` → double), and the actual call
is hand-written for a short list of shapes:

- return `void`: 0, 1 or 2 parameters, with a special case for `(int,int)` and `(int,int,char*)`,
- return `int`: 0 or 1 int parameter,
- return `double`: 1 double parameter.

Anything else returns 0. So this is not a general FFI — it exists to wire up raylib quickly.
If you are writing a real native module, go through `#native` instead; there is no signature
problem there because everything is passed as strings.

### #pragma commandline

```
#pragma commandline
#native <Stdio>

int main() {
    int n = 0;
    Stdio.printf("Number: ");
    Stdio.scanf(n);
    Stdio.printf("You entered: {}\n", n);
    return 0;
}
```

The IDE is a GUI application and launches child processes without a console. In that
situation a `scanf` would block forever waiting for input. If the pragma is present, the
program re-launches itself in the operating system's terminal. It also sets `GCL_TERMINAL=1`,
so any Lua/Python runtime started afterwards opens its own separate window.

Spelling variants are accepted: `commandline`, `commendline` (that was the original spelling
in the example file, so I kept it working), and `cmdline`.

---

## Types

The C types you would expect are all there:

```
char, short, int, long, float, double
long int, long long int, long double
unsigned int
```

On top of those, GCL adds sized types whose width is part of the name:

```
int8    int16    int32    int64    int128
uint8   uint16   uint32   uint64   uint128
float16 float32  float64  float128
bool    void
```

These live in a hard-coded word list inside the lexer, which means you cannot use them as
identifiers — `int int32 = 5;` will not parse.

Numeric literals are C-like. Decimal (`42`), hexadecimal (`0x1F`) and fractional (`1.5`)
forms all work, and so does scientific notation: `1e30`, `1.5E-3`, `2e+8`. The exponent
marker is read only when a digit follows the optional sign, so a typo such as `1else` is
still reported as a literal welded to a name instead of being swallowed as a truncated
exponent. The `l`/`L`, `u`/`U` and `f`/`F` suffixes come after the exponent (`1e3f`).

`gcChar` is a different beast: a real UTF-8 string. Not a pointer into a byte buffer, but a
value that carries its own contents.

```
gcChar text = "a rather long line of text";
```

### Overflow behaviour

Here is a quirk worth knowing early. The runtime stores every value in a `double`, but it
clamps to the declared type on assignment:

```
int8 a = 300;          // 300 -> 44 (signed char)
uint8 b = -1;          // 255
bool c = 42;           // 1
float32 d = 1.0 / 3.0; // collapses to float precision
```

However `int128`, `uint128` and `float128` are **not** really 128-bit. `int128` and `uint128`
are clamped to `long long`, and `float128` to `double`. The "128" in the name currently just
means "the bigger one". Real 128-bit arithmetic would require dropping the `double`
foundation, so it stays on the list.

There is also a special path for `uint64`/`uint128` variables and arrays: the original
literal text is kept alongside the number, because `double` rounds large unsigned values.
That is why `uint64 m = 18446744073709551615;` prints correctly, and why
`uint128 n = 12345678901234567890;` does too even though both are far past the 2^53 where a
`double` stops counting by ones. The mirror is kept only when the literal is a plain run of
decimal digits that actually FITS the type, so a literal the type cannot hold prints the
value really stored instead of digits it never had (`expressions/uint64_literal_print.gcsf`
pins both routes). Put the value into arithmetic and it rounds like any other double.

---

## Variables

Declaration is C-like:

```
int count = 10;
char c = 'A';
char *p = "text";
char buf[7] = "World";
gcChar s = "a longer string";
```

### const, global, local, inline

```
const int LIMIT = 100;
```

`const` is enforced at runtime: assigning to it prints an error and evaluates to 0. It is not
a compile-time check — you find out while running.

`global` and `local` are a bit unusual:

```
int counter = 0;

int bump() {
    global counter;      // bind to the outer counter
    counter = counter + 1;
    return counter;
}

int scratch() {
    local base;          // owned by this call, freed on return
    base = 5;
    return base;
}
```

Writing `global x;` inside a function means "bind to the `x` defined outside". If it does not
exist yet, it is created at 0 and kept after the call returns. Ordinary locals created inside
a function are cleaned up when the function returns.

You can also write a type-less list:

```
global first, second, third;
local scratch_slot;
```

`inline` exists but is a no-op; it is kept only so the LSP recognises it.

`public` and `private` are likewise no-ops. Writing `private` on a variable does not hide it,
so do not treat it as an access-control feature. It is a visibility tag for the LSP.

---

## Operators

```
+  -  *  /  %
++  --
+=  -=  *=  /=
=  ==  !=  <  >  <=  >=
```

`=>` also lexes as `>=`. That was intentional; if you ever wonder why a stray `=>` slips
through, this is why.

Bit and boolean operators come in both symbolic and word form:

| Symbol | Word | Meaning |
| --- | --- | --- |
| `&&` | `AND` | logical and |
| `\|\|` | `OR` | logical or |
| `^` | `XOR` | bitwise xor |
| `~` | `NOT` | bitwise not |
| `!` | | logical not |
| `<<` | `LEFT_SHIFT` | shift left |
| `>>` | `RIGHT_SHIFT` | shift right |
| `&` | `BIT_AND` | bitwise and |
| `\|` | `BIT_OR` | bitwise or |

One caveat: `!` is logical not and `~` is bitwise not, which is correct — but the word `NOT`
resolves to `~` (bitwise), not `!`. So `NOT x` flips the bits. This is easy to trip over.

The unary `&` operator means address-of:

```
Raylib.UpdateCamera(&camera, Raylib.CAMERA_FREE);
```

Its only real job is to flatten a struct's members when passing it to a native module. There
is no actual pointer involved.

### Naming operators with typedef

An interesting feature: you can give operators names with `typedef`.

```
typedef && AND;
typedef || OR;
typedef ^ XOR;
typedef ~ NOT;
typedef << LEFT_SHIFT;
typedef >> RIGHT_SHIFT;
typedef & BIT_AND;
typedef | BIT_OR;
```

After these lines, `AND`, `OR` and friends behave like real operators:

```
number a = 10;
number b = 30;
number xor_result = a XOR b;
number not_result = NOT a;
```

The parser rewrites the names to operators, which reads nicely. Note that you cannot then use
`AND` as a variable name, since the parser treats it as an operator. Something like `AND2` is
fine.

---

## Arrays and strings

```
int8 numbers[5] = {1, 2, 3, 4, 5};
char letters[5] = {'A', 'B', 'C', 'D', 'E'};
char greeting[14] = "Hello, World!";
char names[3][20] = { "Ali", "Veli", "Ayse" };
```

`char names[3][20]` is a two-dimensional char array, which in practice is an array of
strings — three slots of 20 characters. Arrays of `int8`/`int16` and the rest work the same
way.

To get the element count you use `sizeof`:

```
int n = sizeof(numbers) / sizeof(numbers[0]);
```

That works, but **not for the C reason**. Here `sizeof(array)` returns the element count and
`sizeof(array[0])` returns 1, so 5/1 = 5. In C the same expression is 5*1/1 = 5, so the end
result matches, but `sizeof(array)` alone means "5 elements" rather than "5 bytes". Worth
making consistent at some point; for now the `sizeof/sizeof` idiom is correct.

`sizeof("text")` really is `strlen + 1`. And `sizeof(int)` and other type queries return real
byte sizes (int → 4, double → 8, int128 → 16, and so on).

Assigning to an element works:

```
numbers[0] = 99;
greeting[0] = 'X';   // writes a single char, does not truncate the string
```

Indexing a string like `greeting[2]` gives you the character code.

---

## struct, typedef, enum

Familiar C constructs, plus a few conveniences.

```
struct Person {
    int id;
    int age;
    char name[20];
};

typedef struct {
    int id;
    int age;
    char name[20];
} User;
```

### Ways to build a struct

Positional:

```
struct Person p = { 1, 25, "Ali" };
```

Designated, when you only want to set some fields:

```
User u = {
    .id = 2,
    .name = "Veli"
};
```

Empty, then fill in later:

```
User g = {};
g.id = 3;
g.name = "Gamma";
```

Copy from another struct:

```
User copy = u;
```

Return one from a function:

```
User make() {
    User u = { 7, 20, "Seven" };
    return u;
}

User got = make();
```

Returning a bare init list — `return { 1, 2, "x" }` — also works.

Arrays of structs and nested structs are supported:

```
struct Person people[3] = {
    {1, 20, "Ali"},
    {2, 25, "Veli"},
    {3, 30, "Ayse"}
};

int n = sizeof(people) / sizeof(people[0]);
for (int i = 0; i < n; i++) {
    Stdio.printf("{} {} {}\n", people[i].id, people[i].age, people[i].name);
}
```

Member assignment is clamped to the declared type, so writing 300 to an `int8` member gives
you 44, not 300.

### enum

```
enum Day { MONDAY, TUESDAY, WEDNESDAY };
enum Day today = MONDAY;
```

Values start at 0 and increase in order. Setting an explicit starting value (`MONDAY = 5`)
is not supported right now.

The `typedef enum { ... } X;` form works too.

### Passing structs to native modules

This deserves its own paragraph, because it is the only bridge between GCL's struct system
and the native modules.

When you hand a struct variable to a native function, its members are unrolled in declaration
order and passed as arguments:

```
Raylib.Camera3D camera = { ... };  // position.x,y,z target.x,y,z up.x,y,z fovy projection
Raylib.BeginMode3D(camera);        // one argument -> eleven arguments
```

Nested structs are flattened recursively, so sub-structs like `Raylib.Vector3` unroll in the
correct order. Writing `&camera` does the same thing.

The consequence: if you change the field order of a raylib struct, the native side breaks.
Field order is argument order. That is the contract.

---

## Functions

```
void greet() {
    Stdio.printf("hello\n");
}

int add(int a, int b) {
    return a + b;
}

User makeUser() {
    User u = { 1, 20, "Ali" };
    return u;
}
```

Return type `void` means no value comes back. Everything else returns the value of the
`return` expression, and a struct return is copied out.

Functions are collected before the body runs, so a function can be called before it is
defined in the file. `main` is called automatically at the end, if present.

`main` can take `argc`/`argv` and those are filled from the command line:

```
int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        Stdio.printf("index: {} - arg: {}\n", i, argv[i]);
    }
    return 0;
}
```

Function arguments are bound by value. String arguments are copied into the parameter, so
modifying a string parameter does not affect the caller's variable.

---

## Control flow

```
if (a > b) {
    ...
} else if (a == b) {
    ...
} else {
    ...
}

while (count < 10) {
    count++;
}

for (int i = 0; i < n; i++) {
    ...
}

switch (mode) {
    case 1:
        ...
        break;
    default:
        ...
        break;
}
```

`break` and `continue` work as expected. `switch` requires `break` to fall out of a case; it
does not break automatically, same as C.

There is a fail-safe against runaway loops: if a `while` or `for` runs more than a million
iterations it aborts with "possible infinite loop". If you genuinely need a longer loop, that
guard is what stops it.

`for` loop initializers are block-scoped, matching C — the loop variable is removed from the
environment when the loop exits, unless a variable of the same name already existed outside.

---

## printf and scanf

Output goes through `Stdio.printf`, which uses `{}` placeholders rather than `%`:

```
Stdio.printf("name: {} age: {} point: {.2f}\n", name, age, point);
```

- `{}` prints the value as-is.
- `{.Nf}` prints a float with N decimal places, e.g. `{.2f}`.
- `\n`, `\t`, `\r` are handled inside the string.

There is no `%s`/`%d` here — it is `{}` only.

You can also drop the `Stdio.` prefix. Bare `printf(...)` and `scanf(...)` are mapped to the
Stdio module automatically:

```
printf("just printf works too\n");
```

Input uses `scanf`, which is not the C function — it is a safer wrapper that writes into a
variable directly:

```
int number = 0;
Stdio.printf("Value: ");
Stdio.scanf(number);        // reads a number into `number`

gcChar line = "";
Stdio.scanf(line);          // reads a line into `line`
```

There is also a format-plus-variable form, `scanf("format", variable)`. Reading a number
consumes the trailing newline too, so a numeric `scanf` followed by a string `scanf` behaves
correctly. Older code that left the newline behind is why this had to be handled explicitly.

Remember that `scanf` needs a real console. In the IDE that means the program must carry
`#pragma commandline`.

---

## sizeof and strlen

`sizeof` exists in a few forms:

```
sizeof(array)          // number of elements
sizeof(array[0])       // 1
sizeof("text")         // strlen + 1
sizeof(int)            // 4 (real byte sizes for type names)
strlen(text)           // length of a string / gcChar
```

The `sizeof(array) / sizeof(array[0])` idiom is the standard way to get a count.

---

## Native modules

The bundled modules are `Math`, `Stdio`, `Embed`, `Raylib` and `Raygui`. You declare the ones
you use with `#native`, then call members as `Module.member(...)`.

### Math

```
#native <Math>

Math.randInt(0, 99);        Math.randint(0, 99);
Math.randFloat(0, 99);      Math.randfloat(0, 99);
Math.min(0, 99);            Math.max(0, 99);
Math.abs(-99);
Math.floor(9.99);           Math.ceil(9.01);    Math.round(9.5);
Math.sqrt(99);              Math.pow(2, 8);
Math.sin(90);               Math.cos(90);       Math.tan(90);
Math.asin(1);               Math.acos(1);       Math.atan(1);
Math.log(10);               Math.log10(100);    Math.exp(2);
Math.clamp(150, 0, 100);    Math.sign(-99);
Math.randBool();            Math.randChoice(1, 2, 3);    Math.randSign();
```

Trig functions take degrees, not radians — the module converts for you. `randInt` seeds once
on first use.

### Stdio

```
#native <Stdio>

Stdio.printf();
Stdio.scanf(variable);

Stdio.openFile(file="file");      Stdio.openfile(file="file");
Stdio.writeFile(file="f", text="");   Stdio.readFile(file="f");
Stdio.closeFile(file="f");            Stdio.appendFile(file="f", text="");

Stdio.fileExists(file="f");   Stdio.deleteFile(file="f");
Stdio.renameFile(file="f", newName="g");
Stdio.fileSize(file="f");     Stdio.flushFile(file="f");
```

Both camelCase and all-lowercase spellings are registered, so `openfile` and `openFile` are
the same function. The `file=`/`text=` labels are just documentation — the values are passed
by position.

One thing to watch: `readFile` merely reports the size of a file (it returns the byte count,
or -1 if missing); it does not hand you the contents as a string. Reading actual file content
back into a variable is not wired up yet.

### Raylib

`#native <Raylib>`. This is the big one — **1005 members** covering window/core, drawing
modes, input, 2D shapes, textures and images, text and fonts, 3D models, audio, colour
helpers and the type constructors.

`#native <Raygui>` adds **85 more**. Those numbers are generated, not hand-maintained:
`tools/gen_native_db.py` reads `raylib.h` / `raygui.h` together with the module sources and
emits the IDE's completion database (`language/_SRC/complete/complete_native_db.c`), printing
a `[gap]` line for anything the headers declare but the module does not expose.

**Every call needs the `Raylib.` prefix.** The prefix is not optional: `Raylib.InitWindow`,
`Raylib.DrawText`, `Raylib.RED`. Writing `InitWindow(...)` on its own resolves to nothing and
the run fails with "unknown member". Every name in the lists below is written with its prefix
already attached, so you can copy it straight into your code.

Three conventions you have to know before the list means anything.

Colours are 32-bit packed integers, laid out as `R | G<<8 | B<<16 | A<<24`, and they travel
as a single `double`. You get them from the constants (`Raylib.RED`), from
`Raylib.Color(r, g, b, a)`, or from the colour helpers (`Raylib.Fade`, `Raylib.ColorAlpha`),
and you pass them straight into anything that wants a colour.

Struct arguments are flattened into plain numbers. `Rectangle` is four numbers, `Vector2` is
two, `Vector3` is three, `Color` is the one packed number. So `Raylib.DrawRectangleRec` is
really called as `(x, y, w, h, color)`. If you have a struct variable it expands on its own:
with `Raylib.Rectangle r = { 10, 10, 100, 50 };` you can write
`Raylib.DrawRectangleRec(r, Raylib.RED)`.

Functions that return a struct (a texture, a mesh, a camera matrix, a collision result, ...)
give back a numeric **handle** or stash the value in an internal slot; you pass the handle to
the matching draw or `Unload*` call. Functions that return a string (`GetClipboardText`,
`GetFileName`, `TextFormat`, ...) leave the text in that slot, so the useful path is to read
them from the IDE's completion rather than to chain them.

**Reading a value back.** A stolen slot is not a dead end — the module publishes it through
`Last*` members. Read the component you need right after the call:

```
Raylib.MeasureTextEx(font, "score", 20, 1);   // fills the Vector2 slot
float w = Raylib.LastV2X();                    // its .x
float h = Raylib.LastV2Y();                    // its .y

Raylib.GetClipboardText();                     // fills the string slot
int n = Raylib.LastStringLen();
for (int i = 0; i < n; i++) {
    int ch = Raylib.LastStringByte(i);         // byte by byte, UTF-8
}
```

The groups, by what the source call produced:

| Slot | Accessors |
| --- | --- |
| int | `LastInt()` |
| Vector2 / Vector3 / Vector4 | `LastV2X/Y`, `LastV3X/Y/Z`, `LastV4X/Y/Z/W` |
| Rectangle | `LastRectX/Y/W/H` |
| Ray | `LastRayX/Y/Z`, `LastRayDirX/Y/Z` |
| RayCollision | `LastRayColHit/Distance`, `LastRayColPX/PY/PZ`, `LastRayColNX/NY/NZ` |
| BoundingBox | `LastBoxMinX/Y/Z`, `LastBoxMaxX/Y/Z` |
| NPatchInfo | `LastNPatchX/Y/W/H`, `LastNPatchLeft/Top/Right/Bottom/Layout` |
| GlyphInfo | `LastGlyphValue/OffsetX/OffsetY/AdvanceX`, `LastGlyphImgWidth/ImgHeight` |
| Camera / Camera2D | `LastCamPosX/Y/Z`, `LastCamTargetX/Y/Z`, `LastCamUpX/Y/Z`, `LastCamFovy/Projection`, `LastCam2DOffsetX/Y`, `LastCam2DTargetX/Y`, `LastCam2DRotation/Zoom` |
| Quaternion / Transform | `LastQuatX/Y/Z/W`, `LastTransformTX/TY/TZ`, `LastTransformRX/RY/RZ`, `LastTransformSX/SY/SZ` |
| Matrix | `LastMatrix(i)` — i is 0..15 |
| AutomationEvent | `LastAEventFrame/Type`, `LastAEventP0..P3` |
| VrDeviceInfo | `LastVrDevHRes/VRes/HScreen/VScreen/EyeDist/LensSep/Ipd`, `LastVrDevLensDistortion(i)`, `LastVrDevChromaAb(i)` |
| VrStereoConfig | `LastVrProjection(eye, i)`, `LastVrViewOffset(eye, i)`, `LastVrPair(idx)` |
| string | `LastStringLen()`, `LastStringByte(i)` |

Every accessor returns a plain number — the module ABI carries only a `double`. Booleans and
result codes therefore need no conversion; they arrive as `0`/`1`:

```
int hit = Raylib.CheckCollisionPointRec(mouse, box);   // this call returns 0 or 1 directly

Raylib.GetCollisionRec(recA, recB);                    // out-param -> fills the Rectangle slot
float overlapArea = Raylib.LastRectW() * Raylib.LastRectH();
```

`Raygui` works the same way for its out-parameters (a control's `value`, `checked`, `active`,
edited `text`, scroll index, `Color`, ...): `LastInt`, `LastFloat`, `LastBool`, `LastV2X/Y`,
`LastV3X/Y/Z`, `LastColor`, `LastRectX/Y/W/H`, `LastFontBaseSize/GlyphCount/GlyphPadding`,
`LastStringLen/LastStringByte`. The control *returns* its result code, so the changed state is
read from those members:

```
int result = Raygui.GuiSlider(bounds, "left", "right", value, 0, 100);
float changed = Raygui.LastFloat();            // the value the user dragged to
```

`LastVrPair(idx)` covers the eight `float[2]` fields in order: 0-1 leftLensCenter,
2-3 rightLensCenter, 4-5 leftScreenCenter, 6-7 rightScreenCenter, 8-9 scale, 10-11 scaleIn.

**Colours** — a colour is one packed number, used directly as `Raylib.RED`

```
Raylib.RAYWHITE
Raylib.LIGHTGRAY
Raylib.GRAY
Raylib.DARKGRAY
Raylib.YELLOW
Raylib.GOLD
Raylib.ORANGE
Raylib.PINK
Raylib.RED
Raylib.MAROON
Raylib.GREEN
Raylib.LIME
Raylib.DARKGREEN
Raylib.SKYBLUE
Raylib.BLUE
Raylib.DARKBLUE
Raylib.PURPLE
Raylib.VIOLET
Raylib.DARKPURPLE
Raylib.BEIGE
Raylib.BROWN
Raylib.DARKBROWN
Raylib.WHITE
Raylib.BLACK
Raylib.BLANK
Raylib.MAGENTA
```

**Window and core**

```
Raylib.InitWindow
Raylib.CloseWindow
Raylib.WindowShouldClose
Raylib.IsWindowReady
Raylib.IsWindowFullscreen
Raylib.IsWindowHidden
Raylib.IsWindowMinimized
Raylib.IsWindowMaximized
Raylib.IsWindowFocused
Raylib.IsWindowResized
Raylib.IsWindowState
Raylib.SetWindowState
Raylib.ClearWindowState
Raylib.ToggleFullscreen
Raylib.ToggleBorderlessWindowed
Raylib.MaximizeWindow
Raylib.MinimizeWindow
Raylib.RestoreWindow
Raylib.SetWindowIcon
Raylib.SetWindowIcons
Raylib.SetWindowTitle
Raylib.SetWindowPosition
Raylib.SetWindowMonitor
Raylib.SetWindowMinSize
Raylib.SetWindowMaxSize
Raylib.SetWindowSize
Raylib.SetWindowOpacity
Raylib.SetWindowFocused
Raylib.GetWindowHandle
Raylib.GetScreenWidth
Raylib.GetScreenHeight
Raylib.GetRenderWidth
Raylib.GetRenderHeight
Raylib.GetMonitorCount
Raylib.GetCurrentMonitor
Raylib.GetMonitorPosition
Raylib.GetMonitorWidth
Raylib.GetMonitorHeight
Raylib.GetMonitorPhysicalWidth
Raylib.GetMonitorPhysicalHeight
Raylib.GetMonitorRefreshRate
Raylib.GetWindowPosition
Raylib.GetWindowScaleDPI
Raylib.GetMonitorName
Raylib.SetClipboardText
Raylib.GetClipboardText
Raylib.GetClipboardImage
Raylib.EnableEventWaiting
Raylib.DisableEventWaiting
```

**Cursor**

```
Raylib.ShowCursor
Raylib.HideCursor
Raylib.IsCursorHidden
Raylib.EnableCursor
Raylib.DisableCursor
Raylib.IsCursorOnScreen
```

**Drawing modes**

```
Raylib.ClearBackground
Raylib.BeginDrawing
Raylib.EndDrawing
Raylib.BeginMode2D
Raylib.EndMode2D
Raylib.BeginMode3D
Raylib.EndMode3D
Raylib.BeginTextureMode
Raylib.EndTextureMode
Raylib.BeginShaderMode
Raylib.EndShaderMode
Raylib.BeginBlendMode
Raylib.EndBlendMode
Raylib.BeginScissorMode
Raylib.EndScissorMode
Raylib.BeginVrStereoMode
Raylib.EndVrStereoMode
```

**VR, shaders, screen space, timing, memory**

```
Raylib.LoadVrStereoConfig
Raylib.UnloadVrStereoConfig
Raylib.LoadShader
Raylib.LoadShaderFromMemory
Raylib.IsShaderValid
Raylib.GetShaderLocation
Raylib.GetShaderLocationAttrib
Raylib.SetShaderValue
Raylib.SetShaderValueV
Raylib.SetShaderValueMatrix
Raylib.SetShaderValueTexture
Raylib.UnloadShader
Raylib.GetScreenToWorldRay
Raylib.GetScreenToWorldRayEx
Raylib.GetWorldToScreen
Raylib.GetWorldToScreenEx
Raylib.GetWorldToScreen2D
Raylib.GetScreenToWorld2D
Raylib.GetCameraMatrix
Raylib.GetCameraMatrix2D
Raylib.SetTargetFPS
Raylib.GetFrameTime
Raylib.GetTime
Raylib.GetFPS
Raylib.SwapScreenBuffer
Raylib.PollInputEvents
Raylib.WaitTime
Raylib.SetRandomSeed
Raylib.GetRandomValue
Raylib.LoadRandomSequence
Raylib.UnloadRandomSequence
Raylib.TakeScreenshot
Raylib.SetConfigFlags
Raylib.OpenURL
Raylib.SetTraceLogLevel
Raylib.TraceLog
Raylib.SetTraceLogCallback
Raylib.MemAlloc
Raylib.MemRealloc
Raylib.MemFree
```

**File system and data**

```
Raylib.LoadFileData
Raylib.UnloadFileData
Raylib.SaveFileData
Raylib.ExportDataAsCode
Raylib.LoadFileText
Raylib.UnloadFileText
Raylib.SaveFileText
Raylib.SetLoadFileDataCallback
Raylib.SetSaveFileDataCallback
Raylib.SetLoadFileTextCallback
Raylib.SetSaveFileTextCallback
Raylib.FileRename
Raylib.FileRemove
Raylib.FileCopy
Raylib.FileMove
Raylib.FileTextReplace
Raylib.FileTextFindIndex
Raylib.FileExists
Raylib.DirectoryExists
Raylib.IsFileExtension
Raylib.IsFileHidden
Raylib.GetFileLength
Raylib.GetFileModTime
Raylib.GetFileExtension
Raylib.GetFileName
Raylib.GetFileNameWithoutExt
Raylib.GetDirectoryPath
Raylib.GetPrevDirectoryPath
Raylib.GetWorkingDirectory
Raylib.GetApplicationDirectory
Raylib.MakeDirectory
Raylib.ChangeDirectory
Raylib.IsPathFile
Raylib.IsPathDirectory
Raylib.IsPathAbsolute
Raylib.IsFileNameValid
Raylib.LoadDirectoryFiles
Raylib.LoadDirectoryFilesEx
Raylib.UnloadDirectoryFiles
Raylib.IsFileDropped
Raylib.LoadDroppedFiles
Raylib.UnloadDroppedFiles
Raylib.GetDirectoryFileCount
Raylib.GetDirectoryFileCountEx
Raylib.CompressData
Raylib.DecompressData
Raylib.EncodeDataBase64
Raylib.DecodeDataBase64
Raylib.ComputeCRC32
Raylib.ComputeMD5
Raylib.ComputeSHA1
Raylib.ComputeSHA256
Raylib.LoadAutomationEventList
Raylib.UnloadAutomationEventList
Raylib.ExportAutomationEventList
Raylib.SetAutomationEventList
Raylib.SetAutomationEventBaseFrame
Raylib.StartAutomationEventRecording
Raylib.StopAutomationEventRecording
Raylib.PlayAutomationEvent
```

**Type constructors** — call them to build a raylib value; struct value types write into an internal slot, handle types return a handle

```
Raylib.Quaternion
Raylib.Color
Raylib.Camera3D
Raylib.RayCollision
Raylib.Transform
Raylib.BoneInfo
Raylib.Image
Raylib.Texture
Raylib.Texture2D
Raylib.TextureCubemap
Raylib.RenderTexture
Raylib.RenderTexture2D
Raylib.Font
Raylib.Shader
Raylib.Mesh
Raylib.MaterialMap
Raylib.Material
Raylib.ModelSkeleton
Raylib.Model
Raylib.ModelAnimation
Raylib.Wave
Raylib.AudioStream
Raylib.Sound
Raylib.Music
Raylib.VrDeviceInfo
Raylib.FilePathList
Raylib.AutomationEventList
Raylib.Rectangle
Raylib.Vector2
Raylib.Vector3
Raylib.Vector4
Raylib.Matrix
Raylib.Camera
Raylib.Camera2D
Raylib.Ray
Raylib.BoundingBox
Raylib.NPatchInfo
Raylib.GlyphInfo
Raylib.VrStereoConfig
Raylib.AutomationEvent
```

**Input constants**

```
Raylib.CAMERA_FREE
Raylib.CAMERA_ORBITAL
Raylib.CAMERA_FIRST_PERSON
Raylib.CAMERA_THIRD_PERSON
Raylib.CAMERA_PERSPECTIVE
Raylib.CAMERA_ORTHOGRAPHIC
Raylib.CAMERA_CUSTOM
Raylib.KEY_Z
Raylib.KEY_ESCAPE
Raylib.KEY_SPACE
Raylib.KEY_W
Raylib.KEY_A
Raylib.KEY_S
Raylib.KEY_D
Raylib.MOUSE_BUTTON_LEFT
Raylib.MOUSE_BUTTON_RIGHT
Raylib.MOUSE_BUTTON_MIDDLE
```

**Config flags**

```
Raylib.FLAG_VSYNC_HINT
Raylib.FLAG_FULLSCREEN_MODE
Raylib.FLAG_WINDOW_RESIZABLE
Raylib.FLAG_WINDOW_UNDECORATED
Raylib.FLAG_WINDOW_HIDDEN
Raylib.FLAG_WINDOW_MINIMIZED
Raylib.FLAG_WINDOW_MAXIMIZED
Raylib.FLAG_WINDOW_UNFOCUSED
Raylib.FLAG_WINDOW_TOPMOST
Raylib.FLAG_WINDOW_ALWAYS_RUN
Raylib.FLAG_WINDOW_TRANSPARENT
Raylib.FLAG_WINDOW_HIGHDPI
Raylib.FLAG_WINDOW_MOUSE_PASSTHROUGH
Raylib.FLAG_BORDERLESS_WINDOWED_MODE
Raylib.FLAG_MSAA_4X_HINT
Raylib.FLAG_INTERLACED_HINT
```

**Trace log levels**

```
Raylib.LOG_ALL
Raylib.LOG_TRACE
Raylib.LOG_DEBUG
Raylib.LOG_INFO
Raylib.LOG_WARNING
Raylib.LOG_ERROR
Raylib.LOG_FATAL
Raylib.LOG_NONE
```

**Keyboard keys**

```
Raylib.KEY_NULL
Raylib.KEY_APOSTROPHE
Raylib.KEY_COMMA
Raylib.KEY_MINUS
Raylib.KEY_PERIOD
Raylib.KEY_SLASH
Raylib.KEY_ZERO
Raylib.KEY_ONE
Raylib.KEY_TWO
Raylib.KEY_THREE
Raylib.KEY_FOUR
Raylib.KEY_FIVE
Raylib.KEY_SIX
Raylib.KEY_SEVEN
Raylib.KEY_EIGHT
Raylib.KEY_NINE
Raylib.KEY_SEMICOLON
Raylib.KEY_EQUAL
Raylib.KEY_A
Raylib.KEY_B
Raylib.KEY_C
Raylib.KEY_D
Raylib.KEY_E
Raylib.KEY_F
Raylib.KEY_G
Raylib.KEY_H
Raylib.KEY_I
Raylib.KEY_J
Raylib.KEY_K
Raylib.KEY_L
Raylib.KEY_M
Raylib.KEY_N
Raylib.KEY_O
Raylib.KEY_P
Raylib.KEY_Q
Raylib.KEY_R
Raylib.KEY_S
Raylib.KEY_T
Raylib.KEY_U
Raylib.KEY_V
Raylib.KEY_W
Raylib.KEY_X
Raylib.KEY_Y
Raylib.KEY_Z
Raylib.KEY_LEFT_BRACKET
Raylib.KEY_BACKSLASH
Raylib.KEY_RIGHT_BRACKET
Raylib.KEY_GRAVE
Raylib.KEY_SPACE
Raylib.KEY_ESCAPE
Raylib.KEY_ENTER
Raylib.KEY_TAB
Raylib.KEY_BACKSPACE
Raylib.KEY_INSERT
Raylib.KEY_DELETE
Raylib.KEY_RIGHT
Raylib.KEY_LEFT
Raylib.KEY_DOWN
Raylib.KEY_UP
Raylib.KEY_PAGE_UP
Raylib.KEY_PAGE_DOWN
Raylib.KEY_HOME
Raylib.KEY_END
Raylib.KEY_CAPS_LOCK
Raylib.KEY_SCROLL_LOCK
Raylib.KEY_NUM_LOCK
Raylib.KEY_PRINT_SCREEN
Raylib.KEY_PAUSE
Raylib.KEY_F1
Raylib.KEY_F2
Raylib.KEY_F3
Raylib.KEY_F4
Raylib.KEY_F5
Raylib.KEY_F6
Raylib.KEY_F7
Raylib.KEY_F8
Raylib.KEY_F9
Raylib.KEY_F10
Raylib.KEY_F11
Raylib.KEY_F12
Raylib.KEY_LEFT_SHIFT
Raylib.KEY_LEFT_CONTROL
Raylib.KEY_LEFT_ALT
Raylib.KEY_LEFT_SUPER
Raylib.KEY_RIGHT_SHIFT
Raylib.KEY_RIGHT_CONTROL
Raylib.KEY_RIGHT_ALT
Raylib.KEY_RIGHT_SUPER
Raylib.KEY_KB_MENU
Raylib.KEY_KP_0
Raylib.KEY_KP_1
Raylib.KEY_KP_2
Raylib.KEY_KP_3
Raylib.KEY_KP_4
Raylib.KEY_KP_5
Raylib.KEY_KP_6
Raylib.KEY_KP_7
Raylib.KEY_KP_8
Raylib.KEY_KP_9
Raylib.KEY_KP_DECIMAL
Raylib.KEY_KP_DIVIDE
Raylib.KEY_KP_MULTIPLY
Raylib.KEY_KP_SUBTRACT
Raylib.KEY_KP_ADD
Raylib.KEY_KP_ENTER
Raylib.KEY_KP_EQUAL
Raylib.KEY_BACK
Raylib.KEY_MENU
Raylib.KEY_VOLUME_UP
Raylib.KEY_VOLUME_DOWN
```

**Mouse buttons and cursors**

```
Raylib.MOUSE_BUTTON_LEFT
Raylib.MOUSE_BUTTON_RIGHT
Raylib.MOUSE_BUTTON_MIDDLE
Raylib.MOUSE_BUTTON_SIDE
Raylib.MOUSE_BUTTON_EXTRA
Raylib.MOUSE_BUTTON_FORWARD
Raylib.MOUSE_BUTTON_BACK
Raylib.MOUSE_LEFT_BUTTON
Raylib.MOUSE_RIGHT_BUTTON
Raylib.MOUSE_MIDDLE_BUTTON
Raylib.MOUSE_CURSOR_DEFAULT
Raylib.MOUSE_CURSOR_ARROW
Raylib.MOUSE_CURSOR_IBEAM
Raylib.MOUSE_CURSOR_CROSSHAIR
Raylib.MOUSE_CURSOR_POINTING_HAND
Raylib.MOUSE_CURSOR_RESIZE_EW
Raylib.MOUSE_CURSOR_RESIZE_NS
Raylib.MOUSE_CURSOR_RESIZE_NWSE
Raylib.MOUSE_CURSOR_RESIZE_NESW
Raylib.MOUSE_CURSOR_RESIZE_ALL
Raylib.MOUSE_CURSOR_NOT_ALLOWED
```

**Gamepad buttons and axes**

```
Raylib.GAMEPAD_BUTTON_UNKNOWN
Raylib.GAMEPAD_BUTTON_LEFT_FACE_UP
Raylib.GAMEPAD_BUTTON_LEFT_FACE_RIGHT
Raylib.GAMEPAD_BUTTON_LEFT_FACE_DOWN
Raylib.GAMEPAD_BUTTON_LEFT_FACE_LEFT
Raylib.GAMEPAD_BUTTON_RIGHT_FACE_UP
Raylib.GAMEPAD_BUTTON_RIGHT_FACE_RIGHT
Raylib.GAMEPAD_BUTTON_RIGHT_FACE_DOWN
Raylib.GAMEPAD_BUTTON_RIGHT_FACE_LEFT
Raylib.GAMEPAD_BUTTON_LEFT_TRIGGER_1
Raylib.GAMEPAD_BUTTON_LEFT_TRIGGER_2
Raylib.GAMEPAD_BUTTON_RIGHT_TRIGGER_1
Raylib.GAMEPAD_BUTTON_RIGHT_TRIGGER_2
Raylib.GAMEPAD_BUTTON_MIDDLE_LEFT
Raylib.GAMEPAD_BUTTON_MIDDLE
Raylib.GAMEPAD_BUTTON_MIDDLE_RIGHT
Raylib.GAMEPAD_BUTTON_LEFT_THUMB
Raylib.GAMEPAD_BUTTON_RIGHT_THUMB
Raylib.GAMEPAD_AXIS_LEFT_X
Raylib.GAMEPAD_AXIS_LEFT_Y
Raylib.GAMEPAD_AXIS_RIGHT_X
Raylib.GAMEPAD_AXIS_RIGHT_Y
Raylib.GAMEPAD_AXIS_LEFT_TRIGGER
Raylib.GAMEPAD_AXIS_RIGHT_TRIGGER
```

**Material maps**

```
Raylib.MATERIAL_MAP_ALBEDO
Raylib.MATERIAL_MAP_METALNESS
Raylib.MATERIAL_MAP_NORMAL
Raylib.MATERIAL_MAP_ROUGHNESS
Raylib.MATERIAL_MAP_OCCLUSION
Raylib.MATERIAL_MAP_EMISSION
Raylib.MATERIAL_MAP_HEIGHT
Raylib.MATERIAL_MAP_CUBEMAP
Raylib.MATERIAL_MAP_IRRADIANCE
Raylib.MATERIAL_MAP_PREFILTER
Raylib.MATERIAL_MAP_BRDF
Raylib.MATERIAL_MAP_DIFFUSE
Raylib.MATERIAL_MAP_SPECULAR
```

**Shader locations**

```
Raylib.SHADER_LOC_VERTEX_POSITION
Raylib.SHADER_LOC_VERTEX_TEXCOORD01
Raylib.SHADER_LOC_VERTEX_TEXCOORD02
Raylib.SHADER_LOC_VERTEX_NORMAL
Raylib.SHADER_LOC_VERTEX_TANGENT
Raylib.SHADER_LOC_VERTEX_COLOR
Raylib.SHADER_LOC_MATRIX_MVP
Raylib.SHADER_LOC_MATRIX_VIEW
Raylib.SHADER_LOC_MATRIX_PROJECTION
Raylib.SHADER_LOC_MATRIX_MODEL
Raylib.SHADER_LOC_MATRIX_NORMAL
Raylib.SHADER_LOC_VECTOR_VIEW
Raylib.SHADER_LOC_COLOR_DIFFUSE
Raylib.SHADER_LOC_COLOR_SPECULAR
Raylib.SHADER_LOC_COLOR_AMBIENT
Raylib.SHADER_LOC_MAP_ALBEDO
Raylib.SHADER_LOC_MAP_METALNESS
Raylib.SHADER_LOC_MAP_NORMAL
Raylib.SHADER_LOC_MAP_ROUGHNESS
Raylib.SHADER_LOC_MAP_OCCLUSION
Raylib.SHADER_LOC_MAP_EMISSION
Raylib.SHADER_LOC_MAP_HEIGHT
Raylib.SHADER_LOC_MAP_CUBEMAP
Raylib.SHADER_LOC_MAP_IRRADIANCE
Raylib.SHADER_LOC_MAP_PREFILTER
Raylib.SHADER_LOC_MAP_BRDF
Raylib.SHADER_LOC_VERTEX_BONEIDS
Raylib.SHADER_LOC_VERTEX_BONEWEIGHTS
Raylib.SHADER_LOC_MATRIX_BONETRANSFORMS
Raylib.SHADER_LOC_VERTEX_INSTANCETRANSFORM
Raylib.SHADER_LOC_MAP_DIFFUSE
Raylib.SHADER_LOC_MAP_SPECULAR
```

**Shader uniform and attribute types**

```
Raylib.SHADER_UNIFORM_FLOAT
Raylib.SHADER_UNIFORM_VEC2
Raylib.SHADER_UNIFORM_VEC3
Raylib.SHADER_UNIFORM_VEC4
Raylib.SHADER_UNIFORM_INT
Raylib.SHADER_UNIFORM_IVEC2
Raylib.SHADER_UNIFORM_IVEC3
Raylib.SHADER_UNIFORM_IVEC4
Raylib.SHADER_UNIFORM_UINT
Raylib.SHADER_UNIFORM_UIVEC2
Raylib.SHADER_UNIFORM_UIVEC3
Raylib.SHADER_UNIFORM_UIVEC4
Raylib.SHADER_UNIFORM_SAMPLER2D
Raylib.SHADER_ATTRIB_FLOAT
Raylib.SHADER_ATTRIB_VEC2
Raylib.SHADER_ATTRIB_VEC3
Raylib.SHADER_ATTRIB_VEC4
```

**Pixel formats**

```
Raylib.PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
Raylib.PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA
Raylib.PIXELFORMAT_UNCOMPRESSED_R5G6B5
Raylib.PIXELFORMAT_UNCOMPRESSED_R8G8B8
Raylib.PIXELFORMAT_UNCOMPRESSED_R5G5B5A1
Raylib.PIXELFORMAT_UNCOMPRESSED_R4G4B4A4
Raylib.PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
Raylib.PIXELFORMAT_UNCOMPRESSED_R32
Raylib.PIXELFORMAT_UNCOMPRESSED_R32G32B32
Raylib.PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
Raylib.PIXELFORMAT_UNCOMPRESSED_R16
Raylib.PIXELFORMAT_UNCOMPRESSED_R16G16B16
Raylib.PIXELFORMAT_UNCOMPRESSED_R16G16B16A16
Raylib.PIXELFORMAT_COMPRESSED_DXT1_RGB
Raylib.PIXELFORMAT_COMPRESSED_DXT1_RGBA
Raylib.PIXELFORMAT_COMPRESSED_DXT3_RGBA
Raylib.PIXELFORMAT_COMPRESSED_DXT5_RGBA
Raylib.PIXELFORMAT_COMPRESSED_ETC1_RGB
Raylib.PIXELFORMAT_COMPRESSED_ETC2_RGB
Raylib.PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA
Raylib.PIXELFORMAT_COMPRESSED_PVRT_RGB
Raylib.PIXELFORMAT_COMPRESSED_PVRT_RGBA
Raylib.PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA
Raylib.PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA
```

**Texture filter, wrap, cubemap layout, font type**

```
Raylib.TEXTURE_FILTER_POINT
Raylib.TEXTURE_FILTER_BILINEAR
Raylib.TEXTURE_FILTER_TRILINEAR
Raylib.TEXTURE_FILTER_ANISOTROPIC_4X
Raylib.TEXTURE_FILTER_ANISOTROPIC_8X
Raylib.TEXTURE_FILTER_ANISOTROPIC_16X
Raylib.TEXTURE_WRAP_REPEAT
Raylib.TEXTURE_WRAP_CLAMP
Raylib.TEXTURE_WRAP_MIRROR_REPEAT
Raylib.TEXTURE_WRAP_MIRROR_CLAMP
Raylib.CUBEMAP_LAYOUT_AUTO_DETECT
Raylib.CUBEMAP_LAYOUT_LINE_VERTICAL
Raylib.CUBEMAP_LAYOUT_LINE_HORIZONTAL
Raylib.CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR
Raylib.CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE
Raylib.FONT_DEFAULT
Raylib.FONT_BITMAP
Raylib.FONT_SDF
```

**Blend modes**

```
Raylib.BLEND_ALPHA
Raylib.BLEND_ADDITIVE
Raylib.BLEND_MULTIPLIED
Raylib.BLEND_ADD_COLORS
Raylib.BLEND_SUBTRACT_COLORS
Raylib.BLEND_ALPHA_PREMULTIPLY
Raylib.BLEND_CUSTOM
Raylib.BLEND_CUSTOM_SEPARATE
```

**Gestures**

```
Raylib.GESTURE_NONE
Raylib.GESTURE_TAP
Raylib.GESTURE_DOUBLETAP
Raylib.GESTURE_HOLD
Raylib.GESTURE_DRAG
Raylib.GESTURE_SWIPE_RIGHT
Raylib.GESTURE_SWIPE_LEFT
Raylib.GESTURE_SWIPE_UP
Raylib.GESTURE_SWIPE_DOWN
Raylib.GESTURE_PINCH_IN
Raylib.GESTURE_PINCH_OUT
```

**N-patch layout**

```
Raylib.NPATCH_NINE_PATCH
Raylib.NPATCH_THREE_PATCH_VERTICAL
Raylib.NPATCH_THREE_PATCH_HORIZONTAL
```

**Input — keyboard, mouse, gamepad, touch**

```
Raylib.IsKeyPressed
Raylib.IsKeyPressedRepeat
Raylib.IsKeyDown
Raylib.IsKeyReleased
Raylib.IsKeyUp
Raylib.GetKeyPressed
Raylib.GetCharPressed
Raylib.GetKeyName
Raylib.SetExitKey
Raylib.IsGamepadAvailable
Raylib.GetGamepadName
Raylib.IsGamepadButtonPressed
Raylib.IsGamepadButtonDown
Raylib.IsGamepadButtonReleased
Raylib.IsGamepadButtonUp
Raylib.GetGamepadButtonPressed
Raylib.GetGamepadAxisCount
Raylib.GetGamepadAxisMovement
Raylib.SetGamepadMappings
Raylib.SetGamepadVibration
Raylib.IsMouseButtonPressed
Raylib.IsMouseButtonDown
Raylib.IsMouseButtonReleased
Raylib.IsMouseButtonUp
Raylib.GetMouseX
Raylib.GetMouseY
Raylib.GetMousePosition
Raylib.GetMouseDelta
Raylib.SetMousePosition
Raylib.SetMouseOffset
Raylib.SetMouseScale
Raylib.GetMouseWheelMove
Raylib.GetMouseWheelMoveV
Raylib.SetMouseCursor
Raylib.GetTouchX
Raylib.GetTouchY
Raylib.GetTouchPosition
Raylib.GetTouchPointId
Raylib.GetTouchPointCount
```

**Gestures (touch)**

```
Raylib.SetGesturesEnabled
Raylib.IsGestureDetected
Raylib.GetGestureDetected
Raylib.GetGestureHoldDuration
Raylib.GetGestureDragVector
Raylib.GetGestureDragAngle
Raylib.GetGesturePinchVector
Raylib.GetGesturePinchAngle
```

**Camera**

```
Raylib.UpdateCamera
Raylib.UpdateCameraPro
```

**2D shapes — pixels, lines, triangles, rectangles, polygons, circles**

```
Raylib.SetShapesTexture
Raylib.GetShapesTexture
Raylib.GetShapesTextureRectangle
Raylib.DrawPixel
Raylib.DrawPixelV
Raylib.DrawLine
Raylib.DrawLineV
Raylib.DrawLineEx
Raylib.DrawLineStrip
Raylib.DrawLineBezier
Raylib.DrawLineDashed
Raylib.DrawTriangle
Raylib.DrawTriangleLines
Raylib.DrawTriangleGradient
Raylib.DrawTriangleLinesEx
Raylib.DrawTriangleFan
Raylib.DrawTriangleStrip
Raylib.DrawRectangle
Raylib.DrawRectangleV
Raylib.DrawRectangleRec
Raylib.DrawRectanglePro
Raylib.DrawRectangleGradientV
Raylib.DrawRectangleGradientH
Raylib.DrawRectangleGradientEx
Raylib.DrawRectangleLines
Raylib.DrawRectangleLinesEx
Raylib.DrawRectangleRounded
Raylib.DrawRectangleRoundedLines
Raylib.DrawRectangleRoundedLinesEx
Raylib.DrawPoly
Raylib.DrawPolyLines
Raylib.DrawPolyLinesEx
Raylib.DrawCircle
Raylib.DrawCircleV
Raylib.DrawCircleGradient
Raylib.DrawCircleSector
Raylib.DrawCircleSectorLines
Raylib.DrawCircleSectorLinesEx
Raylib.DrawCircleLines
Raylib.DrawCircleLinesV
Raylib.DrawCircleLinesEx
Raylib.DrawEllipse
Raylib.DrawEllipseV
Raylib.DrawEllipseLines
Raylib.DrawEllipseLinesV
Raylib.DrawEllipseLinesEx
Raylib.DrawRing
Raylib.DrawRingLines
Raylib.DrawRingLinesEx
Raylib.DrawSplineLinear
Raylib.DrawSplineBasis
Raylib.DrawSplineCatmullRom
Raylib.DrawSplineBezierQuadratic
Raylib.DrawSplineBezierCubic
Raylib.DrawSplineSegmentLinear
Raylib.DrawSplineSegmentBasis
Raylib.DrawSplineSegmentCatmullRom
Raylib.DrawSplineSegmentBezierQuadratic
Raylib.DrawSplineSegmentBezierCubic
Raylib.GetSplinePointLinear
Raylib.GetSplinePointBasis
Raylib.GetSplinePointCatmullRom
Raylib.GetSplinePointBezierQuadratic
Raylib.GetSplinePointBezierCubic
```

**Collision checks (2D)**

```
Raylib.CheckCollisionRecs
Raylib.CheckCollisionCircles
Raylib.CheckCollisionCircleRec
Raylib.CheckCollisionCircleLine
Raylib.CheckCollisionPointRec
Raylib.CheckCollisionPointCircle
Raylib.CheckCollisionPointTriangle
Raylib.CheckCollisionPointLine
Raylib.CheckCollisionPointPoly
Raylib.CheckCollisionLines
Raylib.GetCollisionRec
```

<!--RAYLIB_CONT_4-->

### Embed

`Embed` runs the Lua and Python runtimes and lets you share variables with them. See the next
section.

---

## Embed: Lua and Python

`Embed` is how GCL talks to Lua and Python. It starts their runtimes as separate processes and
lets you pass values back and forth.

```
#native <Embed>
#native <Stdio>

int main() {
    Embed.SendValue(type="python", name="score", value="42");
    Embed.SendValue(type="lua",    name="speed", value="3.14");

    Embed.Run(type="lua");
    Embed.Run(type="python");

    double from_py = Embed.GetValue(type="python", name="score");
    Stdio.printf("python score: {}\n", from_py);

    Embed.Stop(type="lua");
    Embed.Stop(type="python");
    return 0;
}
```

The interface:

```
Embed.Run(type="lua" | "python");        // start the runtime
Embed.Stop(type="lua" | "python");       // stop it and wait for it to exit
Embed.IsActive(type="lua" | "python");   // 1 or 0
Embed.SendValue(type="...", name="v", value="data");
Embed.GetValue(type="...", name="v");    // returns a double
```

How it actually works: `Run` launches a child `gcl` process running `scripts/main.lua` or
`scripts/main.py` from the project directory. On a terminal-app program each child gets its
own console window. `Stop` waits for that child to finish.

Because these are separate processes, they cannot share memory, so variable exchange goes
through a small shared store file, `gcl_shared.state`, written under the project directory.
`SendValue` writes into it and `GetValue` reads back. Note that `GetValue` returns a
`double`, so only numeric values round-trip cleanly; a string value is parsed with `atof` on
the way back.

There is a real-time sharing pattern used in the Lua-vs-Python pong demo — call `SendValue`
inside your game loop and the other side reads it out every frame. That demo lives in
`language/examples/demo_pong_lua_vs_python/`.

One limit to keep in mind: only one process per runtime type. You can run Lua and Python at
the same time, but not two Luas.

---

## Project layout

`gcl -new <path>` scaffolds a project. The structure it creates:

```
ProjectName/
    assets/
        icon.png          default gnuchan logo; also the built exe's icon
    external/             .so/.dll dropped here for #extern
    include/              .gcsf files for #include
    lib/                  .gclib files
    out/                  project_name and project_name.gcBundle after -build
    scripts/              main.lua, main.py
    main.gcsf
    project.gcdata
```

The `external/` directory is where GCL looks for libraries declared with `#extern`.

### project.gcdata

The project file is JSON:

```json
{
  "project_name": "Demo",
  "developer_name": "developer",
  "version": 0.100,
  "icon": "assets/icon.png",
  "open_lua": true,
  "open_lua_raylib": true,
  "open_python": true,
  "open_python_raylib": true,
  "single_bundle": false,
  "lua_main_file": "main.lua",
  "python_main_file": "main.py",
  "gcl_include_path": "include",
  "gcl_lib_path": "lib",
  "gcl_external_path": "external",
  "python_import_path": "scripts",
  "lua_import_path": "scripts",
  "raylib_asset_directory_path": "assets"
}
```

`project_name` is the only field the build really depends on — it becomes the output exe
name. `icon` is applied to the executable at build time.

---

## Building

```
gcl -build project.gcdata [-o dir]
```

With no `-o`, the output goes into the project's own `out/` directory. The build produces:

```
out/
    ProjectName(.exe)
    ProjectName.gcBundle
```

The `.gcBundle` packs the project plus the runtime `Library/` into one file — it works like a
Godot PCK. When you run the produced executable and it finds the bundle next to itself, it
extracts it and runs `main.gcsf` from inside. The IDE itself (`Programs/*.dll`) is not
included in the bundle; only scripts and runtimes go in.

---

## Environment variables

The runtime uses a few environment variables you may want to set:

| Variable | Purpose |
| --- | --- |
| `GCL_PROJECT_DIR` | project root; Lua/Python child processes find `scripts/main.lua`/`main.py` through this |
| `GCL_SHARED_FILE` | path to `gcl_shared.state`, the shared value store for Embed |
| `GCL_TERMINAL` | set to `1` under `#pragma commandline`; children open their own terminals |
| `GCL_TERMINAL_CMD` | (Linux) override the terminal emulator used for spawning |
| `GCL_EXE_PATH` | path to `gcl.exe`; the IDE uses it to invoke `-run`/`-new`/`-build` |

If `GCL_PROJECT_DIR` is not already set, `gcl -run` sets it to the directory of the script.

---

## Known limitations

A short honest list, so nobody re-discovers them the hard way:

- **No real 128-bit types.** `int128`/`uint128`/`float128` are clamped to 64-bit/`double`.
- **Everything is a `double` at runtime.** Very large integers lose precision once they enter
  arithmetic, even though `uint64`/`uint128` literals print correctly.
- **`#warning`/`#debug` never abort.** Only `#error` stops the build (exit code 1, empty
  stdout).
- **`#extern`/`#register` support only a few calling shapes**, listed earlier.
- **`readFile` returns a size, not contents.** Reading a file's text into a variable is not
  implemented yet.
- **`enum` has no explicit values**; it is always 0, 1, 2, ...
- **`sizeof(array)` returns an element count, not bytes.**
- **Include guards are opt-in** (not bounded). `#pragma once` guards an included file. There
  is no cap on how many distinct guarded files a run may remember; it used to be a fixed 64,
  past which the guard was dropped silently.
- **`#lib` / `.gclib` was removed** — there is no bundle-loading path for it any more.
- **Undefined variables are a runtime error**, printed to stderr; the expression evaluates to
  0 and the program keeps going rather than stopping.

---

## A full example

Pulling most of the above together:

```
#native <Stdio>
#native <Math>

#define MAX_SCORE 100

typedef struct {
    char name[32];
    int  score;
} Player;

int main(int argc, char *argv[]) {
    Stdio.printf("players: {}\n", argc - 1);

    Player players[3] = {
        { "Ali",  Math.randInt(0, MAX_SCORE) },
        { "Veli", Math.randInt(0, MAX_SCORE) },
        { "Ayse", Math.randInt(0, MAX_SCORE) }
    };

    int n = sizeof(players) / sizeof(players[0]);
    int best = -1;

    for (int i = 0; i < n; i++) {
        Stdio.printf("{} -> {}\n", players[i].name, players[i].score);
        if (players[i].score > best) {
            best = players[i].score;
        }
    }

    Stdio.printf("best score: {}\n", best);
    return 0;
}
```

Run it with `gcl -run example.gcsf` from a build tree that has `Library/Math` and
`Library/Stdio` beside the executable.
