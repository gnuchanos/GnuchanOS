#!/bin/bash
set -euo pipefail

echo "=== 09: Plymouth Bootsplash Setup ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"

export DEBIAN_FRONTEND=noninteractive

# Mount chroot
mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true

trap 'umount "$ROOTFS/dev" 2>/dev/null; umount "$ROOTFS/sys" 2>/dev/null; umount "$ROOTFS/proc" 2>/dev/null' EXIT

# Install Plymouth
chroot "$ROOTFS" apt install -y plymouth plymouth-label

# Create Plymouth theme directory
mkdir -p "$ROOTFS/usr/share/plymouth/themes/gnuchanos"

# Copy theme files from overlay
cp "$OVERLAY/usr/share/plymouth/themes/gnuchanos/"* "$ROOTFS/usr/share/plymouth/themes/gnuchanos/" 2>/dev/null || true

# Copy logo
cp "$PROJECT_DIR/../logo.png" "$ROOTFS/usr/share/plymouth/themes/gnuchanos/logo.png" 2>/dev/null || true

# Set as default theme
chroot "$ROOTFS" plymouth-set-default-theme gnuchanos 2>/dev/null || {
    # Manual fallback
    cat > "$ROOTFS/etc/plymouth/plymouthd.conf" << 'EOF'
[Daemon]
Theme=gnuchanos
ShowDelay=0
EOF
}

# Add plymouth to initramfs
echo "add plymouth to initramfs" >> "$ROOTFS/etc/initramfs-tools/conf.d/plymouth" 2>/dev/null || true

echo "=== 09: Bootsplash setup complete ==="
