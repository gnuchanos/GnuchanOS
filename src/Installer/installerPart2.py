import os
import getpass import getpass


class Installer:
    def __init__(self) -> None:
        self.HostName_PC = "ThisIsyMyKingdomCOME"
        self.UserName = "archkubi"
        self.SystemLang = "en_US"
        self.KeyMap = "us"
        self.Gpu = "nvidia"
        self.GpuDate = "old" # "new"
        self.DefaultDir = os.getcwd()

    def CheckTimeZone(self, TimeZone: str = None) -> None:
        if len(TimeZone) > 0:
            os.system(f"timedatectl | grep \"{TimeZone}\"")

    def SetTimeZone(self, ZoneName: str = None, Default: bool = False) -> None:
        while True:
            if not Default:
                self.CheckTimeZone(TimeZone=ZoneName)

                print("Is Settings done??", flush=True)
                UInput = input("YES, NO :> ")
                if "yes" in UInput.lower():
                    _TempCommand = f"ln -sf /usr/share/zoneinfo/{ZoneName}"
                    os.system(_TempCommand)
                    os.system("hwclock --systohc")
                    break
                else:
                    print("OK", flush=True)
                    break
            else:
                os.system("ln -sf /usr/share/zoneinfo/Europe/Istanbul")

    def HostName(self, Name: str = None, Default: bool = False) -> None:
        if not Default:
            if Name != None:
                os.system(f"echo \"{Name}\" > /etc/hostname")
        else:
            os.system("echo ThisIsMyKingdomCOME > /etc/hostname")

    def CreateUserName(self, UserName: str, Default: bool = False) -> None:
        if not Default:
            if len(UserName) > 0:
                _TempCommand1 = f"useradd -m -G wheel -s /bin/bash {UserName}"
                os.system(_TempCommand1)
                _TempCommand2 = f"usermod -a -G sudo {UserName}"
        else:
            os.system("useradd -m -G wheel -s /bin/bash archkubi")

    def ChangeUserPassword(self, UserName: str, Password: str) -> None:
        if len(Password) > 0 and len(UserName) > 0:
            _List = ("root", UserName)

            for i in _List:
                _TenoPassword1 = f"{i}:{Password}"
                _TempCommand1 = f"echo {_TenoPassword1} | chpasswd"
                os.system(_TempCommand1)

    def InstallNetworkManager(self) -> None:
        os.system("pacman -Sy --noconfirm networkmanager")
        os.system("systemctl enable NetworkManager")

    def InstallPipeWire(self) -> None:
        os.system("pacman -Sy --noconfirm pipewire pipewire-alsa pipewire-jack wireplumber alsa-utils")
        os.system("systemctl enable --now pipewire pipewire-jack pipewire-alsa wireplumber")

    def InstallYay(self) -> None:
        tmpDir = os.getcwd()
        tmpYayDir = os.path.join(tmpDir, "yay")

        if not os.path.exists(tmpDir):
            os.system("git clone https://aur.archlinux.org/yay.git")
            os.chdir(tmpYayDir)
            os.system("makepkg -si")
            os.chdir(tmpDir)

    def InstallGPU(self, GpuName: str) -> None:
        if len(GpuName) > 0:
            if "nvidia" in GpuName.lower():
                print("New Open Source Driver (not linux-lts) -> osd")
                print("GTX 1050 TI-> 10xx")

                UInput = input(":> ")
                if "osd" in UInput.lower():
                    os.system("pacman -S nvidia-open nvidia-open-dkms opencl-nvidi lib32-nvidia-utils lib32-opencl-nvidia nvidia-settings")
                elif "10xx" in UInput.lower():
                    print("youn need yay for this do you want to install | YES | NO |")
                    UInput = input(":> ")
                    if "yes" in UInput.lower():
                        self.InstallYay()

                    tmpDirYAY = os.path.join(self.DefaultDir, "yay")
                    if os.path.exists(tmpDirYAY):
                        os.system("yay -Sy nvidia-580xx-dkms nvidia-580xx-settings nvidia-580xx-utils lib32-nvidia-580xx-utils lib32-opencl-nvidia-580xx opencl-nvidia-580xx")

            elif "amd" in GpuName.lower():
                print("New AMD-> new")
                print("Old Radeon-> rd")

                UInput = input(":> ")
                if "new" in UInput.lower():
                    os.system("pacman -Sy --nocomfirm xf86-video-amdgpu mesa vulkan-radeon lib32-mesa lib32-vulkan-radeon")
                elif "rd" in UInput.lower():
                    os.system("pacman -Sy --nocomfirm xf86-video-ati mesa")

            elif "intel" in Gpu.lower():
                os.system("pacman -Sy --nocomfirm xf86-video-intel mesa lib32-mesa lib32-vulkan-intel")

            else:
                print(f"{GpuName} this is not GPU")

    def InstallBrodcomDriver(self) -> None:
        os.system("pacman -Sy --nocomfirm dkms broadcom-wl-dkms")

    def InstallBrowser(self) -> None:
       while True:
            print("Install Qutebrowser -> qb")
            print("Install Chromium -> cm")
            print("Install Firefox  -> ff")
            print("Install Vivaldi  -> v")
            print("exit")
            print('-'*30)

            UInput = input()
            if len(UInput) > 0:
                if "qutebrowser" in UInput.lower():
                    os.system("pacman -Sy --noconfirm qutebrowser")
                    break
                elif "chromium" in UInput.lower():
                    os.system("pacman -Sy --noconfirm chromium")
                    break
                elif "firefox" in UInput.lower():
                    os.system("pacman -Sy --noconfirm firefox")
                    break
                elif "vivaldi" in UInput.lower():
                    os.system("pacman -Sy --noconfirm vivaldi vivaldi-ffmpeg-codecs")
                    break
                elif "exit" in UInput.lower():
                    break

    def InstallDE(self) -> None:
        while True:
            print("Install XFCE -> XE")
            print("Install LXDE -> LE")
            print("Install GNOME -> GE")
            print("Install PLASMA -> PA")
            print("INSTALL CINNAMON -> CN")
            print("INSTALL OPENBOX -> OX")
            print("exit")
            print('-'*30)

            UInput = input(":> ")
            if "xe" in UInput.lower():
                os.system("pacman -Sy --nocomfirm xfce4 xfce4-goodies")
            elif "le" in UInput.lower():
                os.system("pacman -Sy --noconfirm lxde")
            elif "ge" in UInput.lower():
                os.system("pacman -Sy --nocomfirm gnome")
            elif "pa" in UInput.lower():
                os.system("pacman -Sy --noconfirm plasma")
            elif "cn" in UInput.lower():
                os.system("pacman -Sy --noconfirm cinnamon")
            elif "ox" in UInput.lower():
                os.system("pacman -Sy --noconfirm openbox")
            elif "exit" in UInput.lower():
                break
            else:
                print(f"{UInput} ???")

    def InstallDM(self) -> None:
        while True:
            print("Install LIGHTDM -> LM")
            print("Install LXDM -> LXM")
            print("Install GDM -> GM")
            print("Install SDDM -> SM")
            print("Install XDM -> XM")
            print("exit")
            print('-'*30)

            UInput = input(":> ")
            if "lm" in UInput.lower():
                os.system("sudo pacman -Sy --noconfrim lightdm lightdm-gtk-greeter lightdm-gtk-greeter-settings")
                os.system("systemctl enable lightdm.service")
                break
            elif "lxdm" in UInput.lower():
                os.system("sudo pacman -Sy --noconfirm lxdm")
                os.system("systemctl enable lxdm.service")
                break
            elif "gm" in UInput.lower():
                os.system("sudo pacman -Sy --noconfirm gdm")
                os.system("systemcl enable gdm.service")
                break
            elif "SM" in UInput.lower():
                os.system("sudo pacman -Sy --noconfirm sddm sddm-kcm")
                os.system("systemctl enable sddm.service")
                break
            elif "xm" in UInput.lower():
                os.system("sudo pacman -Sy --noconfirm xdm-archlinux")
                os.system("systemctl enable xdm.service")
                break
            elif "exit" in UInput.lower():
                break
            else:
                print(f"{UInput} ???")

    def Settings(self) -> None:
        while True:
            UInput = ''
            print("Default Settings For MYSELF")
            print("Timezone -> t")
            print("Check Timezone Name Right -> ctnr")
            print("HostName/Pc Name -> hn")
            print("Create User (use only english alphabet) -> cu")
            print("Change Password -> cp")
            print("exit")
            print('-'*30)

            UInput = input(":> ")
            if "default" in UInput:
                self.SetTimeZone(Default=True)
                self.HostName(Default=True)
                self.CreateUserName(Default=True)

                _Password = getpass("Password:> ")
                self.ChangeUserPassword(UserName=self.UserName, Password=_Password)

            else:
                if 't' in  UInput.lower():
                    _TimeZone = input("Timezone Example Europe/Istanbul -> ")
                    self.SetTimeZone(ZoneName=_TimeZone)
                elif "ctnr" in UInput.lower():
                    _TimeZone = input("Timezone Example Europe/Istanbul -> ")
                    self.CheckTimeZone(_TimeZone)
                elif "hn" in UInput.lower():
                    self.HostName_PC = input("HostName/PC NAME: ")
                    self.HostName(Name=self.HostName_PC)
                elif "cu" in UInput.lower():
                    self.UserName = input("User Name: ")
                    self.CreateUserName(UserName=self.UserName)
                elif "cp" in UInput.lower():
                    print("warning password not showing on screen!!", flush=True)
                    _Password = getpass("Password:> ")
                    self.ChangeUserPassword(UserName=_UserName, Password=_Password)

                UInput = input("do you check password: | YES | NO |")
                if "yes" in UInput.lower():
                    print(_Password)

            elif "exit" in UInput.lower():
                break

            else:
                print(f"{UInput} ??")

    def InstallSoftware(self) -> None:
        while True:
            print('-'*30, flush=True)
            print("Check Programs In Repo -> cr", flush=True)
            print("Remove Programs -> rm", flush=True)
            print("install Programs -> ins", flush=True)
            print("exit", flush=True)

            UInput = input("")
            if "cr" in UInput.lower():
                _ProgramName = input("Program Name:> ")
                os.system(f"pacman -Ss {_ProgramName}")
            elif "rm" in UInput.lower():
                _ProgramName = input("Program Name:> ")
                os.system(f"pacman -R {_ProgramName}")
            elif "ins" in UInput.lower():
                _ProgramName = input("Program Name:> ")
                os.program(f"pacman -Sy {_ProgramName}")
            elif "exit" in UInput.lower():
                break
            else:
                print(f"{UInput} -> ?")

    def FinishInstall(self):
        while True:
            print("is it done? :> ")
            UInput = input("YES | NO :> ")

            if "yes" in UInput.lower():
                os.system(f"grub-install --target=i386-pc /dev/{self.Disk}")
                os.system("grub-mkconfig -o /boot/grub/grub.cfg")
                print("install is done", flush=True)
                print("first do exit")
                print("second do mount -R /mnt")
                print("and last reboot good luck")
                break

if __name__ == "__main__":
    gc = Installer("sda")
    while True:
        UInput = ""
        print("Important Settings -> is", flush=True)
        print("Install Network Manager -> net", flush=True)
        print("Install Pipewire -> ipipe", flush=True)
        print("Install GPU driver -> igd", flush=True)
        print("Install Brodcom Wifi Driver -> bwd", flush=True)
        print("Install Browser -> ib", flush=True)
        print("Install DE -> ide", flush=True)
        print("Install DM -> idm", flush=True)
        print("Install YAY -> iyay", flush=True)
        print("Install Programs -> ip", flush=True)
        print("Finish Installer -> finish", flush=True)

        UInput = input(":> ")
        if "is" in UInput.lower():
            gc.Settings()
        elif "net" in UInput.lower():
            gc.InstallNetworkManager()
        elif "bwd" in UInput.lower():
            gc.InstallBrodcomDriver()
        elif "pipe" in UInput.lower():
            gc.InstallPipeWire()
        elif "de" in UInput.lower():
            gc.InstallDE()
        elif "dm" in UInput.lower():
            gc.InstallDM()
        elif "ign" in UInput.lower():
            gc.InstallGPU()
        elif "iyay" in UInput.lower():
            gc.InstallYay()
        elif "ib" in UInput.lower():
            gc.InstallBrowser()
        elif "ip" in UInput.lower():
            gc.InstallSoftware()
        elif "igd" in UInput.lower():
            print("Ekran kartin Ne: | AMD (ati) | NVIDIA | INTEL |", flush=True)
            UInput = input(":> ")
            gc.InstallGPU(GpuName=UInput)
        elif "ip" in UInput.lower():
            gc.InstallSoftware()
        elif "finish" in UInput.lower():
            gc.FinishInstall()







