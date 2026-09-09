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

+ - / * %
++ --
+= -= *= /=
= == != < > <= =>

EXAMPLE: printf("NAME: {} Age: {} Point: {.2f}", name, age, point); there is no %s like
EXAMPLE: scanf(name); #// backend is not scanf more safe alternatif

#define
#if, #ifdef, #ifndef, #elif, #else, #endif
#undef
#warning ..., ... #// yellow color
#error ..., ...   #// red color
#debug ..., ...   #// blue color

# LINK
#include <script.gcsf>
    classic  like call
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
```

``` Embed

#native <Embed> D:\GnuchanOS\language\src\Modules

# // Python veya Lua runtime'ı başlat
Embed.Run(type="python");
Embed.Run(type="lua");

# // Çalışan runtime'ı durdur
Embed.Stop(type="python");
Embed.Stop(type="lua"); 

# // Runtime'ın aktif olup olmadığını kontrol et
type isActive = Embed.IsActive(type="python");   # // 1 / 0
type isActive = Embed.IsActive(type="lua");      # // 1 / 0

# // Değişken alma ve gönderme
# // GetValue(type, name) → adı verilen global değişkenin değerini okur (double döner)
# // SendValue(type, name, value) → adı verilen global değişkene değer yazar
# // Not: Python/Lua tarafında değişken __main__ globals içinde tanımlı olmalı.

type value = type;

# // Örnek: Python'a değer gönder
Embed.SendValue(type="python", name="myVar", value="42");
# // Python'da: myVar = 42

# // Örnek: Python'dan değer al
type val = Embed.GetValue(type="python", name="myVar");

# // Örnek: Lua'ya değer gönder
Embed.SendValue(type="lua", name="myVar", value="3.14");
# // Lua'da: myVar = 3.14

# // Örnek: Lua'dan değer al
type val = Embed.GetValue(type="lua", name="myVar");

# // Gerçek zamanlı değişken paylaşımı (loop içinde kullanılabilir)

# Warning: ileride int8, float16 gibi değerler olacak; fakat Python ve Lua'ya
# gönderilecek değerler şimdilik sadece int, float, char ve ileride gelecek
# gcChar olacak.
```
