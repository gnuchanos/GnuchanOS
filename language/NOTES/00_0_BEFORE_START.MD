# file types

  .gcsf   → gnuchan script file  (source code)
  .gclib  → gnuchan library      (library module)
  .gctf   → gnuchan text file    (document / data)
  .gcdata → JSON-style config    (project settings)

  #lib <module_name>      → loads module_name.gclib
  #include <file_name>    → imports file_name.gcsf
  #extern <library>       → links external C library .dll / .so / .a
          extern void my_c_function(); → specifies C ABI usage

# pygon
import file, file, file
import folder in file, file, file
from file import things, things, things
from folder import file, file, file

extern file.dll

# cygon
#extern <file.dll> // .dll, .so, .a
#lib    <file>     // .gclib texfile
#include <file>    // .gcsf

pygon "import" = cygon "include, lib"

shared library pipeline

##### gcl manin parameters

# how parameter works -> gcc -flag value -flag value --> like gcc

gcl -> interactive shell
gcl -version, -v

# path link extra options
gcl -linclude path/folder
gcl -llib path/folder
gcl -lextend path/folder

# debug
gcl -lexer file.gcsf
gcl -parser file.gcsf
gcl -ast file.gcsf
gcl -ir file.gcsf
gcl -codegen file.gcsf

# export or run
gcl -run path/file.gcsf
gcl -all_flags file.gcsf -o path/output

# extra export
gcl -wasm raylib
gcl -wasm binding
gcl -wasm export

gcl -debug run
gcl -version
gcl -help

# luarun
gcl -luarun -> lua55.dll, lua55.so
    gcl -luarun file.lua -dll, -so path/lua55.dll or -so path/lua55.so
        -dll -> missing path or file
        -so  -> missing path or file













