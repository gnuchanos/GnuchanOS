import subprocess

subprocess.run(["sudo", "rfkill", "unblock", "all"])
subprocess.run(["sudo", "modprobe", "-r", "ath5k"])
subprocess.run(["sudo", "modprobe", "ath5k"])