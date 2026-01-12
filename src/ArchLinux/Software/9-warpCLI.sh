#!/bin/bash


# yay ile
yay -S cloudflare-warp

# veya paru ile
paru -S cloudflare-warp

# başlat
warp-cli registration new
warp-cli connect

# durum / kapat
warp-cli status
warp-cli disconnect
