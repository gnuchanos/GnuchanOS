#!/bin/bash
set -euo pipefail

echo "=== 10: GRUB Setup ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"
CONFIG="${CONFIG:-$PROJECT_DIR/config}"

export DEBIAN_FRONTEND=noninteractive

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install GRUB
chroot "$ROOTFS" apt install -y grub-pc grub-common grub2-common

# Set default grub configuration
if [ -f "$OVERLAY/etc/default/grub" ]; then
    cp "$OVERLAY/etc/default/grub" "$ROOTFS/etc/default/grub"
else
    cat > "$ROOTFS/etc/default/grub" << 'GRUBEOF'
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_GFXMODE=1024x768x32
GRUB_GFXPAYLOAD_LINUX=keep
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash radeon.modeset=1 processor.max_cstate=1 zswap.enabled=1 vm.swappiness=10 mitigations=off nopti no_stf"
GRUB_CMDLINE_LINUX=""
GRUB_BACKGROUND="/boot/grub/themes/gnuchanos/background.png"
GRUB_THEME="/boot/grub/themes/gnuchanos/theme.txt"
GRUB_DISTRIBUTOR="GnuchanOS"
GRUBEOF
fi

# Copy GRUB theme
mkdir -p "$ROOTFS/boot/grub/themes/gnuchanos"
if [ -d "$OVERLAY/boot/grub/themes/gnuchanos" ]; then
    cp -r "$OVERLAY/boot/grub/themes/gnuchanos/"* "$ROOTFS/boot/grub/themes/gnuchanos/" 2>/dev/null || true
fi

# Copy background image from project root
if [ -f "$PROJECT_DIR/../bg.png" ]; then
    cp "$PROJECT_DIR/../bg.png" "$ROOTFS/boot/grub/themes/gnuchanos/background.png"
elif [ -f "$OVERLAY/usr/share/backgrounds/gnuchanos/bg.png" ]; then
    cp "$OVERLAY/usr/share/backgrounds/gnuchanos/bg.png" "$ROOTFS/boot/grub/themes/gnuchanos/background.png"
fi

echo "=== 10: GRUB setup complete ==="
