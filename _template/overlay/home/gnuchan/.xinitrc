#!/bin/bash
# .xinitrc — start X11Libre + QTile for GnuchanOS

export DISPLAY=:0

# PulseAudio
pulseaudio --start --daemonize 2>/dev/null || true

# Wallpaper
feh --bg-scale /usr/share/backgrounds/gnuchanos/bg.png 2>/dev/null || true

# Notifications
dunst 2>/dev/null &

# Compositor
picom --config ~/.config/qtile/picom.conf 2>/dev/null &

# Start QTile
exec qtile start