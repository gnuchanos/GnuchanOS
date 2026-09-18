#!/bin/sh
# =============================================================================
# GnuchanPurple - environment variables for window manager sessions
# -----------------------------------------------------------------------------
# Source this from your shell profile, .xinitrc, .xprofile or WM autostart so
# that every GTK application started from a window manager - i3, sway, bspwm,
# Openbox, dwm, awesome and friends - uses the purple theme. Sessions such as
# GNOME, KDE or Xfce set the theme themselves through gsettings and xfconf, so
# sourcing this there is harmless but unnecessary.
#
#   . ~/.config/gnuchan-purple/gtk-env.sh
#
# Only variables are exported here: nothing is written to disk, so removing the
# line above undoes everything.
#
# License: GPL3
# =============================================================================

# Theme every toolkit an application may pull in.
GTK_THEME=GnuchanPurple
export GTK_THEME

# Force the dark variant for applications that ask for the light one.
GTK_THEME_VARIANT=dark
export GTK_THEME_VARIANT

# GTK 2 applications read this file directly.
GTK2_RC_FILES="$HOME/.gtkrc-2.0"
export GTK2_RC_FILES

# Mouse cursor, matched to the palette. Override if you use another one.
XCURSOR_THEME="${XCURSOR_THEME:-Adwaita}"
XCURSOR_SIZE="${XCURSOR_SIZE:-24}"
export XCURSOR_THEME
export XCURSOR_SIZE

# Qt applications follow the GTK palette through the platform theme plugin.
QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME:-gtk3}"
export QT_QPA_PLATFORMTHEME

# Java / Swing applications
_JAVA_AWT_WM_NONREPARENTING=1
export _JAVA_AWT_WM_NONREPARENTING
