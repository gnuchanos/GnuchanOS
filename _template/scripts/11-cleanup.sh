#!/bin/bash
set -euo pipefail

echo "=== 11: Cleanup & Minimization ==="

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

# Copy overlay files (configs, themes, user files)
echo "Copying overlay files to rootfs..."
cp -r "$OVERLAY/etc/"* "$ROOTFS/etc/" 2>/dev/null || true
cp -r "$OVERLAY/usr/"* "$ROOTFS/usr/" 2>/dev/null || true
cp -r "$OVERLAY/home/"* "$ROOTFS/home/" 2>/dev/null || true
cp -r "$OVERLAY/boot/"* "$ROOTFS/boot/" 2>/dev/null || true

# Set correct permissions
chroot "$ROOTFS" chown -R gnuchan:gnuchan /home/gnuchan 2>/dev/null || true

# Remove packages to purge
if [ -f "$CONFIG/packages-purge.txt" ]; then
    PURGE_PKGS=$(grep -v '^#' "$CONFIG/packages-purge.txt" | tr '\n' ' ')
    if [ -n "$PURGE_PKGS" ]; then
        echo "Purging unnecessary packages..."
        chroot "$ROOTFS" apt purge -y $PURGE_PKGS 2>/dev/null || true
    fi
fi

# Autoremove orphans
chroot "$ROOTFS" apt autoremove -y --purge 2>/dev/null || true

# Clean apt cache
chroot "$ROOTFS" apt clean
chroot "$ROOTFS" apt autoclean

# Remove documentation (save space)
rm -rf "$ROOTFS/usr/share/doc/"* 2>/dev/null || true
rm -rf "$ROOTFS/usr/share/man/"* 2>/dev/null || true
rm -rf "$ROOTFS/usr/share/info/"* 2>/dev/null || true
rm -rf "$ROOTFS/usr/share/lintian/"* 2>/dev/null || true
rm -rf "$ROOTFS/var/cache/apt/archives/"*.deb 2>/dev/null || true
rm -rf "$ROOTFS/var/log/"*.log 2>/dev/null || true
rm -rf "$ROOTFS/var/log/apt/"* 2>/dev/null || true

# Remove SSH host keys (regenerated on first boot)
rm -f "$ROOTFS/etc/ssh/"ssh_host_* 2>/dev/null || true

# Remove machine-id (regenerated on first boot)
rm -f "$ROOTFS/etc/machine-id" 2>/dev/null || true
: > "$ROOTFS/etc/machine-id" 2>/dev/null || true

# Remove tmp files
rm -rf "$ROOTFS/tmp/"* 2>/dev/null || true
rm -rf "$ROOTFS/var/tmp/"* 2>/dev/null || true

echo "=== 11: Cleanup complete ==="
