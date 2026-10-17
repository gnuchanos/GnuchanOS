#!/bin/bash
# GnuchanOS installer - put the running live system onto a disk.
#
# --- WHAT THIS IS, AND WHY IT IS NOT CALAMARES ------------------------------
#
# The image carries a whole desktop, all the Gnuchan programs and a compiled
# language; installing it is four things - make partitions, copy the filesystem,
# write a boot loader, make an account. Calamares does all four and a great deal
# more, and its "more" is what this image kept tripping over: a Python module
# that reads a key its config did not set and dies, a boot loader install whose
# real error is written to a log file instead of the screen, a display manager
# list that does not know GnuChanDM. Every one of those is a place the install
# stops with a message that does not say why.
#
# This script does the same four things directly, as shell, in this order, and
# every command it runs prints what it prints to THIS terminal. When a step
# fails the reason is right here on the screen - not in a log nobody reads - and
# there is no config file between the person and the disk.
#
# It is the step AFTER `unpackfs` in Calamares' sequence, written out: this is
# the honest version of it.
#
# --- WHAT IT DOES -----------------------------------------------------------
#
#   1. collect and validate the account details before touching a disk
#   2. show the disks, ask which one, make the person type ERASE
#   3. partition it (msdos for BIOS, gpt with an EFI partition for UEFI)
#   4. make the filesystems
#   5. mount the root (and /boot/efi on UEFI)
#   6. unsquash the live filesystem onto it
#   7. write /etc/fstab from what was made
#   8. chroot in and install GRUB - with its output SHOWN
#   9. rebuild the initramfs for the installed machine
#  10. make the account, with the configured home
#  11. take the live-only bits back out
#  12. unmount, done
#
# License: GPL3.
set -eu

TARGET=/target
LOG=/var/log/gnuchanos-install.log
NEWUSER=""
FW=""
MOUNTED_TARGET=0
MOUNTED_ESP=0
MOUNTED_DEV=0
MOUNTED_DEV_PTS=0
MOUNTED_PROC=0
MOUNTED_SYS=0
MOUNTED_RUN=0

# Every line of this script, and every line of every command it runs, is copied
# to $LOG as well as shown. The terminal is where a person reads it; the file is
# where it can be read afterwards, from the installed machine or over a photo of
# the screen. `tee` is coreutils and always present.
install -d "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1

step() { printf '\n==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
warn() { printf '\n !  %s\n' "$*"; }
die()  { printf '\n !! %s\n' "$*" >&2; exit 1; }

# --- leave the disk clean whatever happens ----------------------------------
#
# A failed install must not leave the target mounted under /target, or the next
# attempt mounts on top of it and the disk fills with two copies. Unmounting in
# an EXIT trap covers failure and success paths, and any failed unmount makes
# the install report failure rather than claiming completion.
cleanup() {
    failed=0
    for mount in run sys proc dev/pts dev; do
        case "$mount" in
            run) mounted=$MOUNTED_RUN ;;
            sys) mounted=$MOUNTED_SYS ;;
            proc) mounted=$MOUNTED_PROC ;;
            dev/pts) mounted=$MOUNTED_DEV_PTS ;;
            dev) mounted=$MOUNTED_DEV ;;
        esac
        if [ "$mounted" -eq 1 ]; then
            if umount "$TARGET/$mount"; then
                case "$mount" in
                    run) MOUNTED_RUN=0 ;;
                    sys) MOUNTED_SYS=0 ;;
                    proc) MOUNTED_PROC=0 ;;
                    dev/pts) MOUNTED_DEV_PTS=0 ;;
                    dev) MOUNTED_DEV=0 ;;
                esac
            else
                warn "could not unmount $TARGET/$mount"
                failed=1
            fi
        fi
    done
    if [ "$MOUNTED_ESP" -eq 1 ]; then
        if umount "$TARGET/boot/efi"; then MOUNTED_ESP=0
        else warn "could not unmount $TARGET/boot/efi"; failed=1
        fi
    fi
    if [ "$MOUNTED_TARGET" -eq 1 ]; then
        if umount "$TARGET"; then MOUNTED_TARGET=0
        else warn "could not unmount $TARGET"; failed=1
        fi
    fi
    return "$failed"
}

cleanup_on_exit() {
    status=$?
    trap - EXIT
    if ! cleanup && [ "$status" -eq 0 ]; then
        status=1
    fi
    exit "$status"
}
trap cleanup_on_exit EXIT

# --- helpers ----------------------------------------------------------------

# The partition device for a disk and a number. SATA disks name theirs sda1,
# sda2; NVMe and eMMC name theirs nvme0n1p1, and the difference is whether the
# disk name already ends in a digit. Getting this wrong points every later step
# at a device that does not exist.
partdev() {
    case "$1" in
        *[0-9]) printf '%sp%s\n' "$1" "$2" ;;
        *)      printf '%s%s\n' "$1" "$2" ;;
    esac
}

# --- 0. preconditions -------------------------------------------------------

[ "$(id -u)" -eq 0 ] || die "this installs a system; run it as root"

SQUASHFS=""
for p in /run/live/medium/live/filesystem.squashfs \
         /lib/live/mount/medium/live/filesystem.squashfs; do
    if [ -f "$p" ]; then SQUASHFS="$p"; break; fi
done
[ -n "$SQUASHFS" ] || die "could not find the live filesystem (filesystem.squashfs); is this the live system?"

for tool in wipefs sfdisk mkfs.ext4 unsquashfs grub-install grub-mkconfig \
            chroot blkid mount umount getent useradd groupadd chpasswd usermod chown \
            systemctl mountpoint install ssh-keygen; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is missing; this image cannot install itself"
done
[ ! -d "$TARGET" ] || ! mountpoint -q "$TARGET" ||
    die "$TARGET is already mounted; refusing to overwrite an existing install"

if [ -d /sys/firmware/efi ]; then FW=uefi; else FW=bios; fi
if [ "$FW" = "uefi" ]; then
    command -v mkfs.fat >/dev/null 2>&1 ||
        die "mkfs.fat is missing; this image cannot prepare an EFI system partition"
fi
note "firmware: $FW"
note "live filesystem: $SQUASHFS"

# --- 1. collect and validate the account details before erasing anything -----

step "Account details"

NEWUSER="${GNUCHANOS_USER:-}"
if [ -z "$NEWUSER" ]; then
    [ "${GNUCHANOS_YES:-0}" != "1" ] || die "the confirmed GUI run did not provide a login name"
    printf 'Login name (lowercase, no spaces): '
    read -r NEWUSER
fi
[ -n "$NEWUSER" ] || die "a login name is required"
[ "${#NEWUSER}" -le 32 ] || die "login names may contain at most 32 characters"
case "$NEWUSER" in
    [a-z_]*) ;;
    *) die "a login name must start with a lowercase letter or _" ;;
esac
case "$NEWUSER" in
    *[!a-z0-9_-]*) die "use only lowercase letters, digits, _ and -" ;;
esac
if [ "$NEWUSER" != "gnuchanos" ] &&
   getent passwd "$NEWUSER" >/dev/null 2>&1; then
    die "the login name $NEWUSER is already used by a system account"
fi

FULLNAME="${GNUCHANOS_FULLNAME:-}"
if [ -z "$FULLNAME" ] && [ "${GNUCHANOS_YES:-0}" != "1" ]; then
    printf 'Full name (optional): '
    read -r FULLNAME
fi
[ -n "$FULLNAME" ] || FULLNAME="$NEWUSER"
[ "${#FULLNAME}" -le 255 ] || die "the full name is too long"
case "$FULLNAME" in
    *:*|*$'\n'*|*$'\r'*) die "the full name cannot contain colons or line breaks" ;;
esac

PASS1="${GNUCHANOS_PASSWORD:-}"
if [ -z "$PASS1" ]; then
    [ "${GNUCHANOS_YES:-0}" != "1" ] || die "the confirmed GUI run did not provide a password"
    while :; do
        printf 'Password: '
        read -rs PASS1; echo
        printf 'Password again: '
        read -rs PASS2; echo
        [ -n "$PASS1" ] || { warn "empty password"; continue; }
        [ "$PASS1" = "$PASS2" ] && break
        warn "the passwords did not match"
    done
fi
[ -n "$PASS1" ] || die "an empty password is not allowed"
[ "${#PASS1}" -le 255 ] || die "passwords may contain at most 255 characters"
case "$PASS1" in
    *$'\n'*|*$'\r'*) die "passwords cannot contain line breaks" ;;
esac

HOSTNAME="${GNUCHANOS_HOSTNAME:-}"
if [ -z "$HOSTNAME" ] && [ "${GNUCHANOS_YES:-0}" != "1" ]; then
    printf 'Hostname [gnuchan; letters, digits and hyphens]: '
    read -r HOSTNAME
fi
[ -n "$HOSTNAME" ] || HOSTNAME=gnuchan
[ "${#HOSTNAME}" -le 63 ] || die "the hostname may contain at most 63 characters"
case "$HOSTNAME" in
    [A-Za-z0-9]* ) ;;
    *) die "the hostname must start with a letter or digit" ;;
esac
case "$HOSTNAME" in
    *[!A-Za-z0-9-]*|*-) die "use letters, digits and hyphens in the hostname" ;;
esac

# --- 2. choose the disk -----------------------------------------------------

step "Disks on this machine"
lsblk -dno NAME,SIZE,TYPE,MODEL 2>/dev/null | awk '$3 == "disk"'

# The answers may be given BEFORE the run, by the GUI, as environment
# variables; when they are, nothing is asked and the script runs unattended.
# GNUCHANOS_DISK, GNUCHANOS_USER, GNUCHANOS_FULLNAME, GNUCHANOS_PASSWORD,
# GNUCHANOS_HOSTNAME, and GNUCHANOS_YES=1 only after the GUI's explicit ERASE
# confirmation. The GUI supplies defaults for optional fields; it never leaves
# a prompt waiting on an absent terminal.
disk="${GNUCHANOS_DISK:-}"
if [ -z "$disk" ]; then
    printf '\nType the disk to install onto (for example /dev/sda), or q to quit: '
    read -r disk
    [ "$disk" = "q" ] && exit 0
fi
[ -n "$disk" ] && [ -b "$disk" ] || die "$disk is not a block device"

# --- 3. confirm, and make it explicit ---------------------------------------

warn "EVERYTHING on $disk will be erased. This cannot be undone."
if [ "${GNUCHANOS_YES:-0}" = "1" ]; then
    note "confirmed before the run (the GUI asked); continuing"
else
    printf 'Type ERASE (in capitals) to continue: '
    read -r confirm
    [ "$confirm" = "ERASE" ] || die "not confirmed; nothing was written"
fi

# --- 4. partition -----------------------------------------------------------
#
# BIOS gets an MBR table and one Linux partition that starts at the first
# aligned sector, which leaves the gap between the MBR and the partition that
# GRUB's first stage embeds into - the ~1MiB it needs on a BIOS machine and the
# thing whose absence makes grub-install answer "embedding is not possible".
#
# UEFI gets a GPT table, a 512MiB EFI system partition (FAT32, the EFI spec's
# own requirement) and the rest for root.
step "Partitioning $disk for $FW"

wipefs -a "$disk"

if [ "$FW" = "uefi" ]; then
    sfdisk "$disk" <<'SFDISK'
label: gpt
,512MiB,U
,,L
SFDISK
    ESP_PART="$(partdev "$disk" 1)"
    ROOT_PART="$(partdev "$disk" 2)"
else
    sfdisk "$disk" <<'SFDISK'
label: dos
,,83,*
SFDISK
    ESP_PART=""
    ROOT_PART="$(partdev "$disk" 1)"
fi

# The kernel does not always know about the new partitions the instant sfdisk
# returns: the table is written but the device nodes appear a moment later, and
# on some kernels only after being told to re-read. This waits for the root
# partition to exist before anything is written to it, so mkfs cannot run on a
# device that is not there yet - which is the kind of failure that looks like a
# bad disk and is really a race.
partprobe "$disk" 2>/dev/null || true
udevadm settle 2>/dev/null || true
_tries=0
while [ ! -b "$ROOT_PART" ] && [ "$_tries" -lt 20 ]; do
    partprobe "$disk" 2>/dev/null || true
    udevadm settle 2>/dev/null || true
    sleep 0.5
    _tries=$((_tries + 1))
done

[ -b "$ROOT_PART" ] || die "the root partition $ROOT_PART never appeared; the kernel did not read the new table"
note "root partition: $ROOT_PART"
if [ -n "$ESP_PART" ]; then
    [ -b "$ESP_PART" ] || die "the EFI partition $ESP_PART never appeared"
    note "EFI partition:  $ESP_PART"
fi

# --- 5. filesystems ---------------------------------------------------------

step "Making filesystems"
if [ -n "$ESP_PART" ]; then
    mkfs.fat -F32 -n EFI "$ESP_PART"
fi
mkfs.ext4 -F -L GnuchanOS "$ROOT_PART"

# --- 6. mount ---------------------------------------------------------------

step "Mounting the target at $TARGET"
mkdir -p "$TARGET"
mount "$ROOT_PART" "$TARGET"
MOUNTED_TARGET=1

if [ -n "$ESP_PART" ]; then
    mkdir -p "$TARGET/boot/efi"
    mount "$ESP_PART" "$TARGET/boot/efi"
    MOUNTED_ESP=1
fi

# --- 7. the live system onto the disk ---------------------------------------
#
# THE HEART OF IT. The squashfs the live session runs from is unsquashed onto
# the new root, so the installed machine is the live one byte for byte: every
# program that was compiled into the image, every theme, every setting. Nothing
# is reinstalled and nothing is rebuilt. `-f` overwrites the lost+found mkfs
# left, which is the only thing on the new filesystem.
step "Copying the live system onto $TARGET (this takes a few minutes)"
unsquashfs -f -d "$TARGET" "$SQUASHFS"

if [ ! -e "$TARGET/etc/os-release" ]; then
    die "the copy does not look like a system (no /etc/os-release); stopping"
fi
note "copied"

# --- 8. fstab, and the machine's own name -----------------------------------

step "Writing /etc/fstab"

ROOT_UUID="$(blkid -s UUID -o value "$ROOT_PART")"
{
    printf '# GnuchanOS - written by the installer.\n'
    printf 'UUID=%s / ext4 errors=remount-ro 0 1\n' "$ROOT_UUID"
    if [ -n "$ESP_PART" ]; then
        ESP_UUID="$(blkid -s UUID -o value "$ESP_PART")"
        printf 'UUID=%s /boot/efi vfat umask=0077 0 1\n' "$ESP_UUID"
    fi
} > "$TARGET/etc/fstab"
note "fstab:"
sed 's/^/      /' "$TARGET/etc/fstab"

# A fresh machine-id, so two machines installed from one image are not the same
# machine to systemd or to the network.
rm -f "$TARGET/etc/machine-id"
printf '%s\n' "$(cat /proc/sys/kernel/random/uuid | tr -d -)" > "$TARGET/etc/machine-id"

# --- 9. GRUB, with its output shown -----------------------------------------
#
# This is the step that decides whether the machine BOOTS, and the step whose
# failure was hardest to read under Calamares: it ran grub-install through a
# wrapper and the real error went to a log file. Here grub-install is run
# directly and everything it prints comes to this terminal, so a failure says
# what is wrong in its own words.
#
# The chroot needs /dev, /proc, /sys and /run: grub-probe reads /proc and /sys
# to work out device names and filesystem types, and without them it fails in
# ways that look like a broken disk.
step "Installing the boot loader"

mount --bind /dev "$TARGET/dev"
MOUNTED_DEV=1
mount --bind /dev/pts "$TARGET/dev/pts"
MOUNTED_DEV_PTS=1
mount --bind /proc "$TARGET/proc"
MOUNTED_PROC=1
mount --bind /sys "$TARGET/sys"
MOUNTED_SYS=1
mount --bind /run "$TARGET/run"
MOUNTED_RUN=1

# /etc/default/grub for grub-mkconfig to read. Keep the live image's theme
# settings before replacing its live-only kernel arguments with the installed
# system's settings.
LIVE_GRUB_DEFAULT="$TARGET/etc/default/grub"
LIVE_GRUB_GFXMODE=""
if [ -f "$LIVE_GRUB_DEFAULT" ]; then
    LIVE_GRUB_GFXMODE=$(sed -n 's/^GRUB_GFXMODE="\([^"]*\)"$/\1/p' "$LIVE_GRUB_DEFAULT")
fi

# The installed system's /etc/default/grub is written ALWAYS, not only
# when absent: the live image's own copy is the ISO's - it names the live boot
# parameters (boot=live, the live medium), and an installed machine booting
# with `boot=live` looks for a squashfs that is not there and stops. What the
# installed machine wants is a plain line with its own root, which the kernel
# adds from the UUID below.
cat > "$TARGET/etc/default/grub" <<'GRUBCFG'
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="GnuChanOS"
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
GRUB_CMDLINE_LINUX=""
GRUB_DISABLE_OS_PROBER=true
GRUBCFG

GRUB_THEME_PATH=/boot/grub/themes/GnuchanOS/theme.txt
[ -f "$TARGET$GRUB_THEME_PATH" ] ||
    die "the GnuChanOS GRUB theme is missing from the installed filesystem"
printf 'GRUB_THEME="%s"\n' "$GRUB_THEME_PATH" >> "$TARGET/etc/default/grub"
if [ -n "$LIVE_GRUB_GFXMODE" ]; then
    case "$LIVE_GRUB_GFXMODE" in
        *[!a-zA-Z0-9xX,._:+-]*)
            warn "ignoring invalid live GRUB_GFXMODE: $LIVE_GRUB_GFXMODE"
            ;;
        *)
            printf 'GRUB_GFXMODE="%s"\n' "$LIVE_GRUB_GFXMODE" >> "$TARGET/etc/default/grub"
            ;;
    esac
fi

if [ "$FW" = "uefi" ]; then
    chroot "$TARGET" grub-install \
        --target=x86_64-efi \
        --efi-directory=/boot/efi \
        --bootloader-id=GnuchanOS \
        --recheck
else
    chroot "$TARGET" grub-install \
        --target=i386-pc \
        --recheck \
        "$disk"
fi

step "Generating /boot/grub/grub.cfg"
chroot "$TARGET" grub-mkconfig -o /boot/grub/grub.cfg
grep -Fq "$GRUB_THEME_PATH" "$TARGET/boot/grub/grub.cfg" ||
    die "GRUB did not enable the GnuChanOS theme in /boot/grub/grub.cfg"

step "Rebuilding the initramfs for this machine"
chroot "$TARGET" update-initramfs -u -k all 2>&1 || warn "initramfs rebuild failed; the live one is still in /boot"

# --- 10. the account --------------------------------------------------------

step "Creating the account you will log in with"

# The live account the image ships (0050-liveuser made it) is taken out of the
# target's user database so the new one can take its place. Its HOME is kept -
# that is the configured desktop - and moved to the new name when the name
# differs, so the person does not start from a bare session.
if chroot "$TARGET" getent passwd gnuchanos >/dev/null; then
    chroot "$TARGET" userdel gnuchanos
fi
if chroot "$TARGET" getent group gnuchanos >/dev/null; then
    chroot "$TARGET" groupdel gnuchanos
fi
if [ "$NEWUSER" != "gnuchanos" ] && [ -d "$TARGET/home/gnuchanos" ] && [ ! -e "$TARGET/home/$NEWUSER" ]; then
    mv "$TARGET/home/gnuchanos" "$TARGET/home/$NEWUSER"
fi

chroot "$TARGET" useradd -m -s /bin/bash -c "$FULLNAME" "$NEWUSER"
printf '%s:%s\n' "$NEWUSER" "$PASS1" | chroot "$TARGET" chpasswd

chroot "$TARGET" getent group sudo >/dev/null ||
    die "the target has no sudo group; cannot configure an administrator account"
if ! chroot "$TARGET" getent group render >/dev/null 2>&1; then
    chroot "$TARGET" groupadd --system render
fi
for g in sudo audio video render cdrom floppy dialout plugdev games netdev bluetooth lpadmin; do
    if chroot "$TARGET" getent group "$g" >/dev/null 2>&1; then
        chroot "$TARGET" usermod -aG "$g" "$NEWUSER"
    fi
done
chroot "$TARGET" chown -R "$NEWUSER:$NEWUSER" "/home/$NEWUSER"
note "account $NEWUSER created"

printf '%s\n' "$HOSTNAME" > "$TARGET/etc/hostname"
cat > "$TARGET/etc/hosts" <<HOSTS
127.0.0.1 localhost
127.0.1.1 $HOSTNAME
::1 localhost ip6-localhost ip6-loopback
HOSTS

# --- 11. take the live-only bits back out ----------------------------------

step "Removing the live session's own shortcuts"

# The live user's passwordless sudo (0050-liveuser) is right on a live image and
# wrong on an installed one: it makes the account a root with no password.
rm -f "$TARGET/etc/sudoers.d/90-gnuchanos-live"

# The installer runs once. On the installed machine it must not open again, so
# the entry that starts it at login is removed - the XDG one and the session's
# own list, which is the one GnuChanWM actually reads.
rm -f "$TARGET/etc/xdg/autostart/gnuchanos-installer.desktop"
if [ -f "$TARGET/home/$NEWUSER/.config/GnuChanWM/GnuChanWM.py" ]; then
    sed -i '/gnuchanos-installer/d' "$TARGET/home/$NEWUSER/.config/GnuChanWM/GnuChanWM.py"
fi
rm -f "$TARGET/usr/local/bin/gnuchanos-installer"

# Give this installation its own SSH identity and make the daemon available
# after reboot. The ISO intentionally contains no shared host keys.
step "Preparing SSH for this installation"
rm -f "$TARGET"/etc/ssh/ssh_host_*_key "$TARGET"/etc/ssh/ssh_host_*_key.pub
chroot "$TARGET" ssh-keygen -A
chroot "$TARGET" sh -c 'set -eu; set -- /etc/ssh/ssh_host_*_key; [ -s "$1" ]; /usr/sbin/sshd -t'
chroot "$TARGET" systemctl unmask ssh.service
chroot "$TARGET" systemctl enable ssh.service
chroot "$TARGET" systemctl is-enabled --quiet ssh.service ||
    die "ssh.service is not enabled in the installed system"
note "generated and validated machine-specific SSH host keys; enabled ssh.service"

step "Enabling firewall and power management"
for tool in ufw tlp; do
    [ -x "$TARGET/usr/sbin/$tool" ] ||
        die "$tool is missing from the target filesystem"
done
for unit in gnuchanos-firewall-setup.service ufw.service tlp.service; do
    [ -f "$TARGET/etc/systemd/system/$unit" ] ||
    [ -f "$TARGET/usr/lib/systemd/system/$unit" ] ||
    [ -f "$TARGET/lib/systemd/system/$unit" ] ||
        die "$unit is missing from the target filesystem"
    chroot "$TARGET" systemctl enable "$unit"
done
note "enabled UFW initialization, UFW, and TLP for the installed system"

# GnuChanDM is the greeter the installed machine comes up to, and the graphical
# target is what starts it. Do not hide failures here: a system with no enabled
# greeter cannot offer a login screen after reboot.
[ -x "$TARGET/usr/local/bin/GnuChanDM" ] ||
    die "GnuChanDM is missing from the target; the image's program build was incomplete"
[ -f "$TARGET/etc/pam.d/gnuchandm" ] ||
    die "the GnuChanDM PAM policy is missing from the target"
[ -f "$TARGET/etc/systemd/system/gnuchandm.service" ] ||
    die "gnuchandm.service is missing from the target"
[ -x "$TARGET/usr/local/bin/GnuChanWM" ] ||
    die "GnuChanWM is missing from the target; the image's program build was incomplete"
[ -f "$TARGET/usr/share/xsessions/gnuchanwm.desktop" ] ||
    die "the GnuChanWM session entry is missing from the target"
chroot "$TARGET" systemctl enable gnuchandm.service
chroot "$TARGET" systemctl set-default graphical.target

# --- 12. done ---------------------------------------------------------------

step "Installation complete"
note "installed on $disk"
note "log in as $NEWUSER"
note "log copied to /var/log/gnuchanos-install.log on the installed system"
install -D -m 0644 "$LOG" "$TARGET/var/log/gnuchanos-install.log"

step "Unmounting"
if ! cleanup; then
    die "one or more target filesystems could not be unmounted"
fi
trap - EXIT

printf '\n'
printf '==============================================================\n'
printf ' GnuChanOS is installed on %s\n' "$disk"
printf ' Log in as: %s\n' "$NEWUSER"
printf ' Remove the medium and reboot.\n'
printf ' Log saved to /var/log/gnuchanos-install.log on the installed system.\n'
printf '==============================================================\n'
