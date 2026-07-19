/* Memory: arena + pool + freelist + page allocator */
#include "gcl.h"
#include "memory.h"
#include "error.h"

/* Pool (<=64 bayt) */
#define POOL_BLOCK 64
#define POOL_COUNT 4096
static char g_pool[POOL_COUNT][POOL_BLOCK];
static uint32_t g_pool_bits[POOL_COUNT/32+1];

static void *pool_alloc(void) {
    for (int i = 0; i < POOL_COUNT; i++) {
        int w = i/32, b = i%32;
        if (!(g_pool_bits[w] & (1u<<b))) {
            g_pool_bits[w] |= (1u<<b);
            return g_pool[i];
        }
    }
    return 0;
}
static void pool_free(void *p) {
    ptrdiff_t d = (char*)p - (char*)g_pool;
    if (d < 0 || d >= (ptrdiff_t)sizeof(g_pool) || d%POOL_BLOCK) return;
    int i = (int)(d/POOL_BLOCK);
    g_pool_bits[i/32] &= ~(1u<<(i%32));
}

/* Arena (gecici tahsis) */
#define ARENA_SZ (1024*1024)
static Arena *g_arena;

void *arena_alloc(size_t sz) {
    sz = (sz+15)&~15;
    if (!g_arena || g_arena->ptr + sz > g_arena->end) {
        size_t cap = sz < ARENA_SZ ? ARENA_SZ : sz;
        Arena *a = malloc(sizeof(Arena)+cap);
        if (!a) return 0;
        a->ptr = (char*)(a+1); a->end = a->ptr+cap;
        a->next = g_arena; g_arena = a;
    }
    void *p = g_arena->ptr; g_arena->ptr += sz; return p;
}
void arena_reset(void) {
    for (Arena *a = g_arena; a; a = a->next) a->ptr = (char*)(a+1);
}

/* Free list (64B-64KB) */
typedef struct FBlk { size_t sz; struct FBlk *next; } FBlk;
#define FHEAP_SZ (1024*1024)
static char g_fheap[FHEAP_SZ];
static FBlk *g_flist;

static void flist_init(void) {
    g_flist = (FBlk*)g_fheap;
    g_flist->sz = FHEAP_SZ - sizeof(FBlk);
    g_flist->next = 0;
}
static void *flist_alloc(size_t sz) {
    sz = (sz+7)&~7; FBlk **pp = &g_flist;
    while (*pp) {
        if ((*pp)->sz >= sz) {
            size_t rem = (*pp)->sz - sz;
            if (rem > sizeof(FBlk)+16) {
                FBlk *n = (FBlk*)((char*)*pp + sizeof(FBlk) + sz);
                n->sz = rem - sizeof(FBlk); n->next = (*pp)->next;
                (*pp)->sz = sz;
            }
            FBlk *b = *pp; *pp = b->next;
            memset(b,0,sizeof(FBlk)); return b;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

/* Page (>64KB) */
static void *page_alloc(size_t sz) { return malloc(sz); }

/* Public API */
void mem_init(void) {
    memset(g_pool_bits,0,sizeof(g_pool_bits));
    flist_init(); g_arena = 0;
}
void mem_shutdown(void) {
    while (g_arena) { Arena *n = g_arena->next; free(g_arena); g_arena = n; }
}

void *area(size_t sz) {
    void *p = 0;
    int kind = 2; /* 0=pool,1=freelist,2=page */
    if (sz <= POOL_BLOCK) { p = pool_alloc(); kind = 0; }
    else if (sz <= 64*1024) { p = flist_alloc(sz); kind = 1; }
    else { p = page_alloc(sz); kind = 2; }
    if (!p) return 0;
    /* tag'i sifirlamadan once yaz, sonra sifirla (tag son byte'da) */
    if (sz >= 16) {
        unsigned char *t = (unsigned char*)p + sz - 1;
        *t = (unsigned char)kind;
        t[-1] = 0xA5;
    }
    memset(p, 0, sz - (sz>=16 ? 2 : 0));
    return p;
}

void area_free(void *p) {
    if (!p) return;
    /* pool mu diye kontrol */
    ptrdiff_t d = (char*)p - (char*)g_pool;
    if (d >= 0 && d < (ptrdiff_t)sizeof(g_pool) && d%POOL_BLOCK==0) {
        pool_free(p); return;
    }
    /* freelist mi? (fheap icinde mi?) */
    d = (char*)p - (char*)g_fheap;
    if (d >= 0 && d < (ptrdiff_t)sizeof(g_fheap)) { return; } /* freelist: no-op */
    /* page (malloc) */
    free(p);
}
