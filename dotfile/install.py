import os




important_things = [
    "rfkill",

]

if __name__ == "__main__":
    for thing in important_things:
        if not os.path.exists(f"/usr/sbin/{thing}"):
            for i in important_things:
                os.system(f"sudo apt install {i} -y")