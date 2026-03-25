import os

# Automatically configure connected displays
os.system('for m in $(xrandr --query | grep " connected" | cut -d" " -f1); do xrandr --output "$m" --auto; done')

# Disable DPMS (Energy Star) settings and turn off screen saver
os.system("xset -dpms")
os.system("xset s off")

# Set Qutebrowser as the default web browser
os.system("xdg-settings set default-web-browser org.qutebrowser.qutebrowser.desktop")

# Set background image using feh
os.system("feh --bg-scale ~/.config/qtile/bg.png")

# Set backspace behavior (delete character in terminal)
os.system("stty erase ^H")

# Restart the notification daemon (dunst)
os.system("pkill dunst")
os.system("dunst")

# Start picom with specified configuration (for window compositing)
os.system("picom --animations -b --vsync --config ~/.config/qtile/picom.conf")
