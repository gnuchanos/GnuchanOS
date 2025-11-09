#!/bin/bash

# intel driver
# vulkan-intel lib32-vulkan-intel 
# sudo pacman -S --needed lib32-mesa vulkan-icd-loader lib32-vulkan-icd-loader

# Wine ve Gerekli Bileşenler
echo "Wine ve gerekli bileşenler yükleniyor..."
echo "Installing Wine and required components..."
sudo pacman -S wine-cachyos wine-gecko wine-mono gstreamer gst-plugins-base gst-plugins-good

# Wine için Ek Bağımlılıklar
echo "Wine için ek bağımlılıklar yükleniyor..."
echo "Installing additional dependencies for Wine..."
sudo pacman -S --needed --asdeps giflib lib32-giflib gnutls lib32-gnutls v4l-utils lib32-v4l-utils \
libpulse lib32-libpulse alsa-plugins lib32-alsa-plugins alsa-lib lib32-alsa-lib sqlite lib32-sqlite \
libxcomposite lib32-libxcomposite ocl-icd lib32-ocl-icd libva lib32-libva gtk3 lib32-gtk3 \
gst-plugins-base-libs lib32-gst-plugins-base-libs vulkan-icd-loader lib32-vulkan-icd-loader \
sdl2 lib32-sdl2

# Proton ve CachyOS Gaming Uygulamaları
echo "Proton ve gaming uygulamaları yükleniyor..."
echo "Installing Proton and CachyOS gaming applications..."
sudo pacman -Sy steam heroic-games-launcher-bin 
sudo pacman -Sy glfw lib32-libjpeg-turbo 
sudo pacman -Sy lib32-mpg123 lib32-opencl-icd-loader lib32-openal libjpeg-turbo libxslt mpg123 opencl-icd-loader openal  
sudo pacman -Sy proton-cachyos proton-cachyos-slr protontricks ttf-liberation wine-cachyos-opt winetricks vulkan-tools


# GStreamer Plugin Paketleri
echo "GStreamer plugin'leri yükleniyor..."
echo "Installing GStreamer plugin packages..."
yay -Sy gst-plugins-{base,good,bad,ugly}

# Oyunlar İçin Gerekli Paketler
echo "Oyunlar için gerekli paketler yükleniyor..."
echo "Installing required packages for games..."
sudo pacman -S --noconfirm kdialog
yay -Sy gamemode lib32-gamemode winetricks

echo "Tüm gerekli paketler yüklendi!"
echo "All required packages have been installed!"
