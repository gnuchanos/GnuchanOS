#!/bin/bash
set -euo pipefail

echo "=== 03: Custom Kernel Build (AMD E1 Optimized) ==="

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"
CONFIG="${CONFIG:-$PROJECT_DIR/config}"

export DEBIAN_FRONTEND=noninteractive

KERNEL_VERSION="6.6.30"  # LTS kernel, good for AMD E1
KERNEL_DIR="$CACHE/linux-$KERNEL_VERSION"
KERNEL_TAR="$CACHE/linux-$KERNEL_VERSION.tar.xz"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.xz"
NUM_CORES=$(nproc)

# Mount chroot for kernel build
mount_chroot() {
    mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
    mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
    mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
}

umount_chroot() {
    umount "$ROOTFS/dev" 2>/dev/null || true
    umount "$ROOTFS/sys" 2>/dev/null || true
    umount "$ROOTFS/proc" 2>/dev/null || true
}

# Install build dependencies in chroot
mount_chroot
trap umount_chroot EXIT

chroot "$ROOTFS" apt install -y \
    build-essential flex bison dwarves libssl-dev libelf-dev \
    bc kmod cpio rsync ncurses-dev

# Download kernel source
if [ ! -f "$KERNEL_TAR" ]; then
    echo "Downloading kernel $KERNEL_VERSION..."
    wget -O "$KERNEL_TAR" "$KERNEL_URL"
fi

# Extract kernel source
if [ ! -d "$KERNEL_DIR" ]; then
    echo "Extracting kernel source..."
    mkdir -p "$KERNEL_DIR"
    tar -xf "$KERNEL_TAR" -C "$CACHE"
fi

cd "$KERNEL_DIR"

# Use custom config if exists, otherwise generate minimal config
if [ -f "$CONFIG/kernel-config" ]; then
    echo "Using custom kernel config from $CONFIG/kernel-config"
    cp "$CONFIG/kernel-config" .config
else
    echo "Generating minimal AMD E1 kernel config..."
    # Start from defconfig
    make defconfig
    
    # Enable AMD Radeon support
    ./scripts/config --enable CONFIG_DRM
    ./scripts/config --enable CONFIG_DRM_KMS_HELPER
    ./scripts/config --enable CONFIG_DRM_RADEON
    ./scripts/config --enable CONFIG_DRM_TTM
    
    # Storage
    ./scripts/config --enable CONFIG_AHCI
    ./scripts/config --enable CONFIG_ATA
    ./scripts/config --enable CONFIG_SATA_AHCI
    ./scripts/config --enable CONFIG_BLK_DEV_SD
    ./scripts/config --enable CONFIG_USB_STORAGE
    
    # Filesystems
    ./scripts/config --enable CONFIG_EXT4_FS
    ./scripts/config --enable CONFIG_VFAT_FS
    ./scripts/config --enable CONFIG_BTRFS_FS
    
    # Network drivers (AMD E1 common)
    ./scripts/config --enable CONFIG_R8169
    ./scripts/config --enable CONFIG_E1000
    ./scripts/config --enable CONFIG_E1000E
    ./scripts/config --enable CONFIG_ALX
    ./scripts/config --enable CONFIG_ATH9K
    
    # Audio
    ./scripts/config --enable CONFIG_SND
    ./scripts/config --enable CONFIG_SND_HDA_INTEL
    ./scripts/config --enable CONFIG_SND_HDA_CODEC_HDMI
    
    # Input
    ./scripts/config --enable CONFIG_INPUT_EVDEV
    ./scripts/config --enable CONFIG_MOUSE_PS2
    ./scripts/config --enable CONFIG_MOUSE_ELAN_I2C
    ./scripts/config --enable CONFIG_MOUSE_SYNAPTICS_I2C
    ./scripts/config --enable CONFIG_MOUSE_ALPS
    ./scripts/config --enable CONFIG_INPUT_TOUCHSCREEN
    ./scripts/config --enable CONFIG_TOUCHSCREEN_ELAN
    ./scripts/config --enable CONFIG_TOUCHSCREEN_SYNAPTICS
    ./scripts/config --enable CONFIG_KEYBOARD_ATKBD
    ./scripts/config --enable CONFIG_KEYBOARD_SYNP
    ./scripts/config --enable CONFIG_SERIO_I8042
    ./scripts/config --enable CONFIG_SERIO_SERPORT
    ./scripts/config --enable CONFIG_I2C_HID
    ./scripts/config --enable CONFIG_HID_MULTITOUCH
    ./scripts/config --enable CONFIG_HID_GENERIC
    
    # Disable bloat
    ./scripts/config --disable CONFIG_BT
    ./scripts/config --disable CONFIG_WIMAX
    ./scripts/config --disable CONFIG_NFC
    ./scripts/config --disable CONFIG_FIREWIRE
    ./scripts/config --disable CONFIG_STAGING
    
    # Compression
    ./scripts/config --enable CONFIG_ZSWAP
    ./scripts/config --enable CONFIG_ZRAM
    
    # Set local version
    ./scripts/config --set-str CONFIG_LOCALVERSION "-gnuchanos"
fi

# Build kernel
echo "Building kernel with $NUM_CORES cores..."
make -j"$NUM_CORES" bzImage
make -j"$NUM_CORES" modules

# Install modules to rootfs
make modules_install INSTALL_MOD_PATH="$ROOTFS"

# Install kernel and System.map
cp arch/x86/boot/bzImage "$ROOTFS/boot/vmlinuz-$KERNEL_VERSION-gnuchanos"
cp System.map "$ROOTFS/boot/System.map-$KERNEL_VERSION-gnuchanos"
cp .config "$ROOTFS/boot/config-$KERNEL_VERSION-gnuchanos"

# Generate initramfs
chroot "$ROOTFS" mkinitramfs -o /boot/initrd.img-"$KERNEL_VERSION-gnuchanos" "$KERNEL_VERSION-gnuchanos"

echo "=== 03: Kernel build complete ==="
echo "Kernel: $ROOTFS/boot/vmlinuz-$KERNEL_VERSION-gnuchanos"
echo "Initrd: $ROOTFS/boot/initrd.img-$KERNEL_VERSION-gnuchanos"
