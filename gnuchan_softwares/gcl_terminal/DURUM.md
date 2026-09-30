# GnuChanTerm — durum ve mimarî

Bu dosya, işin nerede kaldığını kayda geçirmek için var. Bir oturumda
bitmeyecek kadar büyük; bağlam penceresi dolduğunda buradan devam edilir.

## Hedef

**btop çalışsın.** Gerisi ona bağlı: btop çalışırsa geri kalan programlar
Raylib ile yazılabilir; çalışmazsa X11 odaklı devam edilir.

## Kanıtlanmış gereksinim — tahmin değil, kaynaktan okundu

btop'un kaynağı okundu (`src/btop.cpp`, `src/btop_input.cpp`):

- btop terminale **sorgu göndermiyor**, sadece **yazıyor**. Truecolor'ı
  `Config::getB("truecolor")`'dan alıyor, terminale sormuyor. Mouse
  (`?1006h`) ve sync (`?2026h`) için de aynı. Desteklemeyen terminal bunları
  yok sayar, btop aldırmaz.
- btop iki kapı tutuyor, ikisi de `forkpty` ile karşılanıyor:
  - `Term::init()` → `isatty(STDIN)` değilse "No tty detected!" ile çıkış
  - `Term::refresh()` → `TIOCGWINSZ` okunamazsa 100 deneme sonra
    "Failed to get size of terminal!" ile çıkış
- `TIOCGWINSZ` boyutu **forkpty'ye geçirilmeli**. Sonradan ayarlanırsa child
  sormadan önce cevap alamaz.
- Input ham bayt: `pselect` + `read`, `Key_escapes` tablosunda VT dizileri.
  Escape dizilerini **yazmak** yeterli.

## Dosya durumu — HEPSİ YAZILDI

    term_module.h              modül arayüzü (event / tick / cleanup)
    term_grid.h / .c           hücre, satır, palet; scroll = işaretçi rotasyonu
    term_vt.h / .c             parser, iki ekran, DEC graphics, DECSTR
    term_core.h / .c           X bağlantısı, pencere, event loop
    term_pty.h / .c            forkpty, non-blocking I/O, TIOCSWINSZ
    term_style.h / .c          Xft font, palet, GC
    term_render.h / .c         dirty-satır çizim, çalışan çizim
    term_render_internal.h     çizim tamponu (özel durum)
    term_input.h / .c          tuş -> escape dizisi, mouse (SGR + klasik)
    GnuChanTerm.c              giriş noktası + modül listesi + shell spawn
    makefile.py                build/install, gcl_WM deseni
    terminfo/gcl-256color      terminalin kendi terminfo girdisi

## SIRADAKİ İŞ: Linux makinede derle

Bu makine Windows. Derleme yapılamaz, deneme yapılamaz. Yapılacaklar
sırayla:

1. `python3 makefile.py build`
   - Beklenen: uyarısız derleme. `-Wall -Wextra` açık.
   - Eksik paket varsa makefile kendisi kurar (libx11-dev, libxft-dev,
     libfreetype-dev, libxrender-dev, pkg-config, build-essential).
2. `python3 makefile.py run` — shell açılmalı, yazı yazılabilmeli.
3. `python3 makefile.py` — kurar; `/usr/local/bin/GnuChanTerm` ve
   masaüstü girdisi, `tic` ile terminfo.
4. btop: `GnuChanTerm -e btop`

### Bu makinede YAPILAMAYAN doğrulamalar

- Derleme yok, çalıştırma yok. Derleyici hatası çıkarsa buradan devam.
- X11 / Xft başlıkları yok, bu yüzden include sırası ve tip uyumu
  derleyiciyle değil, elle denetlendi (aşağıya bak).

## Elle yapılan uyum denetimi

Tüm `.c` dosyaları header'larıyla karşılaştırıldı; çağrılan her fonksiyonun
imzası header'da birebir var:

    term_grid.c    25 fonksiyon  ↔ term_grid.h    ✓
    term_style.c   12 fonksiyon  ↔ term_style.h   ✓
    term_pty.c      7 fonksiyon  ↔ term_pty.h     ✓
    term_vt.c      11 fonksiyon  ↔ term_vt.h      ✓
    term_render.c   5 fonksiyon  ↔ term_render.h  ✓
    term_core.c    15 fonksiyon  ↔ term_core.h    ✓

Yapı alanları da denetlendi: `TermVt` (grid, alt, active, state, seq,
param_value, param_seen, osc, osc_len, utf8_*, host, unknown_sequences,
mouse_*, bracketed_paste, application_cursor, dec_graphics) — `term_vt.c`
yalnızca bunları kullanır. `TermCell` (ch, fg, bg, truecolor_*, attrs, wide,
wide_cont, dirty) — aynı şekilde.

Bu oturumda düzeltilen gerçek hatalar:

1. `TERM_PTY_MAX_ARGS` hiçbir header'da yoktu, `GnuChanTerm.c` kullanıyordu
   → `GnuChanTerm.c` içinde tanımlandı.
2. `term_input.c` `core->style->cell_width` okuyor ama `term_style.h`'yi
   include etmiyordu → include eklendi.
3. `term_input.c` mouse: `Button1` için `4 + 4` yazıyordu (yani 8) → `4`.
4. `term_vt.c` `vt_soft_reset` tanımlı ama çağrılmıyordu → DECSTR (`ESC [ ! p`)
   handler'ına dönüştürüldü ve CSI tablosuna `{ 'p', 0, '!', do_decstr }`
   olarak bağlandı.

## Mimarî kararlar

- **Saf C99 + Xlib + Xft.** gcl_WM ile aynı desen: çekirdek tek Display ve
  tek event loop sahibi; modüller `event` / `tick` / `cleanup` alır.
- **Event loop hem X hem PTY'yi bekler.** `select()` ile X soketi
  (`ConnectionNumber`) ve PTY master'ı birlikte. Sadece X'i bekleyen bir loop
  btop'un çıktısını fare oynatılana kadar çizmez — bu terminalin en kritik
  satırları `term_core.c` `term_core_step()` içinde.
- **İki ekran:** `grid` (normal) + `alt` (alternatif). btop `?1049h`
  gönderiyor; `?1047` ve `?47` de desteklenir.
- **Hücre = Unicode kod noktası**, byte değil. Braille (`U+2800` ve üstü) ile
  box çizim karakterleri byte'a sığmaz.
- **Izgara = satır tablosu**, tek düz dizi değil. Scroll = işaretçi
  rotasyonu; hücre kopyası yok. `yes` ile başa çıkmanın tek yolu bu.
- **Dirty-takibi satır bazlı.** Cursor blink yalnızca imlecin satırını
  kirletir; tüm ekranı değil. Bu yüzden blink saniyede iki kez tüm ekranı
  çizmez.
- **Glyph önbelleği YOK.** Xft zaten kendi glif önbelleğini tutuyor; ikinci
  katman gereksiz. Tasarruf "değişmeyeni çizmemek"ten gelir.
- **Çizim çalışan hâlde.** Bir satır üç geçişte çizilir: arka planlar
  (aynı renkli komşu hücreler tek dikdörtgen), metin (aynı yüz+renk tek Xft
  çağrısı), altçizgiler. Tampon pixmap'e çizilir, sonra `XCopyArea` ile
  pencereye kopyalanır — yırtılma olmaz.
- **Shader sonra.** Metin Xft ile çizilir; shader yalnızca arka plan katmanı
  olur (tek quad, `#version 120` — GMA 965 GL 2.1 destekliyor).

## Palet indeksleri

    0..15                     programın adlandırdığı on altı renk
    16  TERM_COLOR_INDEX_FG   temanın varsayılan metin rengi
    17  TERM_COLOR_INDEX_BG   temanın varsayılan arka planı
    -1  TERM_COLOR_DEFAULT    program "varsayılan" dedi -> 16 / 17
    -2  TERM_COLOR_TRUECOLOR  program 0xRRGGBB gönderdi

## TERM adı ve terminfo

`TERM=gcl-256color`. `xterm-256color` **değil**: bu bir xterm değil, ve adı
xterm koymak xterm'e özel bir özelliği çağırabilir. `gcl-256color` için özel
durum yok, yani her program terminfo girdisini okur — ve girdi bu terminali
birebir anlatır (`terminfo/gcl-256color`, `tic -x` ile kurulur).

`COLORTERM=truecolor` ayrıca ayarlanır: btop'un baktığı tek şey bu.

## KRİTİK — write_to_file kullanımı

`<path>` etiketi **`<content>`'den ÖNCE** yazılmalı. Sonra yazılırsa araç
yolu okuyamıyor, dosyayı yolun ilk harfleriyle (`gn`) kaydediyor ve her
yazım bir öncekinin üstüne biniyor. Bu oturumda `term_vt.c` böyle 0 byte'a
düştü ve baştan yazıldı.

Doğru sıra:

    <write_to_file>
    <path>gnuchan_softwares/gcl_terminal/term_grid.h</path>
    <content>
    ... içerik ...
