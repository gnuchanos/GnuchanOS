"""Every other desktop's own vocabulary: GNOME, KDE, MATE, LXQt, LXDE,
Cinnamon, Budgie, Pantheon, Deepin, Enlightenment, the window managers,
the status bars, the launchers, the lockers and the portals.

Where a panel means the same thing on two desktops it points at the same
pictogram — ``gnome-display-panel``, ``kcm_randr`` and ``lxqt-config-monitor``
are one monitor — so a machine with two desktops installed reads as one theme
rather than two. The pictograms are the ones :mod:`icons.glyphs_panels` draws
for the settings and applets, and the artwork that already exists for everything
else: a keyboard is a keyboard whichever desktop is asking.

These tables overlap each other and the earlier ones on purpose — ``dolphin`` is
a KDE program and a file manager, ``openbox`` is a window manager and an LXDE
component — so they register through :func:`icons.catalogue.alias`, which leaves
out a name the context already has instead of overwriting it. That is also what
makes the module safe to read one section at a time: a repeated name is not a
mistake to hunt for, it is a name that belongs to more than one place.
"""

from __future__ import annotations

from .catalogue import ACTIONS, APPLICATIONS, alias

# --- GNOME -------------------------------------------------------------------
# The control centre, the thirty-eight panel names GTK looks up by desktop id,
# and the programs that are only ever GNOME's.

alias(ACTIONS, "panel-manager", "gnome-control-center", "org.gnome.Settings", "gnome-settings", "gnome-session-quit")
alias(ACTIONS, "applications-system", "gnome-extensions-app", "gnome-shell-extension-prefs")

alias(ACTIONS, "panel-menu", "gnome-applications-panel")
alias(ACTIONS, "panel-wallpaper", "gnome-background-panel")
alias(ACTIONS, "bluetooth", "gnome-bluetooth-panel")
alias(ACTIONS, "camera-web", "gnome-camera-panel")
alias(ACTIONS, "panel-color", "gnome-color-panel")
alias(ACTIONS, "clock", "gnome-datetime-panel")
alias(ACTIONS, "app", "gnome-default-apps-panel")
alias(ACTIONS, "panel-thermometer", "gnome-diagnostics-panel")
alias(ACTIONS, "panel-display", "gnome-display-panel")
alias(ACTIONS, "panel-shield", "gnome-firmware-security-panel", "gnome-privacy-panel")
alias(ACTIONS, "dialog-information", "gnome-info-overview-panel")
alias(ACTIONS, "input-keyboard", "gnome-keyboard-panel")
alias(ACTIONS, "mark-location", "gnome-location-panel")
alias(ACTIONS, "panel-screensaver", "gnome-lock-panel", "gnome-screen-panel")
alias(ACTIONS, "memory", "gnome-memory-panel")
alias(ACTIONS, "microphone", "gnome-microphone-panel")
alias(ACTIONS, "mouse", "gnome-mouse-panel")
alias(ACTIONS, "panel-bar", "gnome-multitasking-panel")
alias(ACTIONS, "network", "gnome-network-panel")
alias(ACTIONS, "panel-notifications", "gnome-notifications-panel")
alias(ACTIONS, "user", "gnome-online-accounts-panel")
alias(ACTIONS, "panel-power", "gnome-power-panel")
alias(ACTIONS, "printer", "gnome-printers-panel")
alias(ACTIONS, "translation", "gnome-region-panel")
alias(ACTIONS, "drive-removable-media", "gnome-removable-media-panel")
alias(ACTIONS, "search", "gnome-search-panel")
alias(ACTIONS, "share", "gnome-sharing-panel")
alias(ACTIONS, "audio-volume-high", "gnome-sound-panel")
alias(ACTIONS, "usb", "gnome-thunderbolt-panel")
alias(ACTIONS, "panel-accessibility", "gnome-universal-access-panel")
alias(ACTIONS, "system-monitor", "gnome-usage-panel")
alias(ACTIONS, "user-group", "gnome-users-panel")
alias(ACTIONS, "input-tablet", "gnome-wacom-panel")
alias(ACTIONS, "network-wireless", "gnome-wwan-panel")

alias(APPLICATIONS, "panel-bar", "gnome-shell")
alias(APPLICATIONS, "panel-login", "gnome-session")
alias(APPLICATIONS, "code", "gnome-builder")
alias(APPLICATIONS, "accessories-text-editor", "gnome-text-editor")
alias(APPLICATIONS, "utilities-terminal", "gnome-console", "kgx")
alias(APPLICATIONS, "x-office-address-book", "gnome-contacts", "kaddressbook")
alias(APPLICATIONS, "x-office-document", "gnome-documents")
alias(APPLICATIONS, "audio-player", "gnome-music", "elisa", "juk", "kasts")
alias(APPLICATIONS, "image-x-generic", "gnome-photos", "digikam", "kphotoalbum", "lximage-qt", "eom", "xviewer", "pix", "deepin-image-viewer", "ephoto")
alias(APPLICATIONS, "panel-weather", "gnome-weather", "kweather", "plasma-applet-weather")
alias(APPLICATIONS, "text-x-generic", "gnome-logs", "ksystemlog", "mate-system-log")
alias(APPLICATIONS, "battery-good", "gnome-power-statistics", "plasma-applet-battery")
alias(APPLICATIONS, "computer", "gnome-connections", "gnome-remote-desktop", "krdc", "krfb", "krdc-app")
alias(APPLICATIONS, "application-x-firmware", "gnome-firmware")
alias(APPLICATIONS, "lock", "gnome-authenticator", "kleopatra", "kgpg", "lxqt-openssh-askpass")
alias(APPLICATIONS, "network", "gnome-nettool", "econnman", "plasma-applet-network")
alias(APPLICATIONS, "system-monitor", "gnome-usage", "evisum", "qps", "kwin-effects-monitor")
alias(APPLICATIONS, "dialog-error", "gnome-abrt")
alias(APPLICATIONS, "calendar", "gnome-todo", "endeavour", "kalendar", "merkuro", "kongress", "io.elementary.tasks")
alias(APPLICATIONS, "book", "gnome-recipes")
alias(APPLICATIONS, "applications-games", "gnome-chess", "gnome-sudoku", "gnome-mines", "gnome-robots", "gnome-nibbles", "gnome-taquin", "gnome-tetravex", "gnome-2048", "gnome-mahjongg", "gnome-klotski")
alias(APPLICATIONS, "screenshot-tool", "gnome-screenshot", "budgie-screenshot-applet", "deepin-screenshot", "io.elementary.screenshot")
alias(APPLICATIONS, "gparted", "gnome-disk-utility")
alias(APPLICATIONS, "media-flash", "gnome-multi-writer", "deepin-boot-maker")
alias(APPLICATIONS, "font-manager", "gnome-font-viewer")
alias(APPLICATIONS, "panel-color", "gnome-color-manager", "kcolorchooser", "mate-color-select", "hyprpicker")
# --- KDE Plasma --------------------------------------------------------------
# The shell, the KCM panels and the applications. The KCMs are the ones that
# matter most for a theme: Plasma asks for ``kcm_randr`` rather than for
# ``preferences-desktop-display``, so a theme that only implements the
# specification shows a blank square in half of System Settings.

alias(ACTIONS, "preferences-desktop", "plasmashell", "plasma-desktop")
alias(ACTIONS, "panel-manager", "systemsettings", "systemsettings5", "systemsettings6")
alias(ACTIONS, "applications-system", "kcmshell5", "kcmshell6", "kded5", "kded6", "kcm_componentchooser", "kcm_kded", "kcm_qtquicksettings", "kcm_solid_actions", "kcm_standard_actions")
alias(ACTIONS, "dialog-information", "kinfocenter")
alias(ACTIONS, "system-run", "krunner", "kcm_krunner", "kcm_launchfeedback")
alias(ACTIONS, "preferences-system-windows", "kwin", "kwin_x11", "kwin_wayland", "kwin-wayland", "kwin-x11", "kwin-effects", "kcm_kwindecoration", "kcm_kwinrules", "kcm_kwinscripts", "kcm_kwintabbox")
alias(ACTIONS, "panel-login", "ksmserver", "kcm_sddm", "kcm_smserver")
alias(ACTIONS, "panel-wallpaper", "ksplashqml", "kcm_plasma_theme", "kcm_splashscreen", "kcm_wallpaper")
alias(ACTIONS, "panel-display", "kscreen", "kcm_display", "kcm_kscreen", "kcm_randr")
alias(ACTIONS, "panel-key", "kglobalaccel", "kcm_webshortcuts")
alias(ACTIONS, "panel-accessibility", "kaccess", "kcm_access")
alias(ACTIONS, "panel-clipboard", "klipper", "plasma-applet-clipboard")
alias(ACTIONS, "panel-bar", "kcm_activities", "kcm_switcher", "kcm_workspace", "plasma-applet-pager", "plasma-applet-taskmanager", "plasma-applet-windowlist")
alias(ACTIONS, "panel-rocket", "kcm_autostart")
alias(ACTIONS, "search", "kcm_baloofile")
alias(ACTIONS, "bluetooth", "kcm_bluetooth", "plasma-applet-bluetooth")
alias(ACTIONS, "clock", "kcm_clock", "plasma-applet-digitalclock")
alias(ACTIONS, "panel-color", "kcm_colors")
alias(ACTIONS, "panel-timer", "kcm_cron")
alias(ACTIONS, "panel-cursor", "kcm_cursortheme")
alias(ACTIONS, "preferences-desktop-theme", "kcm_desktoptheme", "kcm_gtk", "kcm_lookandfeel", "kcm_style")
alias(ACTIONS, "panel-plug", "kcm_device_automounter")
alias(ACTIONS, "network", "kcm_dnsserver", "kcm_networkmanagement", "kcm_proxy")
alias(ACTIONS, "folder", "kcm_dolphinview", "kcm_paths")
alias(ACTIONS, "panel-power", "kcm_energy")
alias(ACTIONS, "preferences-desktop-font", "kcm_fonts")
alias(ACTIONS, "translation", "kcm_formats", "kcm_language")
alias(ACTIONS, "panel-icons", "kcm_icons")
alias(ACTIONS, "camera-web", "kcm_kamera")
alias(ACTIONS, "network-server", "kcm_kio")
alias(ACTIONS, "lock", "kcm_kwallet")
alias(ACTIONS, "panel-screensaver", "kcm_lockscreen", "kscreenlocker")
alias(ACTIONS, "mouse", "kcm_mouse")
alias(ACTIONS, "panel-brightness", "kcm_nightcolor", "kcm_nightcolour")
alias(ACTIONS, "panel-notifications", "kcm_notifications", "plasma-applet-notifications")
alias(ACTIONS, "audio-volume-high", "kcm_phonon", "kcm_pulseaudio", "kcm_soundtheme", "plasma-pa", "plasma-applet-volumewidget")
alias(ACTIONS, "printer", "kcm_printer_manager")
alias(ACTIONS, "panel-touchpad", "kcm_touchpad")
alias(ACTIONS, "user-group", "kcm_users")
alias(ACTIONS, "applications-internet", "kcm_webshortcuts-browser")

alias(APPLICATIONS, "preferences-desktop", "kinfocenter-app", "kded6-app")
alias(APPLICATIONS, "accessories-text-editor", "kwrite", "tde-kwrite")
alias(APPLICATIONS, "text-x-tex", "kile")
alias(APPLICATIONS, "accessories-calculator", "kcalc", "kmymoney")
alias(APPLICATIONS, "system-monitor", "ksysguard", "plasma-systemmonitor", "plasma-applet-systemmonitor")
alias(APPLICATIONS, "video-player", "kaffeine", "dragon", "xplayer", "rage", "deepin-movie")
alias(APPLICATIONS, "download", "kget", "kup")
alias(APPLICATIONS, "x-office-database", "kexi")
alias(APPLICATIONS, "scanner", "skanlite", "skanpage")
alias(APPLICATIONS, "code", "kompare", "kdiff3")
alias(APPLICATIONS, "applications-development", "kcachegrind", "kdbg", "eflete", "enventor")
alias(APPLICATIONS, "emblem-synchronizing", "kbackup")
alias(APPLICATIONS, "microphone", "krecorder", "deepin-voice-recorder")
alias(APPLICATIONS, "image-editor", "showfoto", "deepin-draw-generic")
alias(APPLICATIONS, "panel-disk", "filelight")
alias(APPLICATIONS, "system-file-manager", "krusader", "konqueror-tde")
alias(APPLICATIONS, "edit", "krename")
alias(APPLICATIONS, "internet-mail", "kontact")
alias(APPLICATIONS, "applications-internet", "akregator")
alias(APPLICATIONS, "panel-timer", "ktimer", "kteatime")
alias(APPLICATIONS, "clock", "kclock", "kclock-app")
alias(APPLICATIONS, "map", "kpublictransport", "itinerary")
alias(APPLICATIONS, "applications-utilities", "kruler")
alias(APPLICATIONS, "folder", "plasma-applet-folder")
alias(APPLICATIONS, "panel-menu", "plasma-applet-kickoff")
alias(APPLICATIONS, "panel-disk", "plasma-applet-diskquota")
alias(APPLICATIONS, "audio-player", "kasts-player")
alias(APPLICATIONS, "panel-weather", "plasma-applet-weather-widget")
alias(APPLICATIONS, "camera-web", "kamoso")
# --- MATE, LXQt and LXDE -----------------------------------------------------
# The control centres of the desktops that grew out of GNOME 2 and KDE 3, which
# is why so many of their names are one word away from GNOME's: the panels are
# the same panels and are pointed at the same pictograms.

alias(ACTIONS, "panel-manager", "mate-control-center", "mate-settings", "lxde-control-center", "lxqt-config")
alias(ACTIONS, "panel-wallpaper", "mate-background-properties")
alias(ACTIONS, "panel-display", "mate-display-properties", "lxqt-config-monitor")
alias(ACTIONS, "panel-key", "mate-keybinding", "lxhotkey", "lxqt-config-globalkeyshortcuts")
alias(ACTIONS, "input-keyboard", "mate-keyboard-properties", "lxinput")
alias(ACTIONS, "mouse", "mate-mouse-properties")
alias(ACTIONS, "network", "mate-network-properties")
alias(ACTIONS, "panel-notifications", "mate-notification-properties", "lxqt-config-notificationd")
alias(ACTIONS, "panel-power", "mate-power-preferences", "lxqt-config-powermanagement")
alias(ACTIONS, "panel-screensaver", "mate-screensaver-preferences")
alias(ACTIONS, "panel-login", "mate-session-properties", "lxqt-config-session", "lxsession-edit")
alias(ACTIONS, "app", "mate-default-applications-properties", "lxqt-config-file-associations")
alias(ACTIONS, "clock", "mate-time-admin")
alias(ACTIONS, "preferences-system-windows", "mate-window-properties")
alias(ACTIONS, "font-manager", "mate-font-viewer")
alias(ACTIONS, "panel-color", "mate-color-select")
alias(ACTIONS, "audio-volume-high", "mate-volume-control", "pavucontrol-qt", "qlipper-audio")
alias(ACTIONS, "user", "mate-about-me")
alias(ACTIONS, "help", "mate-about", "lxqt-about")
alias(ACTIONS, "preferences-desktop-theme", "mate-tweak", "lxappearance-obconf")
alias(ACTIONS, "panel-menu", "mozo", "mate-menu", "brisk-menu")
alias(ACTIONS, "panel-disk", "mate-disk-usage-analyzer")
alias(ACTIONS, "panel-icons", "lxqt-config-appearance")
alias(ACTIONS, "panel-brightness", "lxqt-config-brightness")
alias(ACTIONS, "input-tablet", "lxqt-config-input")
alias(ACTIONS, "translation", "lxqt-config-locale")

alias(APPLICATIONS, "system-file-manager", "caja", "caja-settings", "pcmanfm")
alias(APPLICATIONS, "image-x-generic", "eom-viewer", "gpicview")
alias(APPLICATIONS, "accessories-calculator", "mate-calc")
alias(APPLICATIONS, "accessories-dictionary", "mate-dictionary")
alias(APPLICATIONS, "search", "mate-search-tool", "catfish", "fsearch", "angrysearch", "recoll")
alias(APPLICATIONS, "panel-thermometer", "mate-sensors-applet")
alias(APPLICATIONS, "help", "mate-user-guide")
alias(APPLICATIONS, "panel-bar", "mate-panel", "lxpanel", "lxpanelctl", "lxqt-panel", "lxqt-desktop")
alias(APPLICATIONS, "panel-notifications", "mate-notification-daemon", "lxqt-notificationd", "swaync")
alias(APPLICATIONS, "panel-screensaver", "mate-screensaver", "light-locker", "xscreensaver", "gnome-screensaver", "xfce4-screensaver", "gtklock")
alias(APPLICATIONS, "panel-shield", "mate-polkit", "lxpolkit", "lxqt-policykit-agent")
alias(APPLICATIONS, "preferences-system-windows", "marco", "openbox", "obconf", "obconf-qt", "compton-conf")
alias(APPLICATIONS, "applications-system", "lxqt-admin", "lxqt-sudo", "lxqt-qtplugin", "xdg-desktop-portal-xapp", "screensaver-portal")
alias(APPLICATIONS, "archive-manager", "lxqt-archiver")
alias(APPLICATIONS, "system-run", "lxqt-runner")
alias(APPLICATIONS, "audio-volume-high", "pavucontrol-qt-app")
alias(APPLICATIONS, "panel-power", "lxqt-powermanagement")
alias(APPLICATIONS, "panel-clipboard", "qlipper", "parcellite", "clipman", "copyq", "clipit")
alias(APPLICATIONS, "accessories-text-editor", "featherpad")
alias(APPLICATIONS, "image-editor", "lximage-editor")
alias(APPLICATIONS, "screenshot-tool", "screengrab")
alias(APPLICATIONS, "network", "nm-tray")
alias(APPLICATIONS, "panel-login", "lxdm", "lxsession")
alias(APPLICATIONS, "panel-key", "lxhotkey-app")
alias(APPLICATIONS, "audio-player", "lxmusic")
alias(APPLICATIONS, "system-monitor", "lxtask")

# --- Cinnamon ----------------------------------------------------------------
# The settings panels are separate programs, one per panel, so Cinnamon is the
# desktop that most rewards a table like this: every one of the twenty-one
# entries has its own ``Icon=`` line.

alias(ACTIONS, "panel-manager", "cinnamon-settings", "cinnamon-settings-general")
alias(ACTIONS, "preferences-desktop-theme", "cinnamon-settings-appearance", "cinnamon-settings-themes")
alias(ACTIONS, "panel-wallpaper", "cinnamon-settings-backgrounds")
alias(ACTIONS, "panel-bar", "cinnamon-settings-desklets", "cinnamon-settings-panel", "cinnamon-settings-workspaces")
alias(ACTIONS, "applications-graphics", "cinnamon-settings-effects")
alias(ACTIONS, "applications-system", "cinnamon-settings-extensions")
alias(ACTIONS, "preferences-desktop-font", "cinnamon-settings-fonts")
alias(ACTIONS, "panel-cursor", "cinnamon-settings-hotcorner")
alias(ACTIONS, "input-keyboard", "cinnamon-settings-keyboard")
alias(ACTIONS, "panel-notifications", "cinnamon-settings-notifications")
alias(ACTIONS, "panel-power", "cinnamon-settings-power")
alias(ACTIONS, "panel-shield", "cinnamon-settings-privacy")
alias(ACTIONS, "panel-screensaver", "cinnamon-settings-screensaver")
alias(ACTIONS, "audio-volume-high", "cinnamon-settings-sound")
alias(ACTIONS, "panel-rocket", "cinnamon-settings-startup")
alias(ACTIONS, "user-group", "cinnamon-settings-users")
alias(ACTIONS, "preferences-system-windows", "cinnamon-settings-windows")
alias(ACTIONS, "panel-menu", "cinnamon-menu-editor")

alias(APPLICATIONS, "panel-bar", "cinnamon")
alias(APPLICATIONS, "preferences-system-windows", "muffin")
alias(APPLICATIONS, "panel-screensaver", "cinnamon-screensaver")
alias(APPLICATIONS, "video-player", "xplayer-app")
alias(APPLICATIONS, "document", "xreader")
alias(APPLICATIONS, "app", "xapp")
alias(APPLICATIONS, "panel-bar", "csd-background", "csd-housekeeping", "csd-window-manager")
alias(APPLICATIONS, "panel-accessibility", "csd-a11y-keyboard", "csd-a11y-settings")
alias(APPLICATIONS, "panel-plug", "csd-automount", "csd-orientation")
alias(APPLICATIONS, "panel-color", "csd-color")
alias(APPLICATIONS, "panel-cursor", "csd-cursor")
alias(APPLICATIONS, "input-keyboard", "csd-keyboard", "csd-media-keys")
alias(APPLICATIONS, "mouse", "csd-mouse")
alias(APPLICATIONS, "panel-power", "csd-power")
alias(APPLICATIONS, "printer", "csd-print-notifications")
alias(APPLICATIONS, "panel-screensaver", "csd-screensaver-proxy")
alias(APPLICATIONS, "audio-volume-high", "csd-sound")
alias(APPLICATIONS, "input-tablet", "csd-wacom")
alias(APPLICATIONS, "panel-display", "csd-xrandr")
alias(APPLICATIONS, "applications-system", "csd-xsettings", "csd-clipboard")
# --- Budgie, Pantheon, Deepin, Enlightenment and Trinity ---------------------
# The desktops with their own inverse domain names, their own shells and, in
# Deepin's case, their own entire application set. Trinity is here for the
# machines still running it: it is a fork of KDE 3 and its names are KDE's with
# a prefix, which is exactly the kind of table this module is for.

alias(APPLICATIONS, "panel-manager", "budgie-desktop-settings", "budgie-desktop", "budgie-control-center")
alias(APPLICATIONS, "panel-bar", "budgie-panel", "budgie-workspace-overview")
alias(APPLICATIONS, "preferences-system-windows", "budgie-wm")
alias(APPLICATIONS, "system-run", "budgie-run-dialog")

alias(APPLICATIONS, "panel-manager", "io.elementary.settings", "switchboard")
alias(APPLICATIONS, "system-file-manager", "io.elementary.files")
alias(APPLICATIONS, "software-centre", "io.elementary.appcenter")
alias(APPLICATIONS, "accessories-calculator", "io.elementary.calculator")
alias(APPLICATIONS, "calendar", "io.elementary.calendar")
alias(APPLICATIONS, "camera-web", "io.elementary.camera")
alias(APPLICATIONS, "code", "io.elementary.code")
alias(APPLICATIONS, "internet-mail", "io.elementary.mail")
alias(APPLICATIONS, "audio-player", "io.elementary.music")
alias(APPLICATIONS, "image-x-generic", "io.elementary.photos")
alias(APPLICATIONS, "video-player", "io.elementary.videos")
alias(APPLICATIONS, "panel-key", "io.elementary.shortcut-overlay")
alias(APPLICATIONS, "network", "io.elementary.capnet-assist")
alias(APPLICATIONS, "dialog-information", "io.elementary.feedback")
alias(APPLICATIONS, "panel-bar", "io.elementary.wingpanel")
alias(APPLICATIONS, "preferences-system-windows", "gala")

alias(APPLICATIONS, "panel-manager", "dde-control-center", "startdde")
alias(APPLICATIONS, "system-file-manager", "dde-file-manager")
alias(APPLICATIONS, "panel-menu", "dde-launcher")
alias(APPLICATIONS, "panel-bar", "dde-dock")
alias(APPLICATIONS, "utilities-terminal", "deepin-terminal")
alias(APPLICATIONS, "accessories-text-editor", "deepin-editor")
alias(APPLICATIONS, "audio-player", "deepin-music")
alias(APPLICATIONS, "accessories-calculator", "deepin-calculator")
alias(APPLICATIONS, "system-monitor", "deepin-system-monitor")
alias(APPLICATIONS, "software-centre", "deepin-app-store")
alias(APPLICATIONS, "x-office-drawing", "deepin-draw")
alias(APPLICATIONS, "applications-system", "deepin-installer")

alias(APPLICATIONS, "preferences-system-windows", "enlightenment")
alias(APPLICATIONS, "panel-manager", "enlightenment-settings", "e-settings", "elementary_config")
alias(APPLICATIONS, "utilities-terminal", "terminology")
alias(APPLICATIONS, "accessories-text-editor", "ecrire")
alias(APPLICATIONS, "image-x-generic", "ephoto-app")
alias(APPLICATIONS, "system-monitor", "evisum-app")

alias(APPLICATIONS, "panel-manager", "kcontrol", "tde-systemsettings")
alias(APPLICATIONS, "applications-system", "tdesudo")
alias(APPLICATIONS, "accessories-text-editor", "kate-tde")

# --- Window managers and compositors -----------------------------------------
# Every stacking and tiling manager a machine might be running, with its
# configuration tool, its menu generator and its wallpaper setter. A window
# manager has no icon of its own — it is a process — which is exactly why a
# launcher, a taskbar and a session menu end up drawing a blank square without
# a table like this one.

alias(APPLICATIONS, "preferences-system-windows", "fluxbox", "icewm", "jwm", "pekwm", "fvwm", "fvwm3", "blackbox", "windowmaker", "afterstep", "twm", "ctwm", "vtwm", "amiwm", "larswm", "waimea", "musca", "echinus", "subtle", "notion", "wmii", "ion3")
alias(APPLICATIONS, "panel-manager", "fluxconf", "icewm-settings", "icepref", "jwm-settings", "wmakerconf", "fvwm-themes", "pekwm-theme-index")
alias(APPLICATIONS, "panel-menu", "obmenu", "obmenu-generator", "obamenu", "fbmenugen")
alias(APPLICATIONS, "system-log-out", "oblogout", "wlogout")
alias(APPLICATIONS, "panel-wallpaper", "fbsetbg")
alias(APPLICATIONS, "system-run", "fbrun")
alias(APPLICATIONS, "applications-utilities", "bbtools")

alias(APPLICATIONS, "preferences-system-windows", "i3", "i3-wm", "sway", "dwm", "bspwm", "herbstluftwm", "qtile", "xmonad", "spectrwm", "awesome", "river", "dwl", "labwc", "wayfire", "hyprland")
alias(APPLICATIONS, "panel-bar", "i3bar", "i3status", "i3blocks", "swaybar", "dwmblocks", "xmobar", "taffybar", "wf-shell")
alias(APPLICATIONS, "panel-key", "sxhkd")
alias(APPLICATIONS, "system-run", "dmenu", "i3-input", "i3-msg", "swaymsg", "hyprctl")
alias(APPLICATIONS, "dialog-warning", "i3-nagbar", "swaynag")
alias(APPLICATIONS, "utilities-terminal", "st")
alias(APPLICATIONS, "panel-screensaver", "i3lock", "i3lock-color", "betterlockscreen", "swaylock", "hyprlock", "slock", "vlock", "physlock")
alias(APPLICATIONS, "panel-wallpaper", "swaybg", "hyprpaper")
alias(APPLICATIONS, "panel-timer", "swayidle", "hypridle", "xss-lock")
alias(APPLICATIONS, "screenshot-tool", "hyprshot")
alias(APPLICATIONS, "panel-brightness", "hyprsunset")
alias(APPLICATIONS, "applications-system", "i3-config-wizard", "i3exit", "i3-save-tree", "sway-input")
alias(APPLICATIONS, "preferences-system-windows", "picom", "compton", "xcompmgr", "compiz", "ezoom")
alias(APPLICATIONS, "panel-manager", "compizconfig-settings-manager", "ccsm")

# --- Bars, launchers, lockers and portals ------------------------------------
# The second half of a Wayland or a bare X session: the bar along the top, the
# menu the key binding opens, the locker and the portal every sandboxed
# application goes through to ask for a file.

alias(APPLICATIONS, "panel-bar", "waybar", "polybar", "tint2", "yambar", "sfwbar", "nwg-bar", "nwg-dock", "nwg-panel", "nwg-shell", "eww", "ags", "conky", "gkrellm", "slstatus", "i3status-rust", "nwg-wrapper")
alias(APPLICATIONS, "panel-manager", "tint2conf")
alias(APPLICATIONS, "panel-menu", "nwg-menu", "nwg-drawer", "jgmenu", "openbox-menu", "walker")
alias(APPLICATIONS, "system-run", "rofi", "wofi", "fuzzel", "bemenu", "yad", "zenity", "kdialog", "sherlock", "ulauncher", "albert", "synapse", "kupfer", "gnome-do", "rofi-calc", "rofi-emoji")
alias(APPLICATIONS, "panel-menu", "alacarte", "menulibre")
alias(APPLICATIONS, "applications-system", "xdg-desktop-portal", "xdg-desktop-portal-gtk", "xdg-desktop-portal-gnome", "xdg-desktop-portal-kde", "xdg-desktop-portal-lxqt", "xdg-desktop-portal-wlr", "xdg-desktop-portal-hyprland", "polkit-1", "systemd-logind", "elogind", "earlyoom", "systemd-oomd", "kwalletd5")
alias(APPLICATIONS, "panel-shield", "polkit-gnome", "polkit-kde-authentication-agent-1")
