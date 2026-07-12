# GnuchanOS

## Kurulum Notları

- **WSL**: `D:\WSL\Debian` — build almak için Debian WSL kullanılıyor
- **Language**: `D:\GnuchanOS\language`

> **Önemli**: Build Debian üzerinde yapılıyor ama GnuchanOS **Debian tabanlı değil**. Amaç diğer dağıtımların sorunlarını çözmek değil, kendi sorunlarımı çözmek.

## Hedef

`gcl`, GnuchanOS'un sistem dili olacak (C99 altkümesi). Şunları kapsayacak:
- Terminal emulator, file manager, ve gereken her şey
- Tiling + window manager, display manager
- Bash tabanlı shell — fish shell gibi rahat kullanım
- GTK ve OpenGL

> **Not**: Based distro projesi değil. Amaç kendi sistemini yapmak ama her şeyi sıfırdan yazmamak.

---

## Paket Yönetimi

| Özellik | Detay |
|---|---|
| Backend | `pacman` |
| Repository | Arch Linux |
| Tag | `gnuchos-managed` |

### Komutlar

- `Install`
- `Remove`
- `Update`
- `Search`

### Gelecek

- Kendi repository

---

## Kurallar

- ✅ Sadece user-space uygulamalar
- ❌ Kernel kurulumu yok
- ❌ Bootloader kurulumu yok
- ❌ Init kurulumu yok
- ❌ Sistem kütüphaneleri değiştirilmez
- ❌ `/usr/bin` sistem araçları üzerine yazılmaz

---

## Grafik Yığını

- Mesa
- libdrm
- libinput
- xkbcommon
- Fontconfig
- FreeType
- HarfBuzz
- Vulkan Loader (gelecek)

---

## Sistem Mimarisi

```
Bootloader → GRUB
↓
Linux Kernel
↓
Libre-SystemD
↓
musl / glibc
↓
eudev
↓
XLibre
↓
Kendi Display Manager (GTK tabanlı, minimal, özelleştirilebilir)
  → theme, background, widget, animation
↓
Kendi Window Manager (OpenGL odaklı)
  → burn effect, fade-in, fade-out
  → Tiling sistem ana sistem
  → Floating openbox-like ekstra mod
  → Kısayollar:
    - Super + ←/↑/→/↓ = focus
    - Super+Shift + ←/↑/→/↓ = move
    - Shift+Alt + sol mouse = float mode
  → Qtile benzeri bar sistemi
    - settings menu (change colors, add widget, separator)
```

---

## Uygulamalar

### Format

- AppImage

### Varsayılan Uygulamalar

- Terminal
- File Manager
- Text Editor
- Image Viewer
- Browser
- Archive Manager

### Developer Araçları

- GCC
- Python
- Git
- CMake

---

## GCL Durumu (2026-07-11)

`gcl` artık **saf C99 lexer + parser**. gcLang'e özel her şey çıkarıldı:

| Özellik | Durum |
|---------|-------|
| C99 lexer (tokenizer) | ✅ |
| C99 parser (AST builder) | ✅ |
| `--parse <file>` | ✅ |
| `-ast <file>` | ✅ |
| `-version` / `-help` | ✅ |
| gcLang @lib/@include/@extern | ❌ kaldırıldı |
| IR executor (-run) | ❌ kaldırıldı |
| Build pipeline (-build) | ❌ kaldırıldı |
| Lua runner (-luarun) | ❌ kaldırıldı |
| Debug mode (-debug) | ❌ kaldırıldı |
| Extra comment stilleri | ❌ kaldırıldı (sadece // /* */) |
| Self-host (kendi kendini derleme) | ⏳ sıradaki adım |
