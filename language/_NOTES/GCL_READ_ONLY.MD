# GCL — Gnuchan C-Like Language (GNU99)

# Gnuchan Language System — Before Start

- **Güvenli C** — runtime safety eklenmiş, debug her zaman net
- **Sıfır maliyet** — debug kapalıyken hiçbir overhead yok

# ==========================================================
## CLI Parameters
# ==========================================================

# GCL FLAGS
gcl -flag value or gcl -flag value value value

gcl                     → interactive shell
gcl -version, -v        → version info
gcl -help, -h           → help

# Path / Library
gcl -linclude path      → extra include path
gcl -llib path          → extra library path
gcl -lextend path       → extension module path

# Debug / Pipeline
gcl -lexer file.gcsf    → token stream
gcl -parser file.gcsf   → parse tree
gcl -ast file.gcsf      → AST dump
gcl -ir file.gcsf       → IR dump
gcl -codegen file.gcsf  → generated C code
gcl -istedigin_kadar_flag_girebilirsin file.gcsf -o out → full pipeline to executable

gcl -debug

# ==========================================================
# COMPILER PIPELINE
# ==========================================================
                    [Source Code]
                          │
                          ▼
                      [Lexer]
                          │
                          ▼
                      [Parser]
                          │
                          ▼
                        [AST]
                          │
                          ▼
                [Semantic Analysis]
                          │
                          ▼
                     [Typed AST]
                          │
                          ▼
                         [IR]
                          │
                          ▼
                 [IR Optimization]
                         │
                         ▼
                    [Assembler]
                         │
                         ▼
                   [Object File]
                         │
                         ▼
                      [Linker]
                         │
                         ▼
                    [Executable]
                         │
                         ▼
                     [Run Later]


# ==========================================================
# CLANG / RUST STYLE ERROR HANDLING
# ==========================================================

## Mevcut Durum (v0.1)
- Parser'da `error(E001, line, col, msg)` fonksiyonu
- Çıktı formatı:
  ```
  error[E001]: expected number after '='
   --> test.gcsf:1:8
    |
  1 | int a = ;
    |        ^ expected number after '='
  ```
- Hata kodu: `E001` (tekil, genel)
- Kaynak satırı: caret ile işaretleniyor
- Lokasyon: dosya:satır:kolon doğru

---

### HEADERS

#include <script> or "script" --> .gcsf
#lib     <library> or "library" --> .gclib
#extern  <raylib.dll> or "raylib.dll"

# yorum 
#| yorumlar
devam 
ediyorlar
|#

// c yorumu

/* c
yorumu
bu
*/

#define MAX 100
#undef MAX

#ifdef DEBUG
    printf("Debug");
#endif

#ifndef DEBUG
    #define DEBUG
#endif

#if VERSION == 1
    #define TEXT "v1"
#elif VERSION == 2
    #define TEXT "v2"
#else
    #error VERSION tanımlı değil veya geçersiz.
#endif

#pragma message("Derleniyor...")

#line 200 "example.gcsf"

## MEMORY MANAGEMENT

-- 0: butun malloclar malloc-list icinde toplanir program kapanmadan once free olmadiysa free yapilir /double free check gerekiyor/


###### CHAR --> no \0 thi is real string but look like vanilla char --> STRING LENGHT

## STACK BASED CHAR
char A = 'a'; # "a" -> X
char game_0[64] = "HALF LIFE 3";
char game_1[] = "LEFT 4 DEAD 3";

## HEAP BASED CHAR STRING

char *game_2 = "Team Fortress 3";

if (game_2 == NULL) {
    return null_error_message;
}

# (char*) malloc(64);
char *game_3 = (char*) malloc(64 * sizeof(char));
strcpy(game_3, "serious sam first encounter"); # --> more safe extra boundry check, char size > max malloc size --> return warning!! line number --> extra alan ayir cokmeyi onlemek icin



# EGER PROGRAMIC FREE YAPMAYI UNUTUP KAPATIRSA KAPANMADAN ONCE SON OLARAK BUTUN MALLOKLAR KONTROL EDILIR DOUBLE FREE YASANMADAN FREE YAPILIR
free(game_3);

char GAMES[][] = {
    "HALF LIFE 3",
    "MAFIA 1/2",
    "Left 4 Dead 1",
    "NFS Underground 1/2"
};

char *GAMES_HEAP[] = {
    "Harry Potter 1/2/3 PC",
    "Warcraft 3",
    "GTA Vice City, San Andreas, 4"
};

char **games = (char*) malloc(4 * sizeof(char*));

if (games == NULL) {
    return null_error_message; # return line with null error message
}

games[0] = "remember me?";
games[1] = "no one lives forever";
games[2] = "advanture island";
games[3] = "metal slug";

# Runtime Error: Null Pointer Access at line 42
games[4] = "wet 2";








###### MAIN ENTRY --> sadece main script file icin gerekli sadece
int main(int argc, char *argv[]) {

    return 0;
}

int main(int argc, char **argv) {

    return 0;
}

int main() {

    return 0;
}

##### GELICEKDE SUANDA GEREK YOK
## networking -> built-in networking library
--- daha degil
## http request -> https client functionality
--- daha degil
## json support -> native json parsing/serialization
--- daha degil
## modern I/O async I/O operations
--- daha degil
## unicode support full unicode text handling
--- daha degil
## CLI arguments command line argument parsin
--- daha degil
