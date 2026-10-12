#!/bin/sh
# =============================================================================
# gpu_menu.sh - give the ISO's boot menus one entry per graphics driver.
#
# Each extra entry is a COPY of the ISO's own first entry with one thing
# changed: a `gnuchan.gpu=` word appended to its kernel command line. The live
# system reads that word from /proc/cmdline while it boots and brings up the
# matching driver.
#
#     Live System -> NVIDIA (new)     gnuchan.gpu=nvidia-new
#     Live System -> NVIDIA (legacy)  gnuchan.gpu=nvidia-legacy
#     Live System -> Radeon           gnuchan.gpu=amd
#     Live System -> Intel            gnuchan.gpu=intel
#
# WHY THIS IS A SHELL SCRIPT AND NOT A PYTHON ONE. It was a .py, run by the
# 0080-boot-branding binary hook with `python3`. The hook runs on the BUILD
# HOST - not inside the chroot - and the build is driven from a minimal
# debian:trixie container whose only package is live-build, which does NOT
# depend on python3. So `command -v python3` failed there, the hook quietly
# added nothing, and the finished ISO came out with the distribution's own
# single entry and no graphics choice at all. A menu editor has no business
# dragging an interpreter onto the build host: sh and awk are on every system
# that can run live-build in the first place. This does the same edit with awk.
#
# It edits, in order and each only if it is there:
#
#     <root>/isolinux/live.cfg   (or the first *.cfg that has label+append)
#     <root>/boot/grub/grub.cfg
#
# A menu that already names gnuchan.gpu is left alone, so running this twice is
# the same as running it once.
#
# License: GPL3
# =============================================================================
set -eu

log() { printf '==> gpu-menu: %s\n' "$1"; }

# The awk program does the whole edit in one pass. It reads the file, decides
# from its own content whether it is an ISOLINUX menu (blocks opened by `label`)
# or a GRUB one (blocks opened by `menuentry`), and clones the FIRST block once
# per driver choice with the title and the kernel word swapped.
GPU_MENU_AWK='
BEGIN {
    titles[1] = "Live System -> NVIDIA (new)";    params[1] = "gnuchan.gpu=nvidia-new"
    titles[2] = "Live System -> NVIDIA (legacy)"; params[2] = "gnuchan.gpu=nvidia-legacy"
    titles[3] = "Live System -> Radeon";          params[3] = "gnuchan.gpu=amd"
    titles[4] = "Live System -> Intel";           params[4] = "gnuchan.gpu=intel"
    choices = 4
}
{ lines[NR] = $0 }
END {
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*menuentry([ \t]|$)/) { grub(); exit 0 }
    }
    isolinux()
    exit 0
}

# ISOLINUX: a block starts at a `label <name>` line and runs to the next one.
function isolinux(   first, last, i, c, line, indent, body, m) {
    first = 0
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*label[ \t]+[^ \t]/) { first = i; break }
    }
    if (first == 0) { for (i = 1; i <= NR; i++) print lines[i]; return }

    last = NR
    for (i = first + 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*label[ \t]+[^ \t]/) { last = i - 1; break }
    }

    for (i = 1; i < first; i++) print lines[i]
    for (i = first; i <= last; i++) print lines[i]

    for (c = 1; c <= choices; c++) {
        for (i = first; i <= last; i++) {
            line = lines[i]

            # `menu default` marks the ORIGINAL entry as the one Enter picks; a
            # clone that carried it too would leave several defaults, which is
            # an invalid menu. Dropped.
            if (line ~ /^[ \t]*menu default[ \t]*$/) continue

            if (line ~ /^[ \t]*label[ \t]+[^ \t]/) {
                m = match(line, /^[ \t]*label[ \t]+[^ \t]+/)
                line = substr(line, 1, m + RLENGTH - 1) "-gpu" c substr(line, m + RLENGTH)
            } else if (line ~ /^[ \t]*menu label[ \t]/) {
                indent = line; sub(/[^ \t].*$/, "", indent)
                line = indent "menu label " titles[c]
            } else if (line ~ /^[ \t]*append[ \t]/) {
                indent = line; sub(/[^ \t].*$/, "", indent)
                body = line
                sub(/^[ \t]*append[ \t]+/, "", body)
                sub(/[ \t\r]+$/, "", body)
                line = indent "append " body " " params[c]
            }
            print line
        }
    }
    for (i = last + 1; i <= NR; i++) print lines[i]
}

# GRUB: a block is a `menuentry` and everything up to its closing brace. The
# braces are counted because they nest; a menuentry that only added its own
# opening brace would stop at the first inner `}`.
function grub(   first, last, i, j, c, line, body, m, open, depth, probe) {
    first = 0
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*menuentry([ \t]|$)/) { first = i; break }
    }
    if (first == 0) { for (i = 1; i <= NR; i++) print lines[i]; return }

    depth = 0; open = 0; last = first
    for (j = first; j <= NR; j++) {
        probe = lines[j]; depth += gsub(/\{/, "{", probe)
        if (lines[j] ~ /\{/) open = 1
        probe = lines[j]; depth -= gsub(/\}/, "}", probe)
        last = j
        if (open && depth <= 0) break
    }

    for (i = 1; i <= last; i++) print lines[i]

    for (c = 1; c <= choices; c++) {
        for (i = first; i <= last; i++) {
            line = lines[i]
            if (line ~ /^[ \t]*menuentry([ \t]|$)/) {
                m = match(line, /"[^"]*"/)
                if (m > 0) {
                    line = substr(line, 1, m - 1) "\"" titles[c] "\"" substr(line, m + RLENGTH)
                }
            } else if (line ~ /^[ \t]*(linux|linuxefi)[ \t]/) {
                body = line; sub(/[ \t\r]+$/, "", body)
                line = body " " params[c]
            }
            print line
        }
    }
    for (i = last + 1; i <= NR; i++) print lines[i]
}
'

# Edit one file in place, unless it already names a graphics word.
edit_menu() {
    file=$1
    [ -f "$file" ] || return 0
    if grep -q 'gnuchan.gpu=' "$file" 2>/dev/null; then
        return 0
    fi
    if awk "$GPU_MENU_AWK" "$file" > "$file.gpu-menu.tmp"; then
        mv "$file.gpu-menu.tmp" "$file"
        log "added the graphics driver entries to $file"
    else
        rm -f "$file.gpu-menu.tmp"
        log "could not edit $file; it is left as it is"
    fi
}

# The ISOLINUX menu: live-build's live.cfg, or the first .cfg that has a label
# and an append - the two lines this edits.
find_isolinux() {
    dir=$1/isolinux
    [ -d "$dir" ] || return 1
    if [ -f "$dir/live.cfg" ]; then
        printf '%s\n' "$dir/live.cfg"
        return 0
    fi
    for file in "$dir"/*.cfg; do
        [ -f "$file" ] || continue
        if grep -q 'label' "$file" 2>/dev/null && grep -q 'append' "$file" 2>/dev/null; then
            printf '%s\n' "$file"
            return 0
        fi
    done
    return 1
}

root=${1:-.}

if isolinux=$(find_isolinux "$root"); then
    edit_menu "$isolinux"
else
    log "no ISOLINUX configuration found; the BIOS menu is left as it is"
fi

grub="$root/boot/grub/grub.cfg"
if [ -f "$grub" ]; then
    edit_menu "$grub"
else
    log "no GRUB configuration found; the EFI menu is left as it is"
fi

exit 0
