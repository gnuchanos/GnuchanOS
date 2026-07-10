#!/bin/bash
set -euo pipefail

echo "=== Setting up GnuchanOS Build Environment ==="

# System packages required for building
apt update
apt install -y \
    debootstrap \
    squashfs-tools \
    xorriso \
    grub-common \
    grub-pc-bin \
    grub-efi-amd64-bin \
    grub-efi-amd64-signed \
    mtools \
    dosfstools \
    isolinux \
    syslinux-common \
    git \
    wget \
    curl \
    build-essential \
    gcc \
    make \
    meson \
    ninja-build \
    python3 \
    python3-pip \
    python3-venv \
    python3-setuptools \
    python3-wheel \
    libpixman-1-dev \
    libepoxy-dev \
    libdrm-dev \
    libgbm-dev \
    libxcb*-dev \
    libxcvt-dev \
    libxfont2-dev \
    libxkbfile-dev \
    libpciaccess-dev \
    libudev-dev \
    libdbus-1-dev \
    pkg-config \
    bison \
    flex \
    bc \
    kmod \
    cpio \
    rsync

echo "=== Build environment ready ==="
