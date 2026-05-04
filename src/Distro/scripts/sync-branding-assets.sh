#!/usr/bin/env bash
# Repo kokundeki logo.png / bg.png -> ISO profili ve plymouth varliklari.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PROFILE="${SCRIPT_DIR}/../profile"
SYS="${PROFILE}/syslinux"
DOT="${REPO_ROOT}/src/Dotfile"
PLY_ASSETS="${DOT}/gnuchanBoot/assets"
BR="${PROFILE}/airootfs/usr/share/gnuchanos/branding"

LOGO="${REPO_ROOT}/logo.png"
BG="${REPO_ROOT}/bg.png"

if [[ ! -f "${LOGO}" ]]; then
  echo "Eksik: ${LOGO}" >&2
  exit 1
fi

mkdir -p "${SYS}" "${BR}" "${PLY_ASSETS}"
cp -f "${LOGO}" "${SYS}/gnuchanos-bg.png"
cp -f "${LOGO}" "${BR}/logo.png"
if [[ -f "${BG}" ]]; then
  cp -f "${BG}" "${SYS}/gnuchanos-bg.png"
  cp -f "${BG}" "${BR}/bg.png"
else
  echo "Not: ${BG} yok; syslinux arka plani logo.png ile dolduruldu (bg.png ekleyebilirsin)."
  cp -f "${LOGO}" "${BR}/bg.png"
fi

cp -f "${LOGO}" "${PLY_ASSETS}/logo.png"
cp -f "${SYS}/gnuchanos-bg.png" "${PLY_ASSETS}/background.png"
# Plymouth animasyonu yoksa arka plani tekrar kullan (script ImageNew bekliyor)
cp -f "${SYS}/gnuchanos-bg.png" "${PLY_ASSETS}/animation.png"
cp -f "${SYS}/gnuchanos-bg.png" "${PLY_ASSETS}/suspend.png"

install -d "${DOT}/qtile"
cp -f "${BR}/bg.png" "${DOT}/qtile/bg.png"

echo "Branding -> ${SYS}/gnuchanos-bg.png, ${PLY_ASSETS}, ${DOT}/qtile/bg.png"
