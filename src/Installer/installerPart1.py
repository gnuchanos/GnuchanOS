import os



class Installer:
    def __init__(self, Disk: str = "sda") -> None:
        self.Disk: str = Disk
        self.SwapON: bool = True
        self.SwapSize: str = f"8GB"
        self.BootSize: str = f"1GB"

    def DiskSwap(self, Skip: bool = True) -> None:
        if not Skip:
            try:
                while True:
                    _SWAP = int(input(" Size: only GB and only number please:> "))

                    if len(_SWAP) > 0:
                        elif _SWAP > 8:
                            print(f"you don't  need that much swap?? {UInput}", flush=True)

                            _UInput = input("still do you want??? | YES | NO | :> ")
                            if "yes" in _UInput.lower():
                                self.SwapSize = f"{_SWAP}GB"
                                break

            except Exception as ERR:
                print(f"ONLY NUMBERS PELASE: {_SWAP} ????", flush=True)


    def DiskBootSize(self, Skip: bool = True) -> None:
        if not Skip:
            try:
                UInput = input("Boot Size 512MB-> 1 or 1GB-> 2 :> ")
                if len(UInput) > 0:
                    if  "1" in UInput:
                        self.BootSize = f"512MiB"
                    elif "2" in UInput:
                        self.BootSize = f"1GB"

            except Exception as ERR:
                print(f"{ERR}", flush=True)

    def CreateDisk(self) -> None:
        while True:
            try:
                print(f"Default | 8GB Swap {self.Disk}1 | 1GB Boot {self.Disk}{2} | ALL Disk {self.Disk}3", flush=True)
                print('-'*30, flush=True)

                UInput = input("manuel-> m : default -> d :> ")
                if 'm' in UInput.lower():
                    # SWAP
                    self.DiskSwap(Skip=False)
                    print(f"Swap Size {self.SwapSize}", flush=True)
                    print('-'*30, flush=True)

                    # BOOT SIZE
                    self.DiskBootSize(Skip=False)
                    print(f"Bot Size {self.BootSize}", flush=True)
                    print('-'*30, flush=True)
                    self.CreateDisk_Finish = True

                elif 'd' in UInput.lower():
                    # SWAP
                    self.SwapSize = "8GB"
                    print(f"Swap Size {self.SwapSize}", flush=True)
                    print('-'*30, flush=True)

                    # BOOT SIZE
                    self.BootSize = "1GB"
                    print(f"Bot Size {self.BootSize}", flush=True)
                    print('-'*30, flush=True)
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

            except Exception as ERR:
                print(f"{ERR}", flush=True)


    def StartPacstrap(self, Kernal: str) -> None:
        try:
            print("Linux -> lx", flush=True)
            print("Linux-lts -> lts", flush=True)

            _Extra = "base base-devel grub nano make cmake git wget"

            UInput = input(":> ")
            if "lx" in UInput.lower():
                os.system(f"{_Extra} linux linux-firmware linux-headers")
            elif "lts" in UInput.lower():
                os.system(f"{_Extra} linux-lts linux-firmware linux-lts-headers")

            os.system("genfstab -U /mnt >> /mnt/etc/fstab")
            os.system("cp installerPart2.py /mnt")
            os.system("arch-chroot /mnt python /root/installerPart2.py")

        except Exception as ERR:
            print(f"{ERR}", flush=True)

if __name__ == "__main__":
    os.system("lsblk")
    DISK = input("Disk Name: ")
    gc = Installer("sda")

    gc.CreateDisk()
    gc.StartPacstrap()

