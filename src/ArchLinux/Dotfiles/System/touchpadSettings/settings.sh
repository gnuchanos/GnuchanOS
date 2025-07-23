#!/bin/bash

sudo pacman -Sy --noconfirm xf86-input-libinput

sudo cp 30-touchpad.conf /etc/X11/xorg.conf.d/
