#!/bin/bash




yay -S cloudflare-warp-bin
sudo systemctl enable --now warp-svc
echo "systemctl status warp-svc"

sudo warp-cli registration new
sudo warp-cli connect
warp-cli status

echo "warp-cli status"
