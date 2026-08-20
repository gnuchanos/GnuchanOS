# gcl-lsp — Lua Odaklı Tasarım Dokümanı

## 1. Amaç

GnuChanIDE içindeki otomatik tamamlamanın **Lua için sağlam ve öngörülebilir** çalışmasını sağlamak. Python tarafıyla aynı LSP çekirdeği (gcl-lsp.exe) kullanılır; fark yalnızca dil modelinde:

- `require("mod")` → modül adı önerilir
- `local m = require("mod")` → `m.` → mod.lua üyeleri
- `require("pasta/")` → klasör içindeki .lua dosyaları
- `m.` → modülün return tablo üyeleri (fn/const)
- `local x = 5` → yerel değişken önerisi
- düz yazımda → o dosyadaki semboller + Lua keywords/globals

**Kritik kural: modül → dosya eşlemeleri asla hard-coded değildir; diskten dinamik taranır, IDE açıkken eklenen dosyalar didChange ile otomatik algılanır.**

## 2. Mevcut Durum Analizi (Lua Tarafı)

| Katman | Durum |
|---|---|
| LSP çekirdek | C, NDJSON over stdio — aynı `gcl-lsp.exe` |
| Workspace index | Şu an yalnızca `.py` toplanıyor (`collect_python`); `.lua` desteği gerekiyor |
| Dil tablosu | `python_syntax.c` var; `lua_syntax.c` (keywords + globals) mevcut |
| Import çözümleme | Python (`import/from`) için hazır; Lua (`require`) için ayrı çözücü gerekli |
| didChange | Çalışıyor (yeni .py gözükür); `.lua` uzantısı da eklenmeli |
| Fridge | `gcl -pyrun -resolve` Python için; Lua için `-luarun -resolve` paraleli |

**Güçlü yönler:** Tek çekirdek, geçerli JSON, didChange canlı tarama, paket-bilinçli çözümleme, hata sızdırmazlık — hepsi Python'dan miras alınır.

**Gerekli eklemeler:**
1. `collect_lua` — workspace'te `.lua` dosyalarını da topla (src/ + root + Library/Lua).
2. `index_lua_line` — Lua sözdizimi:
   - `require("mod")` → mod import listesine
   - `local m = require("mod")` → alias `m` = mod
   - `return { ... }` → tablo anahtarları üye olarak (fn/const)
   - `function ad(...)` → üst seviye fn
   - `local ad = ...` → const/local
   - `mod.fn = function() end` → üye fonksiyon
3. `parse_require` — `require("pasta/`  içinde klasör segment önerisi.
4. `lua_syntax.c` — Lua 5.4 keyword + global tablosu.
5. `fridge_query` Lua bayrağı — `gcl -luarun -resolve`.

## 3. Tasarım İlkeleri

1. **Her şey diskten.** Sembol, modül, klasör — hiçbiri elle yazılmaz.
2. **Tek doğruluk kaynağı: dosya yolu.** `require("pasta/util")` → açık dosyanın dizininden, sonra workspace'te göreli yol ile çözülür.
3. **Öngörülebilir öncelik.** Canlı yerel değişkenler → require bağlamı → açık dosyanın own sembolleri → workspace modülleri → Lua dil tablosu.
4. **Asla boş kalma.** LSP çökse/yoksa renderer yerel kelime havuzuna düşer.
5. **Bellek deterministik.** didChange disk gerçeğini yansıtır; cache yalnızca fridge içindir.

## 4. Mimari

```
IDE (react) ──IPC──► Electron main.ts ──stdio NDJSON──► gcl-lsp.exe
                              ▲                                │
                              └────────── yanıtlar ◄───────────┘
```

- Proses: tek `gcl-lsp.exe`; istekler sıralı işlenir.
- State: `Workspace { root, files[], syms[] }` — her file `lang: "lua" | "py"` etiketi taşır.
- Protokol: initialize / didChange / completion / shutdown (Python ile aynı).

## 5. Import Çözümleme Kuralları (Net Tanım)

Modül = `.lua` dosyası (uzantısız adı). Paket = dosya içeren klasör.

### 5.1 `require("X")`
- X = kök modül adı → `X.lua` aranır.
- Öneriler: `require("` yazarken workspace modül adları + paket klasör adları.

### 5.2 `local m = require("X")`
- m = alias. `m.` → X.lua'nın return tablo üyeleri.
- Kaydedilmemiş satırlar da canlı text'ten çözülür (live alias).

### 5.3 `require("P/M")`
- P = paket, M = P içindeki modül.
- `require("P/` yazarken P içindeki .lua dosyaları önerilir.

### 5.4 `m.` (member)
- m bir modül ise → o dosyanın `return { ... }` anahtarları (fn/const).
- m bir paket ise → içindeki modül adları.

### 5.5 `local x = 5` / `function ad()`
- Üst seviye yerel değişkenler ve fonksiyonlar o dosyanın sembolleri olur.

## 6. Tamamlama Kategorileri ve Öncelik Sırası

1. **Canlı yerel değişkenler** — `local x = ...` (imlece kadar).
2. **Require bağlamı**
   - `require("` → modül + paket adları
   - `local m = require(...)` sonrası `m.` → üyeler
3. **Açık dosyanın own sembolleri** — `function ad / local ad`.
4. **Workspace modül adları** — yalnızca `require("` yazarken.
5. **Lua dil tablosu** — keywords + globals (`lua_syntax.c`).
6. **Fridge** — workspace'te olmayan modül için gcl `-luarun -resolve`.

## 7. Doğrulama Senaryoları (Test Planı)

1. `require("testFolder/helloworld")` → helloworld.lua modülü.
2. `local hw = require(...)` sonrası `hw.` → helloworld.lua return tablo üyeleri.
3. `require("final_fantasy/` → seven.lua.
4. `local util = require("util")` → `util.` → util.lua üyeleri.
5. IDE açıkken yeni klasör + .lua ekle → didChange ile 1–2 sn içinde görünür.
6. `pri` → print (Lua globals).
7. Boş prefix Ctrl+Space → asla boş değil.

## 8. Uygulama Adımları (Sırası)

1. `collect_lua` (.lua taraması) + `FileIndex.lang`.
2. `index_lua_line` (require / return-tablo / function / local).
3. `lua_syntax.c` (keywords + globals).
4. `parse_require` + member çözümleme (5.1–5.4).
5. Fridge `-luarun -resolve` bayrağı.
6. didChange uzantı filtresine `.lua` ekle.
7. Derle + test senaryoları + dağıt.
