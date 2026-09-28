
import os

# this is simple script for reboot ath5k --> pressing fn+f11 is not working if you not do this commands "vostro A860" =_= 

"""
sudo rfkill unblock all
sudo modprobe -r ath5k
sudo modprobe ath5k
"""

if __name__ == "__main__":
    os.system("sudo rfkill unblock all")
    os.system("sudo modprobe -r ath5k")
    os.system("sudo modprobe ath5k")