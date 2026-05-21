# GnuchanOS TODO

## 1. Distro ISO Build ve Boot Düzeltmeleri
- `src/Distro/scripts/build-iso.sh` içinde `--clean` desteğini doğrula ve otomatik temizlenen çalışma dizinini düzelt.
- `src/Distro/profile/customize_airootfs.sh` dosyasını root hesabı, `/etc/shadow`, `/etc/passwd` ve `/etc/group` oluşturma için güvenli hale getir.
- `src/Distro/profile/airootfs/etc/fstab` ekle veya güncelle; canlı ISO için gerekli tmpfs/proc/sys/udev girişlerini sağla.
- `src/Distro/profile/profiledef.sh` ve `src/Distro/profile/packages.x86_64` içeriğini gözden geçir; paket listesi ve boot modları uyumluluğunu kontrol et.
- `src/Distro/profile/syslinux/` yapılandırmalarını test et ve BIOS/UEFI geçişini doğrula.

## 2. Canlı Ortam ve Branding
- `src/Distro/scripts/sync-branding-assets.sh` çalışmasını doğrula ve `bg.png` / `logo.png` branding senkronizasyonunu sorunsuz hale getir.
- `src/Dotfile/gnuchanBoot`, `plymouthd.conf`, `qtile/config.py` ve masaüstü ayarlarını canlı ISO ile uyumlu hâle getir.
- LXDm tema, Qtile arka planı ve diğer dotfile bileşenlerini ISO içine yerleştir.

## 3. Belgeler ve CI
- `README.md` içinde ISO derleme adımlarını güncelle; Docker komutu `--clean` ve gerekli `--privileged` kullanımını açıkla.
- `.github/workflows/build-distro.yml` dosyasını `--clean` ekleyerek ve çıktıyı `src/Distro/out/*.iso` olarak güncelle.
- Bu `TODO.md` belgesini güncel tut; yeni yapılacakları kısa ve açık tut.

## 4. İlgili Araçlar ve Yardımcı Scriptler
- `src/install_zapret.sh` ve `src/uninstall_zpret.sh` betiklerinin debug/installation flow'unu gözden geçir.
- `src/Dotfile/gnu_pkg_lists.py` ile `src/Distro/profile/packages.x86_64` arasındaki paket listesi senkronizasyonunu doğrula.

## 5. İleri Düzey İyileştirmeler
- Canlı ISO açılışında `Switch Root` hatalarını azaltmak için initramfs ve systemd servis yapılandırmasını kontrol et.
- Canlı ortamda recovery/emergency mod kullanımı için root erişimi ve `sulogin` düzenlemelerini güvenli hale getir.
- Arşivleme/ISO çıktısı sonrası sonuç dosyalarını otomatik temizleyen bir script ekle.
