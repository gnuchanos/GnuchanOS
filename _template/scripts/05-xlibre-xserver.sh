#!/bin/bash
set -euo pipefail

echo "=== 05: X11Libre XServer Build ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"

export DEBIAN_FRONTEND=noninteractive

XSERVER_DIR="$CACHE/xserver"
XSERVER_REPO="https://github.com/x11libre/xserver.git"

NUM_CORES=$(nproc)

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install build dependencies
chroot "$ROOTFS" apt install -y \
    meson ninja-build pkg-config \
    libpixman-1-dev libepoxy-dev libdrm-dev libgbm-dev \
    libxcb*-dev libxcvt-dev libxfont2-dev libxkbfile-dev \
    libpciaccess-dev libudev-dev libdbus-1-dev \
    x11proto-dev xtrans-dev libxau-dev libxdmcp-dev \
    libxshmfence-dev libxxf86vm-dev

# Clone X11Libre XServer
if [ ! -d "$XSERVER_DIR" ]; then
    echo "Cloning X11Libre XServer..."
    git clone "$XSERVER_REPO" "$XSERVER_DIR"
fi

cd "$XSERVER_DIR"
git pull 2>/dev/null || true

# Build with minimal options
meson setup build/ \
    --prefix=/usr \
    -Dglx=true \
    -Dglamor=false \
    -Ddri3=true \
    -Dudev=true \
    -Dsystemd_logind=true \
    -Dxnest=false \
    -Dxwayland=false \
    -Dxvfb=false

ninja -C build/ -j"$NUM_CORES"

# Install to rootfs
DESTDIR="$ROOTFS" ninja -C build/ install

echo "=== 05: X11Libre XServer build complete ==="
