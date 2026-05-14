#!/usr/bin/env bash
#
# Gnuchanos ISO Builder — stable mkarchiso pipeline
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISTRO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROFILE="${DISTRO_ROOT}/profile"

WORK_DIR="${WORK_DIR:-$HOME/work/gnuchanos-iso-work}"
OUT_DIR="${OUT_DIR:-$HOME/work/gnuchanos-iso-out}"

CLEAN=false
SKIP_DEPS=false

for a in "$@"; do
  [[ "$a" == "--clean" ]] && CLEAN=true
  [[ "$a" == "--skip-deps" ]] && SKIP_DEPS=true
done

log() { echo -e "\n[gnuchanos build] $*"; }
die() { echo -e "\n[ERROR] $*"; exit 1; }

# root check
[[ "$(id -u)" -ne 0 ]] && die "Root gerekli: sudo kullan"

# deps
install_deps() {
  log "Host dependencies installing..."
  pacman -Sy --needed --noconfirm archiso arch-install-scripts squashfs-tools dosfstools \
    libisoburn nodejs rsync xz gnupg || die "deps failed"
}

[[ "$SKIP_DEPS" != true ]] && install_deps

# tools check
for cmd in mkarchiso node rsync; do
  command -v "$cmd" >/dev/null 2>&1 || die "Missing: $cmd"
done

# sanity check profile
[[ ! -d "$PROFILE/airootfs" ]] && die "airootfs missing in profile"
[[ ! -f "$PROFILE/packages.x86_64" ]] && die "packages.x86_64 missing"

# cleanup
if [[ "$CLEAN" == true ]]; then
  log "Cleaning work dir..."
  rm -rf "$WORK_DIR" "$OUT_DIR"
fi

mkdir -p "$WORK_DIR" "$OUT_DIR"

log "Profile: $PROFILE"
log "Work: $WORK_DIR"
log "Out: $OUT_DIR"

# stage 1: dotfiles sync
log "[1/3] Dotfiles + branding"
bash "$SCRIPT_DIR/sync-dotfiles-into-profile.sh"

# stage 2: package list
log "[2/3] packages generation"
node "$SCRIPT_DIR/emit-packages-x86_64.mjs"

# IMPORTANT FIX: ensure clean airootfs state
log "Resetting airootfs safe state..."
mkdir -p "$PROFILE/airootfs/root"
mkdir -p "$PROFILE/airootfs/etc"

# FIX shadow (must exist, but empty safe placeholder)
touch "$PROFILE/airootfs/etc/shadow"
chmod 600 "$PROFILE/airootfs/etc/shadow"

# stage 3: build ISO
log "[3/3] mkarchiso build"
mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE"

# result
log "DONE"
ls -lh "$OUT_DIR"/*.iso 2>/dev/null || true