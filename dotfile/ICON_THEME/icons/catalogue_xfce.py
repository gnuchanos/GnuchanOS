"""The XFCE vocabulary: settings panels, panel plugins, programs and session.

XFCE is the desktop this theme is aimed at first, so it gets the whole of its
own list: every panel in ``xfce4-settings``, every panel plugin, every program
the session starts and the internal daemons nothing else names. The names are
the ones the packages actually put in their ``Icon=`` lines, which is a
different set from the freedesktop specification — ``xfce4-display-settings`` is
not in the specification at all, and a theme that only implements the
specification leaves an XFCE desktop half drawn.

The pictograms come from :mod:`icons.glyphs_panels` where the meaning is new and
from the artwork that already exists where it is not: the display panel is the
monitor the devices context draws, the keyboard panel is the keyboard, the
battery plugin is the battery. Nothing here is a second drawing of something the
theme has.
"""

from __future__ import annotations

from .catalogue import ACTIONS, APPLICATIONS, branch

# --- The settings panels -----------------------------------------------------
# Registered as actions, because that is what a settings dialog is: a menu entry
# that opens a panel. The names the specification also defines —
# ``preferences-desktop-keyboard`` and its neighbours — are left out here
# because :mod:`icons.catalogue` already registers them, and a name may only be
# written once per context.

branch(ACTIONS, "panel-manager", "xfce4-settings", "xfce4-settings-manager", "xfce-settings-manager", "xfce4-settings-helper")
branch(ACTIONS, "applications-system", "xfce4-settings-editor")
branch(ACTIONS, "help", "xfce4-about")

branch(ACTIONS, "preferences-desktop-theme", "xfce4-appearance-settings")
branch(ACTIONS, "panel-icons", "preferences-desktop-icons")
branch(ACTIONS, "panel-wallpaper", "xfce4-desktop-settings", "xfdesktop-settings", "xfce4-backdrop")
branch(ACTIONS, "panel-color", "xfce4-color-settings")

branch(ACTIONS, "panel-display", "xfce4-display-settings", "preferences-desktop-display")
branch(ACTIONS, "panel-brightness", "xfce4-display-settings-brightness")
branch(ACTIONS, "input-keyboard", "xfce4-keyboard-settings")
branch(ACTIONS, "mouse", "xfce4-mouse-settings", "preferences-desktop-mouse")
branch(ACTIONS, "input-tablet", "xfce4-input-settings")
branch(ACTIONS, "panel-touchpad", "xfce4-touchpad-settings")
branch(ACTIONS, "panel-accessibility", "xfce4-accessibility-settings", "xfce4-accessibility")

branch(ACTIONS, "audio-volume-high", "xfce4-sound-settings", "xfce4-mixer")
branch(ACTIONS, "panel-power", "xfce4-power-manager-settings", "xfce4-power-manager")
branch(ACTIONS, "panel-login", "xfce4-session-settings", "xfce4-session")
branch(ACTIONS, "system-log-out", "xfce4-session-logout")
branch(ACTIONS, "panel-screensaver", "xfce4-screensaver-preferences", "xfce4-screensaver")
branch(ACTIONS, "screenshot-tool", "xfce4-screenshooter")
branch(ACTIONS, "panel-notifications", "xfce4-notifyd-config", "xfce4-notifyd")

branch(ACTIONS, "app", "xfce4-mime-settings", "xfce4-mime-editor")
branch(ACTIONS, "preferences-system-windows", "xfwm4-settings", "xfwm4-tweaks-settings", "xfwm4-workspace-settings", "xfce4-window-manager", "xfce4-window-manager-tweaks", "xfce4-workspaces", "xfce4-workspace-settings")
branch(ACTIONS, "panel-bar", "xfce4-panel-settings", "xfce4-panel-preferences")
branch(ACTIONS, "system-monitor", "xfce4-taskmanager")
branch(ACTIONS, "computer", "xfce4-sysinfo")

# --- The panel plugins -------------------------------------------------------
# Forty applets, one pictogram each. They are the most visible icons in an XFCE
# session — they sit in the panel all day — so each one gets its own drawing
# rather than the generic gear XFCE shows when it finds nothing: a CPU graph for
# the cpu graph, a thermometer for the sensors, two eyes for the eyes applet.

branch(APPLICATIONS, "cpu", "xfce4-cpugraph-plugin", "xfce4-cpufreq-plugin")
branch(APPLICATIONS, "system-monitor", "xfce4-systemload-plugin")
branch(APPLICATIONS, "panel-disk", "xfce4-diskperf-plugin", "xfce4-fsguard-plugin")
branch(APPLICATIONS, "panel-thermometer", "xfce4-sensors-plugin", "xfce4-sensors")
branch(APPLICATIONS, "battery-good", "xfce4-battery-plugin")
branch(APPLICATIONS, "panel-brightness", "xfce4-brightness-plugin")
branch(APPLICATIONS, "panel-power", "xfce4-power-manager-plugin", "xfce4-power-manager")
branch(APPLICATIONS, "audio-volume-high", "xfce4-alsa-plugin", "xfce4-pulseaudio-plugin", "xfce4-mixer", "xfce4-volumed")
branch(APPLICATIONS, "audio-player", "xfce4-mpc-plugin")

branch(APPLICATIONS, "panel-traffic", "xfce4-netload-plugin", "xfce4-wavelan-plugin")
branch(APPLICATIONS, "bluetooth", "xfce4-bluetooth-plugin")
branch(APPLICATIONS, "panel-plug", "xfce4-mount-plugin")
branch(APPLICATIONS, "keyboard", "xfce4-kbdleds-plugin")
branch(APPLICATIONS, "panel-key", "xfce4-xkb-plugin")
branch(APPLICATIONS, "panel-eyes", "xfce4-eyes-plugin")
branch(APPLICATIONS, "camera-web", "xfce4-camera-plugin")

branch(APPLICATIONS, "clock", "xfce4-clock", "xfce4-datetime-plugin", "xfce4-orageclock-plugin")
branch(APPLICATIONS, "panel-weather", "xfce4-weather-plugin")
branch(APPLICATIONS, "panel-timer", "xfce4-timer-plugin", "xfce4-time-out-plugin")
branch(APPLICATIONS, "mail", "xfce4-mailwatch-plugin")
branch(APPLICATIONS, "accessories-dictionary", "xfce4-dict", "xfce4-dict-plugin")
branch(APPLICATIONS, "panel-note", "xfce4-notes-plugin", "xfce4-notes")

branch(APPLICATIONS, "panel-menu", "xfce4-whiskermenu-plugin")
branch(APPLICATIONS, "system-run", "xfce4-appfinder")
branch(APPLICATIONS, "mark-location", "xfce4-places-plugin")
branch(APPLICATIONS, "bookmark", "xfce4-smartbookmark-plugin")
branch(APPLICATIONS, "panel-prompt", "xfce4-verve-plugin")
branch(APPLICATIONS, "folder", "xfce4-directory-menu")
branch(APPLICATIONS, "panel-bar", "xfce4-windowck-plugin", "xfce4-tasklist")
branch(APPLICATIONS, "panel-clipboard", "xfce4-clipman-plugin", "xfce4-clipman")
branch(APPLICATIONS, "panel-manager", "xfce4-sample-plugin", "xfce4-embed-plugin", "xfce4-vala-panel")
branch(APPLICATIONS, "slider-handle", "xfce4-generic-slider")
branch(APPLICATIONS, "panel-notifications", "xfce4-indicator-plugin")

# --- The XFCE programs -------------------------------------------------------
# The applications a session starts by hand. The ones every other desktop also
# has — thunar, mousepad, ristretto, parole — are already registered by
# :mod:`icons.catalogue_files` and are left out: what this section adds is the
# tools that exist only in an XFCE install.

branch(APPLICATIONS, "system-file-manager", "thunar-settings", "thunar-file-manager", "thunar-volman", "thunar-sendto-email", "thunar-bulk-rename", "org.xfce.thunar")
branch(APPLICATIONS, "drive-optical", "xfburn")
branch(APPLICATIONS, "network-server", "gigolo")

# --- The session internals ---------------------------------------------------
# The daemons and popup helpers nothing outside a session asks for by name, but
# which a process list, a taskbar or a keyboard shortcut dialog still draws.

branch(APPLICATIONS, "panel-bar", "xfce4-panel", "xfce4-panel-restart")
branch(APPLICATIONS, "panel-wallpaper", "xfdesktop")
branch(APPLICATIONS, "preferences-system-windows", "xfwm4")
branch(APPLICATIONS, "panel-manager", "xfsettingsd")
branch(APPLICATIONS, "panel-login", "xfce4-session", "xfce4-session-manager")
branch(APPLICATIONS, "panel-menu", "xfce4-popup-whiskermenu", "xfce4-popup-applicationsmenu")
branch(APPLICATIONS, "mark-location", "xfce4-popup-places")
branch(APPLICATIONS, "folder", "xfce4-popup-directorymenu")
