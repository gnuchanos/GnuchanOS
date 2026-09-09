/*
 * gcl_shared_state.h — GCL cross-process shared state store (real-time).
 *
 * simple_doc.md: "Gerçek zamanlı değişken paylaşımı (loop içinde kullanılabilir)"
 *
 * Memory-mapped paylaşımlı bellek köprüsü: GCL (Embed.SendValue/GetValue)
 * ile Python/Lua script'leri (gcl.set_state/get_state) arasında gerçek
 * zamanlı veri alışverişi. Alt süreçler (gcl -luarun / -pyrun) aynı
 * GCL_SHARED_FILE dosyasını map'ler.
 *
 * Önceki dosya-tabanlı sürüm her get/set'te tam dosya oku-yaz-rename
 * yapıyordu (disk I/O = lag) ve iki süreç full-snapshot üzerinden yarışıp
 * eski değeri geri yazabiliyordu (top teleport). Bu sürümde get/set
 * yalnızca bir bellek erişimi + çok kısa spinlock'tur.
 *
 * Bağımlılık: stdio, stdlib, string, stdint. Windows / POSIX.
 */

#ifndef GCL_SHARED_STATE_H
#define GCL_SHARED_STATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#define GCL_SHM_MAX_KEYS 64
#define GCL_SHM_KEY_LEN 64
#define GCL_SHM_VAL_LEN 128
#define GCL_SHM_MAGIC 0x47434C53u  /* "GCLS" */

typedef struct {
    char key[GCL_SHM_KEY_LEN];
    char val[GCL_SHM_VAL_LEN];
    int in_use;
} GclShmEntry;

typedef struct {
    volatile int lock;           /* 0=serbest, 1=kilitli (spinlock) */
    unsigned int magic;
    unsigned int seq;
    GclShmEntry entries[GCL_SHM_MAX_KEYS];
} GclShmState;

static char g_gcl_shared_path[4096] = "";
static int g_gcl_shared_ready = 0;
static GclShmState *g_gcl_shm = NULL;

#ifdef _WIN32
static HANDLE g_gcl_shm_file = NULL;
static HANDLE g_gcl_shm_map = NULL;
#else
static int g_gcl_shm_fd = -1;
#endif

static void gcl_shared_resolve_path(void) {
    if (g_gcl_shared_path[0]) return;
    const char *env = getenv("GCL_SHARED_FILE");
    if (env && env[0]) {
        snprintf(g_gcl_shared_path, sizeof(g_gcl_shared_path), "%s", env);
    } else {
#ifdef _WIN32
        const char *tmp = getenv("TEMP");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = ".";
        snprintf(g_gcl_shared_path, sizeof(g_gcl_shared_path), "%s\\gcl_shared.state", tmp);
#else
        snprintf(g_gcl_shared_path, sizeof(g_gcl_shared_path), "/tmp/gcl_shared.state");
#endif
    }
}

static void gcl_shared_init(void) {
    if (g_gcl_shared_ready) return;
    gcl_shared_resolve_path();

#ifdef _WIN32
    g_gcl_shm_file = CreateFileA(g_gcl_shared_path,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_gcl_shm_file == INVALID_HANDLE_VALUE) { g_gcl_shared_ready = 1; return; }
    /* Backing dosyayı gerçekten sizeof(GclShmState) boyutuna getir, yoksa
       mapping 0 uzunlukta olur ve hiçbir okuma/yazma çalışmaz. */
    DWORD need = (DWORD)sizeof(GclShmState);
    DWORD cur = GetFileSize(g_gcl_shm_file, NULL);
    if (cur == INVALID_FILE_SIZE || cur < need) {
        SetFilePointer(g_gcl_shm_file, need, NULL, FILE_BEGIN);
        SetEndOfFile(g_gcl_shm_file);
    }
    g_gcl_shm_map = CreateFileMappingA(g_gcl_shm_file, NULL, PAGE_READWRITE, 0, need, NULL);
    if (!g_gcl_shm_map) { CloseHandle(g_gcl_shm_file); g_gcl_shm_file = NULL; g_gcl_shared_ready = 1; return; }
    g_gcl_shm = (GclShmState *)MapViewOfFile(g_gcl_shm_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!g_gcl_shm) { CloseHandle(g_gcl_shm_map); g_gcl_shm_map = NULL; CloseHandle(g_gcl_shm_file); g_gcl_shm_file = NULL; g_gcl_shared_ready = 1; return; }
#else
    g_gcl_shm_fd = open(g_gcl_shared_path, O_RDWR | O_CREAT, 0644);
    if (g_gcl_shm_fd < 0) { g_gcl_shared_ready = 1; return; }
    if (ftruncate(g_gcl_shm_fd, (off_t)sizeof(GclShmState)) != 0) { close(g_gcl_shm_fd); g_gcl_shm_fd = -1; g_gcl_shared_ready = 1; return; }
    void *p = mmap(NULL, sizeof(GclShmState), PROT_READ | PROT_WRITE, MAP_SHARED, g_gcl_shm_fd, 0);
    if (p == MAP_FAILED) { close(g_gcl_shm_fd); g_gcl_shm_fd = -1; g_gcl_shared_ready = 1; return; }
    g_gcl_shm = (GclShmState *)p;
#endif

    if (g_gcl_shm->magic != GCL_SHM_MAGIC) {
        memset(g_gcl_shm, 0, sizeof(*g_gcl_shm));
        g_gcl_shm->magic = GCL_SHM_MAGIC;
        g_gcl_shm->seq = 1;
        for (int i = 0; i < GCL_SHM_MAX_KEYS; i++) g_gcl_shm->entries[i].in_use = 0;
    }
    g_gcl_shared_ready = 1;
}

static void gcl_shared_lock(void) {
#ifdef _WIN32
    while (InterlockedExchange((volatile LONG *)&g_gcl_shm->lock, 1)) {
        /* spin — kritik bölge çok küçük (birkaç memcpy) */
    }
#else
    while (__sync_lock_test_and_set(&g_gcl_shm->lock, 1)) {
        /* spin — kritik bölge çok küçük (birkaç memcpy) */
    }
#endif
}

static void gcl_shared_unlock(void) {
#ifdef _WIN32
    InterlockedExchange((volatile LONG *)&g_gcl_shm->lock, 0);
#else
    __sync_lock_release(&g_gcl_shm->lock);
#endif
}

static int gcl_shared_set(const char *key, const char *val) {
    gcl_shared_init();
    if (!g_gcl_shm || !key) return 0;
    gcl_shared_lock();
    int slot = -1, free_slot = -1;
    for (int i = 0; i < GCL_SHM_MAX_KEYS; i++) {
        GclShmEntry *e = &g_gcl_shm->entries[i];
        if (e->in_use) {
            if (strcmp(e->key, key) == 0) { slot = i; break; }
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }
    if (slot < 0) slot = free_slot;
    if (slot < 0) { gcl_shared_unlock(); return 0; }
    GclShmEntry *e = &g_gcl_shm->entries[slot];
    if (!e->in_use) {
        memcpy(e->key, key, GCL_SHM_KEY_LEN - 1);
        e->key[GCL_SHM_KEY_LEN - 1] = '\0';
    }
    e->val[0] = '\0';
    strncpy(e->val, val ? val : "", GCL_SHM_VAL_LEN - 1);
    e->val[GCL_SHM_VAL_LEN - 1] = '\0';
    e->in_use = 1;
    g_gcl_shm->seq++;
    gcl_shared_unlock();
    return 1;
}

static int gcl_shared_get(const char *key, char *out, size_t out_size) {
    gcl_shared_init();
    if (!g_gcl_shm || !key || !out || out_size == 0) return 0;
    gcl_shared_lock();
    int found = 0;
    for (int i = 0; i < GCL_SHM_MAX_KEYS; i++) {
        GclShmEntry *e = &g_gcl_shm->entries[i];
        if (e->in_use && strcmp(e->key, key) == 0) {
            size_t n = strlen(e->val);
            if (n >= out_size) n = out_size - 1;
            memcpy(out, e->val, n);
            out[n] = '\0';
            found = 1;
            break;
        }
    }
    gcl_shared_unlock();
    return found;
}

static void gcl_shared_cleanup(void) {
    if (!g_gcl_shared_ready) return;
#ifdef _WIN32
    if (g_gcl_shm) { UnmapViewOfFile(g_gcl_shm); g_gcl_shm = NULL; }
    if (g_gcl_shm_map) { CloseHandle(g_gcl_shm_map); g_gcl_shm_map = NULL; }
    if (g_gcl_shm_file) { CloseHandle(g_gcl_shm_file); g_gcl_shm_file = NULL; }
#else
    if (g_gcl_shm) { munmap(g_gcl_shm, sizeof(GclShmState)); g_gcl_shm = NULL; }
    if (g_gcl_shm_fd >= 0) { close(g_gcl_shm_fd); g_gcl_shm_fd = -1; }
#endif
    g_gcl_shared_ready = 0;
    g_gcl_shared_path[0] = '\0';
}

#endif /* GCL_SHARED_STATE_H */
