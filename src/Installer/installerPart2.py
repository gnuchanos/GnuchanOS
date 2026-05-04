from __future__ import annotations

import getpass
import os
import sys


def _run(cmd: str) -> int:
    """Run shell command; return exit code."""
    return os.system(cmd)


class Installer:
    def __init__(self) -> None:
        self.HostName_PC = "ThisIsMyKingdomCOME"
        self.UserName = "gnuchanos"
        self.SystemLang = "en_US"
        self.KeyMap = "us"
        self.Gpu = "nvidia"
        self.GpuDate = "old"
        self.DefaultDir = os.getcwd()
        self.Disk = self._load_disk_from_marker()

    @staticmethod
    def _load_disk_from_marker() -> str:
        marker = "/root/.installer_disk"
        if os.path.isfile(marker):
            try:
                with open(marker, encoding="utf-8") as f:
                    d = f.read().strip()
                    if d.startswith("/dev/"):
                        d = d[5:]
                    if d:
                        return d
            except OSError:
                pass
        return "sda"

    @property
    def disk_dev(self) -> str:
        return f"/dev/{self.Disk}"

    def check_time_zone(self, time_zone: str | None = None) -> None:
        if not time_zone:
            return
        _run(f'timedatectl | grep "{time_zone}"')

    def set_time_zone(self, zone_name: str | None = None, default: bool = False) -> None:
        if default:
            _run("ln -sf /usr/share/zoneinfo/Europe/Istanbul /etc/localtime")
            _run("hwclock --systohc")
            return
        while True:
            if zone_name:
                self.check_time_zone(time_zone=zone_name)
            print("Is timezone correct? YES | NO :> ", end="", flush=True)
            u = input().strip().lower()
            if "yes" in u and zone_name:
                _run(f"ln -sf /usr/share/zoneinfo/{zone_name} /etc/localtime")
                _run("hwclock --systohc")
            break

    def host_name(self, name: str | None = None, default: bool = False) -> None:
        if default:
            _run(f'echo "{self.HostName_PC}" > /etc/hostname')
            return
        if name:
            _run(f'echo "{name}" > /etc/hostname')

    def create_user_name(self, user_name: str, default: bool = False) -> None:
        if default:
            _run(f"useradd -m -G wheel -s /bin/bash {self.UserName}")
            return
        if user_name:
            _run(f"useradd -m -G wheel -s /bin/bash {user_name}")
            _run(f"usermod -aG wheel {user_name}")

    def change_user_password(self, user_name: str, password: str) -> None:
        if not password or not user_name:
            return
        for account in ("root", user_name):
            _run(f'echo "{account}:{password}" | chpasswd')

    def install_network_manager(self) -> None:
        _run("pacman -Sy --noconfirm networkmanager")
        _run("systemctl enable NetworkManager")

    def install_pipe_wire(self) -> None:
        _run(
            "pacman -Sy --noconfirm pipewire pipewire-alsa pipewire-jack "
            "wireplumber alsa-utils"
        )
        _run("systemctl enable --now pipewire pipewire-jack pipewire-alsa wireplumber")

    def install_yay(self) -> None:
        tmp_dir = os.getcwd()
        yay_dir = os.path.join(tmp_dir, "yay")
        if os.path.isdir(yay_dir):
            os.chdir(yay_dir)
            _run("git pull --ff-only")
        else:
            _run("git clone https://aur.archlinux.org/yay.git")
            os.chdir(yay_dir)
        _run("makepkg -si --noconfirm")
        os.chdir(tmp_dir)

    def install_gpu(self, gpu_name: str) -> None:
        g = gpu_name.strip().lower()
        if not g:
            print("Empty GPU choice.", flush=True)
            return
        if "nvidia" in g:
            print("New open driver (not linux-lts) -> osd | GTX 10xx -> 10xx", flush=True)
            u = input(":> ").strip().lower()
            if "osd" in u:
                _run(
                    "pacman -Sy --noconfirm nvidia-open nvidia-open-dkms "
                    "lib32-nvidia-utils lib32-opencl-nvidia nvidia-settings "
                    "opencl-nvidia"
                )
            elif "10xx" in u:
                print("Needs AUR (yay). Install yay first? YES | NO :> ", end="", flush=True)
                if "yes" in input().lower():
                    self.install_yay()
                if os.path.isdir(os.path.join(self.DefaultDir, "yay")):
                    _run(
                        "yay -Sy --noconfirm nvidia-580xx-dkms nvidia-580xx-settings "
                        "nvidia-580xx-utils lib32-nvidia-580xx-utils "
                        "lib32-opencl-nvidia-580xx opencl-nvidia-580xx"
                    )
        elif "amd" in g or "ati" in g:
            print("New AMD -> new | Older Radeon -> rd", flush=True)
            u = input(":> ").strip().lower()
            if "new" in u:
                _run(
                    "pacman -Sy --noconfirm xf86-video-amdgpu mesa vulkan-radeon "
                    "lib32-mesa lib32-vulkan-radeon"
                )
            elif "rd" in u:
                _run("pacman -Sy --noconfirm xf86-video-ati mesa")
        elif "intel" in g:
            _run(
                "pacman -Sy --noconfirm xf86-video-intel mesa lib32-mesa "
                "vulkan-intel lib32-vulkan-intel"
            )
        else:
            print(f"{gpu_name!r} not recognized (nvidia / amd / intel).", flush=True)

    def install_broadcom_driver(self) -> None:
        _run("pacman -Sy --noconfirm dkms broadcom-wl-dkms")

    def install_browser(self) -> None:
        while True:
            print("qutebrowser -> qb | chromium -> cm | firefox -> ff | vivaldi -> v | exit")
            print("-" * 30, flush=True)
            u = input(":> ").strip().lower()
            if not u or "exit" in u:
                return
            if "qb" in u or "qute" in u:
                _run("pacman -Sy --noconfirm qutebrowser")
                return
            if "cm" in u or "chromium" in u:
                _run("pacman -Sy --noconfirm chromium")
                return
            if "ff" in u or "firefox" in u:
                _run("pacman -Sy --noconfirm firefox")
                return
            if u == "v" or "vivaldi" in u:
                _run("pacman -Sy --noconfirm vivaldi vivaldi-ffmpeg-codecs")
                return
            print("Unknown choice.", flush=True)

    def install_de(self) -> None:
        while True:
            print("XFCE -> xe | LXDE -> le | GNOME -> ge | Plasma -> pa | Cinnamon -> cn | openbox -> ox | exit")
            print("-" * 30, flush=True)
            u = input(":> ").strip().lower()
            if "exit" in u:
                return
            if "xe" in u:
                _run("pacman -Sy --noconfirm xfce4 xfce4-goodies")
            elif "le" in u:
                _run("pacman -Sy --noconfirm lxde")
            elif "ge" in u:
                _run("pacman -Sy --noconfirm gnome")
            elif "pa" in u:
                _run("pacman -Sy --noconfirm plasma")
            elif "cn" in u:
                _run("pacman -Sy --noconfirm cinnamon")
            elif "ox" in u:
                _run("pacman -Sy --noconfirm openbox")
            else:
                print(f"{u!r} ???", flush=True)

    def install_dm(self) -> None:
        while True:
            print("lightdm -> lm | lxdm -> lxm | gdm -> gm | sddm -> sm | xdm -> xm | exit")
            print("-" * 30, flush=True)
            u = input(":> ").strip().lower()
            if "exit" in u:
                return
            if "lm" in u:
                _run(
                    "pacman -Sy --noconfirm -S lightdm lightdm-gtk-greeter "
                    "lightdm-gtk-greeter-settings"
                )
                _run("systemctl enable lightdm.service")
                return
            if "lxm" in u:
                _run("pacman -Sy --noconfirm lxdm")
                _run("systemctl enable lxdm.service")
                return
            if "gm" in u:
                _run("pacman -Sy --noconfirm gdm")
                _run("systemctl enable gdm.service")
                return
            if "sm" in u:
                _run("pacman -Sy --noconfirm sddm sddm-kcm")
                _run("systemctl enable sddm.service")
                return
            if "xm" in u:
                _run("pacman -Sy --noconfirm xorg-xdm")
                _run("systemctl enable xdm.service")
                return
            print(f"{u!r} ???", flush=True)

    def settings(self) -> None:
        while True:
            print("Default bundle -> default | timezone -> t | check tz name -> ctnr")
            print("hostname -> hn | create user -> cu | password -> cp | exit")
            print("-" * 30, flush=True)
            u = input(":> ").strip().lower()
            if "exit" in u:
                return
            head = u.split()[0] if u.split() else ""
            if head == "default" or u == "default":
                self.set_time_zone(default=True)
                self.host_name(default=True)
                self.create_user_name("", default=True)
                pw = getpass.getpass("Password:> ")
                self.change_user_password(self.UserName, pw)
                continue
            if head == "t":
                tz = input("Timezone (e.g. Europe/Istanbul): ").strip()
                self.set_time_zone(zone_name=tz)
            elif head == "ctnr":
                tz = input("Timezone (e.g. Europe/Istanbul): ").strip()
                self.check_time_zone(tz)
            elif head == "hn":
                self.HostName_PC = input("Hostname: ").strip()
                self.host_name(name=self.HostName_PC)
            elif head == "cu":
                self.UserName = input("Username: ").strip()
                self.create_user_name(self.UserName, default=False)
            elif head == "cp":
                print("Password will not echo.", flush=True)
                pw = getpass.getpass("Password:> ")
                self.change_user_password(self.UserName, pw)
                if input("Show password on screen? YES | NO :> ").strip().lower() == "yes":
                    print(pw)
            else:
                print(f"{u!r} ??", flush=True)

    def install_software(self) -> None:
        while True:
            print("-" * 30, flush=True)
            print("Search repo -> cr | remove -> rm | install -> ins | exit", flush=True)
            u = input(":> ").strip().lower()
            head = u.split()[0] if u.split() else ""
            if head == "exit":
                return
            if head == "cr":
                name = input("Package name:> ").strip()
                _run(f"pacman -Ss {name}")
            elif head == "rm":
                name = input("Package name:> ").strip()
                _run(f"pacman -R --noconfirm {name}")
            elif head == "ins":
                name = input("Package name:> ").strip()
                _run(f"pacman -Sy --noconfirm {name}")
            else:
                print(f"{u!r} -> ?", flush=True)

    def finish_install(self) -> None:
        while True:
            u = input("Finish grub install? YES | NO :> ").strip().lower()
            if "yes" not in u:
                return
            _run(f"grub-install --target=i386-pc {self.disk_dev}")
            _run("grub-mkconfig -o /boot/grub/grub.cfg")
            print("Done. Exit chroot, umount -R /mnt, reboot.", flush=True)
            return


def _menu_cmd(line: str) -> str:
    return line.strip().lower().split()[0] if line.strip() else ""


if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root inside the target chroot (after pacstrap).", file=sys.stderr)
        sys.exit(1)

    gc = Installer()
    while True:
        print("settings -> is | network -> net | pipewire -> ipipe | gpu -> igd")
        print("broadcom wl -> bwd | browser -> ib | de -> ide | dm -> idm | yay -> iyay")
        print("packages -> iprog | finish -> finish | quit -> q", flush=True)
        raw = input(":> ")
        cmd = _menu_cmd(raw)
        if cmd in ("q", "exit", "quit"):
            break
        if cmd == "is":
            gc.settings()
        elif cmd == "net":
            gc.install_network_manager()
        elif cmd == "bwd":
            gc.install_broadcom_driver()
        elif cmd == "ipipe":
            gc.install_pipe_wire()
        elif cmd == "ide":
            gc.install_de()
        elif cmd == "idm":
            gc.install_dm()
        elif cmd == "iyay":
            gc.install_yay()
        elif cmd == "ib":
            gc.install_browser()
        elif cmd == "iprog":
            gc.install_software()
        elif cmd == "igd":
            print("GPU: AMD | NVIDIA | INTEL", flush=True)
            gc.install_gpu(input(":> ").strip())
        elif cmd == "finish":
            gc.finish_install()
        else:
            print(f"Unknown: {raw!r}", flush=True)
