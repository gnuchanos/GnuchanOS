import os
import getpass import getpass


class Installer:
    def __init__(self) -> None:
        self.HostName = "ThisIsyMyKingdomCOME"
        self.UserName = "archkubi"
        self.SystemLang = "en_US"
        self.KeyMap = "us"
        self.Gpu = "nvidia"
        self.GpuDate = "old" # "new"

    def CheckTimeZone(self, TimeZone: str = None) -> None:
        if len(TimeZone) > 0:
            os.system(f"timedatectl | grep \"{TimeZone}\"")

    def SetTimeZone(self, ZoneName: str = None) -> None:
        if len(ZoneName) > 0:
            _TempCommand = f"ln -sf /usr/share/zoneinfo/{ZoneName}"
            os.system(_TempCommand)
            os.system("hwclock --systohc")

    def HostName(self, Name: str = None) -> None:
        if Name != None:
            os.system(f"echo \"{Name}\" > /etc/hostname")

    def CreateUserName(self, UserName: str) -> None:
        if len(UserName) > 0:
            _TempCommand1 = f"useradd -m -G wheel -s /bin/bash {UserName}"
            os.system(_TempCommand1)
            _TempCommand2 = f"usermod -a -G sudo {UserName}"

    def ChangeUserPassword(self, UserName: str, Password: str) -> None:
        if len(Password) > 0 and len(UserName) > 0:
            _List = ("root", UserName)

            for i in _List:
                _TenoPassword1 = f"{i}:{Password}"
                _TempCommand1 = f"echo {_TenoPassword1} | chpasswd"
                os.system(_TempCommand1)

    def InstallNetworkManager(self):
        os.system("pacman -Sy networkmanager")
        os.system("systemctl enable NetworkManager")

    def InstallGPU(self, GpuName: str, GpuDate) -> None:
        if len(GpuName) > 0 and len(GpuDate) > 0:
            if "nvidia" in GpuName.lower():
                print("New Open Source Driver (not linux-lts) -> osd")
                print("New Close Source Driver-> csd")
                print("GTX 1050 TI-> 10xx")

                UInput = input(":> ")
                if "osd" in UInput.lower():
                    os.system("pacman -S   nvidia-open nvidia-open-dkms opencl-nvidi lib32-nvidia-utils lib32-opencl-nvidia nvidia-settings")
                elif "10xx" in UInput.lower():
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

    def InstallWifiDriver(self, DriverName: str) -> None:
        print("Install Brodcom Wifi Driver -> bc")
        if len(DriverName) > 0:
            if "brodcom" in DriverName.lower():
                os.system("pacman -Sy --nocomfirm dkms broadcom-wl-dkms")

    def InstallBrowser(self, BraName: str) -> None:
        if len(BraName) > 0:
            if "qutebrowser" in BraName.lower():
                os.system("pacman -Sy --noconfirm qutebrowser")
            elif "chromium" in BraName.lower():
                os.system("pacman -Sy --noconfirm chromium")
            elif "firefox" in BraName.lower():
                os.system("pacman -Sy --noconfirm firefox")
            elif "vivaldi" in BraName.lower():
                os.system("pacman -Sy --noconfirm vivaldi vivaldi-ffmpeg-codecs")

    def InstallSoftware(self, Software: str) -> None:
        pass

    def Settings(self) -> None:
        while True:
            UInput = ''
            print("Timezone -> t")
            print("HostName/Pc Name -> hn")
            print("Create User (use only english alphabet) -> cu")
            print("Change Password -> cp")
            print('-'*30)

            UInput = input(":> ")
            if 't' in  UInput.lower():
                _TimeZone = input("Timezone Example Europe/Istanbul -> ")
                self.SetTimeZone(ZoneName=_TimeZone)
            elif "hn" in UInput.lower():
                _HostName = input("HostName/PC NAME: ")
                self.HostName(Name=_HostName)
            elif "cu" in UInput.lower():
                _UserName = input("User Name: ")
                self.CreateUserName(UserName=_UserName)
            elif "cp" in UInput.lower():
                print("warning password not showing on screen!!")
                _Password = getpass("Password")
                self.ChangeUserPassword(UserName=_UserName, Password=_Password)

                UInput = input("do you check password: | YES | NO |")
                if "yes" in UInput.lower():
                    print(_Password)



if __name__ == "__main__":
    gc = Installer("sda")
    while True:
        print("Install Network Manager >>: N")
        print("Install Pipewire >>: P")
        UInput = input(":> ")






