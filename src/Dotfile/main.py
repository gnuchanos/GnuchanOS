import os, time

class gcNeed:
    def __init__(self):
        self.pathRecord = os.getcwd()

        self.ProgramList = (
            "zip", "unzip", "unrar", "p7zip",
            "jq",
            "gvfs-mtp",
            "ffmpeg",
            "lame", "x264", "xvidcore",
            "libdvdcss libdvdread libdvdnav",
            "net-tools",
            "xsel",
            "pcre2",
            "util-linux",
            "xz",
            "evtest",
            "wget",

            "noto-fonts", "noto-fonts-cjk", "noto-fonts-emoji",
            "xdg-desktop-portal", "xdg-desktop-portal-gtk",

            "gparted", "vlc vlc-plugins-all", "conky", "arandr", "btop",
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
            "irqbalance", "tlp", "cpupower",
        )

        self.rcUpdate = ("irqbalance", "tlp", "cpupower", "fail2ban", "sshd")
        self.devTools = ("gimp", "audacity", "krita", "inkscape", "libreoffice-fresh")

        self.ForGame = (
            "steam",
            "gamemode lib32-gamemode",
            "protontricks",
            "gstreamer",
            "gst-plugins-base-libs lib32-gst-plugins-base-libs",
            "gst-libav gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-plugins-base-libs lib32-gst-plugins-base-libs",
            # yay -Sy gst-plugins-{base,good,bad,ugly}
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
            "ttf-liberation"
        )

        self.Wine = "wine-staging wine-gecko wine-mono winetricks"

    def Install(self) -> None:
        for i in self.ProgramList:
            os.system(f"sudo pacman -Sy --noconfirm {i}")
            print(f"sudo pacman -Sy --noconfirm {i}")
            time.sleep(1)

        for i in self.rcUpdate:
            os.system(f"sudo systemctl enable {i}.service")
            print(f"sudo systemctl enable {i}.service")
            time.sleep(1)

        os.system("sudo cpupower frequency-set -g ondemand")
        print("sudo cpupower frequency-set -g ondemand > performance")
        time.sleep(1)

    def InstallGame(self) -> None:
        _tmpDir = os.path.expanduser("~/.steam/steam/compatibilitytools.d")

        if not os.path.exists(_tmpDir):
            os.mkdir(_tmpDir)

        os.chdir(_tmpDir)
        uInput = input("ProtonGE Link :> ")
        os.system(f"wget {uInput}")

        if os.path.exists(_tmpDir):
            os.chdir(_tmpDir)
            _tmpDirList = os.listdir(_temp)
            for i in _tmpDirList:
                if i.endswith("tar.gz"):
                    os.system("tar -vxf {i}")
                    _tmpDirGEPath = os.path.join(_tmpDir, i)
                    if os.path.exists(_tmpDirGEPath):
                        os.remove(i)

        for i in self.ForGame:
            os.system(f"sudo pacman -Sy --noconfirm {i}")

    def InstallWine(self) -> None:
        os.system(f"sudo pacman -Sy --noconfirm {self.Wine}")

    def InstallDev(self) -> None:
        for i in self.devTools:
            os.system(f"sudo pacman -Sy --noconfirm {i}")

    def uCode(self) -> None:
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

    def InstallQtile(self) -> None:
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
            
            _tempQtilePath = os.path.join(tmpPath, "qtile")

            if not os.path.exists(_tempQtilePath):
                os.system("git clone https://github.com/qtile/qtile.git")
            
            if os.path.exists(_tempQtilePath):
                os.chdir(_tempQtilePath)
                os.system("pip install ./")

            os.chdir(self.pathRecord)

            _TMPQtileDesktop = os.path.join(self.pathRecord, "qtile.desktop")
            if os.path.exists(_TMPQtileDesktop):
                os.system("sudo cp qtile.desktop /usr/share/xsessions")
            else:
                print("missing qtile.desktop")
            
            os.system("sudo pacman -Sy --noconfirm tk python-adblock")

    def InstallGrubAndPlyMouth(self) -> None:
        os.chdir(self.pathRecord)
        tmpPath = os.path.expanduser("~/tmp")

        _TMPMinimalDir = os.path.join(self.pathRecord, "minimal")
        _TMPGnuchanBoot = os.path.join(self.pathRecord, "gnuchanBoot")
        _TMPMkinitcpio = os.path.join(self.pathRecord, "mkinitcpio.conf")
        _TMPGrun = os.path.join(self.pathRecord, "grub")
        _TMPPlymouthd = os.path.join(self.pathRecord, "plymouthd.conf")
        

        if os.path.exists(tmpPath):
            os.system("yay -S update-grub plymouth")
            
            if os.path.exists(_TMPMinimalDir):
                os.system("sudo cp -r minimal/ /boot/grub/themes")
            else:
                print("Missing minimal dir")

            os.system("sudo update-grub")
            
            if os.path.exists(_TMPGnuchanBoot):
                os.system("sudo cp -r gnuchanBoot  /usr/share/plymouth/themes/")
            else:
                print("Missing gnuchanBoot Dir")

            if os.path.exists(_TMPMkinitcpio):
                os.system("sudo cp mkinitcpio.conf /etc/")
            else:
                print("Missing mkinitcpio.conf")

            os.system("sudo cp grub /etc/default/")
            os.system("sudo mkinitcpio -P linux")
            os.system("sudo grub-mkconfig -o /boot/grub/grub.cfg")

            if os.path.exists(_TMPPlymouthd):
                os.system("sudo cp plymouthd.conf /etc/plymouth/")
                os.system("sudo plymouth-set-default-theme -R gnuchanBoot")
            else:
                print("Missing plymouthd.conf")

    def InstallVimThemeXterm(self) -> None:
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

    def ThemesAndIcons(self) -> None:
        # Remove First Shits
        os.system("sudo rm -r /usr/share/themes/*")
        os.system("sudo rm -r /usr/share/icons/*")
       
        # Extract MINE GOD LEVEL SHIT
        _Themes = os.path.expanduser("/usr/share/themes")
        _Icons  = os.path.expanduser("/usr/share/icons")
        if os.path.exists(_Themes) and os.path.exists(_Icons):
            _TMPGnuChanOSTheme = os.path.join(self.pathRecord, "GnuChanOSTheme.tar")
            _TMPCursor = os.path.join(self.pathRecord, "cursor.tar")
            _TMPIcons = os.path.join(self.pathRecord, "icon.tar")

            if os.path.exists(_TMPGnuChanOSTheme):
                os.system("sudo tar -vxf GnuChanOSTheme.tar -C /usr/share/themes")
                print("sudo tar -vxf GnuChanOSTheme.tar -C /usr/share/themes")
            else:
                print("Missing GnuChanOSTheme.tar")

            if os.path.exists(_TMPCursor):
                os.system("sudo tar -vxf cursor.tar -C /usr/share/icons")
                print("sudo tar -vxf cursor.tar -C /usr/share/icons")
            else:
                print("Missing cursor.tar")

            if os.path.exists(_TMPIcons):
                os.system("sudo tar -vxf icon.tar -C /usr/share/icons")
                print("sudo tar -vxf icon.tar -C /usr/share/icons")
            else:
                print("Missing icon.tar")

        # REMOVE OLD SHITS
        _GTK3_0 = os.path.expanduser("~/.cache/gtk-3.0")
        if os.path.exists(_GTK3_0):
            os.system("rm -rf ~/.cache/gtk-3.0")
            print("rm -rf ~/.cache/gtk-3.0")

        _LOCAL_ICONS = os.path.expanduser("~/.local/share/icons")
        if os.path.exists(_LOCAL_ICONS):
            os.system("rm -rf ~/.local/share/icons")
            print("rm -rf ~/.local/share/icons")

        _ETC_GTK3_0 = os.path.expanduser("/etc/gtk-3.0")
        if os.path.exists(_ETC_GTK3_0):
            os.system("sudo cp settings.ini /etc/gtk-3.0")
            print("sudo cp settings.ini /etc/gtk-3.0")

    def FixStupidTouchpad(self) -> None:
        os.system("sudo pacman -Sy --noconfirm xf86-input-libinput")
        print("sudo pacman -Sy --noconfirm xf86-input-libinput")

        os.chdir(self.pathRecord)
        _TMP30Touchpad = os.path.join(self.pathRecord, "./30-touchpad.conf")
        if os.path.exists(_TMP30Touchpad):
            os.system("sudo cp 30-touchpad.conf /etc/X11/xorg.conf.d")
            print("sudo cp 30-touchpad.conf /etc/X11/xorg.conf.d")

    def InstallZram(self) -> None:
        os.system("sudo pacman -Sy --noconfirm zram-generator")
        os.system("sudo cp files/zram-generator.conf /etc/systemd")
        os.system("sudo systemctl daemon-reload")
        os.system("sudo systekctl start /dev/zram0")
        os.system("zramctli")


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
        print("-: install Zram -> zr")
        print("-: install Steam/Gamemode -> sg")
        print("-: install Wine -> w")
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
                print("sudo cp -r Industrial /usr/share/lxdm/themes")
            elif "dev" in uInput.lower():
                gc.InstallDev()
            elif "ucode" in uInput.lower():
                gc.uCode()
            elif "gptheme" in uInput.lower():
                gc.InstallGrubAndPlyMouth()
            elif "vim" in uInput.lower():
                gc.InstallVimThemeXterm()
            elif "zr" in uInput.lower():
                gc.InstallZram()
            elif "sg" in uInput.lower():
                self.InstallGame()
            elif 'w' in uInput.lower():
                self.InstallWine()
        else:
            print("?????")
