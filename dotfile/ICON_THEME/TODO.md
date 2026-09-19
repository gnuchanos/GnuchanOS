# GnuchanPurple icon theme — completion TODO

This file is the work plan for making `dotfile/ICON_THEME` a theme that answers
*every* icon name a desktop asks for: the freedesktop naming specification in
full, the XFCE settings panels and panel plugins, the settings panels of the
other desktops and window managers, and the programs a Linux desktop actually
launches.

It is a TODO rather than documentation because every box below maps to a
concrete change in a named module, and the build verifies it: a name in the
catalogue whose glyph does not exist stops the build, and `--check` reports it.

    python dotfile/ICON_THEME/icon_install.py --build-only   # write the tree
    python dotfile/ICON_THEME/icon_install.py --check        # report problems
    python dotfile/ICON_THEME/icon_install.py --list         # every name
    python -m icons.sheet --size 16 --out sheet-16.svg       # review the set

---

## 1. How the theme is built

Nothing here is hand-drawn per file. An icon is a list of primitives
(`Rect`, `Disc`, `Poly`, `Ring`, `Line`, `Arc`) authored in a 100×100 unit box;
each primitive knows its outline *and* a signed distance function, so the same
artwork is both rasterised to PNG and written as SVG without the two drifting
apart. The pieces:

| Module | Responsibility |
| --- | --- |
| `icons/shape.py`, `icons/primitives.py` | the six primitives, in unit space |
| `icons/raster.py` | primitives → PNG bytes |
| `icons/svg.py` | primitives → SVG document |
| `icons/glyphs_base.py` | the glyph registry, `@glyph`, and the shared drawing helpers |
| `icons/glyphs_*.py` | the artwork, one module per family |
| `icons/catalogue.py` | which names exist, which glyph draws each, in which context |
| `icons/catalogue_*.py` | the rest of the name table, split by subject |
| `icons/tree.py` | directories, `index.theme`, the files, the render cache |
| `icons/sheet.py` | contact sheets, for judging the set rather than one icon |
| `icon_install.py` | build, install, and the `--check` audit |

Adding an icon therefore means one of exactly two things:

* **a name for a picture that exists** — one `branch(...)` line in a catalogue
  module, e.g. `branch(ACTIONS, "keyboard", "preferences-desktop-keyboard")`;
* **a new picture** — one `@glyph("...")` function in a glyph module, then the
  names that draw it.

The build cache is keyed by glyph, tint and size, so a glyph shared by forty
names is rasterised once. That is what keeps a theme this size buildable.

---

## 2. Measured state before this pass

Produced by `_temp/icon_audit.py`, which imports the catalogue modules the same
way `tree.py` does:

| Measure | Before |
| --- | --- |
| Registered glyphs | 292 |
| Catalogue entries (name → glyph → context) | 946 |
| `-symbolic` names | 814 |
| Total icon names | 1743 |
| Populated contexts | 7 of 12 |
| Fixed PNG sizes | 5 (16, 22, 24, 32, 48) |
| Scalable SVG sizes | 3 (64, 128, 256) |
| Animated icons | none |
| `scalable/<Context>` root directories | none |

Contexts that the specification defines, that `catalogue.CONTEXTS` already
lists, and that no branch populates — so the generated `index.theme` has no
directory for them at all:

- [ ] `Animations` — empty
- [ ] `Emblems` — empty (the `emblem-*` names sit in `Status` instead)
- [ ] `Emotes` — empty
- [ ] `International` — empty
- [ ] `UI` — empty

---

## 3. Definition of done

The theme is complete when all of the following hold, and each is checked by a
command rather than by eye:

- [ ] Every context in the specification is populated and written to
      `index.theme` with its own directory at every size.
- [ ] Every name in the Icon Naming Specification resolves, in its specified
      context.
- [ ] Every `*-symbolic` name the spec defines exists as a recolourable
      scalable SVG.
- [ ] The XFCE settings panels, the panel plugins and the XFCE programs are all
      present.
- [ ] The settings panels of GNOME, KDE, MATE, LXQt, LXDE, Cinnamon, Budgie,
      Pantheon, Deepin and Enlightenment are all present.
- [ ] The common window managers, compositors, status bars, launchers, lockers
      and portals are all present.
- [ ] The programs in section 8 are all present.
- [ ] `--check` reports no problems, and `unknown_glyphs()`,
      `duplicate_names()` and `unusable_names()` are all empty.
- [ ] Contact sheets of every family are reviewed at 16 px, not just at 128.
- [ ] An animated `process-working` spinner exists in the `Animations` context.

---

## 4. Phase 0 — infrastructure

Everything in this phase is a prerequisite for the rest; nothing user-visible
depends on it directly.

- [ ] **0.1 Contexts get a tint.** `tree.CONTEXT_TINT_KEY` maps a context to a
      palette entry, but it has no entry for `Emblems`, `Emotes`,
      `International`, `UI` or `Animations`, so they would silently fall back to
      the action tint. Add all five to `CONTEXT_TINT_KEY` and to
      `palette.CONTEXT_TINT`.
- [ ] **0.2 New artwork modules are registered.** `glyphs_base.ARTWORK_MODULES`
      is the one list that decides which artwork is loaded. Add
      `glyphs_panels`, `glyphs_emblems`, `glyphs_emotes`, `glyphs_flags` and
      `glyphs_ui` to it.
- [ ] **0.3 New catalogue modules are imported.** `tree.py` imports
      `catalogue` and `catalogue_files` for their registration side effects.
      Import `catalogue_xfce`, `catalogue_desktops`, `catalogue_programs` and
      `catalogue_extra` the same way, so the names exist before the catalogue is
      validated.
- [ ] **0.4 The size ladder is completed.** Fixed sizes become 8, 12, 16, 22,
      24, 32, 36 and 48; scalable sizes become 64, 96, 128, 192, 256 and 512.
      A theme that answers `8x8` and `96x96` instead of guessing looks correct
      in toolbars and in HiDPI launchers alike.
- [ ] **0.5 A root `scalable/<Context>` directory is written.** GTK 4 and Qt 6
      look there first; today every SVG lives under `<size>/<Context>/scalable`,
      which is the older arrangement. Write both.
- [ ] **0.6 An animated spinner.** Implement a GIF writer (`icons/gif.py`) and
      write `process-working.gif` into the `Animations` directories. The
      rasteriser already produces RGBA frames; the GIF writer needs a palette,
      nearest-colour mapping and LZW, all of which are standard library work.
- [ ] **0.7 Documentation matches the code.** Update the `icons/__init__.py`
      docstring and `__all__` for the new modules, and `tree._readme` for the
      new sizes.

---

## 5. Phase 1 — the five empty contexts

### 5.1 `Emblems` — the badges a file manager overlays on a file

The names exist in the specification, but in this theme they currently sit in
`Status`, so `emblem-important` is written to `Status/` and nothing is written
to `Emblems/`. Register the whole set in both contexts — a name may belong to
two contexts, and `catalogue.entries()` de-duplicates per context, not per name.

- [ ] Badges that reuse existing artwork: `emblem-mail`, `emblem-downloads`,
      `emblem-import`, `emblem-export`, `emblem-symbolic-link`,
      `emblem-synchronized`, `emblem-web`, `emblem-warning`,
      `emblem-locked`, `emblem-desktop`, `emblem-documents`, `emblem-music`,
      `emblem-photos`, `emblem-videos`, `emblem-default`, `emblem-important`,
      `emblem-favorite`, `emblem-shared`, `emblem-readonly`, `emblem-system`,
      `emblem-synchronizing`.
- [ ] New artwork needed: `emblem-generic` (a plain label), `emblem-new`
      (a sparkle badge), `emblem-unreadable` (a page struck through),
      `emblem-unlocked`, `emblem-raised`, `emblem-dropbox`, `emblem-personal`,
      `emblem-urgent`, `emblem-success`, `emblem-error`, `emblem-encrypted`.
- [ ] The Apple-style legacy aliases some applications still ask for:
      `stock_new`, `stock_save`, `stock_open`, `stock_shared`.

### 5.2 `Emotes` — the faces a chat client draws inline

One face shape, one helper, all the expressions, so the family reads as one set.

- [ ] Base: `face-smile`, `face-sad`, `face-plain`, `face-wink`,
      `face-laugh`, `face-smile-big`, `face-surprise`, `face-uncertain`,
      `face-angel`, `face-angry`, `face-cool`, `face-crying`,
      `face-devilish`, `face-embarrassed`, `face-kiss`, `face-monkey`,
      `face-raspberry`, `face-sick`, `face-smirk`, `face-tired`,
      `face-worried`, `face-glasses`, `face-ninja`, `face-yawn`.
- [ ] The `emote-*` and `stock_smiley*` aliases that older GTK clients use.

### 5.3 `International` — locale, character set and flags

- [ ] Locale marks: `locale`, `language`, `charset`, `input-method`,
      `intl-locale` — drawn from the existing translation glyph.
- [ ] `flag-xx` for the countries a desktop is most likely to be configured
      for, from a compact table of band layouts (horizontal bands, vertical
      bands, Nordic cross, diagonal, circle, triangle) so the flags are
      generated rather than drawn one by one.

### 5.4 `UI` — the names for widgets rather than documents

The specification puts these in `UI`, and themes that only write them to
`Actions` are the reason a GTK 4 header bar sometimes falls back.

- [ ] Window controls: `window-close`, `window-minimize`, `window-maximize`,
      `window-restore`, `window-unmaximize`, `window-new`,
      `window-close-others`.
- [ ] Panel arrows: `pan-down`, `pan-up`, `pan-start`, `pan-end` and their
      `-symbolic` forms, plus `pan-down-symbolic` used by expanders.
- [ ] View controls: `zoom-in`, `zoom-out`, `zoom-original`, `view-more`,
      `view-more-horizontal`, `view-list`, `view-grid`, `view-continuous`.
- [ ] New artwork needed: `list-drag-handle` (a grip), `sidebar-show`,
      `sidebar-hide`, `sidebar-show-right`, `sidebar-hide-right`, `tab-drag`,
      `slider-handle`, `selection-mode`, `open-menu` (a hamburger, distinct
      from `bars`), `font-selector`, `color-selector`.

### 5.5 `Animations` — the spinner

- [ ] `process-working` as an animated GIF at 16, 22, 24, 32 and 48 px, with
      enough frames for a smooth ten-to-twelve-step loop at two frames per
      step.
- [ ] Static frames as SVG: `process-working-01` … `process-working-36`, so a
      toolkit that cannot use a GIF can step through them.
- [ ] Aliases the desktops ask for: `spinner`, `loading`, `throbber`,
      `activity-indicator`.

---
## 6. Phase 2 — XFCE

XFCE is the desktop this theme is aimed at first, so it gets the whole
vocabulary: every settings panel, every panel plugin, every XFCE program and the
session internals. The names below are the ones the XFCE packages actually put
in `Icon=` in their `.desktop` files, plus the names their windows ask for when
they are already running.

A new artwork module `glyphs_panels.py` supplies the pictograms these need; a
new catalogue module `catalogue_xfce.py` maps every name below onto one of them.

### 6.1 The settings panels (`xfce4-settings`)

- [ ] Main entries: `xfce4-settings`, `xfce4-settings-manager`,
      `xfce4-settings-editor`, `xfce-settings-manager`, `xfce4-about`,
      `xfce4-settings-helper`.
- [ ] Appearance and desktop: `xfce4-appearance-settings`,
      `preferences-desktop-theme`, `preferences-desktop-icons`,
      `preferences-desktop-wallpaper`, `xfce4-desktop-settings`,
      `xfdesktop-settings`, `xfce4-backdrop`, `xfce4-color-settings`.
- [ ] Displays and input: `xfce4-display-settings`,
      `xfce4-display-settings-brightness`, `xfce4-keyboard-settings`,
      `xfce4-mouse-settings`, `xfce4-input-settings`, `xfce4-touchpad-settings`,
      `xfce4-accessibility-settings`, `xfce4-accessibility`,
      `preferences-desktop-keyboard`, `preferences-desktop-keyboard-shortcuts`,
      `preferences-desktop-mouse`, `preferences-desktop-display`.
- [ ] Sound, power and sessions: `xfce4-sound-settings`, `xfce4-mixer`,
      `xfce4-power-manager-settings`, `xfce4-power-manager`,
      `xfce4-session-settings`, `xfce4-session`, `xfce4-session-logout`,
      `xfce4-screensaver-preferences`, `xfce4-screensaver`,
      `xfce4-screenshooter`, `xfce4-notifyd-config`, `xfce4-notifyd`.
- [ ] Files, window manager and workspaces: `xfce4-mime-settings`,
      `xfce4-mime-editor`, `xfwm4-settings`, `xfwm4-tweaks-settings`,
      `xfwm4-workspace-settings`, `xfce4-window-manager`,
      `xfce4-window-manager-tweaks`, `xfce4-workspaces`,
      `xfce4-workspace-settings`, `xfce4-panel-settings`,
      `xfce4-panel-preferences`, `xfce4-taskmanager`, `xfce4-sysinfo`.

### 6.2 The panel plugins

Forty plugins, one icon each. They are the most visible icons in an XFCE
session — they sit in the panel all day — so each gets its own pictogram rather
than a generic gear.

- [ ] System monitors: `xfce4-cpugraph-plugin`, `xfce4-cpufreq-plugin`,
      `xfce4-systemload-plugin`, `xfce4-diskperf-plugin`, `xfce4-sensors-plugin`,
      `xfce4-fsguard-plugin`, `xfce4-battery-plugin`, `xfce4-brightness-plugin`,
      `xfce4-power-manager-plugin`, `xfce4-alsa-plugin`, `xfce4-mpc-plugin`.
- [ ] Network and hardware: `xfce4-netload-plugin`, `xfce4-wavelan-plugin`,
      `xfce4-bluetooth-plugin`, `xfce4-mount-plugin`, `xfce4-kbdleds-plugin`,
      `xfce4-xkb-plugin`, `xfce4-eyes-plugin`, `xfce4-camera-plugin`.
- [ ] Information: `xfce4-clock`, `xfce4-datetime-plugin`,
      `xfce4-orageclock-plugin`, `xfce4-weather-plugin`, `xfce4-timer-plugin`,
      `xfce4-time-out-plugin`, `xfce4-mailwatch-plugin`, `xfce4-dict`,
      `xfce4-dict-plugin`, `xfce4-notes-plugin`, `xfce4-notes`.
- [ ] Desktop and launchers: `xfce4-whiskermenu-plugin`, `xfce4-appfinder`,
      `xfce4-places-plugin`, `xfce4-smartbookmark-plugin`, `xfce4-verve-plugin`,
      `xfce4-directory-menu`, `xfce4-windowck-plugin`, `xfce4-pulseaudio-plugin`,
      `xfce4-clipman-plugin`, `xfce4-clipman`, `xfce4-sample-plugin`,
      `xfce4-embed-plugin`, `xfce4-generic-slider`, `xfce4-indicator-plugin`,
      `xfce4-vala-panel`, `xfce4-tasklist`.

### 6.3 The XFCE programs

- [ ] `thunar`, `thunar-settings`, `thunar-file-manager`, `thunar-volman`,
      `thunar-sendto-email`, `thunar-bulk-rename`, `org.xfce.thunar`.
- [ ] `mousepad`, `ristretto`, `parole`, `xfburn`, `orage`, `gigolo`,
      `xfce4-terminal`, `xfce4-taskmanager`, `xfce4-screenshooter`,
      `xfce4-appfinder`, `xfce4-dict`, `xfce4-sensors`, `xfce4-disk-usage`,
      `xfce4-mixer`, `xfce4-volumed`, `xfce4-power-manager`.

### 6.4 The session internals

- [ ] `xfce4-panel`, `xfdesktop`, `xfwm4`, `xfsettingsd`, `xfce4-session`,
      `xfce4-session-manager`, `xfce4-panel-restart`,
      `xfce4-popup-whiskermenu`, `xfce4-popup-applicationsmenu`,
      `xfce4-popup-places`, `xfce4-popup-directorymenu`.

---

## 7. Phase 3 — the other desktops and window managers

A new artwork module and a catalogue module (`catalogue_desktops.py`) carry
these. Where a panel has the same meaning as an XFCE one, it points at the same
glyph: `preferences-desktop-display` from GNOME, `kcm_randr` from KDE and
`lxqt-config-monitor` from LXQt are one monitor, so the three desktops read as
one theme rather than three.

### 7.1 GNOME

- [ ] Control centre: `gnome-control-center`, `org.gnome.Settings`,
      `gnome-settings`, `gnome-tweaks`, `gnome-tweak-tool`,
      `gnome-extensions-app`, `gnome-shell-extension-prefs`,
      `gnome-shell`, `gnome-session`, `gnome-session-quit`.
- [ ] The panel names GTK looks up by desktop id — these are the ones that show
      as a blank square when a theme misses them:
      `gnome-applications-panel`, `gnome-background-panel`,
      `gnome-bluetooth-panel`, `gnome-camera-panel`, `gnome-color-panel`,
      `gnome-datetime-panel`, `gnome-default-apps-panel`,
      `gnome-diagnostics-panel`, `gnome-display-panel`,
      `gnome-firmware-security-panel`, `gnome-info-overview-panel`,
      `gnome-keyboard-panel`, `gnome-location-panel`, `gnome-lock-panel`,
      `gnome-memory-panel`, `gnome-microphone-panel`, `gnome-mouse-panel`,
      `gnome-multitasking-panel`, `gnome-network-panel`,
      `gnome-notifications-panel`, `gnome-online-accounts-panel`,
      `gnome-power-panel`, `gnome-printers-panel`, `gnome-privacy-panel`,
      `gnome-region-panel`, `gnome-removable-media-panel`,
      `gnome-screen-panel`, `gnome-search-panel`, `gnome-sharing-panel`,
      `gnome-sound-panel`, `gnome-thunderbolt-panel`,
      `gnome-universal-access-panel`, `gnome-usage-panel`, `gnome-users-panel`,
      `gnome-wacom-panel`, `gnome-wwan-panel`.
- [ ] GNOME programs: `gnome-shell`, `gnome-builder`, `gnome-text-editor`,
      `gnome-console`, `kgx`, `gnome-contacts`, `gnome-documents`,
      `gnome-music`, `gnome-photos`, `gnome-weather`, `gnome-logs`,
      `gnome-power-statistics`, `gnome-connections`, `gnome-remote-desktop`,
      `gnome-firmware`, `gnome-authenticator`, `gnome-nettool`,
      `gnome-system-monitor`, `gnome-usage`, `gnome-abrt`, `gnome-todo`,
      `endeavour`, `gnome-recipes`, `gnome-clocks`, `gnome-chess`,
      `gnome-sudoku`, `gnome-mines`, `gnome-robots`, `gnome-nibbles`,
      `gnome-taquin`, `gnome-tetravex`, `gnome-2048`, `gnome-mahjongg`,
      `gnome-klotski`, `gnome-screenshot`, `gnome-disk-utility`,
      `gnome-multi-writer`, `gnome-font-viewer`, `gnome-color-manager`.

### 7.2 KDE Plasma

- [ ] Shell and settings: `plasmashell`, `plasma-desktop`,
      `systemsettings`, `systemsettings5`, `systemsettings6`, `kcmshell5`,
      `kcmshell6`, `kinfocenter`, `krunner`, `kwin`, `kwin_x11`,
      `kwin_wayland`, `kwin-wayland`, `ksmserver`, `ksplashqml`, `kscreen`,
      `kglobalaccel`, `kaccess`, `klipper`, `kded5`, `kded6`.
- [ ] The KCM panels: `kcm_access`, `kcm_activities`, `kcm_autostart`,
      `kcm_baloofile`, `kcm_bluetooth`, `kcm_clock`, `kcm_colors`,
      `kcm_componentchooser`, `kcm_cron`, `kcm_cursortheme`,
      `kcm_desktoptheme`, `kcm_device_automounter`, `kcm_display`,
      `kcm_dnsserver`, `kcm_dolphinview`, `kcm_energy`, `kcm_fonts`,
      `kcm_formats`, `kcm_gtk`, `kcm_icons`, `kcm_kamera`, `kcm_kded`,
      `kcm_kio`, `kcm_krunner`, `kcm_kscreen`, `kcm_kwallet`,
      `kcm_kwindecoration`, `kcm_kwinrules`, `kcm_kwinscripts`,
      `kcm_kwintabbox`, `kcm_language`, `kcm_launchfeedback`,
      `kcm_lookandfeel`, `kcm_lockscreen`, `kcm_mouse`,
      `kcm_networkmanagement`, `kcm_nightcolor`, `kcm_notifications`,
      `kcm_paths`, `kcm_phonon`, `kcm_plasma_theme`, `kcm_printer_manager`,
      `kcm_proxy`, `kcm_pulseaudio`, `kcm_qtquicksettings`, `kcm_randr`,
      `kcm_sddm`, `kcm_smserver`, `kcm_solid_actions`, `kcm_soundtheme`,
      `kcm_splashscreen`, `kcm_standard_actions`, `kcm_style`, `kcm_switcher`,
      `kcm_touchpad`, `kcm_users`, `kcm_wallpaper`, `kcm_webshortcuts`,
      `kcm_workspace`, `kcm_nightcolour`.
- [ ] KDE programs: `dolphin`, `konsole`, `kate`, `kwrite`, `kdevelop`,
      `kile`, `kcalc`, `ksysguard`, `plasma-systemmonitor`, `ark`, `okular`,
      `gwenview`, `spectacle`, `kmail`, `korganizer`, `kaddressbook`,
      `kontact`, `akregator`, `ktorrent`, `kget`, `kaffeine`, `dragon`,
      `elisa`, `juk`, `kdenlive`, `krita`, `digikam`, `showfoto`,
      `kolourpaint`, `karbon`, `kexi`, `kmymoney`, `skanlite`, `skanpage`,
      `kfind`, `kompare`, `kdiff3`, `kcachegrind`, `kdbg`, `kbackup`, `kup`,
      `ksystemlog`, `kwalletmanager`, `kcharselect`, `kcolorchooser`,
      `kruler`, `ktimer`, `kteatime`, `kweather`, `kclock`, `krecorder`,
      `kamoso`, `krename`, `krusader`, `filelight`, `partitionmanager`,
      `konversation`, `kopete`, `neochat`, `krdc`, `krfb`, `kleopatra`,
      `kgpg`, `kmix`, `kalendar`, `merkuro`, `kasts`, `kongress`,
      `kpublictransport`, `itinerary`, `kphotoalbum`, `plasma-pa`,
      `plasma-discover`, `discover`, `kwin-x11`, `kwin-wayland`.
- [ ] Plasma applets: `plasma-applet-*` variants for the panel —
      `plasma-applet-battery`, `plasma-applet-bluetooth`,
      `plasma-applet-clipboard`, `plasma-applet-digitalclock`,
      `plasma-applet-diskquota`, `plasma-applet-folder`, `plasma-applet-kickoff`,
      `plasma-applet-network`, `plasma-applet-notifications`,
      `plasma-applet-pager`, `plasma-applet-systemmonitor`,
      `plasma-applet-taskmanager`, `plasma-applet-volumewidget`,
      `plasma-applet-weather`, `plasma-applet-windowlist`.

### 7.3 MATE

- [ ] Control centre: `mate-control-center`, `mate-settings`,
      `mate-appearance-properties`, `mate-background-properties`,
      `mate-display-properties`, `mate-keybinding`, `mate-keyboard-properties`,
      `mate-mouse-properties`, `mate-network-properties`,
      `mate-notification-properties`, `mate-power-preferences`,
      `mate-screensaver-preferences`, `mate-session-properties`,
      `mate-default-applications-properties`, `mate-time-admin`,
      `mate-window-properties`, `mate-font-viewer`, `mate-color-select`,
      `mate-volume-control`, `mate-about-me`, `mate-about`, `mate-tweak`,
      `mozo`, `mate-menu`, `brisk-menu`, `mate-disk-usage-analyzer`.
- [ ] MATE programs: `caja`, `caja-settings`, `pluma`, `atril`, `engrampa`,
      `eom`, `mate-calc`, `mate-dictionary`, `mate-screenshot`,
      `mate-search-tool`, `mate-system-monitor`, `mate-system-log`,
      `mate-terminal`, `mate-sensors-applet`, `mate-user-guide`,
      `mate-panel`, `mate-notification-daemon`, `mate-screensaver`,
      `mate-polkit`, `marco`.

### 7.4 LXQt and LXDE

- [ ] LXQt: `lxqt-config`, `lxqt-config-appearance`,
      `lxqt-config-brightness`, `lxqt-config-file-associations`,
      `lxqt-config-globalkeyshortcuts`, `lxqt-config-input`,
      `lxqt-config-locale`, `lxqt-config-monitor`,
      `lxqt-config-notificationd`, `lxqt-config-powermanagement`,
      `lxqt-config-session`, `lxqt-about`, `lxqt-admin`, `lxqt-archiver`,
      `lxqt-openssh-askpass`, `lxqt-policykit-agent`, `lxqt-powermanagement`,
      `lxqt-runner`, `lxqt-sudo`, `lxqt-panel`, `lxqt-notificationd`,
      `lxqt-globalkeyshortcuts`, `lxqt-desktop`, `qterminal`, `qps`,
      `pcmanfm-qt`, `lximage-qt`, `screengrab`, `featherpad`, `qlipper`,
      `compton-conf`, `obconf-qt`, `pavucontrol-qt`, `nm-tray`,
      `lxappearance`, `lxappearance-obconf`, `lxqt-qtplugin`.
- [ ] LXDE: `lxde-control-center`, `lxpanel`, `lxpanelctl`, `lxrandr`,
      `lxtask`, `lxterminal`, `lxinput`, `lxsession`, `lxhotkey`,
      `lxmusic`, `lxdm`, `lxpolkit`, `gpicview`, `pcmanfm`, `lxsession-edit`,
      `lxappearance`, `lxtask`, `obconf`, `openbox`.

### 7.5 Cinnamon

- [ ] Settings: `cinnamon-settings`, `cinnamon-settings-appearance`,
      `cinnamon-settings-backgrounds`, `cinnamon-settings-desklets`,
      `cinnamon-settings-effects`, `cinnamon-settings-extensions`,
      `cinnamon-settings-fonts`, `cinnamon-settings-general`,
      `cinnamon-settings-hotcorner`, `cinnamon-settings-keyboard`,
      `cinnamon-settings-notifications`, `cinnamon-settings-panel`,
      `cinnamon-settings-power`, `cinnamon-settings-privacy`,
      `cinnamon-settings-screensaver`, `cinnamon-settings-sound`,
      `cinnamon-settings-startup`, `cinnamon-settings-themes`,
      `cinnamon-settings-users`, `cinnamon-settings-windows`,
      `cinnamon-settings-workspaces`, `cinnamon-menu-editor`,
      `cinnamon`, `muffin`, `cinnamon-screensaver`.
- [ ] X-apps: `nemo`, `xed`, `xplayer`, `xviewer`, `xreader`, `pix`,
      `xapp`, `xdg-desktop-portal-xapp`, `csd-*` (the Cinnamon settings
      daemons, one per panel).

### 7.6 Budgie, Pantheon, Deepin and Enlightenment

- [ ] Budgie: `budgie-desktop-settings`, `budgie-panel`, `budgie-wm`,
      `budgie-run-dialog`, `budgie-desktop`, `budgie-control-center`,
      `budgie-screenshot-applet`, `budgie-workspace-overview`.
- [ ] Pantheon / elementary: `io.elementary.settings`,
      `switchboard`, `io.elementary.files`, `io.elementary.appcenter`,
      `io.elementary.calculator`, `io.elementary.calendar`,
      `io.elementary.camera`, `io.elementary.code`, `io.elementary.mail`,
      `io.elementary.music`, `io.elementary.photos`, `io.elementary.videos`,
      `io.elementary.screenshot`, `io.elementary.tasks`,
      `io.elementary.shortcut-overlay`, `io.elementary.capnet-assist`,
      `io.elementary.feedback`, `io.elementary.wingpanel`, `gala`.
- [ ] Deepin: `dde-control-center`, `dde-file-manager`, `dde-launcher`,
      `dde-dock`, `deepin-terminal`, `deepin-editor`, `deepin-music`,
      `deepin-movie`, `deepin-image-viewer`, `deepin-screenshot`,
      `deepin-calculator`, `deepin-system-monitor`, `deepin-app-store`,
      `deepin-draw`, `deepin-voice-recorder`, `deepin-boot-maker`,
      `deepin-installer`, `startdde`.
- [ ] Enlightenment: `enlightenment`, `enlightenment-settings`,
      `e-settings`, `terminology`, `rage`, `ecrire`, `ephoto`, `eflete`,
      `econnman`, `evisum`, `enventor`, `elementary_config`.
- [ ] Trinity / TDE, for the machines that still run it: `kcontrol`,
      `tdesudo`, `tde-systemsettings`, `kate-tde`, `konqueror-tde`.

### 7.7 Window managers and their configuration tools

- [ ] Stacking WMs: `openbox`, `obconf`, `obmenu`, `obmenu-generator`,
      `obamenu`, `oblogout`, `fluxbox`, `fbsetbg`, `fbmenugen`, `fbrun`,
      `fluxconf`, `icewm`, `icewm-settings`, `icepref`, `jwm`, `jwm-settings`,
      `pekwm`, `pekwm-theme-index`, `fvwm`, `fvwm3`, `fvwm-themes`,
      `fterm`, `blackbox`, `bbtools`, `windowmaker`, `wmakerconf`,
      `afterstep`, `twm`, `ctwm`, `vtwm`, `amiwm`, `larswm`, `waimea`,
      `musca`, `echinus`, `subtle`, `notion`, `wmii`, `ion3`.
- [ ] Tiling WMs: `i3`, `i3-wm`, `i3bar`, `i3status`, `i3blocks`,
      `i3-config-wizard`, `i3-input`, `i3-msg`, `i3-nagbar`, `i3lock`,
      `i3lock-color`, `betterlockscreen`, `i3exit`, `i3-save-tree`,
      `sway`, `swaybg`, `swaybar`, `swayidle`, `swaylock`, `swaymsg`,
      `swaynag`, `sway-input`, `dwm`, `dwmblocks`, `dmenu`, `st`,
      `bspwm`, `sxhkd`, `herbstluftwm`, `qtile`, `xmonad`, `xmobar`,
      `taffybar`, `spectrwm`, `awesome`, `river`, `dwl`, `labwc`,
      `wayfire`, `wf-shell`, `hyprland`, `hyprctl`, `hyprpaper`,
      `hypridle`, `hyprlock`, `hyprsunset`, `hyprpicker`, `hyprshot`.
- [ ] Compositors: `picom`, `compton`, `xcompmgr`, `compiz`,
      `compizconfig-settings-manager`, `ccsm`, `ezoom`, `kwin-effects`.

### 7.8 Status bars, launchers, lockers and portals

- [ ] Bars and applets: `waybar`, `polybar`, `tint2`, `tint2conf`,
      `yambar`, `sfwbar`, `nwg-bar`, `nwg-dock`, `nwg-drawer`, `nwg-menu`,
      `nwg-panel`, `nwg-shell`, `nwg-wrapper`, `swaync`, `eww`, `ags`,
      `conky`, `gkrellm`, `slstatus`, `i3status-rust`, `walker`.
- [ ] Launchers and menus: `rofi`, `wofi`, `fuzzel`, `bemenu`, `dmenu`,
      `jgmenu`, `openbox-menu`, `xfce4-appfinder`, `alacarte`, `menulibre`,
      `yad`, `zenity`, `kdialog`, `sherlock`, `ulauncher`, `albert`,
      `synapse`, `kupfer`, `gnome-do`, `rofi-calc`, `rofi-emoji`.
- [ ] Lockers and idle managers: `xss-lock`, `slock`, `light-locker`,
      `xscreensaver`, `gnome-screensaver`, `mate-screensaver`,
      `xfce4-screensaver`, `cinnamon-screensaver`, `kscreenlocker`,
      `vlock`, `physlock`, `gtklock`, `swayidle`, `hypridle`, `wlogout`.
- [ ] Portals and session services: `xdg-desktop-portal`,
      `xdg-desktop-portal-gtk`, `xdg-desktop-portal-gnome`,
      `xdg-desktop-portal-kde`, `xdg-desktop-portal-lxqt`,
      `xdg-desktop-portal-wlr`, `xdg-desktop-portal-hyprland`,
      `polkit-gnome`, `polkit-kde-authentication-agent-1`, `lxpolkit`,
      `mate-polkit`, `polkit-1`, `gnome-keyring`, `kwalletd5`,
      `systemd-logind`, `elogind`, `earlyoom`, `systemd-oomd`.

---
## 8. Phase 4 — program icons

`catalogue_programs.py` carries these. The rule is the same as in
`catalogue_files.py`: a program that ships its own icon keeps it, and this table
is what covers the machines the theme has never seen. Names are grouped by the
icon they should draw — one line per picture, many names per line — because that
is the relationship that matters and the one that keeps forty text editors
looking like one family.

- [ ] **Browsers**: `firefox`, `firefox-esr`, `firefox-developer-edition`,
      `librewolf`, `waterfox`, `palemoon`, `basilisk`, `icecat`,
      `gnu-icecat`, `tor-browser`, `torbrowser-launcher`, `chromium`,
      `chromium-browser`, `google-chrome`, `google-chrome-stable`,
      `microsoft-edge`, `microsoft-edge-stable`, `brave-browser`,
      `brave-browser-beta`, `vivaldi`, `vivaldi-snapshot`, `opera`,
      `yandex-browser`, `epiphany`, `falkon`, `midori`, `qutebrowser`,
      `luakit`, `surf`, `dwb`, `uzbl`, `netsurf`, `dillo`, `links`,
      `lynx`, `w3m`, `elinks`, `nyxt`, `min`, `otter-browser`,
      `slimjet`, `browsh`, `searx`.
- [ ] **Mail, chat and conferencing**: `thunderbird`, `betterbird`,
      `evolution`, `geary`, `kmail`, `claws-mail`, `sylpheed`,
      `mailspring`, `mutt`, `neomutt`, `alpine`, `aerc`, `telegram`,
      `telegram-desktop`, `signal-desktop`, `discord`, `element-desktop`,
      `element`, `nheko`, `neochat`, `fractal`, `hexchat`, `pidgin`,
      `polari`, `irssi`, `weechat`, `quassel`, `konversation`, `kopete`,
      `dino`, `gajim`, `psi`, `toxic`, `slack`, `teams`, `zoom`,
      `skypeforlinux`, `whatsapp-desktop`, `caprine`, `vesktop`, `mumble`,
      `teamspeak`, `jami`, `ekiga`, `matrix-client`, `zapzap`.
- [ ] **Media players**: `vlc`, `mpv`, `smplayer`, `totem`, `parole`,
      `celluloid`, `gnome-mplayer`, `mplayer`, `kodi`, `dragon`,
      `kaffeine`, `rhythmbox`, `audacious`, `clementine`, `strawberry`,
      `lollypop`, `elisa`, `deadbeef`, `exaile`, `quodlibet`, `cantata`,
      `musique`, `pragha`, `gnome-music`, `juk`, `amarok`, `cmus`,
      `ncmpcpp`, `mpd`, `sonata`, `ario`, `mpc`, `moc`, `termusic`,
      `freetube`, `ytmdesktop`, `pipe-viewer`.
- [ ] **Video editors and recorders**: `kdenlive`, `shotcut`, `openshot`,
      `pitivi`, `avidemux`, `handbrake`, `flowblade`, `olive`, `natron`,
      `cinelerra`, `lightworks`, `davinci-resolve`, `obs`, `obs-studio`,
      `vokoscreen`, `simplescreenrecorder`, `recordmydesktop`, `kazam`,
      `peek`, `kooha`, `wf-recorder`, `byzanz`, `gromit-mpx`.
- [ ] **Audio editors and music production**: `audacity`, `ardour`, `lmms`,
      `mixxx`, `hydrogen`, `qtractor`, `musescore`, `rosegarden`,
      `carla`, `cadence`, `qjackctl`, `patchage`, `guitarix`, `rakarrack`,
      `yoshimi`, `zynaddsubfx`, `drumgizmo`, `seq66`, `sooperlooper`,
      `giada`, `vcvrack`, `bespoke`, `supercollider`, `puredata`,
      `sonic-pi`, `csound`, `chuck`, `ocenaudio`, `spek`,
      `sonic-visualiser`, `praat`, `reaper`, `bitwig`, `renoise`.
- [ ] **Image viewers and photo managers**: `gwenview`, `eog`, `loupe`,
      `ristretto`, `shotwell`, `feh`, `gthumb`, `nomacs`, `geeqie`,
      `gpicview`, `viewnior`, `comix`, `mcomix`, `sxiv`, `nsxiv`, `imv`,
      `pqiv`, `mirage`, `digikam`, `darktable`, `rawtherapee`,
      `rapid-photo-downloader`, `xviewer`.
- [ ] **Raster and vector graphics**: `gimp`, `krita`, `inkscape`,
      `mypaint`, `pinta`, `kolourpaint`, `scribus`, `karbon`, `synfig`,
      `opentoonz`, `pencil2d`, `azpainter`, `xournalpp`, `xournal`,
      `drawpile`, `blender`, `openscad`, `freecad`, `kicad`, `librecad`,
      `qcad`, `solvespace`, `wings3d`, `meshlab`, `cloudcompare`,
      `slic3r`, `prusa-slicer`, `cura`, `orcaslicer`, `fritzing`,
      `kicad-pcbnew`, `gerbv`, `dia`, `drawio`, `umbrello`, `plantuml`.
- [ ] **Office and documents**: `libreoffice`, `libreoffice-writer`,
      `libreoffice-calc`, `libreoffice-impress`, `libreoffice-draw`,
      `libreoffice-base`, `libreoffice-math`, `libreoffice-startcenter`,
      `onlyoffice-desktopeditors`, `wps-office`, `abiword`, `gnumeric`,
      `calligrawords`, `calligrasheets`, `calligrastage`, `kexi`,
      `softmaker`, `freeoffice`, `textmaker`, `planmaker`,
      `presentations`, `lyx`, `texmaker`, `texstudio`, `kile`, `texworks`,
      `evince`, `okular`, `mupdf`, `xpdf`, `atril`, `zathura`,
      `qpdfview`, `xreader`, `calibre`, `foliate`, `sigil`, `ebook-viewer`,
      `master-pdf-editor`, `pdfarranger`, `pdfsam`, `ocrmypdf`,
      `gscan2pdf`, `xsane`, `simple-scan`, `gimagereader`, `ocrfeeder`.
- [ ] **Notes, tasks and knowledge**: `gnome-notes`, `bijiben`, `notes`,
      `joplin`, `obsidian`, `logseq`, `zettlr`, `simplenote`,
      `standard-notes`, `qownnotes`, `zim`, `rednotebook`, `cherrytree`,
      `tomboy`, `gnote`, `cherrytree`, `trilium`, `taskwarrior`,
      `gnome-todo`, `endeavour`, `planner`, `ganttproject`, `planner`,
      `workrave`, `rsibreak`, `stretchly`.
- [ ] **Text editors and IDEs**: `gedit`, `gnome-text-editor`, `kate`,
      `kwrite`, `mousepad`, `leafpad`, `pluma`, `xed`, `notepadqq`,
      `emacs`, `vim`, `gvim`, `neovim`, `nano`, `micro`, `helix`,
      `kakoune`, `code`, `code-oss`, `vscodium`, `codium`,
      `vscode-insiders`, `cursor`, `zed`, `sublime-text`, `sublime_text`,
      `atom`, `brackets`, `geany`, `kdevelop`, `qtcreator`,
      `android-studio`, `intellij-idea`, `pycharm`, `clion`, `rider`,
      `webstorm`, `phpstorm`, `rubymine`, `goland`, `datagrip`,
      `eclipse`, `netbeans`, `codeblocks`, `anjuta`, `glade`, `lazarus`,
      `gambas3`, `thonny`, `idle3`, `spyder`, `jupyter`,
      `jupyter-notebook`, `jupyterlab`, `rstudio`, `bluej`, `processing`,
      `arduino`, `arduino-ide`, `platformio`.
- [ ] **Version control and diff tools**: `git`, `gitg`, `gitkraken`,
      `gitui`, `lazygit`, `tig`, `git-cola`, `meld`, `kdiff3`, `kompare`,
      `diffuse`, `meld`, `subversion`, `mercurial`, `fossil`, `cvs`,
      `bzr`, `repo`.
- [ ] **Build tools, compilers and debuggers**: `gcc`, `clang`, `make`,
      `cmake`, `meson`, `ninja`, `scons`, `autotools`, `autoconf`,
      `automake`, `libtool`, `pkg-config`, `gdb`, `lldb`, `valgrind`,
      `heaptrack`, `perf`, `strace`, `ltrace`, `objdump`, `readelf`,
      `cppcheck`, `clang-tidy`, `clang-format`, `doxygen`, `sphinx`,
      `mkdocs`, `pandoc`, `asciidoctor`, `kcachegrind`, `kdbg`.
- [ ] **Languages and runtimes**: `python`, `python3`, `idle`, `pip`,
      `pipx`, `poetry`, `virtualenv`, `conda`, `anaconda`, `mamba`,
      `jupyter`, `node`, `npm`, `pnpm`, `yarn`, `deno`, `bun`,
      `typescript`, `go`, `goland`, `rustc`, `cargo`, `rustup`,
      `java`, `openjdk`, `gradle`, `maven`, `ant`, `kotlin`, `scala`,
      `sbt`, `groovy`, `mono`, `dotnet`, `php`, `composer`, `ruby`,
      `gem`, `rails`, `perl`, `cpan`, `lua`, `luajit`, `tclsh`, `wish`,
      `julia`, `octave`, `scilab`, `gnuplot`, `wxmaxima`, `R`, `rstudio`,
      `haskell`, `ghc`, `cabal`, `stack`, `ocaml`, `opam`, `dune`,
      `swift`, `zig`, `nim`, `crystal`, `dart`, `flutter`, `elixir`,
      `erlang`, `clojure`, `lein`, `racket`, `scheme`, `guile`,
      `fortran`, `gfortran`, `gnat`, `ada`, `cobol`, `gnucobol`.
- [ ] **Databases and administration**: `dbeaver`, `pgadmin`,
      `mysql-workbench`, `sqlitebrowser`, `sqliteman`, `sqlite`,
      `postgresql`, `mysql`, `mariadb`, `mongodb`, `redis`, `influxdb`,
      `adminer`, `phpmyadmin`, `kexi`, `libreoffice-base`, `sqlectron`.
- [ ] **Terminals and shells**: `gnome-terminal`, `kgx`, `konsole`,
      `xterm`, `urxvt`, `rxvt`, `alacritty`, `kitty`, `wezterm`, `foot`,
      `footclient`, `tilix`, `terminator`, `xfce4-terminal`,
      `mate-terminal`, `deepin-terminal`, `lxterminal`, `qterminal`,
      `st`, `guake`, `yakuake`, `terminology`, `hyper`, `tabby`,
      `tmux`, `screen`, `byobu`, `zellij`, `bash`, `zsh`, `fish`,
      `nushell`, `elvish`, `xonsh`, `powershell`, `pwsh`, `starship`.
- [ ] **File managers and terminal file managers**: `nautilus`, `dolphin`,
      `thunar`, `pcmanfm`, `pcmanfm-qt`, `nemo`, `caja`, `rox-filer`,
      `spacefm`, `krusader`, `sunflower`, `doublecmd`, `ranger`, `yazi`,
      `lf`, `nnn`, `vifm`, `mc`, `joshuto`, `broot`, `xplr`, `felix`,
      `clifm`, `kfind`, `catfish`, `recoll`, `angrysearch`, `fsearch`.
- [ ] **Archivers and disk tools**: `file-roller`, `ark`, `xarchiver`,
      `engrampa`, `peazip`, `squeeze`, `7z`, `p7zip`, `unzip`, `zip`,
      `gparted`, `gnome-disks`, `kde-partitionmanager`, `partitionmanager`,
      `qt-fsarchiver`, `gparted`, `baobab`, `filelight`, `qdirstat`,
      `ncdu`, `duf`, `dust`, `gdu`, `dupeguru`, `fdupes`, `jdupes`,
      `czkawka`, `rmlint`, `fslint`.
- [ ] **System monitors and hardware**: `gnome-system-monitor`,
      `ksysguard`, `plasma-systemmonitor`, `htop`, `btop`, `bashtop`,
      `bpytop`, `glances`, `nmon`, `iotop`, `iftop`, `nethogs`, `lsof`,
      `stacer`, `hardinfo`, `cpu-x`, `inxi`, `neofetch`, `fastfetch`,
      `screenfetch`, `conky`, `gkrellm`, `psensor`, `xsensors`,
      `lm-sensors`, `solaar`, `piper`, `openrgb`, `polychromatic`,
      `input-remapper`, `antimicrox`, `key-mapper`, `liquidctl`,
      `coolero`, `jstest-gtk`, `evtest`, `xinput`, `wacom`.
- [ ] **Package managers and software centres**: `gnome-software`,
      `plasma-discover`, `discover`, `pamac-manager`, `synaptic`,
      `gnome-packagekit`, `apper`, `muon`, `bauh`, `octopi`, `aptitude`,
      `apt`, `dpkg`, `dnf`, `yumex`, `zypper`, `yast`, `pacman`, `yay`,
      `paru`, `pamac`, `flatpak`, `flatseal`, `snap`, `snap-store`,
      `nix`, `guix`, `brew`, `emerge`, `portage`, `apt-notifier`,
      `software-properties-gtk`, `software-properties-kde`, `add-apt-repository`.
- [ ] **Virtualisation, containers and emulation**: `virtualbox`,
      `virt-manager`, `gnome-boxes`, `qemu`, `virt-viewer`, `libvirt`,
      `vagrant`, `docker`, `docker-compose`, `podman`, `podman-desktop`,
      `distrobox`, `toolbox`, `lxc`, `lxd`, `incus`, `multipass`,
      `kubernetes`, `kubectl`, `helm`, `k9s`, `minikube`, `lens`,
      `vmware-workstation`, `wine`, `winecfg`, `winetricks`, `proton`,
      `protonup-qt`, `bottles`, `playonlinux`, `dosbox`, `dosbox-x`,
      `scummvm`, `retroarch`, `mednafen`, `mupen64plus`, `pcsx2`,
      `rpcs3`, `dolphin-emu`, `ppsspp`, `yuzu`, `ryujinx`, `citra`,
      `cemu`, `duckstation`, `hatari`, `vice`, `fs-uae`, `snes9x`,
      `desmume`, `melonds`, `mgba`, `visualboyadvance`.
- [ ] **Games and launchers**: `steam`, `lutris`, `heroic`, `gamehub`,
      `itch`, `minigalaxy`, `gog-galaxy`, `minecraft`, `minetest`,
      `luanti`, `0ad`, `supertuxkart`, `supertux`, `wesnoth`,
      `openttd`, `xonotic`, `teeworlds`, `sauerbraten`, `freedoom`,
      `openra`, `warzone2100`, `veloren`, `endless-sky`, `gnome-games`,
      `gnome-chess`, `gnome-sudoku`, `gnome-mines`, `gnome-robots`,
      `gnome-nibbles`, `gnome-taquin`, `gnome-tetravex`, `gnome-2048`,
      `gnome-mahjongg`, `gnome-klotski`, `aisleriot`, `quadrapassel`,
      `five-or-more`, `four-in-a-row`, `iagno`, `lightsoff`,
      `swell-foop`, `atomix`, `gnome-control-center-games`.
- [ ] **Networking and remote access**: `nm-connection-editor`,
      `nm-applet`, `nm-tray`, `networkmanager`, `connman-gtk`,
      `system-config-network`, `wpa_gui`, `iwd`, `blueman`,
      `blueman-manager`, `gnome-bluetooth`, `remmina`, `vinagre`,
      `freerdp`, `xfreerdp`, `krdc`, `krfb`, `tigervnc`, `x11vnc`,
      `novnc`, `rustdesk`, `anydesk`, `teamviewer`, `nomachine`, `x2go`,
      `barrier`, `deskflow`, `syncthing`, `rclone`, `winscp`, `filezilla`,
      `gftp`, `curl`, `wget`, `aria2`, `transmission`, `qbittorrent`,
      `deluge`, `ktorrent`, `fragments`, `rtorrent`, `openvpn`,
      `wireguard`, `wg-quick`, `tailscale`, `zerotier`, `openssh`,
      `sshfs`, `mosh`, `putty`, `moserial`, `minicom`, `cutecom`.
- [ ] **Security and privacy**: `firejail`, `gufw`, `firewall-config`,
      `firewalld`, `ufw`, `iptables`, `nftables`, `clamtk`, `clamav`,
      `rkhunter`, `chkrootkit`, `lynis`, `wireshark`, `tshark`, `nmap`,
      `zenmap`, `tcpdump`, `mitmproxy`, `burpsuite`, `zaproxy`, `john`,
      `hashcat`, `aircrack-ng`, `kismet`, `metasploit`, `ettercap`,
      `bettercap`, `sqlmap`, `tor`, `torsocks`, `onionshare`, `nyx`,
      `keepassxc`, `keepass2`, `bitwarden`, `1password`, `pass`,
      `gopass`, `seahorse`, `gnome-keyring`, `kwalletmanager`,
      `kleopatra`, `kgpg`, `gpa`, `gnupg`, `age`, `sops`, `veracrypt`,
      `cryptomator`, `zulucrypt`, `encfs`, `gocryptfs`.
- [ ] **Accessibility**: `orca`, `espeak`, `espeak-ng`, `festival`,
      `flite`, `speech-dispatcher`, `brltty`, `accerciser`, `caribou`,
      `onboard`, `florence`, `mousetweaks`, `dasher`, `kmag`,
      `kmousetool`, `kmouth`, `jovie`, `simon`, `at-spi`,
      `gnome-shell-magnifier`, `compiz-magnifier`.
- [ ] **Printing and scanning**: `system-config-printer`, `printers`,
      `cups`, `hp-setup`, `hplip`, `hp-toolbox`, `skanlite`, `skanpage`,
      `xsane`, `simple-scan`, `gscan2pdf`, `tesseract`, `gimagereader`,
      `ocrmypdf`, `pdfarranger`.
- [ ] **Fonts and typography**: `fontforge`, `font-manager`,
      `gnome-font-viewer`, `kfontview`, `birdfont`, `fontmatrix`,
      `typecatcher`, `gucharmap`, `gnome-characters`, `kcharselect`,
      `fc-list`, `fc-cache`.
- [ ] **Power, thermals and laptops**: `upower`, `power-profiles-daemon`,
      `auto-cpufreq`, `tlp`, `tlpui`, `powertop`, `cpupower-gui`,
      `thermald`, `laptop-mode-tools`, `system76-power`,
      `slimbookbattery`, `xfce4-power-manager`, `mate-power-manager`,
      `gnome-power-statistics`.
- [ ] **Input method and keyboard**: `fcitx`, `fcitx5`,
      `fcitx5-configtool`, `ibus`, `ibus-setup`, `scim`, `uim`, `mozc`,
      `anthy`, `chewing`, `hime`, `gcin`, `keyd`, `xmodmap`,
      `setxkbmap`, `keyboard-configuration`, `xev`, `xinput`.
- [ ] **Displays, colour and appearance tools**: `arandr`, `lxrandr`,
      `xrandr`, `nvidia-settings`, `wdisplays`, `wlr-randr`, `kanshi`,
      `autorandr`, `redshift`, `gammastep`, `wlsunset`, `brightnessctl`,
      `light`, `xbacklight`, `lxappearance`, `gnome-tweaks`,
      `qt5ct`, `qt6ct`, `kvantummanager`, `nwg-look`, `dconf-editor`,
      `gsettings-editor`, `galternatives`, `caffeine`, `clipman`,
      `parcellite`, `qlipper`, `copyq`, `clipit`.
- [ ] **USB, boot and imaging**: `unetbootin`, `balena-etcher`, `etcher`,
      `usb-creator`, `startup-disk-creator`, `gnome-multi-writer`,
      `ventoy`, `woeusb`, `popsicle`, `fedora-media-writer`,
      `suse-studio`, `imagewriter`, `dd`, `clonezilla`, `timeshift`,
      `deja-dup`, `backintime`, `borg`, `vorta`, `duplicity`,
      `rsnapshot`, `backup-tool`, `kbackup`, `luckybackup`, `grsync`.

---
## 9. Phase 5 — filling the gaps in the contexts that already exist

These contexts are populated, but not completely. The list below is what a
comparison against the Icon Naming Specification and against the names the
toolkits ask for in practice turns up.

- [ ] **Actions** — the GTK stock ids every older application still asks for:
      `gtk-about`, `gtk-add`, `gtk-apply`, `gtk-bold`, `gtk-cancel`,
      `gtk-cdrom`, `gtk-clear`, `gtk-close`, `gtk-color-picker`,
      `gtk-connect`, `gtk-convert`, `gtk-copy`, `gtk-cut`, `gtk-delete`,
      `gtk-dialog-authentication`, `gtk-dialog-error`, `gtk-dialog-info`,
      `gtk-dialog-question`, `gtk-dialog-warning`, `gtk-directory`,
      `gtk-disconnect`, `gtk-edit`, `gtk-execute`, `gtk-file`, `gtk-find`,
      `gtk-find-and-replace`, `gtk-floppy`, `gtk-fullscreen`,
      `gtk-goto-bottom`, `gtk-goto-first`, `gtk-goto-last`,
      `gtk-goto-top`, `gtk-go-back`, `gtk-go-down`, `gtk-go-forward`,
      `gtk-go-up`, `gtk-harddisk`, `gtk-help`, `gtk-home`, `gtk-index`,
      `gtk-info`, `gtk-italic`, `gtk-jump-to`, `gtk-justify-center`,
      `gtk-justify-fill`, `gtk-justify-left`, `gtk-justify-right`,
      `gtk-leave-fullscreen`, `gtk-media-forward`, `gtk-media-next`,
      `gtk-media-pause`, `gtk-media-play`, `gtk-media-previous`,
      `gtk-media-record`, `gtk-media-rewind`, `gtk-media-stop`,
      `gtk-missing-image`, `gtk-network`, `gtk-new`, `gtk-no`,
      `gtk-ok`, `gtk-open`, `gtk-paste`, `gtk-preferences`,
      `gtk-print-error`, `gtk-print-paused`, `gtk-print-preview`,
      `gtk-print-report`, `gtk-print-warning`, `gtk-properties`,
      `gtk-quit`, `gtk-redo`, `gtk-refresh`, `gtk-remove`,
      `gtk-revert-to-saved`, `gtk-save`, `gtk-save-as`, `gtk-select-all`,
      `gtk-select-color`, `gtk-select-font`, `gtk-sort-ascending`,
      `gtk-sort-descending`, `gtk-spell-check`, `gtk-stop`, `gtk-strikethrough`,
      `gtk-underline`, `gtk-undo`, `gtk-unselect-all`, `gtk-yes`,
      `gtk-zoom-100`, `gtk-zoom-fit`, `gtk-zoom-in`, `gtk-zoom-out`.
- [ ] **Actions**, continued: the remaining documented names —
      `bookmark_add`, `contact-new`, `document-export`, `document-import`,
      `document-open-remote`, `document-print-direct`,
      `document-print-preview`, `edit-select-region`, `folder-copy`,
      `folder-move`, `format-indent-less`, `media-eject`, `media-record`,
      `media-playlist-consecutive`, `media-playlist-no-repeat`,
      `media-playlist-repeat-song`, `media-playlist-shuffle`,
      `object-flip-horizontal`, `object-flip-vertical`, `object-group`,
      `object-ungroup`, `object-order-back`, `object-order-front`,
      `object-rotate-left`, `object-rotate-right`, `object-straighten`,
      `selection-end`, `selection-start`, `system-search`,
      `system-software-update`, `system-upgrade`, `tab-new`,
      `tools-check-spelling`, `user-info`, `view-conceal`,
      `view-reveal`, `view-sort-ascending`, `view-sort-descending`,
      `window-new`.
- [ ] **Devices**: `drive-harddisk-solidstate`, `drive-harddisk-usb`,
      `drive-harddisk-ieee1394`, `drive-harddisk-system`,
      `drive-multidisk`, `drive-removable-media-usb-pendrive`,
      `drive-removable-media-ieee1394`, `drive-removable-media-flash`,
      `media-cdrom-audio`, `media-cdrom-data`, `media-dvd`,
      `media-optical-bd`, `media-optical-dvd`, `media-optical-cd-audio`,
      `media-optical-cd-video`, `media-tape`, `media-zip`, `media-jaz`,
      `media-mo`, `media-removable-zip`, `media-removable-optical`,
      `media-removable-media`, `audio-card`, `audio-headset`,
      `audio-input-microphone`, `audio-speakers-bluetooth`,
      `camera-photo-burst`, `camera-video`, `camera-web`, `phone-pda`,
      `pda`, `ipod`, `multimedia-player`, `video-projector`,
      `video-display-symbolic`, `input-gaming`, `input-tablet`,
      `input-dialpad`, `input-keyboard-virtual`, `modem`, `ups`,
      `network-wireless-router`, `network-router`, `printer-network`,
      `scanner`, `smartcard`, `usb`, `usb-drive`, `usb-hub`,
      `computer-laptop`, `computer-tablet`, `computer-server`,
      `computer-workstation`.
- [ ] **Places**: `folder-documents`, `folder-downloads`,
      `folder-music`, `folder-pictures`, `folder-videos`,
      `folder-publicshare`, `folder-templates`, `folder-saved-search`,
      `folder-visiting`, `folder-drag-accept`, `folder-remote`,
      `folder-network`, `folder-burn`, `folder-recent`,
      `user-bookmarks`, `user-home`, `user-desktop`, `user-trash`,
      `user-trash-full`, `trash-empty`, `network-server`,
      `network-workgroup`, `network-server-symbolic`, `recent`,
      `file-manager`, `applications-internet`, `applications-other`,
      `applications-system`, `desktop`, `home`, `library-audio`,
      `library-books`, `library-music`, `library-pictures`,
      `library-videos`, `library-software`, `library-places`.
- [ ] **Status**: the whole battery ladder — `battery-missing`,
      `battery-empty`, `battery-caution`, `battery-low`, `battery-good`,
      `battery-full`, `battery-charging`, `battery-level-0` … `battery-level-100`
      in steps of ten, `battery-000` … `battery-100` in steps of ten.
- [ ] **Status**: the NetworkManager ladder — `nm-device-wired`,
      `nm-device-wired-secure`, `nm-device-wireless`,
      `nm-device-wireless-secure`, `nm-device-wwan`, `nm-device-bt`,
      `nm-signal-00` … `nm-signal-100` in steps of 25,
      `nm-signal-weak`, `nm-signal-ok`, `nm-signal-good`, `nm-signal-excellent`,
      `nm-vpn-active-lock`, `nm-vpn-connecting01` … `nm-vpn-connecting12`,
      `nm-vpn-offline`, `nm-vpn-standalone-lock`, `nm-secure-lock`,
      `nm-no-connection`, `nm-adhoc`, `nm-tech-3g`, `nm-tech-4g`,
      `nm-tech-5g`, `nm-tech-gprs`, `nm-tech-edge`, `nm-tech-hspa`,
      `nm-tech-lte`, `nm-tech-umts`.
- [ ] **Status**, continued: `network-wireless-signal-none`,
      `-weak`, `-ok`, `-good`, `-excellent`, `network-wireless-offline`,
      `network-wireless-disconnected`, `network-wired-offline`,
      `network-wired-disconnected`, `network-error`, `network-offline`,
      `network-transmit`, `network-receive`, `network-transmit-receive`,
      `network-idle`, `bluetooth-paired`, `bluetooth-disabled`,
      `bluetooth-active`, `audio-volume-muted-blocking`,
      `audio-volume-muted-symbolic`, `microphone-sensitivity-muted`,
      `microphone-sensitivity-low`, `microphone-sensitivity-medium`,
      `microphone-sensitivity-high`, `user-available`,
      `user-away-extended`, `user-invisible`, `user-idle`,
      `user-status-pending`, `avatar-default-symbolic`,
      `dialog-password`, `dialog-error-symbolic`, `dialog-information-symbolic`,
      `dialog-question-symbolic`, `dialog-warning-symbolic`,
      `security-high`, `security-medium`, `security-low`,
      `software-update-available`, `software-update-urgent`,
      `software-update-available-symbolic`, `package-available`,
      `package-installed-updated`, `package-installed-outdated`,
      `package-broken`, `package-downgrade`, `package-new`,
      `package-purge`, `package-reinstall`, `package-supported`,
      `package-upgrade`, `package-x-generic-symbolic`,
      `printer-error`, `printer-printing`, `printer-warning`,
      `printer-network-error`, `image-loading`, `image-missing`,
      `checkbox-checked`, `checkbox-mixed`, `checkbox-unchecked`,
      `radio-checked`, `radio-mixed`, `radio-unchecked`,
      `starred`, `non-starred`, `semi-starred`,
      `weather-clear`, `weather-clear-night`, `weather-clouds`,
      `weather-few-clouds`, `weather-fog`, `weather-overcast`,
      `weather-showers`, `weather-showers-scattered`, `weather-snow`,
      `weather-storm`, `weather-severe-alert`,
      `alarm`, `alarm-symbolic`, `appointment-missed`,
      `appointment-soon`, `task-due`, `task-past-due`.
- [ ] **MimeTypes**, gap fill: `application-x-appliance`,
      `application-x-cd-image`, `application-x-cue`,
      `application-x-raw-disk-image`, `application-x-appimage`,
      `application-x-arj`, `application-x-cpio`,
      `application-x-lha`, `application-x-lzma`,
      `application-x-lz4`, `application-x-zstd`,
      `application-x-xz`, `application-x-sqlite3`,
      `application-x-sqlite2`, `application-vnd.ms-access`,
      `application-x-dbase`, `application-x-dvi`,
      `application-x-tex`, `application-x-bibtex`,
      `application-x-matroska`, `application-x-ogg`,
      `application-x-flac`, `application-x-wav`,
      `application-x-shockwave-flash`, `application-x-java-archive`,
      `application-x-msdownload`, `application-x-msi`,
      `application-x-wine-extension-*` (the per-extension family),
      `text-x-texinfo`, `text-x-troff`, `text-x-adasrc`,
      `text-x-gettext-translation`, `text-x-lilypond`,
      `text-x-lua`, `text-x-pascal`, `text-x-diff`, `text-x-patch`,
      `text-x-javascript`, `text-x-typescript`, `text-x-actionscript`,
      `text-x-sql`, `text-x-qt`, `text-x-objcsrc`,
      `text-x-rust`, `text-x-go`, `text-x-swift`, `text-x-kotlin`,
      `text-x-haskell`, `text-x-erlang`, `text-x-elixir`,
      `text-x-scheme`, `text-x-lisp`, `text-x-clojure`,
      `text-x-csharp`, `text-x-fortran`, `text-x-matlab`,
      `text-x-r`, `text-x-octave`, `text-x-vhdl`, `text-x-verilog`,
      `x-office-address-book`, `x-office-calendar`, `x-office-database`,
      `x-office-document-template`, `x-office-presentation-template`,
      `x-office-spreadsheet-template`, `application-x-kcsrc`,
      `application-vnd.oasis.opendocument.*` for every variant.
- [ ] **Categories**: the remainder of the menu specification —
      `X-GNOME-Settings-Panel`, `X-GNOME-Utilities`,
      `X-GNOME-Internet`, `X-GNOME-Office`, `X-GNOME-Media`,
      `X-GNOME-System`, `X-GNOME-Other`, `X-KDE-More`,
      `X-KDE-information`, `X-KDE-Utilities-*`, `X-KDE-Settings-*`,
      `Screensaver`, `TrayIcon`, `Applet`, `Shell`, `Core`, `ConsoleOnly`,
      `Building`, `Debugger`, `IDE`, `GUIDesigner`, `Profiling`,
      `RevisionControl`, `Translation`, `Calendar`, `ContactManagement`,
      `Database`, `Dictionary`, `Chart`, `FlowChart`, `PDA`,
      `ProjectManagement`, `Presentation`, `Spreadsheet`,
      `WordProcessor`, `Publishing`, `Viewer`, `TextTools`,
      `DesktopSettings`, `HardwareSettings`, `PackageManager`,
      `Security`, `Accessibility`, `FileTools`, `FileTransfer`,
      `Compression`, `Electronics`, `Engineering`, `Physics`,
      `Chemistry`, `Math`, `Biology`, `Geography`, `Geology`,
      `Astronomy`, `Humanities`, `Art`, `Languages`.
- [ ] **Applications**: `preferences-desktop-remote-desktop`,
      `preferences-system-*` for the remaining names,
      `applications-*` for the remaining categories, `app-*` tiles for
      the letters and digits that no launcher name references yet, and
      `application-x-generic` for the launcher fallback.

---

## 10. Phase 6 — sizes, and how the tree is written

- [ ] Write every icon at 8, 12, 16, 22, 24, 32, 36 and 48 px as PNG.
- [ ] Write every icon at 64, 96, 128, 192, 256 and 512 px as SVG.
- [ ] Write every icon once under a root `scalable/<Context>` directory, which
      is where GTK 4 and Qt 6 look first.
- [ ] Keep the `<size>/<Context>/scalable` directories as well, for the older
      toolkit lookup that still walks the size ladder.
- [ ] Write `symbolic/<Context>/<name>-symbolic.svg` for every name in a
      context that carries symbolic variants.
- [ ] Confirm `index.theme` lists every directory that exists, and no directory
      that does not — `--check` compares the two.

## 11. Phase 7 — verification

Nothing above counts as done until these pass.

- [ ] `python dotfile/ICON_THEME/icon_install.py --check` reports no problems.
- [ ] `unknown_glyphs()`, `duplicate_names()` and `unusable_names()` are empty.
- [ ] The audit script reports every context populated, and no name registered
      twice inside one context.
- [ ] `python dotfile/ICON_THEME/icon_install.py --build-only` completes and
      writes a tree whose `index.theme` `Directories=` line matches the
      directories on disk.
- [ ] Contact sheets are written for each family — panels, emblems, emotes,
      flags, UI, programs — and opened, so the set is judged at 16 px as well as
      at 128 px.
- [ ] A spot check in a real desktop: `lxappearance` lists the theme, a GTK file
      manager shows the themed icons, and a GTK 4 header bar shows the `UI`
      names rather than falling back.
- [ ] The installed tree is byte-identical to the built tree
      (`python dotfile/ICON_THEME/icon_install.py` then a diff).

---

## 12. Decisions and things deliberately out of scope

- **No per-program logo art.** A theme cannot draw Firefox's fox or GIMP's
  Wilber and stay honest about it; programs that ship an icon keep theirs, and
  what this theme adds is the *fallback* for the names a launcher asks for.
  Aliases point at a category pictogram, or at a lettered plate when even that
  would say the wrong thing.
- **No GIF for every animation.** Only `process-working` is animated, because it
  is the one animation a desktop asks for by name. The writer is generic, so a
  second one is a few lines, but guessing at frame counts for animations nothing
  requests would be noise.
- **`-symbolic` only where GTK recolours.** GTK asks for symbolic variants of
  actions, applications, categories, devices, emblems, places, status and UI
  names. It does not ask for them for mime types, emotes, flags or animations,
  so those are not generated.
- **Flags are generated from a colour table, not traced.** A flag that needs a
  coat of arms is approximated by its bands, which is what reads at 16 px anyway.
- **The `hicolor` fallback stays.** `Inherits=Adwaita,hicolor` means a name this
  theme still misses resolves to something rather than to a blank square. The
  point of this plan is to make that fallback rare, not to remove it.

---

## 13. Where the names actually live

This file is the plan; the tables are the implementation. When a name here is
added, it is added to exactly one of these modules:

| Module | Contexts it feeds |
| --- | --- |
| `icons/catalogue.py` | Actions, Places, Devices, Status |
| `icons/catalogue_files.py` | MimeTypes, Categories, Applications |
| `icons/catalogue_xfce.py` | XFCE panels, plugins, programs |
| `icons/catalogue_desktops.py` | GNOME, KDE, MATE, LXQt, Cinnamon, Budgie, Pantheon, Deepin, WMs |
| `icons/catalogue_programs.py` | programs by domain |
| `icons/catalogue_extra.py` | Emblems, Emotes, International, UI, Animations, GTK stock |

and the pictures they draw are in:

| Module | What it draws |
| --- | --- |
| `icons/glyphs_actions.py` | the action vocabulary |
| `icons/glyphs_apps.py` | application plates and category pictograms |
| `icons/glyphs_extra.py` | the glyphs that belong to no single family |
| `icons/glyphs_hardware.py` | computers, drives, input devices |
| `icons/glyphs_letters.py` | the stroke alphabet |
| `icons/glyphs_mime.py` | file types |
| `icons/glyphs_objects.py` | folders, documents, media objects |
| `icons/glyphs_places.py` | home, desktop, trash, network places |
| `icons/glyphs_status.py` | batteries, signal, presence, emblems |
| `icons/glyphs_panels.py` | settings panels and panel applets |
| `icons/glyphs_emblems.py` | overlay badges |
| `icons/glyphs_emotes.py` | faces |
| `icons/glyphs_flags.py` | generated flags |
| `icons/glyphs_ui.py` | widget affordances, spinner frames |

---

## 14. Commands

    # build only, and time it
    python dotfile/ICON_THEME/icon_install.py --build-only

    # audit: contexts, duplicates, unknown glyphs, counts
    python _temp/icon_audit.py

    # every name, one per line
    python dotfile/ICON_THEME/icon_install.py --list

    # a contact sheet per family, to look at rather than to trust
    PYTHONPATH=dotfile/ICON_THEME python -m icons.sheet --size 16 --out /tmp/sheet16.svg
    PYTHONPATH=dotfile/ICON_THEME python -m icons.sheet --size 128 --out /tmp/sheet128.svg

    # install into ~/.local/share/icons and select it
    python dotfile/ICON_THEME/icon_install.py
