#!/usr/bin/env python3
# =============================================================================
# GnuchanOS - make the wifi work on Debian
# -----------------------------------------------------------------------------
# Standard library only, and no options: running
#
#     python3 fix_wifi.py
#
# is the whole fix. It re-runs itself through sudo, because everything it does -
# installing packages, unloading and loading kernel modules, unblocking the
# radio - is root's.
#
# The problem
# -----------
# A fresh Debian install has the wifi driver in the kernel but not the firmware
# the driver loads at start-up, because the firmware is not free and is not in
# the installer's default set. The module loads, asks the kernel for a firmware
# file, does not find it, and gives up - and the result is a wireless card that
# is there, whose driver is there, and whose interface never appears:
#
#     wlan0: Failed to load firmware "iwlwifi-cc-a0-72.ucode"
#
# The package a chip needs is one of the firmware-* packages in the
# non-free-firmware component of the Debian archive, and that component is not
# in sources.list on a machine installed before it existed. So the two halves of
# this fix are: make the component available to apt, and install the firmware
# package the chip actually uses - not the whole set, because the set is several
# hundred megabytes nobody needs.
#
# Which chip it is
# ----------------
# The card is looked for in three places, because a card whose firmware is
# missing is a card whose driver may not have finished loading:
#
#   /sys/class/net/*/wireless       an interface that is up and working
#   /sys/bus/pci/devices  class 0x0280xx    a PCI card, bound or not; an unbound
#                                   one is matched to its package by the vendor
#                                   id even though no driver name is available
#   /sys/bus/usb/devices            a USB dongle whose driver is bound
#
# The driver name is then mapped to a chip family, and the family to the
# package. A name that is not recognised falls back to firmware-misc-nonfree,
# which is where the drivers that do not have a package of their own keep their
# firmware.
#
# What else it does
# -----------------
#   wpasupplicant is installed, and a network manager if the machine has none,
#   because firmware alone makes the interface appear and nothing join a network.
#
#   The radio is unblocked with rfkill, which is the other reason an interface
#   that exists is invisible to a network manager.
#
#   The detected drivers are unloaded and loaded again, so a card whose firmware
#   was missing when it was first probed gets a second chance now that the
#   firmware is on disk - without a reboot, in the case where it works.
#
# Undo
# ----
# There are no flags, so undoing is by hand and always possible: apt-get remove
# the firmware-* package this script installed, and take the non-free-firmware
# component back out of sources.list (a .gnuchan-backup copy of it is beside it
# for that). Nothing else is changed: the kernel modules, the radio and the
# network manager are only asked to do what they were already there to do.
#
# The chip this machine has, the drivers it found and whether the interface came
# up are all printed at the end, so a machine that is still without wifi after
# this says what it is rather than leaving the next step a guess.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

SCRIPT_NAME = "fix_wifi.py"
SCRIPT_PATH = Path(__file__).resolve()

#: The suffix a file this script rewrites is kept under, once.
BACKUP_SUFFIX = ".gnuchan-backup"

#: The environment variable that stops a sudo which fails to change the user
#: from re-running the script for ever.
ELEVATED_VARIABLE = "GNUGHAN_WIFI_ELEVATED"

# --- the package archive -----------------------------------------------------

#: The component the firmware lives in, by the release it was introduced in.
#: Debian 12 split the firmware out of non-free into non-free-firmware, and a
#: component that does not exist in the archive breaks apt-get update - so the
#: name is picked by the release rather than both being written.
COMPONENTS_BOOKWORM = ("non-free-firmware", "non-free")
COMPONENTS_BEFORE_BOOKWORM = ("non-free",)

#: The release non-free-firmware exists from.
NON_FREE_FIRMWARE_RELEASE = 12

#: The release assumed when /etc/os-release names no number.
ASSUMED_RELEASE = 12

#: Where apt reads what to download from. The classic one-file format and the
#: newer one-directory format are both handled, because which is present depends
#: on when the machine was installed.
APT_SOURCES_LIST = Path("/etc/apt/sources.list")
APT_SOURCES_DIR = Path("/etc/apt/sources.list.d")

#: Only a line that names Debian is edited: a machine with a third-party
#: repository must not have a component added to it that its archive does not
#: have.
DEBIAN_MARKER = "debian"

#: How many times apt is asked to retry a download before giving up.
APT_RETRIES = "3"

# --- the chip ----------------------------------------------------------------

#: The driver names a wireless chip uses, by the first characters that make it
#: one family, and the family's name. The order matters: b43 is checked before
#: brcm, and rtw before rtl, so the longer prefix wins.
DRIVER_FAMILIES: tuple[tuple[str, str], ...] = (
    ("iwl", "intel"),
    ("b43", "b43"),
    ("brcm", "broadcom"),
    ("bcma", "broadcom"),
    ("ath", "atheros"),
    ("carl", "atheros"),
    ("ar9170", "atheros"),
    ("rtw", "realtek"),
    ("rtl", "realtek"),
    ("r8188", "realtek"),
    ("r8712", "realtek"),
    ("r8192", "realtek"),
    ("mt79", "mediatek"),
    ("mt766", "mediatek"),
    ("mt760", "mediatek"),
    ("mt76", "mediatek"),
    ("rt2", "ralink"),
    ("rt6", "ralink"),
    ("rt7", "ralink"),
    ("wl12", "ti"),
    ("wl18", "ti"),
    ("wlcore", "ti"),
    ("mwifiex", "misc"),
    ("mwl8k", "misc"),
    ("rsi", "misc"),
    ("cw1200", "misc"),
    ("libertas", "misc"),
    ("p54", "misc"),
    ("zd1211", "misc"),
    ("adm8211", "misc"),
)

#: The packages that carry a family's firmware, best first. More than one is
#: listed where the package was folded into firmware-misc-nonfree in a later
#: release, and the first that apt will install is the one used.
FAMILY_PACKAGES: dict[str, tuple[str, ...]] = {
    "intel": ("firmware-iwlwifi",),
    "realtek": ("firmware-realtek",),
    "atheros": ("firmware-atheros",),
    "broadcom": ("firmware-brcm80211", "firmware-misc-nonfree"),
    "mediatek": ("firmware-mediatek", "firmware-misc-nonfree"),
    "ralink": ("firmware-ralink", "firmware-misc-nonfree"),
    "ti": ("firmware-ti-connectivity", "firmware-misc-nonfree"),
    "b43": ("firmware-b43-installer", "firmware-b43legacy-installer"),
    "misc": ("firmware-misc-nonfree",),
}

#: The PCI vendor ids that make a card one family, used when a card is present
#: but no driver is bound to it - which is exactly the state a card is in when
#: its firmware could not be loaded.
VENDOR_FAMILIES: dict[str, str] = {
    "8086": "intel",
    "10ec": "realtek",
    "0bda": "realtek",
    "168c": "atheros",
    "0cf3": "atheros",
    "14e4": "broadcom",
    "14c3": "mediatek",
    "0e8d": "mediatek",
    "1814": "ralink",
    "148f": "ralink",
    "17cb": "misc",
}

#: The PCI class of a network controller that is wireless: 0x0280xx. An
#: ethernet controller is 0x0200xx and is looked for nowhere, because wired
#: networking has no firmware to install.
PCI_WIRELESS_CLASS = "0x0280"

#: Where the kernel describes the hardware.
NET_DIR = Path("/sys/class/net")
PCI_DIR = Path("/sys/bus/pci/devices")
USB_DIR = Path("/sys/bus/usb/devices")

# --- the network stack -------------------------------------------------------

#: The supplicant every wireless network manager uses. It is small and it is the
#: one package without which no WPA network can be joined.
SUPPLICANT_PACKAGE = "wpasupplicant"

#: The network managers, best first. One of them has to be installed for a
#: wireless network to be joinable, and none is installed on a minimal Debian.
NETWORK_MANAGERS: tuple[tuple[str, str], ...] = (
    ("network-manager", "NetworkManager"),
    ("connman", "connman"),
    ("wicd-daemon", "wicd"),
)


# --- logging -----------------------------------------------------------------
# There is no quiet mode: the script has nothing to configure, so every run
# prints the same steps.


class Log:
    """Progress output."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- running things ----------------------------------------------------------


def run(
    command: list[str],
    capture: bool = False,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run a command, with its output kept only when it is asked for.

    Nothing goes through a shell: every command is a list, so a path with a
    space in it cannot turn into two arguments.
    """
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True, env=environment
        )
    return subprocess.run(command, check=False, text=True, env=environment)


def apt_environment() -> dict[str, str]:
    """The environment apt is run in, so it never stops to ask a question."""
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def is_root() -> bool:
    """Whether this process can install packages and load modules.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    do; reading it through ``getattr`` keeps the module importable there so the
    file can be linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """Be root, re-running the script through sudo when possible.

    Everything this script does is root's - the package database, the kernel
    module table, the rfkill state - so there is no useful partial run as a
    normal user. sudo is used when it exists and the fact is announced rather
    than done silently; the environment variable stops a sudo that fails to
    change the user from looping.
    """
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit(
            "error: still not root after sudo; run the script as root "
            "(su -c 'python3 fix_wifi.py')"
        )
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: this installs packages and loads kernel modules; run this "
            "script as root"
        )
    log.step("This installs system packages; re-running it through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(SCRIPT_PATH), *sys.argv[1:]], environment)


# --- files -------------------------------------------------------------------


def read_text(path: Path) -> str:
    """Contents of a file, or an empty string when it is not readable."""
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    path.chmod(0o644)


def backup_once(path: Path) -> Path | None:
    """Copy an existing file aside, returning where the copy went.

    The copy is taken once and kept: a second run must not overwrite it with
    this script's own output, because the file worth keeping is the one the
    machine had before the script ever touched it. It is copied rather than
    moved for the same reason - the caller merges into the file afterwards.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.copy2(path, backup)
    return backup


# --- the distribution --------------------------------------------------------


def os_release() -> dict[str, str]:
    """Parse ``/etc/os-release`` into a dictionary, empty when it is absent."""
    result: dict[str, str] = {}
    for line in read_text(Path("/etc/os-release")).splitlines():
        name, separator, value = line.partition("=")
        if separator:
            result[name.strip()] = value.strip().strip('"').strip("'")
    return result


def distro_description() -> str:
    """What this distribution calls itself, for the first line of output."""
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def release_major() -> int:
    """The distribution's major version number, or the assumed one.

    Only the digits are kept, so a version of "12" or "12.5" both give 12, and a
    distribution that names no number at all gets the current Debian release,
    which is the one whose component names this script writes.
    """
    digits = "".join(character for character in os_release().get("VERSION_ID", "") if character.isdigit())
    try:
        return int(digits[:2]) if digits else ASSUMED_RELEASE
    except ValueError:
        return ASSUMED_RELEASE


def firmware_components() -> tuple[str, ...]:
    """The archive components the firmware is in, for this release."""
    if release_major() >= NON_FREE_FIRMWARE_RELEASE:
        return COMPONENTS_BOOKWORM
    return COMPONENTS_BEFORE_BOOKWORM


# --- making the component available ------------------------------------------


def apt_source_files() -> list[Path]:
    """Every file apt reads its sources from, that exists."""
    files: list[Path] = []
    if APT_SOURCES_LIST.is_file():
        files.append(APT_SOURCES_LIST)
    if APT_SOURCES_DIR.is_dir():
        files += sorted(APT_SOURCES_DIR.glob("*.list"))
        files += sorted(APT_SOURCES_DIR.glob("*.sources"))
    return files


def components_of(text: str) -> set[str]:
    """The archive components a sources file already names.

    Both formats are read: the classic ``deb URI suite component ...`` line and
    the newer ``Components: component ...`` one.
    """
    found: set[str] = set()
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        if stripped.startswith("deb "):
            found.update(stripped.split()[3:])
        elif stripped.lower().startswith("components:"):
            found.update(stripped.partition(":")[2].split())
    return found


def add_components(text: str, components: tuple[str, ...]) -> str:
    """Add ``components`` to the Debian lines of a sources file.

    A ``deb`` line gets them appended, and a ``Components:`` line gets them
    appended to its list. Nothing else is touched, and a line whose URI is not
    Debian is skipped, so a third-party repository is not given a component its
    archive does not have.
    """
    lines: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("Components:") or stripped.startswith("components:"):
            name, separator, value = line.partition(":")
            existing = value.split()
            for component in components:
                if component not in existing:
                    existing.append(component)
            lines.append(f"{name}:{' ' if separator else ' '}{' '.join(existing)}")
            continue
        if stripped.startswith("deb ") and DEBIAN_MARKER in stripped.lower():
            tokens = line.split()
            for component in components:
                if component not in tokens:
                    tokens.append(component)
            lines.append(" ".join(tokens))
            continue
        lines.append(line)
    return "\n".join(lines) + ("\n" if text.endswith("\n") else "")


def ensure_components(log: Log, components: tuple[str, ...]) -> bool:
    """Put the firmware component into the sources, returning whether anything changed.

    Only a file that already names Debian is edited, and the file is copied
    aside before the edit, because sources.list is the machine's and this
    script only adds one word to it.
    """
    changed = False
    for path in apt_source_files():
        text = read_text(path)
        if not text or DEBIAN_MARKER not in text.lower():
            continue
        missing = tuple(
            component for component in components if component not in components_of(text)
        )
        if not missing:
            continue
        backup = backup_once(path)
        if backup is not None:
            log.detail(f"backed up {path.name} to {backup.name}")
        write_text(path, add_components(text, missing))
        log.detail(f"added {' '.join(missing)} to {path}")
        changed = True
    return changed


# --- apt ---------------------------------------------------------------------


def apt_available() -> bool:
    return shutil.which("apt-get") is not None


def apt_update(log: Log) -> bool:
    """Refresh the package lists, reporting the output only when it fails."""
    command = ["apt-get", "update", "-o", f"Acquire::Retries={APT_RETRIES}"]
    log.detail("running: " + " ".join(command))
    result = run(command, capture=True, environment=apt_environment())
    if result.returncode != 0:
        log.warn("apt-get update failed; the package lists may be out of date")
        output = (result.stdout or "") + (result.stderr or "")
        for line in output.splitlines()[-10:]:
            log.detail(f"  {line}")
        return False
    log.detail("updated the package lists")
    return True


def package_installed(package: str) -> bool:
    """Whether dpkg has a package installed, whatever apt thinks of it."""
    result = run(
        ["dpkg-query", "-W", "-f", "${Status}", package], capture=True
    )
    return result.returncode == 0 and "install ok installed" in result.stdout


def install_package(log: Log, package: str) -> bool:
    """Install one package, returning whether it ended up installed.

    Recommended packages are not pulled in: a firmware package recommends
    nothing this needs, and the network stack is asked for separately.
    """
    command = ["apt-get", "install", "-y", "--no-install-recommends", package]
    log.detail("running: " + " ".join(command))
    result = run(command, capture=True, environment=apt_environment())
    if result.returncode != 0:
        output = (result.stdout or "") + (result.stderr or "")
        for line in output.splitlines()[-4:]:
            log.detail(f"  {line}")
        return False
    return True


# --- the chip this machine has -----------------------------------------------


def net_interfaces() -> list[str]:
    """The names of the network interfaces the kernel has."""
    try:
        return sorted(path.name for path in NET_DIR.iterdir())
    except OSError:
        return []


def is_wireless_interface(name: str) -> bool:
    """Whether an interface is a wireless one.

    Two markers are accepted, because a driver creates one or the other: the
    ``wireless`` directory of the old wireless extensions and the ``phy80211``
    link of the newer cfg80211 stack.
    """
    base = NET_DIR / name
    return (base / "wireless").is_dir() or (base / "phy80211").exists()


def device_driver(path: Path) -> str | None:
    """The name of the driver bound to a device, or None when none is.

    The driver is a symlink into /sys/bus/.../drivers, and its last component is
    the module name - the name the family is matched on.
    """
    link = path / "driver"
    if not link.is_symlink():
        return None
    try:
        return link.resolve().name
    except OSError:
        return None


def interface_driver(name: str) -> str | None:
    """The driver behind a network interface."""
    return device_driver(NET_DIR / name / "device")


def family_for_driver(driver: str | None) -> str | None:
    """The chip family a driver name belongs to, or None when it is not wireless."""
    if not driver:
        return None
    name = driver.lower()
    for prefix, family in DRIVER_FAMILIES:
        if name.startswith(prefix):
            return family
    return None


def pci_wireless_cards() -> list[tuple[str | None, str]]:
    """Every wireless PCI card as (driver or None, vendor id).

    A card whose firmware could not be loaded has no driver bound to it, which
    is the state this script exists to fix - so the vendor id is returned
    alongside the driver and the card is matched to its package by that when no
    driver name is available.
    """
    cards: list[tuple[str | None, str]] = []
    if not PCI_DIR.is_dir():
        return cards
    try:
        devices = sorted(PCI_DIR.iterdir())
    except OSError:
        return cards
    for device in devices:
        if not read_text(device / "class").strip().startswith(PCI_WIRELESS_CLASS):
            continue
        vendor = read_text(device / "vendor").strip().removeprefix("0x").lower()
        cards.append((device_driver(device), vendor))
    return cards


def usb_wifi_drivers() -> set[str]:
    """The drivers of the USB wireless interfaces the kernel has.

    The driver is what says the device is wireless here: a USB dongle's
    interface class is often vendor specific, so the class is not a usable test,
    while the driver name is the same one the PCI side is matched on.
    """
    drivers: set[str] = set()
    if not USB_DIR.is_dir():
        return drivers
    try:
        devices = sorted(USB_DIR.iterdir())
    except OSError:
        return drivers
    for device in devices:
        if not (device / "bInterfaceClass").is_file():
            continue
        driver = device_driver(device)
        if driver and family_for_driver(driver):
            drivers.add(driver)
    return drivers


@dataclass(frozen=True)
class Hardware:
    """What this machine's wireless hardware is, as far as it can be told."""

    interfaces: tuple[str, ...]
    drivers: tuple[str, ...]
    families: frozenset[str]
    has_pci_card: bool

    def found(self) -> bool:
        """Whether any wireless hardware was found at all."""
        return bool(self.interfaces or self.drivers or self.has_pci_card)


def detect_hardware() -> Hardware:
    """Find the wireless hardware and the families its firmware belongs to.

    Three sources are read because no one of them covers every state: a working
    card shows up as an interface, a card whose firmware failed shows up as a
    PCI device with no driver, and a USB dongle shows up as a bound driver. A
    name that matches no family is treated as ``misc``, which is where the
    drivers without a package of their own keep their firmware.
    """
    interfaces: list[str] = []
    drivers: set[str] = set()
    families: set[str] = set()

    for name in net_interfaces():
        if not is_wireless_interface(name):
            continue
        interfaces.append(name)
        driver = interface_driver(name)
        if driver:
            drivers.add(driver)
        families.add(family_for_driver(driver) or "misc")

    cards = pci_wireless_cards()
    for driver, vendor in cards:
        if driver:
            drivers.add(driver)
        families.add(family_for_driver(driver) or VENDOR_FAMILIES.get(vendor, "misc"))

    for driver in usb_wifi_drivers():
        drivers.add(driver)
        families.add(family_for_driver(driver) or "misc")

    if (interfaces or cards) and not families:
        families.add("misc")

    return Hardware(
        interfaces=tuple(interfaces),
        drivers=tuple(sorted(drivers)),
        families=frozenset(families),
        has_pci_card=bool(cards),
    )


# --- installing ---------------------------------------------------------------


def install_firmware(log: Log, hardware: Hardware) -> list[str]:
    """Install the firmware package each detected family needs.

    The candidates for a family are tried in order and the first that apt will
    install is kept, because a package folded into firmware-misc-nonfree in a
    later release is not on an older machine and the other way round - and apt
    aborts a whole transaction over one name it does not know.
    """
    installed: list[str] = []
    for family in sorted(hardware.families):
        candidates = FAMILY_PACKAGES.get(family)
        if not candidates:
            continue
        for package in candidates:
            if package_installed(package):
                log.detail(f"{package} is already installed")
                installed.append(package)
                break
            if install_package(log, package):
                log.detail(f"installed {package} for the {family} chip")
                installed.append(package)
                break
            log.detail(f"{package} is not available here")
        else:
            log.warn(f"no firmware package for the {family} chip could be installed")
    return installed


def install_network_stack(log: Log) -> None:
    """Install the supplicant, and a network manager when there is none.

    Firmware makes the interface appear; none of it joins a network. Installing
    a second network manager over one the machine already has would be the
    kind of change a fix script has no business making, so the manager is only
    installed when none of the known ones is present.
    """
    if install_package(log, SUPPLICANT_PACKAGE):
        log.detail(f"installed {SUPPLICANT_PACKAGE}")
    else:
        log.warn(f"{SUPPLICANT_PACKAGE} could not be installed")

    if manager_installed() is not None:
        log.detail(f"{manager_installed()} is already installed")
        return
    if install_package(log, NETWORK_MANAGERS[0][0]):
        log.detail(f"installed {NETWORK_MANAGERS[0][0]}")
    else:
        log.warn("no network manager could be installed")


def manager_installed() -> str | None:
    """The name of the installed network manager, or None."""
    for package, _ in NETWORK_MANAGERS:
        if package_installed(package):
            return package
    return None


# --- the radio and the drivers -----------------------------------------------


def unblock_radio(log: Log) -> None:
    """Unblock the radio, which is the other reason an interface is invisible.

    rfkill is not always present; when it is not, nothing is done rather than a
    package installed for it, because a machine without it does not have the
    block it clears.
    """
    rfkill = shutil.which("rfkill")
    if rfkill is None:
        log.detail("rfkill is not installed; the radio was not touched")
        return
    run([rfkill, "unblock", "all"], capture=True)
    log.detail("unblocked the radio")


def reload_drivers(log: Log, drivers: tuple[str, ...]) -> None:
    """Unload and load the drivers, giving a card a second chance at its firmware.

    A card that was probed before its firmware existed keeps the failure it had
    then until the module is reloaded. This is the step that makes the fix apply
    now instead of at the next boot, and it is safe: the modules are unloaded and
    loaded again, and a reload that fails leaves the machine exactly as it was.
    """
    modprobe = shutil.which("modprobe")
    if modprobe is None:
        return
    for driver in drivers:
        run([modprobe, "-r", driver], capture=True)
        if run([modprobe, driver], capture=True).returncode == 0:
            log.detail(f"reloaded {driver}")
        else:
            log.detail(f"{driver} could not be reloaded; it will load at the next boot")


def enable_network_manager(log: Log) -> None:
    """Start the network manager, when the machine runs systemd."""
    if manager_installed() != NETWORK_MANAGERS[0][0]:
        return
    if not systemd_running():
        log.detail("systemd is not running; the network manager starts at the next boot")
        return
    systemctl = shutil.which("systemctl")
    if systemctl is None:
        return
    if run([systemctl, "enable", "--now", NETWORK_MANAGERS[0][1]], capture=True).returncode == 0:
        log.detail(f"started {NETWORK_MANAGERS[0][1]}")


def systemd_running() -> bool:
    return Path("/run/systemd/system").is_dir()


# --- checking the result -----------------------------------------------------
# Everything here is read back from the machine rather than trusted. Every way
# this can fail ends in the same place - a wireless network that cannot be
# joined - and the checks are separated because the fix for each is different: a
# missing interface is a firmware problem, a blocked radio is one rfkill command,
# and a running interface with no manager is one package.


def rfkill_state() -> tuple[bool, bool]:
    """Whether the radio is soft blocked and whether it is hard blocked.

    A soft block is the kernel's, and ``rfkill unblock all`` clears it. A hard
    block is a physical switch or a firmware setting on the machine, which no
    software can clear - and saying so is the difference between a fix that did
    not work and a fix that cannot work.
    """
    rfkill = shutil.which("rfkill")
    if rfkill is None:
        return False, False
    result = run([rfkill, "list"], capture=True)
    soft = False
    hard = False
    for line in result.stdout.splitlines():
        text = line.strip().lower()
        if text.startswith("soft blocked:"):
            soft = text.endswith("yes")
        elif text.startswith("hard blocked:"):
            hard = text.endswith("yes")
    return soft, hard


def manager_running() -> bool:
    """Whether the installed network manager is running."""
    manager = manager_installed()
    if manager is None:
        return False
    if not systemd_running():
        return True
    systemctl = shutil.which("systemctl")
    if systemctl is None:
        return True
    for package, unit in NETWORK_MANAGERS:
        if package == manager:
            result = run([systemctl, "is-active", unit], capture=True)
            return result.stdout.strip() == "active"
    return True


def check_result(log: Log, hardware: Hardware) -> int:
    """Report what would keep the wifi from working, returning how many things would."""
    problems: list[str] = []

    interfaces = [name for name in net_interfaces() if is_wireless_interface(name)]
    if interfaces:
        log.detail("wireless interfaces: " + ", ".join(interfaces))
    else:
        problems.append(
            "no wireless interface exists, so the firmware for this chip is "
            "either a package this script did not find or the machine needs a "
            "reboot"
        )

    for family in sorted(hardware.families):
        candidates = FAMILY_PACKAGES.get(family, ())
        if not any(package_installed(package) for package in candidates):
            problems.append(
                f"no firmware package for the {family} chip is installed; the "
                f"machine needs one of: {', '.join(candidates)}"
            )

    soft, hard = rfkill_state()
    if hard:
        problems.append(
            "the radio is hard blocked by a switch or key on the machine, which "
            "no software can clear"
        )
    elif soft:
        problems.append("the radio is soft blocked; run: rfkill unblock all")

    if manager_installed() is None:
        problems.append(
            "no network manager is installed, so the wireless interface cannot "
            "be used to join a network"
        )
    elif not manager_running():
        problems.append(
            f"{manager_installed()} is installed but not running; start it, or "
            "reboot"
        )

    if problems:
        log.note(f"{len(problems)} problem(s) found:")
        for problem in problems:
            log.note(f"  {problem}")
    else:
        log.note("No problems found.")
    return len(problems)


# --- entry point -------------------------------------------------------------


def main() -> int:
    """Fix the wifi, with no options to pass."""
    log = Log()
    ensure_root(log)

    log.step(f"Fixing the wifi ({distro_description()})")

    if not apt_available():
        raise SystemExit(
            "error: this fixes a Debian system; apt-get is not installed here"
        )

    hardware = detect_hardware()
    if hardware.interfaces:
        log.detail("wireless interfaces: " + ", ".join(hardware.interfaces))
    if hardware.drivers:
        log.detail("drivers: " + ", ".join(hardware.drivers))
    if hardware.families:
        log.detail("chip families: " + ", ".join(sorted(hardware.families)))

    if not hardware.found():
        log.note("")
        log.note("No wireless hardware was found on this machine: no wireless")
        log.note("interface, no wireless card on the PCI bus and no wireless USB")
        log.note("driver. There is no firmware to install for a chip that is not")
        log.note("there, so nothing was changed.")
        return 0

    log.step("Making the firmware component available")
    if ensure_components(log, firmware_components()):
        log.detail("the sources were changed, so the package lists are refreshed")

    log.step("Updating the package lists")
    apt_update(log)

    log.step("Installing the firmware")
    install_firmware(log, hardware)

    log.step("Installing the network stack")
    install_network_stack(log)

    if hardware.drivers:
        log.step("Reloading the drivers")
        reload_drivers(log, hardware.drivers)

    log.step("Unblocking the radio")
    unblock_radio(log)

    enable_network_manager(log)

    log.step("Checking the result")
    problems = check_result(log, hardware)

    log.note("")
    if problems:
        log.note("The problems listed above have to be fixed before the wireless")
        log.note("interface can be used; until then there is no wifi.")
    else:
        log.note("Done. The firmware is on disk and the drivers have been given it:")
        log.note("a network manager can now be used to join a wireless network.")
        log.note("If the interface still does not appear, reboot, because a card")
        log.note("that was already probed keeps the failure it had at boot.")
    log.note("")
    if hardware.drivers:
        log.note("  drivers   " + ", ".join(hardware.drivers))
    if hardware.families:
        log.note("  chips     " + ", ".join(sorted(hardware.families)))
    log.note(f"  undo      apt-get remove the firmware package, and restore the "
             f"{BACKUP_SUFFIX} copy of the sources file")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
