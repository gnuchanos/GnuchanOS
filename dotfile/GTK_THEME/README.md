# GnuchanPurple

Dark purple theme for GTK 2, GTK 3, GTK 4 and the window managers that read the
same tree: xfwm4, Marco/Metacity and Openbox.

Everything lives in `GnuchanPurple/`, and the directory name is the theme name
on purpose: every toolkit looks a theme up by the name written in
`gtk-theme-name`, so a directory called anything else is silently ignored.

```
GnuchanPurple/
  index.theme           metatheme entry, read by GNOME Tweaks and MATE
  gtk-2.0/gtkrc
  gtk-3.0/gtk.css
  gtk-4.0/gtk.css
  gtk-4.0/user.css      libadwaita overlay, installed separately
  xfce-notify-4.0/gtk.css
  metacity-1/           Marco and Metacity
  openbox-3/themerc
  xfwm4/                themerc plus 74 rendered PNGs
  thumbnail.png         preview shown by appearance dialogs
  extras/               kitty, Alacritty, Xresources, dunst, rofi, bars, WM colours
```

## Install with the script

```sh
python3 theme_install.py
```

That is the whole install. It writes the theme into the user theme
directories, renders the xfwm4 images into the installed copy, writes the
GTK 2, GTK 3 and GTK 4 configuration and copies the extras. Run it again after
changing anything here and it replaces what it installed with the current
version: there is no separate update step and no option to pass.

The script has no dependencies beyond the Python standard library, including
for the PNG writer xfwm4 needs.

It installs into both `$XDG_DATA_HOME/themes` and `~/.themes`; the second is a
symlink to the first, because GTK 4 prefers the former while GTK 2, xfwm4 and
most window managers still read the latter.

`~/.gtkrc-2.0` and the GTK 3 and GTK 4 `settings.ini` files are **merged**, not
replaced: keys you already set - `gtk-font-name`, `gtk-icon-theme-name`,
`gtk-cursor-theme-name`, `gtk-decoration-layout` - are kept. The previous
contents are still moved to `<name>.gnuchan-backup` on the first run, once, so
running the script a second time never overwrites the file the user had before
it ever ran. Put a backup back by hand if you want that file again.

On a session with no XSettings daemon - i3, sway, Openbox, bspwm - the settings
files above are not read. Add this line to `~/.profile` or `~/.xprofile` and log
back in:

```sh
. ~/.config/gnuchan-purple/gtk-env.sh
```

That line lives in a file the installer does not write, so removing it is a
manual step.

## Install by hand

The tree is complete as committed, PNGs included, so it can be copied without
running the script:

```sh
mkdir -p ~/.local/share/themes
cp -r GnuchanPurple ~/.local/share/themes/
```

Then select `GnuchanPurple` in your appearance settings, or set it directly:

```sh
# GTK 3 and GTK 4, ~/.config/gtk-3.0/settings.ini and the gtk-4.0 one
[Settings]
gtk-theme-name=GnuchanPurple
gtk-application-prefer-dark-theme=1
```

```sh
# GTK 2, ~/.gtkrc-2.0
gtk-theme-name = "GnuchanPurple"
include "/home/<user>/.local/share/themes/GnuchanPurple/gtk-2.0/gtkrc"
```

libadwaita applications ignore `GTK_THEME` and the settings files. They read
`~/.config/gtk-4.0/gtk.css`, so copy the overlay there by hand:

```sh
cp GnuchanPurple/gtk-4.0/user.css ~/.config/gtk-4.0/gtk.css
```

A hand install skips the `index.theme` `ButtonLayout`, the extras under
`~/.config/gnuchan-purple/`, and nothing else.

## Regenerating the images

`xfwm4/` and `thumbnail.png` are produced by the renderer inside
`theme_install.py`; they are committed so the hand install above works. After a
palette change, rebuild them:

```sh
python3 theme_install.py --render-assets
```

The install path renders the same images into the installed copy, so the
checked in files are a convenience, not a source of truth. The palette the
renderer draws from is `_PALETTE_SOURCE` at the top of `theme_install.py`; keep
it in step with the stylesheets by hand.

## Known gaps

- `extras/` files are starting points, not drop in replacements: the i3 and sway
  files carry a commented `colors { }` block to paste into your own bar, and the
  Alacritty file needs Alacritty 0.13 or newer to be used through `import`.
- `dotfile/ICON_THEME/` is a separate project; this theme ships no icons.
