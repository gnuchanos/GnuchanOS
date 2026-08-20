# gcl-lsp — GCL Odaklı Tasarım Dokümanı

## 1. Amaç

GnuChanIDE içindeki otomatik tamamlamanın **GCL dili (`.gcsf` / `.gclib`)** için sağlam ve öngörülebilir çalışmasını sağlamak. Python/Lua ile aynı LSP çekirdeği (gcl-lsp.exe) kullanılır; fark GCL'nin modül sisteminde ve üye modelinde:

- `#include "script.gcsf"` → o dosyanın sembolleri
- `#lib "kutuphane.gclib"` → kütüphane public sembolleri
- `#extern "raylib.dll"` + `#register` → dış API fonksiyonları
- `yapi.` → struct/class üyeleri (`.name`, `.Call`, `.age`)
- `class CHILD(FATHER)` → miras; FATHER üyeleri CHILD'da da önerilir
- `public type ad = değer;` → global
- `void ad(...)` → fonksiyon
- düz yazımda → GCL keywordleri + tipler + printf

**Kritik kural: `#include/#lib` dosya eşlemeleri asla hard-coded değildir; diskten dinamik taranır, IDE açıkken eklenen dosyalar didChange ile otomatik algılanır.**

## 2. Mevcut Durum Analizi (GCL Tarafı)

| Katman | Durum |
|---|---|
| LSP çekirdek | C, NDJSON over stdio — aynı `gcl-lsp.exe` |
| Workspace index | Yalnızca `.py` toplanıyor (`collect_python`); `.gcsf/.gclib` taraması gerekiyor |
| Dil tablosu | `python_syntax.c` var; `gcl_syntax.c` (GCL keyword + tip + printf) mevcut |
| Import çözümleme | Python (`import/from`) hazır; GCL (`#include/#lib/#extern`) için ayrı çözücü gerekli |
| Üye çözümleme | `.` member altyapısı hazır; struct/class üyeleri için index eklenmeli |
| didChange | Çalışıyor (`find` uzantısına `.gcsf/.gclib` eklenmeli) |
| Fridge | Python `-pyrun`, Lua `-luarun`; GCL `-run -resolve` paraleli isteğe bağlı |

**Güçlü yönler:** Tek çekirdek, geçerli JSON, didChange canlı tarama, paket-bilinçli çözümleme, hata sızdırmazlık — Python/Lua'dan miras alınır.

**Gerekli eklemeler:**
1. `collect_gcl` — `.gcsf` ve `.gclib` dosyalarını da topla (src/ + root).
2. `index_gcl_line` — GCL sözdizimi:
   - `#include "name"` → modül import listesine
   - `#lib "name"` → kütüphane import listesine
   - `#extern "name"` + `#register T ad(...)` → dış API sembolleri
   - `public T ad(...)` / `T ad(...)` → fonksiyon
   - `public T ad = ...` / `T ad = ...` → global/const
   - `struct AD { ... }` / `class AD() { void m(){} }` → tip + üyeler
   - `T *ad = ...` → pointer/global
3. `gcl_syntax.c` — GCL keywordleri (`public private const inline global typedef sizeof if else elif while do for switch case break continue return class struct enum tuple dict`), tipler (`int8..int128`, `float16..float128`, `uint8..uint128`, `gcChar`, `bool`, `short int float double long`), `printf/scanf` vb.
4. `parse_preproc` — `#include/#lib/#extern` içindeki dosya adı önerisi.
5. Member çözümleme — `yapi.` → struct/class üyeleri + miras (CHILD(FATHER)).

## 3. Tasarım İlkeleri

1. **Her şey diskten.** Sembol, modül, tipler — hiçbiri elle yazılmaz.
2. **İki modül kaynağı:** `#include` (script) ve `#lib` (kütüphane); ikisi de diske çözülür; `#extern` (dll) ve `#register` yalnızca register satırlarından okunur.
3. **Görünürlük:** `public` her yerde önerilir; `private` yalnızca aynı klasördeki dosyalara.
4. **Öngörülebilir öncelik.** Canlı yerel değişkenler → include/lib bağlamı → açık dosyanın own sembolleri → workspace modülleri → GCL dil tablosu.
5. **Asla boş kalma.** LSP çökse/yoksa renderer yerel kelime havuzuna düşer.

## 4. Mimari

```
IDE (react) ──IPC──► Electron main.ts ──stdio NDJSON──► gcl-lsp.exe
                              ▲                                │
                              └────────── yanıtlar ◄───────────┘
```

- Proses: tek `gcl-lsp.exe`; istekler sıralı işlenir.
- State: `Workspace { root, files[], syms[] }` — her file `lang: "gcl" | "lua" | "py"` etiketi taşır.
- Protokol: initialize / didChange / completion / shutdown (Python ile aynı).

## 5. Modül ve Üye Çözümleme Kuralları (Net Tanım)

Modül = `.gcsf`/`.gclib` dosyası (uzantısız adı). Kütüphane = `#lib` ile dahil edilen dosya.

### 5.1 `#include "X"` / `#include <X>`
- X = workspace'teki `.gcsf` script'i → sembolleri önerilir.
- `#include "` yazarken workspace script adları + paket klasör adları.

### 5.2 `#lib "X"` / `#lib <X>`
- X = `.gclib` kütüphanesi → **public** sembolleri önerilir.
- `#lib "` yazarken workspace kütüphane adları.

### 5.3 `#extern "dll"` + `#register`
- `#register void InitWindow(int w,int h,const char *t);` → `InitWindow` dış API sembolü.
- `Init` yazarken register edilmiş tüm dış fonksiyonlar önerilir.

### 5.4 `yapi.` (member)
- `struct Player` / `class AD()` üyeleri önerilir: `.name`, `.health`, `.Call`.
- `class CHILD(FATHER)` mirası: FATHER üyeleri CHILD üyelerine dahil.

### 5.5 `public` / `private`
- `public`: her dosyadan önerilir.
- `private`: yalnızca aynı klasördeki dosyalara.

### 5.6 Yerel/global bildirimler
- `T ad = val;` → global const.
- `void ad(...)` → global fn.
- `T *ad = ...` → pointer global.

## 6. Tamamlama Kategorileri ve Öncelik Sırası

1. **Canlı yerel değişkenler** — `T ad = ...` (imlece kadar).
2. **Modül bağlamı**
   - `#include "X` → X ile başlayan script adları
   - `#lib "X` → kütüphane adları
   - dll içindeki register sembolleri
3. **Açık dosyanın own sembolleri** — fonksiyonlar, global const, struct/class tipleri.
4. **Workspace modül adları** — yalnızca #include/#lib yazarken.
5. **GCL dil tablosu** — keywordler + tipler + printf/scanf (`gcl_syntax.c`).
6. **Fridge** — `-run -resolve` ile dış API doğrulaması (isteğe bağlı).

## 7. Doğrulama Senaryoları (Test Planı)

1. `#include "testFolder/` → helloworld.gcsf.
2. `#include "helloworld.gcsf"` sonrası `zem` → `zemberek()`.
3. `struct Player p; p.` → `.name`, `.health`, `.speed`.
4. `class CHILD(FATHER)` sonrası `ThisChild.` → FATHER'ın `.Call` + CHILD `.age`.
5. `#extern "raylib.dll"` + `#register void InitWindow` sonrası `InitW` → `InitWindow`.
6. `#lib "math"` sonrası public fn `sq` → `sqrt`.
7. `pri` → `printf` (GCL globals).
8. `import final_fantasy` benzeri: `#lib "final_fantasy"` → seven.gclib üyeleri.
9. Boş prefix Ctrl+Space → asla boş değil.

## 8. Uygulama Adımları (Sırası)

1. `collect_gcl` (.gcsf/.gclib taraması) + `FileIndex.lang`.
2. `index_gcl_line` (5.1–5.6 kural seti).
3. `gcl_syntax.c` (keyword + tip + printf tablosu).
4. `parse_preproc` + `#include/#lib/#extern` çözümleme (5.1–5.3).
5. Struct/class üye index'i + miras (5.4).
6. didChange uzantı filtresine `.gcsf/.gclib` ekle.
7. Derle + test senaryoları + dağıt.
