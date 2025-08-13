sudo apt install -y git build-essential ncurses-dev lua5.1 liblua5.1-dev ruby-dev python3-dev libperl-dev

cp .vimrc ~/
mkdir ~/tmp
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

sudo npm install -g typescript typescript-language-server

python3 install.py --ts-completer

# for xterm
echo "stty erase ^H" >> ~/.bashrc
echo "fish" >> ~/.bashrc


