#!/usr/bin/env python3
# =============================================================================
# GnuchanPurple - LXDM installer and greeter theme
# -----------------------------------------------------------------------------
# Installs the LXDM display manager and configures it to greet with the
# GnuchanPurple theme: the wallpaper from assets/bg_my_games.png, the GnuchanOS
# logo from assets/logo.png as the account avatar, and a dark purple login box
# in the middle of the screen. Standard library only, and no options: running
#
#     python3 settings_lxdm.py
#
# is the whole install.
#
# What it does
# ------------
#   1. installs lxdm and an X server with the distribution's package manager -
#      apt on Debian and Ubuntu, pacman on Arch, dnf or zypper elsewhere - and
#      reports the exact command when it cannot run one;
#   2. builds the theme in /usr/share/lxdm/themes/GnuchanPurple from the
#      committed files next to this script plus three images generated here
#      from the repository's assets: the wallpaper, the logo on the login box,
#      and the avatar shown for every account that has no ~/.face of its own;
#   3. merges the greeter settings into /etc/lxdm/lxdm.conf, touching only the
#      keys this theme owns - the theme, the wallpaper, the panel switches, the
#      clock format, the language and keyboard choosers, the GTK theme, the
#      greeter binary and the account list - and keeping every comment, blank
#      line and unrelated key of the file it finds. The greeter binary is the
#      one key that is never left out of the file: lxdm starts no greeter at all
#      when it is missing or empty, so the X server it started for the greeter
#      is a black screen with nothing drawn on it while the text login on tty1
#      stays where it was - which reads as a display manager that did not start.
#      What is written is the installed greeter when there is one, and the path
#      this distribution is expected to install it to when there is not, so the
#      key cannot be missing after a run;
#   4. copies the GnuchanPurple GTK theme from dotfile/GTK_THEME to
#      /usr/share/themes, where the desktop can find it, because a theme
#      installed in a user's home directory is not visible to a process that
#      runs, as the greeter does, as the lxdm account or as root before anyone
#      has logged in. The greeter is deliberately NOT pointed at it: the
#      [display] gtk_theme key is left exactly as the distribution set it. That
#      key selects the GTK theme for the greeter's own widgets, and it was
#      working before this script ran - a GTK theme the greeter cannot load is
#      a greeter that does not come up at all, which is the whole thing this
#      script exists to avoid. The palette is carried by the theme's own gtkrc
#      and gtk.css, which do not depend on it;
#   5. makes lxdm the display manager: writes the Debian default display
#      manager file, which lxdm's own systemd unit checks before it will start,
#      disables any other display manager that was enabled so that two of them
#      are never fighting over the screen, points systemd's
#      display-manager.service alias at lxdm, which is how a unit with no
#      [Install] section is made to start at boot at all, and makes the machine
#      boot into graphical.target. That last one is not decoration: a display
#      manager is started by graphical.target and by nothing else, so a machine
#      whose default target is still multi-user.target has lxdm installed,
#      enabled, aliased and unreachable, and boots to the text login on tty1 -
#      which is what a display manager that does not start looks like. The
#      target that was there is kept beside the one that replaced it;
#   6. checks the result and says what is wrong if anything is: the theme files
#      are parsed, both greeter interfaces are checked for every widget id
#      greeter.c looks up by name, the generated images are decoded, the
#      configuration is read back the way the greeter reads it, and the display
#      manager itself is asked about - the file lxdm's own unit reads before it
#      will start, the default target, the display-manager.service alias -
#      because every one of those being wrong ends in the same place, the text
#      login on tty1, and a run that installs a display manager which cannot
#      start is the run that should have said so.
#
# At the end it starts lxdm, rather than promising it for the next reboot. A
# display manager that cannot come up is a black screen and a login on tty1 with
# nothing to read, and a reboot is a slow way to find that out: starting it here
# either puts the greeter on the screen immediately or leaves lxdm's own account
# of what it did - /var/log/lxdm.log, which is also where the greeter's stderr
# goes - on the terminal, naming whichever of the X server and the greeter
# failed. Nothing is stopped afterwards: the greeter on the screen is the result
# that was asked for.
#
# The two greeters
# ----------------
# An lxdm theme is a GtkBuilder interface plus a stylesheet, not an HTML page,
# and which pair of files is read depends on how lxdm was built: GTK+ 3
# (greeter-gtk3.ui and gtk.css) only when it was configured with --enable-gtk3,
# and GTK+ 2 (greeter.ui and gtkrc) otherwise, which is the default and what
# Debian and Ubuntu ship. Both pairs are installed here, and they are the same
# theme twice; see GnuchanPurple/gtkrc for the details that differ.
#
# Undo
# ----
# There are no flags, so undoing is by hand and always possible. Every file this
# script rewrote is kept, and every one has to be put back:
#
#     rm -rf /usr/share/lxdm/themes/GnuchanPurple
#     cp /etc/lxdm/lxdm.conf.gnuchan-backup /etc/lxdm/lxdm.conf
#     cp /etc/X11/default-display-manager.gnuchan-backup /etc/X11/default-display-manager
#     mv /etc/systemd/system/default.target.gnuchan-backup /etc/systemd/system/default.target
#
# The last one is a symlink and is put back by naming it again rather than by
# copying over it.
#
# and then enable the display manager that was enabled before, whose name the
# script prints while it runs. The last file is the one worth remembering: the
# manager being replaced reads it about itself and refuses to start when it
# names lxdm, so restoring lxdm's own configuration is not enough on its own.
#
# The palette is the one the GTK theme, the icon theme and VSCodium use.
# dotfile/vscodium_theme/settings.json is the source of truth for it; change it
# there first, then the colours in GnuchanPurple/gtkrc and GnuchanPurple/
# gtk.css and the tables at the top of this file.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ElementTree
import zlib
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve()
SCRIPT_DIR = SCRIPT_PATH.parent

#: The theme name is the directory name, as every LXDM theme's is.
THEME_NAME = "GnuchanPurple"

#: The theme as it is committed, next to this script.
SOURCE_THEME_DIR = SCRIPT_DIR / THEME_NAME

#: Where lxdm looks for themes. LXDM_DATA_DIR in the greeter's Makefile.am is
#: $(datadir)/lxdm, and the Debian package installs the same tree, so this is
#: /usr/share/lxdm/themes on every distribution that packages lxdm.
LXDM_DATA_DIR = Path("/usr/share/lxdm")
LXDM_THEMES_DIR = LXDM_DATA_DIR / "themes"
INSTALLED_THEME_DIR = LXDM_THEMES_DIR / THEME_NAME

#: The greeter's configuration, and the file the Debian package and the systemd
#: unit use to decide which display manager starts.
CONFIG_FILE = Path("/etc/lxdm/lxdm.conf")
DEFAULT_DM_FILE = Path("/etc/X11/default-display-manager")
LXDM_DAEMON = Path("/usr/sbin/lxdm")

#: What systemd starts when graphical.target comes up. lxdm's own unit has no
#: [Install] section - upstream never wrote one - so systemctl cannot enable it
#: at all, and this alias is the only thing that makes it the display manager.
DISPLAY_MANAGER_ALIAS = Path("/etc/systemd/system/display-manager.service")

BACKUP_SUFFIX = ".gnuchan-backup"

#: The GTK theme, which lives in a sibling directory of this one in the
#: repository and is installed system wide for the greeter's own widgets.
GTK_THEME_SOURCE = SCRIPT_DIR.parent / "GTK_THEME" / THEME_NAME
GTK_THEME_DIRS = (Path("/usr/share/themes"), Path("/usr/local/share/themes"))

#: The images this script generates into the theme from the repository assets.
BACKGROUND_FILE = "bg.png"
LOGIN_IMAGE_FILE = "login.png"
AVATAR_FILE = "nobody.png"

#: The names those assets have in the repository.
BACKGROUND_ASSET = "bg_my_games.png"
LOGO_ASSET = "logo.png"

#: Sizes the two copies of the logo are scaled to. The login box image is the
#: larger of the two because it sits alone above the prompt; the avatar is drawn
#: by the greeter at 48x48 and this only keeps it sharp on a high density
#: screen.
LOGIN_IMAGE_SIZE = 168
AVATAR_SIZE = 96

#: Where a pixbuf path in the interfaces is made absolute at install time.
THEME_DIR_PLACEHOLDER = "@THEME_DIR@"

#: Replaced on every run, so running the script twice changes nothing.
MANAGED_MARKER = "# GnuchanPurple greeter settings, written by settings_lxdm.py"

#: Every widget id the greeter looks up by name, and the classes each one may
#: have. greeter.c calls gtk_builder_get_object with these strings and for most
#: of them casts the result without checking it: a missing login_entry, prompt,
#: bottom_pane or exit takes the greeter down, a user_list that is not a
#: GtkIconView is one the account list cannot be loaded into, and a sessions or
#: lang that is not a combo box is one the session and language models have
#: nowhere to go. time is the only one the greeter tests for NULL. sessions_box,
#: lang_box and label_keyboard are looked up in order to hide a chooser, and
#: keyboard both there and in the screen size handler, which hides it without
#: checking for NULL - so all four are required here with the rest.
#:
#: The combo boxes are the one pair that differs between the toolkits: GTK+ 2
#: has GtkComboBoxEntry, GTK+ 3 a GtkComboBox told to carry an entry.
REQUIRED_WIDGETS: dict[str, tuple[str, ...]] = {
    "lxdm": ("GtkWindow",),
    "time": ("GtkLabel",),
    "prompt": ("GtkLabel",),
    "login_entry": ("GtkEntry",),
    "user_list_scrolled": ("GtkScrolledWindow",),
    "user_list": ("GtkIconView",),
    "alignment2": ("GtkAlignment",),
    "bottom_pane": ("GtkEventBox",),
    "sessions_box": ("GtkHBox",),
    "sessions": ("GtkComboBox", "GtkComboBoxEntry"),
    "lang_box": ("GtkHBox",),
    "lang": ("GtkComboBox", "GtkComboBoxEntry"),
    "keyboard": ("GtkComboBox", "GtkComboBoxEntry"),
    "label_keyboard": ("GtkLabel",),
    "exit": ("GtkButton",),
}

#: The interface files, in the order they are installed.
GTK2_INTERFACE = "greeter.ui"
GTK3_INTERFACE = "greeter-gtk3.ui"

#: Everything copied from SOURCE_THEME_DIR into the installed theme. The three
#: image files are generated and are not in this list.
THEME_TEXT_FILES = (GTK2_INTERFACE, GTK3_INTERFACE, "gtkrc", "gtk.css", "index.theme")

#: The greeter binaries a distribution may have installed, in the order worth
#: trying. Debian and Ubuntu put it in /usr/lib/lxdm, a source build puts it in
#: /usr/libexec, and the GTK+ 3 one is named the same because it is the same
#: installed file built against a different toolkit.
GREETER_CANDIDATES = (
    Path("/usr/lib/lxdm/lxdm-greeter-gtk"),
    Path("/usr/libexec/lxdm-greeter-gtk"),
    Path("/usr/local/libexec/lxdm-greeter-gtk"),
    Path("/usr/lib/lxdm/lxdm-greeter-gtk2"),
)

#: Where a distribution that has not installed its greeter yet is expected to
#: put it: Debian and Ubuntu configure --libexecdir=/usr/lib/lxdm, and the
#: automake default everywhere else is /usr/libexec. This is what the [base]
#: greeter key is given when the machine has no greeter at all, because leaving
#: the key out is not an option - see greeter_path below for what lxdm does with
#: a configuration that has no greeter in it.
DEFAULT_GREETER_PATHS: dict[str, Path] = {
    "debian": Path("/usr/lib/lxdm/lxdm-greeter-gtk"),
    "arch": Path("/usr/libexec/lxdm-greeter-gtk"),
    "fedora": Path("/usr/libexec/lxdm-greeter-gtk"),
    "suse": Path("/usr/libexec/lxdm-greeter-gtk"),
}

#: The path of last resort, for a distribution this script cannot name.
FALLBACK_GREETER = Path("/usr/libexec/lxdm-greeter-gtk")

#: The target systemd boots into, and the symlink under /etc that names it.
#: graphical.target is what wants display-manager.service; multi-user.target
#: does not want it at all, so a machine left on multi-user.target boots to the
#: text login on tty1 however well lxdm itself is installed and enabled. A
#: display manager that does not start is usually this one line of systemd
#: configuration, and nothing in lxdm's own files can make up for it.
GRAPHICAL_TARGET = "graphical.target"
DEFAULT_TARGET_LINK = Path("/etc/systemd/system/default.target")

#: Where lxdm writes what it did. log_init() in lxdm.c sends lxdm's own stdout
#: and stderr here, and the greeter inherits both, so this file is where a
#: greeter that did not appear says why - which X server was exec'd, whether it
#: could be connected to, and why every session was let go of.
LXDM_LOG = Path("/var/log/lxdm.log")

#: How long the X server and the greeter are given before the log is read, and
#: how much of the log is shown when they did not come up.
LXDM_START_GRACE_SECONDS = 6
LXDM_LOG_LINES = 20

#: Display managers lxdm replaces when they are enabled. XLDM is not among
#: them, and neither is anything that is not a display manager.
OTHER_DISPLAY_MANAGERS = (
    "gdm3",
    "gdm",
    "lightdm",
    "sddm",
    "xdm",
    "nodm",
    "slim",
    "ly",
    "greetd",
    "entrance",
)

# --- what an lxdm theme is ----------------------------------------------------
# The greeter reads the file pair that matches the toolkit it was built
# against. Both are installed, so either build is greeted with this theme.
GTK2_FILES = (GTK2_INTERFACE, "gtkrc")
GTK3_FILES = (GTK3_INTERFACE, "gtk.css")


# --- logging ------------------------------------------------------------------


class Log:
    """Progress output. There is no quiet mode: the script has nothing to
    configure, so every run prints the same steps."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- running things -----------------------------------------------------------


def run(
    command: list[str],
    capture: bool = False,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run a command, with its output kept only when it is asked for.

    Nothing here goes through a shell: every command is a list, so a path with
    a space in it - and /usr/share/lxdm/themes/... has none, but a user's
    home directory may - cannot turn into two arguments.

    Keeping the output is for the short commands whose text is only wanted when
    they fail. Anything long running, and anything that may want to ask a
    question, is run with its output going to the terminal: a command whose
    output is being swallowed looks exactly like a command that has hung.
    """
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True, env=environment
        )
    return subprocess.run(command, check=False, text=True, env=environment)


def is_root() -> bool:
    """Whether this process can write /etc and /usr/share.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    install; reading it through ``getattr`` keeps the module importable there so
    the palette and the image code can be exercised and the file linted
    anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """Be root, re-running the script through sudo when possible.

    Everything this installer writes is system wide, so there is no useful
    partial run as a normal user. sudo is used when it exists, and the fact is
    announced rather than done silently; the environment variable stops a sudo
    that fails to change the user from looping.
    """
    if is_root():
        return
    if os.environ.get("GNUGHAN_LXDM_ELEVATED") == "1":
        raise SystemExit(
            "error: still not root after sudo; run the script as root "
            "(su -c 'python3 settings_lxdm.py')"
        )
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: installing LXDM writes to /etc and /usr/share; "
            "run this script as root"
        )
    log.step("This install is system wide; re-running it through sudo")
    environment = dict(os.environ, GNUGHAN_LXDM_ELEVATED="1")
    os.execvpe(sudo, [sudo, sys.executable, str(SCRIPT_PATH), *sys.argv[1:]], environment)


# --- which distribution this is ----------------------------------------------


def os_release() -> dict[str, str]:
    """Parse ``/etc/os-release`` into a dictionary, empty if it is absent."""
    result: dict[str, str] = {}
    try:
        text = Path("/etc/os-release").read_text(encoding="utf-8")
    except OSError:
        return result
    for line in text.splitlines():
        name, separator, value = line.partition("=")
        if not separator:
            continue
        result[name.strip()] = value.strip().strip('"').strip("'")
    return result


def distro_family() -> str:
    """One of ``debian``, ``arch``, ``fedora``, ``suse``, or ``unknown``.

    The family is taken from ID_LIKE as well as ID, because a derivative -
    Ubuntu, Mint, Pop, Kali - reports ID_LIKE=debian and uses apt.
    """
    release = os_release()
    tokens = " ".join(
        (release.get("ID", ""), release.get("ID_LIKE", ""), release.get("NAME", ""))
    ).lower()
    for family, markers in (
        ("arch", ("arch", "manjaro", "endeavouros", "garuda")),
        ("debian", ("debian", "ubuntu", "mint", "pop", "kali", "raspbian", "devuan")),
        ("fedora", ("fedora", "rhel", "centos", "rocky", "almalinux")),
        ("suse", ("suse", "sles", "opensuse")),
    ):
        if any(marker in tokens for marker in markers):
            return family
    return "unknown"


def distro_description() -> str:
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


#: The install command per family, without the package names. lxdm is in the
#: repositories of every family except Arch, where it is in the AUR; the X
#: server is listed explicitly because Debian only recommends it, and a greeter
#: with no X server is a greeter that cannot start.
PACKAGE_MANAGERS: dict[str, tuple[str, ...]] = {
    # -y answers apt's own questions and none of dpkg's. The two Dpkg::Options
    # arguments are what stop dpkg asking what to do about a configuration file
    # it finds edited, and /etc/lxdm/lxdm.conf is a conffile: a run that stops
    # to ask about it stops before this script has seen the file at all.
    # --force-confdef takes the package's answer where there is one,
    # --force-confold keeps the file that is already on the machine otherwise,
    # which is the rule this script follows for that file itself.
    "debian": (
        "apt-get",
        "install",
        "-y",
        "--no-install-recommends",
        "-o",
        "Dpkg::Options::=--force-confdef",
        "-o",
        "Dpkg::Options::=--force-confold",
    ),
    "arch": ("pacman", "-S", "--needed", "--noconfirm"),
    "fedora": ("dnf", "install", "-y"),
    "suse": ("zypper", "--non-interactive", "install"),
}

#: The packages to install per family. lxdm pulls its own dependencies (PAM,
#: GTK+ 2 or GTK+ 3, the logging daemon); what is named here is what it does not
#: depend on but needs to be usable: the X server, and on Debian the Clearlooks
#: theme and engines that the greeter's own default configuration names.
PACKAGES: dict[str, tuple[str, ...]] = {
    "debian": ("lxdm", "xserver-xorg", "gtk2-engines"),
    "arch": ("lxdm", "xorg-server"),
    "fedora": ("lxdm", "xorg-x11-server-Xorg", "gnome-themes-extra"),
    "suse": ("lxdm", "xorg-x11-server", "gtk2-metatheme-adwaita"),
}

#: The same two tables keyed by the package manager's own program name, which is
#: what is used when /etc/os-release does not name a family this script knows.
#: A distribution built by hand can call itself anything, and the markers above
#: then match nothing: without these tables the theme and the configuration land
#: on a machine where nothing ever installs lxdm, and it keeps booting to the
#: login on tty1. A machine that has apt-get is a machine where lxdm is
#: installed with apt-get, whatever its ID line says.
PACKAGE_MANAGERS_BY_PROGRAM: dict[str, tuple[str, ...]] = {
    "apt-get": PACKAGE_MANAGERS["debian"],
    "pacman": PACKAGE_MANAGERS["arch"],
    "dnf": PACKAGE_MANAGERS["fedora"],
    "zypper": PACKAGE_MANAGERS["suse"],
}

PACKAGES_BY_PROGRAM: dict[str, tuple[str, ...]] = {
    "apt-get": PACKAGES["debian"],
    "pacman": PACKAGES["arch"],
    "dnf": PACKAGES["fedora"],
    "zypper": PACKAGES["suse"],
}
# --- installing the packages ---------------------------------------------------


def package_program() -> str | None:
    """The package manager to use, by program name, or None if there is none.

    The distribution decides first, because a family that is recognised has a
    preferred manager and package names of its own. When the family is not
    recognised - or its manager is not installed - the machine decides instead:
    the first of the four managers that is really there. That is what makes
    "install lxdm when it is not installed" hold on a distribution whose
    /etc/os-release says something this script has never heard of, which is the
    one case it is most likely to meet.
    """
    family = distro_family()
    preferred = PACKAGE_MANAGERS.get(family, (None,))[0]
    if preferred and shutil.which(preferred):
        return preferred
    for program in PACKAGE_MANAGERS_BY_PROGRAM:
        if shutil.which(program):
            return program
    return None


def package_manager() -> tuple[str, ...] | None:
    """The install command for this machine, or None if it has none of them."""
    program = package_program()
    return PACKAGE_MANAGERS_BY_PROGRAM.get(program) if program else None


def package_environment() -> dict[str, str]:
    """The environment a package manager is run in.

    On Debian and Ubuntu the debconf frontend is told to be non-interactive.
    This installer has no options and asks nothing: a run that stops on a
    question - which display manager should be the default, which keyboard
    layout the X server should use - is a run that looks as if it has hung,
    because the question is asked through a terminal nobody is watching. The
    one answer that matters is given by this script, afterwards, when it makes
    lxdm the display manager itself.
    """
    return dict(
        os.environ,
        DEBIAN_FRONTEND="noninteractive",
        DEBCONF_NONINTERACTIVE_SEEN="true",
    )


def package_command() -> list[str] | None:
    """The command that installs lxdm and an X server here, or None.

    The manager and the package names always come from the same table, so a
    machine that installs with pacman is never handed the Debian package names.
    """
    program = package_program()
    if program is None:
        return None
    return [*PACKAGE_MANAGERS_BY_PROGRAM[program], *PACKAGES_BY_PROGRAM[program]]


def manual_package_hint() -> str:
    """What to tell the user when the script cannot install the packages.

    The command is built from the manager that was found rather than named by
    hand, so the hint is the command that would have run on this machine. It
    names the packages only when there is no manager at all to name them.
    """
    command = package_command()
    if command is None:
        return (
            "install lxdm and an X server for your distribution "
            f"({distro_description()}), then run this script again"
        )
    if command[0] == "pacman":
        return (
            "lxdm is in the AUR on Arch: build it with your AUR helper "
            "(for example 'yay -S lxdm'), then run this script again"
        )
    return "run: " + " ".join(command)


def lxdm_present() -> bool:
    """Whether the lxdm daemon is installed.

    The daemon is what matters, not the package: a system that installed lxdm
    by hand, or built it from source, is one this script can still configure.
    """
    return any(
        path.exists()
        for path in (
            Path("/usr/sbin/lxdm"),
            Path("/usr/sbin/lxdm-binary"),
            Path("/usr/local/sbin/lxdm"),
        )
    )


def install_packages(log: Log) -> bool:
    """Install lxdm and an X server, returning whether lxdm ended up present.

    A failure here is reported and the run continues: the theme and the
    configuration are worth writing even on a machine where the package could
    not be installed, because the moment it arrives the greeter is already
    configured.
    """
    if lxdm_present():
        log.detail("lxdm is already installed")
        return True

    command = package_command()
    if command is None:
        log.detail(manual_package_hint())
        return False

    environment = package_environment()
    if command[0] == "apt-get":
        # A stale package list is the most common reason an apt-get install
        # fails on a machine that has never been updated.
        log.detail("refreshing the package list")
        run(["apt-get", "update"], environment=environment)
    log.detail("running: " + " ".join(command))
    # The output is left where it can be seen. A package manager says what it
    # is downloading and unpacking, and a download that prints nothing for a
    # minute is indistinguishable from a run that has stopped - which is the
    # difference between a user who waits and a user who kills the script.
    result = run(command, environment=environment)
    if result.returncode != 0:
        log.detail("the package manager reported a failure")
        log.detail(manual_package_hint())
        return lxdm_present()
    if not lxdm_present():
        log.detail("the package manager reported success but lxdm is not there")
        log.detail(manual_package_hint())
        return False
    log.detail("lxdm is installed")
    return True


# --- the assets the theme is built from ---------------------------------------


def assets_dirs() -> list[Path]:
    """Every place the repository's images may be, most likely first.

    The first entries are the repository this script is in: the script lives in
    dotfile/LXDM_THEME, so the assets are two directories up from it. The rest
    are where a packager would put them on an installed system, which is what
    makes the script usable when the repository itself is not on the machine.
    """
    found: list[Path] = []
    for candidate in (SCRIPT_DIR, *SCRIPT_DIR.parents):
        assets = candidate / "assets"
        if assets not in found:
            found.append(assets)
    for assets in (
        Path("/usr/share/gnuchanos/assets"),
        Path("/usr/local/share/gnuchanos/assets"),
        Path("/usr/share/gnuchanos"),
    ):
        if assets not in found:
            found.append(assets)
    return found


def find_asset(name: str) -> Path | None:
    """The first copy of an asset, or None."""
    for directory in assets_dirs():
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def require_assets() -> tuple[Path, Path]:
    """The wallpaper and the logo, or a message saying where they were looked
    for.

    Both are needed before anything is written. A theme with no wallpaper and
    no avatar is not the theme that was asked for, and discovering that after
    the configuration has been rewritten would leave the machine in a state
    that is worse than the one it was found in.
    """
    background = find_asset(BACKGROUND_ASSET)
    logo = find_asset(LOGO_ASSET)
    if background is not None and logo is not None:
        return background, logo
    missing = [
        name
        for name, found in ((BACKGROUND_ASSET, background), (LOGO_ASSET, logo))
        if found is None
    ]
    looked = "\n".join(
        f"  {directory / name}" for directory in assets_dirs() for name in missing
    )
    raise SystemExit(
        "error: the images this theme is built from were not found:\n" + looked
    )


# --- file helpers --------------------------------------------------------------


def read_text(path: Path) -> str:
    """Contents of a file, or an empty string when it is not readable."""
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def backup_once(path: Path) -> Path | None:
    """Copy an existing file aside, returning where the copy went.

    The copy is taken once and kept: a second run must not overwrite it with
    this script's own output, because the file worth keeping is the one the
    machine had before the script ever touched it.

    It is copied rather than moved, and that matters more than it looks: the
    caller merges the keys into this same file afterwards, so moving it out of
    the way first would have it merge into nothing and rewrite a configuration
    that had lost every key the distribution shipped - the greeter path, the
    session, an autologin, the X server arguments - while looking, in the log,
    exactly like a run that had kept them all.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.copy2(path, backup)
    return backup
# --- PNG images ----------------------------------------------------------------
# Two of the files the theme needs are not in the repository as such: the logo
# scaled to the size it is drawn at on the login box and as an avatar, on a
# transparent square, and a copy of the wallpaper. Pillow is not installed on a
# fresh system and is not a dependency worth taking for that, so the code below
# reads and writes PNG well enough for both jobs: 1, 2, 4, 8 and 16 bit grey,
# grey with alpha, RGB, RGBA and palette images, interlaced or not. An image
# this cannot read is reported rather than guessed at.


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

#: Channels per pixel, by the colour type in the header: 0 grey, 2 RGB,
#: 3 palette, 4 grey with alpha, 6 RGBA.
PNG_CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}

#: Adam7: the seven passes of an interlaced image, as
#: (x_start, y_start, x_step, y_step).
ADAM7_PASSES = (
    (0, 0, 8, 8),
    (4, 0, 8, 8),
    (0, 4, 4, 8),
    (2, 0, 4, 4),
    (0, 2, 2, 4),
    (1, 0, 2, 2),
    (0, 1, 1, 2),
)


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    """One PNG chunk: length, tag, body, CRC over the tag and the body."""
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def _samples(row: bytearray, count: int, depth: int) -> list[int] | None:
    """The first ``count`` samples of a scanline, widened to 0-255.

    A sample of one, two or four bits is scaled by the largest value it can
    hold, so a one bit image comes out black and white rather than dark grey and
    light grey; a sixteen bit one keeps its high byte, which is what every
    display does with it anyway.
    """
    if depth == 8:
        if len(row) < count:
            return None
        return list(row[:count])
    if depth == 16:
        if len(row) < count * 2:
            return None
        return [row[index * 2] for index in range(count)]
    per_byte = 8 // depth
    mask = (1 << depth) - 1
    scale = 255 // mask
    if len(row) * per_byte < count:
        return None
    values: list[int] = []
    for index in range(count):
        byte = row[index // per_byte]
        shift = 8 - depth * (index % per_byte + 1)
        values.append(((byte >> shift) & mask) * scale)
    return values


def _unfilter(
    raw: bytes, start: int, width: int, height: int, bits_per_pixel: int
) -> tuple[list[bytearray] | None, int]:
    """Undo the per scanline filters of one image or one Adam7 pass.

    Returns the scanlines and the offset the next pass starts at, or None when
    the data runs out or names a filter that does not exist.
    """
    stride = (width * bits_per_pixel + 7) // 8
    # The filters look back by one pixel, which is one byte for everything under
    # eight bits per pixel.
    step = max(1, bits_per_pixel // 8)
    rows: list[bytearray] = []
    previous = bytearray(stride)
    offset = start
    for _ in range(height):
        if offset + 1 + stride > len(raw):
            return None, start
        filter_type = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        if filter_type == 0:
            pass
        elif filter_type == 1:
            for index in range(step, stride):
                row[index] = (row[index] + row[index - step]) & 0xFF
        elif filter_type == 2:
            for index in range(stride):
                row[index] = (row[index] + previous[index]) & 0xFF
        elif filter_type == 3:
            for index in range(stride):
                left = row[index - step] if index >= step else 0
                row[index] = (row[index] + ((left + previous[index]) >> 1)) & 0xFF
        elif filter_type == 4:
            for index in range(stride):
                left = row[index - step] if index >= step else 0
                above = previous[index]
                corner = previous[index - step] if index >= step else 0
                estimate = left + above - corner
                distance_left = abs(estimate - left)
                distance_above = abs(estimate - above)
                distance_corner = abs(estimate - corner)
                if distance_left <= distance_above and distance_left <= distance_corner:
                    predictor = left
                elif distance_above <= distance_corner:
                    predictor = above
                else:
                    predictor = corner
                row[index] = (row[index] + predictor) & 0xFF
        else:
            return None, start
        rows.append(row)
        previous = row
    return rows, offset


def _row_to_rgba(
    row: bytearray,
    width: int,
    depth: int,
    colour_type: int,
    palette: bytes,
    transparency: bytes,
) -> bytearray | None:
    """One scanline as RGBA bytes."""
    channels = PNG_CHANNELS[colour_type]
    samples = _samples(row, width * channels, depth)
    if samples is None:
        return None

    # Colour key transparency: one value of a greyscale or RGB image that is
    # meant to be see through, which is how logos older than the alpha channel
    # carry their transparency.
    key: tuple[int, ...] | None = None
    if transparency and colour_type == 0 and len(transparency) >= 2:
        key = (transparency[0],)
    elif transparency and colour_type == 2 and len(transparency) >= 6:
        key = (transparency[1], transparency[3], transparency[5])

    rgba = bytearray(width * 4)
    for index in range(width):
        base = index * channels
        alpha = 255
        if colour_type == 0:
            red = green = blue = samples[base]
            if key is not None and samples[base] == key[0]:
                alpha = 0
        elif colour_type == 4:
            red = green = blue = samples[base]
            alpha = samples[base + 1]
        elif colour_type == 2:
            red, green, blue = samples[base], samples[base + 1], samples[base + 2]
            if key is not None and (red, green, blue) == key:
                alpha = 0
        elif colour_type == 6:
            red, green, blue = samples[base], samples[base + 1], samples[base + 2]
            alpha = samples[base + 3]
        else:
            entry = samples[base] * 3
            if entry + 3 > len(palette):
                return None
            red, green, blue = palette[entry], palette[entry + 1], palette[entry + 2]
            if samples[base] < len(transparency):
                alpha = transparency[samples[base]]
        target = index * 4
        rgba[target] = red
        rgba[target + 1] = green
        rgba[target + 2] = blue
        rgba[target + 3] = alpha
    return rgba


class Raster:
    """An RGBA image in memory, and the few operations this theme needs."""

    __slots__ = ("width", "height", "pixels")

    def __init__(self, width: int, height: int, pixels: bytearray | None = None) -> None:
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 4) if pixels is None else pixels

    @classmethod
    def solid(cls, width: int, height: int, colour: tuple[int, int, int, int] = (0, 0, 0, 0)) -> "Raster":
        """A rectangle of one colour."""
        return cls(width, height, bytearray(bytes(colour) * (width * height)))

    def scaled_to(self, width: int, height: int) -> "Raster":
        """The image at another size, averaging the pixels it shrinks away.

        Averaging is done on colours premultiplied by their alpha and divided
        back out afterwards: mixing the colours of transparent and opaque pixels
        first is what puts a dark fringe around a logo that is drawn on
        nothing.
        """
        target = Raster.solid(width, height)
        source = self.pixels
        for target_y in range(height):
            first_y = target_y * self.height // height
            last_y = max(first_y + 1, (target_y + 1) * self.height // height)
            last_y = min(last_y, self.height)
            for target_x in range(width):
                first_x = target_x * self.width // width
                last_x = max(first_x + 1, (target_x + 1) * self.width // width)
                last_x = min(last_x, self.width)
                red = green = blue = alpha_sum = 0
                count = (last_x - first_x) * (last_y - first_y)
                for source_y in range(first_y, last_y):
                    row = (source_y * self.width + first_x) * 4
                    for column in range(last_x - first_x):
                        offset = row + column * 4
                        alpha = source[offset + 3]
                        red += source[offset] * alpha
                        green += source[offset + 1] * alpha
                        blue += source[offset + 2] * alpha
                        alpha_sum += alpha
                if alpha_sum == 0:
                    continue  # every pixel behind this one was transparent
                offset = (target_y * width + target_x) * 4
                target.pixels[offset] = min(255, red // alpha_sum)
                target.pixels[offset + 1] = min(255, green // alpha_sum)
                target.pixels[offset + 2] = min(255, blue // alpha_sum)
                target.pixels[offset + 3] = min(255, alpha_sum // count)
        return target

    def fitted(self, size: int, colour: tuple[int, int, int, int] = (0, 0, 0, 0)) -> "Raster":
        """The image scaled to fit a square of ``size``, centred on ``colour``.

        The proportions are kept, so a logo that is taller than it is wide is
        not squashed into a square.
        """
        scale = min(size / self.width, size / self.height)
        target_width = max(1, min(size, round(self.width * scale)))
        target_height = max(1, min(size, round(self.height * scale)))
        scaled = self.scaled_to(target_width, target_height)
        canvas = Raster.solid(size, size, colour)
        left = (size - target_width) // 2
        top = (size - target_height) // 2
        for y in range(target_height):
            source = y * target_width * 4
            target = ((y + top) * size + left) * 4
            canvas.pixels[target:target + target_width * 4] = scaled.pixels[source:source + target_width * 4]
        return canvas

    def write(self, path: Path) -> None:
        """Write the image as an 8 bit RGBA PNG.

        Filter type 0 on every scanline: the point of the encoder is to be
        short and obviously correct, and a screenshot-sized image compresses
        well without the per scanline prediction.
        """
        stride = self.width * 4
        raw = bytearray()
        for row in range(self.height):
            raw.append(0)
            raw += self.pixels[row * stride:(row + 1) * stride]
        payload = (
            PNG_SIGNATURE
            + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
            + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _png_chunk(b"IEND", b"")
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)


def read_png(path: Path) -> Raster | None:
    """Read a PNG, or return None when it is not one this can handle."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return None
    if not data.startswith(PNG_SIGNATURE):
        return None

    header: tuple[int, int, int, int, int, int, int] | None = None
    palette = b""
    transparency = b""
    compressed = bytearray()
    offset = len(PNG_SIGNATURE)
    while offset + 8 <= len(data):
        length, tag = struct.unpack(">I4s", data[offset:offset + 8])
        body = data[offset + 8:offset + 8 + length]
        if len(body) != length:
            return None
        offset += 12 + length
        if tag == b"IHDR":
            if length != 13:
                return None
            header = struct.unpack(">IIBBBBB", body)
        elif tag == b"PLTE":
            palette = body
        elif tag == b"tRNS":
            transparency = body
        elif tag == b"IDAT":
            compressed += body
        elif tag == b"IEND":
            break
    if header is None:
        return None

    width, height, depth, colour_type, compression, filter_method, interlace = header
    if compression != 0 or filter_method != 0 or width == 0 or height == 0:
        return None
    if colour_type not in PNG_CHANNELS or depth not in (1, 2, 4, 8, 16):
        return None
    if depth < 8 and colour_type not in (0, 3):
        return None
    if colour_type == 3 and not palette:
        return None
    try:
        raw = zlib.decompress(bytes(compressed))
    except zlib.error:
        return None

    bits_per_pixel = PNG_CHANNELS[colour_type] * depth
    pixels = bytearray(width * height * 4)
    if interlace == 0:
        rows, _ = _unfilter(raw, 0, width, height, bits_per_pixel)
        if rows is None:
            return None
        for row_index, row in enumerate(rows):
            rgba = _row_to_rgba(row, width, depth, colour_type, palette, transparency)
            if rgba is None:
                return None
            pixels[row_index * width * 4:(row_index + 1) * width * 4] = rgba
    elif interlace == 1:
        position = 0
        for x_start, y_start, x_step, y_step in ADAM7_PASSES:
            pass_width = (width - x_start + x_step - 1) // x_step if width > x_start else 0
            pass_height = (height - y_start + y_step - 1) // y_step if height > y_start else 0
            if pass_width == 0 or pass_height == 0:
                continue
            rows, position = _unfilter(raw, position, pass_width, pass_height, bits_per_pixel)
            if rows is None:
                return None
            for row_index, row in enumerate(rows):
                rgba = _row_to_rgba(row, pass_width, depth, colour_type, palette, transparency)
                if rgba is None:
                    return None
                y = y_start + row_index * y_step
                for column in range(pass_width):
                    x = x_start + column * x_step
                    target = (y * width + x) * 4
                    source = column * 4
                    pixels[target:target + 4] = rgba[source:source + 4]
    else:
        return None
    return Raster(width, height, pixels)
# --- installing the theme ------------------------------------------------------


def render_theme(log: Log, background: Path, logo: Path) -> None:
    """Write the installed theme: the committed files, and the three images.

    The placeholder @THEME_DIR@ is replaced here with the directory the theme is
    being written to, so an installed file can carry an absolute path. Only
    gtk.css uses it: the wallpaper is a CSS url(), which a style provider does
    not resolve the way GtkBuilder resolves a pixbuf, so it is written out in
    full. The two interfaces deliberately do not use it - their logo is named by
    the relative path login.png, which is the form the upstream Industrial theme
    uses and the form GtkBuilder resolves against the directory of the
    interface file it is loading. A pixbuf it cannot resolve takes the whole
    window with it, so that form is the one to keep.

    The logo is scaled twice from the same source: once for the image on the
    login box, once as the avatar the greeter falls back to for an account with
    no picture of its own. The wallpaper is copied byte for byte, because the
    greeter scales it to the screen itself and re-encoding a 4 MB photograph
    would only cost time.
    """
    theme_dir = INSTALLED_THEME_DIR
    theme_dir.mkdir(parents=True, exist_ok=True)

    for name in THEME_TEXT_FILES:
        source = SOURCE_THEME_DIR / name
        if not source.is_file():
            raise SystemExit(f"error: the theme file {source} is missing")
        text = source.read_text(encoding="utf-8")
        text = text.replace(THEME_DIR_PLACEHOLDER, theme_dir.as_posix())
        if THEME_DIR_PLACEHOLDER in text:
            raise SystemExit(f"error: {source} still names {THEME_DIR_PLACEHOLDER} after substitution")
        (theme_dir / name).write_text(text, encoding="utf-8")
        log.detail(f"wrote {theme_dir / name}")

    wallpaper = theme_dir / BACKGROUND_FILE
    shutil.copyfile(background, wallpaper)
    log.detail(f"wrote {wallpaper} (from {background})")

    image = read_png(logo)
    if image is None:
        raise SystemExit(
            f"error: {logo} is not a PNG this script can read; the logo has to "
            "be a PNG for the greeter to show it"
        )
    for name, size in (
        (LOGIN_IMAGE_FILE, LOGIN_IMAGE_SIZE),
        (AVATAR_FILE, AVATAR_SIZE),
    ):
        target = theme_dir / name
        image.fitted(size).write(target)
        log.detail(f"wrote {target} ({size}x{size}, from {logo})")

    # The greeter runs as root, and root's umask is not the user's: the files
    # are made readable explicitly, because a greeter that cannot read its own
    # theme shows the GTK default and looks like nothing was installed.
    for path in sorted(theme_dir.iterdir()):
        if path.is_file():
            path.chmod(0o644)
        elif path.is_dir():
            path.chmod(0o755)
    theme_dir.chmod(0o755)


def install_gtk_theme(log: Log) -> bool:
    """Copy the GnuchanPurple GTK theme where the greeter can find it.

    The greeter draws its widgets with the GTK theme named by the [display]
    gtk_theme key, and it runs as the lxdm account, or as root, before anyone
    has logged in, so a theme installed in a user's home directory is invisible
    to it: this is the one place in this repository that has to install system
    wide. Returns whether a stylesheet the greeter can use landed, which is what
    decides whether the gtk_theme key is set at all.

    Every system theme directory that exists is filled, because GTK searches
    them in an order that differs between builds and because it is one copy of a
    few hundred kilobytes. What is checked on the way out is the theme the
    greeter will actually look for: gtk-2.0/gtkrc for a GTK+ 2 greeter,
    gtk-3.0/gtk.css for a GTK+ 3 one. A checkout with only one of the two is
    still worth naming.

    Nothing is done when the theme is not in this checkout: a machine that only
    has this directory can still install the greeter theme, and the greeter then
    keeps whatever GTK theme it had.
    """
    if not GTK_THEME_SOURCE.is_dir():
        log.detail(f"no GTK theme in this checkout ({GTK_THEME_SOURCE})")
        return False

    installed: list[Path] = []
    for directory in GTK_THEME_DIRS:
        if not directory.is_dir():
            continue
        target = directory / THEME_NAME
        shutil.copytree(
            GTK_THEME_SOURCE,
            target,
            dirs_exist_ok=True,
            ignore=shutil.ignore_patterns("__pycache__", ".DS_Store", "Thumbs.db"),
        )
        log.detail(f"copied the GTK theme into {target}")
        for path in target.rglob("*"):
            try:
                path.chmod(0o755 if path.is_dir() else 0o644)
            except OSError:
                continue
        installed.append(target)

    if not installed:
        log.detail("there is no system theme directory to copy the GTK theme into")
        return False

    toolkits = [
        label
        for label, relative in (
            ("GTK+ 2", "gtk-2.0/gtkrc"),
            ("GTK+ 3", "gtk-3.0/gtk.css"),
        )
        if (installed[0] / relative).is_file()
    ]
    log.detail("the greeter's own widgets use " + ", ".join(toolkits or ["nothing"]))
    return bool(toolkits)


def gtk_theme_available(name: str) -> bool:
    """Whether a GTK theme of this name is in one of the system theme
    directories.

    The configuration names a GTK theme by name rather than by path, so the only
    way to know whether the greeter will find it is to look where GTK looks. A
    theme in somebody's home directory is not counted: the greeter runs as the
    lxdm account or as root, which is the whole reason this script installs its
    GTK theme system wide.
    """
    for directory in GTK_THEME_DIRS:
        theme = directory / name
        if (theme / "gtk-2.0" / "gtkrc").is_file():
            return True
        if (theme / "gtk-3.0" / "gtk.css").is_file():
            return True
    return False


# --- the greeter configuration -------------------------------------------------
# /etc/lxdm/lxdm.conf is a key file with comments in it, and it is what decides
# what the greeter shows. Only the keys below are this script's; every other
# line is left exactly as it was found, comments included, so a machine that has
# been given an autologin, a session or a numlock setting keeps it.
#
# Three things about that file are worth knowing before reading it.
#
#   1. The copy lxdm ships documents the display keys as show_sessions,
#      show_time and show_exit, but the greeter looks for hide_sessions,
#      hide_time and hide_exit: the show_ keys are read by nothing at all. What
#      is set here is what the greeter reads, and the show_ keys are left alone
#      rather than rewritten, because a distribution may patch the greeter to
#      read them.
#
#   2. The greeter reads this file itself, and lxdm starts it as the "lxdm"
#      account when that account exists, so the file has to be readable by a
#      user that is not root. That is why it is written world readable below
#      rather than with the 0640 the package's own build rules give it: there
#      is nothing secret in it - a wallpaper path, a theme name and the display
#      switches - and a greeter that cannot open it falls back to its built in
#      defaults and shows no theme at all.
#
#   3. Two keys gate whole widgets, and the greeter's fallback when a key is
#      absent is "off". lang shows the language chooser and keyboard the
#      keyboard layout one, so both are written explicitly: lang on because the
#      interface carries a language chooser, keyboard off because the one it
#      carries is meant to stay hidden unless somebody wants it - which is also
#      what the shipped configuration defaults to on every distribution.

#: The keys of the [display] group this script owns. The background is written
#: as a POSIX path with forward slashes: lxdm reads the value as a file name,
#: and that is the spelling it expects regardless of what ran this script.
#:
#: datetime is the clock format and is the one value with a shape to respect:
#: the greeter copies it into an eight byte buffer and only when it starts with
#: a percent sign and is three characters at most, so %X - the locale's own
#: time, HH:MM:SS - fits and the "%c" default it replaces would not. The
#: default prints a full date and time, which is a long string to put in the
#: corner of a login screen at the size the stylesheet draws it.
DISPLAY_SETTINGS: dict[str, str] = {
    "theme": THEME_NAME,
    "bg": f"{INSTALLED_THEME_DIR.as_posix()}/{BACKGROUND_FILE}",
    "bottom_pane": "1",
    "transparent_pane": "0",
    "hide_sessions": "0",
    "hide_time": "0",
    "hide_exit": "0",
    "lang": "1",
    "keyboard": "0",
    "datetime": "%X",
}

#: The keys of the [userlist] group this script owns. The account list is what
#: shows an avatar at all, so it is on: with it off the greeter draws a plain
#: user name entry and the logo is only the image above it.
USERLIST_SETTINGS: dict[str, str] = {"disable": "0"}


def read_ini_value(text: str, section: str, key: str) -> str | None:
    """Value of ``key`` in ``section`` of a key file, or None.

    Deliberately small: this reads the greeter's own configuration without
    configparser, so that a duplicate key or a stray line in a file that has
    been edited by hand cannot raise in the middle of the install.
    """
    current: str | None = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith(";"):
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            current = stripped[1:-1].strip()
            continue
        if current is None or current.lower() != section.lower():
            continue
        name, separator, value = stripped.partition("=")
        if separator and name.strip().lower() == key.lower():
            return value.strip()
    return None


def greeter_path(existing: str) -> str:
    """The greeter binary to write into the configuration, always a path.

    The [base] greeter key names a binary by absolute path, and the copy lxdm
    ships names /usr/libexec/lxdm-greeter-gtk while both Debian and Ubuntu
    install theirs in /usr/lib/lxdm, so the shipped value is wrong on half the
    systems that have the package.

    This key is the one this script cannot leave to the distribution's copy of
    the file, and cannot drop on a machine where nothing is installed yet:

      * lxdm's ui_prepare() spawns a greeter only when this key names a
        non-empty path. With the key absent, or empty, lxdm starts the X server
        and puts no window on it at all: a black screen, with the text login
        still sitting on tty1 behind it;
      * a path that does not exist is nearly as bad, because the spawn fails and
        ui_prepare() returns without a word.

    So a configured path is kept only when the binary is really there, any
    installed candidate is taken next, and when the machine has no greeter at
    all the path its distribution is expected to install it to is written
    instead. That last case is a machine where lxdm is not installed yet: the
    configuration is written for the moment it arrives, and lxdm reads this file
    afterwards - not the package's own conffile, which is not installed over a
    file that is already there.
    """
    configured = read_ini_value(existing, "base", "greeter")
    if configured and Path(configured).exists():
        return configured
    for candidate in GREETER_CANDIDATES:
        if candidate.exists():
            return str(candidate)
    return str(DEFAULT_GREETER_PATHS.get(distro_family(), FALLBACK_GREETER))


def managed_settings(existing: str, gtk_theme_installed: bool) -> dict[str, dict[str, str]]:
    """Every key this run owns, grouped by section."""
    managed: dict[str, dict[str, str]] = {
        "display": dict(DISPLAY_SETTINGS),
        "userlist": dict(USERLIST_SETTINGS),
    }
    # gtk_theme is deliberately not one of the keys written here, and neither is
    # any icon theme: the greeter's own gtkrc and gtk.css carry the palette, and
    # these keys select the GTK theme its widgets are drawn with. Overwriting
    # the distribution's choice with a theme of ours is how a greeter that was
    # coming up stops coming up - the X server starts, the window is never drawn,
    # and the machine looks like a display manager that will not start. The
    # parameter is kept so that callers read the same way.
    _ = gtk_theme_installed
    # Always written, never dropped: a configuration that arrives without a
    # greeter key, or with one no longer on the machine, is one lxdm reads and
    # then shows nothing for.
    managed["base"] = {"greeter": greeter_path(existing)}
    return managed


def merge_conf(existing: str, managed: dict[str, dict[str, str]]) -> str:
    """Set the managed keys, keeping every other line of the file.

    The file is walked line by line: comments, blank lines, key order and the
    keys this script does not own survive untouched, a managed key is replaced
    where it already is, and a managed key that is missing is appended to its
    group. Missing groups are created at the end. The marker line above the keys
    that were added is what makes a second run replace them in place instead of
    appending another copy.
    """
    pending = {section: dict(keys) for section, keys in managed.items()}
    written: set[tuple[str, str]] = set()
    marked: set[str] = set()
    body: list[str] = []
    current: str | None = None

    def flush() -> None:
        if current is None:
            return
        keys = pending.get(current)
        if not keys:
            return
        if current not in marked:
            body.append(MANAGED_MARKER)
            marked.add(current)
        for key, value in keys.items():
            body.append(f"{key}={value}")
        pending[current] = {}

    def mark(section: str) -> None:
        if section not in marked:
            body.append(MANAGED_MARKER)
            marked.add(section)

    for line in existing.splitlines():
        stripped = line.strip()
        if stripped == MANAGED_MARKER:
            continue  # rewritten below, where its keys end up
        if stripped.startswith("[") and stripped.endswith("]"):
            flush()
            current = stripped[1:-1].strip()
            body.append(line)
            continue
        if current is not None and "=" in stripped and not stripped.startswith("#"):
            key = stripped.split("=", 1)[0].strip()
            keys = pending.get(current)
            if keys is not None and key in keys:
                if (current, key) in written:
                    continue  # a duplicate of a key already set
                written.add((current, key))
                mark(current)
                body.append(f"{key}={keys.pop(key)}")
                continue
        body.append(line)
    flush()

    for section, keys in pending.items():
        if not keys:
            continue
        if body and body[-1].strip():
            body.append("")
        mark(section)
        body.append(f"[{section}]")
        for key, value in keys.items():
            body.append(f"{key}={value}")

    return "\n".join(body).strip("\n") + "\n"


def install_configuration(log: Log, gtk_theme_installed: bool) -> None:
    """Merge this theme's keys into /etc/lxdm/lxdm.conf.

    The previous contents are copied to lxdm.conf.gnuchan-backup once - not
    moved, and read first - so that what is merged into is the file the machine
    had and the copy of it is the file as the distribution shipped it, which is
    the only undo a script with no flags owes anyone.
    """
    existing = read_text(CONFIG_FILE)
    if not CONFIG_FILE.exists():
        # A configuration for a package that is not installed yet: it is still
        # written, so that installing lxdm later finds the greeter configured.
        log.detail(f"{CONFIG_FILE} does not exist yet; it will be created")
    backup = backup_once(CONFIG_FILE)
    if backup is not None:
        log.detail(f"backed up {CONFIG_FILE.name} to {backup.name}")
    managed = managed_settings(existing, gtk_theme_installed)
    merged = merge_conf(existing, managed)
    write_text(CONFIG_FILE, merged)
    # 0644 rather than the 0640 lxdm's own build rules set: the greeter opens
    # this file itself, and lxdm starts it as the "lxdm" account when that
    # account exists, so a file only root can read is a file the greeter
    # silently ignores - and a greeter that cannot read its settings shows its
    # built in defaults and no theme at all. There is nothing secret in here.
    CONFIG_FILE.chmod(0o644)
    for section, keys in managed.items():
        log.detail(f"[{section}] " + " ".join(f"{key}={value}" for key, value in keys.items()))
# --- making lxdm the display manager -------------------------------------------
# Installing lxdm is not the same as the machine using it: the package neither
# enables its init script (Debian installs it with --no-start and leaves it
# disabled) nor takes the role from whatever display manager was already there,
# and lxdm's own systemd unit refuses to start at all unless
# /etc/X11/default-display-manager names lxdm. All three are done here.
#
# lxdm is not started. Starting a display manager from inside a running session
# takes the screen away from that session, which is not a thing an installer
# should do to whoever ran it; the switch happens at the next reboot.


def systemd_running() -> bool:
    """Whether systemd is the init system here, not merely installed."""
    return Path("/run/systemd/system").is_dir() and shutil.which("systemctl") is not None


def unit_path(name: str) -> Path | None:
    """The first copy of a systemd unit, most local first."""
    for directory in (
        Path("/etc/systemd/system"),
        Path("/run/systemd/system"),
        Path("/usr/lib/systemd/system"),
        Path("/lib/systemd/system"),
    ):
        candidate = directory / name
        if candidate.exists():
            return candidate
    return None


def unit_state(name: str) -> str:
    """The ``systemctl is-enabled`` answer, or an empty string."""
    result = run(["systemctl", "is-enabled", name], capture=True)
    return (result.stdout or "").strip()


def disable_other_display_managers(log: Log) -> str | None:
    """Turn off every other display manager that is enabled.

    Two display managers enabled at once is a race for the screen, and on
    systemd the loser of that race is decided by the order the units come up in.
    Only units that exist and report themselves as enabled are touched, and
    disabling removes the enablement symlinks and nothing else, so the other
    manager is one ``systemctl enable`` away. The name of the one that was
    running is returned, because that is what the user needs to know to get back
    to where they were.
    """
    replaced: list[str] = []
    for name in OTHER_DISPLAY_MANAGERS:
        unit = f"{name}.service"
        if unit_path(unit) is None:
            continue
        if unit_state(unit) not in ("enabled", "enabled-runtime"):
            continue
        if run(["systemctl", "disable", unit], capture=True).returncode == 0:
            log.detail(f"disabled {unit}, which was the display manager")
            replaced.append(name)
        else:
            log.warn(f"could not disable {unit}; both display managers are enabled")
    return replaced[0] if replaced else None


def systemd_default_target() -> str | None:
    """What systemd boots into, or None when it will not say.

    ``systemctl get-default`` prints the target name, or the path of the unit
    file when the target has been set by hand, hence the last component of
    whatever comes back.
    """
    if not systemd_running():
        return None
    result = run(["systemctl", "get-default"], capture=True)
    answer = (result.stdout or "").strip()
    if result.returncode != 0 or not answer:
        return None
    return answer.rsplit("/", 1)[-1]


def keep_default_target(log: Log) -> None:
    """Keep the default.target symlink before it is replaced.

    What is worth keeping here is the link rather than the unit it points at, so
    it is put back by naming it again - see the undo at the top of this file. A
    machine with no link at all was booting systemd's built in default, and that
    is written down as having been the case rather than guessed at.
    """
    link = DEFAULT_TARGET_LINK
    backup = link.with_name(link.name + BACKUP_SUFFIX)
    if backup.exists() or backup.is_symlink():
        return
    try:
        if link.is_symlink():
            pointed_at = os.readlink(link)
            backup.symlink_to(pointed_at)
            log.detail(f"backed up {link.name} to {backup.name} (it named {pointed_at})")
        elif link.exists():
            shutil.copy2(link, backup)
            log.detail(f"backed up {link.name} to {backup.name}")
        else:
            log.detail(f"there is no {link} to keep; systemd was booting its own default")
    except OSError as error:
        log.warn(f"could not back up {link}: {error}")


def boot_to_graphical_target(log: Log) -> str | None:
    """Make the machine boot into graphical.target, returning what it booted.

    A display manager is not run by multi-user.target: it is run because
    graphical.target wants display-manager.service, and systemd only reaches
    graphical.target when that is the default target. Making lxdm the display
    manager is therefore half the job. A machine left on multi-user.target has
    lxdm installed, enabled, aliased and correct in every file this script
    writes, and still boots to the text login on tty1 - which is exactly what
    "the display manager does not start" looks like from the other side of a
    reboot.

    The target is changed only when it is not graphical already, and the link
    that was there is kept. Without systemd, or where systemctl cannot be asked,
    everything is left as it was: this is not the only init system there is, and
    guessing for the others is worse than saying nothing.
    """
    current = systemd_default_target()
    if current is None:
        log.detail("systemd was not asked what it boots; the default target is left alone")
        return None
    if current == GRAPHICAL_TARGET:
        log.detail(f"the default target is already {GRAPHICAL_TARGET}")
        return None

    keep_default_target(log)
    if run(["systemctl", "set-default", GRAPHICAL_TARGET], capture=True).returncode == 0:
        log.detail(f"the default target was {current}; it is now {GRAPHICAL_TARGET}")
        return current
    log.warn(
        f"could not change the default target from {current} to {GRAPHICAL_TARGET}: "
        f"only {GRAPHICAL_TARGET} starts a display manager, so this machine may "
        "still come up on the login on tty1"
    )
    return None


def activate_display_manager(log: Log) -> str | None:
    """Point the machine at lxdm, returning the display manager it replaced.

    The four things a distribution needs are done in the order that leaves the
    system consistent if the script is interrupted: the file the unit and the
    Debian init script both read, then the enablement, then the alias systemd
    uses to know which unit is ``display-manager.service``, and then the default
    target, because the alias decides which unit is the display manager and the
    target decides whether a display manager is started at all.
    """
    try:
        # Backed up like lxdm.conf, and for the same reason: this is the file
        # the display manager being replaced reads about itself and refuses to
        # start over, so putting lxdm back is not the whole undo - the file has
        # to name the other manager again before it will run.
        backup = backup_once(DEFAULT_DM_FILE)
        if backup is not None:
            log.detail(f"backed up {DEFAULT_DM_FILE.name} to {backup.name}")
        DEFAULT_DM_FILE.parent.mkdir(parents=True, exist_ok=True)
        # as_posix() rather than str(): this file is compared literally. lxdm's
        # own unit and the Debian init script both test it for equality with
        # /usr/sbin/lxdm, so the one spelling that may be written into it is the
        # one they test against, whatever running this produced.
        DEFAULT_DM_FILE.write_text(LXDM_DAEMON.as_posix() + "\n", encoding="utf-8")
        log.detail(f"wrote {DEFAULT_DM_FILE}: {LXDM_DAEMON.as_posix()}")
    except OSError as error:
        log.warn(f"could not write {DEFAULT_DM_FILE}: {error}")

    if not systemd_running():
        update_rc = shutil.which("update-rc.d")
        if update_rc is not None:
            run([update_rc, "lxdm", "defaults"])
            log.detail("registered the lxdm init script")
            return None
        log.detail("no systemd and no update-rc.d: enable lxdm with your init system")
        return None

    replaced = disable_other_display_managers(log)

    # Before the unit is looked for, and not after it: the default target is a
    # fact about the machine rather than about this unit, and a machine that
    # boots multi-user.target starts no display manager whichever unit it has.
    boot_to_graphical_target(log)

    unit = unit_path("lxdm.service")
    if unit is None:
        log.warn("there is no lxdm.service here; lxdm will not start at boot")
        return replaced

    if run(["systemctl", "enable", "lxdm.service"], capture=True).returncode == 0:
        log.detail("enabled lxdm.service")
    else:
        # A unit with no [Install] section cannot be enabled; the alias below is
        # what makes systemd start it as the display manager either way.
        log.detail("lxdm.service has no [Install] section; using the alias instead")

    link = DISPLAY_MANAGER_ALIAS
    try:
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.is_symlink() and link.resolve() == unit.resolve():
            log.detail(f"{link} already points at lxdm")
            return replaced
        if link.exists() or link.is_symlink():
            link.unlink()
        link.symlink_to(unit)
        log.detail(f"linked {link} -> {unit}")
        # systemd keeps the units it has already read in memory, so a new alias
        # is one it does not know about until it is told to reread them. Without
        # this the switch only takes effect at the next boot, and anything that
        # asks for display-manager.service before then still gets the old one.
        run(["systemctl", "daemon-reload"], capture=True)
    except OSError as error:
        log.warn(f"could not link {link}: {error}")
    return replaced


# --- checking the result -------------------------------------------------------
# Everything here is read back from what was written rather than trusted: the
# interfaces are parsed, the ids the greeter asks for by name are looked for in
# them, the generated images are decoded again, and the configuration is read as
# the greeter will read it. A greeter that cannot find one of those ids does not
# fall back to anything - it fails - and a theme whose images are missing shows
# an empty box, so both are worth catching before a reboot.


def interface_widgets(path: Path) -> dict[str, str] | None:
    """The id and class of every object in a GtkBuilder file, or None."""
    try:
        root = ElementTree.parse(path).getroot()
    except (OSError, ElementTree.ParseError):
        return None
    widgets: dict[str, str] = {}
    for element in root.iter("object"):
        identifier = element.get("id")
        klass = element.get("class")
        if identifier and klass:
            widgets[identifier] = klass
    return widgets


def check_interfaces() -> list[str]:
    """Problems that would stop the greeter drawing this theme."""
    problems: list[str] = []
    for files in (GTK2_FILES, GTK3_FILES):
        interface = INSTALLED_THEME_DIR / files[0]
        if not interface.is_file():
            continue  # already reported as a missing file
        widgets = interface_widgets(interface)
        if widgets is None:
            problems.append(f"{interface} is not readable as GtkBuilder XML")
            continue
        for identifier, allowed in REQUIRED_WIDGETS.items():
            if identifier not in widgets:
                problems.append(
                    f"{interface} has no {identifier} widget, which the greeter "
                    "looks up by name"
                )
            elif widgets[identifier] not in allowed:
                problems.append(
                    f"{interface}: {identifier} is a {widgets[identifier]}, "
                    f"expected {' or '.join(allowed)}"
                )
    return problems


def check_theme() -> list[str]:
    """Problems with the installed theme files themselves."""
    problems: list[str] = []
    for name in (*THEME_TEXT_FILES, BACKGROUND_FILE, LOGIN_IMAGE_FILE, AVATAR_FILE):
        path = INSTALLED_THEME_DIR / name
        if not path.is_file():
            problems.append(f"{path} is missing")
        elif path.stat().st_size == 0:
            problems.append(f"{path} is empty")
    for name in (LOGIN_IMAGE_FILE, AVATAR_FILE):
        path = INSTALLED_THEME_DIR / name
        if path.is_file() and read_png(path) is None:
            problems.append(f"{path} is not a PNG the greeter can read")
    for path in (INSTALLED_THEME_DIR / name for name in THEME_TEXT_FILES):
        if path.is_file() and THEME_DIR_PLACEHOLDER in read_text(path):
            problems.append(f"{path} still names {THEME_DIR_PLACEHOLDER}")
    return problems + check_interfaces()


def check_configuration() -> list[str]:
    """Problems with what the greeter will read at login."""
    problems: list[str] = []
    text = read_text(CONFIG_FILE)
    if not CONFIG_FILE.is_file():
        problems.append(f"{CONFIG_FILE} is missing")
        return problems
    if read_ini_value(text, "display", "theme") != THEME_NAME:
        problems.append(f"{CONFIG_FILE} does not select the {THEME_NAME} theme")
    background = read_ini_value(text, "display", "bg")
    if not background or not Path(background).is_file():
        problems.append(
            f"{CONFIG_FILE}: the background image is not where the file says "
            f"({background})"
        )
    greeter = read_ini_value(text, "base", "greeter")
    if not greeter:
        problems.append(
            f"{CONFIG_FILE} names no greeter, and lxdm starts none at all without "
            "one: the X server comes up with an empty black screen while tty1 "
            "keeps the login"
        )
    elif not Path(greeter).exists():
        problems.append(
            f"{CONFIG_FILE}: the greeter {greeter} does not exist, so lxdm cannot "
            "start one and the screen stays black"
        )
    if (
        read_ini_value(text, "display", "gtk_theme") == THEME_NAME
        and not gtk_theme_available(THEME_NAME)
    ):
        # Only when the key names this theme: any other name is the
        # distribution's own choice, and the greeter falling back to it is not
        # this script's business.
        problems.append(
            f"{CONFIG_FILE} names the {THEME_NAME} GTK theme, but no theme of "
            "that name is installed, so the greeter's own widgets fall back to "
            "GTK's default style"
        )
    if not lxdm_present():
        problems.append("lxdm is not installed")
    return problems


def check_display_manager() -> list[str]:
    """Problems with the machine actually showing a display manager.

    Everything here ends the same way - the text login on tty1 instead of the
    greeter - and every one of them is possible while the theme, the
    configuration and the enabled unit are all exactly as this script wrote
    them. The run that finishes by promising a greeter at the next boot is the
    run that owes the user this list.
    """
    problems: list[str] = []
    named = read_text(DEFAULT_DM_FILE).strip()
    if not named:
        problems.append(
            f"{DEFAULT_DM_FILE} is missing or empty, and lxdm's own unit, and the "
            "Debian init script, both refuse to start unless it names "
            f"{LXDM_DAEMON.as_posix()}"
        )
    elif named != LXDM_DAEMON.as_posix():
        problems.append(
            f"{DEFAULT_DM_FILE} names {named} instead of {LXDM_DAEMON.as_posix()}"
        )

    unit = unit_path("lxdm.service")
    if unit is None:
        problems.append(
            "there is no lxdm.service, so nothing starts lxdm at boot: install "
            "lxdm and run this script again"
        )
        return problems
    if not systemd_running():
        return problems

    target = systemd_default_target()
    if target is not None and target != GRAPHICAL_TARGET:
        problems.append(
            f"this machine boots {target}, and only {GRAPHICAL_TARGET} starts a "
            "display manager: lxdm is installed and enabled but is never run, so "
            "the machine stops at the login on tty1"
        )
    link = DISPLAY_MANAGER_ALIAS
    if not link.is_symlink():
        problems.append(f"{link} is missing, so systemd has no display manager")
    elif link.resolve() != unit.resolve():
        problems.append(
            f"{link} points at {link.resolve()} instead of {unit}"
        )
    return problems


def check_result(log: Log) -> int:
    """Report what is wrong with the install, returning how many things are."""
    problems = check_theme() + check_configuration() + check_display_manager()
    if not problems:
        log.note(f"No problems found in {INSTALLED_THEME_DIR}.")
        return 0
    log.note(f"{len(problems)} problem(s) found:")
    for problem in problems:
        log.note(f"  {problem}")
    return len(problems)


# --- starting it ---------------------------------------------------------------


def start_display_manager(log: Log) -> bool:
    """Start lxdm now, and show what it logged, returning whether it is up.

    The switch belongs to the next boot either way, but a reboot is a slow way
    to discover that the X server or the greeter cannot start: the symptom is a
    black screen and a login prompt on tty1, and there is nothing on the screen
    to read. lxdm keeps its own account of the attempt in /var/log/lxdm.log -
    the X server it exec'd, whether it could be connected to within the retry
    budget, and why it let the session go - and the greeter's stderr goes to the
    same file, so starting it here and reading that file turns the reboot into a
    message on the terminal.

    Nothing is stopped afterwards. This is the display manager the machine is
    being given, and the greeter on the screen is the result that was asked for.
    """
    if not systemd_running():
        log.detail("there is no systemd here to start lxdm with; it starts with the machine")
        return False

    log.step("Starting lxdm, so a greeter that cannot come up says why here")
    result = run(["systemctl", "start", "lxdm.service"])
    if result.returncode != 0:
        log.warn(
            "systemctl could not start lxdm.service, and its own message is above: "
            "systemctl status lxdm.service and journalctl -u lxdm.service have the rest"
        )
        return False

    log.detail(f"started; the X server and the greeter get {LXDM_START_GRACE_SECONDS} seconds")
    time.sleep(LXDM_START_GRACE_SECONDS)

    lines = read_text(LXDM_LOG).splitlines()
    if lines:
        log.note(f"  ---- {LXDM_LOG} ----")
        for line in lines[-LXDM_LOG_LINES:]:
            log.note("  " + line)

    active = (run(["systemctl", "is-active", "lxdm.service"], capture=True).stdout or "").strip()
    if active == "active":
        log.note("lxdm is up: the greeter is on the screen now, and it comes up on its own")
        log.note("at every boot from here on.")
        return True

    journal = run(
        ["journalctl", "-u", "lxdm.service", "-n", str(LXDM_LOG_LINES), "--no-pager"],
        capture=True,
    )
    if (journal.stdout or "").strip():
        log.note("  ---- journalctl -u lxdm.service ----")
        for line in journal.stdout.strip().splitlines()[-LXDM_LOG_LINES:]:
            log.note("  " + line)

    log.warn(
        "lxdm is not running: the two listings above - lxdm's own log and "
        "systemd's - say what stopped it, and they stay there to be read again"
    )
    return False


# --- entry point ---------------------------------------------------------------


def main() -> int:
    """Install lxdm and the GnuchanPurple greeter, with no options to pass."""
    log = Log()
    ensure_root(log)

    log.step(f"Installing the GnuchanPurple LXDM greeter ({distro_description()})")
    # The images are resolved before anything is written, so a machine that does
    # not have them is left exactly as it was found.
    background, logo = require_assets()
    log.detail(f"wallpaper: {background}")
    log.detail(f"logo: {logo}")
    installed = install_packages(log)

    log.step("Installing the theme")
    render_theme(log, background, logo)
    gtk_theme_installed = install_gtk_theme(log)

    log.step("Configuring the greeter")
    install_configuration(log, gtk_theme_installed)

    log.step("Making lxdm the display manager")
    replaced = activate_display_manager(log)

    log.step("Checking the result")
    problems = check_result(log)

    log.note("")
    if installed:
        log.note("LXDM is installed and will greet with GnuchanPurple:")
    else:
        log.note(
            "The theme and the configuration are in place, but lxdm itself is "
            "not installed yet:"
        )
    if not installed:
        log.note(f"  {manual_package_hint()}")
    log.note(f"  theme      {INSTALLED_THEME_DIR}")
    log.note(f"  settings   {CONFIG_FILE}")
    log.note(f"  wallpaper  {INSTALLED_THEME_DIR / BACKGROUND_FILE}")
    log.note(f"  logo       {INSTALLED_THEME_DIR / LOGIN_IMAGE_FILE}")
    log.note("")
    if replaced:
        log.note(f"{replaced} was the display manager; it has been disabled in favour of lxdm.")
    if problems:
        log.note("The problems listed above have to be fixed before a reboot will show the")
        log.note("greeter; until then this machine comes up on the login on tty1.")
        return 1
    if not installed:
        log.note("lxdm itself is not installed yet, so there is nothing to start:")
        log.note(f"  {manual_package_hint()}")
        return 0
    # Everything is in place, so it is started here rather than promised for a
    # reboot: the greeter either appears now, or lxdm's log says what stopped it.
    start_display_manager(log)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
