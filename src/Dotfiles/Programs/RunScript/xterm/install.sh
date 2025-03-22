#!/bin/bash

sudo pacman -Sy fzf

cp .Xresources ~/
xrdb -merge ~/.Xresources
cp .bashrc ~/

cd ~/
pwd
git clone --recursive https://github.com/akinomyoga/ble.sh.git
cd ble.sh
pwd
make
cd
pwd
