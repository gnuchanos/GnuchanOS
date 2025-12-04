#!/bin/bash

# Çalışma dizinini ayarlıyoruz
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
cd "$SCRIPT_DIR"

# zram-generator paketini yüklüyoruz
sudo pacman -Sy --noconfirm zram-generator

# Yapılandırma dosyasını /etc/systemd/ dizinine kopyalıyoruz
sudo cp files/zram-generator.conf /etc/systemd/

# Systemd daemon'larını yeniden yüklüyoruz
sudo systemctl daemon-reload

# /dev/zram0'ı başlatıyoruz
sudo systemctl start /dev/zram0

# zramctl komutunu çalıştırarak durumu kontrol ediyoruz
zramctl