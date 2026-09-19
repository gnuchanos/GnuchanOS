"""File types and menu categories, as a continuation of the catalogue.

Kept in its own module because it is a different kind of table from the action
vocabulary: these names are not menu items, they are what a file manager and a
launcher ask for when they have a file or a category in hand. The lists are long
on purpose — a file type nobody registered shows as a blank page in the middle
of a themed grid, which is the first thing a user notices.
"""

from __future__ import annotations

from .catalogue import APPLICATIONS, CATEGORIES, MIMETYPES, alias, branch

# --- MimeTypes ---------------------------------------------------------------
# One entry per file type the common desktops ask for. The artwork is shared:
# every one of these is a page with a mark, which is why the whole set hangs
# together in a directory listing.

branch(MIMETYPES, "text-x-generic", "text-x-generic", "text-x-generic-template", "text-plain", "text-x-readme", "text-x-log", "text-x-changelog", "text-x-copying", "text-makefile", "text-x-makefile", "text-x-markdown", "text-x-diff", "text-x-patch", "application-x-zerosize", "empty")
branch(MIMETYPES, "text-html", "text-html", "application-xhtml+xml", "text-xml", "application-xml", "application-rss+xml", "application-atom+xml", "application-json", "application-x-yaml")
branch(MIMETYPES, "text-x-script", "text-x-script", "application-x-shellscript", "application-x-perl", "application-x-ruby", "application-x-php", "application-javascript", "text-x-lua")
branch(MIMETYPES, "text-x-python", "text-x-python", "application-x-python-bytecode")
branch(MIMETYPES, "text-x-c", "text-x-c", "text-x-c++", "text-x-csrc", "text-x-chdr", "text-x-h", "text-x-java", "text-x-tex", "text-x-go", "text-x-rust")
branch(MIMETYPES, "text-csv", "text-csv", "text-x-vcard")
branch(MIMETYPES, "x-office-document", "x-office-document", "application-msword", "application-vnd.oasis.opendocument.text", "application-vnd.openxmlformats-officedocument.wordprocessingml.document", "application-rtf")
branch(MIMETYPES, "x-office-spreadsheet", "x-office-spreadsheet", "application-vnd.ms-excel", "application-vnd.oasis.opendocument.spreadsheet", "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet")
branch(MIMETYPES, "x-office-presentation", "x-office-presentation", "application-vnd.ms-powerpoint", "application-vnd.oasis.opendocument.presentation", "application-vnd.openxmlformats-officedocument.presentationml.presentation")
branch(MIMETYPES, "x-office-drawing", "x-office-drawing", "application-vnd.oasis.opendocument.graphics", "application-vnd.oasis.opendocument.graphics-template")
branch(MIMETYPES, "x-office-address-book", "x-office-address-book")
branch(MIMETYPES, "x-office-calendar", "x-office-calendar", "vcalendar", "text-x-vcalendar")
branch(MIMETYPES, "application-pdf", "application-pdf", "application-x-bzpdf", "application-x-gzpdf", "application-epub+zip", "application-x-mobipocket-ebook")
branch(MIMETYPES, "application-x-executable", "application-x-executable", "application-x-executable-script", "application-x-desktop")
branch(MIMETYPES, "application-x-object", "application-x-object", "application-x-sharedlib", "application-x-archive-object")
branch(MIMETYPES, "application-x-archive", "application-x-archive", "package-x-generic", "application-x-compressed-tar", "application-zip", "application-x-rar", "application-x-7z-compressed", "application-x-gzip", "application-x-bzip-compressed-tar")
branch(MIMETYPES, "application-x-firmware", "application-x-firmware", "application-x-diskimage")
branch(MIMETYPES, "image-x-generic", "image-x-generic", "image-png", "image-jpeg", "image-gif", "image-bmp", "image-svg+xml", "image-x-xcf", "image-x-psd", "image-tiff", "image-webp")
branch(MIMETYPES, "audio-x-generic", "audio-x-generic", "audio-mpeg", "audio-x-wav", "audio-x-flac+ogg", "audio-x-vorbis+ogg", "audio-x-mp4", "audio-x-opus+ogg")
branch(MIMETYPES, "video-x-generic", "video-x-generic", "video-mp4", "video-x-matroska", "video-x-msvideo", "video-mpeg", "video-webm", "video-x-avi")
branch(MIMETYPES, "font-x-generic", "font-x-generic", "application-x-font-ttf", "application-x-font-type1", "application-x-font-otf", "application-x-font-bdf", "application-x-font-snf")
branch(MIMETYPES, "unknown", "unknown", "application-octet-stream", "application-x-ms-dos-executable", "application-x-nintendo-rom")
branch(MIMETYPES, "drive-harddisk", "application-x-cd-image", "application-x-cue", "application-x-raw-disk-image", "application-x-bittorrent", "application-x-appimage")
branch(MIMETYPES, "archive", "application-x-rpm-package", "application-x-deb", "application-vnd.debian.binary-package", "application-x-msi")
branch(MIMETYPES, "printer", "application-vnd.cups-ppd")
branch(MIMETYPES, "image-editor", "application-x-blender")
branch(MIMETYPES, "book", "application-x-mobipocket", "application-x-fictionbook+xml")
branch(MIMETYPES, "code", "application-x-wine-extension-inf", "application-x-desktop-symbolic")

# --- Categories --------------------------------------------------------------
# What a launcher's category page shows. These names are defined by the menu
# specification and are the reason a "System" page looks themed at all.

branch(CATEGORIES, "applications-internet", "applications-internet", "web-browser", "network-web", "Internet")
branch(CATEGORIES, "applications-graphics", "applications-graphics", "Graphics", "applications-3d")
branch(CATEGORIES, "applications-multimedia", "applications-multimedia", "AudioVideo", "Audio", "Video", "Player", "applications-audio", "applications-video")
branch(CATEGORIES, "applications-office", "applications-office", "Office", "applications-word", "applications-spreadsheet", "applications-presentation", "applications-database")
branch(CATEGORIES, "applications-science", "applications-science", "Science", "applications-math", "applications-chemistry", "applications-astronomy")
branch(CATEGORIES, "applications-engineering", "applications-engineering", "Engineering", "applications-cad")
branch(CATEGORIES, "applications-utilities", "applications-utilities", "Utility", "Utilities", "applications-archiving", "applications-file-tools")
branch(CATEGORIES, "applications-development", "applications-development", "Development", "applications-ide", "applications-debugger", "applications-vcs")
branch(CATEGORIES, "applications-games", "applications-games", "Game", "applications-emulators")
branch(CATEGORIES, "applications-education", "applications-education", "Education", "applications-language", "applications-teaching")
branch(CATEGORIES, "applications-accessories", "applications-accessories", "Accessories", "applications-other")
branch(CATEGORIES, "applications-system", "applications-system", "System", "Settings", "preferences-system", "applications-settings")
branch(CATEGORIES, "preferences-desktop", "preferences-desktop", "DesktopSettings", "applications-desktop-settings")
branch(CATEGORIES, "applications-network", "applications-network", "Network", "applications-networking")

# --- The applications a desktop actually starts ---------------------------------
# Programs that ship their own icon keep it. These are the ones that do not
# always have one, or whose icon a minimal desktop has to invent. Naming them
# here is what makes "the theme supports every program" true in practice rather
# than in principle: Firefox, a terminal, lxappearance and a package manager all
# get the right picture on a machine the theme has never seen.

branch(APPLICATIONS, "applications-internet", "firefox", "firefox-esr", "chromium", "chromium-browser", "google-chrome", "google-chrome-stable", "brave-browser", "epiphany", "falkon", "midori", "opera", "vivaldi", "qutebrowser", "luakit")
branch(APPLICATIONS, "internet-mail", "thunderbird", "icedove", "evolution", "geary", "kmail", "claws-mail", "sylpheed", "mail-client")
branch(APPLICATIONS, "internet-chat", "telegram", "telegram-desktop", "signal-desktop", "discord", "element-desktop", "hexchat", "pidgin", "slack", "teams", "zoom", "skypeforlinux", "whatsapp-desktop", "telegram-cli")
branch(APPLICATIONS, "web-browser", "browser", "www-browser", "x-www-browser")
branch(APPLICATIONS, "video-player", "vlc", "mpv", "smplayer", "totem", "parole", "celluloid", "gnome-mplayer", "mplayer", "kodi", "video-player")
branch(APPLICATIONS, "audio-player", "rhythmbox", "audacious", "clementine", "strawberry", "lollypop", "elisa", "deadbeef", "exaile", "quodlibet", "cantata", "musique")
branch(APPLICATIONS, "audio-speakers", "pavucontrol", "gnome-volume-control", "kmix", "qasmixer", "pulsemixer", "alsamixer", "volume-control")
branch(APPLICATIONS, "applications-graphics", "gimp", "krita", "inkscape", "mypaint", "pinta", "kolourpaint", "blender", "openscad", "karbon", "scribus", "graphics-editor")
branch(APPLICATIONS, "image-editor", "photoshop", "gwenview", "eog", "loupe", "ristretto", "shotwell", "feh", "gthumb", "nomacs", "geeqie", "image-viewer")
branch(APPLICATIONS, "accessories-text-editor", "gedit", "kate", "mousepad", "leafpad", "notepadqq", "pluma", "xed", "emacs", "vim", "gvim", "neovim", "nano", "micro", "text-editor-app")
branch(APPLICATIONS, "code", "code", "code-oss", "vscodium", "codium", "sublime_text", "sublime-text", "atom", "brackets", "kdevelop", "geany", "qtcreator", "android-studio", "intellij-idea", "pycharm", "codeblocks", "eclipse", "netbeans")
branch(APPLICATIONS, "utilities-terminal", "gnome-terminal", "kgx", "konsole", "xterm", "urxvt", "rxvt", "alacritty", "kitty", "wezterm", "foot", "footclient", "tilix", "terminator", "xfce4-terminal", "mate-terminal", "lxterminal", "qterminal", "st", "guake", "yakuake", "terminator-terminal")
branch(APPLICATIONS, "system-file-manager", "nautilus", "dolphin", "thunar", "pcmanfm", "pcmanfm-qt", "nemo", "caja", "rox-filer", "spacefm", "ranger", "yazi", "lf", "file-manager", "org.gnome.Nautilus", "org.kde.dolphin")
branch(APPLICATIONS, "accessories-calculator", "gnome-calculator", "gcalctool", "kcalc", "galculator", "qalculate", "qalculate-gtk", "speedcrunch", "calculator-app")
branch(APPLICATIONS, "system-monitor", "gnome-system-monitor", "ksysguard", "ksystemguard", "htop", "gnome-usage", "xfce4-taskmanager", "mate-system-monitor", "plasma-systemmonitor", "system-monitor-app", "process-viewer")
branch(APPLICATIONS, "gparted", "gparted", "gnome-disks", "kde-partitionmanager", "partitionmanager", "xfce4-disk-usage")
branch(APPLICATIONS, "software-centre", "gnome-software", "plasma-discover", "discover", "pamac-manager", "synaptic", "gnome-packagekit", "apper", "muon", "bauh", "octopi")
branch(APPLICATIONS, "system-software-install", "gpk-install-package", "package-manager", "add-software")
branch(APPLICATIONS, "archive-manager", "file-roller", "ark", "xarchiver", "engrampa", "peazip", "squeeze", "archive-manager-app")
branch(APPLICATIONS, "font-manager", "fontforge", "font-manager-app", "gnome-font-viewer", "kfontview")
branch(APPLICATIONS, "accessories-character-map", "gucharmap", "gnome-characters", "kcharselect", "character-map")
branch(APPLICATIONS, "accessories-dictionary", "gnome-dictionary", "goldendict", "x-dictionary", "dictionary-app")
branch(APPLICATIONS, "screenshot-tool", "gnome-screenshot", "spectacle", "flameshot", "scrot", "maim", "xfce4-screenshooter", "mate-screenshot", "ksnip", "shutter", "grimshot")
branch(APPLICATIONS, "download", "transmission", "transmission-gtk", "qbittorrent", "deluge", "rtorrent", "aria2", "ktorrent", "fragments", "downloader")
branch(APPLICATIONS, "applications-games", "steam", "lutris", "heroic", "bottles", "playonlinux", "wine", "wine64", "dosbox", "retroarch", "minecraft", "minetest")
branch(APPLICATIONS, "preferences-desktop-theme", "lxappearance", "gnome-tweaks", "gnome-tweak-tool", "kvantummanager", "qt5ct", "qt6ct", "nwg-look", "xfce4-appearance-settings", "mate-appearance-properties", "unity-tweak-tool", "gsettings-editor", "dconf-editor")
branch(APPLICATIONS, "preferences-desktop", "gnome-control-center", "systemsettings", "systemsettings5", "xfce4-settings-manager", "mate-control-center", "lxqt-config", "lxappearance-settings", "cockpit-settings")
branch(APPLICATIONS, "preferences-system-windows", "arandr", "lxrandr", "xrandr", "nvidia-settings", "gnome-display-panel", "kcm_randr", "autorandr")
branch(APPLICATIONS, "network", "nm-connection-editor", "nm-applet", "connman-gtk", "system-config-network", "wpa_gui", "network-manager-applet")
branch(APPLICATIONS, "bluetooth", "blueman", "blueman-manager", "bluetooth-manager", "gnome-bluetooth")
branch(APPLICATIONS, "printer", "system-config-printer", "printers", "gnome-printers-panel", "kcm_printer_manager", "cups")
branch(APPLICATIONS, "video-display", "vokoscreen", "obs", "obs-studio", "kdenlive", "shotcut", "openshot", "pitivi", "avidemux", "handbrake", "screen-recorder")
branch(APPLICATIONS, "audio-x-generic", "audacity", "ardour", "lmms", "mixxx", "hydrogen", "qtractor", "musescore", "rosegarden", "audio-editor")
branch(APPLICATIONS, "lock", "keepassxc", "bitwarden", "seahorse", "gnome-keyring", "pass", "password-manager", "kwalletmanager")
branch(APPLICATIONS, "emblem-synchronizing", "timeshift", "deja-dup", "backintime", "borg", "syncthing", "rclone", "backup-tool")
branch(APPLICATIONS, "computer", "virtualbox", "virt-manager", "gnome-boxes", "qemu", "virt-manager-app", "remmina", "freerdp", "vinagre")
branch(APPLICATIONS, "applications-system", "utilities-system-monitor-app", "system-settings-app", "hardinfo", "gnome-system-monitor-app")
branch(APPLICATIONS, "help", "yelp", "gnome-help", "khelpcenter", "help-app")
branch(APPLICATIONS, "applications-education", "gcompris", "kgeography", "stellarium", "celestia", "ktouch", "anki", "education-app")
branch(APPLICATIONS, "applications-science", "gnuplot", "octave", "wxmaxima", "avogadro", "kstars", "science-app")
branch(APPLICATIONS, "text-x-generic", "libreoffice-writer", "libreoffice-calc", "libreoffice-impress", "libreoffice-draw", "libreoffice", "libreoffice-startcenter", "onlyoffice-desktopeditors", "wps-office", "abiword", "gnumeric", "calligrawords", "calligrasheets", "calligrastage", "office-suite")
branch(APPLICATIONS, "document", "evince", "okular", "mupdf", "xpdf", "atril", "zathura", "qpdfview", "document-viewer")
branch(APPLICATIONS, "image-x-generic", "gpicview", "viewnior", "comix", "mcomix", "document-scanner")
branch(APPLICATIONS, "book", "calibre", "foliate", "ebook-viewer", "ebook-editor")
branch(APPLICATIONS, "map", "gnome-maps", "marble", "josm", "qgis", "openstreetmap")
branch(APPLICATIONS, "clock", "gnome-clocks", "kclock", "alarm-clock")
branch(APPLICATIONS, "calendar", "gnome-calendar", "korganizer", "orage", "evolution-calendar", "osmo")
branch(APPLICATIONS, "chat", "empathy", "telepathy", "ekiga", "jami")
branch(APPLICATIONS, "microphone", "audacity-recorder", "gnome-sound-recorder", "krecorder", "recorder")
branch(APPLICATIONS, "user-group", "users-admin", "gnome-users-panel", "kuser", "system-config-users")
branch(APPLICATIONS, "app", "application-x-generic", "application-x-executable-generic", "unknown-application")

# --- MimeTypes: the rest of the specification's file types ----------------------
# The specification names these and the table above had not reached them, so a
# directory holding a SQLite database, a Rust source file or a Matroska video
# showed a blank page between the ones the theme did know. They are aliases
# because the pictures are the ones the family already draws: an archive is an
# archive whether it is an arj or a zip, and a source file is the script page
# whether the language is Haskell or Verilog.

alias(MIMETYPES, "drive-harddisk", "application-x-appliance")
alias(
    MIMETYPES,
    "archive",
    "application-x-arj",
    "application-x-cpio",
    "application-x-lha",
    "application-x-lzma",
    "application-x-lz4",
    "application-x-zstd",
    "application-x-xz",
    "application-x-java-archive",
    "application-x-msdownload",
    "application-x-shockwave-flash",
)
alias(
    MIMETYPES,
    "x-office-database",
    "application-x-sqlite3",
    "application-x-sqlite2",
    "application-vnd.ms-access",
    "application-x-dbase",
)
alias(MIMETYPES, "book", "application-x-dvi")
alias(
    MIMETYPES,
    "text-x-generic",
    "application-x-tex",
    "application-x-bibtex",
    "text-x-texinfo",
    "text-x-troff",
    "text-x-adasrc",
    "text-x-gettext-translation",
    "text-x-lilypond",
)
alias(MIMETYPES, "video-x-generic", "application-x-matroska")
alias(MIMETYPES, "audio-x-generic", "application-x-ogg", "application-x-flac", "application-x-wav")
alias(
    MIMETYPES,
    "text-x-script",
    "text-x-pascal",
    "text-x-javascript",
    "text-x-typescript",
    "text-x-actionscript",
    "text-x-sql",
    "text-x-qt",
    "text-x-objcsrc",
    "text-x-swift",
    "text-x-kotlin",
    "text-x-haskell",
    "text-x-erlang",
    "text-x-elixir",
    "text-x-scheme",
    "text-x-lisp",
    "text-x-clojure",
    "text-x-csharp",
    "text-x-fortran",
    "text-x-matlab",
    "text-x-r",
    "text-x-octave",
    "text-x-vhdl",
    "text-x-verilog",
    "application-x-kcsrc",
)
alias(
    MIMETYPES,
    "x-office-document",
    "x-office-document-template",
    "application-vnd.oasis.opendocument.text-template",
    "application-vnd.oasis.opendocument.text-master",
)
alias(
    MIMETYPES,
    "x-office-spreadsheet",
    "x-office-spreadsheet-template",
    "application-vnd.oasis.opendocument.spreadsheet-template",
)
alias(
    MIMETYPES,
    "x-office-presentation",
    "x-office-presentation-template",
    "application-vnd.oasis.opendocument.presentation-template",
)
alias(MIMETYPES, "x-office-drawing", "application-vnd.oasis.opendocument.drawing")
alias(MIMETYPES, "x-office-database", "application-vnd.oasis.opendocument.database")
alias(MIMETYPES, "x-office-address-book", "application-vnd.oasis.opendocument.contact")
alias(MIMETYPES, "x-office-calendar", "application-vnd.oasis.opendocument.calendar")
alias(MIMETYPES, "text-x-c", "text-x-matlab")
alias(MIMETYPES, "image-x-generic", "application-x-krita")

# --- Categories: the rest of the menu specification ----------------------------
# The menu specification defines far more categories than the four a launcher
# usually shows, and the X- prefixed ones are the sets GNOME and KDE invented
# for their own editors. A category with no icon shows as a blank square beside
# its name, which is why the whole list is here rather than the usual four.

alias(
    CATEGORIES,
    "applications-utilities",
    "X-GNOME-Utilities",
    "X-KDE-Utilities-Desktop",
    "X-KDE-Utilities-File",
    "X-KDE-Utilities-Peripherals",
    "X-KDE-Utilities-PIM",
    "FileTools",
    "TextTools",
    "Viewer",
    "Publishing",
    "Compression",
)
alias(CATEGORIES, "applications-internet", "X-GNOME-Internet", "FileTransfer", "Languages")
alias(
    CATEGORIES,
    "applications-office",
    "X-GNOME-Office",
    "ContactManagement",
    "Calendar",
    "ProjectManagement",
    "Presentation",
    "Spreadsheet",
    "WordProcessor",
    "Database",
    "Dictionary",
    "Chart",
    "FlowChart",
    "PDA",
)
alias(CATEGORIES, "applications-multimedia", "X-GNOME-Media", "Art")
alias(
    CATEGORIES,
    "applications-system",
    "X-GNOME-System",
    "X-GNOME-Settings-Panel",
    "X-GNOME-Other",
    "TrayIcon",
    "Core",
    "Shell",
    "Screensaver",
    "PackageManager",
    "Security",
    "Accessibility",
    "HardwareSettings",
)
alias(
    CATEGORIES,
    "applications-development",
    "X-KDE-More",
    "X-KDE-information",
    "X-KDE-Settings-Accessibility",
    "X-KDE-Settings-Components",
    "X-KDE-Settings-Desktop",
    "X-KDE-Settings-Hardware",
    "X-KDE-Settings-LookNFeel",
    "X-KDE-Settings-Network",
    "X-KDE-Settings-Notifications",
    "X-KDE-Settings-Personalization",
    "X-KDE-Settings-Security",
    "X-KDE-Settings-System",
    "X-KDE-Settings-WebShortcuts",
    "X-KDE-Settings-WindowManagement",
    "Building",
    "Debugger",
    "IDE",
    "GUIDesigner",
    "Profiling",
    "RevisionControl",
    "Translation",
)
alias(
    CATEGORIES,
    "applications-science",
    "Math",
    "Physics",
    "Chemistry",
    "Biology",
    "Geography",
    "Geology",
    "Astronomy",
    "Electronics",
    "Engineering",
    "Humanities",
)
alias(CATEGORIES, "applications-accessories", "Applet", "ConsoleOnly")
alias(CATEGORIES, "applications-graphics", "Photography", "2DGraphics", "RasterGraphics", "VectorGraphics")
alias(CATEGORIES, "applications-games", "Amusement", "LogicGame", "BlocksGame")

# --- Applications: the last of the specification's names ------------------------
# Two things are left. ``preferences-desktop-remote-desktop`` is the one settings
# panel the desktop lists had not reached, and the ``app-letter-`` and
# ``app-digit-`` plates are the tiles a launcher falls back to when it has to
# draw an entry whose own icon it could not find: one per letter and per digit,
# so an unknown program called "Krita" shows a K rather than a blank square.

alias(APPLICATIONS, "preferences-desktop", "preferences-desktop-remote-desktop")
alias(APPLICATIONS, "preferences-desktop", "gnome-remote-desktop", "org.gnome.Settings")

for _letter in "abcdefghijklmnopqrstuvwxyz":
    alias(APPLICATIONS, f"app-letter-{_letter}", f"app-letter-{_letter}")

for _digit in range(10):
    alias(APPLICATIONS, f"app-digit-{_digit}", f"app-digit-{_digit}")
# ``x-office-database`` is a file type the specification names in its own right —
# a database file is not a document and not a spreadsheet — and the theme used the
# glyph without ever registering the name. ``package-x-generic-symbolic`` is the
# monochrome form a package manager asks for; the theme writes symbolic variants
# for the contexts GTK recolours, and MimeTypes is not one of them, so this one
# has to be named explicitly.
alias(MIMETYPES, "x-office-database", "x-office-database")
alias(MIMETYPES, "package-x-generic", "package-x-generic-symbolic")
