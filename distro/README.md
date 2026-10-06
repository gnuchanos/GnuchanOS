# GnuchanOS live ISO

This directory is the recipe for the GnuchanOS live ISO: a Debian-based image
that carries the Gnuchan programs (`gnuchan_softwares/`), the GCL language
(`language/`), and the desktop's own files (`dotfile/`), and boots straight to
the GnuChanWM desktop. Everything is installed **inside the image** - nothing is
installed after boot. When the live image runs correctly, the installer
(Calamares) is added on top of it.

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

  * **The Gnuchan programs** — built and installed by
    `gnuchan_softwares/install_all.py`, in its own order. That list is the one
    authority on what the programs are, so a program added there is on the ISO
    with nothing to keep in step here.

  * **The GCL language** — built and installed by `language/makefile.py`, the
    program's own build chain. It fetches Lua, Python and Raylib, compiles the
    interpreter, its modules and the IDE, and puts `gcl` on PATH. The book is
    kept in the image at `/usr/share/gnuchanos/book`.

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

## The hooks, in order

The hooks are what make the image GnuchanOS, and each one is a step that cannot
be expressed as a package:

    0050-liveuser            create the live user (`gnuchanos`, password
                             `gnuchanos`), first, so the installers that write
                             into its home have a home
    0060-branding            name the machine GnuChanOS (os-release, issue,
                             motd, lsb-release), so the console does not say
                             "Debian GNU/Linux 13"
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
    0300-gnuchan-programs    install_all.py (the programs) + language (GCL)
    0400-session             write the PAM services and enable GnuChanDM as the
                             display manager (logout -> login screen)
    9990-cleanup             remove the sources, keep the book

### Why a hook and not a package list alone

Four of the things the image needs cannot be expressed as a package:

  1. **XLibre** is not in Debian, so it comes from its own repository.
  2. **The programs and the language** are built from this checkout, not fetched.
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

## What is added later

The installer (Calamares) goes on top of this image once it boots correctly.
It is deliberately not part of the recipe yet: the live system has to work
first, and an installer built before then is an installer nobody can test.

## License: GPL3
