#!/bin/bash
set -euo pipefail

echo "=== 01: debootstrap — Base Root System ==="

# Source project paths
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# IMPORTANT: ROOTFS must be on native Linux ext4 filesystem (not /mnt/d/).
# Windows NTFS/DrvFs doesn't support Linux special file permissions
# needed by packages like libpam-runtime (setuid, device nodes, etc.).
# This is set by build-all.sh to point to /tmp/gnuchan-build/rootfs
ROOTFS="${ROOTFS:-$PROJECT_DIR/rootfs}"
CACHE="${CACHE:-$PROJECT_DIR/cache}"

export DEBIAN_FRONTEND=noninteractive

# Ensure clean state
if [ -d "$ROOTFS" ]; then
    echo "Removing existing rootfs..."
    rm -rf "$ROOTFS"
fi
mkdir -p "$ROOTFS"

# Determine Debian release
DISTRO="${DISTRO:-bookworm}"
ARCH="${ARCH:-amd64}"

echo "Bootstrapping $DISTRO ($ARCH) into $ROOTFS..."
echo "This may take 2-5 minutes depending on network speed."

# Run debootstrap
debootstrap \
    --variant=minbase \
    --arch="$ARCH" \
    "$DISTRO" \
    "$ROOTFS" \
    http://deb.debian.org/debian

# Verify success
if [ -f "$ROOTFS/etc/debian_version" ]; then
    echo "debootstrap completed successfully."
    cat "$ROOTFS/etc/debian_version"
else
    echo "ERROR: debootstrap failed - /etc/debian_version not found"
    exit 1
fi

echo "=== 01: debootstrap complete ==="
