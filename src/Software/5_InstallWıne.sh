#!/bin/bash

# intel driver
# vulkan-intel lib32-vulkan-intel 
# sudo pacman -S --needed lib32-mesa vulkan-icd-loader lib32-vulkan-icd-loader

# wine
sudo pacman -S  wine-cachyos wine-gecko  wine-mono gstreamer gst-plugins-base gst-plugins-good

sudo pacman -S --needed --asdeps giflib lib32-giflib gnutls lib32-gnutls v4l-utils lib32-v4l-utils libpulse \
lib32-libpulse alsa-plugins lib32-alsa-plugins alsa-lib lib32-alsa-lib sqlite lib32-sqlite libxcomposite \
lib32-libxcomposite ocl-icd lib32-ocl-icd libva lib32-libva gtk3 lib32-gtk3 gst-plugins-base-libs \
lib32-gst-plugins-base-libs vulkan-icd-loader lib32-vulkan-icd-loader sdl2 lib32-sdl2

yay -Sy gst-plugins-{base,good,bad,ugly}

#games
sudo pacman -S --noconfirm kdialog
yay -Sy  gamemode lib32-gamemode mangohud lib32-mangohud winetricks
sudo pacman -Sy steam heroic-games-launcher gamescope
winecfg

winetricks -q corefonts dxvk vkd3d  d3dcompiler_43 cnc_ddraw vcrun2019 vcrun2015 d3dx9 vcrun2019 directx9 mfc42 corefonts vcrun6 quicktime72 quicktime76 cjkfonts faudio dotnet20 vcrun2008 icodecs vcrun6 vcrun2010 msxml3 vcrun2010 dotnet40 physx wmp9 wmp10 wsh57 dxdiagn dxdiag quartz devenum wmp10

#mangohud --dlsym for opengl # MANGOHUD=1 for vulkan
#WINEPREFIX=/home/archkubi/.wine/ primusrun gamemoderun mangohud --dlsym wine falloutwHR.exe

#WINEPREFIX=/home/archkubi/.wine64/ optirun wine Godot_v3.0-stable_win64.exe
# for tf2 -> in console -> tf_time_loading_item_panels 0.0005

#Start Minecraft
#gamemoderun mangohud --dlsym %command% 



# lutris heroicGames steam
# steam > PROTON_USE_WINED3D=1 gamemoderun mangohud --dlsym %command% -gl
# PROTON_USE_WINED3D=1 vblank_mode=0  gamemoderun mangohud --dlsym %command% -gl