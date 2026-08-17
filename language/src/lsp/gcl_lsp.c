/*
 * gcl_lsp.c — GnuChan dil sunucusu (C, NDJSON over stdio)
 *
 * GCL EMBED KURALI ile calisir — Python/Lua'nin kendi sys.path/module
 * kurallari DEGIL, GCL kurali: "import ossuruk" yazildiysa ve
 * main.py + ossuruk.py yan yana (veya workspace'te) duruyorsa LSP bunu
 * gorur ve ossuruk.py icindeki HER sembolu otomatik tamamlamaya verir.
 *
 * BUZDOLABI MODELI (fridge): Python ve Lua, GCL'nin icinde hazir
 * yeteneklerdir. LSP, statik .py taramasi yetmediginde gercek Python'a
 * sorar (gcl -pyrun -resolve "<mod>|<prefix>"): python314.zip stdlib'i,
 * Lib/site-packages pip paketleri ve C-extension (.pyd) modullerinin
 * tamami "import dahil full sistem" taramasiyla gelir. "import numpy as np"
 * yazildiginda np. => numpy cozulur (alias destegi).
 *
 *   -> {"id":1,"method":"initialize","params":{"root":"D:\\proj"}}
 *   <- {"id":1,"result":{"ok":true,"files":42}}
 *
 *   -> {"id":2,"method":"textDocument/completion",
 *       "params":{"lang":"python","file":"D:\\proj\\src\\main.py",
 *                 "line":3,"col":9,"text":"import ossuruk\nossuruk."}}
 *   <- {"id":2,"result":[
 *        {"label":"zamber","kind":"const","detail":"zamber = 100"},
 *        {"label":"yarrak","kind":"fn","detail":"yarrak(eben, deden)"}, ... ]}
 *
 * Derleme:
 *   gcc -std=c11 -O2 gcl_lsp.c python_syntax.c -o gcl-lsp.exe
 */

#define _CRT_SECURE_NO_WARNINGS
/* Linux/glibc: strict -std=c11 ile DT_DIR (dirent.h), readlink, popen,
 * strtok_r gibi POSIX sembolleri gizli kalir — derleme hatalari bunlardi.
 * _GNU_SOURCE (veya _DEFAULT_SOURCE) hepsini acar. Windows'ta gerekmez. */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Python dilinin kendi kelimeleri (keywords + builtins):
 * "impo" -> import, "pri" -> print gibi tamamlama bu tablodan gelir. */
#include "python_syntax.h"

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define GCL_PATH_MAX 1024
#define GCL_NAME_MAX 256
#define GCL_PARAM_MAX 512
#define GCL_FILES_MAX 2048
#define GCL_SYMS_MAX 65536
#define GCL_IMPORTS_MAX 128
#define GCL_LINE_MAX (1 << 20)
/* Dosya indexleme icin satir buffer'i: tek satir 64KB'yi asmaz; main'deki
 * 1MB static buffer RPC girdisi icindir (uzun text uyeleri). Stack'i
 * sismamak adina indexleme buffer'lari kucuktur. */
#define GCL_INDEX_LINE (1 << 16)
/* BUZDOLABI: gcl -pyrun -resolve cache (modul basina bir kez) */
#define GCL_FRIDGE_CACHE 64
#define GCL_FRIDGE_BUF (1 << 18)

/* ------------------------------------------------------------------ */
/* Veri modelleri                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
  SYM_FN = 0,
  SYM_CLASS,
  SYM_CONST,
  SYM_MODULE
} SymKind;

typedef struct {
  char name[GCL_NAME_MAX];
  SymKind kind;
  char params[GCL_PARAM_MAX]; /* imza veya "class" veya "= ..." */
  char mod[GCL_NAME_MAX];     /* ait oldugu modul (dosya adi, uzantisiz) */
  char file[GCL_PATH_MAX];
} Symbol;

typedef struct {
  char path[GCL_PATH_MAX];
  char name[GCL_NAME_MAX];     /* basename uzantisiz */
  char imports[GCL_IMPORTS_MAX][GCL_NAME_MAX];
  char import_aliases[GCL_IMPORTS_MAX][GCL_NAME_MAX]; /* "import x as y" -> y */
  char wildcard[GCL_NAME_MAX]; /* "from X import *" -> X; bos = yok */
  int import_count;
  int sym_start;               /* Workspace.syms icindeki baslangic indisi */
  int sym_count;
} FileIndex;

typedef struct {
  char root[GCL_PATH_MAX];
  FileIndex files[GCL_FILES_MAX];
  int file_count;
  Symbol syms[GCL_SYMS_MAX];
  int sym_count;
} Workspace;

/* BUZDOLABI cache — workspace YENIDEN INDEXLENSE DE (didChange: dosya
 * eklendi/silindi) KORUNUR. Workspace struct'indan ayri tutulur cunku
 * index_workspace memset ile ws'i sifirlar; cache sifirlanirsa her
 * completion'da gcl -pyrun -resolve yeniden spawn olur (saniyeler surer).
 * Ayri global yapi: "import os" -> os. tamamlamalari her didChange'te
 * surekli tekrar calismaz, cache kalici kalir. */
typedef struct {
  char mods[GCL_FRIDGE_CACHE][GCL_NAME_MAX];
  char outs[GCL_FRIDGE_CACHE][GCL_FRIDGE_BUF];
  int count;
} FridgeCache;
static FridgeCache g_fridge;

/* ------------------------------------------------------------------ */
/* Dosya / yol yardimcilari                                            */
/* ------------------------------------------------------------------ */

static void path_join(char *out, size_t cap, const char *a, const char *b) {
  snprintf(out, cap, "%s/%s", a, b);
  for (char *p = out; *p; p++) {
    if (*p == '\\') *p = '/';
  }
}

static const char *path_base(const char *p) {
  const char *s = strrchr(p, '/');
  return s ? s + 1 : p;
}

/* Windows'ta yollar case-insensitive'dir ("D:/x" == "d:/x") ve \ ile /
 * denktir. Index yollari her zaman / ile normalize edilir (path_join); ancak
 * Electron dosya yolunu Windows'ta \ ile gonderir. path_eq ikisini de denk
 * sayar — find_file, resolve_module ve paket karsilastirmalari ayrac/case
 * farkindan etkilenmez. */
static int path_eq(const char *a, const char *b) {
  if (a == NULL || b == NULL) return a == b;
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a;
    unsigned char cb = (unsigned char)*b;
    if (ca == '\\') ca = '/';
    if (cb == '\\') cb = '/';
#ifdef _WIN32
    if (ca >= 'A' && ca <= 'Z') ca += (unsigned char)32;
    if (cb >= 'A' && cb <= 'Z') cb += (unsigned char)32;
#endif
    if (ca != cb) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

/* path_starts_with: `a`, `b` ile mi basliyor? (ayrac + case normalize).
 * file_dir_base'de gelen dosya yolu ile workspace root'u karsilastirmak icin:
 * root "c:/Users/x", gelen yol "c:\\Users\\x\\src\\main.py" olabilir. */
static int path_starts_with(const char *a, const char *b) {
  if (a == NULL || b == NULL) return 0;
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a;
    unsigned char cb = (unsigned char)*b;
    if (ca == '\\') ca = '/';
    if (cb == '\\') cb = '/';
#ifdef _WIN32
    if (ca >= 'A' && ca <= 'Z') ca += (unsigned char)32;
    if (cb >= 'A' && cb <= 'Z') cb += (unsigned char)32;
#endif
    if (ca != cb) return 0;
    a++;
    b++;
  }
  return *b == 0;
}

static void strip_ext(char *out, size_t cap, const char *name) {
  const char *base = path_base(name);
  const char *dot = strrchr(base, '.');
  size_t n = dot ? (size_t)(dot - base) : strlen(base);
  if (n >= cap) n = cap - 1;
  memcpy(out, base, n);
  out[n] = '\0';
}

/* Dosyanin workspace root'a goreli dizininin SON SEGMENTI:
 *   root/src/pyFiles/test.py -> "pyFiles"
 *   root/src/main.py         -> "src"   (import kokudur, onerilmez)
 *   root/test.py             -> ""      (kok dosya: dizin yok)
 * "from pyFiles import test" cozumlemesi + klasor adi tamamlamasi icin.
 * Yollar hep '/' ayracildir (path_join normalleştirir). */
static void file_dir_base(Workspace *ws, const char *path, char *out, size_t cap) {
  out[0] = 0;
  if (!ws || !path || !ws->root[0]) return;
  /* Windows'ta gelen yol \ ayracli olabilir (Electron path.join): root ile
   * ayrac + case normalize ederek karsilastir, aksi halde paket klasoru
   * hic bulunamaz ("from testFolder import helloworld" cozulemez). */
  if (!path_starts_with(path, ws->root)) return;
  size_t rlen = strlen(ws->root);
  const char *rel = path + rlen;
  while (*rel == '/' || *rel == '\\') rel++;
  /* goreli yoldaki son ayraci bul (hem / hem \ kabul et) */
  const char *sl = rel;
  {
    const char *p = rel;
    for (; *p; p++)
      if (*p == '/' || *p == '\\') sl = p;
  }
  if (*sl == '\0') return; /* kok dosya: dizin yok */
  if (sl == rel) return;   /* kokteki dosya */
  const char *seg = rel;
  for (const char *q = rel; q < sl; q++)
    if (*q == '/' || *q == '\\') seg = q + 1;
  size_t n = (size_t)(sl - seg);
  if (n >= cap) n = cap - 1;
  memcpy(out, seg, n);
  out[n] = 0;
}

/* isimlendirilebilir mi: harf/_ ile baslar, sonra harf/rakam/_ */
static int is_ident(const char *s) {
  if (!s || !(*s == '_' || isalpha((unsigned char)*s))) return 0;
  for (s++; *s; s++) {
    if (!(*s == '_' || isalnum((unsigned char)*s))) return 0;
  }
  return 1;
}

/* dizin "govde" mi — BUZDOLABI: pyLibrary/Lib/site-packages artik "govde"
 * degil; statik tarama onlari gormez (stdlib zip'te, C-ext .pyd), full
 * erisim gcl -pyrun -resolve ile saglanir. Workspace govde sayilanlar
 * sadece gercek coplerdir. */
static int is_junk_dir(const char *name) {
  static const char *junk[] = {
    "node_modules", ".git", "__pycache__", "build", "dist", "release",
    "_temp", "python_win", "python_linux", "runtime",
    "GnuChanIDE_JUNKS", NULL,
  };
  for (int i = 0; junk[i]; i++)
    if (strcmp(name, junk[i]) == 0) return 1;
  return 0;
}

/* ------------------------------------------------------------------ */
/* workspace taramasi                                                  */
/* ------------------------------------------------------------------ */

/* Bir klasordeki .py dosyalarini (derinlik sinirli) toplar */
static void collect_python(Workspace *ws, const char *dir, int depth) {
  if (depth > 6) return;

#ifdef _WIN32
  char pat[GCL_PATH_MAX];
  struct _finddata_t fd;
  snprintf(pat, sizeof pat, "%s/*", dir);
  intptr_t h = _findfirst(pat, &fd);
  if (h == -1) return;
  do {
    if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
    char full[GCL_PATH_MAX];
    path_join(full, sizeof full, dir, fd.name);
    if (fd.attrib & _A_SUBDIR) {
      if (!is_junk_dir(fd.name)) collect_python(ws, full, depth + 1);
    } else {
      size_t n = strlen(fd.name);
      if (n > 3 && strcmp(fd.name + n - 3, ".py") == 0) {
        if (ws->file_count < GCL_FILES_MAX) {
          FileIndex *f = &ws->files[ws->file_count++];
          snprintf(f->path, sizeof f->path, "%s", full);
          strip_ext(f->name, sizeof f->name, fd.name);
          f->import_count = 0;
          f->sym_start = ws->sym_count;
          f->sym_count = 0;
        }
      }
    }
  } while (_findnext(h, &fd) == 0);
  _findclose(h);
#else
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
    char full[GCL_PATH_MAX];
    path_join(full, sizeof full, dir, e->d_name);
    if (e->d_type == DT_DIR) {
      if (!is_junk_dir(e->d_name)) collect_python(ws, full, depth + 1);
    } else {
      size_t n = strlen(e->d_name);
      if (n > 3 && strcmp(e->d_name + n - 3, ".py") == 0) {
        if (ws->file_count < GCL_FILES_MAX) {
          FileIndex *f = &ws->files[ws->file_count++];
          snprintf(f->path, sizeof f->path, "%s", full);
          strip_ext(f->name, sizeof f->name, e->d_name);
          f->import_count = 0;
          f->sym_start = ws->sym_count;
          f->sym_count = 0;
        }
      }
    }
  }
  closedir(d);
#endif
}

/* import listesine ekle (tekil); alias varsa import_aliases'e yazilir */
static void add_import_ex(FileIndex *f, const char *mod, const char *alias) {
  for (int i = 0; i < f->import_count; i++)
    if (strcmp(f->imports[i], mod) == 0 &&
        strcmp(f->import_aliases[i], alias ? alias : "") == 0)
      return;
  if (f->import_count < GCL_IMPORTS_MAX) {
    snprintf(f->imports[f->import_count], GCL_NAME_MAX, "%s", mod);
    snprintf(f->import_aliases[f->import_count], GCL_NAME_MAX, "%s",
             alias ? alias : "");
    f->import_count++;
  }
}

/* "width: int, height: int = 5" -> "width, height=5" */
static void clean_params(const char *raw, char *out, size_t cap) {
  const char *s = raw;
  size_t o = 0;
  while (*s && o + 1 < cap) {
    if (*s == ':') {
      /* ":" + sonraki "type" kismini at; "=" varsa degeri tut */
      const char *eq = strchr(s, '=');
      const char *comma = strchr(s, ',');
      if (eq && (!comma || eq < comma)) {
        while (*s && *s != '=') s++;
      } else {
        while (*s && *s != ',') s++;
        continue;
      }
    }
    out[o++] = *s++;
  }
  out[o] = '\0';
  /* bosluklari tekille */
  char tmp[GCL_PARAM_MAX];
  snprintf(tmp, sizeof tmp, "%s", out);
  s = tmp;
  o = 0;
  int space = 0;
  while (*s && o + 1 < cap) {
    if (*s == ' ' || *s == '\t') {
      if (!space && o > 0) out[o++] = ' ';
      space = 1;
    } else {
      out[o++] = *s;
      space = 0;
    }
    s++;
  }
  out[o] = '\0';
}

static void trim(char *s) {
  char *start = s;
  while (*start == ' ' || *start == '\t') start++;
  size_t n = strlen(start);
  while (n > 0 && (start[n - 1] == ' ' || start[n - 1] == '\t' ||
                   start[n - 1] == '\r' || start[n - 1] == '\n'))
    start[--n] = '\0';
  if (start != s) memmove(s, start, n + 1);
}

/* Python satiri isle: syms + imports guncellenir. */
static void index_python_line(Workspace *ws, FileIndex *f, const char *raw) {
  char line[GCL_INDEX_LINE];
  snprintf(line, sizeof line, "%s", raw);
  trim(line);
  if (!*line) return;

  int indent = 0;
  {
    const char *p = raw;
    while (*p == ' ' || *p == '\t') {
      indent++;
      p++;
    }
  }

  const char *s = line;

  /* import X [as Y] */
  if (strncmp(s, "import ", 7) == 0) {
    s += 7;
    char mod[GCL_NAME_MAX] = {0};
    int k = 0;
    while (*s && !isspace((unsigned char)*s) && *s != ',' && k < GCL_NAME_MAX - 1)
      mod[k++] = *s++;
    mod[k] = 0;
    /* alias: import X as Y -> alias da tutulur.
     * Boylece "import numpy as np" yazildiginda np. => numpy cozulur. */
    char alias[GCL_NAME_MAX] = {0};
    char *as = strstr(s, "as");
    if (as) {
      const char *a = as + 2;
      while (*a == ' ') a++;
      if (is_ident(a)) {
        int k2 = 0;
        while (isalnum((unsigned char)*a) || *a == '_') alias[k2++] = *a++;
        alias[k2] = 0;
      }
    }
    if (is_ident(mod)) {
      /* import a.b: alias VARSA SON segment (dosya modulu) alinir:
       *   import testFolder.helloworld as hw  ->  hw = helloworld dosya modulu.
       *   Boylece "hw." => helloworld.py uyeleri cozulur (onceki kod ilk
       *   segmente kripiyor, alias hicbir seye baglanmiyordu).
       * Alias YOKSA ILK segment kalir (paket koku):
       *   import os.path -> os. => os cozulur, path os uyesidir. */
      if (alias[0]) {
        char *dot = strrchr(mod, '.');
        if (dot) memmove(mod, dot + 1, strlen(dot + 1) + 1);
      } else {
        char *dot = strchr(mod, '.');
        if (dot) *dot = 0;
      }
      add_import_ex(f, mod, alias);
    }
    return;
  }

  /* from X import Y [as Z] */
  if (strncmp(s, "from ", 5) == 0) {
    s += 5;
    char mod[GCL_NAME_MAX] = {0};
    int k = 0;
    while (*s && !isspace((unsigned char)*s) && k < GCL_NAME_MAX - 1)
      mod[k++] = *s++;
    mod[k] = 0;
    if (is_ident(mod)) add_import_ex(f, mod, "");
    /* from x import Y -> Y de global isimdir */
    const char *imp = strstr(s, "import");
    if (imp) {
      const char *n = imp + 6;
      while (*n == ' ') n++;
      /* from X import * (wildcard): modul uyeleri dosyada modul adi
       * yazilmadan kullanilabilir -> FileIndex.wildcard'a kaydet. */
      if (*n == '*') {
        snprintf(f->wildcard, sizeof f->wildcard, "%s", mod);
        return;
      }
      while (*n && (isalnum((unsigned char)*n) || *n == '_') &&
             ws->sym_count < GCL_SYMS_MAX && f->sym_count < GCL_SYMS_MAX) {
        char nm[GCL_NAME_MAX] = {0};
        int k2 = 0;
        while (isalnum((unsigned char)*n) || *n == '_') nm[k2++] = *n++;
        nm[k2] = 0;
        Symbol *sym = &ws->syms[ws->sym_count];
        snprintf(sym->name, sizeof sym->name, "%s", nm);
        sym->kind = SYM_FN;
        snprintf(sym->params, sizeof sym->params, "(%s import)", mod);
        snprintf(sym->mod, sizeof sym->mod, "%s", f->name);
        snprintf(sym->file, sizeof sym->file, "%s", f->path);
        ws->sym_count++;
        f->sym_count++;
        while (*n == ',' || *n == ' ') n++;
      }
    }
    return;
  }

  /* yalnizca ust seviye def/class/const (indent == 0) */
  if (indent != 0) return;

  /* async def | def name(params): */
  const char *defp = strstr(s, "def ");
  if (defp && defp == s + (s[0] == 'a' && s[1] == 's' && s[2] == 'y' &&
                           s[3] == 'n' && s[4] == 'c' && s[5] == ' ' ? 6 : 0)) {
    const char *n = defp + 4;
    while (*n == ' ') n++;
    char name[GCL_NAME_MAX] = {0};
    int k = 0;
    while (isalnum((unsigned char)*n) || *n == '_') name[k++] = *n++;
    if (!*name) return;
    /* params: ( ile ) arasi */
    char rawp[GCL_PARAM_MAX] = {0};
    const char *op = strchr(n, '(');
    if (op) {
      const char *cl = strchr(op + 1, ')');
      size_t plen = cl ? (size_t)(cl - op - 1) : 0;
      if (plen >= sizeof rawp) plen = sizeof rawp - 1;
      memcpy(rawp, op + 1, plen);
      rawp[plen] = 0;
    }
    char params[GCL_PARAM_MAX] = {0};
    clean_params(rawp, params, sizeof params);
    if (ws->sym_count >= GCL_SYMS_MAX || f->sym_count >= GCL_SYMS_MAX) return;
    Symbol *sym = &ws->syms[ws->sym_count];
    snprintf(sym->name, sizeof sym->name, "%s", name);
    sym->kind = SYM_FN;
    snprintf(sym->params, sizeof sym->params, "%s", params);
    snprintf(sym->mod, sizeof sym->mod, "%s", f->name);
    snprintf(sym->file, sizeof sym->file, "%s", f->path);
    ws->sym_count++;
    f->sym_count++;
    return;
  }

  /* class Name: */
  if (strncmp(s, "class ", 6) == 0) {
    const char *n = s + 6;
    while (*n == ' ') n++;
    char name[GCL_NAME_MAX] = {0};
    int k = 0;
    while (isalnum((unsigned char)*n) || *n == '_') name[k++] = *n++;
    if (!*name) return;
    if (ws->sym_count >= GCL_SYMS_MAX || f->sym_count >= GCL_SYMS_MAX) return;
    Symbol *sym = &ws->syms[ws->sym_count];
    snprintf(sym->name, sizeof sym->name, "%s", name);
    sym->kind = SYM_CLASS;
    snprintf(sym->params, sizeof sym->params, "class");
    snprintf(sym->mod, sizeof sym->mod, "%s", f->name);
    snprintf(sym->file, sizeof sym->file, "%s", f->path);
    ws->sym_count++;
    f->sym_count++;
    return;
  }

  /* NAME = value  (ust seviye sabit) */
  {
    const char *eq = strchr(s, '=');
    const char *ne = strstr(s, "!=");
    const char *cel = strstr(s, "<=");
    const char *gel = strstr(s, ">=");
    if (eq && ne != eq && cel != eq && gel != eq) {
      char name[GCL_NAME_MAX] = {0};
      size_t ln = (size_t)(eq - s);
      while (ln > 0 && (s[ln - 1] == ' ' || s[ln - 1] == '\t')) ln--;
      if (ln > 0 && ln < sizeof name) {
        memcpy(name, s, ln);
        name[ln] = 0;
        if (is_ident(name) && strcmp(name, "self") != 0 &&
            strcmp(name, "cls") != 0) {
          if (ws->sym_count >= GCL_SYMS_MAX || f->sym_count >= GCL_SYMS_MAX)
            return;
          Symbol *sym = &ws->syms[ws->sym_count];
          snprintf(sym->name, sizeof sym->name, "%s", name);
          sym->kind = SYM_CONST;
          char val[GCL_NAME_MAX] = {0};
          const char *v = eq + 1;
          while (*v == ' ') v++;
          size_t vlen = strlen(v);
          if (vlen >= sizeof val) vlen = sizeof val - 1;
          memcpy(val, v, vlen);
          val[vlen] = 0;
          trim(val);
          snprintf(sym->params, sizeof sym->params, "%s = %s", name, val);
          snprintf(sym->mod, sizeof sym->mod, "%s", f->name);
          snprintf(sym->file, sizeof sym->file, "%s", f->path);
          ws->sym_count++;
          f->sym_count++;
        }
      }
    }
  }
}

/* Bir dosyayi indexle (dosya zaten Workspace.files'da kayitli) */
static void index_file(Workspace *ws, FileIndex *f) {
  f->import_count = 0;
  f->sym_count = 0;
  f->sym_start = ws->sym_count;
  f->wildcard[0] = 0;

  FILE *fp = fopen(f->path, "r");
  if (!fp) return;
  static char line[GCL_INDEX_LINE];
  while (fgets(line, sizeof line, fp)) {
    index_python_line(ws, f, line);
  }
  fclose(fp);
}

/* Workspace'i (root'un tamami) indexle.
 * Dikkat: root, &ws->root ile ayni bellek olabilir (didChange gibi);
 * memset'ten ONCE yerel buffer'a kopyalanir, aksi halde sifirlanmis root
 * ile tum disk taranir (files:2048 aniomalisi). */
static int index_workspace(Workspace *ws, const char *root) {
  char root_copy[GCL_PATH_MAX];
  if (root != NULL) {
    snprintf(root_copy, sizeof root_copy, "%s", root);
  } else {
    root_copy[0] = '\0';
  }
  memset(ws, 0, sizeof *ws);
  snprintf(ws->root, sizeof ws->root, "%s", root_copy);
  for (char *p = ws->root; *p; p++)
    if (*p == '\\') *p = '/';

  /* .py dosyalarini topla (src/ + root + Library/Python) */
  char src_dir[GCL_PATH_MAX];
  path_join(src_dir, sizeof src_dir, ws->root, "src");
  collect_python(ws, src_dir, 0);
  collect_python(ws, ws->root, 0);
  char lib_dir[GCL_PATH_MAX];
  path_join(lib_dir, sizeof lib_dir, ws->root, "Library/Python");
  collect_python(ws, lib_dir, 0);

  /* hepsini indexle */
  for (int i = 0; i < ws->file_count; i++)
    index_file(ws, &ws->files[i]);

  return ws->file_count;
}

/* ------------------------------------------------------------------ */
/* import -> dosya cozumleme (GCL kurali: kardes dosya)                */
/* ------------------------------------------------------------------ */

/* Alt dizin karsilastirmasi: ayrac ve (Windows'ta) case normalize ederek
 * iki yolun AYNI dizin oldugunu denetler. `cand` index yolu (normalize), 
 * `dir` Electron'dan gelen \ ayracli yol olabilir. */
static int same_dir(const char *cand, const char *dir) {
  if (cand == NULL || dir == NULL) return 0;
  return path_eq(cand, dir);
}

/* Modul adi ver; once acik dosyanin dizininde, sonra workspace'te ara. */
static FileIndex *resolve_module(Workspace *ws, const char *file, const char *mod) {
  char dir[GCL_PATH_MAX];
  snprintf(dir, sizeof dir, "%s", file ? file : ws->root);
  /* acik dosyanin dizini: SON ayraci bul — Windows'ta \ ile / ayni anlamda. */
  {
    char *last = NULL;
    for (char *p = dir; *p; p++) {
      if (*p == '/' || *p == '\\') last = p;
    }
    if (last != NULL) *last = 0;
    else {
      /* ayrac yoksa root'u dene */
      snprintf(dir, sizeof dir, "%s", ws->root);
    }
  }

  /* 1) kardes dosya: acik dosyanin yanindaki mod.py */
  for (int i = 0; i < ws->file_count; i++) {
    if (strcmp(ws->files[i].name, mod) == 0) {
      char cand[GCL_PATH_MAX];
      snprintf(cand, sizeof cand, "%s", ws->files[i].path);
      char *sl = strrchr(cand, '/');
      if (sl) *sl = 0;
      if (path_eq(cand, dir)) return &ws->files[i];
    }
  }

  /* 2) herhangi bir konumdaki mod.py (basename eslesmesi) */
  for (int i = 0; i < ws->file_count; i++) {
    if (strcmp(ws->files[i].name, mod) == 0) return &ws->files[i];
  }
  return NULL;
}

/* out_item, emit_package_members'tan SONRA tanimli (JSON yazma blogu);
 * ileri bildirim gerekli. */
static void out_item(const char *label, const char *kind, const char *detail);

/* Ayni ada sahip dosyalar farkli klasorlerdeyse karismasin: acik dosyanin
 * "from X import ..." yaptigi X paket dizinindeki eslesen modul ONCELIKLI.
 *   from beta import util ; util.  ->  beta/util.py (alpha'da da util varsa
 *   bile yanlis dosya secilmez). cur == NULL ise ilk eslesen fallback. */
static FileIndex *resolve_module_pkg(Workspace *ws, const char *file,
                                     const char *mod, FileIndex *cur) {
  FileIndex *first = NULL;
  for (int i = 0; i < ws->file_count; i++) {
    if (strcmp(ws->files[i].name, mod) != 0) continue;
    if (!first) first = &ws->files[i];
    if (cur && cur->import_count > 0) {
      char dbase[GCL_NAME_MAX];
      file_dir_base(ws, ws->files[i].path, dbase, sizeof dbase);
      for (int k = 0; k < cur->import_count; k++) {
        if (dbase[0] && strcmp(dbase, cur->imports[k]) == 0)
          return &ws->files[i];
      }
    }
  }
  return first;
}

/* member_mod bir KLASOR (paket) adiysa icindeki .py dosyalarini onerir.
 *   alpha.  ->  alpha/ icindeki util modulu. Bu, paket member tamamlamayi
 *   (dokuman 2-4) ve "import alpha." ara segment onerilerini (2-3) birlikte
 *   cozer. Bulunduysa 1, yoksa 0 doner. */
static int emit_package_members(Workspace *ws, const char *pkg,
                                const char *prefix) {
  int got = 0;
  for (int i = 0; i < ws->file_count; i++) {
    char dbase[GCL_NAME_MAX];
    file_dir_base(ws, ws->files[i].path, dbase, sizeof dbase);
    if (!dbase[0] || strcmp(dbase, pkg) != 0) continue;
    if (strcmp(ws->files[i].name, pkg) == 0) continue; /* paketle ayni adli dosya */
    if (prefix[0] && strncmp(ws->files[i].name, prefix, strlen(prefix)) != 0)
      continue;
    char detail[GCL_PATH_MAX + 64];
    snprintf(detail, sizeof detail, "module %s (pkg %s)", ws->files[i].name, pkg);
    out_item(ws->files[i].name, "module", detail);
    got = 1;
  }
  return got;
}

/* ------------------------------------------------------------------ */
/* BUZDOLABI: gcl -pyrun -resolve (gercek Python sorgusu)              */
/* ------------------------------------------------------------------ */

/* exe'nin yanindaki gcl.exe'yi bulup "gcl -pyrun -resolve <mod>|<prefix>"
 * calistirir; stdout'taki NDJSON item satirlarini out'a kopyalar.
 * Baska cikti (gcl banner vs.) kopyalanmaz; yalnizca {id,kind,label,...}
 * haricindeki satirlar elenir. */
static int fridge_query(Workspace *ws, const char *mod, const char *prefix,
                        char *out, size_t out_cap) {
  char gcl_path[GCL_PATH_MAX];
  char cmd[GCL_PATH_MAX + GCL_NAME_MAX * 2 + 64];
  char dump[GCL_FRIDGE_BUF];
  FILE *p = NULL;
  size_t used = 0;

  if (out == NULL || out_cap == 0) return 1;
  out[0] = 0;

  /* gcl-lsp'nin kendi yolu -> yanindaki gcl/gcl.exe.
   * `exe` HER IKI platformda da tanimli olmali — onceden yalnizca _WIN32
   * dalinda tanimliydi, Linux CI hatasi "exe undeclared" buydu. Onceki
   * tasarim Linux'ta yolu IKI kez boluyordu (once gcl_path, sonra exe
   * icin tekrar) ve gcl bir dizin yukarida araniyordu — BUZDOLABI
   * Linux'ta susuyordu. Simdi tam yol exe'ye alinir ve tek dirname ile
   * dizin bolunur. */
  {
    char exe[GCL_PATH_MAX];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof exe);
    if (n == 0 || n >= (DWORD)sizeof exe) return 1;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0 || n >= (ssize_t)sizeof exe) return 1;
    exe[n] = 0;
#endif
    char *sl = strrchr(exe, '/');
    if (!sl) sl = strrchr(exe, '\\');
    if (sl) *sl = 0;
    snprintf(gcl_path, sizeof gcl_path, "%s/%s", exe,
#ifdef _WIN32
             "gcl.exe"
#else
             "gcl"
#endif
    );
  }

  snprintf(cmd, sizeof cmd, "\"%s\" -pyrun -resolve \"%s|%s\"",
           gcl_path, mod, prefix);

#ifdef _WIN32
  {
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE r = NULL, w = NULL;

    ZeroMemory(&sa, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&r, &w, &sa, 0)) return 1;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    si.hStdOutput = w;
    si.hStdError = w;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof pi);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
      CloseHandle(r);
      CloseHandle(w);
      return 1;
    }
    /* Ebeveynde yazma ucu kapatilir; yalnizca cocuk yazar. */
    CloseHandle(w);

    /* DEADLOCK ONLEME: pipe'i beklemeden ONCE bosalt. GCL -pyrun -resolve
     * "os|" gibi buyuk modullerde yuzlerce NDJSON satiri basar; pipe tamponu
     * (4KB) dolarsa cocuk bloklanir. Ebeveyn once okuyup bosaltir, sonra
     * prosesin cikisini bekler. (Kaynak: std::io::pipes drain-before-wait) */
    used = 0;
    for (;;) {
      DWORD got = 0;
      if (!ReadFile(r, dump + used, (DWORD)(sizeof dump - used - 1), &got, NULL) ||
          got == 0)
        break;
      if (used < sizeof dump - 1) used += (size_t)got;
    }
    dump[used] = 0;
    CloseHandle(r);

    /* Watchdog: koklu takilma ihtimaline karsi 10s siniri. */
    if (WaitForSingleObject(pi.hProcess, 10000) == WAIT_TIMEOUT) {
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 1000);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
#else
  {
    p = popen(cmd, "r");
    if (!p) return 1;
    used = 0;
    while (used + 1 < sizeof dump && fgets(dump + used, (int)(sizeof dump - used), p))
      used += strlen(dump + used);
    dump[used] = 0;
    pclose(p);
  }
#endif

  /* NDJSON item satirlarini kopyala: {"label":... satirlari */
  {
    char *save = NULL;
    char *tok = strtok_r(dump, "\n", &save);
    size_t o = 0;
    while (tok && o + 1 < out_cap) {
      char *t = tok;
      while (*t == ' ' || *t == '\r') t++;
      if (strncmp(t, "{\"label\":", 9) == 0 ||
          strncmp(t, "{\"label\" :", 10) == 0) {
        size_t n = strlen(t);
        if (o + n + 2 >= out_cap) n = out_cap - o - 2;
        memcpy(out + o, t, n);
        o += n;
        out[o++] = '\n';
      }
      tok = strtok_r(NULL, "\n", &save);
    }
    out[o] = 0;
  }
  return out[0] ? 0 : 1;
}

/* fridge cache'e eris: modul icin onceden toplanmis NDJSON satirlari.
 * Workspace'in disindaki global g_fridge'de tutulur — didChange yeniden
 * indexleme yaptiginda cache korunur ve gcl -pyrun -resolve her seferinde
 * yeniden spawn olmaz. */
static const char *fridge_get(Workspace *ws, const char *mod) {
  for (int i = 0; i < g_fridge.count; i++)
    if (strcmp(g_fridge.mods[i], mod) == 0)
      return g_fridge.outs[i];
  if (g_fridge.count >= GCL_FRIDGE_CACHE) return NULL;
  snprintf(g_fridge.mods[g_fridge.count], GCL_NAME_MAX, "%s", mod);
  if (fridge_query(ws, mod, "", g_fridge.outs[g_fridge.count],
                   GCL_FRIDGE_BUF) != 0) {
    g_fridge.outs[g_fridge.count][0] = 0;
  }
  {
    const char *r = g_fridge.outs[g_fridge.count];
    g_fridge.count++;
    return r;
  }
}

/* ------------------------------------------------------------------ */
/* JSON yazma                                                          */
/* ------------------------------------------------------------------ */

static void json_escape(char *out, size_t cap, const char *in) {
  size_t o = 0;
  for (const char *p = in; *p && o + 6 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"') {
      out[o++] = '\\';
      out[o++] = '"';
    } else if (c == '\\') {
      out[o++] = '\\';
      out[o++] = '\\';
    } else if (c == '\n') {
      out[o++] = '\\';
      out[o++] = 'n';
    } else if (c == '\r') {
      out[o++] = '\\';
      out[o++] = 'r';
    } else if (c == '\t') {
      out[o++] = '\\';
      out[o++] = 't';
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = 0;
}

static const char *kind_str(SymKind k) {
  switch (k) {
    case SYM_FN: return "fn";
    case SYM_CLASS: return "class";
    case SYM_CONST: return "const";
    default: return "module";
  }
}

/* ------------------------------------------------------------------ */
/* JSON okuma (basit: "key":"value" ve "key":sayi)                     */
/* ------------------------------------------------------------------ */

/* verilen json icinde "key":"..." degerini cozer; yoksa NULL.
 * "key": "value" (bosluklu) ve "key":"value" (bosluksuz) ikisini de gorur. */
static const char *json_str(const char *json, const char *key, char *out, size_t cap) {
  char pat[128];
  snprintf(pat, sizeof pat, "\"%s\":", key);
  const char *p = strstr(json, pat);
  if (!p) return NULL;
  p += strlen(pat);
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return NULL;
  p++;
  size_t o = 0;
  while (*p && *p != '"' && o + 1 < cap) {
    if (*p == '\\' && p[1]) {
      p++;
      switch (*p) {
        case 'n': out[o++] = '\n'; break;
        case 'r': out[o++] = '\r'; break;
        case 't': out[o++] = '\t'; break;
        default: out[o++] = *p; break;
      }
    } else {
      out[o++] = *p;
    }
    p++;
  }
  out[o] = 0;
  return out;
}

static int json_num(const char *json, const char *key) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\":", key);
  const char *p = strstr(json, pat);
  if (!p) return 0;
  p += strlen(pat);
  while (*p == ' ') p++;
  return atoi(p);
}

/* ------------------------------------------------------------------ */
/* tamamlama                                                           */
/* ------------------------------------------------------------------ */

#define OUT_MAX (1 << 20)
static char g_out[OUT_MAX];
static size_t g_n;

/* TEKIL GOZETICI: ayni label (kelime) birden fazla kaynaktan (modul,
 * sembol, Python builtin vs.) gelebilir. "print" hem kullanici fonksiyonu
 * hem builtin olabilir; "ossuruk" hem import hem workspace modulu olabilir.
 * Popup'ta ayni kelimenin 2-3 kez gorunmemesi icin her completion'da
 * label'lar bir kez yazilir. */
#define GCL_SEEN_MAX 4096
static char g_seen[GCL_SEEN_MAX][GCL_NAME_MAX];
static int g_seen_n;

/* label daha once ciktiya yazildiysa 1, ilk kez goruluyorsa 0 (+kayit). */
static int seen_add(const char *label) {
  for (int i = 0; i < g_seen_n; i++)
    if (strcmp(g_seen[i], label) == 0) return 1;
  if (g_seen_n < GCL_SEEN_MAX) {
    snprintf(g_seen[g_seen_n], GCL_NAME_MAX, "%s", label);
    g_seen_n++;
  }
  return 0;
}

static void out_append(const char *s) {
  size_t n = strlen(s);
  if (g_n + n + 1 < OUT_MAX) {
    memcpy(g_out + g_n, s, n);
    g_n += n;
    g_out[g_n] = 0;
  }
}

static void out_item(const char *label, const char *kind, const char *detail) {
  if (seen_add(label)) return; /* ayni kelime zaten listede */
  char lj[512], kj[64], dj[1024];
  json_escape(lj, sizeof lj, label);
  json_escape(kj, sizeof kj, kind);
  json_escape(dj, sizeof dj, detail);
  char item[2048];
  snprintf(item, sizeof item,
           "{\"label\":\"%s\",\"kind\":\"%s\",\"detail\":\"%s\"},",
           lj, kj, dj);
  out_append(item);
}

/* ciktidakini sifirla (tekil listesi dahil) */
static void out_begin(void) {
  g_n = 0;
  g_out[0] = 0;
  g_seen_n = 0;
}

/* ------------------------------------------------------------------ */

/* "osso" veya "ossuruk." kalibinda prefix + member cikar */
static void parse_trigger(const char *before, char *prefix, size_t cap,
                          char *member_mod, size_t mcap, int *is_member) {
  *is_member = 0;
  prefix[0] = 0;
  member_mod[0] = 0;
  size_t n = strlen(before);
  if (n == 0) return;

  /* mod. : en sagdaki noktaya bak; solda ident varsa member */
  const char *last_dot = NULL;
  for (const char *p = before; *p; p++)
    if (*p == '.') last_dot = p;
  if (last_dot) {
    /* mod. arkasinda ident olmali ve noktadan once de ident olmali */
    const char *after = last_dot + 1;
    int after_ok = 1;
    for (const char *p = after; *p; p++)
      if (!(isalnum((unsigned char)*p) || *p == '_')) { after_ok = 0; break; }
    if (after_ok) {
      /* noktadan once ident mi */
      const char *mod_start = last_dot;
      while (mod_start > before &&
             (isalnum((unsigned char)mod_start[-1]) || mod_start[-1] == '_'))
        mod_start--;
      int mod_ok = mod_start < last_dot;
      if (mod_ok) {
        size_t mlen = (size_t)(last_dot - mod_start);
        if (mlen < mcap) {
          memcpy(member_mod, mod_start, mlen);
          member_mod[mlen] = 0;
        }
        size_t alen = strlen(after);
        if (alen < cap) {
          memcpy(prefix, after, alen);
          prefix[alen] = 0;
        }
        *is_member = 1;
        return;
      }
    }
  }

  /* duz yazi: sondaki kelime prefix */
  const char *w = before + n;
  while (w > before && (isalnum((unsigned char)w[-1]) || w[-1] == '_')) w--;
  if (w < before + n) {
    size_t wl = (size_t)((before + n) - w);
    if (wl < cap) {
      memcpy(prefix, w, wl);
      prefix[wl] = 0;
    }
  }
}

/* ciktiya sembol ekle (prefix filtresi + tekil + kaynak oncelik) */
static void emit_symbol(const Symbol *sym, const char *prefix,
                        const char *src_file) {
  (void)src_file;
  if (prefix[0] && strncmp(sym->name, prefix, strlen(prefix)) != 0) return;
  char detail[GCL_PARAM_MAX + 256];
  if (sym->kind == SYM_FN)
    snprintf(detail, sizeof detail, "%s(%s)", sym->name, sym->params);
  else if (sym->kind == SYM_CLASS)
    snprintf(detail, sizeof detail, "class %s (%s.py)", sym->name, sym->mod);
  else
    snprintf(detail, sizeof detail, "%s", sym->params);
  out_item(sym->name, kind_str(sym->kind), detail);
}

/* fridge NDJSON satirini ciktiya aktar (prefix filtresi uygulanir) */
static void emit_fridge_line(const char *json_line, const char *prefix) {
  char label[GCL_NAME_MAX] = {0};
  char kind[64] = {0};
  char detail[GCL_PARAM_MAX + 128] = {0};
  json_str(json_line, "label", label, sizeof label);
  json_str(json_line, "kind", kind, sizeof kind);
  json_str(json_line, "detail", detail, sizeof detail);
  if (!label[0]) return;
  if (prefix[0] && strncmp(label, prefix, strlen(prefix)) != 0) return;
  out_item(label, kind[0] ? kind : "module", detail);
}

/* import uzerinden alias'i gercek modul adina cevir */
static const char *alias_to_mod(FileIndex *cur, const char *name) {
  if (!cur) return name;
  for (int i = 0; i < cur->import_count; i++) {
    if (cur->import_aliases[i][0] && strcmp(cur->import_aliases[i], name) == 0)
      return cur->imports[i];
  }
  return name;
}

/* CANLI TEXT alias cozumleme: disk index (cur) yalnizca KAYDEDILMIS satirlari
 * gorur; IDE her tus vurusunda text'i gonderir. "import testFolder.helloworld
 * as hw" yazilip henuz kaydedilmeden "hw." yazildiginda disk listesinde yoktur.
 * Burada imlece kadar olan kismi satir satir tarar:
 *   import <a.b.c> as <name>  ->  name = c (son segment — dosya modulu)
 *   from <pkg> import <X> as <name> -> name = <pkg>
 * Bulunamazsa name aynen doner (eski davranis). */
static const char *live_alias_to_mod(FileIndex *cur, const char *text,
                                     const char *until, const char *name,
                                     char *out, size_t cap) {
  const char *mapped;
  if (name == NULL || name[0] == '\0') return name;
  mapped = alias_to_mod(cur, name);
  if (mapped != name) return mapped;
  if (text == NULL || until == NULL || until <= text) return name;
  out[0] = 0;

  {
    const char *q = text;
    while (q < until && *q) {
      const char *line_start = q;
      /* "import X as Y" */
      if (strncmp(q, "import ", 7) == 0) {
        const char *imp = q + 7;
        const char *as = strstr(imp, " as ");
        if (as && as < until) {
          const char *a = as + 4;
          while (a < until && *a == ' ') a++;
          {
            size_t nl = strlen(name);
            if (strncmp(a, name, nl) == 0 &&
                (a[nl] == '\0' || a[nl] == '\n' || a[nl] == '\r' ||
                 isspace((unsigned char)a[nl]))) {
              size_t mlen = (size_t)(as - imp);
              while (mlen > 0 && (imp[mlen - 1] == ' ' || imp[mlen - 1] == '\t'))
                mlen--;
              if (mlen > 0 && mlen < cap) {
                memcpy(out, imp, mlen);
                out[mlen] = 0;
                {
                  char *dot = strrchr(out, '.');
                  if (dot && dot[1]) {
                    size_t rem = strlen(dot + 1);
                    memmove(out, dot + 1, rem + 1);
                  }
                }
                return out;
              }
            }
          }
        }
      }
      /* "from P import X as Y" -> Y alias'i P paketine bagla */
      if (strncmp(q, "from ", 5) == 0) {
        const char *fimp = strstr(q, " import ");
        if (fimp && fimp < until) {
          const char *n = fimp + 8;
          while (n < until && *n == ' ') n++;
          {
            const char *as = strstr(n, " as ");
            if (as && as < until) {
              const char *a = as + 4;
              while (a < until && *a == ' ') a++;
              {
                size_t nl = strlen(name);
                if (strncmp(a, name, nl) == 0 &&
                    (a[nl] == '\0' || a[nl] == '\n' || a[nl] == '\r' ||
                     isspace((unsigned char)a[nl]))) {
                  size_t mlen = (size_t)(fimp - (q + 5));
                  while (mlen > 0 && (q + 5)[mlen - 1] == ' ') mlen--;
                  if (mlen > 0 && mlen < cap) {
                    memcpy(out, q + 5, mlen);
                    out[mlen] = 0;
                    return out;
                  }
                }
              }
            }
          }
        }
      }
      while (q < until && *q && *q != '\n') q++;
      if (q < until && *q == '\n') q++;
      (void)line_start;
    }
  }
  return name;
}

/* open file'in import listesini baslangic dosyasindan bulur.
 * Windows'ta buyuk/kucuk harf farkini yok say: disk yollari
 * case-insensitive'dir. */
static FileIndex *find_file(Workspace *ws, const char *file) {
  if (!file) return NULL;
  for (int i = 0; i < ws->file_count; i++) {
    if (path_eq(ws->files[i].path, file)) return &ws->files[i];
  }
  return NULL;
}

static void complete(Workspace *ws, const char *file, const char *text,
                     int line, int col) {
  /* text'te `line` (1-based) satirin ilk `col` karakterini al */
  const char *p = text;
  for (int i = 1; i < line && p && *p; i++) {
    p = strchr(p, '\n');
    if (!p) break;
    p++;
  }
  if (!p) p = text;
  /* line boyunca ilerle: col-1 kadar karakter (0-based col kabul ediyoruz) */
  for (int i = 0; i < col - 1 && *p; i++) p++;
  /* satir icindeki konum: before = satir basindan imlece */
  const char *line_start = p;
  while (line_start > text && line_start[-1] != '\n') line_start--;

  /* 1MB buffer stack'i asar: static alanda tut */
  static char before[GCL_LINE_MAX];
  size_t bl = (size_t)(p - line_start);
  if (bl >= sizeof before) bl = sizeof before - 1;
  memcpy(before, line_start, bl);
  before[bl] = 0;

  char prefix[GCL_NAME_MAX];
  char member_mod[GCL_NAME_MAX];
  int is_member;
  parse_trigger(before, prefix, sizeof prefix, member_mod, sizeof member_mod,
                &is_member);

  /* kardes dosyanin import listesi (member cozumleme + once siralamasi) */
  FileIndex *cur = find_file(ws, file);
  const char *cur_name = cur ? cur->name : NULL;

  out_begin();
  /* stdio yanit bicimi {"id":N,"result":[...]} olmali (VALID JSON).
   * "result:" sarmalayicisi olmadan {"id":2,[...]} GECERSIZ JSON uretilir;
   * Electron main.ts JSON.parse'e takilir, lspPending asili kalir ve
   * renderer her tus vurusunda 400ms bekleyip fallback'e duserdi —
   * "from folder import file calismiyor + tamamlama yavasladi" nin
   * dogrudan kaynagi buydu. */
  out_append("\"result\":[");

  if (is_member) {
    const char *mod = member_mod;
    /* alias? "import numpy as np" -> np. => numpy.
     * ONCE disk listesi (kaydedilmis dosya), yoksa CANLI TEXT'teki
     * "import X as Y" / "from P import Y as Z" satirlarina bakariz:
     * IDE her tus vurusunda text'i gonderdigi icin henuz kaydedilmemis
     * importlar da cozulur. */
    {
      static char live_mod[GCL_NAME_MAX];
      mod = live_alias_to_mod(cur, text, p, mod, live_mod, sizeof live_mod);
    }
    /* 0) mod bir PAKET (klasor) adiysa icindeki .py modullerini oner:
     *    "alpha." -> alpha/ icindeki util ; "import alpha." -> ayni akis.
     *    Bu, paket member tamamlamayi ve noktali import segment onerilerini
     *    tek noktada cozer. Bulunduysa fridge/hata yolu atlanir. */
    if (emit_package_members(ws, mod, prefix)) goto member_done;
    /* 1) statik: workspace'teki kardes dosya — PAKET-BILINCLI: ayni ada
     *    sahip dosyalar farkli klasorlerdeyse (alpha/util.py + beta/util.py)
     *    acik dosyanin import ettigi paket oncelik alir; basename yalnizca
     *    son care olur. (dokuman 2-1 cozumu) */
    FileIndex *modf = resolve_module_pkg(ws, file, mod, cur);
    if (modf) {
      for (int s = modf->sym_start; s < modf->sym_start + modf->sym_count; s++) {
        const Symbol *sym = &ws->syms[s];
        if (!prefix[0] || strncmp(sym->name, prefix, strlen(prefix)) == 0)
          emit_symbol(sym, prefix, file);
      }
    }
    /* 2) BUZDOLABI: statik yetmedi -> gercek Python'a sor.
     *    "import os" => os. => os|prefix gercek stdlib sembolleri. */
    if (!modf) {
      const char *cached = fridge_get(ws, mod);
      if (cached && cached[0]) {
        char linebuf[4096];
        const char *nl = cached;
        while (*nl) {
          const char *end = strchr(nl, '\n');
          size_t ln = end ? (size_t)(end - nl) : strlen(nl);
          if (ln == 0) break;
          if (ln >= sizeof linebuf) ln = sizeof linebuf - 1;
          memcpy(linebuf, nl, ln);
          linebuf[ln] = 0;
          emit_fridge_line(linebuf, prefix);
          if (!end) break;
          nl = end + 1;
        }
      } else {
        /* gorunmeyen modul: kullaniciya HATA MESAJI gonderilmez. Fridge
         * bulamadiysa (ornegin alpha bir paket adi ama stdlib'de yok) sessiz
         * gec — popup'a "import error" / "resolve error" sizintisi olmaz. */
      }
    }
  } else {
    /* 0x) CANLI YEREL DEGISKENLER: "screenWidth = 1600" gibi henuz
     *      KAYDEDILMEMIS satirlar imlece kadar taranir ve const olarak
     *      onerilir. "InitWindow(scre" yazildiginda screenWidth/screenHeight
     *      gibi degiskenler prefix ile eslesir (disk index'i beklemeden). */
    {
      const char *q = text;
      while (q < p && *q) {
        /* satir basi: NAME = value (tek satirlik, ust seviye) */
        const char *ln = q;
        const char *eq = strchr(ln, '=');
        if (eq && eq < p) {
          const char *ne = strstr(ln, "!=");
          const char *cel = strstr(ln, "<=");
          const char *gel = strstr(ln, ">=");
          if (ne != eq && cel != eq && gel != eq) {
            size_t ln_ = (size_t)(eq - ln);
            while (ln_ > 0 && (ln[ln_ - 1] == ' ' || ln[ln_ - 1] == '\t')) ln--;
            if (ln_ > 0 && ln_ < GCL_NAME_MAX) {
              char nm[GCL_NAME_MAX];
              memcpy(nm, ln, ln_);
              nm[ln_] = 0;
              if (is_ident(nm) && strcmp(nm, "self") != 0 &&
                  strcmp(nm, "cls") != 0) {
                if (!prefix[0] || strncmp(nm, prefix, strlen(prefix)) == 0) {
                  char val[GCL_NAME_MAX] = {0};
                  const char *v = eq + 1;
                  while (*v == ' ') v++;
                  /* deger SATIR SONUNDA biter (text tek buffer: sonraki
                   * satirlar da `strlen(v)` icinde; \n'de kIrp) */
                  size_t vl = 0;
                  while (v[vl] && v[vl] != '\n' && v[vl] != '\r') vl++;
                  if (vl >= sizeof val) vl = sizeof val - 1;
                  memcpy(val, v, vl);
                  val[vl] = 0;
                  {
                    char det[GCL_NAME_MAX + 64];
                    snprintf(det, sizeof det, "%s = %s", nm, val);
                    out_item(nm, "const", det);
                  }
                }
              }
            }
          }
        }
        while (*q && *q != '\n') q++;
        if (*q == '\n') q++;
      }
    }
    /* 0a) "from MOD import PREFIX" — import edilen modulun UYELERINI oner.
     *      "from pyRaylib import ossu" yazildiginda pyRaylib.py'nin
     *      (baska bir .py dosyasi) ossurmak/zemberek/KEY_ENTER gibi uyeleri
     *      ozel olarak kullanima verilir. Modul workspace'te yoksa
     *      BUZDOLABI (gercek Python) devreye girer. */
    {
      int from_done = 0;
      if (strncmp(before, "from ", 5) == 0) {
        const char *fin = strstr(before + 5, " import ");
        if (fin) {
          char fmod[GCL_NAME_MAX] = {0};
          size_t mlen = (size_t)(fin - (before + 5));
          if (mlen > 0 && mlen < sizeof fmod) {
            memcpy(fmod, before + 5, mlen);
            fmod[mlen] = 0;
          }
          if (fmod[0]) {
            /* KISAYOL: "from pydir import test" — pydir bir KLASOR
             * (paket), modul degil. Icine bak: adi prefix ile eslesen
             * .py dosyalari modul olarak onerilir. */
            {
              char dbase[GCL_NAME_MAX];
              int found_dir = 0;
              for (int i = 0; i < ws->file_count; i++) {
                file_dir_base(ws, ws->files[i].path, dbase, sizeof dbase);
                if (!dbase[0] || strcmp(dbase, fmod) != 0) continue;
                found_dir = 1;
                if (!prefix[0] ||
                    strncmp(ws->files[i].name, prefix, strlen(prefix)) == 0) {
                  char detail[GCL_PATH_MAX + 64];
                  snprintf(detail, sizeof detail, "module %s (%s)",
                           ws->files[i].name, ws->files[i].path);
                  out_item(ws->files[i].name, "module", detail);
                  from_done = 1;
                }
              }
              if (found_dir) goto from_ctx_done;
            }
            FileIndex *fmodf = resolve_module(ws, file, fmod);
            if (fmodf) {
              from_done = 1;
              for (int s = fmodf->sym_start;
                   s < fmodf->sym_start + fmodf->sym_count; s++) {
                const Symbol *sym = &ws->syms[s];
                if (!prefix[0] ||
                    strncmp(sym->name, prefix, strlen(prefix)) == 0)
                  emit_symbol(sym, prefix, file);
              }
            } else {
              const char *cached = fridge_get(ws, fmod);
              if (cached && cached[0]) {
                from_done = 1;
                char linebuf[4096];
                const char *nl = cached;
                while (*nl) {
                  const char *end = strchr(nl, '\n');
                  size_t ln = end ? (size_t)(end - nl) : strlen(nl);
                  if (ln == 0) break;
                  if (ln >= sizeof linebuf) ln = sizeof linebuf - 1;
                  memcpy(linebuf, nl, ln);
                  linebuf[ln] = 0;
                  emit_fridge_line(linebuf, prefix);
                  if (!end) break;
                  nl = end + 1;
                }
              }
            }
          }
        }
      }
      if (from_done) goto from_ctx_done;
    }
    /* 0b) "from X import *" — X'in UYELERI bu dosyada dogrudan kullanilir.
     *      "from pyRaylib import *" yazildiktan sonra InitWind, ClearBg gibi
     *      fonksiyonlar modul adi olmadan onerilir.
     *      ONCE disk index (kaydedilmis dosya), SONRA canli text (henuz
     *      kaydedilmemis — IDE her tus vurusunda text'i gonderir). */
    {
      const char *wc_mod = NULL;
      char wc_buf[GCL_NAME_MAX] = {0};
      if (cur && cur->wildcard[0]) {
        wc_mod = cur->wildcard;
      } else if (p > text) {
        /* text'in imlece kadar olan kisminda "from MOD import *" ara.
         * Satir satir ilerle; modul adi = "from " ile " import " arasi. */
        const char *q = text;
        while (q < p && *q) {
          if (strncmp(q, "from ", 5) == 0) {
            const char *imp = strstr(q, "import");
            const char *star = strstr(q, "*");
            if (imp && star && imp < star && star < p) {
              size_t mlen = (size_t)(imp - (q + 5));
              while (mlen > 0 && (q + 5)[mlen - 1] == ' ') mlen--;
              if (mlen > 0 && mlen < sizeof wc_buf) {
                memcpy(wc_buf, q + 5, mlen);
                wc_buf[mlen] = 0;
                wc_mod = wc_buf;
                break;
              }
            }
          }
          while (*q && *q != '\n') q++;
          if (*q == '\n') q++;
        }
      }
      if (wc_mod) {
        FileIndex *wm = resolve_module(ws, file, wc_mod);
        if (!wm) {
          for (int i = 0; i < ws->file_count; i++) {
            char base[GCL_NAME_MAX];
            strip_ext(base, sizeof base, ws->files[i].path);
            if (strcmp(base, wc_mod) == 0) { wm = &ws->files[i]; break; }
          }
        }
        if (wm) {
          for (int s = wm->sym_start; s < wm->sym_start + wm->sym_count; s++) {
            const Symbol *sym = &ws->syms[s];
            if (!prefix[0] || strncmp(sym->name, prefix, strlen(prefix)) == 0)
              emit_symbol(sym, prefix, file);
          }
        } else {
          /* workspace'te yoksa: BUZDOLABI — gercek Python modulu */
          const char *cached = fridge_get(ws, wc_mod);
          if (cached && cached[0]) {
            char linebuf[4096];
            const char *nl = cached;
            while (*nl) {
              const char *end = strchr(nl, '\n');
              size_t ln = end ? (size_t)(end - nl) : strlen(nl);
              if (ln == 0) break;
              if (ln >= sizeof linebuf) ln = sizeof linebuf - 1;
              memcpy(linebuf, nl, ln);
              linebuf[ln] = 0;
              emit_fridge_line(linebuf, prefix);
              if (!end) break;
              nl = end + 1;
            }
          }
        }
      }
    }
    /* 0) import edilmis modul adlari (once) */
    if (cur) {
      /* hem isim hem alias onerilir */
      for (int i = 0; i < cur->import_count; i++) {
        const char *disp = cur->import_aliases[i][0]
                               ? cur->import_aliases[i]
                               : cur->imports[i];
        if (!prefix[0] || strncmp(disp, prefix, strlen(prefix)) == 0) {
          char detail[GCL_NAME_MAX + 64];
          snprintf(detail, sizeof detail, "module %s (import)", disp);
          out_item(disp, "module", detail);
        }
      }
    }
    /* 1) workspace modul adlari — YALNIZCA import/from yazarken:
     *    "import o" -> os, ossuruk gibi moduller onerilir. Duz metinde
     *    modul adi onermek "statik trash" olarak algilanir. */
    if (strncmp(before, "import ", 7) == 0 ||
        strncmp(before, "from ", 5) == 0) {
      /* Klasor paketleri: "from pyF" -> pyFiles (veya pydir) onerilir.
       * Ayni klasorun her dosyasi icin ad tekrar etse de out_item tekil
       * gozetmen (seen_add) sayesinde BIR kez yazar. */
      {
        char dbase[GCL_NAME_MAX];
        for (int i = 0; i < ws->file_count; i++) {
          file_dir_base(ws, ws->files[i].path, dbase, sizeof dbase);
          if (!dbase[0]) continue;
          if (!prefix[0] ||
              strncmp(dbase, prefix, strlen(prefix)) == 0) {
            char detail[GCL_PATH_MAX + 64];
            snprintf(detail, sizeof detail, "package %s (folder)", dbase);
            out_item(dbase, "module", detail);
          }
        }
      }
      for (int i = 0; i < ws->file_count; i++) {
        if (!prefix[0] ||
            strncmp(ws->files[i].name, prefix, strlen(prefix)) == 0) {
          char detail[GCL_PATH_MAX + 64];
          snprintf(detail, sizeof detail, "module %s (%s)", ws->files[i].name,
                   ws->files[i].path);
          out_item(ws->files[i].name, "module", detail);
        }
      }
    }
    /* 2) YALNIZCA acik dosyanin kendi sembolleri. Baska dosyalarin
     *    sembolleri (import edilmemisse) onerilmez — "number", "dumper"
     *    gibi test dosyalari duz metinde statik trash olarak gorunur. */
    for (int s = 0; s < ws->sym_count; s++) {
      const Symbol *sym = &ws->syms[s];
      if (cur_name && strcmp(sym->mod, cur_name) == 0) {
        if (!prefix[0] || strncmp(sym->name, prefix, strlen(prefix)) == 0)
          emit_symbol(sym, prefix, file);
      }
    }
    /* 3) Python'un KENDI dili: keywords + builtins ("pri" -> print).
     *    python_syntax.c tablosu, prefix ile eslesenleri doldurur. */
    {
      const int pn = py_syntax_count(prefix);
      for (int i = 0; i < pn; i++) {
        char lbl[GCL_NAME_MAX], knd[64], det[GCL_NAME_MAX + 64];
        py_syntax_at(i, prefix, lbl, sizeof lbl, knd, sizeof knd, det, sizeof det);
        if (lbl[0]) out_item(lbl, knd[0] ? knd : "keyword", det);
      }
    }
  }

member_done:
from_ctx_done:
  /* son virgullu kapatma ortak nokta: from-import ciktisiyla da calisir */

  /* son virgulu kaldir */
  if (g_n > 1 && g_out[g_n - 1] == ',') {
    g_out[g_n - 1] = ']';
    g_out[g_n] = 0;
  } else {
    out_append("]");
  }
}

/* ------------------------------------------------------------------ */
/* RPC loopu                                                           */
/* ------------------------------------------------------------------ */

/* Yolu mutlak yap: Windows'ta _fullpath, digerlerinde realpath. Basarisizsa
 * girdi aynen dondurulur. */
static void absolutize(char *out, size_t cap, const char *in) {
#if defined(_WIN32)
  char *r = _fullpath(out, in, (size_t)cap);
  if (r == NULL) snprintf(out, cap, "%s", in);
#else
  char resolved[GCL_PATH_MAX];
  if (realpath(in, resolved) != NULL) {
    snprintf(out, cap, "%s", resolved);
  } else {
    snprintf(out, cap, "%s", in);
  }
#endif
}

static void strip_crlf(char *s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}

static void send_response(int id, const char *body) {
  /* NDJSON: tek satir */
  printf("{\"id\":%d,%s}\n", id, body);
  fflush(stdout);
}

int main(void) {
  /* Workspace (100MB+ sembol havuzu) ve satir buffer'i stack'e sigmaz:
   * static (global) alanda tut — aksi halde Windows'ta aninda stack
   * overflow olur ve program tek satir cikti vermeden coker. */
  static Workspace ws;
  static char line[GCL_LINE_MAX];
  memset(&ws, 0, sizeof ws);

  while (fgets(line, sizeof line, stdin)) {
    strip_crlf(line);
    if (!*line) continue;

    char method[128] = {0};
    json_str(line, "method", method, sizeof method);
    int id = json_num(line, "id");

    if (strcmp(method, "initialize") == 0) {
      char root[GCL_PATH_MAX] = {0};
      char abs_root[GCL_PATH_MAX] = {0};
      json_str(line, "root", root, sizeof root);
      /* Relative root, didChange sonrasi yanlis dizin taramasina yol aciyor
       * (files:2048 aniomalisi). Root'u mutlak yap — electron zaten mutlak
       * gonderir; burada guvenlik kusagi olarak normalize edilir. */
      absolutize(abs_root, sizeof abs_root, root);
      int n = abs_root[0] ? index_workspace(&ws, abs_root) : 0;
      char resp[512];
      snprintf(resp, sizeof resp, "\"result\":{\"ok\":true,\"files\":%d,\"root\":\"%s\"}",
               n, abs_root);
      send_response(id, resp);
    } else if (strcmp(method, "shutdown") == 0) {
      send_response(id, "\"result\":null");
      break;
    } else if (strcmp(method, "textDocument/didChange") == 0) {
      /* Dosya eklendi / silindi / degisti -> workspace'i YENIDEN INDEXLE.
       * IDE acikken yeni klasor + py script eklenince LSP bunu gorur;
       * aksi halde yalnizca proje acilisindaki eski index kalir ve
       * "from yeniKlasor import ..." asla cozulemezdi. Fridge cache
       * (g_fridge) ayri tutuldugu icin burada korunur. */
      int n = index_workspace(&ws, ws.root);
      char resp[128];
      snprintf(resp, sizeof resp,
               "\"result\":{\"ok\":true,\"files\":%d}", n);
      send_response(id, resp);
    } else if (strcmp(method, "textDocument/completion") == 0) {
      static char file[GCL_PATH_MAX];
      static char text[GCL_LINE_MAX]; /* 1MB — stack'i asar, static olmali */
      file[0] = 0;
      text[0] = 0;
      json_str(line, "file", file, sizeof file);
      json_str(line, "text", text, sizeof text);
      int ln = json_num(line, "line");
      int col = json_num(line, "col");
      if (!ws.root[0]) {
        /* initialize gelmedi: root'u file'ın dizininden kur */
        char root[GCL_PATH_MAX] = {0};
        snprintf(root, sizeof root, "%s", file);
        char *sl = strrchr(root, '/');
        while (sl && sl != root) {
          *sl = 0;
          if (access(root, 0) == 0) break;
          sl = strrchr(root, '/');
        }
        if (root[0]) index_workspace(&ws, root);
      }
      complete(&ws, file, text, ln, col);
      /* 1MB+ body static g_out'dan: son virgul temizlendi, yanit yaz */
      send_response(id, (const char *)g_out);
    }
  }
  return 0;
}
