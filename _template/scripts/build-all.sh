#!/bin/bash
set -euo pipefail

# ============================================================================
# GnuchanOS Build Orchestrator
# ============================================================================
# Usage:
#   ./build-all.sh                    # Full build
#   ./build-all.sh --skip-kernel      # Skip kernel build (use Debian kernel)
#   ./build-all.sh --skip-liberated   # Skip liberated systemd (use upstream)
#   ./build-all.sh --skip-xlibre      # Skip X11Libre (use Xorg)
#   ./build-all.sh --skip-qtile       # Skip Qtile source build (use pip)
#   ./build-all.sh --resume N         # Resume from script N (e.g., --resume 8)
#   ./build-all.sh --clean            # Clean rootfs before starting
# ============================================================================

# === Project Paths (on D:\ for editing) ===
export PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
export OVERLAY="$PROJECT_DIR/overlay"
export CONFIG="$PROJECT_DIR/config"
export SCRIPTS="$PROJECT_DIR/scripts"

# === Build paths MUST be on native ext4 (WSL filesystem), not DrvFs/NTFS ===
# Windows NTFS/DrvFs doesn't support Linux special file permissions
# (setuid, device nodes, etc.), which breaks debootstrap extraction of
# packages like libpam-runtime, and also breaks chroot operations.
BUILD_BASE="/tmp/gnuchan-build"
export ROOTFS="$BUILD_BASE/rootfs"
export ISO_DIR="$BUILD_BASE/iso"
export CACHE="$BUILD_BASE/cache"

# === Build Config ===
export DISTRO="${DISTRO:-bookworm}"
export ARCH="${ARCH:-amd64}"
export USERNAME="${USERNAME:-gnuchan}"
export PASSWORD="${PASSWORD:-gnuchan}"
export HOSTNAME="${HOSTNAME:-gnuchanos}"
export LANGUAGE="${LANGUAGE:-en_US.UTF-8}"
export KEYBOARD_LAYOUT="${KEYBOARD_LAYOUT:-us}"
export TIMEZONE="${TIMEZONE:-UTC}"

# === Misc ===
export DEBIAN_FRONTEND="noninteractive"
# NOTE: Do NOT export TMPDIR - it leaks into chroot commands and breaks
# packages like ca-certificates that try to use the host temp dir path
# inside the chroot where it doesn't exist.
TMPDIR="${TMPDIR:-$CACHE/tmp}"
mkdir -p "$TMPDIR"
LOGFILE="$CACHE/build.log"

# === Parse Arguments ===
SKIP_KERNEL=false
SKIP_LIBERATED=false
SKIP_XLIBRE=false
SKIP_QTILE=false
RESUME_FROM=1
DO_CLEAN=false

for arg in "$@"; do
    case $arg in
        --skip-kernel) SKIP_KERNEL=true ;;
        --skip-liberated) SKIP_LIBERATED=true ;;
        --skip-xlibre) SKIP_XLIBRE=true ;;
        --skip-qtile) SKIP_QTILE=true ;;
        --resume) RESUME_FROM="$2"; shift ;;
        --clean) DO_CLEAN=true ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
    shift
done

# === Logging ===
log() {
    echo "[$(date '+%H:%M:%S')] $1" | tee -a "$LOGFILE"
}

# === Script Runner ===
run_script() {
    local script="$1"
    local name=$(basename "$script")
    log "=== Running $name ==="
    # Pass critical path vars explicitly so they work even under sudo
    if bash -c "
      export ROOTFS='$ROOTFS'
      export CACHE='$CACHE'
      export ISO_DIR='$ISO_DIR'
      export OVERLAY='$OVERLAY'
      export CONFIG='$CONFIG'
      export SCRIPTS='$SCRIPTS'
      export PROJECT_DIR='$PROJECT_DIR'
      export DEBIAN_FRONTEND=noninteractive
      bash '$script'
    " 2>&1 | tee -a "$LOGFILE"; then
        log "=== $name OK ==="
        return 0
    else
        log "=== FAILED: $name ==="
        return 1
    fi
}

# === Check DrvFs (NTFS) usage and warn ===
if df "$PROJECT_DIR" | grep -q 'drvfs\|9p\|fuseblk'; then
    log "WARNING: Project is on DrvFs/NTFS ($PROJECT_DIR). Build relocated to ext4: $BUILD_BASE"
fi

# === Clean build dirs if requested ===
if [ "$DO_CLEAN" = true ]; then
    log "Cleaning build directories..."
    rm -rf "$BUILD_BASE"
fi

# Create necessary directories
mkdir -p "$ROOTFS" "$ISO_DIR" "$CACHE" "$TMPDIR"

# === Build Pipeline ===
log "=== GnuchanOS Build Started ==="
log "Distro: $DISTRO, Arch: $ARCH"
log "Build base (ext4): $BUILD_BASE"
log "Rootfs: $ROOTFS"
log "Logfile: $LOGFILE"
log "Project dir (editing): $PROJECT_DIR"

# Script order
run_script "$SCRIPTS/01-bootstrap.sh"      || exit 1
run_script "$SCRIPTS/02-chroot-configure.sh" || exit 1

if [ "$SKIP_KERNEL" = false ]; then
    run_script "$SCRIPTS/03-kernel.sh" || {
        log "WARNING: Custom kernel build failed, using Debian kernel"
        mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
        mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
        mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
        chroot "$ROOTFS" apt install -y linux-image-amd64 firmware-linux || true
        umount "$ROOTFS/dev" 2>/dev/null || true
        umount "$ROOTFS/sys" 2>/dev/null || true
        umount "$ROOTFS/proc" 2>/dev/null || true
    }
else
    log "Skipping kernel build (--skip-kernel)"
fi

if [ "$SKIP_LIBERATED" = false ]; then
    run_script "$SCRIPTS/04-liberated-systemd.sh" || {
        log "WARNING: Liberated systemd build failed, using upstream systemd"
        mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
        mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
        mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
        chroot "$ROOTFS" apt install -y systemd || true
        umount "$ROOTFS/dev" 2>/dev/null || true
        umount "$ROOTFS/sys" 2>/dev/null || true
        umount "$ROOTFS/proc" 2>/dev/null || true
    }
else
    log "Skipping liberated systemd build (--skip-liberated)"
fi

if [ "$SKIP_XLIBRE" = false ]; then
    run_script "$SCRIPTS/05-xlibre-xserver.sh" || {
        log "WARNING: X11Libre build failed, using Xorg"
        mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
        mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
        mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
        chroot "$ROOTFS" apt install -y xserver-xorg-core xserver-xorg-video-radeon || true
        umount "$ROOTFS/dev" 2>/dev/null || true
        umount "$ROOTFS/sys" 2>/dev/null || true
        umount "$ROOTFS/proc" 2>/dev/null || true
    }
else
    log "Skipping X11Libre build (--skip-xlibre)"
fi

if [ "$SKIP_QTILE" = false ]; then
    run_script "$SCRIPTS/06-qtile.sh" || {
        log "WARNING: Qtile source build failed, installing via pip"
        mount --bind /proc "$ROOTFS/proc" 2>/dev/null || true
        mount --bind /sys "$ROOTFS/sys" 2>/dev/null || true
        mount --bind /dev "$ROOTFS/dev" 2>/dev/null || true
        chroot "$ROOTFS" pip3 install qtile || true
        umount "$ROOTFS/dev" 2>/dev/null || true
        umount "$ROOTFS/sys" 2>/dev/null || true
        umount "$ROOTFS/proc" 2>/dev/null || true
    }
else
    log "Skipping Qtile source build (--skip-qtile)"
fi

run_script "$SCRIPTS/07-gnuchangui.sh"     || log "WARNING: GnuChanGUI install had issues"
run_script "$SCRIPTS/08-packages.sh"       || exit 1
run_script "$SCRIPTS/09-bootsplash.sh"     || log "WARNING: Bootsplash setup had issues"
run_script "$SCRIPTS/10-grub.sh"           || log "WARNING: GRUB setup had issues"
run_script "$SCRIPTS/11-cleanup.sh"        || log "WARNING: Cleanup had issues"
run_script "$SCRIPTS/12-iso.sh"            || exit 1

# === Copy ISO back to D:\ for Windows access ===
PROJECT_ISO="$PROJECT_DIR/iso"
mkdir -p "$PROJECT_ISO"
if [ -f "$ISO_DIR/gnuchanos.iso" ]; then
    log "Copying ISO to $PROJECT_ISO/gnuchanos.iso ..."
    cp "$ISO_DIR/gnuchanos.iso" "$PROJECT_ISO/gnuchanos.iso"
    log "ISO copied to project dir (D:\)."
else
    log "WARNING: ISO not found at $ISO_DIR/gnuchanos.iso"
fi

log "=== Build Complete! ==="
log "Build data (ext4): $BUILD_BASE"
log "ISO (ext4): $ISO_DIR/gnuchanos.iso"
log "ISO (project): $PROJECT_ISO/gnuchanos.iso"
ls -lh "$ISO_DIR/gnuchanos.iso" 2>/dev/null || true
ls -lh "$PROJECT_ISO/gnuchanos.iso" 2>/dev/null || true
