#!bin/bash




# 1. ZeroTier paketini yükle (resmi reposunda var)
sudo pacman -S zerotier-one

# 2. ZeroTier servisini başlat ve otomatik başlatmaya ekle
sudo systemctl enable --now zerotier-one.service

# 3. ZeroTier durumunu kontrol et
sudo systemctl status zerotier-one.service




echo "sudo zerotier-cli join network_id"
echo "sudo zerotier-cli listnetworks"




