#!/bin/bash

yay -S update-grub plymouth

sudo cp -r minimal/ /boot/grub/themes
sudo update-grub

sudo cp -r gnuchanBoot  /usr/share/plymouth/themes/
sudo cp mkinitcpio.conf /etc/
sudo cp grub /etc/default/
sudo mkinitcpio -P linux-cachyos; sudo grub-mkconfig -o /boot/grub/grub.cfg
sudo cp plymouthd.conf /etc/plymouth/
sudo plymouth-set-default-theme -R gnuchanBoot
