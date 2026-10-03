import os
from re import purge

# this is just simple script i write it that way i don't care

important_things = [
    "rfkill",
    "ntpsec",
    "deluge-gtk",
    "deluge",
    "python3-libtorrent",
    "python3-setuptools",
    "deluged",
    "openssh-server"
]

commands = [
    "sudo apt purge xfce4 xfce4-* firefox firefox-esr -y",
    "sudo apt autoremove --purge -y",
    "sudo apt clean",
    "sudo systemctl enable --now ssh",
    "sudo apt purge xfce4 xfce4-* xfconf libxfce4* thunar -y",
    "sudo apt autoremove --purge -y",
    "sudo apt purge lightdm -y",
    "sudo apt autoremove --purge -y",
    "sudo apt purge lightdm-gtk-greeter -y",
    "sudo apt purgefirefox-esr -y",
    "sudo apt clean"
]

if __name__ == "__main__":
    for thing in important_things:
        if not os.path.exists(f"/usr/sbin/{thing}"):
            os.system("sudo apt update && sudo apt upgrade -y")
            for i in important_things:
                os.system(f"sudo apt install {i} -y")

                if i == "ntpsec":
                    os.system("sudo systemctl enable ntpsec")
                    os.system("sudo systemctl start ntpsec")
                    
                    _input = input("Do you want to set the timezone to Europe/Istanbul? (y/n): ")
                    if _input == "y":
                        os.system("sudo timedatectl set-timezone Europe/Istanbul")
                    os.system("sudo timedatectl set-ntp true")

    for command in commands:
        os.system(command)






