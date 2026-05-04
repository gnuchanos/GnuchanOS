#!/usr/bin/env bash
# Run from Linux (or WSL with paths adjusted). Copies repo Dotfile into ISO profile overlay.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISTRO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOTFILE_SRC="$(cd "${DISTRO_ROOT}/../Dotfile" && pwd)"
DEST="${DISTRO_ROOT}/profile/airootfs/usr/share/gnuchanos/dotfiles"

if [[ ! -d "${DOTFILE_SRC}" ]]; then
  echo "Missing Dotfile tree: ${DOTFILE_SRC}" >&2
  exit 1
fi

mkdir -p "${DEST}"
rsync -a --delete "${DOTFILE_SRC}/" "${DEST}/"
echo "Dotfile -> ${DEST}"

bash "${SCRIPT_DIR}/sync-branding-assets.sh"
