#!/bin/bash
set -euo pipefail

echo "=== 12: ISO Creation ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
ISO_DIR="${ISO_DIR:-$PROJECT_DIR/iso}"
OVERLAY="${OVERLAY:-$PROJECT_DIR/overlay}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"

export DEBIAN_FRONTEND=noninteractive

# Ensure ISO directory is clean
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub/themes/gnuchanos"
mkdir -p "$ISO_DIR/live"
mkdir -p "$ISO_DIR/isolinux"

# 1. Create SquashFS from rootfs
echo "Creating SquashFS (zstd compression)..."
mksquashfs "$ROOTFS" "$ISO_DIR/live/filesystem.squashfs" \
    -comp zstd -Xcompression-level 15 \
    -noappend -e boot

# 2. Copy kernel and initrd
echo "Copying kernel and initrd..."
cp "$ROOTFS/boot/vmlinuz"* "$ISO_DIR/boot/vmlinuz" 2>/dev/null || {
    echo "WARNING: No kernel found, using fallback"
    ls -la "$ROOTFS/boot/"
}
cp "$ROOTFS/boot/initrd.img"* "$ISO_DIR/boot/initrd.img" 2>/dev/null || {
    echo "WARNING: No initrd found"
}

# 3. Copy GRUB live config
if [ -f "$OVERLAY/boot/grub/grub.cfg" ]; then
    cp "$OVERLAY/boot/grub/grub.cfg" "$ISO_DIR/boot/grub/grub.cfg"
else
    # Create default live boot config
    cat > "$ISO_DIR/boot/grub/grub.cfg" << 'GRUBEOF'
set timeout=10
set default=0

menuentry "GnuchanOS 1.0 — Live (BIOS)" {
    linux /boot/vmlinuz boot=live live-media-path=/live \
          quiet splash radeon.modeset=1 \
          processor.max_cstate=1 zswap.enabled=1 \
          vm.swappiness=10 mitigations=off nopti no_stf \
          init=/lib/systemd/systemd
    initrd /boot/initrd.img
}

menuentry "GnuchanOS 1.0 — Live (BIOS, nomodeset)" {
    linux /boot/vmlinuz boot=live live-media-path=/live \
          quiet splash radeon.modeset=0 nomodeset \
          processor.max_cstate=1 \
          init=/lib/systemd/systemd
    initrd /boot/initrd.img
}

menuentry "GnuchanOS 1.0 — Live (BIOS, debug)" {
    linux /boot/vmlinuz boot=live live-media-path=/live \
          radeon.modeset=1 loglevel=7 systemd.log_level=debug \
          init=/lib/systemd/systemd
    initrd /boot/initrd.img
}

menuentry "Boot from first hard disk" {
    set root=(hd0)
    chainloader +1
}
GRUBEOF
fi

# 4. Copy GRUB theme
cp -r "$OVERLAY/boot/grub/themes/gnuchanos/"* "$ISO_DIR/boot/grub/themes/gnuchanos/" 2>/dev/null || true
cp "$PROJECT_DIR/../bg.png" "$ISO_DIR/boot/grub/themes/gnuchanos/background.png" 2>/dev/null || true
cp "$PROJECT_DIR/../logo.png" "$ISO_DIR/boot/grub/themes/gnuchanos/logo.png" 2>/dev/null || true

# 5. Create ISOLINUX config (BIOS fallback)
cat > "$ISO_DIR/isolinux/isolinux.cfg" << 'ISOLINUXEOF'
DEFAULT gnuchanos
LABEL gnuchanos
    SAY Booting GnuchanOS 1.0...
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.img boot=live live-media-path=/live quiet splash radeon.modeset=1 processor.max_cstate=1 zswap.enabled=1 vm.swappiness=10 mitigations=off nopti no_stf init=/lib/systemd/systemd
IPAPPEND 2
ISOLINUXEOF

# 6. Create hybrid ISO with xorriso
OUTPUT_ISO="$ISO_DIR/gnuchanos.iso"

echo "Creating hybrid ISO..."
xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "GNUCHANOS" \
    -eltorito-boot isolinux/isolinux.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --eltorito-catalog isolinux/boot.cat \
    -eltorito-alt-boot \
    -e boot/grub/efi.img \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    -o "$OUTPUT_ISO" \
    "$ISO_DIR"

echo "=== 12: ISO creation complete ==="
echo "Output: $OUTPUT_ISO"
ls -lh "$OUTPUT_ISO"
