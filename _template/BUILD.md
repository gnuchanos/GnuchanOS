# GnuchanOS ISO Build Rehberi

## Seçenek 1: WSL2 (Önerilen — Windows üzerinde)

### Adım 1: WSL2 + Debian Kur
```powershell
# PowerShell (Admin) — eğer yoksa:
wsl --install -d Debian
wsl --set-default-version 2

# WSL2'de minimum 20 GB ayır (C: doluysa D:'ye taşı)
# D:\WSL\ dizinine kurulum:
# (Araştır: "move WSL to another drive")
```

### Adım 2: WSL2'de build ortamını hazırla
```bash
# WSL2 içinde:
cd /mnt/d/GnuchanOS/_template/scripts

# Build öncesi env var'ları (C: dolu → D: tempWork kullan)
export TMPDIR=/mnt/d/GnuchanOS/_tempWork/tmp
export CCACHE_DIR=/mnt/d/GnuchanOS/_tempWork/ccache
export PIP_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/pip
export APT_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/apt
export GIT_CLONE_DIR=/mnt/d/GnuchanOS/_tempWork/git
export DEBOOTSTRAP_DIR=/mnt/d/GnuchanOS/_tempWork/debootstrap

# Build araçlarını kur (tek seferlik)
sudo bash setup-build-env.sh
```

### Adım 3: ISO Build
```bash
# Tam build (kernel, systemd, xserver hepsi derlenir — ~1-2 saat)
sudo bash build-all.sh

# Veya hızlı build (kernel/systemd/xserver skip — Debian paketleri kullanılır)
sudo bash build-all.sh --skip-kernel --skip-liberated --skip-xlibre

# Sadece belirli script'ten devam etmek için:
sudo bash build-all.sh --resume 8
```

### Adım 4: ISO'yu test et
ISO çıktısı: `/mnt/d/GnuchanOS/_template/iso/gnuchanos.iso`
Bunu VirtualBox ile boot et.

---

## Seçenek 2: Debian VM (VirtualBox)

### Adım 1: Debian 12 VM kur
- **RAM:** 4 GB+
- **Disk:** 20 GB+
- **Network:** NAT (internet erişimi için)
- **ISO:** debian-12.x.x-amd64-netinst.iso

### Adım 2: VM içinde
```bash
# D: sürücüsünü bağla (VirtualBox Paylaşımlı Klasör)
sudo mkdir -p /mnt/gnuchan
sudo mount -t 9p -o trans=virtio,version=9p2000.L /mnt/gnuchan

# Build ortamı
cd /mnt/gnuchan/_template/scripts
sudo bash setup-build-env.sh

# Build
sudo bash build-all.sh
```

---

## Gereken Minimum Paketler (setup-build-env.sh içeriği)

| Paket | Ne İçin |
|-------|---------|
| `debootstrap` | Rootfs oluşturma |
| `squashfs-tools` | `mksquashfs` — ISO sıkıştırma |
| `xorriso` | Hybrid ISO oluşturma |
| `grub-pc-bin grub-efi-amd64-bin` | BIOS+EFI boot |
| `isolinux syslinux-common` | BIOS fallback |
| `build-essential gcc make meson ninja-build` | Kernel, systemd, xserver derleme |
| `python3 python3-pip` | QTile, GnuChanGUI |
| `libpixman-1-dev libepoxy-dev libdrm-dev libgbm-dev` | X11Libre bağımlılıkları |
| `libxcb*-dev libxcvt-dev libxfont2-dev` | X11Libre bağımlılıkları |
| `bison flex bc kmod cpio` | Kernel build |
| `git wget curl` | Kaynak indirme |

---

## Hızlı Başlangıç (Kısa Yol)

WSL2'de tek seferde:

```bash
# 1. WSL2 Debian'a gir
wsl -d Debian

# 2. Build araçlarını kur
cd /mnt/d/GnuchanOS/_template/scripts
sudo bash setup-build-env.sh

# 3. Ortam değişkenlerini ayarla (kopyala yapıştır)
export TMPDIR=/mnt/d/GnuchanOS/_tempWork/tmp
export CCACHE_DIR=/mnt/d/GnuchanOS/_tempWork/ccache
export PIP_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/pip
export APT_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/apt
export GIT_CLONE_DIR=/mnt/d/GnuchanOS/_tempWork/git
export DEBOOTSTRAP_DIR=/mnt/d/GnuchanOS/_tempWork/debootstrap

# 4. Build (kernel falan beklemeden hızlı ISO almak için --skip)
sudo bash build-all.sh --skip-kernel --skip-liberated --skip-xlibre

# 5. ISO hazır!
ls -lh /mnt/d/GnuchanOS/_template/iso/gnuchanos.iso
```

---

## Önemli Notlar

- **C: dolu → build D:'de yapılır.** `_tempWork/` tüm geçici dosyalar içindir.
- **build-all.sh öncesi env var'larını set etmeyi unutma.** Aksi halde C: dolar.
- **--skip-* flagleri** custom kernel/systemd/xserver derlemesini atlar, Debian paketlerini kullanır. İlk denemede bunu kullan.
- **ilk build ~1-2 saat**, sonraki build'ler cache sayesinde daha hızlı.
- **Minimum 10 GB boş disk** gerekli (rootfs + kernel build + ISO).
