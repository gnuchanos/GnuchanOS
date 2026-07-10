# GnuchanOS Build Fixes — Completed

## Root Cause Found & Fixed

**Problem:** `E: Tried to extract package, but tar failed` on `libpam-runtime`
**Root cause:** `debootstrap` was running on Windows NTFS via DrvFs (`/mnt/d/`), which doesn't support Linux special file permissions (setuid, device nodes, etc.). PAM packages require these permissions.

## Changes Made

1. **`_template/scripts/build-all.sh`**
   - Moved ROOTFS, CACHE, ISO_DIR to `/tmp/gnuchan-build/` (WSL native ext4)
   - Added DrvFs detection warning
   - `run_script()` now passes env vars explicitly (works through sudo)
   - Removed `export TMPDIR` (leaked into chroot, breaking ca-certificates)
   - Post-build: copies ISO back to `_template/iso/` on D:\

2. **All 12 scripts** (01-12): Paths now use `${ROOTFS:-$PROJECT_DIR/rootfs}` pattern so they work both from build-all.sh (ext4) and standalone (old D:\ fallback)

3. **`_template/scripts/02-chroot-configure.sh`**
   - Fixed: `locale.gen` not found in minbase → install `locales` first
   - Fixed: chroot apt commands leak TMPDIR → wrap with `bash -c "unset TMPDIR; apt ..."`

4. **`_template/config/packages-required.txt`**
   - Fixed: `libxfont2-dev` → `libxfont-dev` (renamed in Debian)
   - Fixed: `libinput` → `libinput10` (package name)
   - Removed: `wine32` (not available on amd64, replaced by `libwine`)

## Verified

- ✅ debootstrap completes successfully on native ext4 at `/tmp/gnuchan-build/`
- ✅ Steps 01-07 run clean (tested with `--skip-kernel --skip-liberated --skip-xlibre --skip-qtile`)
- ✅ Package installation fix confirmed

## Next Steps

- [ ] Run full `build.bat build` to produce ISO
- [ ] Test ISO in VirtualBox
