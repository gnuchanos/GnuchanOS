#!/bin/bash
set -e

sudo pacman -Syu --noconfirm
sudo pacman -S --noconfirm gnome-boxes qemu libvirt virt-manager dnsmasq vde2 bridge-utils openbsd-netcat

sudo systemctl enable --now libvirtd
sudo usermod -aG libvirt $USER
