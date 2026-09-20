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
# Written to work under any POSIX shell: no arrays, no "local", no bashisms.
#
# License: GPL3
# =============================================================================

# Theme every toolkit an application may pull in.
#
# No ":dark" suffix here, on purpose. GTK 3.20 and newer split GTK_THEME on the
# last colon and then load *only* <theme>/gtk-3.0/gtk-dark.css; gtk.css is not
# read at all. This theme's gtk-dark.css is a one line forwarder to gtk.css, so
# the variant used to work by that indirection - but it made the whole theme
# depend on one relative @import resolving, and when it does not (GTK reports
# nothing useful) the application ends up with no stylesheet of ours at all and
# keeps the toolkit's default grey. Naming the theme without the variant loads
# gtk.css directly, which is the file this theme actually maintains, and the
# theme is dark-only anyway: gtk-application-prefer-dark-theme in the settings
# files keeps its dark appearance with or without the suffix.
GTK_THEME=GnuchanPurple
export GTK_THEME

# GTK 2 applications read this file directly, and GTK 2 does not understand the
# ":dark" suffix above.
#
# The system wide gtkrc is kept in the list on purpose. Setting GTK2_RC_FILES at
# all replaces GTK 2's built in default of "~/.gtkrc-2.0:/etc/gtk-2.0/gtkrc"
# with whatever is written here, so listing only the user file would silently
# narrow the search path and drop anything a distribution installs system wide.
# An entry that does not exist is skipped, so this is safe on distributions
# that ship no /etc/gtk-2.0/gtkrc.
GTK2_RC_FILES="$HOME/.gtkrc-2.0:/etc/gtk-2.0/gtkrc"
export GTK2_RC_FILES

# Mouse cursor, matched to the palette. Override if you use another one.
XCURSOR_THEME="${XCURSOR_THEME:-Adwaita}"
XCURSOR_SIZE="${XCURSOR_SIZE:-24}"
export XCURSOR_THEME
export XCURSOR_SIZE

# Qt applications follow the GTK palette through a platform theme plugin.
#
# Only set when the plugin is actually installed. A value that names a plugin
# which is not there is worse than leaving the variable unset: Qt aborts the
# application at startup with
#
#   Could not find the Qt platform theme plugin 'gtk3' in ""
#
# and that is what a user without qt5-style-plugins / qt6-qtbase-gtk3 saw.
# QT_QPA_PLATFORMTHEME is never overwritten, so a value the user exported
# earlier (for example "qt5ct") always wins.
if [ -z "${QT_QPA_PLATFORMTHEME:-}" ]; then
    for gnuchan_qt_plugins in \
        "${QT_PLUGIN_PATH:-}" \
        "$HOME/.local/lib/qt6/plugins" \
        "$HOME/.local/lib/qt5/plugins" \
        /usr/lib/qt6/plugins \
        /usr/lib/qt5/plugins \
        /usr/lib/x86_64-linux-gnu/qt6/plugins \
        /usr/lib/x86_64-linux-gnu/qt5/plugins \
        /usr/lib64/qt6/plugins \
        /usr/lib64/qt5/plugins
    do
        [ -n "$gnuchan_qt_plugins" ] || continue
        if [ -e "$gnuchan_qt_plugins/platformthemes/libqgtk3.so" ]; then
            QT_QPA_PLATFORMTHEME=gtk3
            export QT_QPA_PLATFORMTHEME
            break
        fi
    done
    unset gnuchan_qt_plugins
fi

# Java / Swing applications
_JAVA_AWT_WM_NONREPARENTING=1
export _JAVA_AWT_WM_NONREPARENTING
