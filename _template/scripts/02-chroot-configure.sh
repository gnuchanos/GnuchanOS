#!/bin/bash
set -euo pipefail

echo "=== 02: chroot Configuration ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"
CONFIG="${CONFIG:-$PROJECT_DIR/config}"

export DEBIAN_FRONTEND=noninteractive
HOSTNAME="${HOSTNAME:-gnuchanos}"
USERNAME="${USERNAME:-gnuchan}"
PASSWORD="${PASSWORD:-gnuchan}"
LANGUAGE="${LANGUAGE:-en_US.UTF-8}"
KEYBOARD_LAYOUT="${KEYBOARD_LAYOUT:-us}"
TIMEZONE="${TIMEZONE:-UTC}"

# Mount chroot filesystems
mount_chroot() {
    mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
    mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
    mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
    mount --bind /dev/pts "$ROOTFS/dev/pts" 2>/dev/null || true
}

umount_chroot() {
    umount "$ROOTFS/dev/pts" 2>/dev/null || true
    umount "$ROOTFS/dev" 2>/dev/null || true
    umount "$ROOTFS/sys" 2>/dev/null || true
    umount "$ROOTFS/proc" 2>/dev/null || true
}

mount_chroot
trap umount_chroot EXIT

# Copy resolv.conf for network access
cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf" 2>/dev/null || true

# Set hostname
echo "$HOSTNAME" > "$ROOTFS/etc/hostname"

# Set hosts
cat > "$ROOTFS/etc/hosts" << 'EOF'
127.0.0.1   localhost
127.0.1.1   gnuchanos.localdomain gnuchanos

::1         localhost ip6-localhost ip6-loopback
ff02::1     ip6-allnodes
ff02::2     ip6-allrouters
EOF

# Configure APT sources
cat > "$ROOTFS/etc/apt/sources.list" << 'EOF'
deb http://deb.debian.org/debian bookworm main contrib non-free-firmware
deb http://deb.debian.org/debian-security bookworm-security main contrib non-free-firmware
deb http://deb.debian.org/debian bookworm-updates main contrib non-free-firmware
EOF

# Ensure non-interactive mode for apt/dpkg inside chroot
cat > "$ROOTFS/etc/apt/apt.conf.d/99noninteractive" << 'APTEOF'
APT::Get::Assume-Yes "true";
APT::Get::force-yes "true";
APT::Get::allow-unauthenticated "false";
Dpkg::Options {
    "--force-confdef";
    "--force-confold";
}
APTEOF

# Install locales package and generate locale
# NOTE: unset TMPDIR as the chroot doesn't have access to our temp dir
chroot "$ROOTFS" bash -c "unset TMPDIR; apt install -y locales" 2>/dev/null || true
if [ -f "$ROOTFS/etc/locale.gen" ]; then
    chroot "$ROOTFS" sed -i "s/^# *${LANGUAGE}/${LANGUAGE}/" /etc/locale.gen
    chroot "$ROOTFS" locale-gen
else
    # Fallback: create locale manually if package install failed
    mkdir -p "$ROOTFS/usr/lib/locale"
    localedef --force --inputfile=en_US --charmap=UTF-8 --prefix="$ROOTFS" "$LANGUAGE" 2>/dev/null || true
fi
chroot "$ROOTFS" update-locale LANG="$LANGUAGE" LANGUAGE="${LANGUAGE%%.*}:en" LC_ALL="$LANGUAGE" 2>/dev/null || true

# Set timezone
chroot "$ROOTFS" ln -sf "/usr/share/zoneinfo/$TIMEZONE" /etc/localtime

# Set keyboard layout
cat > "$ROOTFS/etc/default/keyboard" << 'EOF'
XKBMODEL="pc105"
XKBLAYOUT="us"
XKBVARIANT=""
XKBOPTIONS=""
BACKSPACE="guess"
EOF

# Console keymap
echo "KEYMAP=${KEYBOARD_LAYOUT}" > "$ROOTFS/etc/vconsole.conf"

# Create user
chroot "$ROOTFS" useradd -m -s /bin/bash -G sudo,audio,video,plugdev "$USERNAME" 2>/dev/null || true
echo "${USERNAME}:${PASSWORD}" | chroot "$ROOTFS" chpasswd

# Lock root password
chroot "$ROOTFS" passwd -l root 2>/dev/null || true

# Set environment
cat > "$ROOTFS/etc/environment" << 'EOF'
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
LANG=en_US.UTF-8
LANGUAGE=en_US:en
LC_ALL=en_US.UTF-8
EDITOR=nano
EOF

# Set os-release
cat > "$ROOTFS/etc/os-release" << 'EOF'
PRETTY_NAME="GnuchanOS 1.0 (Minimal)"
NAME="GnuchanOS"
VERSION_ID="1.0"
VERSION="1.0 (Minimal)"
VERSION_CODENAME=gnuchanos
ID=gnuchanos
ID_LIKE=debian
HOME_URL="https://github.com/gnuchanos/GnuchanOS"
SUPPORT_URL="https://github.com/gnuchanos/GnuchanOS"
BUG_REPORT_URL="https://github.com/gnuchanos/GnuchanOS"
LOGO=/usr/share/gnuchanos/logo.png
ANSI_COLOR="0;36"
EOF

# Update apt and install base packages
# IMPORTANT: Unset TMPDIR for chroot commands - the chroot environment
# doesn't have access to the build host's TMPDIR path
chroot "$ROOTFS" bash -c "unset TMPDIR; apt update"
chroot "$ROOTFS" bash -c "unset TMPDIR; apt install -y apt-transport-https ca-certificates curl gnupg"

echo "=== 02: chroot configuration complete ==="
