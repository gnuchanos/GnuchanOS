# gcl-lsp — Python Odaklı Tasarım Dokümanı

## 1. Amaç

GnuChanIDE içindeki otomatik tamamlamanın **Python için sağlam ve öngörülebilir** çalışmasını sağlamak. Kullanıcı Python yazarken:

- `import x` → modül adları
- `import paket.modul` → paket + modül erişimi
- `import paket.modul as k` → alias (`k.` çalışır)
- `from paket import modul` → klasör içindeki .py dosyaları
- `from paket.modul import sembol` → modül içindeki fonksiyon/const/class
- `from paket import *` → modülün tüm sembolleri
- `modul.` → o modülün üyeleri
- düz yazımda → o dosyadaki semboller + Python keywords/builtins

**Kritik kural: `from klasor import dosya` gibi "dosya → görünür dosya" eşlemeleri asla hard-coded değildir; diskten dinamik olarak taranır ve IDE açıkken eklenen dosyalar otomatik algılanır.**

## 2. Mevcut Durum Analizi

| Katman | Durum |
|---|---|
| LSP çekirdeği | C, NDJSON over stdio (`gcl_lsp.c`), tek proses |
| Workspace index | Açılışta + `didChange` ile diskten taranır (src/, root, Library/Python) |
| Tamamlama yanıtı | `{"id":N,"result":["label","kind","detail"]}` — geçerli JSON (düzeltildi) |
| Fridge (stdlib) | `gcl -pyrun -resolve` ile gerçek Python sorgusu; cache ayrı tutulur |
| IDE entegrasyonu | Watcher → `lspDidChange` (add/unlink/change) |

**Güçlü yönler:** Tek proses, milisaniye yanıt; disk taraması hızlı; fridge stdlib/pip paketlerini gerçek Python'dan çözüyor; `didChange` artık yeni dosyaları anında index'e alıyor.

**Zayıf yönler / açık noktalar:**
1. İmport çözümleme tam dosya yolu üzerinden değil, "basename üzerinden global arama" — aynı ada sahip iki dosya (ör. `src/a/util.py` ve `src/b/util.py`) karışabilir. **GERÇEK TEST (1:51):** `from beta import u` → `beta/util.py` doğru öneriliyor (from-import modül çözümlemesi klasör segmentiyle çalışıyor). ANCAK `from beta import util` yazıp `util.` yazınca `beta/util.py` yerine `alpha/util.py`'nin `alpha_deger` üyesi dönüyor — basename çakışmasında ilk eşleşen kazanıyor. **DOĞRULANDI, kısmen.**
2. `from paket import dosya` yalnızca **son klasör segmenti** ile eşleşiyor (`file_dir_base`); `src/final_fantasy/seven.py` için `from final_fantasy import seven` çalışır ama `from src.final_fantasy import seven` çalışmaz. (Test edilmedi — tasarım düzeyinde açık.)
3. Noktalı import (`import a.b.c`) ilk segmentte kesiliyor; `a.b.` yazıldığında b/c segmentleri önerilmiyor. **GERÇEK TEST (1:51):** `import alpha.` → sadece `alpha` modül adı önerildi; `alpha.util` segmenti yok. **DOĞRULANDI.**
4. Klasör adları "package" olarak öneriliyor ama içi açılmıyor (member tamamlama sadece modül için). **GERÇEK TEST (1:51):** `alpha.` → `util` önerilmedi; bunun yerine popup'a hata mesajları sızdı (`import error: No module named 'alpha'`, `python resolve error: alpha|`). Bu hem §2-4'ü hem de yeni bir bug'ı doğruluyor: **fridge hatası popup'a sızıyor** — gizlenmeli.
5. Sınıf üyeleri (`self.x`, `obj.y`) henüz index'lenmiyor. (Kod incelemesi: sınıf adı index'leniyor, `self.` üyeleri içerideki `def`'ler değil.) **DOĞRULANDI.**

## 3. Tasarım İlkeleri

1. **Her şey diskten.** Sembol, klasör, modül, import adı — hiçbiri elle yazılmaz.
2. **Tek doğruluk kaynağı: dosya yolu.** Import çözümleme önce açık dosyanın dizininden, sonra workspace köküne göre **göreli yol** ile yapılır (basename değil).
3. **Öngörülebilir öncelik.** Tamamlama çıktısı her zaman aynı sırada: canlı yazılanlar → import bağlamı → workspace modülleri → açık dosyanın sembolleri → Python dil tablosu.
4. **Asla boş kalma.** LSP çökse/yoksa IDE yerel kelime havuzuna düşer (renderer fallback).
5. **Bellek = deterministik.** Yeniden index (didChange) diskteki gerçeği yansıtır; cache sadece fridge (stdlib) içindir.

## 4. Mimari

```
IDE (react)  ──IPC──►  Electron main.ts  ──stdio NDJSON──►  gcl-lsp.exe
                                    ▲                                │
                                    └────────── yanıtlar ◄───────────┘
```

- **Proses:** tek `gcl-lsp.exe`; istekler sıralı işlenir (basit, sıralı stdout).
- **state:** `Workspace { root, files[], syms[] }` + ayrı `FridgeCache` (stdlib için).
- **Protokol:**
  - `initialize {root}` → index kur, `{ok, files}`
  - `textDocument/didChange {file}` → workspace'i diskten yeniden tara
  - `textDocument/completion {file,line,col,text}` → tamamlama listesi
  - `shutdown` → çık

## 5. Import Çözümleme Kuralları (Net Tanım)

Modül = `.py` dosyası (uzantısız adı). Paket = dosya içeren klasör.

### 5.1 `import X`
- X = kök seviye modül adı → `X.py` aranır (önce açık dosyanın dizini, sonra workspace'te göreli yol).
- Öneriler: `import ` yazarken workspace modül adları + paket klasör adları.

### 5.2 `import P.M` (noktalı)
- P = kök paket, M = P içindeki modül.
- `import a.b.c` → modüller: `a` (paket), `b` (alt paket), `c` (modül).
- `a.b.` yazıldığında işin b segmenti gösterilir (ara segmentler de önerilir).

### 5.3 `import P.M as k`
- k = son segment (M). `k.` → M'nin üyeleri.
- `live_alias_to_mod` zaten kaydedilmemiş satırları da çözer (korunur).

### 5.4 `from P import X`
- P bir **paket** ise → X = P içindeki .py dosya adları (modül önerileri).
- P bir **modül** ise → X = P.py içindeki semboller (fn/class/const).
- P workspace'te yoksa → fridge (gerçek Python) devreye girer.

### 5.5 `from P import *`
- P'nin tüm üyeleri, modül adı yazmadan kullanılabilir hale gelir (wildcard).
- Disk index + canlı text'ten (kaydedilmemiş) çözülür.

### 5.6 Nokta dizisi — göreli import desteği
- `from . import X`, `from .. import X` → açık dosyanın konumundan yukarı çözülür (yeni).

## 6. Tamamlama Kategorileri ve Öncelik Sırası

Tamamlama tek fonksiyondan üretilir; çıktı **sıralı** döner:

1. **Canlı yerel değişkenler** — imlece kadar yazılmış `ismi = değer` satırları (const).
2. **Import bağlamı**
   - `from P import ` → paket içi modüller (5.4)
   - `import ` → modül + paket adları
   - `P.` → P'ye bağlı öneriler (modül ise üyeler, paket ise alt modüller)
3. **Açık dosyanın own sembolleri** — `def/class/NAME =` (üst seviye).
4. **Workspace modül adları** — yalnızca import/from yazarken.
5. **Python dil tablosu** — keywords + builtins (`python_syntax.c`).
6. **Fridge** — workspace'te olmayan modül adları için gerçek Python sorgusu (stdlib, pip).

Tekilleştirme `label` üzerinden yapılır (aynı kelime iki kez listelenmez).

## 7. Gerçek Konumla Çözümleme (Yeni — Zayıf Yön 1/2/3'ün Çözümü)

Mevcut `resolve_module` basename arıyor. Yeni tasarım:

- Workspace `files[]` her biri **göreli yol** taşır (`src/final_fantasy/seven.py`).
- Çözümlemede önce: açık dosyanın dizini + modül adı → `dizin/modul.py` (tam yol eşleşmesi).
- Bulunamazsa: `src/<paket>/<modul>.py` şeklinde göreli yol denenir.
- Basename eşleşmesi yalnızca **son çare**.

Bu, aynı ada sahip farklı klasörlerdeki dosyaların karışmasını önler (netlik).

## 8. Sınıf Üyeleri (Zayıf Yön 5 — Yeni)

- `class X:` altındaki `def m(self,...)` → `X.m` (fn), `self.` tamamlamasında önerilir.
- `self.alan = ...` → alan const olarak.
- `obj.` için tip çıkarımı YAPILMAZ (statik analiz kapsamı dışı); yalnızca açık dosyada tanımlı sınıf üyeleri.

## 9. Fridge (Stdlib / Pip) Davranışı

- Trigger: member tamamlama (modül index'te yok) veya `from X import` (X workspace'te yok).
- Çalıştırma: `gcl -pyrun -resolve "<mod>|<prefix>"` — gerçek Python modülünü içeri alıp sembollerini basar.
- Cache: modül başına bir kez, `g_fridge` (workspace yeniden index'inden bağımsız).
- Başarısızlık: sessiz; yalnızca modül adı önerilir.

## 10. Hata/Performans Garantileri

- LSP her yanıtı **tek satır geçerli JSON** olarak basar; aksi durumda IDE sonsuza beklemez (renderer 300–400ms timeout + fallback).
- didChange fire-and-forget: IDE tıkanmaz.
- Index yeniden tarama 2048 dosya / 65536 sembol sınırında kesilir (güvenli).
- Watchdog: fridge sorgusu 10s'de iptal edilir.

## 11. Doğrulama Senaryoları (Test Planı)

1. `from testFolder import hel` → `helloworld` (modül).
2. `import final_fantasy` → `final_fantasy` (paket); `from final_fantasy import s` → `seven`.
3. `import util as u` (kaydedilmemiş) → `u.` → util üyeleri.
4. `from pkg import *` sonrası düz `pri` → print dahil tüm pkg üyeleri + builtin.
5. IDE açıkken yeni klasör + .py ekle → `from yeniKlasor import ` → 1–2 sn içinde öner.
6. `from paket import modul` — aynı ada sahip iki dosya farklı klasörlerde → doğru olanı gelir.
7. `ri.` → `InitWindow` (pyRaylib wrapper) — fridge/static.
8. Bos prefix Ctrl+Space → tüm modüller + builtin'ler (boş liste asla).

## 12. Uygulama Adımları (Sırası)

1. `resolve_module`'u göreli yol tabanlı yap (5.4 + 7).
2. Noktalı import segment önerileri (`import a.b` → a, a.b).
3. `from .` / `from ..` göreli import (5.6).
4. Paket member tamamlama: `paket.` → alt modüller.
5. Sınıf üyesi index'i (8).
6. Test senaryolarını otomatik doctest haline getir (11).
7. `gcl-lsp.exe` derle, `app.asar` dağıt.
  