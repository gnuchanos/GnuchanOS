#!/bin/bash

mkdir ~/tmp
cd ~/tmp
git clone https://aur.archlinux.org/yay.git
cd yay
makepkg -si

yay -Sy picom-ftlabs-git
yay -S rar irssi gpu-screen-recorder --rebuild
