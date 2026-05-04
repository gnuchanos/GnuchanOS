import os
import time

from gnu_pkg_lists import (
    DEV_TOOLS,
    FOR_GAME,
    PROGRAM_LIST,
    RC_SERVICES,
    WINE_PKGS,
)


class gcNeed:
    def __init__(self) -> None:
        self.pathRecord = os.path.abspath(os.path.dirname(__file__))

    def install(self) -> None:
        for line in PROGRAM_LIST:
            os.system(f"sudo pacman -Sy --noconfirm {line}")
            print(f"sudo pacman -Sy --noconfirm {line}", flush=True)
            time.sleep(1)

        for svc in RC_SERVICES:
            os.system(f"sudo systemctl enable {svc}.service")
            print(f"sudo systemctl enable {svc}.service", flush=True)
            time.sleep(1)

        os.system("sudo cpupower frequency-set -g ondemand")
        print("sudo cpupower frequency-set -g ondemand", flush=True)
        time.sleep(1)

    def install_game(self) -> None:
        tmp_dir = os.path.expanduser("~/.steam/steam/compatibilitytools.d")
        os.makedirs(tmp_dir, exist_ok=True)
        os.chdir(tmp_dir)
        u = input("ProtonGE URL (tar.gz) :> ").strip()
        if len(u) > 0:
            os.system(f"wget {u}")
        if os.path.isdir(tmp_dir):
            os.chdir(tmp_dir)
            for name in os.listdir(tmp_dir):
                if name.endswith("tar.gz"):
                    os.system(f"tar -vxf {name}")
                    extracted = os.path.join(tmp_dir, name.replace(".tar.gz", ""))
                    if os.path.exists(extracted):
                        try:
                            os.remove(name)
                        except OSError:
                            pass
        for line in FOR_GAME:
            os.system(f"sudo pacman -Sy --noconfirm {line}")

    def install_wine(self) -> None:
        os.system(f"sudo pacman -Sy --noconfirm {WINE_PKGS}")

    def install_dev(self) -> None:
        for pkg in DEV_TOOLS:
            os.system(f"sudo pacman -Sy --noconfirm {pkg}")

    def ucode(self) -> None:
        while True:
            u = input("Intel or AMD (exit) :> ").strip()
            if u.lower() == "exit":
                return
            if len(u) == 0:
                continue
            if "intel" in u.lower():
                os.system("sudo pacman -Sy --noconfirm intel-ucode")
                return
            if "amd" in u.lower():
                os.system("sudo pacman -Sy --noconfirm amd-ucode")
                return
            print("intel | amd | exit", flush=True)

    def install_qtile(self) -> None:
        """Depo qtile; eski pip/git yolu yerine resmi paket."""
        os.system(
            "sudo pacman -Sy --noconfirm qtile picom "
            "python-psutil python-cairocffi python-cffi python-xcffib python-iwlib tk"
        )
        desk = os.path.join(self.pathRecord, "qtile.desktop")
        if os.path.isfile(desk):
            os.system(f"sudo cp {desk} /usr/share/xsessions/")
        else:
            print("missing qtile.desktop", flush=True)

    def install_grub_and_plymouth(self) -> None:
        os.chdir(self.pathRecord)
        tmp_path = os.path.expanduser("~/tmp")
        minimal = os.path.join(self.pathRecord, "minimal")
        gnuchan_boot = os.path.join(self.pathRecord, "gnuchanBoot")
        mkinit = os.path.join(self.pathRecord, "mkinitcpio.conf")
        grub_cfg = os.path.join(self.pathRecord, "grub")
        plymouthd = os.path.join(self.pathRecord, "plymouthd.conf")

        if not os.path.isdir(tmp_path):
            print("~/tmp yok; once olustur ve yay ile gerekli paketleri kur.", flush=True)
            return

        os.system("yay -S --noconfirm update-grub plymouth 2>/dev/null || true")

        if os.path.isdir(minimal):
            os.system("sudo cp -r minimal/ /boot/grub/themes/")
        else:
            print("Missing minimal dir", flush=True)

        os.system("sudo update-grub 2>/dev/null || true")

        if os.path.isdir(gnuchan_boot):
            os.system("sudo cp -r gnuchanBoot /usr/share/plymouth/themes/")
        else:
            print("Missing gnuchanBoot Dir", flush=True)

        if os.path.isfile(mkinit):
            os.system("sudo cp mkinitcpio.conf /etc/")
        else:
            print("Missing mkinitcpio.conf", flush=True)

        if os.path.isfile(grub_cfg):
            os.system("sudo cp grub /etc/default/")
        os.system("sudo mkinitcpio -P linux 2>/dev/null || sudo mkinitcpio -P")
        os.system("sudo grub-mkconfig -o /boot/grub/grub.cfg")

        if os.path.isfile(plymouthd):
            os.system("sudo cp plymouthd.conf /etc/plymouth/")
            os.system("sudo plymouth-set-default-theme -R gnuchanBoot")
        else:
            print("Missing plymouthd.conf", flush=True)

    def install_vim_theme_xterm(self) -> None:
        os.chdir(self.pathRecord)
        tmp_path = os.path.expanduser("~/tmp")
        if not os.path.isdir(tmp_path):
            print("~/tmp gerekli.", flush=True)
            return

        os.system("cp .Xresources ~/ 2>/dev/null || true")
        os.system("cp .bashrc ~/ 2>/dev/null || true")
        os.system("cp .zshrc ~/ 2>/dev/null || true")
        os.system(
            "sudo pacman -Sy --noconfirm zsh-autosuggestions zsh-syntax-highlighting"
        )
        os.system("cp .vimrc ~/ 2>/dev/null || true")
        os.chdir(tmp_path)
        if not os.path.isdir(os.path.join(tmp_path, "vim")):
            os.system("git clone https://github.com/vim/vim.git")
        vim_dir = os.path.join(tmp_path, "vim")
        if os.path.isdir(vim_dir):
            os.chdir(vim_dir)
            os.system(
                "./configure --prefix=/usr/local --enable-python3interp --enable-rubyinterp "
                "--enable-luainterp --enable-perlinterp --with-features=huge"
            )
            os.system("make")
            os.system("sudo make install")
            time.sleep(1)
            os.system(
                "curl -fLo ~/.vim/autoload/plug.vim --create-dirs "
                "https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim"
            )
            os.system("vim +PlugInstall +qall")
            time.sleep(1)
            ycm = os.path.expanduser("~/.vim/plugged/YouCompleteMe")
            if os.path.isdir(ycm):
                os.chdir(ycm)
                os.system("python3 install.py --clangd-completer --ts-completer --java-completer")
        os.chdir(self.pathRecord)

    def themes_and_icons(self) -> None:
        os.system("sudo rm -rf /usr/share/themes/* /usr/share/icons/*")
        themes = "/usr/share/themes"
        icons = "/usr/share/icons"
        if not (os.path.isdir(themes) and os.path.isdir(icons)):
            return
        theme_tar = os.path.join(self.pathRecord, "GnuChanOSTheme.tar")
        cursor_tar = os.path.join(self.pathRecord, "cursor.tar")
        icon_tar = os.path.join(self.pathRecord, "icon.tar")
        if os.path.isfile(theme_tar):
            os.system(f"sudo tar -vxf {theme_tar} -C /usr/share/themes")
        else:
            print("Missing GnuChanOSTheme.tar", flush=True)
        if os.path.isfile(cursor_tar):
            os.system(f"sudo tar -vxf {cursor_tar} -C /usr/share/icons")
        else:
            print("Missing cursor.tar", flush=True)
        if os.path.isfile(icon_tar):
            os.system(f"sudo tar -vxf {icon_tar} -C /usr/share/icons")
        else:
            print("Missing icon.tar", flush=True)

        gtk_cache = os.path.expanduser("~/.cache/gtk-3.0")
        if os.path.isdir(gtk_cache):
            os.system(f"rm -rf {gtk_cache}")
        local_icons = os.path.expanduser("~/.local/share/icons")
        if os.path.isdir(local_icons):
            os.system(f"rm -rf {local_icons}")
        if os.path.isdir("/etc/gtk-3.0"):
            os.system("sudo cp settings.ini /etc/gtk-3.0/")

    def fix_stupid_touchpad(self) -> None:
        os.system("sudo pacman -Sy --noconfirm xf86-input-libinput")
        os.chdir(self.pathRecord)
        conf = os.path.join(self.pathRecord, "30-touchpad.conf")
        if os.path.isfile(conf):
            os.system("sudo mkdir -p /etc/X11/xorg.conf.d")
            os.system(f"sudo cp 30-touchpad.conf /etc/X11/xorg.conf.d/")

    def install_zram(self) -> None:
        os.system("sudo pacman -Sy --noconfirm zram-generator")
        cfg = os.path.join(self.pathRecord, "files", "zram-generator.conf")
        if os.path.isfile(cfg):
            os.system(f"sudo cp {cfg} /etc/systemd/zram-generator.conf")
        os.system("sudo systemctl daemon-reload")

    def raylib_from_github(self) -> None:
        os.system(
            "sudo pacman -Sy --noconfirm cmake ninja git libx11 libxcursor libxinerama libxrandr"
        )
        tmp = os.path.expanduser("~/tmp")
        os.makedirs(tmp, exist_ok=True)
        os.chdir(tmp)
        raylib_dir = os.path.join(tmp, "raylib")
        if not os.path.isdir(raylib_dir):
            os.system("git clone https://github.com/raysan5/raylib")
        if not os.path.isdir(raylib_dir):
            return
        os.chdir(raylib_dir)
        build = os.path.join(raylib_dir, "build")
        os.makedirs(build, exist_ok=True)
        os.chdir(build)
        os.system("cmake .. -DBUILD_SHARED_LIBS=ON")
        os.system("cmake --build .")
        os.system("sudo cp libraylib.so* /usr/lib/ 2>/dev/null || true")
        os.system("sudo cp ../src/raylib.h /usr/include/ 2>/dev/null || true")
        os.chdir(self.pathRecord)


if __name__ == "__main__":
    os.system("sudo sed -i 's/^#Color/Color/g' /etc/pacman.conf")
    os.system(
        "sudo sh -c \"grep -q '^ILoveCandy' /etc/pacman.conf || "
        "sed -i '/VerbosePkgLists/a ILoveCandy' /etc/pacman.conf\""
    )

    root_dot = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root_dot)
    os.system("cp -r pip ~/.config 2>/dev/null || true")
    os.system("sudo mkdir -p /usr/share/lxdm/themes")
    os.system("sudo cp -r Industrial /usr/share/lxdm/themes/ 2>/dev/null || true")
    os.system("mkdir -p ~/.config && cp -r qtile ~/.config/ 2>/dev/null || true")
    time.sleep(1)

    tmp_path = os.path.expanduser("~/tmp")
    if not os.path.isdir(tmp_path):
        os.system("sudo pacman -Sy --noconfirm archlinux-keyring xorg-xinput")

    gc = gcNeed()

    while True:
        print("-: install all Programs and library :> all", flush=True)
        print("-: install qtile -> qtile", flush=True)
        print("-: Change Lxdm Theme -> lxdm", flush=True)
        print("-: install dev tools -> dev", flush=True)
        print("-: install ucode -> ucode", flush=True)
        print("-: install grub and plymouth theme -> gptheme", flush=True)
        print("-: install vim and xterm theme -> vim", flush=True)
        print("-: install Zram -> zr", flush=True)
        print("-: install Steam/Gamemode -> sg", flush=True)
        print("-: install Wine -> w", flush=True)
        print("-: exit", flush=True)

        u = input(":> ").strip()
        if u == "exit":
            break
        if len(u) == 0:
            print("?????", flush=True)
            continue
        low = u.lower()
        if "all" in low:
            gc.install()
            time.sleep(1)
            os.chdir(gc.pathRecord)
            os.system("mkdir -p ~/.config")
            os.system("cp -r dunst qutebrowser zathura ~/.config/ 2>/dev/null || true")
            mh = os.path.join(gc.pathRecord, "extra", "Select Dotfiles", "MangoHud")
            if os.path.isdir(mh):
                os.system('cp -r "extra/Select Dotfiles/MangoHud" ~/.config/MangoHud')
        elif "qtile" in low:
            gc.install_qtile()
        elif "lxdm" in low:
            os.system("sudo cp -r Industrial /usr/share/lxdm/themes/")
        elif "dev" in low:
            gc.install_dev()
        elif "ucode" in low:
            gc.ucode()
        elif "gptheme" in low:
            gc.install_grub_and_plymouth()
        elif "vim" in low:
            gc.install_vim_theme_xterm()
        elif "zr" in low:
            gc.install_zram()
        elif "sg" in low:
            gc.install_game()
        elif low == "w" or (low.startswith("w") and "wine" in low):
            gc.install_wine()
        else:
            print("?????", flush=True)
