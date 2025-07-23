sudo apt update
sudo apt install grub2 plymouth

sudo cp -r minimal /boot/grub/themes
sudo cp -r gnuchanBoot /usr/share/plymouth/themes/

sudo cp mkinitcpio.conf /etc/initramfs-tools/
sudo cp grub /etc/default

sudo update-initramfs -u
sudo grub-mkconfig -o /boot/grub/grub.cfg

sudo cp plymouthd.conf /etc/plymouth/
sudo plymouth-set-default-theme -R gnuchanBoot