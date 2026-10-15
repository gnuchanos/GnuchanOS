# GnuchanOS live ISO

This directory is the recipe for the GnuchanOS live ISO: a Debian-based image
that carries the Gnuchan programs and the GCL language (each in its own
repository, fetched at build time by `GnuChanPrograms.sh`), and the desktop's
own files (`dotfile/`), and boots straight to the GnuChanWM desktop. Everything
is installed **inside the image** - nothing is installed after boot. The
installer is this repository's own program, not Calamares: a small X11 window
(`installer/src/`, written in C against Xlib and Xft like the rest of the
desktop) asks which disk and the account's name and password, then runs a shell
script (`installer/gnuchanos-install.sh`) that partitions the disk, copies the
live filesystem onto it, writes GRUB and makes the account - showing every line
the script prints.

Nothing here installs onto the machine that builds it. The image is produced in
`distro/_build/` (ignored by git) and the live-build working tree never touches
the source tree.

## What it is made with

  * **live-build** — Debian's own tool for building a live image. It turns a
    package list and an overlay directory into a bootable ISO, and keeps the
    image a *Debian* image rather than a custom squashfs that drifts from the
    distribution.
  * **Debian trixie** (stable) as the base, with backports enabled because
    XLibre needs the newer libraries the base release does not ship — the same
    reason `dotfile/XLIBRE/Install_xlibre.py` enables backports on a stable
    machine.

## What goes in

  * **XLibre** — the X server the desktop runs on. It is not in Debian, so it
    comes from the xlibre-debian repository, installed by the same script that
    installs it on a normal machine (`dotfile/XLIBRE/Install_xlibre.py`). The
    image therefore boots on XLibre rather than on X.Org.

  * **The Gnuchan programs** — each lives in its own repository under the
    `gnuchanos` account and is fetched and installed by `GnuChanPrograms.sh`:
    the image is built by cloning them and running each one's own `makefile.py`,
    so nothing about how a program is built is repeated here. That script holds
    the one list of what the programs are and the order they install in. The
    distro build is strict: a program that cannot be fetched or built fails the
    image build instead of shipping a desktop with missing components. Set
    `GNUCHAN_PROGRAMS_STRICT=0` only when deliberately building an incomplete
    development image.

  * **The GCL language** — its own repository (`gcl`), fetched by the same
    script as the programs and built by its own `makefile.py`, which is in that
    repository. It fetches Lua, Python and Raylib, compiles the interpreter,
    its modules and the IDE, and puts `gcl` on PATH. The book is this
    repository's own (`book/`) and is kept in the image at
    `/usr/share/gnuchanos/book`.

  * **The dotfiles** — the GRUB theme, the Plymouth theme, the GTK theme, the
    icon and cursor themes, the fonts, and the per-machine fixes under
    `dotfile/`. The boot themes are the machine's and are installed as root;
    the themes a session loads are installed into the live user's home, which
    is why the live user is created first.

  * **The session** — the desktop's own display manager, **GnuChanDM**, owns the
    console: it greets, authenticates the live user and starts **GnuChanWM** as
    the session. Logging out of the desktop comes back to the login screen,
    because the greeter is still running underneath. The live credentials are
    `gnuchanos` / `gnuchanos` (see the liveuser hook). The greeter and the lock
    screen both authenticate through PAM, so `/etc/pam.d/gnuchandm` and
    `/etc/pam.d/gnuchansl` are written by the session hook.

## The files here

    README.md            this note
    build.sh             build the ISO (run this on a Debian machine)
    GnuChanPrograms.sh   fetch and install the Gnuchan programs from their own
                         repositories (run inside the image by a hook)
    installer/           the installer: src/ is GnuChanInstall, the X11 window,
                         and gnuchanos-install.sh is the script that does the
                         disk work (see "The installer" below)
    config/              the live-build configuration tree
    config/package-lists/  the packages the image carries
    config/hooks/normal/   scripts run inside the image, in name order
    config/bootloaders/    files live-build copies into the ISO's own
                           bootloader tree (the /boot and /isolinux of the
                           image, not of an installed system);
                           splash.svg is the first screen of the ISO

`config/` is a plain live-build tree. `build.sh` copies it into
`distro/_build/work/`, stages the source trees into the overlay, runs
`lb config` and then `lb build` there, so the authored configuration is never
mixed with the files live-build generates. There is deliberately no
`config/auto/`: live-build runs an `auto/` script and then re-runs its own
command with `noauto`, and a build that does its two steps in `build.sh` has no
redirection to get wrong and no risk of an `auto/build` calling `lb build` on
top of a script that already did.

## Building

On a Debian machine with `live-build` installed:

    sudo apt-get install -y live-build
    cd distro
    sudo ./build.sh

The result is `distro/_build/GnuchanOS-trixie-amd64.iso`.

The build is long: it fetches Lua, Python and Raylib and compiles every program
and the language inside the chroot, on top of the usual debootstrap.

### Building from WSL

The build works from WSL. When the source is on `/mnt/c`, it copies the tree to
a fresh, unique directory under `/var/tmp` and copies the ISO back after a
successful build. The ISO is copied in bounded chunks, verified, and only then
replaces any previous output, so a failed copy leaves the complete Linux-side
image available. Set `GNUCHANOS_BUILD_DIR` to choose a different Linux
filesystem parent directory. It must be outside the source tree; existing
directories are never cleared.

When `GnuChanDM` is checked out beside `GnuchanOS` (for example,
`Documents/GnuChanDM` beside `Documents/GnuchanOS`), that local checkout is
used for the ISO in preference to the remote DM repository. An explicit
checkout at `distro/repos/GnuChanDM` takes precedence over the sibling
checkout. This lets local display-manager fixes be tested in a WSL ISO before
they are published.

### Building an image without a token

`build.sh` stages a directory of local checkouts into the image, at the path
`GnuChanPrograms.sh` looks **first**: `distro/repos/<Name>`, copied to
`/opt/gnuchanos/repos/<Name>`. A build that has checkouts there uses them
instead of cloning, which is what makes an image possible when a program's
repository cannot be reached - or when it is **private** and the machine doing
the build has no credential.

The directory name is the **program's** name (`GnuChanWM`, `GCL`, ...), not the
repository's: the language's repository is `gcl` while its directory is `GCL`.
A build with no `distro/repos/` still works - the script falls back to cloning
from `github.com/gnuchanos` - so this is an optimisation, not a requirement,
except where a repository is private, which the clone always required a
credential for.

The working checkout at `../GnuChanDM` is also staged automatically if present,
unless `distro/repos/GnuChanDM` already exists. Thus local display-manager
changes are used in a WSL image build before they are pushed to the remote.

## Continuous integration and releases

`.github/workflows/iso.yml` builds the image and publishes it, **by hand**
(`workflow_dispatch`) and not on every push: a full live-build is hours of work
and several gigabytes, and a push is already checked by each program's own
workflow. Run it from the Actions tab when an image should exist.

The job does the one thing the build inside the image cannot: it fetches the
program repositories **with a token** into `distro/repos/`, so the image build
uses those checkouts and needs no network to github. The token is the
`GNUCHAN_REPOS_TOKEN` secret, a PAT with read access to the program
repositories. With the secret unset, the fetch falls back to anonymous https and
a private repository simply does not arrive - which makes the strict image
build fail rather than silently producing an incomplete ISO.

The build itself runs in a **privileged `debian:trixie` container**, because
`build.sh` is Debian-only and drives `live-build`, which needs root and mounts.
The image is uploaded as an artifact and then published as a release.

### Releasing

The version lives in `version.txt` at the repository root
(`GnuchanOS/version.txt`) and is the ONLY place it is written by hand. Bump it,
commit, run the workflow, and it publishes a release `v<version>` with the ISO
attached as `GnuchanOS-<version>-amd64.iso`. Run it again without touching the
file and the release already exists, so nothing new goes out - which makes the
workflow safe to re-run after a failure.

A version written with a suffix is published as it is: `0.1.1_test` becomes the
release `v0.1.1_test`, which is how a test image is told apart from a real one
without a second file to keep in step.

## The hooks, in order

The hooks are what make the image GnuchanOS, and each one is a step that cannot
be expressed as a package:

    0045-locale              generate the locales and set the system default.
                             The `locales` package alone does nothing: every
                             entry in /etc/locale.gen ships commented out, so
                             without this the image comes up in the C locale
                             and every program that calls setlocale - which
                             the Gnuchan programs all do - gets an encoding
                             that cannot represent Turkish text. en_US.UTF-8
                             and tr_TR.UTF-8 are generated; English is the
                             default.
    0050-liveuser            create the live user (`gnuchanos`, password
                             `gnuchanos`), first, so the installers that write
                             into its home have a home
    0060-branding            name the machine GnuChanOS (os-release, issue,
                             motd, lsb-release), so the console does not say
                             "Debian GNU/Linux 13"
    0070-network             turn the network ON: enable NetworkManager and
                             systemd-resolved, tell NetworkManager to hand its
                             DNS to resolved, and install the boot-time unit
                             that points /etc/resolv.conf at resolved's stub.
                             Without it the image connects to a wireless
                             network and passes no traffic - association is the
                             driver's job, but an address, a route and a
                             resolver come from these two services running.
                             The resolv.conf symlink is made at BOOT and not
                             here on purpose: the stub does not exist in the
                             chroot, and 0100 and 0300 still need DNS to
                             download while the image is built.
    0080-boot-branding       a BINARY hook: give the ISO's OWN boot menu the
                             GnuchanOS look. The ISO has TWO boot menus and they
                             are drawn differently, which is the whole reason
                             this is one hook and not a theme installer:
                             the BIOS menu is ISOLINUX, which has no theme and
                             whose background, logo and name are a single
                             bitmap, `isolinux/splash.png`; the EFI menu is
                             GRUB, which IS themed from a theme directory.
                             The bitmap is a BUILD INPUT, not something this
                             hook draws: the repository ships
                             `config/bootloaders/splash.svg` (the GnuchanOS
                             wallpaper with the GnuchanOS mascot on it, both
                             embedded in the file), live-build copies it into
                             each bootloader tree and renders it to splash.png
                             with the same step it uses for its own Debian
                             splash. This hook rewrites the menu
                             TEXT that still names the base distribution and
                             copies the GRUB theme the dotfiles hook built
                             inside the chroot into the ISO's EFI bootloader
                             tree, so the EFI menu boots with the same purple
                             theme an installed machine gets. It is a
                             .hook.binary and not a .hook.chroot because the
                             boot menu is generated in the binary stage, after
                             the live filesystem hooks have run; a chroot hook
                             themes the INSTALLED system's GRUB, not the ISO's.
                             (See "Boot menu branding" below.)
    0100-xlibre              run dotfile/XLIBRE/Install_xlibre.py
    0200-dotfiles            run the theme and settings installers under dotfile/,
                             the per-machine hardware fixes, and install the
                             wallpaper
    0300-gnuchan-programs    GnuChanPrograms.sh: the programs AND the GCL
                             language, each fetched from its own repository
    0360-zapret-config       force the zapret config settings the bypass needs
                             to RUN rather than merely be installed. The init
                             script only applies its firewall rules when
                             INIT_APPLY_FW=1, and GnuChanNetworkManager's own
                             makefile rewrites the config from zapret's
                             install_easy.sh, which does not set it - so
                             without this the nfqws daemon starts and receives
                             no packets at all. It runs AFTER 0300 (and so
                             after the network manager installed zapret and
                             rewrote the config) and is the last writer, so
                             the finished config is right. NFQWS_OPT - the
                             desync itself - is left alone, because the method
                             that works is chosen per network by blockcheck.
    0400-session             write the PAM services and enable GnuChanDM as the
                             display manager (logout -> login screen)
    0450-installer           build and install the installer: run
                             installer/src/makefile.py, which compiles the X11
                             window to /usr/local/bin/GnuChanInstall, installs
                             the script to /usr/local/sbin/gnuchanos-install,
                             and writes the launcher (/usr/local/bin/gnuchanos-
                             installer, which runs the window as root) and the
                             menu entry. It also adds the launcher to GnuChanWM's
                             own autostart list; GnuChanDM remains enabled and
                             still starts the chosen session.
    9990-cleanup             remove the sources, keep the book

### Why a hook and not a package list alone

Four of the things the image needs cannot be expressed as a package:

  1. **XLibre** is not in Debian, so it comes from its own repository.
  2. **The programs and the language** are built from source, each from its own
     repository, by a script that clones and builds them.
  3. **The themes** are files from `dotfile/`, run through their own installers.
  4. **The session** is the PAM services GnuChanDM and GnuChanSL authenticate
     through and the systemd wiring that makes GnuChanDM own the console, which
     a hook writes so a change to one is a change to the ISO.

## Session on the live image

    gnuchandm.service (starts X on :0, puts the greeter on it)
        -> GnuChanDM    (authenticates the user, runs the chosen session)
        -> GnuChanWM    (the session the user picked from the list)

GnuChanDM is the display manager and is **enabled**: it owns the console, and
because it stays running while the session runs, logging out of GnuChanWM
brings the login screen back instead of dropping straight into a new session.
The two PAM services it and the lock screen authenticate through -
`/etc/pam.d/gnuchandm` and `/etc/pam.d/gnuchansl` - are written by the session
hook; without them `pam_start` refuses every password, which is what made the
lock screen unable to open.

The notification server and the dock are started by GnuChanWM from its own
autostart list in `~/.config/GnuChanWM/GnuChanWM.py`.

On the live image, that same list starts `gnuchanos-installer` when GnuChanWM
starts. The launcher uses the live user's passwordless sudo rule to open the
installer as root without a password prompt; this does not skip GnuChanDM or
log in the user automatically. The installer removes its autostart entry and
the live-only sudo rule from the installed system.

## Boot menu branding

The ISO shows **two** boot menus, and they are not drawn the same way, which is
why the boot branding is two mechanisms and not one theme:

  * **BIOS/legacy machines boot through ISOLINUX.** ISOLINUX has no theme of its
    own: everything behind the menu - the background, the logo and the
    distribution's name - is a **single bitmap**, and the menu is drawn over it
    (`menu background splash.png` in live-build's `stdmenu.cfg`). live-build
    generates that bitmap from *its own* Debian `splash.svg` (a black screen
    with the Debian swirl and "Debian GNU/Linux …"), which is why the image came
    up on Debian's splash however well the installed copy was themed. **The fix
    is a build input:** the repository ships `config/bootloaders/splash.svg` -
    the GnuchanOS wallpaper with the GnuchanOS mascot (`assets/logo.png`)
    centred on it, both embedded in the file as data URIs so it carries its own
    pictures - and live-build copies it into **each** bootloader tree ahead of
    the hooks and renders it to `splash.png` with the very same `rsvg-convert`
    step it uses for its own: at 640x480 for the ISOLINUX menu, at 800x600 for
    the GRUB one. There is nothing to run at build time; the file being there is
    the whole change, and it is why neither menu says Debian or carries the
    Debian swirl any more.

  * **EFI machines boot through GRUB**, and GRUB *is* themed from a theme
    directory. The `0080-boot-branding` binary hook copies the theme the
    dotfiles hook built inside the chroot (`/boot/grub/themes/GnuchanOS`, made by
    `dotfile/GRUB_THEME/settings_grub.py`) into the ISO's own `boot/grub/`, and
    points GRUB at it, so the EFI menu boots with the same purple theme an
    installed machine gets.

The hook also rewrites the menu **text** that still names the base distribution
(live-build fills its templates from `_PROJECT = "Debian GNU/Linux"`). It changes
the top-level ISOLINUX title only in `menu.cfg` on purpose: `menu title` is the
directive every nested submenu carries too ("Utilities", "Advanced install
options"), and rewriting them all would leave a list of submenus that cannot be
told apart.

Because these are files of the ISO's *bootloader* tree and not its filesystem,
they are configured by their presence under `config/bootloaders/` and by the
`0080` binary hook, **not** by an installer inside the image: a chroot hook runs
against the installed system's GRUB, which is a different thing from the menu the
ISO itself boots with.

## The installer

The image carries the installer, and **it is the image installing itself**: the
installer unsquashes the live system's own squashfs onto the target disk. There
is no second build and no package list installed onto the machine - the bytes on
the ISO are the bytes on the disk.

THE INSTALLER IS THIS REPOSITORY'S OWN PROGRAM, AND IT IS NOT CALAMARES.
Calamares used to be the installer and was dropped, because the install kept
stopping inside it for reasons it would not show: its bootloader module read a
config key its file did not set and died with "raised an exception"; a failed
grub-install was reported only as "returned error code 1" with the real reason
written to a log file and not the screen; and its display manager list does not
know GnuChanDM.

The replacement is two halves that install together, and the split between them
is the point:

  * **`installer/src/`** - **GnuChanInstall**, the WINDOW. An X11 program
    written in C against Xlib and Xft, built the same way GnuChanSL, GnuChanDM
    and GnuChanDock are, with its own `makefile.py`. It asks the four things a
    person has to decide - which disk, the login name, the password (twice) and
    the computer's name - validates them before anything is written, and then
    shows the install running.

  * **`installer/gnuchanos-install.sh`** - the INSTALL. One shell script that
    partitions the disk (msdos for BIOS, gpt with an EFI partition for UEFI),
    makes the filesystems, unsquashes the live system onto the target, writes
    `/etc/fstab`, installs GRUB, rebuilds the initramfs, and creates the
    account - printing every command's own output. Everything that touches the
    disk is here, and on purpose: the tools it runs each print their own errors,
    so a failure says what is wrong in its own words.

    When writing the installed system's `/etc/default/grub`, it replaces the
    live-only boot arguments but keeps the staged GnuchanOS theme and graphics
    mode. It verifies that `grub-mkconfig` enabled that theme before reporting a
    successful installation.

The window does not touch a disk. It hands the answers to the script as
environment variables, runs it as a child process, and reads its output a line
at a time into a pane in the window - with `select()` on both the X connection
and the script's pipe, in one thread, so the window keeps redrawing while the
install runs and no Xlib call is ever made from a second thread. The script
writes everything to `/var/log/gnuchanos-install.log` in the live environment
and copies the completed log to the installed system at the same path.

After pressing Install, type `ERASE` into the visible confirmation box and
press Enter. Computer names accept uppercase or lowercase letters, digits and
hyphens; they cannot start with or end with a hyphen.

The disk is chosen from a dropdown in the first row; open it and click the
intended disk, or use the arrow keys and Enter. With more than four disks, the
list scrolls.

One step is GnuchanOS's own rather than a stock installer's, and it is the
account: the live image ships a `gnuchanos` user (the liveuser hook), so the
script takes that account out of the target and hands its home to the account
the person names. The new account is added to the `sudo` group and the
available desktop-device groups. Everything else the installer does is what
any installer does.

## License: GPL3
