#!/usr/bin/env python3
# =============================================================================
# GnuchanOS - Plymouth boot splash installer
# -----------------------------------------------------------------------------
# Installs the GnuchanOS boot splash and makes the machine boot with it: the
# wallpaper from assets/bg.png, the GnuchanOS logo from assets/logo.png, a ring
# of dots that turns while the boot is running, a progress bar, the messages
# plymouth asks to have shown, and the panel an encrypted root asks for its
# passphrase in. Standard library only, and no options: running
#
#     python3 settings_plymouth.py
#
# is the whole install.
#
# What it does
# ------------
#   1. installs plymouth itself with the distribution's package manager - apt
#      on Debian and Ubuntu, pacman on Arch, dnf or zypper elsewhere - and
#      reports the exact command when it cannot run one;
#   2. builds the theme in /usr/share/plymouth/themes/GnuchanOS from the two
#      files committed next to this script plus nine images generated here:
#      the wallpaper, the logo on a transparent square, and the dot, bullet,
#      lock, panel, field and two progress images the script draws with. Every
#      one of them is written by the PNG encoder in this file, because Pillow
#      is not installed on a fresh system and is not a dependency worth taking
#      for images that are discs and rounded rectangles;
#   3. puts `splash` on the kernel command line, in /etc/default/grub, because
#      that word is the one thing that decides whether a splash is shown at
#      all: a machine with the theme installed, selected and inside its
#      initramfs still boots to a screen full of kernel messages without it;
#   4. selects the theme by writing the two files plymouthd reads the name
#      from: [Daemon] Theme= in /etc/plymouth/plymouthd.conf, which is what
#      plymouthd and the distribution's initramfs hook both read it from, and
#      the default.plymouth link, which is what the versions older than that
#      file read instead. They are the two files plymouth-set-default-theme
#      writes itself, which is why the selection holds on a machine that has no
#      such tool, and why what is written is read by the same hook the tool's
#      own output is read by;
#   5. regenerates the initramfs with the distribution's own tool -
#      update-initramfs -u -k all, mkinitcpio -P or dracut -f --regenerate-all,
#      this machine's family first and the others after it. This is the step
#      that is easy to leave out and impossible to leave out: plymouth runs from
#      the initramfs, before the root filesystem is mounted, so a theme that is
#      installed and never put in there is a theme that is never drawn - and the
#      machine looks exactly as it did before;
#   6. regenerates grub.cfg with update-grub, or grub-mkconfig where there is
#      no such wrapper, because `splash` only reaches the kernel through the
#      entry that is generated from /etc/default/grub;
#   7. checks the result: the .plymouth file is parsed and both paths in it are
#      looked for, every image the script names is decoded, every function the
#      script calls is compared with the list of functions plymouth's script
#      module really provides - a theme that names one that is not there does
#      not lose that one thing, it fails to load and the whole boot falls back
#      to the text splash - and the kernel command line, the selected theme and
#      the initramfs are read back.
#
# Nothing here reboots the machine. The splash is drawn from the next boot,
# which the script says in its last line.
#
# The palette is the one the GTK theme, the icon theme, the cursor theme, the
# GRUB theme and VSCodium use. dotfile/vscodium_theme/settings.json is the
# source of truth for it; change it there first, then the table below and the
# colours in GnuchanOS/GnuchanOS.script, which repeats them as the three
# numbers between zero and one that Image.Text wants.
#
# Undo
# ----
# There are no flags, so undoing is by hand and always possible:
#
#     rm -rf /usr/share/plymouth/themes/GnuchanOS
#     cp /etc/plymouth/plymouthd.conf.gnuchan-backup /etc/plymouth/plymouthd.conf
#     cp /etc/default/grub.gnuchan-backup /etc/default/grub
#     plymouth-set-default-theme <the theme the backup names>
#     update-initramfs -u -k all
#     update-grub
#
# The first line is not enough on its own, and that is the thing to remember
# about this script: the theme is copied into the initramfs, so until the
# initramfs is generated again the machine keeps booting the theme it already
# has, however thoroughly the directory has been deleted.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import math
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

RGBA = tuple[int, int, int, int]

SCRIPT_PATH = Path(__file__).resolve()
SCRIPT_DIR = SCRIPT_PATH.parent

#: The theme name. It is the directory name in the one place a plymouth theme
#: is installed, the Name= in the .plymouth file and the value the selection is
#: written as, so the three cannot disagree.
THEME_NAME = "GnuchanOS"

#: The theme as it is committed next to this script: the .plymouth descriptor
#: and the script, which name their images by file name and are told the
#: directory they are going into at install time. Every image is generated.
SOURCE_THEME_DIR = SCRIPT_DIR / THEME_NAME
THEME_FILE = f"{THEME_NAME}.plymouth"
SPLASH_SCRIPT_FILE = f"{THEME_NAME}.script"

#: Replaced in both committed files with the directory they are installed into.
#: plymouth resolves neither ImageDir nor ScriptFile relative to the .plymouth
#: file, so both are written out absolute rather than left to be resolved.
THEME_DIR_PLACEHOLDER = "@THEME_DIR@"

#: Where a plymouth theme is installed. It is the compiled-in default of
#: plymouth and not a choice of this script's.
PLYMOUTH_THEMES_DIR = Path("/usr/share/plymouth/themes")
INSTALLED_THEME_DIR = PLYMOUTH_THEMES_DIR / THEME_NAME

#: The file plymouthd is told which theme to draw in, and the link that the
#: versions before that file read instead. Which of the two a machine uses
#: depends on its plymouth, so both are written and both are checked.
PLYMOUTHD_CONF = Path("/etc/plymouth/plymouthd.conf")
DEFAULT_THEME_FILE = PLYMOUTH_THEMES_DIR / "default.plymouth"

#: The daemon, which is what "plymouth is installed" means. A machine that
#: built plymouth from source puts it in one of the other places; a machine
#: with only the client installed has none of them and nothing to draw with.
PLYMOUTHD_PATHS = (
    Path("/usr/sbin/plymouthd"),
    Path("/usr/bin/plymouthd"),
    Path("/usr/libexec/plymouth/plymouthd"),
    Path("/usr/lib/plymouth/plymouthd"),
    Path("/usr/local/sbin/plymouthd"),
)

#: The theme selector. Where it exists it is the right way to select a theme,
#: because it also knows how to regenerate this distribution's initramfs.
THEME_SELECTOR_PATHS = (
    Path("/usr/sbin/plymouth-set-default-theme"),
    Path("/usr/bin/plymouth-set-default-theme"),
    Path("/usr/local/sbin/plymouth-set-default-theme"),
    Path("/usr/local/bin/plymouth-set-default-theme"),
)

#: The suffix every file this script rewrites is kept under, once.
BACKUP_SUFFIX = ".gnuchan-backup"

#: The environment variable that stops a sudo which fails to change the user
#: from re-running the script for ever.
ELEVATED_VARIABLE = "GNUGHAN_PLYMOUTH_ELEVATED"


# --- the images the theme is built from ---------------------------------------

#: The files the installed theme is made of. The first two are the repository's
#: own pictures, copied and scaled; the rest are drawn here.
BACKGROUND_FILE = "background.png"
LOGO_FILE = "logo.png"
DOT_FILE = "dot.png"
BULLET_FILE = "bullet.png"
LOCK_FILE = "lock.png"
PANEL_FILE = "panel.png"
FIELD_FILE = "field.png"
PROGRESS_TRACK_FILE = "progress_track.png"
PROGRESS_FILL_FILE = "progress_fill.png"

#: The names those two assets have in the repository.
BACKGROUND_ASSET = "bg.png"
LOGO_ASSET = "logo.png"

#: The size the logo is written at. The script scales it to the screen; it is
#: written at 256 so that there is something to scale down from on a 4K screen,
#: and the logo is fitted inside the square rather than stretched to fill it.
LOGO_SIZE = 256

#: The dot of the ring and the bullet of the passphrase field. Both are drawn
#: at 64 so the scale down to the size the screen asks for is an average over
#: several pixels rather than a copy, which is what keeps their edges round.
DOT_SIZE = 64
DOT_RADIUS = 27
BULLET_SIZE = 64
BULLET_RADIUS = 23

#: The padlock beside the passphrase field, and its parts. The shackle is an arc
#: drawn as a band, so it is given a centre, a radius and a width.
LOCK_SIZE = 128
LOCK_BODY = (28.0, 60.0, 72.0, 54.0)
LOCK_BODY_RADIUS = 11.0
LOCK_SHACKLE_CENTRE = (64.0, 60.0)
LOCK_SHACKLE_RADIUS = 23.0
LOCK_SHACKLE_WIDTH = 11.0
LOCK_KEYHOLE = (64.0, 80.0)
LOCK_KEYHOLE_RADIUS = 7.0

#: The panel the passphrase is asked in, the field inside it, and the two
#: halves of the progress bar. All four are written at one size and scaled by
#: the script to the share of the screen they take, which is why the corner
#: radius is small: a corner is the one part of a rounded rectangle that does
#: not survive being scaled along one axis, and at this radius it does not
#: have to.
PANEL_WIDTH = 640
PANEL_HEIGHT = 180
PANEL_RADIUS = 18.0
FIELD_WIDTH = 640
FIELD_HEIGHT = 96
FIELD_RADIUS = 10.0
BAR_WIDTH = 640
BAR_HEIGHT = 48
BAR_RADIUS = 24.0

#: How much the panel and the field let through, as the alpha of their fill, so
#: that the wallpaper is still visible behind a passphrase prompt.
PANEL_ALPHA = 236
FIELD_ALPHA = 245

#: Sub-samples per axis when a shape is drawn. Three is enough to make a disc
#: look round at the sizes used here without turning nine images into a slow
#: job.
SAMPLES = 3

#: The palette, from dotfile/vscodium_theme/settings.json and the same table as
#: settings_grub.py and the GTK, icon and cursor themes.
_PALETTE: dict[str, str] = {
    "wallpaper": "#1c0532",
    "bg_darkest": "#09030d",
    "surface_alt": "#180a22",
    "raised": "#1c0b27",
    "border": "#32143f",
    "border_strong": "#6d28d9",
    "accent": "#a855f7",
    "accent_active": "#9333ea",
    "accent_light": "#c77dff",
    "accent_pale": "#d8a4ff",
    "selected_bg": "#542080",
    "fg": "#ead7ff",
    "fg_bright": "#f3e8ff",
    "fg_dim": "#b78ed6",
    "white": "#ffffff",
}


def hex_to_rgba(value: str, alpha: int = 255) -> RGBA:
    """Parse ``#rgb`` or ``#rrggbb`` into an RGBA tuple."""
    text = value.strip().lstrip("#")
    if len(text) == 3:
        text = "".join(character * 2 for character in text)
    if len(text) != 6:
        raise ValueError(f"not a colour: {value!r}")
    return (
        int(text[0:2], 16),
        int(text[2:4], 16),
        int(text[4:6], 16),
        alpha,
    )


PALETTE: dict[str, RGBA] = {
    name: hex_to_rgba(value) for name, value in _PALETTE.items()
}


# --- the kernel command line ---------------------------------------------------

#: The file the kernel command line is read from, and the comment this script
#: writes above the key it owns there. The marker is deliberately not the one
#: settings_grub.py uses: the two scripts own different keys of the same file
#: and each has to leave the other's lines alone.
GRUB_DEFAULT_FILE = Path("/etc/default/grub")
MANAGED_MARKER = "# GnuchanOS plymouth settings, written by settings_plymouth.py"

#: The word that turns the splash on. plymouthd is started by the initramfs
#: whatever the command line says; without `splash` it is started in a mode
#: that never draws, which is why this is not a detail.
SPLASH_TOKEN = "splash"

#: The keys the command line may be in. GRUB_CMDLINE_LINUX_DEFAULT is Debian's
#: and everyone derived from it, and GRUB_CMDLINE_LINUX is what a distribution
#: with no such key uses - Fedora and RHEL among them.
CMDLINE_KEYS = ("GRUB_CMDLINE_LINUX_DEFAULT", "GRUB_CMDLINE_LINUX")

#: The key written when the file has neither, which is a file that has never
#: been edited and is about to be given the one key that matters.
CMDLINE_FALLBACK_KEY = CMDLINE_KEYS[0]


# --- the script module's own functions -----------------------------------------
# The theme is a script the plymouth script module runs, and a name it does not
# provide is not a function that does nothing: calling a member that is not
# there fails, the whole script fails with it, and the boot falls back to the
# text splash - which looks exactly like a theme that was never installed. The
# names below are the ones in src/plugins/splash/script/script-lib-plymouth.c,
# script-lib-sprite.c and script-lib-math.c, and the check at the end of this
# script reports anything used that is not among them.
SCRIPT_LIBRARIES: dict[str, frozenset[str]] = {
    "Plymouth": frozenset(
        {
            "SetRefreshFunction",
            "SetRefreshRate",
            "SetBootProgressFunction",
            "SetRootMountedFunction",
            "SetKeyboardInputFunction",
            "SetUpdateStatusFunction",
            "SetDisplayNormalFunction",
            "SetDisplayPasswordFunction",
            "SetDisplayQuestionFunction",
            "SetDisplayMessageFunction",
            "SetHideMessageFunction",
            "SetQuitFunction",
            "SetSystemUpdateFunction",
            "GetMode",
        }
    ),
    "Window": frozenset(
        {
            "GetWidth",
            "GetHeight",
            "GetX",
            "GetY",
            "SetBackgroundTopColor",
            "SetBackgroundBottomColor",
        }
    ),
    "Image": frozenset(
        {
            "New",
            "Text",
            "Scale",
            "Rotate",
            "Adopt",
            "GetWidth",
            "GetHeight",
        }
    ),
    "Sprite": frozenset(
        {
            "New",
            "SetImage",
            "SetX",
            "SetY",
            "SetZ",
            "SetPosition",
            "SetOpacity",
        }
    ),
    "Math": frozenset(
        {
            "Abs",
            "Min",
            "Max",
            "Clamp",
            "Pi",
            "Cos",
            "Sin",
            "Tan",
            "ATan2",
            "Sqrt",
            "Int",
        }
    ),
}

#: A library member used in the script: ``Plymouth.SetQuitFunction`` and the
#: like. Only the capitalised libraries above are looked at, so the methods of
#: a sprite - ``something.SetOpacity`` - are not mistaken for library calls.
LIBRARY_CALL = re.compile(
    r"\b(" + "|".join(SCRIPT_LIBRARIES) + r")\.([A-Za-z_][A-Za-z0-9_]*)"
)

#: An image the script loads, by the name it gives it. Every one of them has to
#: be in the theme directory: an image that is not there makes Image() return
#: NULL, and the first method called on that NULL is where the script stops.
IMAGE_REFERENCE = re.compile(r"""\bImage\s*\(\s*"([^"]*)"\s*\)""")


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
    a space in it cannot turn into two arguments.

    Keeping the output is for the short commands whose text is only wanted when
    they fail. A package manager, update-initramfs and update-grub are not
    among them: their output is the only sign of what they did, and a run that
    printed nothing for a minute would look like a run that had stopped.
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

    Everything this installer writes - /usr/share/plymouth, /etc/plymouth,
    /etc/default/grub and the initramfs - is root's, so there is no useful
    partial run as a normal user. sudo is used when it exists and the fact is
    announced rather than done silently; the environment variable stops a sudo
    that fails to change the user from looping.
    """
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit(
            "error: still not root after sudo; run the script as root "
            "(su -c 'python3 settings_plymouth.py')"
        )
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: installing a boot splash writes to /etc, /usr/share and the "
            "initramfs; run this script as root"
        )
    log.step("This install is system wide; re-running it through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
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
    """What this distribution calls itself, for the first line of output."""
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


#: The install command per family, without the package names. The apt arguments
#: are the ones settings_lxdm.py uses and for the same reason: -y answers apt's
#: own questions and none of dpkg's, and a run that stops to ask about a
#: configuration file stops before this script has configured anything at all.
#: plymouth's own debconf question - which theme should be the default - is
#: answered non-interactively by package_environment below, and answered
#: properly by this script afterwards into the same file.
PACKAGE_MANAGERS: dict[str, tuple[str, ...]] = {
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

#: The packages to install per family. The theme is a script theme, so the
#: script module has to be there as well as the daemon: Debian and Arch ship it
#: inside the plymouth package, Fedora and openSUSE split it out into
#: plymouth-plugin-script, and a machine without it draws plymouth's built in
#: fallback because there is nothing to run a script theme with.
PACKAGES: dict[str, tuple[str, ...]] = {
    "debian": ("plymouth", "plymouth-themes"),
    "arch": ("plymouth",),
    "fedora": ("plymouth", "plymouth-plugin-script", "plymouth-system-theme"),
    "suse": ("plymouth", "plymouth-plugin-script"),
}

#: The same two tables keyed by the package manager's own program name, which
#: is what is used when /etc/os-release does not name a family this script
#: knows. A distribution built by hand can call itself anything, and the markers
#: above then match nothing; a machine that has apt-get is a machine where
#: plymouth is installed with apt-get, whatever its ID line says.
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


def package_program() -> str | None:
    """The package manager to use, by program name, or None if there is none.

    The distribution decides first, because a family that is recognised has a
    preferred manager and package names of its own. When the family is not
    recognised - or its manager is not installed - the machine decides instead:
    the first of the four managers that is really there.
    """
    family = distro_family()
    preferred = PACKAGE_MANAGERS.get(family, (None,))[0]
    if preferred and shutil.which(preferred):
        return preferred
    for program in PACKAGE_MANAGERS_BY_PROGRAM:
        if shutil.which(program):
            return program
    return None


def package_command() -> list[str] | None:
    """The command that installs plymouth here, or None.

    The manager and the package names always come from the same table, so a
    machine that installs with pacman is never handed the Debian package names.
    """
    program = package_program()
    if program is None:
        return None
    return [*PACKAGE_MANAGERS_BY_PROGRAM[program], *PACKAGES_BY_PROGRAM[program]]


def package_environment() -> dict[str, str]:
    """The environment a package manager is run in.

    On Debian and Ubuntu the debconf frontend is told to be non-interactive.
    This installer has no options and asks nothing: a run that stops on
    debconf's question about which plymouth theme should be the default is a run
    that looks as if it has hung, because the question is asked through a
    terminal nobody is watching.
    """
    return dict(
        os.environ,
        DEBIAN_FRONTEND="noninteractive",
        DEBCONF_NONINTERACTIVE_SEEN="true",
    )


def manual_package_hint() -> str:
    """What to tell the user when the script cannot install the packages.

    The command is built from the manager that was found rather than named by
    hand, so the hint is the command that would have run on this machine.
    """
    command = package_command()
    if command is None:
        return (
            "install plymouth and its script module for your distribution "
            f"({distro_description()}), then run this script again"
        )
    return "run: " + " ".join(command)


def plymouth_present() -> bool:
    """Whether the plymouth daemon is installed.

    The daemon is what matters and not the package: a machine that installed
    plymouth by hand, or built it from source, is one this script can still
    configure. A machine with the client alone has no daemon and nothing to draw
    with, which is a different thing and is not reported as present.
    """
    return any(path.exists() for path in PLYMOUTHD_PATHS)


#: Where the splash modules are kept, as directory patterns. The path has the
#: architecture and the distribution baked into it - /usr/lib/x86_64-linux-gnu
#: on Debian, /usr/lib64 on the RPM family, /usr/lib on Arch - so it is globbed
#: rather than guessed, and the one file looked for is the script module, which
#: is the only thing this theme can be drawn by.
PLUGIN_DIR_PATTERNS = (
    "usr/lib/*/plymouth",
    "usr/lib/plymouth",
    "usr/lib64/plymouth",
    "usr/libexec/plymouth",
    "usr/local/lib/plymouth",
)

SCRIPT_MODULE_NAME = "script.so"


def script_module() -> Path | None:
    """The script module of plymouth, or None.

    The theme is a ModuleName=script theme. Without this file plymouth does not
    fail to draw the theme, it fails to find something to draw it with, and the
    boot shows the text splash that is compiled in - which looks the same as a
    theme that was never selected at all.
    """
    for pattern in PLUGIN_DIR_PATTERNS:
        for directory in sorted(Path("/").glob(pattern)):
            candidate = directory / SCRIPT_MODULE_NAME
            if candidate.is_file():
                return candidate
    return None


def install_packages(log: Log) -> bool:
    """Install plymouth, returning whether it ended up present.

    A failure here is reported and the run continues: the theme and the
    configuration are worth writing even on a machine where the package could
    not be installed, because the moment it arrives the splash is already
    configured.
    """
    if plymouth_present():
        log.detail("plymouth is already installed")
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
    # The output is left where it can be seen: a package manager says what it is
    # downloading and unpacking, and a download that prints nothing for a minute
    # is indistinguishable from a run that has stopped.
    result = run(command, environment=environment)
    if result.returncode != 0:
        log.detail("the package manager reported a failure")
        log.detail(manual_package_hint())
        return plymouth_present()
    if not plymouth_present():
        log.detail("the package manager reported success but plymouthd is not there")
        log.detail(manual_package_hint())
        return False
    log.detail("plymouth is installed")
    return True
# --- the assets the theme is built from ---------------------------------------


def assets_dirs() -> list[Path]:
    """Every place the repository's images may be, most likely first.

    The script lives in dotfile/PLYMOUTH_THEME, so the assets are two
    directories up from it; the rest are where a packager would put them on an
    installed system, which is what makes this usable when the repository is not
    on the machine.
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

    Both are needed before anything is written. A splash with no wallpaper and
    no logo is not the splash that was asked for, and discovering that after
    /etc/default/grub has been rewritten would leave the machine in a state that
    is worse than the one it was found in.
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

    It is copied rather than moved, and that matters: the caller merges keys
    into this same file afterwards, so moving it out of the way first would have
    it merge into nothing and rewrite a file that had lost every key the
    distribution shipped - while looking, in the log, exactly like a run that
    had kept them all.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.copy2(path, backup)
    return backup


def copy_file(log: Log, source: Path, target: Path) -> None:
    """Copy one file, creating its directory, and make it world readable.

    plymouthd reads these files as root long before there is a session, but a
    theme directory is also read by tools running as a normal user - the check
    in this script, for one - so the modes are set explicitly rather than left
    to whatever umask this run inherited.
    """
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    target.chmod(0o644)
    log.detail(f"wrote {target}")


def replace_tree(log: Log, source: Path, target: Path) -> None:
    """Replace a theme directory with the one that was just built.

    The old directory is removed rather than merged over: an image a later
    version of this script no longer generates would otherwise stay behind and
    keep being loaded by a theme directory that is this script's own by
    construction, and a stale image is a stale theme.
    """
    if target.exists():
        shutil.rmtree(target)
    for path in sorted(source.rglob("*")):
        destination = target / path.relative_to(source)
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        destination.chmod(0o644)
    for path in sorted(target.rglob("*")):
        if path.is_dir():
            path.chmod(0o755)
    target.chmod(0o755)
    log.detail(f"installed the theme into {target}")


# --- PNG images ----------------------------------------------------------------
# Every image this theme needs except the wallpaper and the logo is generated
# here, and Pillow is not installed on a fresh system and is not a dependency
# worth taking for images that are discs and rounded rectangles. So the code
# below reads PNG well enough for the repository's own assets - 1, 2, 4, 8 and
# 16 bit grey, grey with alpha, RGB, RGBA and palette images, interlaced or not
# - and writes 8 bit RGBA, which is what plymouth's png reader takes. An image
# that cannot be read is reported rather than guessed at.


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
    hold, so a one bit image is black and white rather than dark grey and light
    grey; a sixteen bit one keeps its high byte, which is what every display
    does with it anyway.
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


def read_png(path: Path) -> "Raster | None":
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
def _inside_rounded(
    px: float, py: float, x: float, y: float, width: float, height: float, radius: float
) -> bool:
    """Whether a point is inside a rounded rectangle."""
    if px < x or py < y or px > x + width or py > y + height:
        return False
    corner = min(radius, min(width, height) / 2.0)
    if corner <= 0.0:
        return True
    left = x + corner
    right = x + width - corner
    top = y + corner
    bottom = y + height - corner
    if left <= px <= right or top <= py <= bottom:
        return True
    centre_x = left if px < left else right
    centre_y = top if py < top else bottom
    return (px - centre_x) ** 2 + (py - centre_y) ** 2 <= corner * corner


def _segment_distance(
    px: float, py: float, ax: float, ay: float, bx: float, by: float
) -> float:
    """Distance from a point to a line segment."""
    dx = bx - ax
    dy = by - ay
    length_squared = dx * dx + dy * dy
    if length_squared <= 0.0:
        return math.hypot(px - ax, py - ay)
    t = ((px - ax) * dx + (py - ay) * dy) / length_squared
    t = min(1.0, max(0.0, t))
    return math.hypot(px - (ax + dx * t), py - (ay + dy * t))


class Raster:
    """An RGBA image in memory, the few shapes this theme is drawn from, and
    the PNG writer that puts it on disk.

    The shapes are the ones a boot splash is made of: filled and stroked
    rounded rectangles for the panel, the field and the two halves of the
    progress bar, a disc for the dot of the ring and the bullet of the
    passphrase field, and an arc for the shackle of the padlock. Every edge is
    antialiased by supersampling, which is what makes a 64 pixel dot still look
    round once plymouth has scaled it down to twenty.
    """

    __slots__ = ("width", "height", "pixels")

    def __init__(self, width: int, height: int, pixels: bytearray | None = None) -> None:
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 4) if pixels is None else pixels

    @classmethod
    def solid(cls, width: int, height: int, colour: RGBA = (0, 0, 0, 0)) -> "Raster":
        """A rectangle of one colour, transparent unless told otherwise."""
        return cls(width, height, bytearray(bytes(colour) * (width * height)))

    # --- drawing ----------------------------------------------------------

    def blend(self, x: int, y: int, colour: RGBA, coverage: float) -> None:
        """Put one pixel of ``colour`` over the image, in source-over order.

        The colours are composited with the alpha of what is already there, so
        two translucent shapes over one another do not multiply their colours
        together the way a plain assignment would.
        """
        if coverage <= 0.0 or not (0 <= x < self.width and 0 <= y < self.height):
            return
        source_alpha = (colour[3] / 255.0) * min(1.0, coverage)
        if source_alpha <= 0.0:
            return
        offset = (y * self.width + x) * 4
        pixels = self.pixels
        destination_alpha = pixels[offset + 3] / 255.0
        out_alpha = source_alpha + destination_alpha * (1.0 - source_alpha)
        if out_alpha <= 0.0:
            pixels[offset:offset + 4] = b"\x00\x00\x00\x00"
            return
        keep = destination_alpha * (1.0 - source_alpha)
        pixels[offset] = round(
            (colour[0] * source_alpha + pixels[offset] * keep) / out_alpha
        )
        pixels[offset + 1] = round(
            (colour[1] * source_alpha + pixels[offset + 1] * keep) / out_alpha
        )
        pixels[offset + 2] = round(
            (colour[2] * source_alpha + pixels[offset + 2] * keep) / out_alpha
        )
        pixels[offset + 3] = round(out_alpha * 255.0)

    def rounded_rect(
        self,
        x: float,
        y: float,
        width: float,
        height: float,
        colour: RGBA,
        radius: float = 0.0,
        filled: bool = True,
        thickness: float = 1.0,
    ) -> None:
        """A filled or stroked rounded rectangle, antialiased by supersampling.

        A stroke sits just inside the edge, so the panel and the hairline border
        around it are this function twice with the same rectangle and different
        arguments.
        """
        inset = 0.0 if filled else thickness / 2.0
        inner_x = x + thickness
        inner_y = y + thickness
        inner_width = width - 2 * thickness
        inner_height = height - 2 * thickness
        inner_radius = max(0.0, radius - thickness)
        step = 1.0 / SAMPLES
        for row in range(int(math.floor(y)), int(math.ceil(y + height))):
            for column in range(int(math.floor(x)), int(math.ceil(x + width))):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if not _inside_rounded(px, py, x, y, width, height, radius):
                            continue
                        if filled:
                            hits += 1
                            continue
                        if inset > 0.0 and inner_width > 0.0 and inner_height > 0.0:
                            if _inside_rounded(
                                px, py, inner_x, inner_y, inner_width, inner_height, inner_radius
                            ):
                                continue
                        hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def line(
        self,
        x0: float,
        y0: float,
        x1: float,
        y1: float,
        colour: RGBA,
        thickness: float = 1.0,
    ) -> None:
        """A line segment of the given width, antialiased."""
        half = thickness / 2.0
        left = int(math.floor(min(x0, x1) - half - 1))
        right = int(math.ceil(max(x0, x1) + half + 1))
        top = int(math.floor(min(y0, y1) - half - 1))
        bottom = int(math.ceil(max(y0, y1) + half + 1))
        step = 1.0 / SAMPLES
        for row in range(top, bottom + 1):
            for column in range(left, right + 1):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if _segment_distance(px, py, x0, y0, x1, y1) <= half:
                            hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def disc(self, centre_x: float, centre_y: float, radius: float, colour: RGBA) -> None:
        """A filled circle, antialiased."""
        self.ring(centre_x, centre_y, radius / 2.0, radius, colour)

    def ring(
        self,
        centre_x: float,
        centre_y: float,
        radius: float,
        thickness: float,
        colour: RGBA,
    ) -> None:
        """A circle outline, drawn as a band of the given width."""
        half = thickness / 2.0
        outer = radius + half
        left = int(math.floor(centre_x - outer - 1))
        right = int(math.ceil(centre_x + outer + 1))
        top = int(math.floor(centre_y - outer - 1))
        bottom = int(math.ceil(centre_y + outer + 1))
        step = 1.0 / SAMPLES
        for row in range(top, bottom + 1):
            for column in range(left, right + 1):
                hits = 0
                for sub_y in range(SAMPLES):
                    for sub_x in range(SAMPLES):
                        px = column + (sub_x + 0.5) * step
                        py = row + (sub_y + 0.5) * step
                        if abs(math.hypot(px - centre_x, py - centre_y) - radius) <= half:
                            hits += 1
                if hits:
                    self.blend(column, row, colour, hits / (SAMPLES * SAMPLES))

    def arc(
        self,
        centre_x: float,
        centre_y: float,
        radius: float,
        thickness: float,
        colour: RGBA,
        start_degrees: float,
        end_degrees: float,
    ) -> None:
        """Part of a ring, from one angle to another, going clockwise.

        Drawn as a run of short segments rather than as a curve, which is what
        these shapes are at the size they are used: the shackle of the padlock
        is half a ring of radius 23, and six degrees a step is finer than the
        edge of the disc it is drawn on.

        The angles are the usual screen ones, with y growing downwards, so 0 is
        to the right, 90 below, 180 to the left and 270 above - which makes the
        upper half of a ring the run from 180 to 360.
        """
        step = 6.0
        angle = start_degrees
        previous: tuple[float, float] | None = None
        while angle <= end_degrees:
            radians = math.radians(angle)
            point = (
                centre_x + radius * math.cos(radians),
                centre_y + radius * math.sin(radians),
            )
            if previous is not None:
                self.line(previous[0], previous[1], point[0], point[1], colour, thickness)
            previous = point
            angle += step
        if previous is not None:
            radians = math.radians(end_degrees)
            self.line(
                previous[0],
                previous[1],
                centre_x + radius * math.cos(radians),
                centre_y + radius * math.sin(radians),
                colour,
                thickness,
            )

    # --- scaling and files ------------------------------------------------

    def scaled_to(self, width: int, height: int) -> "Raster":
        """The image at another size, averaging the pixels it shrinks away.

        Averaging is done on colours premultiplied by their alpha and divided
        back out afterwards: mixing the colours of transparent and opaque pixels
        first is what puts a dark fringe around a logo that is drawn on nothing.
        """
        target = Raster.solid(width, height)
        source = self.pixels
        for target_y in range(height):
            first_y = target_y * self.height // height
            last_y = min(max(first_y + 1, (target_y + 1) * self.height // height), self.height)
            for target_x in range(width):
                first_x = target_x * self.width // width
                last_x = min(max(first_x + 1, (target_x + 1) * self.width // width), self.width)
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

    def fitted(self, size: int) -> "Raster":
        """The image scaled to fit a transparent square of ``size``.

        The proportions are kept, so a logo that is wider than it is tall is not
        squashed into a square, and the square is what the theme draws: the
        script scales the sprite to the size the screen asks for, and a sprite
        has one width and one height with no way to keep an aspect ratio of its
        own.
        """
        scale = min(size / self.width, size / self.height)
        target_width = max(1, min(size, round(self.width * scale)))
        target_height = max(1, min(size, round(self.height * scale)))
        scaled = self.scaled_to(target_width, target_height)
        canvas = Raster.solid(size, size)
        left = (size - target_width) // 2
        top = (size - target_height) // 2
        for y in range(target_height):
            source = y * target_width * 4
            target = ((y + top) * size + left) * 4
            canvas.pixels[target:target + target_width * 4] = scaled.pixels[
                source:source + target_width * 4
            ]
        return canvas

    def write(self, path: Path) -> None:
        """Write the image as an 8 bit RGBA PNG.

        Filter type 0 on every scanline: the point of the encoder is to be short
        and obviously correct, and these images are small.
        """
        stride = self.width * 4
        raw = bytearray()
        for row in range(self.height):
            raw.append(0)
            raw += self.pixels[row * stride:(row + 1) * stride]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(
            PNG_SIGNATURE
            + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
            + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _png_chunk(b"IEND", b"")
        )

    def write_readable(self, path: Path) -> None:
        """Write the image and make it world readable, as the theme expects."""
        self.write(path)
        path.chmod(0o644)


# --- drawing the theme ----------------------------------------------------------


def render_rounded(
    width: int,
    height: int,
    radius: float,
    fill: RGBA,
    border: RGBA | None = None,
    border_thickness: float = 1.0,
) -> Raster:
    """A rounded rectangle: the fill, then the border just inside it."""
    image = Raster.solid(width, height)
    image.rounded_rect(0.0, 0.0, float(width), float(height), fill, radius=radius)
    if border is not None:
        image.rounded_rect(
            0.0,
            0.0,
            float(width),
            float(height),
            border,
            radius=radius,
            filled=False,
            thickness=border_thickness,
        )
    return image


def render_dot() -> Raster:
    """One dot of the ring that turns while the boot is running."""
    image = Raster.solid(DOT_SIZE, DOT_SIZE)
    image.disc(DOT_SIZE / 2.0, DOT_SIZE / 2.0, float(DOT_RADIUS), PALETTE["accent"])
    return image


def render_bullet() -> Raster:
    """One character of a passphrase, as the field shows it.

    A sprite is given an image rather than a shape, so a circle has to exist as
    a file before twelve of them can be drawn across a field.
    """
    image = Raster.solid(BULLET_SIZE, BULLET_SIZE)
    image.disc(
        BULLET_SIZE / 2.0, BULLET_SIZE / 2.0, float(BULLET_RADIUS), PALETTE["accent_pale"]
    )
    return image


def render_lock() -> Raster:
    """The padlock beside the passphrase field: a body, a shackle, a keyhole."""
    body_colour = PALETTE["accent_pale"]
    hole_colour = PALETTE["bg_darkest"]
    image = Raster.solid(LOCK_SIZE, LOCK_SIZE)

    x, y, width, height = LOCK_BODY
    image.rounded_rect(x, y, width, height, body_colour, radius=LOCK_BODY_RADIUS)

    # The shackle's centre is on the top edge of the body and its two ends are
    # at the same height, so the arc meets the body across its whole width and
    # there is no seam between the two.
    centre_x, centre_y = LOCK_SHACKLE_CENTRE
    image.arc(
        centre_x,
        centre_y,
        LOCK_SHACKLE_RADIUS,
        LOCK_SHACKLE_WIDTH,
        body_colour,
        180.0,
        360.0,
    )

    key_x, key_y = LOCK_KEYHOLE
    image.disc(key_x, key_y, LOCK_KEYHOLE_RADIUS, hole_colour)
    image.line(
        key_x,
        key_y,
        key_x,
        key_y + LOCK_KEYHOLE_RADIUS * 1.8,
        hole_colour,
        LOCK_KEYHOLE_RADIUS * 0.9,
    )
    return image


def render_panel() -> Raster:
    """The panel a passphrase is asked in, over the wallpaper."""
    return render_rounded(
        PANEL_WIDTH,
        PANEL_HEIGHT,
        PANEL_RADIUS,
        PALETTE["wallpaper"] + (PANEL_ALPHA,),
        PALETTE["accent"] + (150,),
        2.0,
    )


def render_field() -> Raster:
    """The field inside the panel that the passphrase is typed into."""
    return render_rounded(
        FIELD_WIDTH,
        FIELD_HEIGHT,
        FIELD_RADIUS,
        PALETTE["surface_alt"] + (FIELD_ALPHA,),
        PALETTE["border_strong"] + (220,),
        2.0,
    )


def render_progress_track() -> Raster:
    """The empty bar below the ring."""
    return render_rounded(
        BAR_WIDTH,
        BAR_HEIGHT,
        BAR_RADIUS,
        PALETTE["surface_alt"] + (235,),
        PALETTE["border"] + (255,),
        2.0,
    )


def render_progress_fill() -> Raster:
    """The part of that bar the boot has got through."""
    return render_rounded(
        BAR_WIDTH,
        BAR_HEIGHT,
        BAR_RADIUS,
        PALETTE["accent"] + (255,),
    )


def theme_image_names() -> tuple[str, ...]:
    """Every image file the installed theme is expected to have."""
    return (
        BACKGROUND_FILE,
        LOGO_FILE,
        DOT_FILE,
        BULLET_FILE,
        LOCK_FILE,
        PANEL_FILE,
        FIELD_FILE,
        PROGRESS_TRACK_FILE,
        PROGRESS_FILL_FILE,
    )


def write_theme_images(log: Log, directory: Path, background: Path, logo: Path) -> None:
    """Write every image the theme is made of into ``directory``.

    The wallpaper is copied byte for byte: the script scales it to the screen
    itself, and decoding and re-encoding four megabytes of photograph would cost
    time and gain nothing. The logo is read and fitted onto a transparent
    square, and everything else is drawn here from the palette, so the only
    images anyone has to keep in step by hand are the two in assets/.
    """
    copy_file(log, background, directory / BACKGROUND_FILE)

    image = read_png(logo)
    if image is None:
        raise SystemExit(
            f"error: {logo} is not a PNG this script can read; the logo has to be "
            "a PNG for the splash to show it"
        )
    image.fitted(LOGO_SIZE).write_readable(directory / LOGO_FILE)
    log.detail(f"wrote {directory / LOGO_FILE} ({LOGO_SIZE}x{LOGO_SIZE})")

    drawn = (
        (DOT_FILE, render_dot()),
        (BULLET_FILE, render_bullet()),
        (LOCK_FILE, render_lock()),
        (PANEL_FILE, render_panel()),
        (FIELD_FILE, render_field()),
        (PROGRESS_TRACK_FILE, render_progress_track()),
        (PROGRESS_FILL_FILE, render_progress_fill()),
    )
    for name, rendered in drawn:
        rendered.write_readable(directory / name)
    log.detail(f"drew {len(drawn)} images into {directory}")
# --- building and installing the theme ------------------------------------------


def render_theme_files(log: Log, directory: Path, installed: Path) -> None:
    """Write the .plymouth descriptor and the script into the theme directory.

    The descriptor names the directory the theme is *installed* into twice -
    once as the ImageDir every Image() in the script is resolved against, and
    once as the script itself - and neither is resolved relative to the file, so
    both are written out absolute. It is the installed directory and not the one
    being written: the theme is rendered in a temporary directory and moved into
    place in one step, so a descriptor naming the temporary directory would name
    a directory that is removed before plymouth reads it, and the splash would
    fall back to the text one with every file in place.
    """
    directory.mkdir(parents=True, exist_ok=True)
    here = installed.as_posix()
    descriptor = read_text(SOURCE_THEME_DIR / THEME_FILE)
    if not descriptor:
        raise SystemExit(
            f"error: {SOURCE_THEME_DIR / THEME_FILE} is missing or empty; "
            "it is the theme this script installs"
        )
    if THEME_DIR_PLACEHOLDER not in descriptor:
        raise SystemExit(
            f"error: {SOURCE_THEME_DIR / THEME_FILE} does not name "
            f"{THEME_DIR_PLACEHOLDER}, so the theme directory could not be "
            "substituted into it"
        )
    (directory / THEME_FILE).write_text(
        descriptor.replace(THEME_DIR_PLACEHOLDER, here),
        encoding="utf-8",
    )
    log.detail(f"wrote {directory / THEME_FILE}")

    script = read_text(SOURCE_THEME_DIR / SPLASH_SCRIPT_FILE)
    if not script:
        raise SystemExit(
            f"error: {SOURCE_THEME_DIR / SPLASH_SCRIPT_FILE} is missing or empty; "
            "it is what draws the theme"
        )
    (directory / SPLASH_SCRIPT_FILE).write_text(
        script.replace(THEME_DIR_PLACEHOLDER, here),
        encoding="utf-8",
    )
    log.detail(f"wrote {directory / SPLASH_SCRIPT_FILE}")


def build_theme(log: Log, background: Path, logo: Path) -> Path:
    """Render the whole theme into a fresh temporary directory.

    It is built once and moved into place in one step, so a run that stops part
    way through - a full disk, a wallpaper that turns out not to be a PNG, an
    interrupted terminal - leaves the theme that was already installed exactly
    where it was rather than a half written one where plymouth reads it. A
    splash whose images are half there is not a splash that draws less, it is
    one that does not draw at all.
    """
    build = Path(tempfile.mkdtemp(prefix="gnuchan-plymouth-theme-"))
    # The descriptor is written for the directory the theme is going to and not
    # for this one: see render_theme_files for why that is the difference
    # between a splash and the text one.
    render_theme_files(log, build, INSTALLED_THEME_DIR)
    write_theme_images(log, build, background, logo)
    return build


def install_theme(log: Log, build: Path) -> None:
    """Put the built theme where plymouth reads it."""
    replace_tree(log, build, INSTALLED_THEME_DIR)


def cleanup(log: Log, build: Path) -> None:
    """Remove the temporary directory the theme was rendered into.

    It is removed whether the run succeeded or failed: what it held has been
    copied into the theme directory, and a directory of a few hundred kilobytes
    of PNGs left under /tmp every time this runs is litter nobody goes looking
    for.
    """
    try:
        shutil.rmtree(build)
    except OSError as error:
        log.detail(f"could not remove the build directory {build}: {error}")


# --- the kernel command line ----------------------------------------------------
# plymouthd is started by the initramfs whether or not the command line asks for
# it, and it stays silent unless the line carries `splash`: the word is the
# switch that turns the splash on at all. Everything else on the line is the
# machine's - the root filesystem, the resume image, the distribution's own
# arguments - and is left exactly as it was found.


def assignment(line: str) -> tuple[str, str] | None:
    """The name and value of a shell assignment, or None.

    Only the exact shape ``NAME=value``, with an optional leading ``export``, is
    recognised, and a line that starts with a comment is not one. That is what
    makes the merge idempotent, and it is the same reader settings_grub.py
    writes with, so the two scripts see the same file the same way.
    """
    text = line.strip()
    if not text or text.startswith("#"):
        return None
    if text.startswith("export "):
        text = text[len("export "):].strip()
    name, separator, value = text.partition("=")
    name = name.strip()
    if not separator or not name:
        return None
    if not all(character.isalnum() or character == "_" for character in name):
        return None
    return name, value


def read_grub_key(text: str, key: str) -> str | None:
    """The value of a key in /etc/default/grub, unquoted, or None."""
    for line in text.splitlines():
        entry = assignment(line)
        if entry is not None and entry[0] == key:
            return entry[1].strip().strip('"').strip("'")
    return None


def cmdline_key(existing: str) -> str:
    """Which key the kernel command line is in on this machine.

    Debian and everyone derived from it has GRUB_CMDLINE_LINUX_DEFAULT, which is
    the line the user edits; the RPM family has only GRUB_CMDLINE_LINUX, which
    is the line everything else appends to. Whichever of the two is already in
    the file is the one written, and a file with neither - which is a file that
    has never been touched - is given the Debian one.
    """
    for key in CMDLINE_KEYS:
        if read_grub_key(existing, key) is not None:
            return key
    return CMDLINE_FALLBACK_KEY


def merge_splash(existing: str, key: str) -> str:
    """Add ``splash`` to the kernel command line, keeping every other word.

    The value is read, split into words, given ``splash`` if it does not already
    have it, and written back quoted; every other word of it and every other
    line of the file is left as it was. The value is quoted because the file is
    a shell fragment that the header sources, and an unquoted command line is
    one whose words become separate arguments to whatever reads the file.
    """
    words = [word for word in (read_grub_key(existing, key) or "").split() if word]
    if SPLASH_TOKEN not in words:
        words.append(SPLASH_TOKEN)
    value = '"' + " ".join(words) + '"'

    written = False
    body: list[str] = []
    for line in existing.splitlines():
        if line.strip() == MANAGED_MARKER:
            continue
        entry = assignment(line)
        if entry is None:
            body.append(line)
            continue
        name, _ = entry
        if name == key:
            if written:
                continue  # a duplicate of the key that was just written
            written = True
            body.append(f"{name}={value}")
            continue
        body.append(line)

    if not written:
        if body and body[-1].strip():
            body.append("")
        body.append(f"{key}={value}")

    for position, line in enumerate(body):
        entry = assignment(line)
        if entry is not None and entry[0] == key:
            body.insert(position, MANAGED_MARKER)
            break

    text = "\n".join(body).strip("\n")
    return text + "\n" if text else ""


def install_splash(log: Log) -> None:
    """Put ``splash`` in /etc/default/grub.

    The file as it was is copied aside once, before the key is merged into it,
    because that copy is the whole undo: putting it back and running update-grub
    is what takes the splash off the kernel command line again.
    """
    existing = read_text(GRUB_DEFAULT_FILE)
    if not existing:
        log.detail(f"{GRUB_DEFAULT_FILE} does not exist; it will be created")
    key = cmdline_key(existing)
    backup = backup_once(GRUB_DEFAULT_FILE)
    if backup is not None:
        log.detail(f"backed up {GRUB_DEFAULT_FILE.name} to {backup.name}")
    merged = merge_splash(existing, key)
    write_text(GRUB_DEFAULT_FILE, merged)
    log.detail(f"{key}={read_grub_key(merged, key)}")


# --- regenerating grub.cfg ------------------------------------------------------

#: The GRUB directory candidates, in the order worth trying. Debian and every
#: distribution derived from it uses /boot/grub; the RPM family uses /boot/grub2.
GRUB_BOOT_DIRS = (Path("/boot/grub"), Path("/boot/grub2"))


def grub_boot_dir() -> Path | None:
    """The GRUB directory this machine boots from, or None."""
    for directory in GRUB_BOOT_DIRS:
        if (directory / "grub.cfg").is_file():
            return directory
    for directory in GRUB_BOOT_DIRS:
        if directory.is_dir():
            return directory
    return None


def grub_mkconfig_command(boot_dir: Path) -> list[str] | None:
    """The command that regenerates grub.cfg here, or None.

    ``update-grub`` is Debian's wrapper and knows the right output path, so it
    is preferred where it exists; grub-mkconfig is told which file to write,
    which is what makes this work on a distribution that has no wrapper.
    """
    update_grub = shutil.which("update-grub")
    if update_grub is not None:
        return [update_grub]
    mkconfig = shutil.which("grub-mkconfig") or shutil.which("grub2-mkconfig")
    if mkconfig is None:
        return None
    return [mkconfig, "-o", str(boot_dir / "grub.cfg")]


def regenerate_grub_config(log: Log, boot_dir: Path) -> bool:
    """Run update-grub, returning whether grub.cfg was rewritten.

    A command line change only reaches the kernel through the entry generated
    from /etc/default/grub, so the file has to be generated again for `splash`
    to be on it. Nothing is regenerated by hand: grub.cfg is generated, and one
    written here would be overwritten by the next package upgrade and would miss
    whichever entries the machine's own /etc/grub.d adds.
    """
    command = grub_mkconfig_command(boot_dir)
    if command is None:
        log.warn(
            "neither update-grub nor grub-mkconfig is installed, so grub.cfg was "
            "not regenerated and `splash` will not reach the kernel until one of "
            "them is run"
        )
        return False
    log.detail("running: " + " ".join(command))
    # The output is left on the terminal: it is the only sign of what the
    # command did, and a run that printed nothing for a minute would look like a
    # run that had stopped.
    result = run(command)
    if result.returncode != 0:
        log.warn(f"{command[0]} failed; grub.cfg was not regenerated")
        return False
    log.detail(f"regenerated {boot_dir / 'grub.cfg'}")
    return True
# --- selecting the theme --------------------------------------------------------
# plymouthd draws the theme whose name it is given, and it is given one in two
# places: [Daemon] Theme= in /etc/plymouth/plymouthd.conf, which is what modern
# plymouth and the distribution's initramfs hook both read the name from, and
# the default.plymouth link, which is what the versions older than that file read
# instead. Both are written here, and they are the two files
# plymouth-set-default-theme writes itself - which is why the selection holds on
# a machine that has no such tool, and why what is written is read by the same
# hook the tool's own output is read by.


def theme_selector() -> Path | None:
    """plymouth's own theme selector, or None when this machine has none.

    It is not what selects the theme here: the two files below are, so that the
    selection can be read back, and so that it is written on a machine whose
    plymouth ships no selector at all. It is what *reports* the selection, which
    is why the check at the end of this script asks it and not the files, and
    the report is the same string the initramfs hook asks it for.
    """
    for path in THEME_SELECTOR_PATHS:
        if path.is_file():
            return path
    found = shutil.which("plymouth-set-default-theme")
    return Path(found) if found else None


def read_ini_value(text: str, section: str, key: str) -> str | None:
    """Value of ``key`` in ``section`` of a key file, or None.

    Deliberately small, and shared with the check: plymouthd.conf is a file
    people edit by hand, and a parser that raises on a duplicate key or a stray
    line is a parser that stops the install.
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


def plymouthd_theme(text: str) -> str | None:
    """The theme /etc/plymouth/plymouthd.conf names, or None."""
    return read_ini_value(text, "Daemon", "Theme")


def merge_plymouthd_theme(existing: str, theme: str) -> str:
    """Set the theme in plymouthd.conf, keeping every other line of it.

    The key is replaced where it already stands and the group is added at the
    end when the file has none; a duplicate of the key is dropped, because two
    Theme= lines are a file whose meaning depends on which one plymouthd reads
    last. Comments, blank lines and the other keys of the group - the show
    delay, the device timeout, the debug switches - are left as they were found.
    """
    written = False
    current: str | None = None
    body: list[str] = []
    for line in existing.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            current = stripped[1:-1].strip()
            body.append(line)
            continue
        if (
            current is not None
            and current.lower() == "daemon"
            and stripped
            and not stripped.startswith(("#", ";"))
            and "=" in stripped
        ):
            name = stripped.split("=", 1)[0].strip()
            if name.lower() == "theme":
                if written:
                    continue
                written = True
                body.append(f"Theme={theme}")
                continue
        body.append(line)

    if not written:
        if body and body[-1].strip():
            body.append("")
        body.append("[Daemon]")
        body.append(f"Theme={theme}")
    text = "\n".join(body).strip("\n")
    return text + "\n" if text else ""


def install_theme_selection(log: Log) -> None:
    """Point plymouthd at this theme in the two files it reads.

    The configuration is copied aside once before it is merged into, so undoing
    the selection is putting that file back - see the undo at the top of this
    file. The link is only replaced when it does not already point at the theme:
    a link that already names this theme is one this script wrote, and rewriting
    it on every run would be a change to the machine with nothing to show for
    it.
    """
    existing = read_text(PLYMOUTHD_CONF)
    if not PLYMOUTHD_CONF.exists():
        log.detail(f"{PLYMOUTHD_CONF} does not exist yet; it will be created")
    backup = backup_once(PLYMOUTHD_CONF)
    if backup is not None:
        log.detail(f"backed up {PLYMOUTHD_CONF.name} to {backup.name}")
    write_text(PLYMOUTHD_CONF, merge_plymouthd_theme(existing, THEME_NAME))
    log.detail(f"{PLYMOUTHD_CONF}: Theme={THEME_NAME}")

    link = DEFAULT_THEME_FILE
    wanted = INSTALLED_THEME_DIR / THEME_FILE
    try:
        if (link.is_symlink() or link.exists()) and link.resolve() == wanted.resolve():
            log.detail(f"{link} already points at the theme")
            return
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.exists() or link.is_symlink():
            link.unlink()
        link.symlink_to(wanted)
        log.detail(f"linked {link} -> {wanted}")
    except OSError as error:
        log.warn(f"could not link {link}: {error}")


# --- the initramfs --------------------------------------------------------------
# plymouth runs from the initramfs, before the root filesystem is mounted, so
# the theme has to be inside it: one that is installed and selected but never put
# in there is never drawn, and the machine looks exactly as it did before. Which
# tool does that is the distribution's decision, so they are all known here and
# this machine's own family is tried first.

#: The commands that regenerate the initramfs, by family, this family's own
#: first. -u -k all is update-initramfs's spelling of "every kernel", -P is
#: mkinitcpio's spelling of "every preset", and --regenerate-all is dracut's.
#: Each entry is the program and its arguments with the program first, and what
#: is really installed on PATH is what decides which of them is used - a family
#: is not the same thing as an installation.
INITRAMFS_TOOLS: dict[str, tuple[tuple[str, ...], ...]] = {
    "debian": (
        ("update-initramfs", "-u", "-k", "all"),
        ("mkinitcpio", "-P"),
        ("dracut", "-f", "--regenerate-all"),
    ),
    "arch": (
        ("mkinitcpio", "-P"),
        ("update-initramfs", "-u", "-k", "all"),
        ("dracut", "-f", "--regenerate-all"),
    ),
    "fedora": (
        ("dracut", "-f", "--regenerate-all"),
        ("mkinitcpio", "-P"),
        ("update-initramfs", "-u", "-k", "all"),
    ),
    "suse": (
        ("dracut", "-f", "--regenerate-all"),
        ("mkinitcpio", "-P"),
        ("update-initramfs", "-u", "-k", "all"),
    ),
}

#: What is tried on a machine whose distribution this script cannot name.
FALLBACK_INITRAMFS_TOOLS = INITRAMFS_TOOLS["debian"]


def initramfs_commands() -> list[list[str]]:
    """Every command that regenerates the initramfs here, best first.

    A machine whose /etc/os-release says something this script has never heard
    of still has one of these three if it has an initramfs at all, which is why
    the family only orders the list and the lookup decides what is in it.
    """
    commands: list[list[str]] = []
    for tool in INITRAMFS_TOOLS.get(distro_family(), FALLBACK_INITRAMFS_TOOLS):
        program = shutil.which(tool[0])
        if program is not None:
            commands.append([program, *tool[1:]])
    return commands


def regenerate_initramfs(log: Log) -> bool:
    """Put the theme into the initramfs, returning whether it worked.

    The first command that succeeds is the one that did it and the rest are not
    run: regenerating an initramfs is minutes of work on a machine with several
    kernels, and doing it twice for one change is a cost with nothing to show
    for it. A failure is reported with the commands that have to be run by hand,
    because the one thing this step cannot do is leave quietly - a theme that was
    never put in the initramfs is not drawn, whatever else is right.
    """
    for command in initramfs_commands():
        log.detail("running: " + " ".join(command))
        # The output goes to the terminal: it is the only sign of what the
        # command did, and a run that printed nothing for a minute would look
        # like a run that had stopped.
        if run(command).returncode == 0:
            log.detail("the initramfs was regenerated and now carries the theme")
            return True
        log.warn(f"{command[0]} failed")
    log.warn(
        "the initramfs was not regenerated, so the machine keeps booting the "
        "splash it already had however well the theme is installed and selected; "
        "run one of: update-initramfs -u -k all, mkinitcpio -P, "
        "dracut -f --regenerate-all"
    )
    return False
# --- checking the result -------------------------------------------------------
# Everything here is read back from what was written rather than trusted: the
# descriptor is parsed and both paths in it are looked for, every image the
# script names is decoded, every function the script calls is compared with the
# ones plymouth's script module really provides, and the kernel command line,
# the selection and the initramfs are read back. A theme that names a function
# that is not there, or an image that is not there, does not lose that one thing:
# it fails to load, and the boot falls back to the text splash - which looks
# exactly like a theme that was never installed at all.


def descriptor_values(text: str) -> dict[str, str]:
    """The keys of a .plymouth file, both groups flattened into one dictionary.

    The file has a [Plymouth Theme] group and a [script] one, and the three keys
    this script cares about - ModuleName, ImageDir and ScriptFile - do not
    collide, so one dictionary is enough and is what the checks below read. The
    group a key is in is not kept: a descriptor that names ScriptFile in the
    wrong group is refused by plymouth anyway, and reporting it as a missing key
    is the same problem said in one line instead of two.
    """
    values: dict[str, str] = {}
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", "[")):
            continue
        name, separator, value = stripped.partition("=")
        if not separator:
            continue
        values[name.strip().lower()] = value.strip()
    return values


def check_theme_files() -> list[str]:
    """Problems with the two committed files and the nine generated images."""
    problems: list[str] = []
    descriptor = INSTALLED_THEME_DIR / THEME_FILE
    script = INSTALLED_THEME_DIR / SPLASH_SCRIPT_FILE
    for path in (descriptor, script):
        if not path.is_file():
            problems.append(f"{path} is missing")
        elif path.stat().st_size == 0:
            problems.append(f"{path} is empty")
        elif THEME_DIR_PLACEHOLDER in read_text(path):
            problems.append(f"{path} still names {THEME_DIR_PLACEHOLDER}")

    values = descriptor_values(read_text(descriptor))
    if values.get("modulename", "") != "script":
        problems.append(
            f"{descriptor} names the module {values.get('modulename', '')!r}; this "
            "theme is a script theme and is drawn by nothing else"
        )
    for key, wanted in (("imagedir", INSTALLED_THEME_DIR), ("scriptfile", script)):
        named = values.get(key)
        if not named:
            problems.append(f"{descriptor} does not name {key}")
        elif Path(named) != wanted and not Path(named).is_file():
            problems.append(f"{descriptor} names {key}={named}, which is not there")

    for name in theme_image_names():
        path = INSTALLED_THEME_DIR / name
        if not path.is_file():
            problems.append(f"{path} is missing")
        elif path.stat().st_size == 0:
            problems.append(f"{path} is empty")
        elif read_png(path) is None:
            problems.append(f"{path} is not a PNG plymouth can read")
    return problems


def check_theme_script() -> list[str]:
    """Problems that would stop the theme's script from drawing.

    The two lists this compares against are the reason the script can be checked
    at all before a reboot: a library member that is not in SCRIPT_LIBRARIES is a
    call that fails, and an image that is not in the theme directory is an
    Image() that returns NULL and a method called on nothing one line later.
    Either way the script stops and the whole boot falls back to the text splash.
    """
    problems: list[str] = []
    path = INSTALLED_THEME_DIR / SPLASH_SCRIPT_FILE
    text = read_text(path)
    if not text:
        return [f"{path} is missing or empty"]

    known = {
        f"{library}.{member}"
        for library, members in SCRIPT_LIBRARIES.items()
        for member in members
    }
    for library, member in sorted(set(LIBRARY_CALL.findall(text))):
        call = f"{library}.{member}"
        if call not in known:
            problems.append(
                f"{path} calls {call}, which plymouth's script module does not "
                "provide: the script fails and the boot falls back to the text splash"
            )

    for name in sorted(set(IMAGE_REFERENCE.findall(text))):
        if not (INSTALLED_THEME_DIR / name).is_file():
            problems.append(
                f"{path} loads {name!r}, which is not in {INSTALLED_THEME_DIR}, so "
                "the image is NULL and the first method called on it fails"
            )
    return problems


def check_splash_token() -> list[str]:
    """Problems with the kernel command line the splash is switched on by."""
    text = read_text(GRUB_DEFAULT_FILE)
    if not text:
        return [f"{GRUB_DEFAULT_FILE} is missing or empty"]
    key = cmdline_key(text)
    if SPLASH_TOKEN not in (read_grub_key(text, key) or "").split():
        return [
            f"{GRUB_DEFAULT_FILE} does not put `{SPLASH_TOKEN}` on the kernel "
            "command line, and plymouthd draws nothing without it: the theme is "
            "installed, selected and in the initramfs, and none of it is shown"
        ]
    return []


def check_selection() -> list[str]:
    """Problems with the two files that say which theme plymouthd draws."""
    problems: list[str] = []
    conf = read_text(PLYMOUTHD_CONF)
    if not conf:
        problems.append(f"{PLYMOUTHD_CONF} is missing or empty")
    else:
        named = plymouthd_theme(conf)
        if named != THEME_NAME:
            problems.append(
                f"{PLYMOUTHD_CONF} names the theme {named!r} instead of "
                f"{THEME_NAME!r}, so plymouthd draws another theme and the "
                "initramfs hook copies another one in"
            )

    link = DEFAULT_THEME_FILE
    wanted = INSTALLED_THEME_DIR / THEME_FILE
    if not (link.exists() or link.is_symlink()):
        problems.append(
            f"{link} is missing, and the plymouth versions older than "
            "plymouthd.conf resolve the theme through it"
        )
    elif link.resolve() != wanted.resolve():
        problems.append(f"{link} points at {link.resolve()} instead of {wanted}")
    elif not wanted.is_file():
        problems.append(f"{link} points at {wanted}, which is not there")
    return problems


def initramfs_image() -> Path | None:
    """The initramfs of the running kernel, or None when it cannot be named.

    The kernel release is asked of the running system rather than guessed at,
    because the image is named after it: initrd.img-<release> on Debian,
    initramfs-<release>.img on the RPM family, initramfs-<release> on Arch. A
    machine that keeps its images somewhere other than /boot, or names them some
    other way - a container, a machine that boots with something else - has no
    image to look at and is reported as having none rather than as being wrong.
    """
    release = platform.release()
    for name in (
        f"initrd.img-{release}",
        f"initramfs-{release}.img",
        f"initramfs-{release}",
        f"initrd-{release}",
    ):
        candidate = Path("/boot") / name
        if candidate.is_file():
            return candidate
    return None


def check_initramfs() -> list[str]:
    """Problems with the initramfs the theme has to be inside.

    The image is dated rather than opened. The theme was written a moment ago,
    so an image older than it was generated before it and cannot contain it -
    which is the whole of the failure this check exists for, and the one that is
    invisible from every other file on the machine. Nothing is reported on a
    machine with no image to compare against.
    """
    image = initramfs_image()
    if image is None:
        return []
    try:
        image_time = image.stat().st_mtime
    except OSError:
        return []
    for name in (THEME_FILE, SPLASH_SCRIPT_FILE):
        path = INSTALLED_THEME_DIR / name
        try:
            if path.stat().st_mtime > image_time:
                return [
                    f"{image} is older than {path}, so it was generated before "
                    "the theme was installed and does not have it in it: the "
                    "machine boots the splash it already had until the initramfs "
                    "is generated again"
                ]
        except OSError:
            continue
    return []


def check_plymouth() -> list[str]:
    """Problems with the things that draw the theme at all.

    The selector is asked what it reports, because that is the string the
    distribution's own initramfs hook reads the theme name from: a machine whose
    hook would copy a different theme into the initramfs is a machine that boots
    a different theme, however right the two files this script wrote are.
    """
    problems: list[str] = []
    if not plymouth_present():
        problems.append("plymouth is not installed, so nothing draws a boot splash")
    if script_module() is None:
        problems.append(
            "plymouth's script module is not installed, and this theme is drawn "
            "by nothing else: install plymouth-themes on Debian, or "
            "plymouth-plugin-script on Fedora and openSUSE"
        )
    selector = theme_selector()
    if selector is not None:
        result = run([str(selector)], capture=True)
        reported = (result.stdout or "").strip()
        if result.returncode == 0 and reported and reported != THEME_NAME:
            problems.append(
                f"{selector} reports the theme {reported!r} rather than "
                f"{THEME_NAME!r}, so the initramfs hook copies another theme in"
            )
    return problems


def check_result(log: Log, rebuilt: bool) -> int:
    """Report what is wrong with the install, returning how many things are.

    The theme files come first and the initramfs last, because that is the order
    they depend on each other in: a missing image makes the theme fail to load,
    and a theme that was never put in the initramfs is not drawn whatever else is
    right. The initramfs is dated only when this run did not regenerate it: a run
    that did has just written an image newer than the theme, and dating it again
    would compare the two files it wrote itself.
    """
    problems = (
        check_theme_files()
        + check_theme_script()
        + check_splash_token()
        + check_selection()
        + check_plymouth()
    )
    if not rebuilt:
        problems.extend(check_initramfs())
    if not problems:
        log.note(f"No problems found in {INSTALLED_THEME_DIR}.")
        return 0
    log.note(f"{len(problems)} problem(s) found:")
    for problem in problems:
        log.note(f"  {problem}")
    return len(problems)


# --- entry point ---------------------------------------------------------------


def main() -> int:
    """Install the GnuchanOS boot splash, with no options to pass."""
    log = Log()
    ensure_root(log)

    log.step(f"Installing the GnuchanOS Plymouth theme ({distro_description()})")

    # The images are resolved before anything is written, so a machine that does
    # not have them is left exactly as it was found.
    background, logo = require_assets()
    log.detail(f"wallpaper: {background}")
    log.detail(f"logo: {logo}")
    installed = install_packages(log)

    log.step("Building the theme")
    build = build_theme(log, background, logo)

    try:
        log.step("Installing the theme")
        install_theme(log, build)

        log.step("Selecting the theme")
        install_theme_selection(log)

        log.step(f"Putting `{SPLASH_TOKEN}` on the kernel command line")
        install_splash(log)

        log.step("Regenerating the initramfs")
        rebuilt = regenerate_initramfs(log)

        log.step("Regenerating grub.cfg")
        boot_dir = grub_boot_dir()
        if boot_dir is None:
            log.warn(
                "there is no /boot/grub or /boot/grub2, so grub.cfg was not "
                f"regenerated and `{SPLASH_TOKEN}` will not reach the kernel "
                "until it is"
            )
        else:
            regenerate_grub_config(log, boot_dir)

        log.step("Checking the result")
        problems = check_result(log, rebuilt)
    finally:
        cleanup(log, build)

    log.note("")
    if installed:
        log.note("Plymouth is installed and will boot with the GnuchanOS splash:")
    else:
        log.note(
            "The theme and the configuration are in place, but plymouth itself "
            "is not installed yet:"
        )
    if not installed:
        log.note(f"  {manual_package_hint()}")
    log.note(f"  theme      {INSTALLED_THEME_DIR}")
    log.note(f"  settings   {PLYMOUTHD_CONF}")
    log.note(f"  wallpaper  {INSTALLED_THEME_DIR / BACKGROUND_FILE}")
    log.note(f"  logo       {INSTALLED_THEME_DIR / LOGO_FILE}")
    log.note("")
    if problems:
        log.note("The problems listed above have to be fixed before the splash will be")
        log.note("drawn; until then the machine boots the splash it already has.")
    else:
        log.note("The splash is drawn from the next boot, and not before: plymouth")
        log.note("reads the theme out of the initramfs, once, at boot.")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
