import os



class Installer:
    def __init__(self, Disk: str = "sda") -> None:
        self.Disk: str = Disk
        self.SwapON: bool = True
        self.SwapSize: str = f"8GB"
        self.BootSize: str = f"1GB"

    def DiskSwap(self, Skip: bool = True) -> None:
        if not Skip:
            UInput = input("You Want to SWAP?? | YES | NO |:> ")
            if "yes" in UInput.lower():
                _SWAP = input(" Size: ")
                if len(_SWAP) > 0:
                    if "GB" not in _SWAP.upper():
                        self.SwapSize = f"{_SWAP}GB"
                    else:
                        self.SwapSize = _SWAP.upper()

    def DiskBootSize(self, Skip: bool = True) -> None:
        if not Skip:
            UInput = input("Boot Size 512MB-> 1 or 1GB-> 2 :> ")
            if len(UInput) > 0:
                if "1" in UInput:
                    self.BootSize = f"512MiB"
                elif "2" in UInput:
                    self.BootSize = f"1GB"

    def CreateDisk(self) -> None:
        while True:
            print(f"Default | 8GB Swap {self.Disk}1 | 1GB Boot {self.Disk}{2} | ALL Disk {self.Disk}3")
            print('-'*30)

            UInput = input("manuel-> m : default -> d :> ")
            if 'm' in UInput.lower():
                # SWAP
                self.DiskSwap(Skip=False)
                print(f"Swap Size {self.SwapSize}")
                print('-'*30)

                # BOOT SIZE
                self.DiskBootSize(Skip=False)
                print(f"Bot Size {self.BootSize}")
                print('-'*30)
                self.CreateDisk_Finish = True

            elif 'd' in UInput.lower():
                # SWAP
                self.SwapSize = "8GB"
                print(f"Swap Size {self.SwapSize}")
                print('-'*30)

                # BOOT SIZE
                self.BootSize = "1GB"
                print(f"Bot Size {self.BootSize}")
                print('-'*30)
                self.CreateDisk_Finish = True

            # Create Disk Space
            os.system(f"parted {self.Disk} --script mklabel msdos")
            os.system(f"parted {self.Disk} --script mkpart primary linux-swap 0GB {self.SwapSize}")
            os.system(f"parted {self.Disk} --script mkpart primary ext4 {self.SwapSize} {self.BootSize}")
            os.system(f"parted {self.Disk} --script set 2 boot on")
            os.system(f"parted {self.Disk} --script mkpart primary ext4 {self.BootSize} 100%")

            os.system("parted {self.Disk} print")

            UInput = input("everythings is fine??: | YES | NO |:> ")
            if "yes" in UInput.lower():
                os.system(f"mkswap /dev/{self.Disk}/1")
                os.system(f"swapon /dev/{self.Disk}/1")
                os.system(f"mkfs.ext4 /dev/{self.Disk}/2")
                os.system(f"mkfs.ext4 /dev/{self.Disk}/3")
                os.system(f"mount /dev/{self.Disk}/3 /mnt")
                os.system(f"mkdir /mnt/boot")
                os.system(f"mount /dev/{self.Disk}/2 /mnt/boot")

    def StartPacstrap(self, Kernal: str) -> None:
        while True:
            print("Linux -> lx")
            print("Linux-lts -> lts")

            _Extra = "base base-devel grub nano"

            UInput = input(":> ")
            if "lx" in UInput.lower():
                os.system(f"{_Extra} linux linux-firmware linux-headers")
            elif "lts" in UInput.lower():
                os.system(f"{_Extra} linux-lts linux-firmware linux-lts-headers make cmake git wget")

            os.system("genfstab -U /mnt >> /mnt/etc/fstab")
            os.system("cp installerPart2.py")
            os.system("arch-chroot /mnt python /root/installerPart2.py")

if __name__ == "__main__":
    os.system("lsblk")
    DISK = input("Disk Name: ")
    gc = Installer("sda")
    while True:
        gc.CreateDisk()
        gc.StartPacstrap()



