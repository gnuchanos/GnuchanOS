#!/bin/sh
# build.sh — compile GnuChanNotification where the Debian installer's
# pkg-config branch cannot run (an Arch test machine has no apt-get). It names
# the same libraries makefile.py names and links the same binary into the same
# _temp build directory, so what is compiled here is what the installer would
# have compiled there.
set -e
cd "$(dirname "$0")"
OUT="$HOME/gnuchan_build/_temp/gcl_NOTIFICATION-build"
mkdir -p "$OUT"
CFLAGS=$(pkg-config --cflags x11 xft xext fontconfig freetype2 imlib2 dbus-1)
LIBS=$(pkg-config --libs x11 xft xext fontconfig freetype2 imlib2 dbus-1)
exec gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -O2 -D_DEFAULT_SOURCE \
    -I. $CFLAGS \
    notif_parser.c notif_config.c notif_shape.c notif_image.c \
    notif_item.c notif_render.c notif_dbus.c GnuChanNotification.c \
    -o "$OUT/GnuChanNotification" $LIBS
