#!/usr/bin/env bash
#
# Gnuchanos live ISO — tek komutla derleme (Arch Linux host veya WSL2 Arch).
#

set -euo pipefail

usage() {
  sed -n '1,25p' "$0" | sed 's/^# \{0,1\}//'
  exit 0
}

for a in "$@"; do
  [[ "$a" == "-h" || "$a" == "--help" ]] && usage
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISTRO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${DISTRO_ROOT}/../.." && pwd)"
PROFILE="$(cd "${DISTRO_ROOT}/profile" && pwd)"

WORK_DIR="${WORK_DIR:-$HOME/work/gnuchanos-iso-work}"
OUT_DIR="${OUT_DIR:-$HOME/work/gnuchanos-iso-out}"
CLEAN=false
SKIP_DEPS=false
for a in "$@"; do
  [[ "$a" == "--clean" ]] && CLEAN=true
  [[ "$a" == "--skip-deps" ]] && SKIP_DEPS=true
done

log() { printf '\n[\033[0;36mgnuchanos build\033[0m] %s\n' "$*" >&2; }
die() { printf '\n[\033[0;31mHATA\033[0m] %s\n' "$*" >&2; exit 1; }

if [[ "$(id -u)" -ne 0 ]]; then
  die "Root gerekli. Ornek: sudo $0 $*"
fi

install_host_deps() {
  local pkgs=(
    archiso arch-install-scripts squashfs-tools dosfstools
    libisoburn nodejs rsync xz gnupg
  )
  log "Host bagimliliklari: pacman -Sy --needed --noconfirm (${#pkgs[@]} paket)"
  pacman -Sy --needed --noconfirm "${pkgs[@]}" \
    || die "pacman bagimlilik kurulumu basarisiz."
}

[[ "$SKIP_DEPS" != true ]] && install_host_deps

for need in mkarchiso pacstrap rsync mksquashfs xz gpg node; do
  command -v "$need" >/dev/null 2>&1 || die "Eksik: $need"
done

[[ ! -f "${REPO_ROOT}/logo.png" ]] && die "Depo kokunde logo.png bulunamadi: ${REPO_ROOT}/logo.png"
grep -q '^\[multilib\]' /etc/pacman.conf || log "UYARI: [multilib] yok; lib32 paketleri basarisiz olabilir."

avail_kb="$(df -Pk "${WORK_DIR%/*}" 2>/dev/null | awk 'NR==2 {print $4}')"
[[ -n "$avail_kb" && "$avail_kb" -lt 8388608 ]] && log "UYARI: ~8 GiB altinda bos alan var; derleme dusebilir."

[[ "$CLEAN" == true && -d "$WORK_DIR" ]] && { log "Temizleniyor: $WORK_DIR"; rm -rf "$WORK_DIR"; }

mkdir -p "$WORK_DIR" "$OUT_DIR"

log "Depo:     $REPO_ROOT"
log "Profil:   $PROFILE"
log "Work:     $WORK_DIR"
log "Cikti:    $OUT_DIR"

log "[1/3] Dotfile + branding senkronu"
bash "$SCRIPT_DIR/sync-dotfiles-into-profile.sh"

log "[2/3] packages.x86_64 (gnu_pkg_lists.py -> Node)"
node "$SCRIPT_DIR/emit-packages-x86_64.mjs"

log "[3/3] mkarchiso (uzun surebilir)"
mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE"

log "Bitti. ISO dosyalari:"
ls -la "$OUT_DIR"/*.iso 2>/dev/null || ls -la "$OUT_DIR"