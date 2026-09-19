"""The programs a desktop actually launches, by domain.

This is the table that makes "the theme supports every program" true on a
machine the theme has never seen. A program that ships its own icon keeps it —
this theme does not draw Firefox's fox, and pretending to would be a lie — so
what is here is the *fallback*: the name a launcher, a file manager or a menu
asks for when it has a ``.desktop`` file in front of it and no logo behind it,
and the name of a program that has never had an icon at all. A terminal gets the
terminal, forty text editors get one page with a pencil on it, and a whole
directory of game emulators reads as one family instead of forty blank squares.

Names are grouped by the picture they draw, because that is the relationship
that matters: one line per icon, many names per line, and the line is what keeps
forty editors looking like they came from one theme.

The table deliberately overlaps :mod:`icons.catalogue_files` and
:mod:`icons.catalogue_desktops` — ``thunar``, ``kate`` and ``gwenview`` appear
in more than one of them — so it registers through
:func:`icons.catalogue.alias`, which leaves out a name the context already has.
"""

from __future__ import annotations

from .catalogue import APPLICATIONS, alias

# --- Browsers ----------------------------------------------------------------
# One globe for every browser, including the three that are really a rendering
# engine with a window around it, and the text browsers, which are terminals and
# are drawn as terminals.

alias(APPLICATIONS, "applications-internet", "firefox-esr", "firefox-developer-edition", "librewolf", "waterfox", "palemoon", "basilisk", "icecat", "gnu-icecat", "tor-browser", "torbrowser-launcher", "microsoft-edge", "microsoft-edge-stable", "brave-browser-beta", "vivaldi-snapshot", "yandex-browser", "netsurf", "dillo", "min", "otter-browser", "slimjet", "searx")
alias(APPLICATIONS, "utilities-terminal", "links", "lynx", "w3m", "elinks", "browsh")

# --- Mail, chat and conferencing ---------------------------------------------
# A letter for the mail clients, a speech bubble for everything that happens in
# a window, a prompt for the four that happen in a terminal.

alias(APPLICATIONS, "internet-mail", "thunderbird", "betterbird", "evolution", "geary", "kmail", "claws-mail", "sylpheed", "mailspring")
alias(APPLICATIONS, "utilities-terminal", "mutt", "neomutt", "alpine", "aerc", "irssi", "weechat", "quassel", "toxic")
alias(APPLICATIONS, "internet-chat", "telegram", "telegram-desktop", "signal-desktop", "discord", "element-desktop", "element", "nheko", "neochat", "fractal", "hexchat", "pidgin", "polari", "konversation", "kopete", "dino", "gajim", "psi", "slack", "teams", "matrix-client", "zapzap", "caprine", "vesktop", "mumble", "teamspeak", "jami", "ekiga")
alias(APPLICATIONS, "microphone", "zoom", "skypeforlinux", "whatsapp-desktop")

# --- Media players and production --------------------------------------------
# A screen with a play mark for video, a note for audio, and the multimedia
# category pictogram for the tools that *edit* rather than play: a video editor
# that showed a play triangle would be lying about what it does.

alias(APPLICATIONS, "video-player", "vlc", "mpv", "smplayer", "totem", "parole", "celluloid", "gnome-mplayer", "mplayer", "kodi", "dragon", "kaffeine", "freetube", "ytmdesktop", "pipe-viewer")
alias(APPLICATIONS, "audio-player", "rhythmbox", "audacious", "clementine", "strawberry", "lollypop", "deadbeef", "exaile", "quodlibet", "cantata", "musique", "pragha", "gnome-music", "juk", "amarok", "cmus", "ncmpcpp", "mpd", "sonata", "ario", "mpc", "moc", "termusic")
alias(APPLICATIONS, "applications-multimedia", "kdenlive", "shotcut", "openshot", "pitivi", "avidemux", "handbrake", "flowblade", "olive", "natron", "cinelerra", "lightworks", "davinci-resolve", "audacity", "ardour", "lmms", "mixxx", "hydrogen", "qtractor", "musescore", "rosegarden", "carla", "cadence", "qjackctl", "patchage", "guitarix", "rakarrack", "yoshimi", "zynaddsubfx", "drumgizmo", "seq66", "sooperlooper", "giada", "vcvrack", "bespoke", "supercollider", "puredata", "sonic-pi", "csound", "chuck", "ocenaudio", "spek", "sonic-visualiser", "praat", "reaper", "bitwig", "renoise")
alias(APPLICATIONS, "screenshot-tool", "obs", "obs-studio", "vokoscreen", "simplescreenrecorder", "recordmydesktop", "kazam", "peek", "kooha", "wf-recorder", "byzanz", "gromit-mpx")

# --- Image viewers and graphics ----------------------------------------------

alias(APPLICATIONS, "image-x-generic", "gwenview", "eog", "loupe", "ristretto", "shotwell", "feh", "gthumb", "nomacs", "geeqie", "gpicview", "viewnior", "comix", "mcomix", "sxiv", "nsxiv", "imv", "pqiv", "mirage", "xviewer")
alias(APPLICATIONS, "image-editor", "darktable", "rawtherapee", "rapid-photo-downloader", "mypaint", "pinta", "kolourpaint", "krita", "gimp", "inkscape", "synfig", "opentoonz", "pencil2d", "azpainter", "xournalpp", "xournal", "drawpile")
alias(APPLICATIONS, "applications-graphics", "scribus", "karbon", "blender", "openscad", "freecad", "kicad", "librecad", "qcad", "solvespace", "wings3d", "meshlab", "cloudcompare", "slic3r", "prusa-slicer", "cura", "orcaslicer", "fritzing", "kicad-pcbnew", "gerbv", "dia", "drawio", "umbrello", "plantuml")


# --- Office, documents and reference -----------------------------------------
# The word processor, the spreadsheet, the presentation and the drawing have
# their own pictograms because the specification defines them; a suite as a whole
# gets the generic office page, and a reader that only shows a file gets the
# document mark.

alias(APPLICATIONS, "text-x-generic", "libreoffice", "libreoffice-startcenter", "onlyoffice-desktopeditors", "wps-office", "softmaker", "freeoffice", "presentations")
alias(APPLICATIONS, "x-office-document", "abiword", "calligrawords", "textmaker", "lyx", "texmaker", "texstudio", "texworks")
alias(APPLICATIONS, "x-office-spreadsheet", "gnumeric", "calligrasheets", "planmaker")
alias(APPLICATIONS, "x-office-presentation", "calligrastage")
alias(APPLICATIONS, "document", "okular", "mupdf", "xpdf", "atril", "zathura", "qpdfview", "master-pdf-editor")
alias(APPLICATIONS, "book", "calibre", "foliate", "sigil", "ebook-viewer")
alias(APPLICATIONS, "scanner", "ocrmypdf", "gscan2pdf", "xsane", "simple-scan", "gimagereader", "ocrfeeder", "tesseract")
alias(APPLICATIONS, "document", "pdfarranger", "pdfsam")

# --- Notes, tasks and knowledge ----------------------------------------------
# A sticky note for the notebooks, a calendar for the task managers: what a user
# is doing with the file, not what file it is.

alias(APPLICATIONS, "panel-note", "gnome-notes", "bijiben", "notes", "joplin", "obsidian", "logseq", "zettlr", "simplenote", "standard-notes", "qownnotes", "zim", "rednotebook", "cherrytree", "tomboy", "gnote", "trilium")
alias(APPLICATIONS, "calendar", "taskwarrior", "planner", "ganttproject")
alias(APPLICATIONS, "clock", "workrave", "rsibreak", "stretchly")

# --- Text editors and IDEs ---------------------------------------------------
# One page with a pencil for every editor, and the angle-bracket mark for
# everything that is an environment rather than an editor. That split is the one
# that matters in a launcher: a user looking for a text editor should not have to
# read past two dozen IDEs.

alias(APPLICATIONS, "accessories-text-editor", "notepadqq", "emacs", "vim", "gvim", "neovim", "nano", "micro", "helix", "kakoune", "atom", "brackets")
alias(APPLICATIONS, "code", "vscode-insiders", "cursor", "zed", "geany", "qtcreator", "clion", "rider", "webstorm", "phpstorm", "rubymine", "datagrip", "anjuta", "glade", "lazarus", "gambas3", "thonny", "idle3", "spyder", "jupyter", "jupyter-notebook", "jupyterlab", "rstudio", "bluej", "processing")
alias(APPLICATIONS, "applications-development", "android-studio", "intellij-idea", "pycharm", "goland", "eclipse", "netbeans", "codeblocks", "arduino", "arduino-ide", "platformio")

# --- Version control, build tools and languages ------------------------------
# Everything in this half of the table is a command-line tool with no window of
# its own, so the picture is the category rather than a logo: the development
# pictogram for the tools that build and the code mark for the runtimes, which is
# enough for a menu that has to show *something* rather than a blank square.

alias(APPLICATIONS, "applications-development", "git", "gitg", "gitkraken", "gitui", "lazygit", "tig", "git-cola", "meld", "diffuse", "subversion", "mercurial", "fossil", "cvs", "bzr", "repo")
alias(APPLICATIONS, "applications-development", "gcc", "clang", "make", "cmake", "meson", "ninja", "scons", "autotools", "autoconf", "automake", "libtool", "pkg-config", "gdb", "lldb", "valgrind", "heaptrack", "perf", "strace", "ltrace", "objdump", "readelf", "cppcheck", "clang-tidy", "clang-format", "doxygen", "sphinx", "mkdocs", "pandoc", "asciidoctor")
alias(APPLICATIONS, "code", "python", "python3", "idle", "pip", "pipx", "poetry", "virtualenv", "conda", "anaconda", "mamba", "node", "npm", "pnpm", "yarn", "deno", "bun", "typescript", "go", "rustc", "cargo", "rustup", "java", "openjdk", "gradle", "maven", "ant", "kotlin", "scala", "sbt", "groovy", "mono", "dotnet", "php", "composer", "ruby", "gem", "rails", "perl", "cpan", "lua", "luajit", "tclsh", "wish", "julia", "octave", "scilab", "haskell", "ghc", "cabal", "stack", "ocaml", "opam", "dune", "swift", "zig", "nim", "crystal", "dart", "flutter", "elixir", "erlang", "clojure", "lein", "racket", "scheme", "guile", "fortran", "gfortran", "gnat", "ada", "cobol", "gnucobol")
alias(APPLICATIONS, "applications-science", "gnuplot", "wxmaxima")

# --- Databases ---------------------------------------------------------------
# A cylinder for every one of them, from the administration front ends to the
# servers nobody launches by hand.

alias(APPLICATIONS, "x-office-database", "dbeaver", "pgadmin", "mysql-workbench", "sqlitebrowser", "sqliteman", "sqlite", "postgresql", "mysql", "mariadb", "mongodb", "redis", "influxdb", "adminer", "phpmyadmin", "sqlectron")


# --- Terminals and shells ----------------------------------------------------
# One terminal emulator for all of them, and the terminal mark for the shells
# themselves: a shell is what a terminal runs, and there is no picture of a shell
# that is not a terminal.

alias(APPLICATIONS, "utilities-terminal", "rxvt", "byobu", "zellij", "bash", "zsh", "fish", "nushell", "elvish", "xonsh", "powershell", "pwsh", "starship")
alias(APPLICATIONS, "applications-system", "tmux", "screen")

# --- File managers and terminal file managers --------------------------------
# A file manager is a folder, which is the one thing every desktop already
# agrees about. The terminal ones get the terminal, because that is the window
# they are actually in.

alias(APPLICATIONS, "system-file-manager", "doublecmd", "sunflower")
alias(APPLICATIONS, "utilities-terminal", "ranger", "yazi", "lf", "nnn", "vifm", "mc", "joshuto", "broot", "xplr", "felix", "clifm")
alias(APPLICATIONS, "search", "kfind", "recoll")

# --- Archivers and disk tools ------------------------------------------------
# The parcel for everything that packs a file up, the partition mark for the
# tools that change a disk, and the doughnut for the ones that only measure it.

alias(APPLICATIONS, "archive-manager", "squeeze", "7z", "p7zip", "unzip", "zip", "arj", "unrar", "zstd")
alias(APPLICATIONS, "gparted", "qt-fsarchiver", "usb-creator")
alias(APPLICATIONS, "panel-disk", "baobab", "qdirstat", "ncdu", "duf", "dust", "gdu", "dupeguru", "fdupes", "jdupes", "czkawka", "rmlint", "fslint")

# --- System monitors and hardware --------------------------------------------
# The line chart for anything that reports on the machine, the processor for the
# tools that talk to a device, and the game controller for the ones that remap
# an input device's buttons.

alias(APPLICATIONS, "system-monitor", "htop", "btop", "bashtop", "bpytop", "glances", "nmon", "iotop", "iftop", "nethogs", "lsof", "stacer")
alias(APPLICATIONS, "cpu", "hardinfo", "cpu-x", "inxi", "neofetch", "fastfetch", "screenfetch", "psensor", "xsensors", "lm-sensors", "solaar")
alias(APPLICATIONS, "panel-color", "piper", "openrgb", "polychromatic", "liquidctl", "coolero")
alias(APPLICATIONS, "input-gaming", "input-remapper", "antimicrox", "key-mapper", "jstest-gtk", "evtest", "xinput", "wacom")

# --- Package managers and software centres -----------------------------------
# The shopping bag for the ones a user is meant to see, the system cog for the
# ones that only appear in a script.

alias(APPLICATIONS, "software-centre", "pamac-manager", "gnome-packagekit", "muon", "octopi", "flatseal", "snap-store", "apt-notifier", "software-properties-gtk", "software-properties-kde")
alias(APPLICATIONS, "applications-system", "aptitude", "apt", "dpkg", "dnf", "yumex", "zypper", "yast", "pacman", "yay", "paru", "pamac", "flatpak", "snap", "nix", "guix", "brew", "emerge", "portage", "add-apt-repository")

# --- Virtualisation, containers and emulation --------------------------------
# The computer for the machines that run machines, the category mark for the
# wine bottles and the games console for the emulators, because an emulator is
# how a user plays the game.

alias(APPLICATIONS, "computer", "virt-viewer", "libvirt", "vagrant", "docker-compose", "podman", "podman-desktop", "distrobox", "toolbox", "lxc", "lxd", "incus", "multipass", "kubernetes", "kubectl", "helm", "k9s", "minikube", "lens", "vmware-workstation")
alias(APPLICATIONS, "applications-games", "wine", "winecfg", "winetricks", "proton", "protonup-qt", "bottles", "playonlinux", "dosbox", "dosbox-x", "scummvm", "retroarch", "mednafen", "mupen64plus", "pcsx2", "rpcs3", "dolphin-emu", "ppsspp", "yuzu", "ryujinx", "citra", "cemu", "duckstation", "hatari", "vice", "fs-uae", "snes9x", "desmume", "melonds", "mgba", "visualboyadvance")

# --- Games -------------------------------------------------------------------
# Everything that is played rather than used, with the launchers a user starts
# them from.

alias(APPLICATIONS, "applications-games", "steam", "lutris", "heroic", "gamehub", "itch", "minigalaxy", "gog-galaxy", "minecraft", "minetest", "luanti", "0ad", "supertuxkart", "supertux", "wesnoth", "openttd", "xonotic", "teeworlds", "sauerbraten", "freedoom", "openra", "warzone2100", "veloren", "endless-sky", "gnome-games", "aisleriot", "quadrapassel", "five-or-more", "four-in-a-row", "iagno", "lightsoff", "swell-foop", "atomix")

# --- Networking and remote access --------------------------------------------
# The globe for anything that talks to a network, the computer for the remote
# desktops, the letter-box arrow for the file transfers and the plug for the
# tunnels.

alias(APPLICATIONS, "network", "networkmanager", "connman-gtk", "wpa_gui", "iwd", "blueman", "gnome-bluetooth", "remmina", "vinagre", "freerdp", "xfreerdp", "tigervnc", "x11vnc", "novnc", "barrier", "deskflow", "syncthing", "winscp", "filezilla", "gftp")
alias(APPLICATIONS, "computer", "rustdesk", "anydesk", "teamviewer", "nomachine", "x2go")
alias(APPLICATIONS, "download", "rclone", "curl", "wget", "aria2", "rtorrent")
alias(APPLICATIONS, "network-server", "openvpn", "wireguard", "wg-quick", "tailscale", "zerotier", "openssh", "sshfs", "mosh", "putty", "moserial", "minicom", "cutecom")

# --- Security and privacy ----------------------------------------------------
# The shield for the tools that defend the machine, the key for the ones that
# keep a password, and a plain marked page for the encrypted volumes.

alias(APPLICATIONS, "panel-shield", "firejail", "gufw", "firewall-config", "firewalld", "ufw", "iptables", "nftables", "clamtk", "clamav", "rkhunter", "chkrootkit", "lynis", "wireshark", "tshark", "nmap", "zenmap", "tcpdump", "mitmproxy", "burpsuite", "zaproxy", "john", "hashcat", "aircrack-ng", "kismet", "metasploit", "ettercap", "bettercap", "sqlmap", "tor", "torsocks", "onionshare", "nyx")
alias(APPLICATIONS, "lock", "keepass2", "1password", "gopass", "gpa", "gnupg", "gpg2", "age", "sops", "seahorse")
alias(APPLICATIONS, "emblem-encrypted", "veracrypt", "cryptomator", "zulucrypt", "encfs", "gocryptfs")

# --- Accessibility, printing, fonts, power and the rest ----------------------
# The remaining categories, each one short because the programs in it are the
# ones a desktop shows in a settings page rather than in a launcher.

alias(APPLICATIONS, "panel-accessibility", "orca", "espeak", "espeak-ng", "festival", "flite", "speech-dispatcher", "brltty", "accerciser", "caribou", "onboard", "florence", "mousetweaks", "dasher", "kmag", "kmousetool", "kmouth", "jovie", "simon", "at-spi")
alias(APPLICATIONS, "printer", "hp-setup", "hplip", "hp-toolbox", "cups")
alias(APPLICATIONS, "font-manager", "fontforge", "birdfont", "fontmatrix", "typecatcher")
alias(APPLICATIONS, "accessories-character-map", "fc-list", "fc-cache")
alias(APPLICATIONS, "panel-power", "upower", "power-profiles-daemon", "auto-cpufreq", "tlp", "tlpui", "powertop", "cpupower-gui", "thermald", "laptop-mode-tools", "system76-power", "slimbookbattery")
alias(APPLICATIONS, "input-keyboard", "fcitx", "fcitx5", "fcitx5-configtool", "ibus", "ibus-setup", "scim", "uim", "mozc", "anthy", "chewing", "hime", "gcin", "keyd", "xmodmap", "setxkbmap", "keyboard-configuration", "xev")
alias(APPLICATIONS, "panel-display", "xrandr", "nvidia-settings", "wdisplays", "wlr-randr", "kanshi", "autorandr")
alias(APPLICATIONS, "panel-brightness", "redshift", "gammastep", "wlsunset", "brightnessctl", "light", "xbacklight")
alias(APPLICATIONS, "preferences-desktop-theme", "galternatives", "caffeine", "nwg-look")
alias(APPLICATIONS, "media-flash", "unetbootin", "balena-etcher", "etcher", "startup-disk-creator", "ventoy", "woeusb", "popsicle", "fedora-media-writer", "suse-studio", "imagewriter")
alias(APPLICATIONS, "emblem-synchronizing", "clonezilla", "timeshift", "deja-dup", "backintime", "borg", "vorta", "duplicity", "rsnapshot", "backup-tool", "luckybackup", "grsync")
alias(APPLICATIONS, "applications-utilities", "yad", "galculator")


# --- the last of the program names ----------------------------------------------
# Eighteen names the program tables above had not reached, and the two magnifiers
# the accessibility packages ship. They are aliases onto the picture the domain
# already draws rather than new artwork, because none of them is a mark anyone
# would recognise on its own: ``fterm`` is a terminal, ``dwb`` is a browser, and
# a magnifier is the search mark whether it belongs to GNOME or to Compiz.

alias(APPLICATIONS, "utilities-terminal", "fterm", "hyper", "tabby", "dd")
alias(APPLICATIONS, "applications-internet", "surf", "dwb", "uzbl", "nyxt")
alias(APPLICATIONS, "x-office-database", "libreoffice-base")
alias(APPLICATIONS, "applications-office", "libreoffice-math")
alias(APPLICATIONS, "code", "R")
alias(APPLICATIONS, "computer", "docker")
alias(APPLICATIONS, "applications-games", "gnome-control-center-games")
alias(APPLICATIONS, "search", "gnome-shell-magnifier", "compiz-magnifier")
alias(APPLICATIONS, "font-manager", "font-manager")
alias(APPLICATIONS, "battery-good", "mate-power-manager")
alias(APPLICATIONS, "input-keyboard", "lxqt-globalkeyshortcuts")
