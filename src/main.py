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
            "mtpfs",
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
            "lib32-alsa-oss",
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
            "bchunk",
            "dmenu",
            "make", "cmake",
            "openssh",
            "imidity++",
            "fail2ban",
            "deluge-gtk",
            "mkinitcpio",
            "feh",
            "dunst",
            "zathura", "zathura-pdf-poppler",
            "ristretto", 
            "lxapperance", "lxapperance-obconf",
            "scrot",
            "npm",
            "nrg2iso",
            "yt-dlp",
            "ncdu",
            "irqbalance", "tlp", "cpupower",
        )

        self.rcUpdate = (
	        "irqbalance",
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
            time.sleep(1)

        for i in self.rcUpdate:
            os.system(f"sudo systemctl enable {i}.service")
            print(f"sudo systemctl enable {i}.service")
            time.sleep(1)

        os.system("sudo cpupower frequency-set -g ondemand")
        print("sudo cpupower frequency-set -g ondemand > performance")
        time.sleep(1)


    def InstallDev(self):
        for i in self.devTools:
            os.system(f"sudo pacman -Sy --noconfirm {i}")

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
            os.mkdir(tmpPath)
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

            os.system("sudo pacman -Sy --noconfirm python-psutil python-cairocffi python-cffi python-xcffib python-iwlib")
            os.system("git clone https://github.com/qtile/qtile.git")
            _tempQtilePath = os.path.join(tmpPath, "qtile")
            os.chdir(_tempQtilePath)
            os.system("pip install ./")

            os.chdir(self.pathRecord)

            os.system("sudo cp qtile.desktop /usr/share/xsessions")
            os.system("sudo pacman -Sy --noconfirm tk python-adblock")

    def InstallGrubAndPlyMouth(self):
        os.chdir(self.pathRecord)
        tmpPath = os.path.expanduser("~/tmp")

        if os.path.exists(tmpPath):
            os.system("yay -S update-grub plymouth")
            os.system("sudo cp -r minimal/ /boot/grub/themes")
            os.system("sudo update-grub")
            os.system("sudo cp -r gnuchanBoot  /usr/share/plymouth/themes/")
            os.system("sudo cp mkinitcpio.conf /etc/")
            os.system("sudo cp grub /etc/default/")
            os.system("sudo mkinitcpio -P linux")
            os.system("sudo grub-mkconfig -o /boot/grub/grub.cfg")
            os.system("sudo cp plymouthd.conf /etc/plymouth/")
            os.system("sudo plymouth-set-default-theme -R gnuchanBoot")

    def InstallVimThemeXterm(self):
        os.chdir(self.pathRecord)
        tmpPath = os.path.expanduser("~/tmp")

        if os.path.exists(tmpPath):

            os.system("cp .Xresources ~/")
            os.system("cp .bashrc ~/")
            os.system("cp .zshrc ~/")
            os.system("sudo pacman -Sy --noconfirm zsh-autosuggestions zsh-syntax-highlighting")

            os.system("cp .vimrc ~/")
            os.chdir(tmpPath)
            os.system("git clone https://github.com/vim/vim.git")
            _temp = os.path.join(tmpPath, "vim")
            print(f"----------------> {_temp}")
            if os.path.exists(_temp):
                os.chdir(_temp)
                os.system("./configure --prefix=/usr/local --enable-python3interp --enable-rubyinterp --enable-luainterp --enable-perlinterp --with-features=huge")
                os.system("make")
                os.system("sudo make install")

                time.sleep(1)
                os.system("curl -fLo ~/.vim/autoload/plug.vim --create-dirs https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim")
                os.system("vim +PlugInstall +qall")

                time.sleep(1)
                _temp = os.path.expanduser("~/.vim/plugged/YouCompleteMe")
                os.chdir(_temp)
                os.system("python3 install.py --clangd-completer")


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
        print("-: Change Lxdm Thene -> .lxdm")
        print("-: install dev tools")
        print("-: install ucode -> ucode")
        print("-: install grub and plymouth theme -> gptheme")
        print("-: install vim and xterm theme -> vim")
        print("-: exit")

        uInput = input(":> ")
        if "exit" == uInput:
            break
        elif len(uInput) > 0:
            if "all" in uInput.lower():
                gc.Install()
                time.sleep(1)
                os.chdir(gc.pathRecord)
                os.system("cp -r dunst qutebrowser zathura MangoHud ~/.config")
            elif "qtile" in uInput.lower():
                gc.InstallQtile()
            elif "lxdm" in uInput.lower():
                os.system("sudo cp -r Industrial /usr/share/lxdm/themes")
            elif "dev" in uInput.lower():
                gc.InstallDev()
            elif "ucode" in uInput.lower():
                gc.uCode()
            elif "gptheme" in uInput.lower():
                gc.InstallGrubAndPlyMouth()
            elif "vim" in uInput.lower():
                gc.InstallVimThemeXterm()
        else:
            print("?????")
