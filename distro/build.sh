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
# The source the image is built FROM - gnuchan_softwares/, dotfile/, language/,
# book/ and assets/ - is staged into the overlay below, and the last hook
# removes it again, so the finished ISO carries the installed programs and not
# the tree they were built from.
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

# --- checks -----------------------------------------------------------------

[ "$(id -u)" -eq 0 ] || die "this builds a system image; run it as root"

command -v apt-get >/dev/null 2>&1 || die "this builds a Debian image and needs apt"

if ! command -v lb >/dev/null 2>&1; then
    say "Installing live-build"
    apt-get update
    apt-get install -y --no-install-recommends live-build
fi

[ -d "$HERE/config/hooks" ] || die "no config/hooks beside this script"

for tree in gnuchan_softwares dotfile language; do
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
       -o -path '*/hooks/*' -o -path '*/package-lists/*' \) \
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
cp -a "$OS_ROOT/gnuchan_softwares" "$STAGE/"
cp -a "$OS_ROOT/dotfile" "$STAGE/"
# The GCL language is a program of its own with its own build chain
# (language/makefile.py). It is built and installed by a hook so the live image
# carries the `gcl` command and the IDE, the way an installed machine does. The
# source goes in whole: the build reads language/_SRC and language/tests, and
# the book is documentation the image ships.
cp -a "$OS_ROOT/language" "$STAGE/"
[ -d "$OS_ROOT/book" ] && cp -a "$OS_ROOT/book" "$STAGE/"
# The themes are rendered from assets/bg.png and assets/logo.png, and the GCL
# build embeds assets/icon.png; the installers look for assets/ a couple of
# directories up from their own file, so the repository root - assets/
# included - is what has to be beside the sources.
if [ -d "$OS_ROOT/assets" ]; then
    cp -a "$OS_ROOT/assets" "$STAGE/"
fi
note "staged $STAGE"

# --- the build --------------------------------------------------------------

cd "$WORK"
say "Configuring live-build ($DISTRIBUTION/$ARCHITECTURE)"
lb config \
    --distribution "$DISTRIBUTION" \
    --architectures "$ARCHITECTURE" \
    --archive-areas "main contrib non-free non-free-firmware" \
    --binary-images iso-hybrid \
    --bootappend-live "boot=live components username=gnuchanos hostname=gnuchan quiet splash" \
    --apt-recommends false \
    --debootstrap-options "--variant=minbase" \
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
