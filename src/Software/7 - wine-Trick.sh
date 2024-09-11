#!/bin/bash

# intel driver
# vulkan-intel lib32-vulkan-intel 
sudo pacman -S --needed lib32-mesa vulkan-icd-loader lib32-vulkan-icd-loader

# wine
sudo pacman -S wine-staging
sudo pacman -S --needed --asdeps giflib lib32-giflib gnutls lib32-gnutls v4l-utils lib32-v4l-utils libpulse \
lib32-libpulse alsa-plugins lib32-alsa-plugins alsa-lib lib32-alsa-lib sqlite lib32-sqlite libxcomposite \
lib32-libxcomposite ocl-icd lib32-ocl-icd libva lib32-libva gtk3 lib32-gtk3 gst-plugins-base-libs \
lib32-gst-plugins-base-libs vulkan-icd-loader lib32-vulkan-icd-loader sdl2 lib32-sdl2

#games
sudo pacman -S --noconfirm kdialog
yay -Sy  gamemode lib32-gamemode protonup-qt mangohud lib32-mangohud 

#1024x768 -> 1024x655

winecfg

WINEPREFIX=~/.wine WINEARCH=win32 wine winecfg
WINEPREFIX=~/.wine64 WINEARCH=win64 wine winecfg

echo "WINEPREFIX=~/.wine64 winecfg"
echo "WINEPREFIX=~/.wine64 winetricks"
echo "WINEPREFIX=~/.wine64 wine64" # or wine for 32bit

#mangohud --dlsym for opengl # MANGOHUD=1 for vulkan
#WINEPREFIX=/home/archkubi/.wine/ primusrun gamemoderun mangohud --dlsym wine falloutwHR.exe

#WINEPREFIX=/home/archkubi/.wine64/ optirun wine Godot_v3.0-stable_win64.exe
# for tf2 -> in console -> tf_time_loading_item_panels 0.0005

#Start Minecraft
#gamemoderun mangohud --dlsym java -jar TLauncher.jar

#primusrun gamemoderun mangohud --dlsym %command% 

# lutris heroicGames steam
# steam > PROTON_USE_WINED3D=1 primusrun gamemoderun mangohud --dlsym %command% -gl
PROTON_USE_WINED3D=1 vblank_mode=0 primusrun gamemoderun mangohud --dlsym %command% -gl