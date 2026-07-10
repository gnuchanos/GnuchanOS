#!/bin/bash
set -euo pipefail

echo "=== 06: QTile Source Build ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"

export DEBIAN_FRONTEND=noninteractive

QTILE_DIR="$CACHE/qtile"
QTILE_REPO="https://github.com/qtile/qtile.git"

NUM_CORES=$(nproc)

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install Python build deps and Qtile deps
chroot "$ROOTFS" apt install -y \
    python3 python3-pip python3-venv python3-setuptools python3-wheel \
    python3-dbus libdbus-1-dev libcairo2-dev libgirepository1.0-dev \
    pkg-config

# Clone Qtile
if [ ! -d "$QTILE_DIR" ]; then
    echo "Cloning Qtile..."
    git clone "$QTILE_REPO" "$QTILE_DIR"
fi

cd "$QTILE_DIR"
git pull 2>/dev/null || true

# Install Qtile and dependencies
pip3 install --prefix=/usr --root-user-action=ignore \
    xcffib cairocffi dbus-next

# Build and install Qtile
python3 setup.py build
python3 setup.py install --prefix=/usr --root="$ROOTFS"

# Copy QTile config files from overlay
mkdir -p "$ROOTFS/home/gnuchan/.config/qtile"
cp -r "$OVERLAY/home/gnuchan/.config/qtile"/* "$ROOTFS/home/gnuchan/.config/qtile/" 2>/dev/null || true
chroot "$ROOTFS" chown -R gnuchan:gnuchan /home/gnuchan/.config/qtile

echo "=== 06: QTile build complete ==="
