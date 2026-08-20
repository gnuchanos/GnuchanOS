# GCL is HUB LANGUAGE

# Part Part System -> Part 1, Part 2


# Part 1

## Giriş (Intro)

-flags yerine her turlu flaglar gelebilir fark etmiyor

```
gcl -flags main.gcsf -o path/output yada sadece output
    # gcc codegen

gcl -flags -run path/main.gcsf yada main.gcsf
```

gnu/focus language but it's can be support windows only no mac or android

`D:\GnuchanOS\language\BUILD` output gcl, .gcsf, .gclib, .so, .dll, .a dosyalarinin olucagi yer burasi proje test emek icin gereksiz compiler coplerini tutma

### flags

```
gcl -lexer file.gcsf yada file.gclib
gcl -parser file.gcsf yada file.gclib
gcl -ast file.gcsf yada file.gclib
gcl -ir file.gcsf yada file.gclib
gcl -debug -run file.gcsf

gcl -linclude path -llib path -lextern path -run main.gcsf
```

### all types

```
.gcsf   #// script file
.gclib  #// library/module
.gctf   #// text file
.gcdata #// json ile file + #// comment line support
```

## Language Algorithm

```
GCL Source #// .gclib/extern -> .gcsf/extern -> main.gcsf
     │
     ▼
src/
├── FastIR/
│
├── SharedPipeline/
│   ├── Linker/
│   ├── Lexer/
│   ├── Parser/
│   ├── Semantic/
│   ├── TypeChecker/
│   ├── Memory/
│   ├── GarbageCollector/
│   ├── AST/
│   ├── Diagnostics/
│   ├── Ir/
│   └── Common/

.gclib/extern -> include .gcsf/extern -> main.gcsf
SharedPipeline -> FastIR
```

## memory management

```
            Program
                │
    ┌───────────┼───────────┐
    │           │           │
    Stack       Heap      Static Data
    │           │
    │      ┌────┼─────┐
    │      │    │     │
    Local  Arena Pool Custom
```

Her şeyde OOM + null + bounds + overflow kontrolü

## hata raporlama

python like system

## Dil Sözdizimi (Syntax)

### comment

```
# comment
#| multi
line
comment |#
```

### classic c like include

```
#include "script.gcsf" yada <script.gcsf>
    how to call: member
```

### for moduler system

```
#lib "library.gclib" yada <library.gclib>
    how to call: library.member
```

### .so, .dll, .a

```
#extern "raylib.dll" yada <raylib.so>
    #register void InitWindow(int width, int height, const char *title);
    #register void CloseWindow(void);


    how to call: InitWindow(300, 300, "uwu");
```

### macros

this macros only for .gclib and include .gcsf not for main.gcsf

```
#register --> from #extern <raylib.dll> -> .so, .dll -> #register type function()
#ifdef
#ifndef
#if
#elif
#else
#endif
```

`#if` --> gnuLinux, gnu_linux, gnu, linux, windows

you can use in main.gcsf, include .gcsf and .gclib

```
#define   --> normal
#undef
#warning "text", "text", variable, variable --> yellow color text
#error   "ERROR!!" --> red color text
#debug   "error message" --> blue flush after error
```

### output

```
#debug ..., ... #// mavi
#warning ..., ... #// sari
#error ..., ... #// kirmizi
```

### main girişi (main())

kabuledilen main girisi sadece main.gcsf icin #include .gcsf icin gereksiz

```
int main(void) { return 0; }
int main() { return 0; }
int main(int argc, char *argv[]) { return 0; }
int main(int argc, char **argv) { return 0; }
int main(int argc, char *argv[argc]) { return 0; }
```

### global değişkenler

```
#// in main.gcsf global variable
int global_variable = 10;

int main() {
    #// in main.gcsf file local variable
    int local = 10;
}
```

# Part 2


## GCL Temelleri (GCL 101)

```
public type identifier = value;   #// you can acces in everyware
private  type identifier = value; #// only in folder

const type identifier = var;

inline identitiy
global identitiy

type identifier = variable;
type identifier1, identifier1, identifier1 = variable;

void print_this_global() {
    global gx;

    printf("Global GX: %d \n", gx);
}

void print_this_local() {
    inline gx;

    int gx = 31;

    printf("local GX: %d \n", gx);
}

int gx = 10;
print_this_global();
print_this_local();

type a = var;
pointer ->, &
```

# Part 3

## Veri Tipleri (Types)

### PRINTF

```
printf("format", arguments...);
```

```
// Signed integers
printf("int8:        %d\n", number1);
printf("int16:       %d\n", number2);
printf("int32:       %d\n", number3);
printf("int64:       %lld\n", number4);
printf("int128:      %lld\n", number5);

printf("short:       %hd\n", number6);
printf("int:         %d\n", number7);
printf("long:        %ld\n", number8);
printf("long long:   %lld\n", number9);

// Unsigned integers
printf("uint8:             %u\n", unumber1);
printf("uint16:            %u\n", unumber2);
printf("uint32:            %u\n", unumber3);
printf("uint64:            %llu\n", unumber4);
printf("uint128:           %llu\n", unumber5);

printf("unsigned short:    %hu\n", unumber6);
printf("unsigned int:      %u\n", unumber7);
printf("unsigned long:     %lu\n", unumber8);
printf("unsigned long long:%llu\n", unumber9);

// Floating point
printf("float16:       %f\n", fnumber1);
printf("float32:       %f\n", fnumber2);
printf("float64:       %f\n", fnumber3);
printf("float128:      %Lf\n", fnumber4);

printf("float:         %f\n", fnumber5);
printf("double:        %f\n", fnumber6);
printf("long double:   %Lf\n", fnumber7);



```

```
Length Modifiers
----------------
hh    char
h     short
l     long
ll    long long
z     size_t
```

```
Width & Precision
-----------------
%5d    Minimum width
%-5d   Left align
%05d   Zero padding
%.2f   Precision
%8.2f  Width + Precision
```

### vanilla c standard

```
short, int, float, double
long long int
long double

unsigned
unsigned long int

char != gcChar ayni degil amac enazindan geri donuk c uyumlulugunu tutmak
    char is regular vanilla 1 byte char
    gcChar UTF-8

bool

char *text = null;

sizeof(type) or sizeof(variable)
```

```
+ - * / %

++ --
+= -=
== !=

>, <, >=, <=

&&, ||, !, &, |, ^, ~, <<, >>

pointer  &, *
```

```
\n
\t
\r
\\
\"
\'
```

### SCANF -> Safer than vanilla C

```
scanf("%type", text);
```

```
#// Runtime checks:
#// - Detect buffer overflow.
#// - Prevent writing past the buffer.
#// - Emit a #warning if the input exceeds the buffer size.
#// - Truncate the input safely.
#// - Always null-terminate the string.
```

### gcl standard

```
int8   number1 = 10;
int16  number2 = 100;
int32  number3 = 1000;
int64  number4 = 10000;
int128 number5 = 100000;

float16  number6 = 10.30;
float32  number7 = 10.300;
float64  number8 = 10.3000;
float128 number9 = 10.30000;

uint8   number1 = 10;
uint16  number2 = 100;
uint32  number3 = 1000;
uint64  number4 = 10000;
uint128 number5 = 100000;
```

# Part 4

### this is not vanilla char

```
char TEXT   = 'a'; #// 1 utf-8 char
```

#### Stack UTF-8 string (immutable)

```
gcChar TEXT[] = "this is lenght + UTF-8 based char system focus more safe stack use with const";
```

- UTF-8 encoded
- Length-aware
- Stack allocated
- Immutable
- Fixed size

#### Heap UTF-8 string (mutable)

```
gcChar *TEXT = "this in heap UTF-8 string real strings";
```

- UTF-8 encoded
- Length-aware
- Heap allocated
- Mutable
- Dynamic size

otomatik program sonu free olucak yada opsiyonel TEXT.free(); -> TEXT null olucak yeni deger almadan kullanilamaz
bu demektir ki kullanici if (TEXT != null) {} yapmali

### Simple Array Types

#### VANILLA C

```
char alphabet[] = {'a', 'b', 'c'};
char game[]     = "Half Life 3";
char games[][32]  = {
    "half life 3",
    "left 4 dead 3",
    "team fortress 3"
};

char ***games = malloc(sizeof(char ***) * 4); #// 3D malloc
```

```
#// Runtime checks:
#// - Check allocation size.
#// - If the requested size exceeds the maximum limit,
#//   automatically allocate a larger block and emit a #warning.
#// - Detect allocation failure.
```

```
#// Memory management:
#// - Call free(games); before program exit.
#// - If free() is called twice, emit a #warning.
#// - After free(), automatically set games -> null.
```

#### VANILLA GCL

```
gcChar a = 'a';                      #// utf-8 char
gcChar game[] = "Half Life 3";
gcChar *game = "Half Life 3";

# type identifier[] @dimension_size = {}

gcChar alphabet[] @1 = {'a', 'b', 'c'}; #// utf-8 char array/list
```

```
#// 0D boyutun uzunlugunu [] ile belirliyorsun
#// @2 boyut kacinci yuseklige cikar max belirlemek
#// 0D binanin giris kati @2 ise ikinci kati demek
#// [][][][] yok = @4 var
type identifier[] @2 = {
    #//     0D boyut
    {
        #// 1D boyut
        {
            #// 2D boyut
        }
    },
}

gcChar gamesgames[container size][inside container char size] @3 = {
    { "game 0", { "info", { "Half-Life",      1998, 10 } } },
    { "game 1", { "info", { "Portal",         2007, 10 } } },
    { "game 2", { "info", { "Counter-Strike", 2000,  9 } } },
    { "game 3", { "info", { "Left 4 Dead 2",  2009, 10 } } }
};
    #// : number means dimension size

gcChar *gamesgames[] @3 = { #// : number means dimension size
    { "game 0", { "info", { "Half-Life",      1998, 10 } } },
    { "game 1", { "info", { "Portal",         2007, 10 } } },
    { "game 2", { "info", { "Counter-Strike", 2000,  9 } } },
    { "game 3", { "info", { "Left 4 Dead 2",  2009, 10 } } }
};
    #// no need to malloc
```

# Part 5

#### numbers

```
type identifier[] @1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
```

```
#define MAX_PLAYER 10
typedef struct Players {
    gcChar *name; #// give you regular string size and more runtime safe
    int8    age;
} Player;
```

2 way do this array

#### first way regular molloc

```
Players *Player_List = malloc(reserve=sizeof(Player*) * MAX_PLAYER);

Player[index].Name
Player[index].age

free(Player_List); #// do free and make it null
```

#### second way is easy and safe way

```
Player *Player_List = gcMalloc(reserve=MAX_PLAYER, extra=10);
```

```
# Creates an array with MAX_PLAYER capacity.
# If the array becomes full, automatically expands by +10 elements.
# Example: 10 -> 20 -> 30 -> 40 -> ...

# Memory management:
# - You may call free(Player_List) manually.
# - If you don't, GCL automatically frees the memory before the program exits.
```

# Part 6

### typedef

```
typedef int32_t int32;
typedef unsigned int uint;
typedef char* string;
```

only for GCL

```
typedef == equals;
typedef != notEquals;

typedef && and;
typedef || or;
typedef !  not;

typedef &  bitAnd;
typedef |  bitOr;
typedef ^  bitXor;
typedef ~  bitNot;

typedef << leftShift;
typedef >> rightShift;
```

# Part 7

### enum

```
enum Color {
    RED,
    GREEN,
    BLUE
};

enum Color _RED = RED;
```

# Part 8

### struct

```
struct Player {
    char name[32];
    int health;
    float speed;
};

struct Player thisPlayer = {
    .name   = "GnuChanOS",
    .health = 100,
    .speed  = 20 
};
```

call> thisPlayer.name

### typedef enum and struct

```
typedef struct Player {
    char name[32];
    int health;
    float speed;
} Player;

Player thisPlayer = { ... };

typedef enum Color {
    RED,
    GREEN,
    BLUE
} Color;

Color _RED = RED;
```

# Part 9

### tuple

```
tuple mix = ('a', "half life 3", 10, 300.2000, 33);
```

### dict

hizli obje olusturma

```
typedef struct Vector2 {
    float x;
    float y;
} Vector2;
```

direk objenin kendisini olusturma ve hemen kullanma

```
dict player = {
    gcChar name : "gnuchanos",
    Vector2 position: {300, 300},
    int8 scale: 30,
    gcChar *favGame[]: {"half life 3", "left 4 dead 3", "team fotress 3"}
};
```

# Part 10

## Akış Kontrolü (Control Flow)

### boolean

```
if (...) {
    ...
} else if (...) {
    ...
} elif () {

} else {

}

switch() {
    case:

        break;

    default:

        break;
    
}
```

# Part 11

### for

```
for(int i = 0; i < siz; i++) {
    ...
}
```

break, continue, return

# Part 12

### while

```
while (...) {

}

do {

} while(...) {

}
```

break, continue, return

# Part 13

### functions

```
void function(..., ...);
type function(..., ...);
```

# Part 14

## class

### 101

```
class no_head() {
    void call() {}
} #// no_head.call()

class with_head() {
    void head() {
        int age = 30;

        return 0;
    }

    void call() {  }
} #// with_head().call(), .age
```

### real example

```
class FATHER() {
    @return
    gcChar Call() {
        return "where are you";
    }
}

class CHILD(FATHER) {
    void head(gcChar name, int8 age) {
        gcChar name = name;
        int8   age  = age;

    
        return 0;
    }

    void talk() {
        #// @return -> no need () only return
        gcChar what = FATHER.Call;

        if (what == "where are you") {
            printf("i'm coming father!! \n");
        }

    }

    int age() {
        return age;
    }
}


FATHER ThisFather = FATHER(); #// no head means ()
CHILD  ThisChild  = CHILD(ThisFather);

printf("FATHER CALL: %s", ThisFather.Call());

printf("Child Name: %s \n", ThisChild.name);
printf("Child Age: %d \n", ThisChild.age());
printf("FATHER CALL From Child No Header: %s \n", ThisChild.Call());
printf("FATHER CALL From Child With Header: %s \n", ThisChild.talk());
