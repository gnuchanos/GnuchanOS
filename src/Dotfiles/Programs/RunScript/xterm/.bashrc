[ -z "$PS1" ] && return

stty erase ^H
neofetch
fish
stty erase ^H