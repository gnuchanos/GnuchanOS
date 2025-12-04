#!/bin/bash

cp .Xresources ~/
cp .bashrc ~/
cp .zshrc ~/

sudo pacman -S bash-completion
sudo pacman -S zsh-autosuggestions zsh-syntax-highlighting

source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh
source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh

chsh -s /bin/zsh

# -----------------------------------------------------------------------------------

cp .vimrc ~/
mkdir ~/tmp
cd ~/tmp
git clone https://github.com/vim/vim.git
cd vim
./configure --prefix=/usr/local --enable-python3interp --enable-rubyinterp --enable-luainterp --enable-perlinterp --with-features=huge
make
sudo make install

curl -fLo ~/.vim/autoload/plug.vim --create-dirs \
    https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim

vim +PlugInstall +qall

# Eğer .ycm_extra_conf.py'ye ihtiyacın varsa kopyala, değilse atla
# cp .ycm_extra_conf.py ~/.vim/plugged/YouCompleteMe

cd ~/.vim/plugged/YouCompleteMe

python3 install.py --clangd-completer

