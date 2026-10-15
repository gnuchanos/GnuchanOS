#!/bin/sh
# GnuchanOS - build the live ISO.
#
#     sudo ./build.sh
#
# Runs live-build over the configuration tree in distro/config and leaves the
# image in distro/_build/. It must run on Debian with live-build installed; it
# installs live-build with apt once if it is missing.
#
# The build is OUT OF TREE. `lb config` writes its generated files into the
# directory it runs in - config/binary, config/chroot, the .conf files, the
# whole working tree - and running it here would put every one of them beside
# the authored configuration, mixed in with the files that ARE the
# configuration. So the authored tree is copied into distro/_build/work/, and
# live-build runs there: the source tree is touched by nothing, and cleaning up
# is deleting one directory.
#
# There is deliberately NO auto/config here. live-build looks for auto/ in its
# RUNNING directory and, when it finds one, runs it and then RE-RUNS the same
# command with a first argument of "noauto" - which is a mechanism for a
# configuration that needs to do work before lb config, and one this build
# does not need. Doing the two steps here - staging, then `lb config`, then
# `lb build` - is the same thing written once and with no redirection to get
# wrong: an auto/build that calls `lb build` on top of a script that calls
# `lb build` is a recursion, and the way to not have it is to not have the
# auto script.
#
# The source the image is built FROM - dotfile/, book/ and assets/, plus
# GnuChanPrograms.sh - is staged into the overlay below, and the last hook
# removes it again, so the finished ISO carries the installed programs and not
# the trees they were built from. The programs and the GCL language are NOT
# staged: they live in their own repositories and GnuChanPrograms.sh fetches
# them into the image while it builds.
#
# License: GPL3.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
OS_ROOT=$(cd "$HERE/.." && pwd)
OUTPUT="$HERE/_build"
WORK="$OUTPUT/work"
STAGE="$WORK/config/includes.chroot/opt/gnuchanos"

# The distribution the image is built on. Trixie is Debian stable and is what
# the programs are developed against.
DISTRIBUTION=trixie
ARCHITECTURE=amd64

say() { printf '==> %s\n' "$1"; }
note() { printf '    %s\n' "$1"; }
die() { printf '  ! %s\n' "$1" >&2; exit 1; }

copy_iso_back() {
    source_iso=$1
    destination_iso=$2
    temporary_iso="${destination_iso}.tmp.$$"

    if ! dd if="$source_iso" of="$temporary_iso" bs=4M conv=fsync status=progress; then
        die "ISO copy failed; complete image remains at $source_iso (partial copy: $temporary_iso)"
    fi
    if ! cmp -s "$source_iso" "$temporary_iso"; then
        die "ISO copy verification failed; complete image remains at $source_iso (copy: $temporary_iso)"
    fi
    mv -f "$temporary_iso" "$destination_iso" ||
        die "could not finalize ISO at $destination_iso; complete image remains at $source_iso"
}

# --- checks -----------------------------------------------------------------

[ "$(id -u)" -eq 0 ] || die "this builds a system image; run it as root"

# --- build on a Linux filesystem ---------------------------------------------
#
# live-build builds a whole Debian system, and debootstrap cannot build one on a
# Windows filesystem. A build run from /mnt/c fails at the first package with
#
#     E: Tried to extract package, but tar failed. Exit...
#
# because Windows' filesystem is case-INSENSITIVE - pam.7.gz and PAM.7.gz are
# the same name, so tar cannot create the symlink - and it does not carry the
# file modes or device nodes a Debian root needs. A network or removable mount
# is the same. (WSL itself is FINE: loop devices and squashfs work there, which
# is why the build succeeds once the TREE is on a Linux filesystem.)
#
# So rather than tell the user to move the tree, the tree is copied to a Linux
# filesystem here, the build re-runs there, and the finished ISO is copied back
# beside this script. It is automatic: `sudo sh build.sh` works from wherever it
# was invoked - /mnt/c included.
SOURCE_DISTRO="$HERE"
FS_TYPE=$(stat -f -c %T "$SOURCE_DISTRO" 2>/dev/null || echo unknown)
case "$FS_TYPE" in
    ext2/ext3|ext4|xfs|btrfs|zfs|tmpfs|overlay)
        : # a Linux filesystem: build here, as before
        ;;
    *)
        if [ "${GNUCHANOS_RELOCATED:-0}" = "1" ]; then
            die "the tree is still on '$FS_TYPE' after relocating; set GNUCHANOS_BUILD_DIR to a Linux path"
        fi
        BUILD_PARENT="${GNUCHANOS_BUILD_DIR:-/var/tmp}"
        say "The tree is on '$FS_TYPE', which cannot hold a Debian build"
        BUILD_PARENT=$(realpath -m -- "$BUILD_PARENT") ||
            die "could not resolve GNUCHANOS_BUILD_DIR"
        case "$BUILD_PARENT/" in
            "$OS_ROOT/"*)
                die "GNUCHANOS_BUILD_DIR must be outside the source tree"
                ;;
        esac
        mkdir -p "$BUILD_PARENT"
        BUILD_FS_TYPE=$(stat -f -c %T "$BUILD_PARENT" 2>/dev/null || echo unknown)
        case "$BUILD_FS_TYPE" in
            ext2/ext3|ext4|xfs|btrfs|zfs|tmpfs|overlay) ;;
            *) die "GNUCHANOS_BUILD_DIR is on '$BUILD_FS_TYPE', not a Linux filesystem" ;;
        esac
        RELOCATE=$(mktemp -d "$BUILD_PARENT/gnuchanos-build.XXXXXX")
        say "copying it to $RELOCATE (a Linux filesystem); the ISO is copied back"
        # A unique directory avoids deleting a pre-existing user directory or
        # confusing stale build output with the image produced by this run.
        tar -C "$OS_ROOT" --exclude=./distro/_build --exclude=./.git -cf - . \
            | tar -C "$RELOCATE" -xf -
        if [ -d "$OS_ROOT/../GnuChanDM/src" ] &&
           [ -f "$OS_ROOT/../GnuChanDM/src/makefile.py" ] &&
           [ ! -d "$RELOCATE/distro/repos/GnuChanDM" ]; then
            mkdir -p "$RELOCATE/distro/repos"
            cp -a "$OS_ROOT/../GnuChanDM" "$RELOCATE/distro/repos/GnuChanDM"
            rm -rf "$RELOCATE/distro/repos/GnuChanDM/.git"
            note "staged the sibling GnuChanDM checkout for the relocated build"
        fi
        # Re-run in the copy. The guard variable stops a second relocation if
        # the copy somehow landed on a filesystem that still is not Linux.
        GNUCHANOS_RELOCATED=1 sh "$RELOCATE/distro/build.sh" || exit $?
        # Put the finished ISO where the person running this expects it. Stream
        # it in bounded chunks to avoid large-file copy allocation failures on
        # WSL's Windows filesystem, verify it, then replace any partial old ISO.
        set -- "$RELOCATE/distro/_build/"*.iso
        [ -f "$1" ] || die "build completed but produced no ISO in $RELOCATE/distro/_build"
        mkdir -p "$SOURCE_DISTRO/_build"
        for source_iso do
            destination_iso="$SOURCE_DISTRO/_build/$(basename "$source_iso")"
            say "Copying $(basename "$source_iso") back to the Windows filesystem"
            copy_iso_back "$source_iso" "$destination_iso"
        done
        say "ISO copied and verified in $SOURCE_DISTRO/_build/"
        note "the build tree was kept at $RELOCATE"
        exit 0
        ;;
esac

command -v apt-get >/dev/null 2>&1 || die "this builds a Debian image and needs apt"

# LIVE-BUILD AND RSYNC ARE INSTALLED TOGETHER, AND RSYNC IS THE ONE THAT KEEPS
# BEING MISSED.
#
# rsync is only a RECOMMENDS of live-build, so --no-install-recommends drops it
# - and live-build USES it: Chroot_copy_dir() in its own chroot.sh copies the
# staged overlay into the chroot with `rsync`, and without rsync it falls back
# to writing a tarball, entering the chroot and unpacking it byte by byte for
# EVERY directory it stages. That is the "a build that looks hung" slow path.
#
# SO WHY BOTH CHECKS. The two are independent: a machine can already have
# live-build (the first `command -v lb` is then skipped) while rsync is still
# missing - which is exactly the state a machine left by an earlier run of this
# script is in, because earlier it installed only live-build. Installing rsync
# on its own line is what catches that state; the apt that may have just run is
# not repeated, because apt-get install is idempotent.
#
# AND IT GOES ON THE IMAGE TOO. GnuChanPrograms.sh and the hooks that run INSIDE
# the chroot call rsync in places (the program build helpers), and a chroot with
# no rsync reports
#
#     rsync failed with error code 127
#
# which is rsync simply not being there. rsync is named in the package list
# (GnuchanOS.list.chroot) for that reason, so the chroot has it; this block is
# the HOST side, so the build machine that drives lb has it as well. Neither is
# optional and neither is a substitute for the other.
if ! command -v rsync >/dev/null 2>&1; then
    say "Installing rsync on the build host (live-build copies its overlay"
    say "  with it; the tarball fallback without it is why a build crawls)"
    apt-get update
    apt-get install -y --no-install-recommends rsync
fi

if ! command -v lb >/dev/null 2>&1; then
    say "Installing live-build"
    apt-get update
    apt-get install -y --no-install-recommends live-build rsync
fi

[ -d "$HERE/config/hooks" ] || die "no config/hooks beside this script"

# Only dotfile/ is required. The programs AND the GCL language live in their own
# repositories now (see GnuChanPrograms.sh) and are fetched or copied at build
# time, so gnuchan_softwares/ and language/ are not part of this tree any more
# and must not be required of it.
for tree in dotfile; do
    [ -d "$OS_ROOT/$tree" ] || die "missing source tree: $OS_ROOT/$tree"
done

# --- a clean working copy ----------------------------------------------------

# The previous run's working tree is not reused: `lb config` on top of one
# gives a mixture of the old and the new configuration, which is the classic
# way to get an image that does not match the tree it was built from.
say "Preparing the working copy"
rm -rf "$WORK"
mkdir -p "$WORK"
cp -a "$HERE/config" "$WORK/config"

# The tree is edited on Windows as often as on Linux, and a Windows checkout
# carries CRLF line endings unless git is told otherwise. They are fatal here
# and silent: a package list whose every line ends in "\r" makes apt look for a
# package literally named "systemd\r" and report "Unable to locate" for every
# line, and a hook whose shebang is "#!/bin/sh\r" is a script the kernel cannot
# run. Both are converted to LF in the working copy, so the build does not
# depend on the checkout having been made with the right git configuration.
# (.gitattributes pins them to LF for FUTURE checkouts; this makes the CURRENT
# one work.)
#
# sed strips a trailing CR wherever it finds one and is a no-op where there is
# none, so this is run over every text file of the tree rather than testing each
# one first. It is written as find -exec and not as a `while read` loop because
# this script is run by /bin/sh, which on Debian is dash: dash's read has no -d,
# and a loop using it fails on the first file and takes the cleanup with it.
find "$WORK/config" -type f \
    \( -name '*.chroot' -o -name '*.list.chroot' -o -name '*.conf' \
       -o -name '*.sh' -o -name '*.py' -o -name '*.qss' -o -name '*.qml' \
       -o -name '*.desc' -o -path '*/hooks/*' -o -path '*/package-lists/*' \) \
    -exec sed -i 's/\r$//' {} + 2>/dev/null || true

# The hooks are EXECUTED, so they have to carry the execute bit. It is set here
# rather than trusted to the checkout: the file mode git stores is not carried
# by every filesystem this tree is edited on (a Windows working copy has no
# execute bit at all), so a hook that is executable in the repository can
# arrive in the working copy as a plain file and make the build stop with
# "Permission denied" on the first one.
#
# The hooks live under config/hooks/normal/, not directly under config/hooks/:
# live-build scans exactly two subdirectories - normal/ and live/ - and runs
# every hook in them, so a hook beside them rather than inside one is a hook
# live-build never sees. The glob names the subdirectory because of that.
#
# BOTH suffixes are set executable, and both matter. A .hook.chroot runs inside
# the live filesystem while it is being built; a .hook.binary runs against the
# finished image tree, which is where the boot menu and the ISO's own files are
# - the boot branding hook (0080-boot-branding.hook.binary) is one. Setting
# only .hook.chroot left every binary hook a plain file, and live-build skips a
# hook it cannot execute.
#
# The bare `.hook` suffix is set as well, for an older live-build that named
# them that way. `2>/dev/null || true` is there because a glob that matches
# nothing makes the shell pass the pattern through literally, and chmod would
# then fail on a file that does not exist.
chmod +x "$WORK"/config/hooks/normal/*.hook 2>/dev/null || true
chmod +x "$WORK"/config/hooks/normal/*.hook.chroot 2>/dev/null || true
chmod +x "$WORK"/config/hooks/normal/*.hook.binary 2>/dev/null || true

# --- stage the source the hooks build from -----------------------------------

# Copied, not linked: live-build copies includes.chroot/ into the image, and a
# link would point at a path that does not exist inside it. The last hook
# removes /opt/gnuchanos again, so the finished ISO carries the installed
# programs and not their sources.
say "Staging the source trees into the overlay"
rm -rf "$STAGE"
mkdir -p "$STAGE"
# The programs are their own repositories now (see GnuChanPrograms.sh) and are
# not part of this tree. A checkout of them that IS here - the repository as it
# was before they moved, or a developer who cloned them back in - is staged so
# the build uses it instead of the network. It is not required.
if [ -d "$OS_ROOT/gnuchan_softwares" ]; then
    cp -a "$OS_ROOT/gnuchan_softwares" "$STAGE/"
else
    note "no gnuchan_softwares/ in the tree; the programs will be fetched"
fi

# Local checkouts of the PROGRAM repositories, in the place GnuChanPrograms.sh
# looks FIRST. That script's $SRC defaults to /opt/gnuchanos/repos - beside the
# trees staged here - and a checkout it finds there is copied into the build
# instead of being cloned. So a clone placed at distro/repos/<Name> is staged to
# /opt/gnuchanos/repos/<Name> and is used in preference to the network.
#
# This is what lets a build of programs held in PRIVATE repositories work at
# all: the fetch needs a credential, and the one place a credential exists is
# the thing doing the fetching - the CI workflow or a developer's own clone,
# neither of which the build inside the image has. Fetch once where the token
# is, drop the checkouts here, and the image build needs no network to github.
#
# The directory name is the PROGRAM's name (GCL, GnuChanWM, ...), not the
# repository's: GnuChanPrograms.sh keys $SRC by the program name, and the GCL
# language's repository is `gcl` while its directory there is `GCL`. A build
# that is run with no token and no checkouts still works - the script falls
# back to cloning - so this is an optimisation that makes the private case
# possible, not a new requirement (except where the repositories are private,
# which is a requirement the fetch ALWAYS had).
if [ -d "$HERE/repos" ]; then
    cp -a "$HERE/repos" "$STAGE/"
    note "staged local program checkouts from $HERE/repos"
fi
# Use the sibling checkout when present so local DM fixes are built into this
# ISO instead of silently replaced by the remote default branch.
DM_CHECKOUT="$OS_ROOT/../GnuChanDM"
if [ -d "$DM_CHECKOUT/src" ] && [ -f "$DM_CHECKOUT/src/makefile.py" ] &&
   [ ! -d "$STAGE/repos/GnuChanDM" ]; then
    mkdir -p "$STAGE/repos"
    cp -a "$DM_CHECKOUT" "$STAGE/repos/GnuChanDM"
    rm -rf "$STAGE/repos/GnuChanDM/.git"
    note "staged local GnuChanDM source from $DM_CHECKOUT"
fi
# The script that fetches and installs the programs is this tree's own and is
# staged with everything else: the hook that runs it looks for it under
# /opt/gnuchanos, beside the trees it installs from.
cp -a "$HERE/GnuChanPrograms.sh" "$STAGE/"
# The installer is this tree's own and is staged the same way, whole: its X11
# window (installer/src/, built by its own makefile.py) and the shell script it
# runs to do the disk work (installer/gnuchanos-install.sh). The 0450-installer
# hook builds and installs both. It is a program of this repository, not a
# package, so it is staged here and not fetched.
cp -a "$HERE/installer" "$STAGE/"
cp -a "$OS_ROOT/dotfile" "$STAGE/"
# The GCL language is one of the fetched repositories now (see
# GnuChanPrograms.sh, which carries it in the same list as the programs) - it is
# not a tree of this repository any more, so it is not staged from here.
# The book is this tree's own documentation for the language and is still
# shipped; the cleanup hook keeps it at /usr/share/gnuchanos/book.
[ -d "$OS_ROOT/book" ] && cp -a "$OS_ROOT/book" "$STAGE/"
# The pictures the programs and the themes are built from. They belong to this
# repository - assets/bg.png, assets/logo.png, assets/icon.png - and
# GnuChanPrograms.sh copies them into the directory it clones the repositories
# into, because a program's makefile reaches assets/ through the common parent
# it calls REPO_ROOT. The themes read them from here as well.
if [ -d "$OS_ROOT/assets" ]; then
    cp -a "$OS_ROOT/assets" "$STAGE/"
fi

# The overlay above carries files that are EXECUTED inside the image, and the
# most important of them is GnuChanPrograms.sh - the 0300 hook runs it to fetch
# and build every Gnuchan program. That is the whole reason a second CRLF pass
# is needed here, and it is not belt-and-braces: the pass earlier in this
# script normalises $WORK/config, and these files are staged INTO that tree
# afterwards, so that pass runs over a directory they are not in yet.
#
# What a CRLF copy of GnuChanPrograms.sh does inside the chroot is fail before
# it fetches anything. dash reads its "set -u" line as the option "-u\r" and
# answers
#
#     set: Illegal option -
#
# and the script exits at once, so the image ships with NO Gnuchan program in
# it at all - an ISO that boots to a bare X server with no window manager, no
# terminal and no greeter, which is indistinguishable from "the programs do
# not work" when the truth is "nothing ever ran". The same sed as the config/
# pass, applied to the files that pass does not reach.
find "$STAGE" -type f \
    \( -name '*.chroot' -o -name '*.list.chroot' -o -name '*.conf' \
       -o -name '*.sh' -o -name '*.py' -o -name '*.qss' -o -name '*.qml' \
       -o -name '*.desc' -o -path '*/hooks/*' -o -path '*/package-lists/*' \) \
    -exec sed -i 's/\r$//' {} + 2>/dev/null || true

note "staged $STAGE"

# --- the build --------------------------------------------------------------

# THE DEBOOTSTRAP VARIANT IS LEFT AT ITS DEFAULT ON PURPOSE.
#
# An earlier version passed --variant=minbase to shrink the image, and that one
# flag is what broke the network. debootstrap's minbase variant installs only
# ESSENTIAL packages: the whole Priority: important layer is left out, and that
# layer is where the base networking tools live - iproute2 (`ip`), iputils-ping
# (`ping`), and the rest of what a machine needs before anything above it can
# use the network. The result was an image with no `ip` at all, and a wireless
# connection that associated and took a lease but could not pass traffic,
# because the tools that put an address and a route on the interface were not
# there. (--apt-recommends false does NOT cause this and is kept: iproute2 and
# iputils-ping are Priority: important, not Recommends, so what dropped them was
# the variant and not the recommends.)
#
# The default variant costs a little image size and is the only one that carries
# the standard base. It is left alone here, and the tools that matter to the
# Gnuchan programs are ALSO named in the package list, so the image does not
# depend on the variant staying standard.
cd "$WORK"
say "Configuring live-build ($DISTRIBUTION/$ARCHITECTURE)"
lb config \
    --distribution "$DISTRIBUTION" \
    --architectures "$ARCHITECTURE" \
    --archive-areas "main contrib non-free non-free-firmware" \
    --binary-images iso-hybrid \
    --bootappend-live "boot=live components username=gnuchanos hostname=gnuchan quiet splash" \
    --apt-recommends false \
    --mirror-bootstrap "http://deb.debian.org/debian" \
    --mirror-chroot "http://deb.debian.org/debian" \
    --mirror-binary "http://deb.debian.org/debian" \
    --iso-application "GnuchanOS" \
    --iso-publisher "GnuchanOS" \
    --iso-volume "GnuchanOS"

say "Building the ISO (this takes a long while)"
lb build

IMAGE=$(find "$WORK" -maxdepth 1 -name 'live-image-*.hybrid.iso' | head -n 1)
if [ -z "$IMAGE" ]; then
    die "the build finished but no image was produced"
fi
cp "$IMAGE" "$OUTPUT/GnuchanOS-$DISTRIBUTION-$ARCHITECTURE.iso"

say "Done"
note "image: $OUTPUT/GnuchanOS-$DISTRIBUTION-$ARCHITECTURE.iso"
note "The working tree is $WORK and can be removed with: rm -rf $OUTPUT"
