#!/bin/bash

# disable sleep mode
xset -dpms
xset s off


# auto refresh
for m in $(xrandr --query | grep " connected" | cut -d" " -f1); do xrandr --output "$m" --auto; done






# default app
xdg-settings set default-web-browser org.qutebrowser.qutebrowser.desktop

nitrogen --head=0 --set-scaled ~/.config/qtile/bg.png
nitrogen --head=1 --set-scaled ~/.config/qtile/bg.png
nitrogen  --restore &

stty erase ^H

pkill dunst &
dunst &

picom --animations -b --vsync --config ~/.config/qtile/picom.conf

