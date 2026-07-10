#!/bin/bash
set -euo pipefail

echo "=== 04: Liberated systemd Build ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"
CONFIG="${CONFIG:-$PROJECT_DIR/config}"

export DEBIAN_FRONTEND=noninteractive

LIBERT_DIR="$CACHE/liberated-systemd"
LIBERT_REPO="https://github.com/Jeffrey-Sardina/liberated-systemd.git"

NUM_CORES=$(nproc)

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install build deps
chroot "$ROOTFS" apt install -y meson ninja-build pkg-config \
    libcap-dev libmount-dev libseccomp-dev libblkid-dev \
    liblz4-dev liblzma-dev libzstd-dev

# Clone liberated systemd
if [ ! -d "$LIBERT_DIR" ]; then
    echo "Cloning liberated-systemd..."
    git clone "$LIBERT_REPO" "$LIBERT_DIR"
fi

cd "$LIBERT_DIR"
git pull 2>/dev/null || true

# Build with minimal options
meson setup build/ \
    --prefix=/usr \
    -Drootprefix=/usr \
    -Dsysvinit=false \
    -Dpam=false \
    -Dacl=false \
    -Daudit=false \
    -Dgcrypt=false \
    -Dgnutls=false \
    -Dmicrohttpd=false \
    -Dquotacheck=false \
    -Dtmpfiles=true \
    -Dhwdb=true \
    -Dman=false \
    -Dnetworkd=false \
    -Dtimesyncd=false \
    -Dlocaled=false \
    -Dhostnamed=false \
    -Dnss-resolve=false \
    -Dnss-myhostname=false \
    -Dnss-mymachines=false \
    -Dnss-systemd=false

ninja -C build/ -j"$NUM_CORES"

# Install to rootfs
DESTDIR="$ROOTFS" ninja -C build/ install

echo "=== 04: Liberated systemd build complete ==="
