import os




important_things = [
    "rfkill",
    "ntpsec"

]

if __name__ == "__main__":
    for thing in important_things:
        if not os.path.exists(f"/usr/sbin/{thing}"):
            for i in important_things:
                os.system(f"sudo apt install {i} -y")

                if i == "ntpsec":
                    os.system("sudo systemctl enable ntpsec")
                    os.system("sudo systemctl start ntpsec")
                    
                    _input = input("Do you want to set the timezone to Europe/Istanbul? (y/n): ")
                    if _input == "y":
                        os.system("sudo timedatectl set-timezone Europe/Istanbul")
                    os.system("sudo timedatectl set-ntp true")














