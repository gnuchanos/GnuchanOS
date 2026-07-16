# 🚀 GnuchanOS — Roadmap & Todo

> **Vizyon:** GNU Guix tabanlı, %100 özgür (kapalı kaynak yok — zorunlu GPU/WiFi driverları hariç), GCL ile yazılmış tam kendi kendine yeten dağıtım.
>
> **Dil Felsefesi:** Python gibi yorumla, C gibi derle — aynı dilde iki hedef.
> - `gcl -run file.gcsf` → GCPY (Python-like) interpreted mode
> - `gcl -build file.gcsf` → GCL (C-like GNU99) compiled mode
> - **Nihai hedef:** Python'dan hızlı, C'den güvenli, tam kontrollü bir dil

---

## 📋 İçindekiler

1. [Faz 0 — Temel Dil (GCL Runtime & Compiler)](#faz-0--temel-dil-gcl-runtime--compiler)
2. [Faz 1 — İşletim Sistemi Çekirdek Bileşenleri](#faz-1--işletim-sistemi-çekirdek-bileşenleri)
3. [Faz 2 — Masaüstü Ortamı & Grafik Arayüz](#faz-2--masaüstü-ortamı--grafik-arayüz)
4. [Faz 3 — Sistem Araçları & Altyapı](#faz-3--sistem-araçları--altyapı)
5. [Faz 4 — Paket Yönetimi, Bootloader & Dağıtım](#faz-4--paket-yönetimi-bootloader--dağıtım)
6. [Faz 5 — Tema & Kişiselleştirme Sistemi](#faz-5--tema--kişiselleştirme-sistemi)
7. [Faz 6 — Güvenlik & Gizlilik](#faz-6--güvenlik--gizlilik)
8. [Faz 7 — Release & Sürekli Geliştirme](#faz-7--release--sürekli-geliştirme)
9. [Teknik Borç & Altyapı](#teknik-borç--altyapı)

---

## Faz 0 — Temel Dil (GCL Runtime & Compiler)

> **Durum:** 🟡 Kısmen başlandı (flag sistemi çalışıyor, lexer/parser stub)

### 0.1 Lexer (Ortak)
- [ ] Token tipleri: `TOKEN_IDENTIFIER`, `TOKEN_NUMBER`, `TOKEN_STRING`, `TOKEN_OPERATOR`, `TOKEN_KEYWORD`...
- [ ] GCL keyword listesi (`int`, `char`, `struct`, `if`, `while`, `for`, `return`...)
- [ ] GCPY keyword listesi (`def`, `class`, `import`, `if`, `elif`, `else`, `for`, `in`...)
- [ ] String literal işleme (tek tırnak, çift tırnak, f-string, escape seq)
- [ ] Number literal işleme (int, float, hex, binary, complex)
- [ ] Comment atlama (`//`, `#`, `/* */`)
- [ ] `.gcsf` dosyasından token akışı üretme
- [ ] `-lexer` flag'ini gerçek lexer'a bağlama

### 0.2 Parser (Ortak)
- [ ] AST düğüm yapısı: `AST_Program`, `AST_FunctionDef`, `AST_If`, `AST_For`, `AST_While`, `AST_BinaryOp`, `AST_Literal`, `AST_Identifier`, `AST_Block`, `AST_Return`, `AST_FuncCall`, `AST_VarDecl`...
- [ ] İfade ayrıştırma (expression parsing — recursive descent)
- [ ] Blok yapıları (`{ }` ile)
- [ ] Fonksiyon tanımı (GCL: `int add(int a, int b) { }`, GCPY: `def add(a, b) { }`)
- [ ] Kontrol akışı (if/elif/else, switch/case, match/case)
- [ ] Döngüler (for classic, for-in, while, do-while)
- [ ] Import/include direktifleri (GCL: `#include`, GCPY: `import`)
- [ ] `-parser` flag'ini gerçek parser'a bağlama

### 0.3 AST
- [ ] AST dump (debug çıktısı)
- [ ] AST dolaşma (visitor pattern)
- [ ] `-ast` flag'ini bağlama

### 0.4 GCL Codegen (C backend)
- [ ] AST → C kodu dönüşümü
- [ ] Tip bilgisi çıktısı (int → int, float → float, struct → struct...)
- [ ] GCL güvenli fonksiyon çağrıları (`gc_malloc`, `gc_free`, `gc_assert`...)
- [ ] `#include` → `#include` olarak geçirme
- [ ] `#extern` → `extern` declaration + link
- [ ] `-codegen` flag'ini bağlama
- [ ] GCC ile derleme (C codegen → .c → GCC → executable)
- [ ] `-build` flag'ini tamamlama

### 0.5 GCPY Bytecode & VM
- [ ] Bytecode instruction set tasarımı
  - `OP_PUSH`, `OP_POP`, `OP_ADD`, `OP_SUB`, `OP_MUL`, `OP_DIV`, `OP_MOD`, `OP_POW`
  - `OP_JMP`, `OP_JMP_IF`, `OP_JMP_IF_NOT`
  - `OP_CALL`, `OP_RETURN`, `OP_MAKE_FUNC`, `OP_GET_LOCAL`, `OP_SET_LOCAL`
  - `OP_MAKE_LIST`, `OP_MAKE_DICT`, `OP_MAKE_SET`, `OP_MAKE_TUPLE`
  - `OP_GET_ATTR`, `OP_SET_ATTR`, `OP_GET_ITEM`, `OP_SET_ITEM`
  - `OP_IMPORT`, `OP_LOAD_CONST`
- [ ] Bytecode codegen (AST → bytecode[])
- [ ] VM: program counter, call stack, value stack
- [ ] Value system: `GCL_Value` (tagged union — int, float, string, object, list, dict, func, null, bool)
- [ ] Garbage collector (baseline: reference counting, future: mark-sweep)
- [ ] Built-in fonksiyonlar: `print()`, `len()`, `range()`, `type()`, `input()`...
- [ ] Built-in tipler: `list`, `dict`, `set`, `str`, `int`, `float`, `bool`, `NoneType`
- [ ] `-run` flag'ini gerçek VM'e bağlama
- [ ] String immutability, slicing
- [ ] Exception handling (`try/except/finally`)

### 0.6 Runtime Safety System
- [ ] Memory allocation table (`GCL_AllocEntry`)
- [ ] `gc_malloc`, `gc_free`, `gc_realloc`, `gc_calloc`
- [ ] Bounds checking (`gc_get`, `gc_set`, `gc_offset`)
- [ ] Null pointer guard
- [ ] Double-free detection
- [ ] Use-after-free detection
- [ ] Memory leak report (debug modunda)
- [ ] Instruction ID sistemi (`GCLID:file:line:col:func`)
- [ ] Error rapor formatı (kutu içinde, anlaşılır)
- [ ] Debug thread (opsiyonel crash dump)
- [ ] `GCL_DEBUG` / `GCL_RELEASE` makro kontrolü

### 0.7 CLI Flags (Tamamlama)
- [ ] `-version`, `-v`
- [ ] `-help`, `-h`
- [ ] `-linclude`, `-llib`, `-lextend`
- [ ] `-all_flags file.gcsf -o output`
- [ ] `-debug run|tokens|ast|mem`
- [ ] `-wasm raylib|binding|export`
- [ ] `-luarun`, `-pyrun` (zaten çalışıyor)
- [ ] `-dll`, `-so` (zaten çalışıyor)

### 0.8 Test
- [ ] Lexer birim testleri
- [ ] Parser birim testleri
- [ ] Codegen test (çıktı C derlenebilir mi?)
- [ ] VM test (her instruction ayrı ayrı)
- [ ] Örnek `.gcsf` dosyaları (merhaba dünya, fibonacci, fizzbuzz)
- [ ] GCL compile + çalıştırma testi
- [ ] GCPY yorumlama testi
- [ ] Runtime safety test (bounds, null, double-free)

---

## Faz 1 — İşletim Sistemi Çekirdek Bileşenleri

> **Temel:** GNU Guix tabanlı dağıtım
> **İlke:** Zorunlu olmadıkça kapalı kaynak yok

### 1.1 libre-Systemd (Init Sistemi)
- [ ] `libre-systemd` — systemd-benzeri ama tamamen özgür init
- [ ] Service unit dosyaları (`.service` GCL ile çalıştırılabilir)
- [ ] Journal log sistemi
- [ ] Service dependency management
- [ ] Socket activation
- [ ] Timer based scheduling
- [ ] GCL ile yazılacak

### 1.2 xlibre (Display Server / Session)
- [ ] `xlibre` — X11 uyumlu, Wayland düşünülebilir
- [ ] Minimal display server
- [ ] GCL ile yazılacak
- [ ] Input handling (klavye, fare, dokunmatik)
- [ ] Compositor entegrasyonu (WM ile)

### 1.3 GCL Driver Framework
- [ ] GCL ile kullanıcı alanı driver yazma altyapısı
- [ ] GPU driver arayüzü (kapalı kaynak binary + GCL wrapper)
- [ ] WiFi driver arayüzü (kapalı kaynak + GCL wrapper)
- [ ] PCI/USB device enumeration
- [ ] Interrupt handling (GCL callback)
- [ ] **Not:** GPU/WiFi kapalı kaynak driverlar GCL wrapper ile sarılacak

### 1.4 Guix Entegrasyonu
- [ ] Guix channel for GnuchanOS
- [ ] `.scm` → GCL manifest dönüşümü veya GCL manifest desteği
- [ ] Guix system config → GCL config
- [ ] Guix package build with GCL
- [ ] Reproducible build pipeline

### 1.5 Kernel
- [ ] Linux-libre kernel (LTS sürüm takibi)
- [ ] Kernel konfigürasyonu (minimal, sadece gerekli modüller)
- [ ] Kernel module signing
- [ ] Custom kernel patch set (GnuchanOS-specific)
- [ ] GCL ile kernel modülü yazma altyapısı (kernel module .gcl → .ko)

---

## Faz 2 — Masaüstü Ortamı & Grafik Arayüz

> **WM:** Qtile benzeri, tam customize edilebilir
> **DM:** Ly benzeri, minimal ve hızlı
> **GUI:** GTK wrapper ile tam GCL kontrolü

### 2.1 GTK Wrapper (GCL-GTK)
- [ ] `gcl-gtk` — C ile GTK4 binding, GCL'den çağrılabilir
- [ ] Widget'lar: Window, Button, Label, Entry, Box, Grid, TextView, Image...
- [ ] Signal/Callback sistemi (GCL fonksiyonları GTK event'lerine bağlanır)
- [ ] CSS styling desteği
- [ ] Builder (Glade/XML) desteği
- [ ] `.gcsf` içinden direkt GTK kullanımı:
  ```gcl
  gtk_window win = gtk_window_new("Merhaba");
  win.set_title("Gnuchan App");
  win.set_default_size(800, 600);
  win.show_all();
  gtk_main();
  ```

### 2.2 Gnuchan WM (Pencere Yöneticisi)
- [ ] `gwm` — GCL Window Manager
- [ ] Qtile-benzeri GCL konfigürasyonu
- [ ] Tiling (monad, bsp, grid, vertical/horizontal)
- [ ] Floating mode
- [ ] Multi-monitor support
- [ ] Keybinding (GCL ile tanımlanır)
- [ ] Gruplar / workspace / sanal masaüstü
- [ ] Bar / panel (GCL-GTK ile)
- [ ] Sistem tepsisi
- [ ] Animasyonlar (kompozitör entegrasyonu)
- [ ] **Tema sistemi desteği**

### 2.3 Gnuchan DM (Giriş Yöneticisi)
- [ ] `gnuchan display manager` — GCL Display Manager
- [ ] Ly-benzeri minimal TUI/CLI giriş
- [ ] Alternatif: GTK ile grafik giriş
- [ ] Session seçimi (Gnuchan, Xfce, vs.)
- [ ] Kullanıcı listesi
- [ ] Auto-login
- [ ] **Tema sistemi desteği**

### 2.4 Gnuchan File Manager
- [ ] `gnuchan file manager` — GCL File Manager
- [ ] GTK tabanlı
- [ ] Çift panel (Midnight Commander benzeri)
- [ ] İkon görünümü, liste görünümü
- [ ] Sekmeli gezinti
- [ ] Dahili terminal (GCL Terminal ile)
- [ ] Dosya önizleme (metin, resim, video)
- [ ] Git entegrasyonu
- [ ] **Tema sistemi desteği**

### 2.5 Gnuchan Terminal
- [ ] `gnuchan xterm` — GCL Terminal
- [ ] xterm tabanlı ama gelişmiş
- [ ] Sekmeli terminal
- [ ] Bölünmüş pencere (tmux-like)
- [ ] GPU hızlandırmalı render (GTK + OpenGL)
- [ ] Renk şeması ve tema desteği
- [ ] Font ligature support (Fira Code, etc.)
- [ ] True color support
- [ ] Clickable URLs
- [ ] GCL ile script edilebilir

### 2.6 Gnuchan Shell
- [ ] `gnuchan hard shell` — GCL Shell
- [ ] Bash tabanlı ama GCL ile genişletilmiş
- [ ] GCL syntax ile scripting (normal shell scripting yanında)
- [ ] Auto-completion (GCL type-aware)
- [ ] Syntax highlighting
- [ ] Pipeline görselleştirme
- [ ] Built-in GCL execution
- [ ] POSIX uyumlu (bash script'leri de çalıştırabilir)
- [ ] Prompt customize (GCL ile)

### 2.7 Bildirim Sistemi (Notification Daemon)
- [ ] `gnuchan notify` — Bildirim daemon
- [ ] GTK bildirim balonları
- [ ] Bildirim geçmişi
- [ ] Do Not Disturb modu
- [ ] Bildirim kategorileri ve filtreleme
- [ ] **Tema sistemi desteği**

### 2.8 Ekran Yönetimi
- [ ] `gnuchan display settings` — Ekran ayarları aracı
- [ ] Çözünürlük/yenileme hızı değiştirme
- [ ] Çoklu monitör düzeni (xrandr GCL wrapper)
- [ ] Ölçekleme (HiDPI desteği)
- [ ] Parlaklık/kontrast ayarları

---

## Faz 3 — Sistem Araçları & Altyapı

### 3.1 Temel Sistem Araçları (GCL ile, Bash üzerine)
> **Felsefe:** Bash'ten kurtulmak değil, Bash'in üzerine geliştirmek.
> Bu araçlar mevcut GNU coreutils'in yerini almaz — GCL ile yazılmış, Bash ile birleştirilebilir, pipe/blok içinde kullanılabilir GCL araçlarıdır. Bash scriptleri bunları çağırabilir, GCL araçları Bash komutlarını çalıştırabilir.

- [ ] `ls` — GCL directory listing (pipe uyumlu)
- [ ] `ps` — GCL process list
- [ ] `top` — GCL system monitor
- [ ] `find` — GCL file search
- [ ] `grep` — GCL text search
- [ ] `cat` — GCL file viewer
- [ ] `sed` — GCL stream editor
- [ ] `awk` — GCL text processor
- [ ] Hepsi GCL ile yazılacak, Bash ile birlikte çalışacak

### 3.2 Ağ Araçları
- [ ] `curl` — GCL HTTP client
- [ ] `ping` — GCL ICMP ping
- [ ] `nslookup` — GCL DNS lookup
- [ ] `netstat` — GCL network stats
- [ ] `ip` — GCL network config (iproute2 arayüzü)

### 3.3 Sistem Servisleri
- [ ] GCL ile yazılmış DHCP client
- [ ] GCL ile yazılmış DNS resolver (stub)
- [ ] GCL ile yazılmış NTP client
- [ ] GCL ile yazılmış Audio server (PipeWire GCL wrapper)
- [ ] GCL ile yazılmış Bluetooth yöneticisi (BlueZ GCL wrapper)
- [ ] GCL ile yazılmış Yazdırma yöneticisi (CUPS GCL wrapper)

### 3.4 Sistem Yönetimi
- [ ] **Kontrol Paneli** — `gnuchan control center` — GTK ile merkezi sistem yönetimi
  - Kullanıcı yönetimi (ekle/sil/yetkilendir)
  - Servis yönetimi (başlat/durdur/etkinleştir)
  - Depolama yönetimi (diskler, bölümler, mount)
  - Güç yönetimi (pil durumu, uyku, hazırda bekletme, kapa kapatma)
  - Ağ yönetimi (WiFi, ethernet, VPN, proxy)
  - Tarih/saat ayarları
  - Dil/klavye/bölge ayarları
  - Kullanıcı arayüzü ayarları (ölçek, yazı tipi, imleç)
  - Erişilebilirlik ayarları
  - Bluetooth ayarları
  - Yazıcı ayarları
- [ ] **Güç Yönetimi** — TLP benzeri güç profilleri
- [ ] **Yedekleme Aracı** — `gnuchan backup` — rsync/deja-dup benzeri

### 3.5 Geliştirme Araçları
- [ ] **GCL Debugger** — `gcl-dbg` — adım adım çalıştırma, breakpoint, değişken izleme
- [ ] **GCL LSP Server** — `gcl-lsp` — dil sunucusu (otomatik tamamlama, hata gösterme, tanıma git)
- [ ] **GCL Formatter** — `gcl-fmt` — kod biçimlendirici
- [ ] **GCL REPL** — interaktif kabuk (zaten `main.c`'de taslağı var)
- [ ] **Metin Editörü** — `gnuchan edit` — GTK ile gelişmiş metin editörü (VS Code hafifi)
  - Syntax highlighting (GCL, GCPY, C, Python, Lua, Bash...)
  - Dosya ağacı
  - Sekme desteği
  - GCL LSP entegrasyonu
  - Tema sistemi desteği
- [ ] **GCL SDK** — dil paketi, başlıklar, örnekler, dokümantasyon

### 3.6 Çoklu Ortam (Multimedia)
- [ ] **Resim Görüntüleyici** — `gnuchan image viewer` — GTK ile
- [ ] **Video Oynatıcı** — `gnuchan video player` — mpv/GStreamer GCL wrapper
- [ ] **Müzik Oynatıcı** — `gnuchan music player` — basit müzik çalar
- [ ] **Ekran Görüntüsü** — `gnuchan screenshot` — screenshot aracı
- [ ] **Ekran Kaydı** — `gnuchan screen recorder` — video kayıt aracı (ffmpeg wrapper)

### 3.7 Erişilebilirlik
- [ ] **Screen Reader** — Orca entegrasyonu
- [ ] **Yüksek Kontrast Teması** — sistem genelinde
- [ ] **Büyüteç** — ekran büyütme aracı
- [ ] **Klavye Kısayolları** — tam klavye ile yönetim
- [ ] ** Ekran Klavyesi** — GTK sanal klavye

---

## Faz 4 — Paket Yönetimi, Bootloader & Dağıtım

### 4.1 Gnuchan Package Manager (gpm)
- [ ] `gpm` — GCL Package Manager
- [ ] GitHub odaklı (repo = GitHub repo): `gpm install kullanici/repo`
- [ ] `gpm search kelime`
- [ ] `gpm update`
- [ ] `gpm remove paket`
- [ ] `gpm list`
- [ ] `gpm info paket`
- [ ] `gpm pacman` — paket yöneticisi Pac-Man animasyonu 🟡
- [ ] Manifest formatı: `.gcpackage` (GCL config)
- [ ] Dependency resolution
- [ ] Version pinning
- [ ] Atomic updates (snapshot/rollback)
- [ ] İkili paket cache (GitHub releases)
- [ ] Kaynak koddan derleme
- [ ] Guix altyapısını kullanabilir (arka planda)
- [ ] GPG imza doğrulama
- [ ] GCL ile yazılacak

### 4.2 Bootloader Yönetimi
- [ ] GRUB kurulum/kurtarma/kaldırma aracı
- [ ] Çoklu boot yapılandırması (Windows, diğer Linux'lar)
- [ ] GRUB tema yönetimi
- [ ] Secure Boot desteği
- [ ] Kernel parametre yönetimi
- [ ] `gnuchan boot config` — GRUB yapılandırma editörü

### 4.3 ISO / Dağıtım Builder
- [ ] `gcl-mkiso` — ISO oluşturma aracı
- [ ] Customizable boot screen (GRUB tema)
- [ ] Customizable splash screen (Plymouth)
- [ ] Live USB desteği (kalıcı depolamalı)
- [ ] Calamares benzeri kurulum sihirbazı
- [ ] Disk şifreleme ile kurulum seçeneği
- [ ] Bölümleme arayüzü (GParted-like GCL-GTK)
- [ ] **Kurtarma ortamı** — bozuk sistemi onarma modu

---

## Faz 5 — Tema & Kişiselleştirme Sistemi

> **İlke:** Sistemdeki HER program tema sistemini destekleyecek.

### 5.1 Tema Motoru
- [ ] `gcl-theme` — Merkezi tema motoru
- [ ] Temel dosya formatı: `.gctheme` (GCL config)
- [ ] Renk paleti tanımlama
- [ ] Yazı tipi tanımlama
- [ ] İkon seti tanımlama
- [ ] GTK CSS tema
- [ ] WM bar/panel renkleri
- [ ] Terminal renk şeması
- [ ] File manager renkleri
- [ ] DM (login screen) teması
- [ ] Metin editörü teması
- [ ] Global font ayarları
- [ ] İmleç teması
- [ ] Ses teması (system sounds)

### 5.2 İkon Oluşturma Yazılımı
- [ ] `gcl-icon-maker` — İkon tasarım aracı
- [ ] GTK ile çizim arayüzü
- [ ] SVG export
- [ ] PNG export
- [ ] İkon seti paketleme
- [ ] Tema ile entegrasyon

### 5.3 Tema Oluşturma Yazılımı
- [ ] `gcl-theme-creator` — Tema tasarım aracı
- [ ] WYSIWYG tema editörü
- [ ] Canlı önizleme (anında tema değişimi)
- [ ] Renk paleti oluşturucu
- [ ] Tema paylaşım (GitHub)

### 5.4 GRUB Boot Screen
- [ ] GRUB tema desteği (GCL ile özelleştirilebilir)
- [ ] GRUB renkleri, arka plan, font
- [ ] Çoklu boot görüntüsü

### 5.5 Plymouth Boot Splash
- [ ] Boot animasyonu değiştirme aracı
- [ ] GCL ile özel boot animasyonu

---

## Faz 6 — Güvenlik & Gizlilik

### 6.1 Sistem Güvenliği
- [ ] Linux Security Module (AppArmor / SELinux) profili
- [ ] Kernel hardening (KASLR, SMAP, SMEP, PTI)
- [ ] Kernel module signing
- [ ] Secure Boot entegrasyonu
- [ ] Firewall (`nftables` GCL wrapper) — `gnuchan firewall`
- [ ] Disk şifreleme (LUKS) kurulum aracı
- [ ] GPG imzalama ve doğrulama altyapısı
- [ ] Audit log sistemi

### 6.2 Uygulama Güvenliği
- [ ] Sandbox (bubblewrap / firejail entegrasyonu)
- [ ] GCL güvenli dil kontrolleri (buffer overflow, use-after-free, integer overflow)
- [ ] Permission sistemi (Android/iOS benzeri izin modeli)
- [ ] Flatpak/AppImage güvenlik politikası

### 6.3 Gizlilik
- [ ] Varsayılan DNS over HTTPS
- [ ] Varsayılan tracker/kayıt kapalı
- [ ] Ağ aktivite monitörü
- [ ] Microphone/kamera erişim kontrolü

---

## Faz 7 — Release & Sürekli Geliştirme

### 7.1 Alpha Sürümü
- [ ] GCL dili: Lexer + Parser + GCPY VM çalışıyor
- [ ] GCL dili: GCL Codegen + GCC çalışıyor
- [ ] Runtime safety sistemi çalışıyor
- [ ] `gcl -run` ve `gcl -build` tam çalışıyor
- [ ] Örnek programlar: merhaba dünya, fibonacci, fizzbuzz, snake game

### 7.2 Beta Sürümü (GnuchanOS Desktop)
- [ ] GTK wrapper tamam
- [ ] Gnuchan WM çalışıyor
- [ ] Gnuchan DM çalışıyor
- [ ] Gnuchan Terminal çalışıyor
- [ ] Gnuchan Shell çalışıyor
- [ ] GCL ile yazılmış temel sistem araçları
- [ ] Paket yöneticisi çalışıyor
- [ ] Tema sistemi çalışıyor
- [ ] Metin editörü çalışıyor

### 7.3 Stable Sürümü (GnuchanOS 1.0)
- [ ] Tam masaüstü ortamı
- [ ] Tüm sistem araçları GCL ile yazılmış
- [ ] libre-Systemd çalışıyor
- [ ] xlibre çalışıyor
- [ ] GCL Driver Framework çalışıyor
- [ ] ISO builder çalışıyor
- [ ] Kurulum sihirbazı çalışıyor
- [ ] Tema + ikon araçları çalışıyor
- [ ] Firewall + güvenlik altyapısı çalışıyor
- [ ] Dökümantasyon tamam

### 7.4 Sürekli
- [ ] Performance optimization (GCL VM JIT?)
- [ ] WASM target
- [ ] Cross-compilation
- [ ] Android/iOS backend (GCL -> Java/Swift)
- [ ] LLVM IR backend
- [ ] GCL -> GPU shader compilation
- [ ] Community package repository
- [ ] Flatpak/AppImage support

---

## Teknik Borç & Altyapı

### Build Sistemi
- [ ] `build.py` → `build.gcl` (GCL ile build)
- [ ] Makefile alternatifi
- [ ] Cross-compilation desteği

### Test
- [ ] Unit test framework (GCL ile)
- [ ] CI/CD (GitHub Actions)
- [ ] Integration tests
- [ ] Benchmark suite

### Dökümantasyon
- [ ] GCL dil referansı
- [ ] GCPY dil referansı
- [ ] API dökümantasyonu
- [ ] GnuchanOS kurulum rehberi
- [ ] GnuchanOS geliştirici rehberi
- [ ] Tema oluşturma rehberi

---

## 🔤 Kısaltmalar & Terminoloji

| Kısaltma | Açılım |
|----------|--------|
| GCL | Gnuchan C-Like (derlenen dil) |
| GCPY | Gnuchan Python-Like (yorumlanan dil) |
| GCL-VM | Gnuchan Virtual Machine (bytecode interpreter) |
| WM | Window Manager |
| DM | Display Manager |
| gpm | Gnuchan Package Manager |
| GTK | GIMP Toolkit (GUI framework) |
| Guix | GNU Guix package manager |
| libre-Systemd | Özgür init sistemi |
| xlibre | Özgür display server |
| LSP | Language Server Protocol |
| LUKS | Linux Unified Key Setup |

---

## 🎯 Öncelik Sırası (Kısa Vade)

> Şu anki kod tabanına bakarak **en kritik ilk adımlar:**

1. **Lexer** (ortak) — token altyapısı
2. **Parser** (ortak) — AST ağacı
3. **GCPY Bytecode + VM** — `-run` çalışsın
4. **GCL Codegen** — `-build` çalışsın
5. **Runtime Safety** — memory error'lar netsin
6. **GTK Wrapper** — GUI kapısı açılsın
7. **Gnuchan Terminal** — ilk çalışan GCL-GTK uygulaması
8. **Gnuchan WM** — pencere yöneticisi
9. **Gnuchan DM** — giriş ekranı
10. **Package Manager** — dağıtım altyapısı

---

*Son güncelleme: 2026-07-17*
*Bu belge GnuchanOS geliştirme sürecinde güncellenecektir.*
