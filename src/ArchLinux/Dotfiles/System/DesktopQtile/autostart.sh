#!/bin/bash

# disable sleep mode
xset -dpms
xset s off

# display size with vga and main display for laptop
sh ~/.config/qtile/display.sh  &

# default app
xdg-settings set default-web-browser org.qutebrowser.qutebrowser.desktop

nitrogen --head=0 --set-scaled ~/.config/qtile/bg.png
nitrogen --head=1 --set-scaled ~/.config/qtile/bg.png
nitrogen  --restore &

stty erase ^H

pkill dunst &
dunst &

picom --backend glx --vsync --config ~/.config/qtile/picom.conf

