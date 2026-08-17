# GCL Shell — Fish Benzeri Gelişmiş Shell Yol Haritası

> Hedef: `gcl` interaktif kabuğunu, fish'i çağrıştıran modern bir satır
> düzenleyiciye dönüştürmek. **Windows ve GNU/Linux'ta birebir aynı
> davranış.** Kod paylaşımı ortak çekirdek katmanından gelir; platforma
> özgü kod yalnızca "bu tuşu nasıl okurum" ve "ekrana nasıl yazarım"
> sorusuna cevap verir.

---

## 1. Mevcut Durum Analizi (kayıt niteliğinde)

| Dosya | Yapı | Eksikler |
|---|---|---|
| `language/src/shell_windows.c` | `printf(prompt)` + `fgets` + `shell_split` döngüsü | Satır düzenleme, geçmiş, tamamlama, vurgu yok. CRLF için `_setmode(_O_BINARY)` eklendi (IDE pipe entegrasyonu). |
| `language/src/shell_gnuLinux.c` | aynı mantık | Aynı eksikler; `termios` raw mod yok. |
| `language/src/ide/src/components/TerminalPanel.tsx` | xterm.js + satır modeli (pendingLineRef) | IDE tarafı tuşları karakter karakter yakalayıp satır halinde stdin'e gönderiyor. Shell gelişince bu katman "pasif terminal" konumuna geçmeli. |

Ayrıca iki dosyadaki komut seti (`ls, pwd, cd, echo, clear, help, exit,
version`) birebir aynı — bu, davranış farkının komutlardan değil **girdi
işleminden** kaynaklandığını gösterir. Yani çözüm merkezi: girdi.

---

## 2. Mimari Karar: Ortak Line Editor Çekirdeği

Mevcut projede `language/src/SharedPipeline/` diye ortak hedef var. Bunun
yanına shell için yeni, platformdan bağımsız bir katman gelir:

```
language/src/Shell/
├─ gcl_shell_core.c        # ortak çekirdek (platformsuz)
├─ gcl_shell_core.h        # public API (satır düzenleyici + komut motoru)
├─ gcl_shell_linebuf.c     # satır modeli: buffer, imleç, undo? (ilk faz: hayır)
├─ gcl_shell_history.c     # oturum içi + kalıcı geçmiş (dosya)
├─ gcl_shell_complete.c    # tamamlama motoru (komut + yol)
├─ gcl_shell_highlight.c   # basit sözdizimi renklendirme kararları
├─ gcl_shell_editor.c      # tuş olayı -> satır durumu (ortak FSM)
└─ platform/
   ├─ gcl_shell_platform_win.c    # Windows: ReadConsoleInput/haberci _getch
   └─ gcl_shell_platform_posix.c  # Linux: termios raw + read()
```

Kural 1: `main.c` hangi platformdaysa o platform dosyasını derler; çekirdek
dosyalar ikisine de **aynı kaynaktan** girer. Kural 2: çekirdek yalnızca
`sint` tabanlı bir "tuş girdisi" API'si görür, platform dosyası her tuşu
kodlanmış bir olaya çevirir (ascii / escape / ctrl). Böylece ev (Home) tuşu
Windows'ta `0x47` önekiyle, Linux'ta `\x1b[H` ile gelse de çekirdek aynı
`KEY_HOME` olayını alır.

Bu mimarinin avantajı: IDE'deki xterm.js de bu API'nin üçüncü bir
"görünümü" olur — main.ts, terminal:write kanalından gelen tuşları
olaylaştırıp aynı çekirdeğe verir; hatta asıl güç, shell'in kendi
tamamlama/vurgu durumunu tek yerde tutmasıdır.

---

## 3. Hedef Davranış (fish Benzeri Özellikler)

Her maddenin yanında "neden fark edileceği" yazıyor — böylece doğrulama
adımı da tanımlı olur. İlk sürümde tamamlanan kriter: **iki platformda
aynı tuş aynı sonucu üretir.**

### 3.1 Satır Düzenleme (Line Editing)
- Sol/sağ ok: imleç hareketi; Home/End satır başı/sonu.
- Backspace ve Delete (silme yönü ayırt edilir).
- Ctrl+A / Ctrl+E: satır başı / sonu.
- Ctrl+U: imleçten satır başına kadar sil; Ctrl+K: imleçten satır sonuna.
- Ctrl+W: önceki boşluğa kadar kelime sil.
- Çift satır sarması: uzun satırlar görünümde sarılır, imleç "satır içi"
  konumunu kaybetmez (tek boyutlu karakter indeksi tutulur).

### 3.2 Geçmiş (History)
- Yukarı/aşağı ok: komut geçmişinde dolaşma; düzenlenmemiş öğe korunur.
- Ctrl+R: art arda yazılan metinle geçmişte artan arama (basit: her harfte
  ilk eşleşmeye atla).
- Kalıcı geçmiş: `~/.gcl_history` (Linux) / `%USERPROFILE%\.gcl_history`
  (Windows) — aynı format, aynı okuma kod yolu; platform yalnızca kök
  dizin yolunu verir.
- Geçmiş uzunluğu sınırlı (varsayılan 1000), ortak fonksiyonda kırpılır.

### 3.3 Sekme Tamamlama (Tab Completion)
- Komut adları (`he` + Tab → `help`), dosya/dizin adları (yol tamamlama).
- Tam aday yoksa: `cd ` tam olarak tamamlanır; birden çok aday → ekrana
  sütunlu liste + prompt yeniden çizilir (fish davranışı).
- Tamamlanma her zaman mevcut imleç konumunda yapılır; ilk sürümde
  "satır sonu" kısıtlaması kaldırılır (çekirdek buffer buna izin verir).
- Yol tamamlama: Windows'ta `\` ve `/`, Linux'ta `/`; ayraç normalizasyonu
  tamamlama motorunun sorumluluğunda, platform değil.

### 3.4 Otomatik Öneri (Autosuggestion)
- Yazılan önek geçmişteki bir komutla eşleşiyorsa, kalan kısım **soluk**
  renkte önerilir (fish'in gri önerisi gibi).
- Sağ ok veya Ctrl+F: öneriyi kabul et; başka karakter: öneri kaybolur.
- Görünüm katmanı renklendirmeyi `\x1b[2m` (dim) ile yapar; çekirdek
  yalnızca "öneri metni + uzunluğu" bilgisini verir.

### 3.5 Sözdizimi Vurgusu (Syntax Highlighting)
- Komut ilk kelimeyse renkli, bilinen bir komutsa ayrı, dosya yoluna
  benzeyen ifade ayrı, metin/parametre ayrı.
- Vurgu, satır değiştiğinde tam satırı yeniden çizen `render()` fonksiyonu
  üzerinden yapılır (imleç konumu korunur).
- renk kodu üretimi platformdan bağımsız ANSI'dir; Windows'ta buna gerek
  kalmaz çünkü IDE/konsole ANSI'yi işler, Linux'ta da öyle.

### 3.6 Çok Satırlı Girdi (Gelecek Faz)
- Açık parantez / çift tırnak varsa devam promptu: `gcl> ` yerine `...> `.
- Bu faz için parantez eşleştirme basit sayaç kullanır; gelişmiş ayrıştırma
  sonraya bırakılır (SharedPipeline lexer'ı ile birleştirilebilir).

### 3.7 IDE Entegrasyonu (xterm.js)
- TerminalPanel'deki yerel `pendingLineRef` modeli zaten "satır halinde
  gönder" prensibini kurdu; bu davranış aynen kalır.
- Yeni akış: xterm tuşları doğrudan shell çekirdeğine gitmez; mevcut pipe
  düzeni korunur, çünkü gcl shell'i **konsolda** olgunlaştıktan sonra IDE
  tarafı aynı çekirdeğin verdiği tamamlama/öneri meta verilerini (opsiyonel
  IPC mesajı) kullanabilir. Öncelik konsol tarafındadır.

---

## 4. Uygulama Adımları (Faz Sıralı)

### Faz 1 — Line Editor Çekirdeği (satır modeli + tuş olayları)
- `gcl_shell_linebuf.c`: karakter dizisi, imleç, insert/delete, set/clear.
- `gcl_shell_editor.c`: tuş kodları → komutlar FSM'si (move, edit, history,
  complete, accept-suggestion).
- Platform dosyalarını **aynı anda** yaz: her tuş eşleme tablosu, iki
  yerde aynı mantıkla.
- Kabul kriteri: konsolda yazma/silme/ok tuşları/Home/End iki platformda
  aynı davranır.

### Faz 2 — Geçmiş + Kalıcı Kayıt
- `gcl_shell_history.c`: dizi + dosya okuma/yazma, sınır, dedup.
- Yukarı/aşağı + Ctrl+R arama.
- Kabul kriteri: `~/.gcl_history` ve `%USERPROFILE%\.gcl_history` aynı
  formatta; ikinci oturum açılışında önceki komutlar gelir.

### Faz 3 — Tamamlama + Otomatik Öneri
- `gcl_shell_complete.c`: komut listesi + yol taraması (ortak API;
  Windows `_findfirst`, Linux `opendir` platform dışına sızar, dönen liste
  ortak).
- Öneri: geçmiş + komut adı kaynaklı; görünüm katmanı dim boyar.
- Kabul kriteri: `ls` yazarken Tab → `ls` tamamlanır; öneri gri görünür,
  sağ ok kabul eder; `cd ` + Tab dizin listeler.

### Faz 4 — Vurgu + Prompt Teması
- `gcl_shell_highlight.c`: komut/parametre/yol sınıflandırma.
- Prompt yapılandırılabilir (renk, `user@host cwd>` gibi) — gelecek ayar
  dosyasından okunacak.
- Kabul kriteri: `login`/`cd` gibi komutlarda ilk kelime renkli; ANSI
  sızdırmaz (IDE'de stripAnsi güvencesi korunur).

### Faz 5 — Çok Satır + Son Rötuş
- Açık parantez/tırnak devam promptu.
- Ctrl+L (clear), Ctrl+D (boş satırda çıkış) — fish uyumu.
- Kabul kriteri: iki platformda aynı komut seti + aynı tuş haritası; IDE
  pipe modunda CRLF'siz, ANSI'siz çalışır.

---

## 5. Teknik Kısıtlar ve Kararlar (Değişmez Kurallar)

1. **Platform kodu sızmaz.** `gcl_shell_core.*` içinde `#ifdef _WIN32`
   bulunmaz; yalnızca `platform/` dosyaları ve `main.c`'deki seçimde.
2. **Girdi tek kaynaktan.** Konsol + IDE(pipe) aynı çekirdeği kullanır;
   pipe modunda satır sonu `\n` olarak normalize edilir (zaten yapılıyor).
3. **Çıktı ANSI.** Renk/vurgu ANSI kaçışlarıyla; IDE `stripAnsi` yalnızca
   "düz metin Output" için, terminal paneli ham kanal kullanır.
4. **Bellek:** sınırlı buffer (mevcut `SHELL_MAX_LINE` 2048 korunur),
   taşma yok, calloc öncelikli — derleyici `-Wall -Wextra` temiz.
5. **Derleme:** `makefile.py` gcl_BUILD'ye yeni dosyaları ekler; iki
   platformda aynı `gcc` komutu (Linux `-lm -ldl`, Windows `-lm -lws2_32`
   farkı dışında).
6. **Geriye uyum:** `printf("gcl %s - interactive shell ...")` banner
   metni ve `gcl> ` promptu korunur; IDE beklentileri (satır bazlı stdin)
   değişmez.

---

## 6. Doğrulama ve Test Planı

- **Tuş haritası testi:** her EOF/escape/ctrl olayı iki platformda aynı
  `KEY_*` koduna dönüşmeli — `platform` testleri ayrı butonunda.
- **Senaryo testleri (klavye):** `hel` + Tab → `help`; geçmiş analizi ile
  öneri; `cd ..` sonrası `pwd` doğrulaması; uzun satır sarma davranışı.
- **CI benzetimi:** console uyumlu bir "tuş kaynağı" üzerinden her
  platformun build'i aynı girdi dosyasını tüketir ve beklenen ekran
  görüntüsünü üretir (ilk fazda manuel olarak).
- **IDE regression:** TerminalPanel ile shell'in birlikte çalışması:
  yaz, sil, Tab, geçmiş → hâlâ satır basılı giriş bekler, çıktı temiz.

---

## 7. Özellik Matrisi (başlangıç ve hedef)

| Özellik | Bugün | Hedef (bu plan sonu) |
|---|---|---|
| Satır düzenleme | hayır | evet (imleç + ctrl kısayolları) |
| Geçmiş / kalıcı kayıt | hayır | evet (+Ctrl+R) |
| Tab tamamlama | sadece IDE'de (GCL_COMMANDS) | shell'de komut + yol |
| Otomatik öneri | hayır | evet (dim öneri) |
| Sözdizimi vurgusu | hayır | evet (komut/yol/parametre) |
| Çok satırlı girdi | hayır | evet (devam promptu) |
| Platform davranış farkı | mevcut (akış ayrı) | tek çekirdek, aynı davranış |

---

## 8. Kapanış Notu

Bu doküman bir "yapılacaklar listesi" değil, uygulamanın tamamlanmış
tasarım sözleşmesidir. `terminal_todo.md` geldiği gibi değil, takip
edilebilir bir mühendislik dosyası olarak yaşayacak: her fazın çıktısı,
kabul kriteri ve bu planın 5. bölümündeki değişmez kuralları alt alta
dizildiğinde, Linux'ta yazılmış bir gcl shell'i Windows'ta **aynı
davranışla** çalışır.
