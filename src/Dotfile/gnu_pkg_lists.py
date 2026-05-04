"""
Tek kaynak: gcNeed / canli ISO icin pacman paket gruplari.
main.py ve Distro/scripts/emit-packages-x86_64.py buradan okur.
"""

# --- ISO icin sabit (resmi depolar; multilib dahil) ---
ISO_BOOTSTRAP = (
    "amd-ucode",
    "intel-ucode",
    "arch-install-scripts",
    "base",
    "btrfs-progs",
    "cryptsetup",
    "dosfstools",
    "e2fsprogs",
    "efibootmgr",
    "grub",
    "linux",
    "linux-firmware",
    "mkinitcpio",
    "mkinitcpio-archiso",
    "nano",
    "networkmanager",
    "openssh",
    "parted",
    "sudo",
    "syslinux",
    "systemd-resolvconf",
    "terminus-font",
    "wpa_supplicant",
    "xfsprogs",
    "archlinux-keyring",
)

# Canli masaustu: main.py akisi + grafik oturumu
ISO_DESKTOP = (
    "xorg-server",
    "xorg-xinit",
    "xorg-xinput",
    "xorg-xrandr",
    "xorg-xsetroot",
    "xterm",
    "mesa",
    "vulkan-intel",
    "vulkan-radeon",
    "xf86-input-libinput",
    "lxdm",
    "qtile",
    "picom",
    "pipewire",
    "pipewire-alsa",
    "pipewire-pulse",
    "wireplumber",
    "alsa-utils",
    "pavucontrol",
    "qutebrowser",
    "mangohud",
    "plymouth",
    "zram-generator",
)

# main.py ProgramList
PROGRAM_LIST = (
    "zip unzip unrar p7zip",
    "jq",
    "gvfs-mtp",
    "ffmpeg",
    "lame x264 xvidcore",
    "libdvdcss libdvdread libdvdnav",
    "net-tools",
    "xsel",
    "pcre2",
    "util-linux",
    "xz",
    "evtest",
    "wget",
    "noto-fonts noto-fonts-cjk noto-fonts-emoji",
    "xdg-desktop-portal xdg-desktop-portal-gtk",
    "gparted vlc vlc-plugins-all conky arandr btop",
    "jdk-openjdk",
    "dmenu",
    "make cmake",
    "openssh",
    "fail2ban",
    "deluge-gtk",
    "feh",
    "dunst",
    "zathura zathura-pdf-poppler",
    "ristretto",
    "lxappearance lxappearance-obconf",
    "scrot",
    "npm",
    "yt-dlp",
    "ncdu",
    "irqbalance tlp cpupower",
)

RC_SERVICES = (
    "irqbalance",
    "tlp",
    "cpupower",
    "fail2ban",
    "sshd",
)

DEV_TOOLS = ("gimp", "audacity", "krita", "inkscape", "libreoffice-fresh")

FOR_GAME = (
    "steam",
    "gamemode lib32-gamemode",
    "protontricks",
    "gstreamer",
    "gst-plugins-base-libs lib32-gst-plugins-base-libs",
    "gst-libav gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-plugins-base-libs lib32-gst-plugins-base-libs",
    "libpulse lib32-libpulse",
    "alsa-plugins lib32-alsa-plugins",
    "alsa-lib lib32-alsa-lib",
    "sqlite lib32-sqlite",
    "libxcomposite lib32-libxcomposite",
    "lib32-openal",
    "lib32-libjpeg-turbo",
    "vulkan-icd-loader lib32-vulkan-icd-loader vulkan-tools",
    "gtk3 lib32-gtk3",
    "sdl2 lib32-sdl2",
    "giflib lib32-giflib",
    "gnutls lib32-gnutls",
    "v4l-utils lib32-v4l-utils",
    "ocl-icd lib32-ocl-icd",
    "libva lib32-libva",
    "glfw",
    "mpg123 lib32-mpg123",
    "opencl-icd-loader lib32-opencl-icd-loader openal",
    "libxslt",
    "ttf-liberation",
)

WINE_PKGS = "wine-staging wine-gecko wine-mono winetricks"

# Resmi `qtile` paketi bagimliliklari getirir; pip ile git kurulum ISO’da kullanilmaz.

RAYLIB_DEPS = ("cmake", "ninja", "git", "libx11", "libxcursor", "libxinerama", "libxrandr")


def _tokens(line: str) -> list[str]:
    return [p for p in line.split() if p]


def flatten_groups(*groups: tuple[str, ...]) -> list[str]:
    out: list[str] = []
    for g in groups:
        for line in g:
            out.extend(_tokens(line))
    return out


def all_live_packages() -> list[str]:
    """ISO packages.x86_64 icin tek liste (yinelemesiz, sirali)."""
    chunks: list[str] = []
    chunks.extend(ISO_BOOTSTRAP)
    chunks.extend(ISO_DESKTOP)
    chunks.extend(flatten_groups(PROGRAM_LIST))
    chunks.extend(flatten_groups(FOR_GAME))
    chunks.extend(_tokens(WINE_PKGS))
    chunks.extend(DEV_TOOLS)
    chunks.extend(RAYLIB_DEPS)
    # lib32 steam etc. — multilib acik olmali
    seen: set[str] = set()
    ordered: list[str] = []
    for p in chunks:
        if p not in seen:
            seen.add(p)
            ordered.append(p)
    return sorted(ordered)
