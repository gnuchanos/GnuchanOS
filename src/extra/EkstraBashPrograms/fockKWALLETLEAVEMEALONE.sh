rm -rf ~/.kde/share/apps/kwallet/
rm -rf ~/.kde4/share/apps/kwallet/
rm -rf ~/.kde/share/config/kwalletrc
rm -rf ~/.kde4/share/config/kwalletrc
sudo pacman -Rdd kwallet signon-kwallet-extension kaccounts-integration
killall kwalletd6
