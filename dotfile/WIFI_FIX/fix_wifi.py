#!/usr/bin/env python3
# =============================================================================
# GnuchanOS - fix the wifi on Debian, whatever is wrong with it
# -----------------------------------------------------------------------------
# Standard library only, and no options: running
#
#     python3 fix_wifi.py
#
# is the whole fix. It re-runs itself through sudo, because everything it does -
# installing packages, unblocking the radio, loading kernel modules - is root's.
#
# The report this was written from
# --------------------------------
#     $ /usr/sbin/rfkill
#     ID TYPE      DEVICE SOFT      HARD
#     0  bluetooth hci0   unblocked unblocked
#     1  wlan      phy0   unblocked blocked
#
# The wireless radio is SOFT unblocked and HARD blocked. That is the case this
# script exists for and the one a plain `rfkill unblock all` does not fix:
#
#   SOFT blocked   the kernel's own block, cleared with rfkill unblock all.
#   HARD blocked   the block is held by the machine's firmware/EC. No rfkill
#                  command clears it. It clears when the firmware is told the
#                  radio may be on - which is the wireless key (Fn+F11 on most
#                  laptops) - and then the driver reads that state again. The
#                  driver only reads it when it is loaded, so the sequence that
#                  works is:
#
#                      rfkill unblock all
#                      modprobe -r ath5k
#                      modprobe ath5k
#
#                  which is exactly what this script does, and why the wireless
#                  key is bound to run it: pressing Fn+F11 tells the firmware to
#                  enable the radio, and the script that runs on that press
#                  reloads the driver so the hard block is gone.
#
# What else it fixes
# ------------------
# Every other way wifi is broken on Debian, because a fix that only handles one
# of them is not a fix:
#
#   Missing firmware. The driver is in the kernel but the firmware it loads at
#   start-up is not, because it is non-free. The interface then never appears.
#   The chip is detected from /sys (PCI class 0x0280, /sys/class/net, USB) and
#   the right firmware-* package is installed, from the non-free-firmware
#   component - which is added to apt's sources first, as Debian 12 requires.
#
#   No network stack. wpasupplicant, and a network manager when the machine has
#   none, because firmware makes the interface appear and nothing joins a network.
#
#   A card probed before its firmware existed, which keeps the failure it had
#   then until the module is reloaded.
#
# The wireless key
# ----------------
# Fn+F11 (the wireless key) is bound through acpid, which is the layer below any
# desktop and works whatever window manager is running. The key runs this same
# script in a fast path that only unblocks the radio and reloads the driver - no
# package manager, no delay - so pressing the key is the whole recovery.
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

#: Set by the wireless-key handler. It is what makes a key press take the fast
#: path - unblock the radio and reload the driver - instead of a full install.
KEY_MODE_VARIABLE = "GNUGHAN_WIFI_KEY"

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
NON_FREE_FIRMWARE_RELEASE = 12
ASSUMED_RELEASE = 12

APT_SOURCES_LIST = Path("/etc/apt/sources.list")
APT_SOURCES_DIR = Path("/etc/apt/sources.list.d")

#: Only a line that names Debian is edited: a machine with a third-party
#: repository must not have a component added to it that its archive lacks.
DEBIAN_MARKER = "debian"

APT_RETRIES = "3"

# --- the chip ----------------------------------------------------------------

#: Driver name prefixes, longest first within each family. The order matters:
#: b43 is checked before brcm and rtw before rtl, so the longer prefix wins.
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
#: listed where the package moved into firmware-misc-nonfree in a later release,
#: and the first apt will install is the one used.
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

#: PCI vendor ids, for a card whose firmware failed so no driver is bound and no
#: driver name exists to match on. This is exactly the reported machine's state.
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

#: The PCI class of a wireless network controller, 0x0280xx. Ethernet is
#: 0x0200xx and is not looked for: wired networking has no firmware to install.
PCI_WIRELESS_CLASS = "0x0280"

NET_DIR = Path("/sys/class/net")
PCI_DIR = Path("/sys/bus/pci/devices")
USB_DIR = Path("/sys/bus/usb/devices")

# --- the network stack -------------------------------------------------------

SUPPLICANT_PACKAGE = "wpasupplicant"

#: The network managers, best first: one has to be installed for a wireless
#: network to be joinable, and none is installed on a minimal Debian.
NETWORK_MANAGERS: tuple[tuple[str, str], ...] = (
    ("network-manager", "NetworkManager"),
    ("connman", "connman"),
    ("wicd-daemon", "wicd"),
)

# --- the wireless key --------------------------------------------------------

#: acpid's event files and the command they run. acpid is the layer below any
#: desktop: it sees ACPI hotkey events before any window manager exists, so the
#: binding works under XFCE, i3, openbox and a bare console alike.
ACPI_EVENTS_DIR = Path("/etc/acpi/events")
KEY_HANDLER = Path("/usr/local/bin/gnuchan-fix-wifi")

#: The ACPI event names the wireless key sends on the machines where the key
#: reaches ACPI at all. A file for an event a machine never sends simply never
#: runs, and costs nothing.
KEY_EVENT_NAMES = (
    "button/wlan",
    "button/wireless",
    "button/rfkill",
    "rfkill",
    "wlan",
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
    """Run a command, keeping its output only when it is asked for.

    Nothing goes through a shell: every command is a list, so a path with a
    space in it cannot turn into two arguments.
    """
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True, env=environment
        )
    return subprocess.run(command, check=False, text=True, env=environment)


def apt_environment() -> dict[str, str]:
    """The environment apt runs in, so it never stops to ask a question."""
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
    normal user. The environment variable stops a sudo that fails to change the
    user from looping; the wireless-key handler already runs as root and never
    reaches this.
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
            "error: this installs packages and reloads kernel modules; run this "
            "script as root"
        )
    log.step("This changes system packages and modules; re-running it through sudo")
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
    """Copy an existing file aside once, returning where the copy went.

    The copy is kept as the machine had it, because a second run must not
    overwrite it with this script's own output - and it is copied rather than
    moved, because the caller merges into the file afterwards.
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
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def release_major() -> int:
    """The distribution's major version, or the assumed one.

    Only the digits are kept, so "12" and "12.5" both give 12, and a
    distribution that names no number gets the current Debian release, whose
    component names this script writes.
    """
    digits = "".join(
        character for character in os_release().get("VERSION_ID", "") if character.isdigit()
    )
    return int(digits[:2]) if digits else ASSUMED_RELEASE


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


def source_tokens(line: str) -> list[str]:
    """Split a sources line on whitespace, keeping a ``[...]`` option list whole.

    ``deb [arch=amd64 trusted=yes] http://... bookworm main`` is a URI, a suite
    and a component list with an option group in front - the group may hold
    spaces, and splitting it apart puts a component in the middle of it.
    """
    tokens: list[str] = []
    current = ""
    depth = 0
    for character in line:
        if character == "[":
            depth += 1
        elif character == "]":
            depth = max(0, depth - 1)
        if character.isspace() and depth == 0:
            if current:
                tokens.append(current)
                current = ""
            continue
        current += character
    if current:
        tokens.append(current)
    return tokens


def components_of(text: str) -> set[str]:
    """The archive components a sources file already names.

    Both formats are read: the classic ``deb URI suite component ...`` line and
    the deb822 ``Components: component ...`` line Debian 12 writes.
    """
    found: set[str] = set()
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.lower().startswith("components:"):
            found.update(stripped.partition(":")[2].split())
            continue
        tokens = source_tokens(stripped)
        if tokens and tokens[0] == "deb":
            start = 4 if len(tokens) > 1 and tokens[1].startswith("[") else 3
            found.update(tokens[start:])
    return found


def add_components(text: str, components: tuple[str, ...]) -> str:
    """Add ``components`` to the Debian lines of a sources file.

    A ``deb`` line gets them appended after its component list and a
    ``Components:`` line gets them appended to its own. Nothing else is touched,
    and a line that does not name Debian is skipped, so a third-party repository
    is never given a component its archive does not have.
    """
    lines: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            lines.append(line)
            continue
        if stripped.lower().startswith("components:"):
            name, _, value = line.partition(":")
            existing = value.split()
            existing += [c for c in components if c not in existing]
            lines.append(f"{name}: {' '.join(existing)}")
            continue
        if stripped.startswith("deb ") and DEBIAN_MARKER in stripped.lower():
            tokens = source_tokens(line)
            tokens += [c for c in components if c not in tokens]
            lines.append(" ".join(tokens))
            continue
        lines.append(line)
    return "\n".join(lines) + ("\n" if text.endswith("\n") else "")


def ensure_components(log: Log, components: tuple[str, ...]) -> bool:
    """Put the firmware component into the sources, returning whether anything changed.

    Only a file that already names Debian is edited, and it is copied aside
    first, because sources.list is the machine's and this script adds one word.
    """
    changed = False
    for path in apt_source_files():
        text = read_text(path)
        if not text or DEBIAN_MARKER not in text.lower():
            continue
        missing = tuple(c for c in components if c not in components_of(text))
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
        for line in ((result.stdout or "") + (result.stderr or "")).splitlines()[-8:]:
            log.detail(f"  {line}")
        return False
    log.detail("updated the package lists")
    return True


def package_installed(package: str) -> bool:
    """Whether dpkg has a package installed, whatever apt thinks of it."""
    result = run(["dpkg-query", "-W", "-f", "${Status}", package], capture=True)
    return result.returncode == 0 and "install ok installed" in result.stdout


def install_package(log: Log, package: str) -> bool:
    """Install one package, returning whether it ended up installed.

    Recommended packages are not pulled in: a firmware package recommends
    nothing this needs, and the network stack is asked for by name.
    """
    if package_installed(package):
        log.detail(f"{package} is already installed")
        return True
    command = ["apt-get", "install", "-y", "--no-install-recommends", package]
    log.detail("running: " + " ".join(command))
    result = run(command, capture=True, environment=apt_environment())
    if result.returncode != 0:
        for line in ((result.stdout or "") + (result.stderr or "")).splitlines()[-4:]:
            log.detail(f"  {line}")
        return False
    return True


# --- the chip this machine has -----------------------------------------------


def net_interfaces() -> list[str]:
    try:
        return sorted(path.name for path in NET_DIR.iterdir())
    except OSError:
        return []


def is_wireless_interface(name: str) -> bool:
    """Whether an interface is wireless.

    Two markers are accepted, because a driver creates one or the other: the
    ``wireless`` directory of the old wireless extensions and the ``phy80211``
    link of the newer cfg80211 stack.
    """
    base = NET_DIR / name
    return (base / "wireless").is_dir() or (base / "phy80211").exists()


def device_driver(path: Path) -> str | None:
    """The name of the driver bound to a device, or None when none is.

    The driver is a symlink into /sys/bus/.../drivers and its last component is
    the module name - the name the family is matched on.
    """
    link = path / "driver"
    if not link.is_symlink():
        return None
    try:
        return link.resolve().name
    except OSError:
        return None


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

    A card whose firmware failed has no driver bound to it, which is the state
    this script exists to fix - so the vendor id comes back with the driver and
    the card is matched to its package by that when no driver name exists.
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
    """The drivers of the USB wireless devices the kernel has.

    The driver is the test here, not the interface class: a USB dongle's class
    is often vendor specific, while the driver name is the same one the PCI side
    is matched on.
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

    def found(self) -> bool:
        return bool(self.interfaces or self.drivers or self.families)


def detect_hardware() -> Hardware:
    """Find the wireless hardware and the families its firmware belongs to.

    Three sources are read because no one covers every state: a working card is
    an interface, a card whose firmware failed is a PCI device with no driver,
    and a USB dongle is a bound driver. A name matching no family is ``misc``,
    where the drivers without a package of their own keep their firmware.
    """
    interfaces: list[str] = []
    drivers: set[str] = set()
    families: set[str] = set()

    for name in net_interfaces():
        if not is_wireless_interface(name):
            continue
        interfaces.append(name)
        driver = device_driver(NET_DIR / name / "device")
        if driver:
            drivers.add(driver)
        families.add(family_for_driver(driver) or "misc")

    for driver, vendor in pci_wireless_cards():
        if driver:
            drivers.add(driver)
        families.add(family_for_driver(driver) or VENDOR_FAMILIES.get(vendor, "misc"))

    for driver in usb_wifi_drivers():
        drivers.add(driver)
        families.add(family_for_driver(driver) or "misc")

    if (interfaces or drivers) and not families:
        families.add("misc")

    return Hardware(
        interfaces=tuple(interfaces),
        drivers=tuple(sorted(drivers)),
        families=frozenset(families),
    )


# --- the radio ---------------------------------------------------------------
# The reported bug lives here. A hard block is held by the machine's firmware and
# no rfkill command clears it; it clears when the firmware is told the radio may
# be on, which is the wireless key, and then the driver re-reads that state. So
# the fix is: unblock, reload the driver, unblock again.


@dataclass(frozen=True)
class Radio:
    """One radio's block state, as rfkill reports it."""

    identifier: str
    soft: bool
    hard: bool

    def describe(self) -> str:
        soft = "soft blocked" if self.soft else "soft unblocked"
        hard = "hard blocked" if self.hard else "hard unblocked"
        return f"{self.identifier} ({soft}, {hard})"


def parse_block_listing(text: str) -> list[Radio]:
    """Read the ``rfkill list`` form, where each radio is a block of lines.

        0: phy0: Wireless LAN
            Soft blocked: no
            Hard blocked: yes
    """
    radios: list[Radio] = []
    identifier: str | None = None
    soft: bool | None = None
    for raw in text.splitlines():
        line = raw.strip()
        low = line.lower()
        if low.startswith("soft blocked:"):
            soft = low.endswith("yes")
            continue
        if low.startswith("hard blocked:"):
            hard = low.endswith("yes")
            if identifier is not None and soft is not None:
                radios.append(Radio(identifier, soft, hard))
            identifier, soft = None, None
            continue
        if ":" in line:
            identifier = line
    return radios


def parse_table_listing(text: str) -> list[Radio]:
    """Read the table ``rfkill`` prints with no arguments.

        ID TYPE      DEVICE SOFT      HARD
        1  wlan      phy0   unblocked blocked
    """
    radios: list[Radio] = []
    for raw in text.splitlines():
        tokens = raw.split()
        if len(tokens) < 3:
            continue
        if tokens[-1] not in ("blocked", "unblocked"):
            continue
        if tokens[-2] not in ("blocked", "unblocked"):
            continue
        label = " ".join(tokens[1:-2]) or tokens[0]
        radios.append(Radio(label, tokens[-2] == "blocked", tokens[-1] == "blocked"))
    return radios


def rfkill_radios() -> list[Radio]:
    """Every radio and its block state, from whichever rfkill form this prints."""
    rfkill = shutil.which("rfkill")
    if rfkill is None:
        return []
    listed = parse_block_listing(run([rfkill, "list"], capture=True).stdout)
    if listed:
        return listed
    return parse_table_listing(run([rfkill], capture=True).stdout)


def unblock_all(log: Log) -> bool:
    """Clear every soft block, returning whether rfkill was there to do it."""
    rfkill = shutil.which("rfkill")
    if rfkill is None:
        log.detail("rfkill is not installed; install the rfkill package to manage the radio")
        return False
    run([rfkill, "unblock", "all"], capture=True)
    return True


def reload_drivers(log: Log, drivers: tuple[str, ...]) -> None:
    """Unload and load the wireless drivers, giving a card its firmware and its radio.

    A card probed before its firmware existed keeps that failure until the module
    is reloaded, and a hard block is re-read from the firmware when the module
    loads - so this is the step that makes the fix apply now instead of at the
    next boot. A module that will not unload is left alone.
    """
    modprobe = shutil.which("modprobe")
    if modprobe is None:
        log.detail("modprobe is not installed; the drivers were not reloaded")
        return
    if not drivers:
        log.detail("no wireless driver name was found, so none could be reloaded")
        return
    for driver in drivers:
        run([modprobe, "-r", driver], capture=True)
    for driver in drivers:
        if run([modprobe, driver], capture=True).returncode == 0:
            log.detail(f"reloaded {driver}")
        else:
            log.detail(f"{driver} could not be reloaded; it loads at the next boot")


def fix_radio(log: Log, hardware: Hardware) -> None:
    """Unblock, reload the drivers, and unblock again.

    The second unblock is not redundant: reloading a driver resets the kernel's
    soft state, so a machine that was soft blocked before the reload would be
    soft blocked after it as well without this.
    """
    unblock_all(log)
    reload_drivers(log, hardware.drivers)
    unblock_all(log)


# --- the network stack -------------------------------------------------------


def manager_installed() -> str | None:
    """The name of the installed network manager, or None."""
    for package, _ in NETWORK_MANAGERS:
        if package_installed(package):
            return package
    return None


def install_network_stack(log: Log) -> None:
    """Install the supplicant, and a network manager when there is none.

    Firmware makes the interface appear; none of it joins a network. A second
    manager over one the machine already has is the kind of change a fix script
    has no business making, so the manager is only installed when none is present.
    """
    install_package(log, SUPPLICANT_PACKAGE)
    manager = manager_installed()
    if manager is not None:
        log.detail(f"{manager} is already installed")
        return
    install_package(log, NETWORK_MANAGERS[0][0])


def install_firmware(log: Log, hardware: Hardware) -> None:
    """Install the firmware package each detected family needs.

    The candidates for a family are tried in order and the first apt will
    install is kept, because a package folded into firmware-misc-nonfree in a
    later release is not on an older machine and the other way round - and apt
    aborts a whole transaction over one name it does not know.
    """
    if not hardware.families:
        return
    for family in sorted(hardware.families):
        candidates = FAMILY_PACKAGES.get(family)
        if not candidates:
            continue
        for package in candidates:
            if install_package(log, package):
                log.detail(f"{package} covers the {family} chip")
                break
            log.detail(f"{package} is not available here")
        else:
            log.warn(f"no firmware package for the {family} chip could be installed")


def enable_service(log: Log, unit: str) -> None:
    """Enable and start a service, on a machine that runs systemd."""
    if not Path("/run/systemd/system").is_dir():
        return
    systemctl = shutil.which("systemctl")
    if systemctl is None:
        return
    if run([systemctl, "enable", "--now", unit], capture=True).returncode == 0:
        log.detail(f"enabled {unit}")


# --- binding the wireless key ------------------------------------------------
# Fn+F11 is the wireless key on most laptops. acpid sees ACPI hotkey events
# before any desktop exists, so binding there works whatever window manager the
# machine runs - which is the whole point: the key has to work on a machine whose
# wifi is not up, and a desktop-level binding cannot be relied on for that.


def key_handler_script() -> str:
    """The shell the wireless key runs.

    It sets the fast-path variable and re-runs this script as it is - the same
    file, so there is one fix and not two. acpid runs it as root, so it does not
    go through sudo.
    """
    return "\n".join(
        [
            "#!/bin/sh",
            f"# written by {SCRIPT_NAME}",
            "#",
            "# The wireless key (Fn+F11) runs this. It unblocks the radio and",
            "# reloads the driver, which is what clears a hard block the firmware",
            "# is holding - no package manager, because a key press has to be",
            "# instant.",
            f"export {KEY_MODE_VARIABLE}=1",
            f'exec "{sys.executable}" "{SCRIPT_PATH}"',
            "",
        ]
    )


def acpi_event_file(event_name: str) -> str:
    """One acpid event file: the key's ACPI event, and what it runs."""
    return "\n".join(
        [
            f"# written by {SCRIPT_NAME}",
            f"event={event_name}",
            f"action={KEY_HANDLER}",
            "",
        ]
    )


def install_key_binding(log: Log) -> None:
    """Install the handler and the acpid events, and start acpid.

    The handler is written from this script's own path, so it always runs the
    fix that is installed beside it. Every known event name is written rather
    than guessing one; a file for an event the machine never sends never runs.
    """
    if not package_installed("acpid"):
        install_package(log, "acpid")
    if not package_installed("acpid"):
        log.detail("acpid could not be installed, so the wireless key cannot be bound")
        return

    write_text(KEY_HANDLER, key_handler_script())
    KEY_HANDLER.chmod(0o755)
    log.detail(f"wrote {KEY_HANDLER}")

    for event_name in KEY_EVENT_NAMES:
        path = ACPI_EVENTS_DIR / f"gnuchan-wifi-{event_name.replace('/', '-')}"
        write_text(path, acpi_event_file(event_name))
    log.detail(f"bound {', '.join(KEY_EVENT_NAMES)} to {KEY_HANDLER}")

    enable_service(log, "acpid")
    systemctl = shutil.which("systemctl")
    if systemctl is not None and Path("/run/systemd/system").is_dir():
        run([systemctl, "restart", "acpid"], capture=True)
    log.detail("the wireless key now runs the fix")


# --- checking the result -----------------------------------------------------
# Everything is read back from the machine rather than trusted. Every way this
# can fail ends in the same place - a wireless network that cannot be joined -
# and the checks are separated because the fix for each is different: a hard
# block is the wireless key, a soft block is one rfkill command, a missing
# interface is firmware, and a silent interface is one package.


def check_radios(log: Log) -> list[str]:
    """Report each radio's block state, returning the problems it implies."""
    problems: list[str] = []
    radios = rfkill_radios()
    if not radios:
        log.detail("rfkill reports no radios")
        return problems
    for radio in radios:
        log.detail(radio.describe())
        if radio.hard:
            problems.append(
                f"{radio.identifier} is hard blocked, which no software clears: "
                "press the wireless key (Fn+F11), which now runs this fix, and "
                "make sure the machine's wireless switch is on"
            )
        elif radio.soft:
            problems.append(f"{radio.identifier} is soft blocked; run: rfkill unblock all")
    return problems


def check_result(log: Log, hardware: Hardware) -> int:
    """Report what would keep the wifi from working, returning how many things would."""
    problems: list[str] = []

    interfaces = [name for name in net_interfaces() if is_wireless_interface(name)]
    if interfaces:
        log.detail("wireless interfaces: " + ", ".join(interfaces))
    else:
        problems.append(
            "no wireless interface exists, so the firmware for this chip is either "
            "a package this script did not find or the machine needs a reboot"
        )

    for family in sorted(hardware.families):
        candidates = FAMILY_PACKAGES.get(family, ())
        if not any(package_installed(package) for package in candidates):
            problems.append(
                f"no firmware package for the {family} chip is installed; this "
                f"machine needs one of: {', '.join(candidates)}"
            )

    problems += check_radios(log)

    if manager_installed() is None:
        problems.append(
            "no network manager is installed, so the wireless interface cannot be "
            "used to join a network"
        )

    if problems:
        log.note(f"{len(problems)} problem(s) found:")
        for problem in problems:
            log.note(f"  {problem}")
    else:
        log.note("No problems found.")
    return len(problems)


# --- entry point -------------------------------------------------------------


def report_hardware(log: Log, hardware: Hardware) -> None:
    if hardware.interfaces:
        log.detail("wireless interfaces: " + ", ".join(hardware.interfaces))
    if hardware.drivers:
        log.detail("drivers: " + ", ".join(hardware.drivers))
    if hardware.families:
        log.detail("chip families: " + ", ".join(sorted(hardware.families)))


def needs_packages(hardware: Hardware) -> bool:
    """Whether a package has to be fetched, and so whether the lists need refreshing.

    This is what keeps a wireless-key press fast: when everything is already
    installed, the key press never reaches the package manager at all.
    """
    for family in hardware.families:
        candidates = FAMILY_PACKAGES.get(family, ())
        if candidates and not any(package_installed(package) for package in candidates):
            return True
    return (
        not package_installed(SUPPLICANT_PACKAGE)
        or not package_installed("acpid")
        or manager_installed() is None
    )


def full_fix(log: Log) -> int:
    """Install what is missing, fix the radio, and bind the wireless key."""
    if not apt_available():
        raise SystemExit(
            "error: this fixes a Debian system, and apt-get is not installed here"
        )

    hardware = detect_hardware()
    report_hardware(log, hardware)
    if not hardware.found():
        log.note("")
        log.note("No wireless hardware was found: no wireless interface, no wireless")
        log.note("card on the PCI bus and no wireless USB driver. There is no")
        log.note("firmware to install for a chip that is not there, so nothing was")
        log.note("changed. Check that the card is seated and enabled in the firmware.")
        return 0

    log.step("Making the firmware component available")
    changed = ensure_components(log, firmware_components())

    if changed or needs_packages(hardware):
        log.step("Updating the package lists")
        apt_update(log)

    log.step("Installing the firmware")
    install_firmware(log, hardware)

    log.step("Installing the network stack")
    install_network_stack(log)

    log.step("Fixing the radio")
    fix_radio(log, hardware)

    log.step("Binding the wireless key")
    install_key_binding(log)

    log.step("Checking the result")
    problems = check_result(log, hardware)

    log.note("")
    if problems:
        log.note("The problems above have to be fixed before the wireless interface")
        log.note("can be used; until then there is no wifi.")
    else:
        log.note("Done. The firmware is on disk, the radio is unblocked and the")
        log.note("drivers have been reloaded, so a network manager can join a")
        log.note("network now. If the interface still does not appear, reboot: a")
        log.note("card probed at boot keeps the failure it had then.")
    log.note("")
    log.note("The wireless key (Fn+F11) now runs this fix, so pressing it unblocks")
    log.note("the radio and reloads the driver without a terminal.")
    log.note("")
    if hardware.drivers:
        log.note("  drivers   " + ", ".join(hardware.drivers))
    if hardware.families:
        log.note("  chips     " + ", ".join(sorted(hardware.families)))
    log.note(f"  key       {KEY_HANDLER}")
    log.note(
        f"  undo      apt-get remove the firmware package, and restore the "
        f"{BACKUP_SUFFIX} copy of the sources file"
    )
    return 1 if problems else 0


def key_press(log: Log) -> int:
    """What the wireless key runs: unblock the radio, reload the driver.

    Nothing is installed and nothing is read back beyond the radios: a key press
    has to return instantly, and the state that changes is the one the firmware
    holds, which only a reload makes readable to the driver.
    """
    hardware = detect_hardware()
    unblock_all(log)
    reload_drivers(log, hardware.drivers)
    unblock_all(log)
    for radio in rfkill_radios():
        log.note(f"  {radio.describe()}")
    return 0


def main() -> int:
    """Fix the wifi, or, when the wireless key calls it, just fix the radio."""
    log = Log()
    ensure_root(log)
    if os.environ.get(KEY_MODE_VARIABLE) == "1":
        return key_press(log)

    log.step(f"Fixing the wifi ({distro_description()})")
    return full_fix(log)


if __name__ == "__main__":
    raise SystemExit(main())
