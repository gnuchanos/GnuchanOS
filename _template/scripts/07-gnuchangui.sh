#!/bin/bash
set -euo pipefail

echo "=== 07: GnuChanGUI Library Install ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"

export DEBIAN_FRONTEND=noninteractive

GUI_DIR="$CACHE/gnuchangui"
GUI_REPO="https://github.com/gnuchanos/GnuChanGUI.git"

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install Python dependencies for GnuChanGUI
chroot "$ROOTFS" apt install -y python3-pil python3-pil.imagetk 2>/dev/null || true
chroot "$ROOTFS" pip3 install --prefix=/usr --root-user-action=ignore Pillow 2>/dev/null || true

# Clone GnuChanGUI
if [ ! -d "$GUI_DIR" ]; then
    echo "Cloning GnuChanGUI..."
    git clone "$GUI_REPO" "$GUI_DIR"
fi

cd "$GUI_DIR"
git pull 2>/dev/null || true

# Install into overlay
mkdir -p "$OVERLAY/home/gnuchan/.config/GnuChanGUI"
cp -r "$GUI_DIR/gnuchangui" "$OVERLAY/home/gnuchan/.config/GnuChanGUI/" 2>/dev/null || {
    # If structure is different, copy all python files
    cp *.py "$OVERLAY/home/gnuchan/.config/GnuChanGUI/" 2>/dev/null || true
}

# Copy to rootfs as well
mkdir -p "$ROOTFS/home/gnuchan/.config/GnuChanGUI"
cp -r "$OVERLAY/home/gnuchan/.config/GnuChanGUI"/* "$ROOTFS/home/gnuchan/.config/GnuChanGUI/" 2>/dev/null || true
chroot "$ROOTFS" chown -R gnuchan:gnuchan /home/gnuchan/.config/GnuChanGUI 2>/dev/null || true

echo "=== 07: GnuChanGUI install complete ==="
