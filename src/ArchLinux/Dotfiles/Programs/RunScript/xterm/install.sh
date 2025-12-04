#!/bin/bash

cp .Xresources ~/
cp .bashrc ~/
cp .zshrc ~/

pwd
cp -r fish ~/.config
sudo pacman -S bash-completion
sudo pacman -S zsh-autosuggestions zsh-syntax-highlighting


source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh
source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh


chsh -s /bin/zsh

