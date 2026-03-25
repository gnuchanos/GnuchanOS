#!/bin/bash

cp .Xresources ~/
cp .bashrc ~/
cp .zshrc ~/

sudo pacman -S zsh-autosuggestions zsh-syntax-highlighting

# -----------------------------------------------------------------------------------

cp .vimrc ~/
mkdir ~/tmp
cd ~/tmp
pwd
git clone https://github.com/vim/vim.git
cd ~/tmp/vim
pwd
./configure --prefix=/usr/local --enable-python3interp --enable-rubyinterp --enable-luainterp --enable-perlinterp --with-features=huge
make
sudo make install

curl -fLo ~/.vim/autoload/plug.vim --create-dirs \
    https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim

vim +PlugInstall +qall

# Eğer .ycm_extra_conf.py'ye ihtiyacın varsa kopyala, değilse atla
# cp .ycm_extra_conf.py ~/.vim/plugged/YouCompleteMe

cd ~/.vim/plugged/YouCompleteMe
pwd
python3 install.py --clangd-completer

