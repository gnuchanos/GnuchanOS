## GITHUB ACTION FOR REALEASED

# HOW WORKS

### IDE FOCUS EDUCATIONAL PLATFORM 
### Gnu/linux and Windows only
### gcl project writtend small .c files for better reading

# Hardcoded project build system

```
Build/
    OS_NAME/
        Library/
            Math.dll, .so
            Stdio.dll, .so
            Embed.dll, .so
            Lsp.dll, .so D:\GnuchanOS\language\src\LSP
            Raylib.dll, .so
            Raygui.dll, .so
            Embeded/
                Lua_Runtime/
                    lua.dll       --> Embeded c build --> .so
                    LuaRaylib.dll --> Embeded c build --> .so
                    LuaRaygui.dll --> Embeded c build --> .so
                Python_Runtime/
                    Python.dll    --> Embeded c build --> .so
                    PyRaylib.dll  --> Embeded c build --> .so
                    PyRaygui.dll  --> Embeded c build --> .so
                    Python/
        Programs/
            ide.dll
        gcl, .exe
```

``` gcl -new project_name
Project_name/
    assets/ --> raylib de kullanilicak gcl, lua, python otomatik gorebilmesi gerekiyor
        icon.png --> default gnuchan logo
    external/ -> .so, .dll gibi seyleri burada ariycak gcl python lua ozellikle #external <file.dll>
    include/ .gcsf #include
    lib/ .gclib #lib
    out/ project_name, project_name.gcBundle
    scripts/ lua, python files in here
    main.gcsf
    project.gcdata
```

``` project.gcdata
{
  "project_name": "Demo",
  "developer_name": "developer",
  "version": 0.100,
  "icon": "assets/icon.png", #// D:\GnuchanOS\assets\icon.png
  "open_lua": true,
  "open_python": true,
  "lua_main_file": "main.lua",
  "python_main_file": "main.py",
  "gcl_include_path": "include",
  "gcl_external_path": "external",
  "python_import_path": "scripts",
  "lua_import_path": "scripts",
  "raylib_asset_directory_path": "assets"
}
```

gcl -debug -run file.gcsf
gcl -pyrun file.py
gcl -luarun file.lua

gcl -ide D:\GnuchanOS\language\src\IDE


#// this is only in gcl -ide
gcl -build project.gcdata -o path/dir or in ide gui
    Project_name.exe
    Project_name.gcBundle #// all runtime and dll or so in this place works like redot engine pack

    can't include Programs/*.dll only scripts and runtimes


``` gcl language D:\GnuchanOS\language\src\SharedPipeline, D:\GnuchanOS\language\src\GCL\SimpleRunner

vanilla c

int main(int argc, char *argv[]) { return 0; }
int main(int argc, char **argv) { return 0; }

char
short, int, float, double, long int, long long int, long double
unsigned int

&&   AND
||   OR
^   XOR
~   NOT
<<  left shift
>>  right shift

sizeof()
sizeof() / sizeof()
strlen()

gcl
int8, int16, int32, int64, int128
float16, float32, float64, float128
uint8, uint16, uint32, uint64, uint128
gcChar "UTF-8 Lenght real string"

# NUMERIC LITERALS

    42         decimal
    0x1F       hexadecimal
    1.5        decimal fraction
    1e30       exponent (C99): 'e'/'E', optional sign, digits
    2.5e-1     -> 0.25      1.5E-3 -> 0.0015      2e+8 -> 200000000

    The exponent marker is consumed only when a digit follows the optional
    sign, so `1else` is still reported as "numeric literal is immediately
    followed by 'e'" and never read as a truncated exponent. The l/L, u/U and
    f/F suffixes follow the exponent (`1e3f`). Every value is a double at
    runtime and the declared type clamps it on the way in; a uint64/uint128
    literal keeps its exact digits for printing only while those digits are a
    plain run of decimal digits that actually fit the type.

+ - / * %
++ --
+= -= *= /= %= &= |= ^=
= == != < > <= =>

EXAMPLE: printf("NAME: {} Age: {} Point: {.2f}", name, age, point); there is no %s like
EXAMPLE: scanf(name); #// backend is not scanf more safe alternatif

#define
#if, #ifdef, #ifndef, #elif, #else, #endif
#undef
#warning ..., ... #// yellow color (diagnostic: compilation continues)
#error ..., ...   #// red color + COMPILATION STOPS (exit 1, no statement runs)
#debug ..., ...   #// blue color (diagnostic: compilation continues)

# LINK
#include <script.gcsf>
    classic  like call

        int pastes = 0;
        #include <inc_plain.gcsf>      # inc_plain.gcsf has NO guard
        #include <inc_plain.gcsf>
        printf("pastes={}\n", pastes);    # -> pastes=2

    Opt in to include-once by putting `#pragma once` in the INCLUDED file.
    Leading whitespace (spaces/tabs) before the `#` is allowed, and the
    directive may sit anywhere in the file — not only on the first line.

        #pragma once                       # inc_once.gcsf
        pastes = pastes + 1;

        int pastes = 0;
        #include <inc_once.gcsf>
        #include <inc_once.gcsf>         # skipped — already included once
        printf("pastes={}\n", pastes);    # -> pastes=1

    Limits and guarantees (all measured, all pinned by tests):
      - The "already included" registry is keyed on the RESOLVED path and is
        reset for every program run.
      - The registry GROWS with the program: there is no fixed limit on how
        many distinct `#pragma once` files a run may remember. (It used to be a
        fixed 64-entry table that stopped registering silently, so the 65th
        guarded file silently lost its guard. That table is gone; memory is now
        proportional to the number of distinct guarded files.)
      - An `#include` inside an INACTIVE branch is never looked up and never
        pasted: `#if 0 ... #include <x> ... #endif` is dead code, exactly as
        in C. Only an ACTIVE include is resolved and text-pasted.
      - `#include` nesting is limited to 32 levels; exceeding it STOPS the
        build (a diagnostic, then `Error: #include failed, compilation
        stopped`, empty stdout, exit 1) instead of overflowing the native
        stack.
      - A missing `#include` STOPS the build the same way — exit 1, no
        statement runs — and the diagnostic names the file and the
        directories that were searched.
      - `#native <X>` that cannot be loaded is reported at STARTUP (not only
        at the first call) and makes the exit code non-zero — even if no
        member is ever called. To load a module only on some platforms, guard
        the directive with `#if`, do not rely on the diagnostic staying quiet.

    Golden tests: language/tests/gcsf/preproc/double_include.gcsf (pastes=2),
    language/tests/gcsf/preproc/pragma_once.gcsf (pastes=1),
    language/tests/gcsf/preproc/pragma_once_many.gcsf (three distinct guarded
    fragments, each included twice in a non-sequential order -> hits=111),
    language/tests/gcsf/modules/missing_module.gcsf (unknown module -> exit 1),
    language/tests/gcsf/preproc/include_inactive.gcsf (a dead include is not
    pasted), include_inactive_missing.gcsf (a dead include is not even looked
    up), include_missing_active.gcsf (a live missing include aborts),
    include_cycle.gcsf (self-include aborts at the depth limit).
    ``` Test.gclib
    type function(....) {
        
    }
    ```
#native <Math>
    Math.member --> from .dll or .so

#extern <raylib.dll>
    #register void InitWindow(int width, int height, const char *title);

# public or private

public variable --> can be call
private variable --> only local
const

# GLOBAL or INLINE variables

type variable = 30;
type func() {
    global variable;
    return variable + 30;
}

type func() {
    inline variable; local variable
    typep variable = 30;
    
    return variable + 30;
}
 
# BOOLEAN
if () {

} else if () {

} else {

}

switch () {
    case 1:
        ...
        break;
    default:
        ...
        break;
}

# loops

for (int i = 0; i < size; i++) {

}

while (...) {

}

# try / catch / finally / throw

try {
    int a = 10 / 0;              // runtime error: division by zero (GCL3008)
} catch (err) {
    printf("error: {} ({})\n", err.message, err.code);
} finally {
    // always runs, whether an error occurred or not
}

throw "something went wrong";    // raise "..." is an alias of throw

# catch variable fields

err.message    // text of the error
err.code       // GCL3xxx diagnostic code: 3010 out of bounds,
               // 3003 undefined variable, 3008 division by zero,
               // 3009 possible infinite loop, 3014 throw, ...
err.line       // source line (1-based)
err.col        // source column (1-based)

# error handling behaviour

// - A runtime error raised inside `try` is CAUGHT: it prints nothing, does
//   not raise the exit code, and execution continues in `catch` (which may
//   be omitted when `finally` is present).
// - `finally` runs on EVERY exit path of its `try`: the normal end of the
//   block, a caught error, an error that is NOT caught, and `return`, `break`
//   or `continue` used inside the `try` (or inside the `catch`) body:

int f() {
    try {
        return 1;                     // `finally` still runs before this returns
    } finally {
        printf("cleanup\n");
    }
}

// - A `return` / `break` / `continue` issued BY the `finally` body WINS over
//   the one that was in flight; an error raised by `finally` REPLACES any
//   error already in flight (the rule Python's `finally` follows).
// - A braced cleanup block (`finally { ... }` and `defer { ... }`) runs on
//   those exit paths too - the cleanup is executed in a clean transfer state.
// - An uncaught `throw` STOPS the program: the message goes to stderr and
//   the exit code becomes non-zero (like Python).
// - An ordinary runtime error that is NOT caught keeps the legacy tolerant
//   behaviour: it is reported to stderr, the exit code becomes non-zero, and
//   execution continues.
// - A native call whose arguments raise the error is NOT executed at all, so
//   it cannot print partial output.
// - Errors are catchable regardless of kind: division by zero, modulo by
//   zero, array out of bounds (read and write), negative index, undefined
//   variable, unknown member/module/function, wrong argument count,
//   assignment to a `const`, assignment to a number literal, `break` outside
//   a loop or switch, and a runaway loop.
// - The VALUE a failed expression leaves behind is defined, not garbage:
//     * division by zero yields inf (or nan for 0/0) and modulo by zero
//       yields nan - C semantics, and arithmetic keeps working afterwards;
//     * storing one of those non-finite values into an INTEGER type yields
//       `0`, for every integer type alike (int, short, char, long long,
//       int8..int128, uint8..uint128, uint64). A float/double target keeps
//       nan/inf instead, because narrowing a double to a float preserves the
//       value where narrowing it to an int cannot.
//   The diagnostic reports the problem; these results exist so the stored
//   value is predictable. Pinned by expressions/compound_assign_zero.gcsf.
// - Storing a FINITE value that does not fit the target type is defined as
//   well, and the rule splits at exactly the line where C stops defining the
//   conversion:
//     * INSIDE long long (|v| < 2^63) the ordinary C conversion is applied,
//       because C defines it: `int8 x = 200;` is -56, `uint8 x = -1;` is
//       255, `int x = 3e9;` wraps negative.
//     * OUTSIDE long long (|v| >= 2^63) C is undefined, so the value saturates
//       to the DECLARED TYPE's own floor/ceiling rather than taking the low
//       bits of a 64-bit clamp: `int64 x = 10^30;` stores LLONG_MAX (which as
//       a double IS 2^63, printed 9.2233720368547758e+18), `int x = 10^30;`
//       stores INT_MAX 2147483647, `int8 x = 10^30;` stores 127, and
//       `uint64 x = -10^30;` stores 0. The declared type's cast still runs
//       afterwards, exactly as it does for an ordinary C conversion.
//   A float/double target is untouched by any of this and keeps the value
//   (10^30 stays 1e+30). Decimal literals accept an exponent (TURN 33):
//   `1e30` IS 10^30 and `2.5e-1` is 0.25, so the value can be written either
//   way - the pinned case keeps the long spelling. Pinned by
//   expressions/out_of_range_finite.gcsf and
//   expressions/exponent_literal.gcsf.
// - A compound assignment (a += b, a %= b, ...) is exactly `a = (a OP b)`, so
//   it behaves like the plain operator in every respect: `%=` on a fractional
//   value uses fmod and keeps the fraction, and `%=`/`/=` by zero raise the
//   same GCL3008 error. Applied to a text variable the ordinary numeric
//   coercion applies (the string reads as its number, the result is stored
//   back as text): `gcChar s = "abc"; s %= 2;` leaves "0", and a PLAIN
//   `s = 3;` leaves "3" - the compound form is not a special rule. Pinned by
//   expressions/compound_assign_double.gcsf and compound_assign_string.gcsf.
// - A runaway loop is detected after 1,000,000 iterations of a single `while`
//   or `for`. Inside `try` it is caught as GCL3009, so the loop is abandoned
//   and execution resumes in `catch`. Without `try` the loop is unrecoverable:
//   the message goes to stderr and the program stops with a non-zero exit
//   code. The budget can be raised for long-running programs (a game loop is
//   not a bug) with the environment variable GCL_MAX_LOOP_ITERATIONS.

# defer

defer Cleanup();          // runs when the enclosing block ends (newest first)

# defer — cleanup that always runs

// `defer <statement>;` registers an action for the block it appears in. The
// action runs when that block ends, in REVERSE order (LIFO) — the order C++
// destructors unwind in. It runs on EVERY exit path:
//
//   - the normal end of the block,
//   - `break` and `continue`,
//   - `return` (the function body is a block),
//   - while an error UNWINDS towards its `try`.
//
// The block's own variables are still alive while the action runs, so it can
// use the resource it is closing. That is the whole point of `defer`:

int fd = 0;

int load(char *path) {
    fd = Stdio.openFile(path);
    defer Stdio.closeFile(fd);       // runs on the `return` below
    if (fd < 0) { return 0; }        // ... and on this early exit too
    return 1;
}

// One loop ITERATION is a frame of its own, so an action inside a loop body
// runs once per iteration, not once per loop:

for (int i = 0; i < 3; i++) {
    defer printf("release {}\n", i); // release 0, release 1, release 2
}

// A `defer` inside a braced body belongs to THAT block, so it observes the
// value the block left behind:
int w = 0;
while (w < 2) {
    defer printf("{} \n", w);        // prints 1, then 2 (w++ is inside)
    w = w + 1;
}

// A single-statement body (no braces) has no block of its own, so its action
// belongs to the ENCLOSING block. Use braces when you want block scope.
// A `defer { ... }` block is also accepted and follows the same
// every-exit-path rule as the single-statement form - including `return`,
// `break` and `continue`, and it runs before the `finally` of an enclosing
// `try`. A program-level `defer` runs after `main()` finishes.
//
// Rules:
//   - actions run newest-first, LIFO;
//   - an action that itself fails REPLACES the error in flight and the
//     remaining actions are skipped (the same rule Python's `finally` uses);
//   - `defer` must be followed by a statement: `defer;` is a parse error.

# typedef

typedef int number;
number x = 10;

#// no typedef examples
enum Day { MONDAY, TUESDAY, WEDNESDAY };
enum Day today;

struct Student { char name[20]; int age; };
struct Student student1;
student1.age = 20;

#// with typedef examples
typedef enum { MONDAY, TUESDAY, WEDNESDAY } today;
Day today;

typedef struct { char name[20]; int age; } Student;
Student student1;
student1.age = 20;

``` EXAMPLE STRUCT
#native <Stdio>
#native <Math>



typedef struct hello {
    int testo;
    float rollo;
    char tollo[32];
    gcChar name;
} Hello;


int main() {

    int8 number = Math.randInt(30, 90);
    gcChar longText = "very very very long text";


    // 1. Create empty and assign fields using dot notation
    Hello a;
    a.testo = 10;
    a.rollo = 20.5;
    a.tollo = "hello";
    a.name = "GNUCHANOS";


    // 2. Create using a brace initializer
    Hello b = {
        20,
        30.5,
        "world",
        "ALPHA"
    };


    // 3. Create using braces with named fields
    Hello c = {
        .testo = 30,
        .rollo = 40.5,
        .tollo = "test",
        .name = "BETA"
    };


    // 4. Initialize only specific fields
    Hello d = {
        .name = "GAMMA"
    };


    // 5. Create using variables
    int x = 50;
    float y = 60.5;
    gcChar text = "dynamic";

    Hello e = {
        x,
        y,
        text,
        "DELTA"
    };


    // 6. Copy from another struct
    Hello f = e;


    // 7. Create with default values, then assign fields
    Hello g = {};

    g.testo = 70;
    g.rollo = 80.5;
    g.tollo = "filled";
    g.name = "EPSILON";


    // 8. Create from a function return value
    Hello makeHello() {
        return {
            90,
            100.5,
            "function",
            "ZETA"
        };
    }

    Hello h = makeHello();


    Stdio.printf("A: {} \n", a.name);
    Stdio.printf("B: {} \n", b.name);
    Stdio.printf("C: {} \n", c.name);
    Stdio.printf("D: {} \n", d.name);
    Stdio.printf("E: {} \n", e.name);
    Stdio.printf("F: {} \n", f.name);
    Stdio.printf("G: {} \n", g.name);
    Stdio.printf("H: {} \n", h.name);

    Stdio.printf("Number: {} \n", number);

    return 0;
}
```




# functions
void func(....) {
    no return
}

type return_func(....) {
    return variable;
}

# typedef struct + func

typedef struct {
    char name[20];
    int age;
} Student;

Student getStudent() {
    Student s = {"John", 20};
    return s;
}

Student student1;
student1 = getStudent();

printf("Name: {} \n", student1.name);
printf("Age: {} \n", student1.age);



```


``` Math.dll, .so 

#native <Math> D:\GnuchanOS\language\src\Modules

Math.randInt(0, 99); Math.randint(0, 99);
Math.randFloat(0, 99); Math.randfloat(0, 99);

Math.min(0, 99); Math.max(0, 99);
Math.abs(-99);

Math.floor(9.99); Math.ceil(9.01); Math.round(9.5);

Math.sqrt(99);
Math.pow(2, 8);

Math.sin(90); Math.cos(90); Math.tan(90);
Math.asin(1); Math.acos(1); Math.atan(1);

Math.log(10); Math.log10(100); Math.exp(2);

Math.clamp(150, 0, 100);
Math.sign(-99);

Math.randBool();
Math.randChoice(1, 2, 3);
Math.randSign();

```

``` Stdio.dll, .so

#native <Stdio> D:\GnuchanOS\language\src\Modules


Stdio.printf();
Stdio.scanf("%", ...); #// full input and safe backend

# // PLACEHOLDERS: `{}`, `{ }` (spaces allowed) and `{.Nf}` (`N` = number of
# //   digits, e.g. `{.2f}`). `{}` prints the argument as TEXT; `{.Nf}` prints it
# //   NUMERICALLY with N decimals:
# //       printf("[{}] [{.2f}]\n", "1.5", 1.5);   # -> [1.5] [1.50]
# //
# //   `{.Nf}` ON A char - THE DIGIT BOUNDARY (read this before using it):
# //     Module arguments arrive as TEXT, and WHICH text depends on the ARGUMENT,
# //     not on printf:
# //       - a char / gcChar VALUE carries its CHARACTER text  ->  "A", "7"
# //       - a `'x'` LITERAL carries its numeric text: '7' IS 55 in C, so it
# //         carries "55" (and 'A' carries "65")
# //       - a `(char)N` CAST carries the character for N -> (char)65 is "A"
# //       - an int / float carries its number text -> 7 is "7", 1.5 is "1.5"
# //     In a NUMERIC context the module reads that text as:
# //       - ONE character that is NOT a digit -> its CHARACTER CODE, the same
# //         rule as `int m = c` and `int n = s[1]`:
# //             gcChar c = "A";  printf("{.2f}", c);   # -> 65.00  (NOT 0.00)
# //       - ONE character that IS a digit -> the NUMBER: "7" is both the number 7
# //         and the character '7', and the number reading is what a real numeric
# //         argument needs, so that one wins:
# //             char e = '7';    printf("{.2f}", e);   # -> 7.00
# //       - anything else -> the ordinary number read (`atof`): "12" is 12.00,
# //         "1.5" is 1.50, non-numeric text is 0.00
# //     THE BOUNDARY IN ONE LINE: `{.2f}` on a char VARIABLE holding '7' prints
# //     7.00, but on the bare `'7'` LITERAL it prints 55.00 - the literal is the
# //     code 55, not a character value:
# //         printf("{.2f}", '7');                  # -> 55.00
# //     This is a limit of the text-based module ABI, not of printf. Pinned by
# //     language/tests/gcsf/stdio/char_format_dot.gcsf (15 contracts).

Stdio.openFile(file="file"); Stdio.openfile(file="file");
Stdio.writeFile(file="file", text=""); Stdio.writefile(file="file", text="");
Stdio.readFile(file="file"); Stdio.readfile(file="file");
Stdio.closeFile(file="file"); Stdio.closefile(file="file");

Stdio.appendFile(file="file", text=""); Stdio.appendfile(file="file", text="");

Stdio.fileExists(file="file"); Stdio.fileexists(file="file");
Stdio.deleteFile(file="file"); Stdio.deletefile(file="file");
Stdio.renameFile(file="file", newName=""); Stdio.renamefile(file="file", newName="");

Stdio.fileSize(file="file"); Stdio.filesize(file="file");
Stdio.flushFile(file="file"); Stdio.flushfile(file="file");

# // RETURN VALUES
# //   openFile(file[, mode])  -> OPEN HANDLE (>= 1), or -1 when it cannot open.
# //       mode defaults to "r"; "w"/"a"/"r+" give write/append/update.
# //   closeFile(handle)       -> 1 closed, 0 invalid or already closed.
# //   writeFile/appendFile    -> 1/0. The first argument is a HANDLE (the file
# //       is not reopened) or a PATH; both forms are accepted.
# //   readFile(handle_or_path)-> number of BYTES READ, or -1 on failure. The
# //       bytes land in the last-string buffer and are read back with
# //       Stdio.LastStringLen() and Stdio.LastStringByte(i) — byte by byte,
# //       the same convention the Raylib text members use.
# //   fileExists/fileSize/deleteFile/renameFile/flushFile -> as named.
```

``` Embed

#native <Embed> D:\GnuchanOS\language\src\Modules

# // Start the Python or Lua runtime
Embed.Run(type="python");
Embed.Run(type="lua");

# // Stop the active runtime
Embed.Stop(type="python");
Embed.Stop(type="lua"); 

# // Check whether the runtime is active
type isActive = Embed.IsActive(type="python");   # // 1 / 0
type isActive = Embed.IsActive(type="lua");      # // 1 / 0

# // Get and send variables
# // GetValue(type, name) → reads the value of the named global variable (returns a double)
# // SendValue(type, name, value) → writes a value to the named global variable
# // Note: on the Python/Lua side, the variable must be defined in __main__ globals.

type value = type;

# // Example: send a value to Python
Embed.SendValue(type="python", name="myVar", value="42");
# // In Python: myVar = 42

# // Example: read a value from Python
type val = Embed.GetValue(type="python", name="myVar");

# // Example: send a value to Lua
Embed.SendValue(type="lua", name="myVar", value="3.14");
# // In Lua: myVar = 3.14

# // Example: read a value from Lua
type val = Embed.GetValue(type="lua", name="myVar");

# // Real-time variable sharing (usable inside a loop)

# Warning: int8, float16, and similar types will be added later; for now the values sent to Python and Lua
# are only int, float, char, and eventually gcChar.
```
