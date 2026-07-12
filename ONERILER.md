# GnuchanOS — Öneriler


gcl-py -> self host ismi

ocaml -> gcl-py (bu dilin amaci kolay bir sekilde gcl compilerini gelistirmek bu kadar) -> gcl

D:\GnuchanOS\language\self_hosting
D:\GnuchanOS\language\self_hosting\todo.md tamamlanana kadar durma be arkadas
projeyi olustur todo.md de olanlari ocaml ile gelistir extra baska bir sey yapma



## 1. Dizin Yapısı

Şu an her şey `D:\GnuchanOS` altında düz duruyor. İleride bileşenler çoğaldıkça şöyle bir yapı öneririm:

```
GnuchanOS/
├── language/          # GCL derleyici — zaten var
├── wm/                # Window manager (OpenGL)
├── dm/                # Display manager (GTK)
├── shell/             # Shell (fish-like)
├── terminal/          # Terminal emulator
├── file-manager/      # File manager
├── packages/          # Paket yönetimi araçları
├── scripts/           # Build ve kurulum scriptleri
├── ONERILER.md
├── gnuchanos.md
└── Makefile           # Üst seviye build (tüm bileşenleri build alır)
```

## 2. Öncelik Sırası (Ne önce gelmeli?)

### 1. Aşama — GCL Dilini Oturtmak (şu an burdasın)
- Lexer + Parser stabil çalışıyor
- Sırada: AST yürütücü (interpreter/compiler), standart kütüphane
- GCL kendi kendini host edebilir hale gelmeli (self-hosting compiler)

### 2. Aşama — Shell
- Bash tabanlı ama fish gibi otomatik önerme, syntax highlighting
- GCL ile yazılabilir mi? Başlangıçta Python/C ile prototip, sonra GCL'e taşı

### 3. Aşama — Window Manager
- OpenGL, tiling odaklı
- Qtile benzeri bar sistemi
- Kısayollar zaten belirlenmiş (`super + yön`, `shift+alt+click`)
- libinput, xkbcommon ile input yönetimi

### 4. Aşama — Display Manager
- GTK tabanlı, minimal
- Theme, background, widget, animation

### 5. Aşama — Uygulamalar
- Terminal, file manager, text editor...
- Her biri ayrı bir repo/bileşen olarak geliştirilebilir

## 3. Build Sistemi

- Her bileşenin kendi Makefile'ı olmalı
- Üst seviye bir Makefile tüm bileşenleri sırayla build almalı
- Örnek üst Makefile:
```make
all:
	cd language && make
	cd wm && make
	cd shell && make
```

## 4. Debian WSL Kullanımı

Şu an build Debian WSL'de alınıyor. Bunun için:
- `scripts/build.sh` — tüm build işlemini otomatize eder
- `scripts/install.sh` — çıktıları hedef sisteme kurar
- WSL içinde cross-compilation gerekmez, native build yeterli

## 6. Test

- GCL için bir test framework'ü şart
- `make test` ile her build'den sonra testler otomatik çalışmalı
- WM/DM için Xvfb/sanal ekran ile headless test

## 7. CI/CD (Gelecek)

- GitHub Actions ile her push'ta build + test
- WSL'de build alınıyorsa, GitHub runner'da da aynı ortam kurulabilir

---

## Özet — Hemen Şimdi Ne Yapılabilir?

1. Proje dizin yapısını yukarıdaki gibi düzenle
2. GCL'de AST yürütücüye devam et (interpreter)
3. Shell prototipi için basit bir readline loop yaz (Python ile olabilir)
4. `scripts/` klasörü aç, build/install scriptleri ekle
5. `make test` hedefi ekle (GCL için test dosyaları)
