#!/bin/bash


sudo pacman -S gnome-boxes
sudo systemctl enable --now libvirtd
LC_ALL=C lscpu | grep Virtualization
sudo usermod -aG libvirt $(whoami)
newgrp libvirt
sudo systemctl start libvirtd
sudo systemctl enable libvirtd

sudo pacman -S xf86-video-amdgpu






