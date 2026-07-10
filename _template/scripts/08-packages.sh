#!/bin/bash
set -euo pipefail

echo "=== 08: Package Installation ==="

# Source build environment
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CONFIG="${CONFIG:-$PROJECT_DIR/config}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"

export DEBIAN_FRONTEND=noninteractive

# Mount chroot filesystems
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null || true

# Copy overlay resolv.conf for network
cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf" 2>/dev/null || true

# Install required packages from config file
if [ -f "$CONFIG/packages-required.txt" ]; then
    chroot "$ROOTFS" apt update
    chroot "$ROOTFS" apt install -y $(grep -v '^#' "$CONFIG/packages-required.txt")
else
    echo "WARNING: $CONFIG/packages-required.txt not found, using default package set"
    chroot "$ROOTFS" apt update
    chroot "$ROOTFS" apt install -y \
        adduser bash bash-completion ca-certificates coreutils cpio curl \
        dash debconf debianutils diffutils dpkg e2fsprogs findutils gawk git gpg \
        grep gzip hostname initramfs-tools iproute2 iptables iputils-ping kmod \
        less libc-bin locales login man-db mawk mount nano ncurses-base \
        ncurses-term netbase openssh-client openssh-server procps sed sudo tar \
        tzdata udev util-linux wget xz-utils zstd \
        xinit x11-utils x11-xserver-utils xdg-utils xterm \
        feh dunst dmenu lxappearance \
        python3 python3-pip python3-venv python3-setuptools python3-wheel \
        build-essential gcc make meson ninja-build \
        libpixman-1-dev libepoxy-dev libdrm-dev libgbm-dev libxcb*-dev \
        libxcvt-dev libxfont-dev libxkbfile-dev libpciaccess-dev \
        libudev-dev libdbus-1-dev \
        pulseaudio pavucontrol \
        yt-dlp ffmpeg \
        network-manager
fi

# Remove packages to purge
if [ -f "$CONFIG/packages-purge.txt" ]; then
    chroot "$ROOTFS" apt purge -y $(grep -v '^#' "$CONFIG/packages-purge.txt") 2>/dev/null || true
    chroot "$ROOTFS" apt autoremove -y --purge
fi

# Install Qtile Python dependencies via pip
chroot "$ROOTFS" pip3 install --prefix=/usr \
    cairocffi \
    xcffib \
    dbus-next

# Copy overlay files (package configs, NetworkManager, etc.)
cp -r "$OVERLAY/etc"/* "$ROOTFS/etc/" 2>/dev/null || true

# Enable NetworkManager
chroot "$ROOTFS" systemctl enable NetworkManager.service 2>/dev/null || true

# Disable SSH service by default
chroot "$ROOTFS" systemctl disable ssh.service 2>/dev/null || true

# Clean up
umount "$ROOTFS/dev/pts" 2>/dev/null || true
umount "$ROOTFS/dev" 2>/dev/null || true
umount "$ROOTFS/sys" 2>/dev/null || true
umount "$ROOTFS/proc" 2>/dev/null || true

echo "=== 08: Package Installation complete ==="
