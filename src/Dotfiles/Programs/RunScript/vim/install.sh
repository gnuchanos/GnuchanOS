#!/bin/bash

cp .vimrc ~/
curl -fLo ~/.vim/autoload/plug.vim --create-dirs \
    https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
vim +PlugInstall +qall
cp .ycm_extra_conf.py ~/.vim/plugged/YouCompleteMe
cd ~/.vim/plugged/YouCompleteMe
pwd
#python3 install.py --clang-completer --system-libclang
python3 install.py --clangd-completer

pwd

# for xterm
echo "stty erase ^H" >> ~/.bashrc
echo "fish" >> ~/.bashrc




