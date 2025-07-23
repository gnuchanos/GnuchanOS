
sudo rm -r /usr/share/themes/
sudo mkdir /usr/share/themes

tar -vxf GnuChanOSTheme.tar
sudo mv GnuChanOSTheme  /usr/share/themes/

tar -vxf cursor.tar
tar -vxf icon.tar
sudo rm -r  /usr/share/icons/
sudo mkdir /usr/share/icons/
sudo mv GnuChanOS-icons/ /usr/share/icons/
sudo mv GnuchanCursors/ /usr/share/icons/

echo "🗑️ Önbellek temizleniyor..."
rm -rf ~/.cache/gtk-3.0
rm -rf ~/.icons
rm -rf ~/.local/share/icons


sudo cp settings.ini /etc/gtk-3.0