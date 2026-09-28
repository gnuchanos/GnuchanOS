#!/bin/bash
cd /mnt/d/GnuchanOS/gnuchan_softwares/gcl_WM || exit 1
echo "===== 1140..1320 ====="
sed -n '1140,1320p' wm_desktop.c
echo
echo "===== 1320..1472 ====="
sed -n '1320,1472p' wm_desktop.c
