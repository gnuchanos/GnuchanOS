#!/bin/bash



echo "Wine ve gerekli bileşenler yükleniyor..." 
sudo pacman -S --noconfirm wine-staging wine-gecko wine-mono gstreamer gst-plugins-base gst-plugins-good 

echo "Wine için ek bağımlılıklar yükleniyor..." 
sudo pacman -S --noconfirm --needed giflib lib32-giflib gnutls lib32-gnutls v4l-utils lib32-v4l-utils \ 
	libpulse lib32-libpulse alsa-plugins lib32-alsa-plugins alsa-lib lib32-alsa-lib sqlite lib32-sqlite \ 
	libxcomposite lib32-libxcomposite ocl-icd lib32-ocl-icd libva lib32-libva gtk3 lib32-gtk3 \ 
	gst-plugins-base-libs lib32-gst-plugins-base-libs vulkan-icd-loader lib32-vulkan-icd-loader \ 
	sdl2 lib32-sdl2 

echo "GStreamer plugin'leri yükleniyor..." 
sudo pacman -S --noconfirm gst-plugins-bad gst-plugins-ugly 

echo "Oyunlar için gerekli paketler yükleniyor..." sudo pacman -S --noconfirm kdialog 
# AUR paketleri 
yay -S --noconfirm gamemode lib32-gamemode winetricks mangohud lib32-mangohud
