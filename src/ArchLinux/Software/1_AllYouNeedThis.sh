#!/bin/bash

# Pacman Logo
sudo sed -i 's/^#Color/Color/g' /etc/pacman.conf
sudo sed -i 's/VerbosePkgLists/VerbosePkgLists\nILoveCandy/g' /etc/pacman.conf

# 
echo "#[multilib]"
echo "#Include = /etc/pacman.d/mirrorlist"
read -p "Don't forget remove this -> # <Press Enter>"
sudo nano /etc/pacman.conf
sudo pacman -Syu --noconfirm archlinux-keyring xorg-xinput

# Install all packages
sudo pacman -Sy --noconfirm zip
sudo pacman -Sy --noconfirm unzip unrar
sudo pacman -Sy --noconfirm p7zip
sudo pacman -Sy --noconfirm expac
sudo pacman -Sy --noconfirm jshon
sudo pacman -Sy --noconfirm gvfs-mtp
sudo pacman -Sy --noconfirm mtpfs
sudo pacman -Sy --noconfirm exfat-utils
sudo pacman -Sy --noconfirm a52dec
sudo pacman -Sy --noconfirm faac
sudo pacman -Sy --noconfirm fuse-exfat
sudo pacman -Sy --noconfirm faad2
sudo pacman -Sy --noconfirm jasper
sudo pacman -Sy --noconfirm lame
sudo pacman -Sy --noconfirm libdca
sudo pacman -Sy --noconfirm libdv

sudo pacman -Sy --noconfirm gst-libav
sudo pacman -Sy --noconfirm libmad
sudo pacman -Sy --noconfirm libtheora
sudo pacman -Sy --noconfirm libmpeg2
sudo pacman -Sy --noconfirm wavpack
sudo pacman -Sy --noconfirm x264
sudo pacman -Sy --noconfirm xvidcore
sudo pacman -Sy --noconfirm libdvdcss
sudo pacman -Sy --noconfirm libdvdread
sudo pacman -Sy --noconfirm libdvdnav
sudo pacman -Sy --noconfirm dvd+rw-tools

sudo pacman -Sy --noconfirm dvdauthor
sudo pacman -Sy --noconfirm dvgrab
sudo pacman -Sy --noconfirm lib32-alsa-lib
sudo pacman -Sy --noconfirm lib32-alsa-plugins
sudo pacman -Sy --noconfirm lib32-libpulse
sudo pacman -Sy --noconfirm lib32-alsa-oss
sudo pacman -Sy --noconfirm net-tools
sudo pacman -Sy --noconfirm xsel

sudo pacman -Sy --noconfirm pcre
sudo pacman -Sy --noconfirm pcre2
sudo pacman -Sy --noconfirm lib32-pcre
sudo pacman -Sy --noconfirm lib32-pcre2
sudo pacman -Sy --noconfirm util-linux
sudo pacman -Sy --noconfirm util-linux-libs
sudo pacman -Sy --noconfirm lib32-util-linux
sudo pacman -Sy --noconfirm xz
sudo pacman -Sy --noconfirm lib32-xz

sudo pacman -Sy --noconfirm gparted
sudo pacman -Sy --noconfirm vlc
sudo pacman -Sy --noconfirm conky
sudo pacman -Sy --noconfirm leafpad
sudo pacman -Sy --noconfirm arandr
sudo pacman -Sy --noconfirm btop
sudo pacman -Sy --noconfirm jdk-openjdk
sudo pacman -Sy --noconfirm bchunk
sudo pacman -Sy --noconfirm dmenu
sudo pacman -Sy --noconfirm rofi
sudo pacman -Sy --noconfirm fastfetch
sudo pacman -Sy --noconfirm make
sudo pacman -Sy --noconfirm cmake
sudo pacman -Sy --noconfirm openssh
sudo pacman -Sy --noconfirm timidity
sudo pacman -Sy --noconfirm fail2ban
sudo pacman -Sy --noconfirm deluge-gtk
sudo pacman -Sy --nocomfirm mkinitcpio


# Install Personal Programs
sudo pacman -Sy --noconfirm ranger
sudo pacman -Sy --noconfirm cmus
sudo pacman -Sy --noconfirm dunst
sudo pacman -Sy --noconfirm nitrogen
sudo pacman -Sy --noconfirm zathura
sudo pacman -Sy --noconfirm zathura-pdf-poppler
sudo pacman -Sy --noconfirm ristretto
sudo pacman -Sy --noconfirm lxappearance
sudo pacman -Sy --noconfirm lxapperance-obconf
sudo pacman -Sy --noconfirm scrot
sudo pacman -Sy --noconfirm npm
sudo pacman -Sy --noconfirm nrg2iso
sudo pacman -Sy --noconfirm yt-dlp
sudo pacman -Sy --nocomfirm ncdu

sudo pacman -Sy --noconfirm joystick evtest
sudo pacman -S noto-fonts-cjk noto-fonts-emoji noto-fonts
sudo pacman -S xdg-desktop-portal xdg-desktop-portal-gtk



# System Optimisation
sudo pacman -Sy --noconfirm irqbalance
sudo pacman -Sy --noconfirm tlp
sudo pacman -Sy --noconfirm cpupower


sudo systemctl enable --now irqbalance
sudo systemctl enable --now tlp
sudo systemctl enable --now cpupower
sudo cpupower frequency-set -g ondemand


sudo cpupower frequency-set -g performance

# for ssd
sudo systemctl enable --now fstrim.timer
sudo fstrim -v /


# Connect SSH
sudo systemctl enable --now fail2ban
sudo systemctl start fail2ban

sudo pacman -S --noconfirm openssh
sudo systemctl enable sshd
sudo systemctl start sshd

echo "blacklist nouveau" | sudo tee -a /etc/modprobe.d/blacklist-nouveau.conf
echo "options nouveau modeset=0" | sudo tee -a /etc/modprobe.d/blacklist-nouveau.conf
sudo mkinitcpio -p linux-cachyos

# CPU UCode (Important)
while true; do
    read -p "Cpu UCODE -> <intel | amd > exit  :| " userInput
    userInput=$(echo "$userInput" | tr '[:upper:]' '[:lower:]')
    if [[ "$userInput" == "amd" ]]; then
        sudo pacman -Sy --noconfirm amd-ucode
        break
    elif [[ "$userInput" == "intel" ]]; then
        sudo pacman -Sy --noconfirm intel-ucode
        break
    else
        echo "Please enter either 'intel' or 'amd'"
    fi
done

# Browser Selection
while true; do
    echo "Browser List: -> if you don't want to install browser  -> 'exit'"
    echo "QuteBrowser -> qt"
    echo "Firefox     -> fx"
    echo "Chromium    -> ch"
    echo "Brave       -> br"
    echo "Vivaldi     -> vr"

    read -p "Enter Browser Name:| " userInput
    userInput=$(echo "$userInput" | tr '[:upper:]' '[:lower:]')

    if [[ "$userInput" == "qt" || "$userInput" == "qutebrowser" ]]; then
        sudo pacman -Sy --noconfirm qutebrowser python-adblock
        break
    elif [[ "$userInput" == "fx" || "$userInput" == "firefox" ]]; then
        sudo pacman -Sy --noconfirm firefox firefox-adblock-plus
        break
    elif [[ "$userInput" == "ch" || "$userInput" == "chromium" ]]; then
        sudo pacman -Sy --noconfirm chromium
        break
    elif [[ "$userInput" == "br" || "$userInput" == "brave" ]]; then
        sudo pacman -Sy --noconfirm brave-browser
        break
    elif [[ "$userInput" == "vr" || "$userInput" == "vivaldi" ]]; then
        sudo pacman -Sy --noconfirm vivaldi vivaldi-ffmpeg-codecs
        break
    elif [[ "$userInput" == "exit" ]]; then
        break
    else
        echo "What??? Please enter a valid option."
    fi
done
