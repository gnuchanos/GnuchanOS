/*
 * gcl_stdio.c — GCL Stdio module (.dll/.so).
 *
 * simple_doc.md:
 *   Stdio.printf(); Stdio.scanf("%", ...);
 *   openFile/writeFile/readFile/closeFile/appendFile,
 *   fileExists/deleteFile/renameFile/fileSize/flushFile
 *   (camelCase and lowercase variants)
 *
 * DOSYA MODELI — iki bicim de desteklenir:
 *
 *   - TUTAMAC (handle): `int fd = Stdio.openFile(path);`
 *     `defer Stdio.closeFile(fd);`  (simple_doc.md'deki ornek)
 *     Dosya bir kez acilir; ardisik yazma/okuma ayni tutamac uzerinden gider.
 *
 *   - AD (path): `Stdio.writeFile(path, text)` gibi tek atimlik cagrilar
 *     dosyayi kendi acar, isini bitirir ve kapatir.
 *
 * Argumanin tutamac mi yol mu oldugu soyle ayirt edilir: arguman tamamen
 * rakamsa VE o numarada ACIK bir tutamac varsa tutamac sayilir; aksi halde
 * yol olarak kullanilir.
 *
 * METIN DONUSU: native modul ABI'si yalnizca double dondurur (gcl_module.h).
 *   Bu yuzden metin donduren uye (readFile) icerigi g_str tamponuna yazar ve
 *   OKUNAN BAYT SAYISINI dondurur; cagiran GCL kodu metni
 *   `LastStringLen()` + `LastStringByte(i)` ile okur — Raylib modulu ile
 *   ayni sozlesme (bkz. language/tests/raylib_file_callback.gcsf).
 */

#include "gcl_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* ------------------------------------------------------------------ */
/* Metin donus kanali (Raylib ile ayni sozlesme)                       */
/* ------------------------------------------------------------------ */

#define GCL_STDIO_STRCAP 65536
static char g_str[GCL_STDIO_STRCAP] = {0};

static void stdio_set_str(const char *s) {
    snprintf(g_str, sizeof(g_str), "%s", s ? s : "");
}

static void stdio_set_bytes(const char *b, size_t n) {
    if (!b || n == 0) { g_str[0] = '\0'; return; }
    if (n > sizeof(g_str) - 1) n = sizeof(g_str) - 1;
    memcpy(g_str, b, n);
    g_str[n] = '\0';
}

static double fn_last_string_len(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)strlen(g_str);
}

static double fn_last_string_byte(int argc, const char **argv) {
    int i = (argc > 0 && argv[0]) ? atoi(argv[0]) : 0;
    int n = (int)strlen(g_str);
    if (i < 0 || i >= n) return 0.0;
    return (double)(unsigned char)g_str[i];
}

/* ------------------------------------------------------------------ */
/* Acik dosya tutamaci tablosu                                         */
/* ------------------------------------------------------------------ */

#define GCL_STDIO_MAX_FILES 32
static FILE *g_files[GCL_STDIO_MAX_FILES] = {0};

/* Tutamac numarasi 1 tabanlidir; 0 ve negatifler "gecersiz" demektir. */
static int stdio_file_add(FILE *f) {
    for (int i = 0; i < GCL_STDIO_MAX_FILES; i++) {
        if (!g_files[i]) { g_files[i] = f; return i + 1; }
    }
    return -1;
}

static FILE *stdio_file_get(double handle) {
    int h = (int)handle;
    if (h < 1 || h > GCL_STDIO_MAX_FILES) return NULL;
    return g_files[h - 1];
}

static int stdio_file_close(double handle) {
    int h = (int)handle;
    if (h < 1 || h > GCL_STDIO_MAX_FILES) return 0;
    if (!g_files[h - 1]) return 0;
    fclose(g_files[h - 1]);
    g_files[h - 1] = NULL;
    return 1;
}

/* Arguman tutamac BICIMINDE mi (istege bagli isaret + yalnizca rakam)?
   Bicim kontrolu ile CANLILIK kontrolu AYRI tutulur: asagidaki
   `writeFile`/`appendFile`/`readFile`/`fileSize`/`flushFile` beslisi
   "tutamac YA DA yol" kabul eder ve eski kod tutamak bicimli ama OLU bir
   degeri YOL sanip fopen/stat'a veriyordu. Sonuc:
       int fd = Stdio.openFile(p, "w");
       Stdio.closeFile(fd);
       Stdio.writeFile(fd, "x");      // 1 donerdi ve "1" adli dosya OLUSURDU
   yani hem sahte basari hem de sessiz cop dosya. */
static int stdio_handle_shaped(const char *arg) {
    if (!arg || !*arg) return 0;
    const char *p = arg;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return 0;
        p++;
    }
    return 1;
}

/* Arguman CANLI bir tutamac mi? Oyleyse *out'a dosyayi yazar ve 1 doner. */
static int stdio_arg_handle(const char *arg, FILE **out) {
    if (!stdio_handle_shaped(arg)) return 0;
    FILE *f = stdio_file_get((double)atoi(arg));
    if (!f) return 0;
    if (out) *out = f;
    return 1;
}

/* Tutamak BICIMLI ama CANLI DEGIL mi? Boyle bir arguman YOL olarak da
   KULLANILMAZ (bkz. yukarisi): "1" gibi bir metin dosya adi degil, OLU bir
   tutamaktir; cagiran kendi hata degerini dondurur. */
static int stdio_dead_handle(const char *arg) {
    return stdio_handle_shaped(arg) && !stdio_file_get((double)atoi(arg));
}

/* ------------------------------------------------------------------ */
/* printf / scanf                                                      */
/* ------------------------------------------------------------------ */

/* `{.Nf}` SAYISAL bir yer tutucudur. Modul argumanlari METIN geldigi icin
   tek karakterlik, RAKAM OLMAYAN bir arguman sayisal baglamda KARAKTER KODU
   olarak yorumlanir — `int n = s[1];` ile ayni kural (char_rendering.gcsf:
   gcChar elemani sayisal baglamda 98'dir). Aksi halde `printf("{.2f}", c)`
   sessizce 0.00 basiyordu (`atof("A")` == 0), oysa ayni degisken `{}` ile 'A',
   `int m = c` ile 65 verir.
   RAKAM olan tek karakter (char '7') ayirt EDILEMEZ: "7" hem 7 sayisi hem '7'
   karakteri olabilir; sayi yorumu (7.00) secilir, cunku gercek bir sayisal
   argumani bozmak daha kotudur. Bu sinir ABI'nin metin tabanli olmasindan
   gelir ve testte acikca pinlenmistir. */
static double stdio_numeric_arg(const char *s) {
    if (!s || !*s) return 0.0;
    if (s[1] == '\0' && !isdigit((unsigned char)s[0]))
        return (double)(unsigned char)s[0];
    return atof(s);
}

static double fn_printf(int argc, const char **argv) {
    if (argc <= 0 || !argv[0]) return 0.0;
    const char *fmt = argv[0];
    int argi = 1;
    const char *p = fmt;
    while (*p) {
        if (*p == '{') {
            /* B19 — yer tutucu: `{}`, `{ }` (bosluklu) veya `{.Nf}`.
               ESKI HATA: `{` gorulunce `.`/rakam ayristirilip `}` kontrol
               ediliyor ama `}` TUKETILMIYORDU; aracil bosluk da yutuluyordu.
               `printf("[{ }]", 5)` bu yuzden `[5 }]` veriyordu (olmasi gereken
               `[5]`). Artik: bosluk atlanir, yer tutucu TAM olarak `}` dahil
               tuketilir; gecerli yer tutucu degilse `{` LITERAL basilir. */
            const char *q = p + 1;
            int prec = -1;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '.') {
                q++;
                int n = 0, digits = 0;
                while (isdigit((unsigned char)*q)) {
                    if (n < 100) n = n * 10 + (*q - '0');
                    q++;
                    digits++;
                }
                if (digits > 0) {
                    prec = n > 99 ? 99 : n;
                    if (*q == 'f' || *q == 'F') q++;
                }
            }
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '}') {
                q++;   /* kapanis parantezi TUKETILIR (eski hata buradaydi) */
                if (argi < argc && argv[argi]) {
                    if (prec >= 0) {
                        double d = stdio_numeric_arg(argv[argi]);
                        printf("%.*f", prec, d);
                    } else {
                        fputs(argv[argi], stdout);
                    }
                }
                argi++;
                p = q;
                continue;
            }
            /* Gecerli yer tutucu degil → '{' literal olarak basilir. */
            putchar('{');
            p++;
        } else if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                default: putchar(*p); break;
            }
            p++;
        } else {
            putchar(*p);
            p++;
        }
    }
    fflush(stdout);
    return 0.0;
}

/* scanf burada bilincli olarak bos: girdi okuma yorumlayicinin kendi
   `do_scanf` yolundan gecer (tampon sahipligi ve tur bazli temizlik orada
   yonetilir). Modul tarafi yalnizca API yuzeyi tutarliligi icin durur. */
static double fn_scanf(int argc, const char **argv) {
    (void)argc; (void)argv;
    return 0.0;
}

/* ------------------------------------------------------------------ */
/* Dosya islemleri                                                     */
/* ------------------------------------------------------------------ */

/* openFile(file[, mode]) -> tutamac (1..N), basarisizsa -1.
   Eski surum 1/0 donuyordu ve tutamac diye bir sey yoktu; simple_doc.md
   `fd = Stdio.openFile(path); defer Stdio.closeFile(fd);` dedigi icin
   gercek bir tutamac doner ve basarisizlik NEGATIF olur (ornek `fd < 0`
   ile kontrol ediyor). mode verilmezse "r". */
static double fn_open_file(int argc, const char **argv) {
    const char *file = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *mode = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : "r";
    if (!file[0]) return -1.0;
    FILE *f = fopen(file, mode);
    if (!f) return -1.0;
    int slot = stdio_file_add(f);
    if (slot < 0) { fclose(f); return -1.0; }
    return (double)slot;
}

/* closeFile(tutamac) -> 1 kapandi, 0 gecersiz/zaten kapali.
   Eski surum her zaman 0 donen bir no-op'ti (kaynak sizintisi). */
static double fn_close_file(int argc, const char **argv) {
    if (argc <= 0 || !argv[0]) return 0.0;
    if (!stdio_arg_handle(argv[0], NULL)) return 0.0;
    return (double)stdio_file_close((double)atoi(argv[0]));
}

/* writeFile(tutamac_veya_yol, text) -> 1/0.
   Tutamac verilirse dosya YENIDEN ACILMAZ, akisa yazilir (openFile mode="w"/"a"). */
static double fn_write_file(int argc, const char **argv) {
    const char *target = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *text = (argc > 1 && argv[1]) ? argv[1] : "";
    FILE *f = NULL;
    if (stdio_arg_handle(target, &f)) {
        if (fputs(text, f) == EOF) return 0.0;
        fflush(f);
        return 1.0;
    }
    if (stdio_dead_handle(target)) return 0.0;   /* OLU tutamak: yol DEGIL */
    if (!target[0]) return 0.0;
    f = fopen(target, "w");
    if (!f) return 0.0;
    fputs(text, f);
    fclose(f);
    return 1.0;
}

/* appendFile(tutamac_veya_yol, text) -> 1/0.
   ESKI HATA: writeFile ve flushFile "OLU tutamak yol DEGILDIR" kontrolunu
   yapiyordu, appendFile YAPMIYORDU. Sonuc: kapatilmis bir tutamacla
   `appendFile(fd, "ZZZ")` cagrisi `fopen("1", "a")` yolundan gecerdi —
   hem 1 donerdi hem de depo kokunde "1" adli cop bir dosya birakirdi.
   Ayni `stdio_dead_handle()` kapisi buraya da konur. */
static double fn_append_file(int argc, const char **argv) {
    const char *target = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *text = (argc > 1 && argv[1]) ? argv[1] : "";
    FILE *f = NULL;
    if (stdio_arg_handle(target, &f)) {
        if (fputs(text, f) == EOF) return 0.0;
        fflush(f);
        return 1.0;
    }
    if (stdio_dead_handle(target)) return 0.0;   /* OLU tutamak: yol DEGIL */
    if (!target[0]) return 0.0;
    f = fopen(target, "a");
    if (!f) return 0.0;
    fputs(text, f);
    fclose(f);
    return 1.0;
}

/* readFile(tutamac_veya_yol) -> okunan BAYT SAYISI; icerik g_str'de.
   Acilamazsa -1 (g_str temizlenir). Metni almak icin:
       int n = Stdio.readFile("a.txt");
       int i = 0;
       while (i < n) { ... Stdio.LastStringByte(i) ...; i = i + 1; }
   ESKI HATA: readFile yalnizca DOSYA BOYUTUNU donduruyordu ve icerigi
   hicbir yere yazmiyordu — yani `Stdio.readFile(p)` cagrisindan sonra
   metin ELDE EDILEMIYORDU (Raylib'e mecbur kaliniyordu). */
static double fn_read_file(int argc, const char **argv) {
    const char *target = (argc > 0 && argv[0]) ? argv[0] : "";
    if (!target[0]) { stdio_set_str(""); return -1.0; }

    FILE *f = NULL;
    int owns = 0;
    if (stdio_arg_handle(target, &f)) {
        owns = 0;                       /* cagiran kapatsin */
    } else {
        f = fopen(target, "rb");
        if (!f) { stdio_set_str(""); return -1.0; }
        owns = 1;
    }

    char *buf = (char *)malloc(GCL_STDIO_STRCAP);
    if (!buf) { if (owns) fclose(f); stdio_set_str(""); return -1.0; }

    size_t got = fread(buf, 1, GCL_STDIO_STRCAP - 1, f);
    buf[got] = '\0';
    stdio_set_bytes(buf, got);
    free(buf);

    if (owns) fclose(f);
    return (double)got;
}

static double fn_file_exists(int argc, const char **argv) {
    const char *file = (argc > 0 && argv[0]) ? argv[0] : "";
    struct stat st;
    if (!file[0]) return 0.0;
    return stat(file, &st) == 0 ? 1.0 : 0.0;
}

static double fn_delete_file(int argc, const char **argv) {
    const char *file = (argc > 0 && argv[0]) ? argv[0] : "";
    if (!file[0]) return 0.0;
    return remove(file) == 0 ? 1.0 : 0.0;
}

static double fn_rename_file(int argc, const char **argv) {
    const char *file = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *newName = (argc > 1 && argv[1]) ? argv[1] : "";
    if (!file[0] || !newName[0]) return 0.0;
    return rename(file, newName) == 0 ? 1.0 : 0.0;
}

/* fileSize(tutamac_veya_yol) -> bayt sayisi, hata -1. */
static double fn_file_size(int argc, const char **argv) {
    const char *target = (argc > 0 && argv[0]) ? argv[0] : "";
    FILE *f = NULL;
    if (stdio_arg_handle(target, &f)) {
        long pos = ftell(f);
        if (pos < 0) return -1.0;
        if (fseek(f, 0, SEEK_END) != 0) return -1.0;
        long end = ftell(f);
        fseek(f, pos, SEEK_SET);
        return end < 0 ? -1.0 : (double)end;
    }
    struct stat st;
    if (!target[0] || stat(target, &st) != 0) return -1.0;
    return (double)st.st_size;
}

/* flushFile(tutamac_veya_yol) -> 1/0. */
static double fn_flush_file(int argc, const char **argv) {
    const char *target = (argc > 0 && argv[0]) ? argv[0] : "";
    FILE *f = NULL;
    if (stdio_arg_handle(target, &f)) {
        return fflush(f) == 0 ? 1.0 : 0.0;
    }
    if (stdio_dead_handle(target)) return 0.0;   /* OLU tutamak: yol DEGIL */
    if (!target[0]) return 0.0;
    f = fopen(target, "r+");
    if (!f) return 0.0;
    fflush(f);
    fclose(f);
    return 1.0;
}

/* ------------------------------------------------------------------ */
/* Uye tablosu                                                         */
/* ------------------------------------------------------------------ */

static const GclNativeEntry g_entries[] = {
    {"printf", fn_printf},
    {"scanf", fn_scanf},

    {"LastStringLen", fn_last_string_len},
    {"lastStringLen", fn_last_string_len},
    {"laststringlen", fn_last_string_len},
    {"LastStringByte", fn_last_string_byte},
    {"lastStringByte", fn_last_string_byte},
    {"laststringbyte", fn_last_string_byte},

    {"openFile", fn_open_file},
    {"openfile", fn_open_file},
    {"writeFile", fn_write_file},
    {"writefile", fn_write_file},
    {"readFile", fn_read_file},
    {"readfile", fn_read_file},
    {"closeFile", fn_close_file},
    {"closefile", fn_close_file},
    {"appendFile", fn_append_file},
    {"appendfile", fn_append_file},
    {"fileExists", fn_file_exists},
    {"fileexists", fn_file_exists},
    {"deleteFile", fn_delete_file},
    {"deletefile", fn_delete_file},
    {"renameFile", fn_rename_file},
    {"renamefile", fn_rename_file},
    {"fileSize", fn_file_size},
    {"filesize", fn_file_size},
    {"flushFile", fn_flush_file},
    {"flushfile", fn_flush_file},
};

GCL_EXPORT const GclNativeEntry *gcl_stdio_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
