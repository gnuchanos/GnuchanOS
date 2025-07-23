#!/bin/bash

sudo pacman -S dotnet-sdk 
sudo pacman -S mono
sudo pacman -S mono-tools
sudo pacman -S mono-msbuild
sudo pacman -S mono-msbuild-sdkresolver
sudo pacman -S mono-addins

yay -Sy  unityhub
