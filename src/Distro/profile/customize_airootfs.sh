#!/usr/bin/env bash
# ISO kok dosya sistemi (squashfs) — mkarchiso customize asamasi.
set -euo pipefail

sed -i 's/^#\(en_US.UTF-8 UTF-8\)/\1/' /etc/locale.gen
locale-gen
echo 'LANG=en_US.UTF-8' >/etc/locale.conf
echo 'gnuchanos' >/etc/hostname

cat >/etc/os-release <<'EOF'
NAME="Gnuchanos"
PRETTY_NAME="Gnuchanos"
ID=gnuchanos
VERSION_ID=rolling
BUILD_ID=rolling
ANSI_COLOR="1;35"
HOME_URL="https://github.com/gnuchanos/GnuchanOS"
SUPPORT_URL="https://github.com/gnuchanos/GnuchanOS"
BUG_REPORT_URL="https://github.com/gnuchanos/GnuchanOS"
LOGO=gnuchanos
EOF

printf '%s\n' 'Gnuchanos live' >/etc/issue

sed -i 's/^#Color/Color/' /etc/pacman.conf 2>/dev/null || true
grep -q '^ILoveCandy' /etc/pacman.conf 2>/dev/null || sed -i '/^Color/a ILoveCandy' /etc/pacman.conf 2>/dev/null || true

DOT=/usr/share/gnuchanos/dotfiles
if [[ ! -d "$DOT" ]]; then
  echo "WARN: $DOT yok — once scripts/build-iso.sh veya sync-dotfiles-into-profile.sh calistir."
else
  chmod -R a+rX "$DOT" 2>/dev/null || true

  place_dotfiles_in_home() {
    local h="$1"
    install -d "$h/.config"
    for name in qtile dunst qutebrowser zathura pip; do
      if [[ -d "$DOT/$name" ]]; then
        cp -a "$DOT/$name" "$h/.config/"
      fi
    done
    if [[ -d "$DOT/extra/Select Dotfiles/MangoHud" ]]; then
      cp -a "$DOT/extra/Select Dotfiles/MangoHud" "$h/.config/MangoHud"
    fi
    for f in .bashrc .zshrc .vimrc .Xresources .ycm_extra_conf.py; do
      if [[ -f "$DOT/$f" ]]; then
        cp -a "$DOT/$f" "$h/"
      fi
    done
  }

  place_dotfiles_in_home /etc/skel
  install -d /etc/skel
  place_dotfiles_in_home /etc/skel

  if [[ -f /usr/share/gnuchanos/branding/bg.png ]]; then
    install -d /root/.config/qtile /etc/skel/.config/qtile
    cp -f /usr/share/gnuchanos/branding/bg.png /root/.config/qtile/bg.png
    cp -f /usr/share/gnuchanos/branding/bg.png /etc/skel/.config/qtile/bg.png
  fi

  if [[ -d "$DOT/Industrial" ]]; then
    install -d /usr/share/lxdm/themes
    cp -a "$DOT/Industrial" /usr/share/lxdm/themes/
  fi

  if [[ -f "$DOT/qtile.desktop" ]]; then
    install -d /usr/share/xsessions
    cp -a "$DOT/qtile.desktop" /usr/share/xsessions/
  fi

  if [[ -f "$DOT/30-touchpad.conf" ]]; then
    install -d /etc/X11/xorg.conf.d
    cp -a "$DOT/30-touchpad.conf" /etc/X11/xorg.conf.d/
  fi

  if [[ -f "$DOT/settings.ini" ]]; then
    install -d /etc/gtk-3.0
    cp -a "$DOT/settings.ini" /etc/gtk-3.0/
  fi

  if [[ -f "$DOT/files/zram-generator.conf" ]]; then
    cp -a "$DOT/files/zram-generator.conf" /etc/systemd/zram-generator.conf
  fi

  if [[ -d "$DOT/gnuchanBoot" ]]; then
    install -d /usr/share/plymouth/themes
    cp -a "$DOT/gnuchanBoot" /usr/share/plymouth/themes/
  fi
  if [[ -f "$DOT/plymouthd.conf" ]]; then
    install -d /etc/plymouth
    cp -a "$DOT/plymouthd.conf" /etc/plymouth/plymouthd.conf
  fi
  if [[ -d /usr/share/plymouth/themes/gnuchanBoot ]]; then
    plymouth-set-default-theme gnuchanBoot 2>/dev/null || true
  fi

  if [[ -d "$DOT/minimal" ]]; then
    install -d /boot/grub/themes
    cp -a "$DOT/minimal" /boot/grub/themes/ 2>/dev/null || true
  fi
  if [[ -f "$DOT/grub" ]]; then
    cp -a "$DOT/grub" /etc/default/grub
  fi
fi

systemctl enable lxdm.service 2>/dev/null || true

for svc in irqbalance tlp cpupower fail2ban sshd; do
  systemctl enable "${svc}.service" 2>/dev/null || true
done
