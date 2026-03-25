import os, time

class gcNeed:
    def __init__(self):
        self.pathRecord = os.getcwd()

        self.ProgramList = (
            # library
            "zip", "unzip", "unrar", "p7zip",
            "expac", # pacman database
            "jshon", # json parse
            "gvfs-mtp", # android mtp
            # mtpfs missing
            "exfat-utils", # old tools
            "a52dec", # sound
            "faac", # AAC encode
            "faad2", # AAC decode
            "jasper", # jpeg-2000
            "fuse-exfat", # mount exfat file system
            "lame", # mp3 encode
            "libdca", # dts sound decode
            "libdv", # DV old digital video format
            "gst-libav",
            "libmad",
            "libtheora",
            "libmpeg2",
            "wavpack",
            "x264",
            "xvidcore",
            "libdvdcss",
            "libdvdread",
            "libdvdnav",
            "dvd+rw-tools",
            "dvdauthor",
            # dvdgrab missing
            "lib32-alsa-lib",
            "lib32-alsa-plugins",
            # lib32-alsa-libpulse missing
            # lib32-alsa-oss missing
            "net-tools",
            "xsel",
            "pcre", "pcre2", "lib32-pcre", "lib32-pcre2",
            "util-linux", "util-linux-libs", "lib32-util-linux",
            "xz", "lib32-xz",
            "joystick", "evtest",

            # extra
            "noto-fonts-cjk", "noto-fonts-emoji", "noto-fonts",
            "xdg-desktop-portal", "xdg-desktop-portal-gtk",

            # Programs
            "gparted",
            "vlc",
            "conky",
            "arandr",
            "btop",
            "jdk-openjdk",
            # bchunk missing
            "dmenu",
            "make", "cmake",
            "openssh-openrc",
            # timidity missing
            "fail2ban-openrc",
            "deluge-gtk",
            "mkinitcpio",
            "feh",
            "dunst",
            "zathura", "zathura-pdf-poppler",
            "ristretto", 
            "lxapperance", "lxapperance-obconf",
            "scrot",
            "npm",
            # nrg2iso missing
            "yt-dlp",
            "ncdu",
            # irqbalance only for systemd
            "tlp-openrc", "cpupower-openrc",
        )

        self.rcUpdate = (
            "tlp",
            "cpupower",
            "fail2ban",
            "sshd"
        )

        self.devTools = (
            "gimp",
            "audacity",
            "krita",
            "inkscape",
            "libreoffice-fresh"
        )

    def Install(self):
        for i in self.ProgramList:
            os.system(f"sudo pacman -Sy --noconfirm {i}")
    
    def InstallDev(self):
        for i in self.devTools:
            os.system(f"sudo pacman -Sy --noconfirm {i}")

    def LastSettings(self):
        for i in self.rcUpdate:
            os.system(f"sudo rc-update add {i} default")
            print(f"sudo rc-update add {i} default")
            time.sleep(1)

            os.system(f"sudo rc-service {i} start")
            print(f"sudo rc-service {i} start")
            time.sleep(1)

        os.system("sudo cpupower frequency-set -g ondemand")
        print("sudo cpupower frequency-set -g ondemand > performance")
        time.sleep(1)

    def uCode(self):
        while True:
            uInput = input("Intel or Amd: ")
            if "exit" == uInput:
                break
            elif len(uInput) > 0:
                if "intel" in uInput.lower():
                    os.system("sudo pacman -Sy --noconfirm intel-ucode")
                    break
                elif "amd" in uInput.lower():
                    os.system("sudo pacman -Sy --noconfirm amd-ucode")
                    break
                else:
                    print("???? intel or amd > exit")

    def InstallQtile(self):
        self.pathRecord = os.getcwd()
        tmpPath = os.path.expanduser("~/tmp")
        
        if not os.path.exists(tmpPath):        
            os.mkdir("~/tmp")
        else:
            os.chdir(tmpPath)
            _nowPath = os.path.join(tmpPath, "yay")

            if not os.path.exists(_nowPath):
                os.system("git clone https://aur.archlinux.org/yay.git")

            if os.path.exists(_nowPath):
                os.chdir(_nowPath)
                os.system("makepkg -si")
 
            os.chdir(tmpPath)
            os.system("yay -S rar irssi")
            os.system("yay -Sy python-pip")

            os.system("pip install psutil cairocffi cffi xcffib iwlib")
            os.system("git clone https://github.com/qtile/qtile.git")
            _tempQtilePath = os.path.join(tmpPath, "qtile")
            os.chdir(_tempQtilePath)
            os.system("pip install ./")

            os.chdir(self.pathRecord)

            os.system("sudo cp qtile.desktop /usr/share/xsessions")
            os.system("sudo pacman -Sy tk python-adblock")

if __name__ == "__main__":
    os.system("sudo sed -i 's/^#Color/Color/g' /etc/pacman.conf")
    os.system("sudo sed -i '/VerbosePkgLists/a ILoveCandy' /etc/pacman.conf")
    
    # Copy Dir
    os.system("sudo cp -r pip/ ~/.config")
    os.system("sudo cp -r Industrial /usr/share/lxdm/themes")
    os.system("cp -r qtile ~/.config")
    time.sleep(1)

    tmpPath = os.path.expanduser("~/tmp")
    if not os.path.exists(tmpPath):
        os.system("sudo nano /etc/pacman.conf")
        time.sleep(1)
    
        os.system("sudo pacman -Syu --noconfirm archlinux-keyring xorg-xinput")
        time.sleep(1)

    gc = gcNeed()

    while True:
        print("-: install all Programs and library :> all")
        print("-: install qtile -> qtile")
        print("-: install dev tools")
        print("-: install ucode -> ucode")
        print("-: apply settings -> settings")

        uInput = input(":> ")
        if "exit" == uInput:
            break
        elif len(uInput) > 0:
            if "all" in uInput.lower():
                gc.Install()
                time.sleep(1)
                os.chdir(gc.pathRecord)
                os.system("cp -r dunst qutebrowser zathura MangoHud ~/.config")
            elif "settings" in uInput.lower():
                gc.LastSettings()
            elif "qtile" in uInput.lower():
                gc.InstallQtile()
            elif "dev" in uInput.lower():
                gc.InstallDev()
            elif "ucode" in uInput.lower():
                gc.uCode()
        else:
            print("?????")
