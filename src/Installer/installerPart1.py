from __future__ import annotations

import os
import re
import shutil


def _parse_size_mib(size_str: str) -> int:
    """Parse '8GB', '1GB', '512MiB' into mebibytes for parted."""
    s = size_str.strip().upper().replace(" ", "")
    m = re.match(r"^(\d+(?:\.\d+)?)(GIB|GB|MIB|MB)$", s)
    if not m:
        raise ValueError(f"Unrecognized size: {size_str!r}")
    val = float(m.group(1))
    unit = m.group(2)
    if unit in ("GB", "GIB"):
        return int(val * 1024)
    return int(val)


class Installer:
    def __init__(self, disk: str = "sda") -> None:
        d = disk.strip()
        self.Disk = d[5:] if d.startswith("/dev/") else d
        if not self.Disk:
            self.Disk = "sda"
        self.SwapON: bool = True
        self.SwapSize: str = "8GB"
        self.BootSize: str = "1GB"

    @property
    def disk_dev(self) -> str:
        return f"/dev/{self.Disk}"

    def _swap_boot_end_mib(self) -> tuple[int, int]:
        swap_mib = _parse_size_mib(self.SwapSize)
        boot_mib = _parse_size_mib(self.BootSize)
        return swap_mib, swap_mib + boot_mib

    def disk_swap(self, skip: bool = True) -> None:
        if skip:
            return
        while True:
            try:
                n = int(input("Swap size in GB (whole number):> ").strip())
                if n <= 0:
                    print("Must be positive.", flush=True)
                    continue
                if n > 8:
                    print(f"{n} GB swap is large. Still use it? YES | NO :> ", end="", flush=True)
                    if "yes" not in input().lower():
                        continue
                self.SwapSize = f"{n}GB"
                return
            except ValueError:
                print("Enter digits only (GB).", flush=True)

    def disk_boot_size(self, skip: bool = True) -> None:
        if skip:
            return
        u = input("Boot: 512MiB -> 1 | 1GiB -> 2 :> ").strip()
        if "1" in u:
            self.BootSize = "512MiB"
        elif "2" in u:
            self.BootSize = "1GB"

    def create_disk(self) -> None:
        while True:
            try:
                swap_mib, boot_end_mib = self._swap_boot_end_mib()
                print(
                    f"Default layout: swap {self.disk_dev}1 ({self.SwapSize}) | "
                    f"boot {self.disk_dev}2 | root {self.disk_dev}3",
                    flush=True,
                )
                print("-" * 30, flush=True)
                u = input("manual -> m | default -> d | quit -> q :> ").strip().lower()
                if u == "q":
                    return
                if u == "m":
                    self.disk_swap(skip=False)
                    print(f"Swap size {self.SwapSize}", flush=True)
                    print("-" * 30, flush=True)
                    self.disk_boot_size(skip=False)
                    print(f"Boot size {self.BootSize}", flush=True)
                    print("-" * 30, flush=True)
                elif u == "d":
                    self.SwapSize = "8GB"
                    self.BootSize = "1GB"
                    print(f"Swap size {self.SwapSize}", flush=True)
                    print("-" * 30, flush=True)
                    print(f"Boot size {self.BootSize}", flush=True)
                    print("-" * 30, flush=True)
                else:
                    print("Enter m, d, or q.", flush=True)
                    continue

                swap_mib, boot_end_mib = self._swap_boot_end_mib()
                os.system(f"parted {self.disk_dev} --script mklabel msdos")
                os.system(
                    f"parted {self.disk_dev} --script mkpart primary linux-swap 0MiB {swap_mib}MiB"
                )
                os.system(
                    f"parted {self.disk_dev} --script mkpart primary ext4 "
                    f"{swap_mib}MiB {boot_end_mib}MiB"
                )
                os.system(f"parted {self.disk_dev} --script set 2 boot on")
                os.system(
                    f"parted {self.disk_dev} --script mkpart primary ext4 "
                    f"{boot_end_mib}MiB 100%"
                )
                os.system(f"parted {self.disk_dev} print")

                u = input("Partition layout OK? YES | NO :> ").strip().lower()
                if "yes" not in u:
                    continue

                os.system(f"mkswap {self.disk_dev}1")
                os.system(f"swapon {self.disk_dev}1")
                os.system(f"mkfs.ext4 -F {self.disk_dev}2")
                os.system(f"mkfs.ext4 -F {self.disk_dev}3")
                os.system(f"mount {self.disk_dev}3 /mnt")
                os.system("mkdir -p /mnt/boot")
                os.system(f"mount {self.disk_dev}2 /mnt/boot")
                return

            except Exception as err:
                print(f"{err}", flush=True)

    def start_pacstrap(self) -> None:
            base = (
                "base base-devel linux-firmware grub nano make cmake git wget "
                "python"
            )
        try:
            print("Kernel: linux -> lx | linux-lts -> lts", flush=True)
            choice = input(":> ").strip().lower()
            if "lts" in choice:
                packages = f"{base} linux-lts linux-lts-headers"
            else:
                packages = f"{base} linux linux-headers"

            os.system(f"pacstrap -K /mnt {packages}")
            os.system("mkdir -p /mnt/root")
            with open("/mnt/root/.installer_disk", "w", encoding="utf-8") as f:
                f.write(self.Disk)
            os.system("genfstab -U /mnt >> /mnt/etc/fstab")

            here = os.path.dirname(os.path.abspath(__file__))
            part2 = os.path.join(here, "installerPart2.py")
            if os.path.isfile(part2):
                shutil.copy2(part2, "/mnt/root/installerPart2.py")
            else:
                print("Warning: installerPart2.py not found next to this script.", flush=True)

            os.system("arch-chroot /mnt python3 /root/installerPart2.py")

        except Exception as err:
            print(f"{err}", flush=True)


if __name__ == "__main__":
    os.system("lsblk")
    raw = input("Disk name (e.g. sda or /dev/sda): ").strip()
    if raw.startswith("/dev/"):
        raw = raw[5:]
    disk_name = raw or "sda"
    inst = Installer(disk_name)
    inst.create_disk()
    inst.start_pacstrap()
