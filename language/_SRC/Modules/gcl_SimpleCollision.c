/*
RaylibSimpleCollision.TerrainCollision(PLAYER, Terrain);
RaylibSimpleCollision.SimpleBoxCollision(PLAYER, MESH1, MESH2, ...);
*/

/* Backend.

   GODOT CharacterBody3D MODELI, SAF SEKIL COZUMU.

   Oyuncu bir NOKTA ya da bir kutu degil, DIMDIK DURAN BIR KAPSULDUR:

       yaricap  COL_PLAYER_RADIUS   (0.3)
       boy      PLAYER.Height       (1.8)
       ayak     PLAYER.Position.y

   Kapsul, ayak ile bas arasindaki dogru parcasinin yaricap kadar ICERI
   cekilmis halidir: eksen a = (x, ayak + r, z), b = (x, bas - r, z).

   CARPISMA RAYCAST DEGIL, SEKIL-UCGEN testidir: kapsul ekseni ile her ucgenin
   arasindaki EN KISA MESAFE hesaplanir; mesafe yaricaptan kucukse kapsul
   ucgenin ICINE girmis demektir ve o kadar geri itilir.

   BU BIR YUKSEKLIK ALANI DEGILDIR. Arazi bir ucgen kumesidir: magaralar,
   tuneller, kopruler, cikintilar ve tavanlar OLABILIR. "Arazi" bir (x, z)
   icin tek bir yukseklik donduren bir zemin DEGIL, her yonde yuzeyi olan bir
   hacimdir. Bu yuzden hicbir yerde "ayagin altindaki en yuksek yuzey" gibi
   bir soru sorulmaz: boyle bir soru, magaranin tavanini zemin sanip oyuncuyu
   yukari isinlardi.

   Cozum YALNIZCA iki soruya bakar:
     1. Kapsul bir ucgenle kesisiyor mu (en yakin mesafe < yaricap)?
     2. Kesistigi noktada itme hangi yone olmali (yurunebilir zemin mi duvar mi)?

   Egim kendiliginden dogru davranir: dik bir yuzeyin normali yataydir, itme
   yatay olur; oyuncu duvara tirmanamaz, boyunca KAYAR. Yurunebilir bir egimde
   itme yukaridir; oyuncu yurur.

   SIMPLEBOXCOLLISION, ayni sekil cozumunu verilen MESH'lerin ucgenlerine
   uygular. Ucgenler DUNYA uzayinda, Position/Rotate/Scale uygulanmis olarak
   gelir (gcl_simplemesh_triangle), yani DONDURULMUS bir kutu da dogru temsil
   edilir. Raylib'in CheckCollisionBoxes'i yalnizca eksen-hizali kutularla
   calisir ve dondurulmus bir kutuyu yanlis temsil ederdi; bu yuzden burada
   kullanilmaz.

   Sekil cozumu ucgen verisine ihtiyac duyar; ucgenlerin sahibi
   RaylibSimpleMesh.dll'dir ve oradan DUNYA uzayinda alinir
   (gcl_simplemesh_triangle). Cizilen geometri ile carpisilan geometri boylece
   tek kaynaktan gelir. */

#include "gcl_module.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COL_PLAYER_SLOTS  27
#define COL_BOX_SLOTS     12   /* the Mesh struct: Material + Handle + transform */
#define COL_SLOT_COUNT    27

#define COL_DEG2RAD       0.017453292519943295

/* Kapsul yaricapi. FPS tipinde boyle bir yaprak yok ve tipi degistirmek
   RaylibFPS.dll ile runtime'i da degistirirdi; o yuzden modul sabitidir.
   0.3 kabaca bir omuz genisligidir. */
#define COL_PLAYER_RADIUS 0.3

/* YURUNEBILIRLIK ESIGI (Godot `floor_max_angle`). Bir temas normalinin YUKARI
   bileseni bu degerden BUYUKSE yuzey yurunebilir sayilir (hafif egim, duz
   zemin) ve itme YALNIZCA DIKEY olur. Kucukse dik sayilir (duvar/kayalik) ve
   itme normal boyunca, yani YATAY olur.

   0.7071 = cos(45). Bu esik OLMADAN egik bir yuzeyin normali boyunca itmek
   oyuncuyu yukari VE GERI tasir; geri bilesen ileri hareketi yedigi icin
   hafif egimler bile tirmanilamaz hale gelir. */
#define COL_COS_SLOPE 0.7071

#define COL_MAX_COLLIDERS   8     /* tek cagrida en fazla kac mesh        */

/* Cozulme gecisi: her gecisde EN DERIN temas bulunup uygulanir, sonra kapsul
   tazelenir. Godot da cozumu tek seferde degil, iteratif yapar: bir itme
   oyuncuyu ikinci bir yuzeye sokabilir. */
#define COL_SOLVE_ITERS     6

/* Ucgen tamponu BUYUYEBILIR. Arazi objesi disaridan gelir ve boyutu ONCEDEN
   BILINMEZ: sabit bir tavan, tavani asan bir arazide collision'in BIR KISMINI
   sessizce dusururdu — karakterin zeminin bir parcasindan gecmesi tam olarak
   bu olurdu. Tampon bu yuzden malloc/realloc ile buyur. */
#define COL_TRIS_INIT 16384

/* ARAZI IZGARASI: X/Z duzleminde hucre boyu (metre) ve eksen basina en fazla
   hucre. Hucre kapsul yaricapindan (0.3) belirgin sekilde buyuk secilir ki
   kapsul bir hucrenin kenarindayken komsu hucreler de gezilsin ve temas
   kacmasin. COL_GRID_MAX cok buyuk bir arazide bellegi sinirlar; asilirsa
   hucre boyu buyur (izgara kabalasir) ama DOGRULUK bozulmaz — yalnizca
   elenen ucgen sayisi azalir. */
#define COL_GRID_CELL 4.0
#define COL_GRID_MAX  512

/* SUPURMELI COZUMDE tek karede en fazla kac adim. Tavan, sonsuz donguye ve
   asiri hizda kare basina binlerce cozume karsi korur; 64 adim x 0.3 m = 19 m
   tek karede, ki bu hicbir oynanabilir sahneye sigmaz. */
#define COL_MAX_SWEEP_STEPS 64

enum {
    S_POS_X = 0, S_POS_Y, S_POS_Z,
    S_ROT_X, S_ROT_Y, S_ROT_Z,
    S_LOOK_X, S_LOOK_Y, S_LOOK_Z,
    S_HEIGHT, S_GRAVITY, S_GRAVITY_ON,
    S_FORWARD, S_BACKWARD, S_LEFT, S_RIGHT,
    S_CAM_POS_X, S_CAM_POS_Y, S_CAM_POS_Z,
    S_CAM_TGT_X, S_CAM_TGT_Y, S_CAM_TGT_Z,
    S_CAM_UP_X, S_CAM_UP_Y, S_CAM_UP_Z,
    S_CAM_FOVY, S_CAM_PROJECTION
};

enum { T_TEXTURE = 0, T_COLOR, T_HANDLE };

/* Mesh struct'inin yuvasi - gcl_SimpleMesh.c ve gcl_native_types.c ile AYNI
   sira (Material.Texture, Material.Color, Handle, Position/Rotate/Scale). */
enum {
    B_TEXTURE = 0, B_COLOR, B_HANDLE,
    B_POS_X, B_POS_Y, B_POS_Z,
    B_ROT_X, B_ROT_Y, B_ROT_Z,
    B_SCALE_X, B_SCALE_Y, B_SCALE_Z
};

static double g_slot[COL_SLOT_COUNT];

/* ONCEKI KARENIN AYAK YUKSEKLIGI — tunnel korumasi icin.

   NEDEN SART: cozum YALNIZCA ORTUSME arar. Oyuncu hizli duserse tek karede
   yuzeyin USTUNDEN ALTINA gecebilir; o anda kapsul hicbir ucgenle KESISMEZ,
   resolve_pass "temas yok" der ve oyuncu arazinin ICINDEN gecer. Bu tam
   olarak "fps dusunce laptopta terrain collision calismiyor" diye bildirilen
   hal: FPS dustukce dt buyur, adim yaricapi asar ve gecis baslar.

   HIZ SINIRI TEK BASINA COZMEZ: yercekimi hizi sinirsiz buyutur. dt 1/30'da
   kilitli olsa bile ~1 saniyelik dususten sonra adim 0.3 m'yi asar. Cozum,
   hareketi YARICAP BOYUNDA adimlara bolup HER adimda cozmektir (swept
   collision): boylece kapsul yuzeye her zaman en fazla bir yaricap yaklasir
   ve temas KACIRILAMAZ. Deger, ilk karede supurme yapilmasin diye cok
   asagidan baslar. */
static double g_last_feet_y = -1.0e30;

/* ---------- 3B vektor yardimcilari ---------- */

static void v3(double *o, double x, double y, double z) { o[0] = x; o[1] = y; o[2] = z; }
static void vcopy(double *o, const double *a) { o[0] = a[0]; o[1] = a[1]; o[2] = a[2]; }
static void vsub(double *o, const double *a, const double *b) {
    o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2];
}
static void vscale(double *o, const double *a, double s) {
    o[0] = a[0] * s; o[1] = a[1] * s; o[2] = a[2] * s;
}
static double vdot(const double *a, const double *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static double vlen2(const double *a) { return vdot(a, a); }
static double vlen(const double *a) { return sqrt(vlen2(a)); }
static double vdist(const double *a, const double *b) {
    double d[3]; vsub(d, a, b); return vlen(d);
}

/* ---------- nokta -> ucgen: en yakin nokta (Ericson, RTCD) ----------

   Nokta ucgenin hangi bolgesine (kose / kenar / ic yuz) dusuyorsa oraya
   yansitilir. Bu, carpismanin kose ve kenarlarda da dogru olmasini saglar;
   yalnizca yuz duzlemine yansitmak, kapsulu ucgenin disindaki bosluga
   iterdi. */
static void closest_pt_tri(const double p[3],
                           const double a[3], const double b[3], const double c[3],
                           double q[3]) {
    double ab[3], ac[3], ap[3], bp[3], cp[3];
    double d1, d2, d3, d4, d5, d6, vc, vb, va, denom, v, w;

    vsub(ab, b, a); vsub(ac, c, a); vsub(ap, p, a);
    d1 = vdot(ab, ap); d2 = vdot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) { vcopy(q, a); return; }

    vsub(bp, p, b);
    d3 = vdot(ab, bp); d4 = vdot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) { vcopy(q, b); return; }

    vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        v = d1 / (d1 - d3);
        q[0] = a[0] + ab[0] * v; q[1] = a[1] + ab[1] * v; q[2] = a[2] + ab[2] * v;
        return;
    }

    vsub(cp, p, c);
    d5 = vdot(ab, cp); d6 = vdot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) { vcopy(q, c); return; }

    vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        w = d2 / (d2 - d6);
        q[0] = a[0] + ac[0] * w; q[1] = a[1] + ac[1] * w; q[2] = a[2] + ac[2] * w;
        return;
    }

    va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        double bc[3];
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        vsub(bc, c, b);
        q[0] = b[0] + bc[0] * w; q[1] = b[1] + bc[1] * w; q[2] = b[2] + bc[2] * w;
        return;
    }

    denom = 1.0 / (va + vb + vc);
    v = vb * denom; w = vc * denom;
    q[0] = a[0] + ab[0] * v + ac[0] * w;
    q[1] = a[1] + ab[1] * v + ac[1] * w;
    q[2] = a[2] + ab[2] * v + ac[2] * w;
}

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

/* ---------- iki dogru parcasi arasindaki en yakin noktalar ----------

   Kapsul ekseni ile ucgen KENARLARI arasindaki mesafe bununla olculur. */
static double seg_seg_closest(const double p1[3], const double q1[3],
                              const double p2[3], const double q2[3],
                              double c1[3], double c2[3]) {
    double d1[3], d2[3], r[3];
    double a, e, f, c, b, denom, s, t;
    const double eps = 1.0e-12;

    vsub(d1, q1, p1); vsub(d2, q2, p2); vsub(r, p1, p2);
    a = vdot(d1, d1); e = vdot(d2, d2); f = vdot(d2, r);

    if (a <= eps && e <= eps) { vcopy(c1, p1); vcopy(c2, p2); return vdist(c1, c2); }

    if (a <= eps) {
        s = 0.0;
        t = clamp01(f / e);
    } else {
        c = vdot(d1, r);
        if (e <= eps) {
            t = 0.0;
            s = clamp01(-c / a);
        } else {
            b = vdot(d1, d2);
            denom = a * e - b * b;
            s = (denom > eps) ? clamp01((b * f - c * e) / denom) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) { t = 0.0; s = clamp01(-c / a); }
            else if (t > 1.0) { t = 1.0; s = clamp01((b - c) / a); }
        }
    }

    c1[0] = p1[0] + d1[0] * s; c1[1] = p1[1] + d1[1] * s; c1[2] = p1[2] + d1[2] * s;
    c2[0] = p2[0] + d2[0] * t; c2[1] = p2[1] + d2[1] * t; c2[2] = p2[2] + d2[2] * t;
    return vdist(c1, c2);
}

/* ---------- ucgen kumesi (kapsulun carpistigi geometri) ----------

   Ucgenler DUNYA uzayinda, mesh modulunden alinir ve her biri KENDI AABB'siyle
   saklanir: AABB testi kapsulun uzagindaki ucgenleri neredeyse bedava eler,
   asil (pahali) kapsul-ucgen testi yalnizca yakin ucgenlerde calisir. */
typedef struct {
    double v[9];
    double lo[3], hi[3];
} ColTri;

/* X/Z DUZLEMINDE BIR IZGARA. Ucgenleri kapsule gore onceden eler.

   NEDEN: resolve_pass her cagride butun kumeyi tarar ve AABB testini her
   ucgen icin yapar. fps_first_demo'da arazi 41.454 ucgendir ve resolve_pass
   kare basina COL_SOLVE_ITERS (6) kez cagrilir; yani kare basina yuz binlerce
   AABB testi. Oysa oyuncunun o karede
   dokunabilecegi ucgen sayisi birkac douzendir.

   Y KOVALANMAZ. Yalnizca X ve Z bolunur, ucgenin Y araligi ne olursa olsun
   icinde bulundugu X/Z hucresine yazilir. Sebep: arazi bir yukseklik alani
   DEGIL — magaralar ve tavanlar vardir. Y'yi de kovalarsak bir tavanin
   altindaki oyuncu onu bulamaz. X/Z bolmesi bu dogrulugu BOZMAZ; yalnizca
   ayni sutundaki az sayida ucgen fazladan test edilir.

   Izgara YALNIZCA ARAZININ onbelleginde kurulur. Scratch kume
   (SimpleBoxCollision'in collider'lari) kucuktur ve her karede degisir; ona
   izgara kurmak, kazanci kurma maliyetine yedirir. */
typedef struct {
    int *items;      /* bu hucredeki ucgen indeksleri */
    int  count;
    int  cap;
} GridCell;

typedef struct {
    GridCell *cells;
    int    nx, nz;       /* hucre sayisi                */
    double min_x, min_z; /* izgaranin sol-alt kosesi    */
    double cell;         /* hucre boyu (kare, metre)    */
    int    built;        /* 0 ise resolve_pass tam tarar */
} TriGrid;

typedef struct {
    ColTri *tris;
    int     count;   /* kullanimdaki ucgen sayisi */
    int     cap;     /* ayrilmis kapasite        */
    TriGrid grid;    /* yalnizca ARAZI icin kurulur */
} TriBuffer;

/* IKI AYRI KUME. `g_tris` bir CAGRI icindeki collider'larin scratch tamponudur
   (SimpleBoxCollision onu doldurur). `g_terrain_tris` ise ARAZININ
   ONBELLEGIDIR ve cagrilar arasinda YASAR. Tek global dizi uzerinden
   calisildigi surece ikisi birbirini ezerdi: SimpleBoxCollision'in doldurdugu
   kume bir sonraki karede TerrainCollision'in onbellegini silerdi.
   resolve_pass hangi kumeyle calisacagini bu yuzden ARGUMANLA alir. */
static TriBuffer g_tris;          /* scratch: bir cagrinin collider'lari */
static TriBuffer g_terrain_tris;  /* ARAZI onbellegi (kalici)            */
static int       g_terrain_handle = -2;   /* -2 = henuz toplanmadi       */

/* Kapasiteyi en az `need` ucgen alacak sekilde buyutur. 0 = bellek yetmedi. */
static int tri_reserve(TriBuffer *b, int need) {
    ColTri *grown;
    int cap;
    if (need <= b->cap) return 1;
    cap = b->cap > 0 ? b->cap : COL_TRIS_INIT;
    while (cap < need) cap *= 2;
    grown = (ColTri *)realloc(b->tris, (size_t)cap * sizeof(ColTri));
    if (!grown) return 0;
    b->tris = grown;
    b->cap = cap;
    return 1;
}

/* ---------- izgara (yalnizca ARAZI icin) ---------- */

/* Bir hucreye ucgen indeksi ekler; kapasite gerekirse buyur. */
static int grid_add(GridCell *cell, int idx) {
    if (cell->count >= cell->cap) {
        int cap = cell->cap > 0 ? cell->cap * 2 : 4;
        int *grown = (int *)realloc(cell->items, (size_t)cap * sizeof(int));
        if (!grown) return 0;
        cell->items = grown;
        cell->cap = cap;
    }
    cell->items[cell->count++] = idx;
    return 1;
}

static void grid_free(TriGrid *g) {
    if (!g) return;
    if (g->cells) {
        for (int i = 0; i < g->nx * g->nz; i++) free(g->cells[i].items);
        free(g->cells);
    }
    memset(g, 0, sizeof(*g));
}

/* Arazi ucgenlerinden X/Z izgarasini kurar. Y kovalanmaz (bkz. TriGrid):
   ucgen, Y araligi ne olursa olsun X/Z'de dokundugu HER hucreeye yazilir.
   Boylece magara tavani da, taban da ayni sekilde bulunur. */
static void grid_build(TriGrid *g, const ColTri *tris, int count) {
    double min_x = 1.0e30, min_z = 1.0e30, max_x = -1.0e30, max_z = -1.0e30;
    double span_x, span_z, cell;
    int nx, nz, i;

    grid_free(g);
    if (!g || !tris || count <= 0) return;

    for (i = 0; i < count; i++) {
        if (tris[i].lo[0] < min_x) min_x = tris[i].lo[0];
        if (tris[i].hi[0] > max_x) max_x = tris[i].hi[0];
        if (tris[i].lo[2] < min_z) min_z = tris[i].lo[2];
        if (tris[i].hi[2] > max_z) max_z = tris[i].hi[2];
    }
    span_x = max_x - min_x;
    span_z = max_z - min_z;
    if (!(span_x > 0.0)) span_x = COL_GRID_CELL;
    if (!(span_z > 0.0)) span_z = COL_GRID_CELL;

    cell = COL_GRID_CELL;
    if (span_x / cell > (double)COL_GRID_MAX) {
        cell = span_x / (double)COL_GRID_MAX;
    }
    if (span_z / cell > (double)COL_GRID_MAX) {
        cell = span_z / (double)COL_GRID_MAX;
    }
    if (!(cell > 0.0)) cell = COL_GRID_CELL;

    nx = (int)(span_x / cell) + 1;
    nz = (int)(span_z / cell) + 1;
    if (nx < 1) nx = 1;
    if (nz < 1) nz = 1;
    if (nx > COL_GRID_MAX) nx = COL_GRID_MAX;
    if (nz > COL_GRID_MAX) nz = COL_GRID_MAX;

    g->cells = (GridCell *)calloc((size_t)nx * (size_t)nz, sizeof(GridCell));
    if (!g->cells) return;
    g->nx = nx; g->nz = nz;
    g->min_x = min_x; g->min_z = min_z;
    g->cell = cell;

    for (i = 0; i < count; i++) {
        int cx0 = (int)((tris[i].lo[0] - min_x) / cell);
        int cx1 = (int)((tris[i].hi[0] - min_x) / cell);
        int cz0 = (int)((tris[i].lo[2] - min_z) / cell);
        int cz1 = (int)((tris[i].hi[2] - min_z) / cell);

        if (cx0 < 0) cx0 = 0;
        if (cz0 < 0) cz0 = 0;
        if (cx1 > nx - 1) cx1 = nx - 1;
        if (cz1 > nz - 1) cz1 = nz - 1;
        if (cx1 < cx0) cx1 = cx0;
        if (cz1 < cz0) cz1 = cz0;

        for (int cz = cz0; cz <= cz1; cz++) {
            for (int cx = cx0; cx <= cx1; cx++) {
                grid_add(&g->cells[cz * nx + cx], i);
            }
        }
    }
    g->built = 1;
}

/* ---------- mesh modulunden ucgen erisimi ---------- */

typedef int (*GclTriCountFn)(double, double, double, double, double,
                             double, double, double, double, double);
typedef int (*GclTriFn)(double, double, double, double, double,
                        double, double, double, double, double, int, float *);

static void *mesh_symbol(const char *name) {
    return gcl_module_symbol("RaylibSimpleMesh.dll", name);
}

static GclTriCountFn tri_count_fn(void) {
    static GclTriCountFn fn = NULL; static int tried = 0;
    if (!tried) { tried = 1; fn = (GclTriCountFn)mesh_symbol("gcl_simplemesh_triangle_count"); }
    return fn;
}

static GclTriFn tri_fn(void) {
    static GclTriFn fn = NULL; static int tried = 0;
    if (!tried) { tried = 1; fn = (GclTriFn)mesh_symbol("gcl_simplemesh_triangle"); }
    return fn;
}

/* Bir mesh'in TUM ucgenlerini dunya uzayinda tampona ekler. `xf` dokuz
   donusum yuvasidir (px,py,pz, rx,ry,rz, sx,sy,sz). Donus: eklenen ucgen
   sayisi. Mesh modulu yuklu degilse ya da handle bilinmiyorsa 0. */
static int gather_mesh(TriBuffer *b, int handle, const double xf[9]) {
    GclTriCountFn count = tri_count_fn();
    GclTriFn      tri   = tri_fn();
    int total, i, added = 0;

    if (!b || !count || !tri || handle < 0) return 0;

    total = count((double)handle,
                  xf[0], xf[1], xf[2], xf[3], xf[4], xf[5], xf[6], xf[7], xf[8]);
    if (total <= 0) return 0;

    /* Tamponu simdiden buyut: eksik ucgen collision'in bir kisminin
       dusmesi demektir, sessizce kirpMAK yerine yer ac. */
    if (!tri_reserve(b, b->count + total)) return 0;

    for (i = 0; i < total; i++) {
        float w9[9];
        ColTri *t;

        if (!tri((double)handle,
                 xf[0], xf[1], xf[2], xf[3], xf[4], xf[5], xf[6], xf[7], xf[8],
                 i, w9)) continue;

        t = &b->tris[b->count];
        for (int k = 0; k < 9; k++) t->v[k] = (double)w9[k];

        for (int c = 0; c < 3; c++) {
            double a0 = t->v[c], a1 = t->v[3 + c], a2 = t->v[6 + c];
            double lo = a0, hi = a0;
            if (a1 < lo) lo = a1; else if (a1 > hi) hi = a1;
            if (a2 < lo) lo = a2; else if (a2 > hi) hi = a2;
            t->lo[c] = lo; t->hi[c] = hi;
        }

        b->count++;
        added++;
    }
    return added;
}

/* ---------- kapsul ---------- */

typedef struct {
    double a[3];   /* eksenin alt ucu (ayak + yaricap)   */
    double b[3];   /* eksenin ust ucu (bas  - yaricap)   */
    double r;      /* yaricap                            */
} ColCapsule;

static void player_capsule(ColCapsule *cap) {
    double feet = g_slot[S_POS_Y];
    double head = feet + g_slot[S_HEIGHT];

    cap->r = COL_PLAYER_RADIUS;
    v3(cap->a, g_slot[S_POS_X], feet + cap->r, g_slot[S_POS_Z]);
    v3(cap->b, g_slot[S_POS_X], head - cap->r, g_slot[S_POS_Z]);
    if (cap->b[1] < cap->a[1]) cap->b[1] = cap->a[1];   /* cok kisa govde */
}

/* ---------- kapsul vs ucgen: en kucuk cozulme ----------

   En kisa mesafe, (a) kapsul ekseni ile ucgenin UC KENARI ve (b) eksenin IKI
   UCU ile ucgen YUZU arasindaki mesafelerin en kucugudur. Bu, parca-ucgen
   mesafesinin tam kapsamidir: iki disbukey kume arasindaki en kisa mesafe ya
   kenar-kenar ya da kose-yuz ciftinde olusur.

   Ayrik ise 0 doner. Degilse `normal` KUTUDAN/OBCEDEN kapsule dogru birim
   vektordur ve `pen` ic ice gecme derinligidir.

   Bu test YONSUZDUR: hangi taraftan geldiginizi sormaz, yalnizca kapsulun
   ucgenle kesisip kesismedigine bakar. Bu yuzden magara duvarlari, tavanlar ve
   tabanlar AYNI sekilde cozulur — ust taraf icin ayri bir mantik YOKTUR. */
static int capsule_tri(const ColCapsule *cap,
                       const double v0[3], const double v1[3], const double v2[3],
                       double normal[3], double *pen) {
    double best = 1.0e30;
    double bp[3], bq[3];
    int i;

    for (i = 0; i < 3; i++) {
        const double *p, *q;
        double c1[3], c2[3], d;

        if (i == 0)      { p = v0; q = v1; }
        else if (i == 1) { p = v1; q = v2; }
        else             { p = v2; q = v0; }

        d = seg_seg_closest(cap->a, cap->b, p, q, c1, c2);
        if (d < best) { best = d; vcopy(bp, c1); vcopy(bq, c2); }
    }

    {
        double q[3], d;
        closest_pt_tri(cap->a, v0, v1, v2, q);
        d = vdist(cap->a, q);
        if (d < best) { best = d; vcopy(bp, cap->a); vcopy(bq, q); }

        closest_pt_tri(cap->b, v0, v1, v2, q);
        d = vdist(cap->b, q);
        if (d < best) { best = d; vcopy(bp, cap->b); vcopy(bq, q); }
    }

    if (best >= cap->r) return 0;

    if (best > 1.0e-9) {
        double d[3];
        vsub(d, bp, bq);
        vscale(normal, d, 1.0 / best);
    } else {
        /* Eksen ucgeni KESIYOR: yon yuzey normalinden gelir, isaret kapsulun
           ucgen duzleminin hangi tarafinda oldugundan. */
        double e1[3], e2[3], n[3], mid[3];
        double d;
        vsub(e1, v1, v0); vsub(e2, v2, v0);
        n[0] = e1[1] * e2[2] - e1[2] * e2[1];
        n[1] = e1[2] * e2[0] - e1[0] * e2[2];
        n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        if (vlen2(n) < 1.0e-18) { v3(normal, 0.0, 1.0, 0.0); *pen = cap->r; return 1; }
        vscale(n, n, 1.0 / vlen(n));
        mid[0] = (cap->a[0] + cap->b[0]) * 0.5;
        mid[1] = (cap->a[1] + cap->b[1]) * 0.5;
        mid[2] = (cap->a[2] + cap->b[2]) * 0.5;
        d = (mid[0] - v0[0]) * n[0] + (mid[1] - v0[1]) * n[1] + (mid[2] - v0[2]) * n[2];
        if (d < 0.0) vscale(n, n, -1.0);
        vcopy(normal, n);
    }

    *pen = cap->r - best;
    return 1;
}

/* Bir ucgeni kapsule karsi dener ve daha derin bir temas bulursa en iyiyi
   guncelleyip 1 doner. AABB testi ONCE yapilir: uzak ucgenler neredeyse
   bedava elenir, asil (pahali) kapsul-ucgen testi yalnizca yakinda calisir.
   Bu ayirma, izgaranin dogrulugu bozmadan yalnizca hiz kazandirdigi yerdir. */
static int tri_consider(const ColTri *t, const ColCapsule *cap,
                        const double lo[3], const double hi[3],
                        double best_n[3], double *best_pen) {
    double n[3], pen;

    if (t->hi[0] < lo[0] || t->lo[0] > hi[0]) return 0;
    if (t->hi[1] < lo[1] || t->lo[1] > hi[1]) return 0;
    if (t->hi[2] < lo[2] || t->lo[2] > hi[2]) return 0;

    if (!capsule_tri(cap, t->v, t->v + 3, t->v + 6, n, &pen)) return 0;
    if (pen > *best_pen) {
        *best_pen = pen;
        vcopy(best_n, n);
        return 1;
    }
    return 0;
}

/* ---------- ucgen kumesine karsi en derin temasi bul ve uygula ----------

   Bir gecis: TUM ucgenler taranir, EN DERIN temas secilir, kapsul o kadar
   itilir. Godot'un cozumu de iteratiftir; tek gecis yetmez cunku bir itme
   oyuncuyu ikinci bir yuzeye sokabilir. */
static int resolve_pass(const TriBuffer *buf, ColCapsule *cap, double out_normal[3]) {
    double best_pen = 0.0, best_n[3];
    int found = 0, i;

    /* Kapsul AABB'si: uzak ucgenleri elemek icin. */
    double lo[3], hi[3];
    for (i = 0; i < 3; i++) {
        double a = cap->a[i], b = cap->b[i];
        lo[i] = (a < b ? a : b) - cap->r;
        hi[i] = (a > b ? a : b) + cap->r;
    }

    if (!buf) return 0;

    v3(best_n, 0.0, 1.0, 0.0);

    /* IZGARA VARSA YALNIZCA KAPSULUN USTUNDEKI HUCRELER. Kapsulun X/Z AABB'si
       kac hucreye dokunuyorsa yalnizca onlar gezilir; 41.454 ucgenlik bir
       arazide bu, kare basina yuz binlerce test yerine birkac douzen demektir.
       En DERIN temas yine secilir: gezilen kumeler tum adaylari kapsar, yani
       sonuc tam taramanin AYNISIDIR — degisen yalnizca kaca bakildigidir.
       Izgara yoksa (scratch kume, ya da bellek yetmedi) tam tarama yapilir. */
    if (buf->grid.built) {
        const TriGrid *g = &buf->grid;
        int cx0 = (int)((lo[0] - g->min_x) / g->cell);
        int cx1 = (int)((hi[0] - g->min_x) / g->cell);
        int cz0 = (int)((lo[2] - g->min_z) / g->cell);
        int cz1 = (int)((hi[2] - g->min_z) / g->cell);

        if (cx0 < 0) cx0 = 0;
        if (cz0 < 0) cz0 = 0;
        if (cx1 > g->nx - 1) cx1 = g->nx - 1;
        if (cz1 > g->nz - 1) cz1 = g->nz - 1;

        for (int cz = cz0; cz <= cz1; cz++) {
            for (int cx = cx0; cx <= cx1; cx++) {
                const GridCell *cell = &g->cells[cz * g->nx + cx];
                for (int k = 0; k < cell->count; k++) {
                    int idx = cell->items[k];
                    if (idx < 0 || idx >= buf->count) continue;
                    if (tri_consider(&buf->tris[idx], cap, lo, hi,
                                     best_n, &best_pen)) {
                        found = 1;
                    }
                }
            }
        }
    } else {
        for (i = 0; i < buf->count; i++) {
            if (tri_consider(&buf->tris[i], cap, lo, hi, best_n, &best_pen)) {
                found = 1;
            }
        }
    }

    if (!found) return 0;

    /* ZEMIN / DUVAR AYRIMI. Yurunebilir bir yuzeyde (normal yeterince YUKARI)
       itme YALNIZCA DIKEYDIR. Normali oldugu gibi uygulamak egik yuzeylerde
       oyuncuyu yukari VE GERI tasir; geri bilesen ilerlemeyi yedigi icin
       hafif egimler tirmanilamaz hale gelir. Dik yuzeyde ise itme normal
       boyunca, yani YATAY kalir: duvara tirmanilmaz, boyunca KAYILIR. */
    if (best_n[1] > COL_COS_SLOPE) {
        best_n[0] = 0.0;
        best_n[2] = 0.0;
        best_n[1] = 1.0;
    }

    g_slot[S_POS_X] += best_n[0] * best_pen;
    g_slot[S_POS_Y] += best_n[1] * best_pen;
    g_slot[S_POS_Z] += best_n[2] * best_pen;
    player_capsule(cap);

    if (out_normal) vcopy(out_normal, best_n);
    return 1;
}

/* ---------- okuma / kamera ---------- */

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

static void read_player(int argc, const char **argv) {
    for (int i = 0; i < COL_PLAYER_SLOTS; i++) g_slot[i] = num_arg(argc, argv, i);
    if (!(g_slot[S_HEIGHT] > 0.0))   g_slot[S_HEIGHT]  = 1.8;
    if (!(g_slot[S_GRAVITY] > 0.0))  g_slot[S_GRAVITY] = 9.8;
    if (!(g_slot[S_CAM_FOVY] > 0.0)) g_slot[S_CAM_FOVY] = 60.0;
}

static void build_camera(void) {
    float yaw   = (float)(g_slot[S_ROT_X] * COL_DEG2RAD);
    float pitch = (float)(g_slot[S_ROT_Y] * COL_DEG2RAD);
    float cp    = cosf(pitch);
    float ex = (float)g_slot[S_POS_X];
    float ey = (float)(g_slot[S_POS_Y] + g_slot[S_HEIGHT]);
    float ez = (float)g_slot[S_POS_Z];
    float tx = (float)g_slot[S_LOOK_X];
    float ty = (float)g_slot[S_LOOK_Y];
    float tz = (float)g_slot[S_LOOK_Z];

    if (tx == 0.0f && ty == 0.0f && tz == 0.0f) {
        tx = ex + sinf(yaw) * cp * 10.0f;
        ty = ey + sinf(pitch) * 10.0f;
        tz = ez + cosf(yaw) * cp * 10.0f;
    }
    g_slot[S_CAM_POS_X] = ex; g_slot[S_CAM_POS_Y] = ey; g_slot[S_CAM_POS_Z] = ez;
    g_slot[S_CAM_TGT_X] = tx; g_slot[S_CAM_TGT_Y] = ty; g_slot[S_CAM_TGT_Z] = tz;
    g_slot[S_CAM_UP_X] = 0.0; g_slot[S_CAM_UP_Y] = 1.0; g_slot[S_CAM_UP_Z] = 0.0;
    g_slot[S_CAM_PROJECTION] = 0;   /* CAMERA_PERSPECTIVE */
}

/* ---------- TerrainCollision(player, terrain) ----------

   Arazi de bir UCGEN KUMESIDIR; kapsul ona ayni sekilde carpistirilir.
   Donusum yuvasi yoktur (arazi dosyanin koydugu yerde durur), bu yuzden
   kimlik donusum gonderilir.

   YALNIZCA SEKIL COZUMU. Arazi bir yukseklik alani SAYILMAZ: (x, z) altinda
   "en yuksek yuzey" diye bir soru SORULMAZ, cunku magarali bir arazide bu
   sorunun cevabi magaranin tavanidir ve oyuncuyu yukari isinlardi. */
/* ARAZI ONBELLEGI: arazi HER KAREDE aynidir, hareket etmez ve sekli degismez.
   Onbellek olmadan bu fonksiyon her karede butun modeli moduller-arası
   cagriyla YENIDEN topluyordu — fps_first_demo'da 41.454 ucgen, kare basina.
   Ucgenler bir kez cikarilip tutulur; sonraki kareler yalnizca kapsulu
   cozer. Ayni handle degismedigi surece yeniden toplanmaz; handle degisirse
   (baska bir arazi yuklendiyse) onbellek tazelenir. */
static void terrain_buffer_ensure(int handle) {
    if (g_terrain_handle == handle) return;   /* onbellek gecerli */
    g_terrain_tris.count = 0;
    gather_mesh(&g_terrain_tris, handle, (const double[9]){ 0,0,0, 0,0,0, 1,1,1 });

    /* Izgara, ucgenler DEGISTIGINDE kurulur ve sonra dokunulmaz. Arazi
       hareket etmedigi icin bu bir kerelik bir maliyettir; kurulduktan sonra
       her kare yalnizca kapsulun ustundeki hucreler gezilir. Toplama
       basarisiz olursa (handle -1, mesh yok) izgara kurulmaz ve resolve_pass
       tam taramaya duserdi — zaten 0 ucgen vardir, yani bedava. */
    grid_build(&g_terrain_tris.grid, g_terrain_tris.tris, g_terrain_tris.count);
    g_terrain_handle = handle;
}

static double fn_terrain_collision(int argc, const char **argv) {
    ColCapsule cap;
    double feet_now, feet_prev, step_y;
    int handle, steps;

    read_player(argc, argv);
    handle = (argc > COL_PLAYER_SLOTS + T_HANDLE)
           ? (int)num_arg(argc, argv, COL_PLAYER_SLOTS + T_HANDLE) : -1;

    terrain_buffer_ensure(handle);

    /* ---------- SUPURMELI (SWEPT) DIKEY COZUM ----------

       FPS modulu dikey hareketi ZATEN uygulamis olarak gelir (bkz.
       gcl_raylib_fps.c: y += vel*dt). Bu hareket tek karede yaricaptan
       buyukse kapsul yuzeyi iskalar ve ICINDEN gecer. Bu yuzden bu kareki
       dikey FARK, yaricaptan buyuk olmayan adimlara bolunur ve HER adimda
       cozulur.

       Cozumun yaptigi itme bir sonraki adima TASINIR — konum dogrudan
       yuvalarda tutuldugu icin — boylece kapsul yuzeyin altina itemez.
       Yukari hareket de (ziplama) ayni yoldan gecer; tavan varsa kapsul ona
       carpip durur.

       Ilk karede onceki konum yoktur; supurme yerine tek cozum yapilir, cunku
       oyuncu zaten yuzeyin ustunde baslar. */
    feet_now  = g_slot[S_POS_Y];
    feet_prev = g_last_feet_y;

    if (feet_prev > -1.0e29) {
        double dy  = feet_now - feet_prev;
        double ady = (dy < 0.0) ? -dy : dy;

        steps = (int)(ady / (double)COL_PLAYER_RADIUS) + 1;
        if (steps < 1) steps = 1;
        if (steps > COL_MAX_SWEEP_STEPS) steps = COL_MAX_SWEEP_STEPS;

        /* Onceki konumdan basla ve adim adim ilerle. */
        g_slot[S_POS_Y] = feet_prev;
        step_y = dy / (double)steps;

        for (int s = 0; s < steps; s++) {
            g_slot[S_POS_Y] += step_y;
            player_capsule(&cap);
            for (int i = 0; i < COL_SOLVE_ITERS; i++) {
                if (!resolve_pass(&g_terrain_tris, &cap, NULL)) break;
            }
        }
    } else {
        player_capsule(&cap);
        for (int i = 0; i < COL_SOLVE_ITERS; i++) {
            if (!resolve_pass(&g_terrain_tris, &cap, NULL)) break;
        }
    }

    g_last_feet_y = g_slot[S_POS_Y];

    build_camera();
    return 0.0;
}

/* ---------- SimpleBoxCollision(player, MESH...) ----------

   OYUNCU KAPSULU, verilen MESH'LERIN UCGENLERINE karsi cozulur. Ucgenler
   gcl_simplemesh_triangle'dan DUNYA uzayinda, Position/Rotate/Scale
   uygulanmis olarak gelir; dondurulmus bir kutu (or. Rotate.y = 45) bu yuzden
   DOGRU temsil edilir. Raylib'in CheckCollisionBoxes'i yalnizca eksen-hizali
   kutularla calisir ve dondurulmus bir kutuyu yanlis temsil ederdi; bu yuzden
   burada kullanilmaz.

   DUZLESMIS ARGUMAN DUZENI: 27 oyuncu yuvasi | ardindan HER collider icin
   12 yuva (Mesh struct'i). Collider sayisi degiskendir.

   HAREKET ARGUMANI YOKTUR: bu bir COZULME cagrisidir, hareket ettirme degil.
   Oyuncunun konumunu FPS ve TerrainCollision ilerletir; burada yalnizca
   kutunun icinde kalmis bir kapsul DISARI itilir. */
static double fn_simple_box_collision(int argc, const char **argv) {
    ColCapsule cap;
    int base = COL_PLAYER_SLOTS, k;

    if (argc < COL_PLAYER_SLOTS + COL_BOX_SLOTS) return 0.0;

    read_player(argc, argv);

    /* Collider'lari topla: her 12'lik blok bir Mesh struct'idir. Bunlar
       HAREKET EDEBILIR (tasinmis bir kutu), bu yuzden her cagride yeniden
       toplanirlar; onbellege ALINMAZLAR. */
    g_tris.count = 0;
    {
        int n = 0;
        while (n < COL_MAX_COLLIDERS && base + COL_BOX_SLOTS <= argc) {
            double mesh[COL_BOX_SLOTS];
            for (k = 0; k < COL_BOX_SLOTS; k++) mesh[k] = num_arg(argc, argv, base + k);
            base += COL_BOX_SLOTS;
            gather_mesh(&g_tris, (int)mesh[B_HANDLE],
                        (const double[9]){ mesh[B_POS_X], mesh[B_POS_Y], mesh[B_POS_Z],
                                           mesh[B_ROT_X], mesh[B_ROT_Y], mesh[B_ROT_Z],
                                           mesh[B_SCALE_X], mesh[B_SCALE_Y], mesh[B_SCALE_Z] });
            n++;
        }
    }

    player_capsule(&cap);

    /* COZULME PASS'LERI: bir itme kapsulu ikinci bir yuzeye sokabilir, bu
       yuzden tek gecis yetmez (Godot da iteratiftir). */
    for (int i = 0; i < COL_SOLVE_ITERS; i++) {
        if (!resolve_pass(&g_tris, &cap, NULL)) break;
    }

    build_camera();
    return 0.0;
}

/* LastSlot(): argumansiz -> yuva sayisi; LastSlot(i) -> i. yuva. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)COL_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= COL_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(terrain_collision,    "TerrainCollision"),
    E(simple_box_collision, "SimpleBoxCollision"),
    E(last_slot,            "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplecollision_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
