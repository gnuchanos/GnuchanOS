#!/usr/bin/env python3
# =============================================================================
# GnuchanOS - make the laptop function (Fn) keys work on Debian
# -----------------------------------------------------------------------------
# Standard library only, and no options: running
#
#     python3 fix_fn_keys.py
#
# is the whole fix. It re-runs itself through sudo, because everything it
# writes is root's.
#
# The problem
# -----------
# On a laptop the top row of keys - brightness, volume, wireless, sleep - and
# the Fn key itself are driven by the embedded controller (EC) through ACPI.
# The EC is firmware, and the firmware decides which operating system it
# believes it is talking to before it answers at all: it reads the _OSI strings
# the kernel advertises at boot ("Linux", "Windows 2009", "Windows 2015" ...)
# and enables the function-key events only for the ones on its own list. A
# machine whose EC has no "Linux" on that list enables none of them, and every
# Fn key silently does nothing.
#
# The kernel does not advertise "Linux" by default any more - some ECs take it
# and disable features instead - so a laptop that needs the older behaviour has
# to be told so on the kernel command line:
#
#     acpi_osi=Linux
#
# That is what this script writes. It is the fix that works on the widest range
# of machines, and it is safe: the string only changes what the firmware is
# willing to answer, not what the kernel does.
#
# Apple keyboards are the other half of the problem and a different one. There
# the keys are not ACPI at all, they are USB/HID, and hid_apple hands the top
# row to the special functions (brightness, volume) while F1-F12 need Fn held
# down. The exchange is the fnmode module parameter:
#
#     fnmode=0  the Fn key does nothing
#     fnmode=1  the special functions, Fn for F1-F12  (the default)
#     fnmode=2  F1-F12, Fn for the special functions
#     fnmode=3  automatic
#
# On an Apple machine this script writes fnmode=2, so the top row types F1-F12.
#
# What it writes
# --------------
#   /etc/default/grub
#       GRUB_CMDLINE_LINUX_DEFAULT gets acpi_osi=Linux appended, and every
#       other word on the line is kept. grub.cfg is regenerated afterwards with
#       update-grub, because a change to /etc/default/grub does nothing until
#       that is run.
#
#   /etc/modprobe.d/gnuchan-fn-keys.conf
#       Only on an Apple machine: "options hid_apple fnmode=2". The initramfs is
#       regenerated afterwards, because the parameter has to be in force when the
#       module is first loaded - which on a laptop with an encrypted root is
#       inside the initramfs, before /etc is readable.
#
# Both files are copied aside once, before the first run that rewrites them.
#
# A second run changes nothing: the token is already on the line, or it is not,
# and either way the file that is written is the file that is already there.
#
# Undo
# ----
# There are no flags, so undoing is by hand and always possible: take
# acpi_osi=Linux back out of GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub, or
# put the .gnuchan-backup copy back over it, remove
# /etc/modprobe.d/gnuchan-fn-keys.conf on an Apple machine, and run update-grub
# and update-initramfs -u.
#
# The fix is read when the machine boots and not before, so the function keys
# start working after a reboot. The last thing this script prints says so.
#
# Distribution
# ------------
# Debian and what is derived from it. The machine is asked what it is through
# /sys/class/dmi/id, which the kernel fills in on every architecture.
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

SCRIPT_NAME = "fix_fn_keys.py"
SCRIPT_PATH = Path(__file__).resolve()
MARKER = f"written by {SCRIPT_NAME}"

#: The suffix every file this script rewrites is kept under, once.
BACKUP_SUFFIX = ".gnuchan-backup"

#: The environment variable that stops a sudo which fails to change the user
#: from re-running the script for ever.
ELEVATED_VARIABLE = "GNUGHAN_FN_KEYS_ELEVATED"

# --- what is written ---------------------------------------------------------

#: Where the kernel command line is configured. /etc/default/grub is a shell
#: fragment that /etc/grub.d/00_header sources; the line this script edits is
#: the one Debian passes to the kernel itself.
GRUB_DEFAULT_FILE = Path("/etc/default/grub")
CMDLINE_KEY = "GRUB_CMDLINE_LINUX_DEFAULT"

#: The _OSI string the firmware is asked to answer. "Linux" is the one the ECs
#: that broke the Fn keys on Debian were written against.
ACPI_OSI = "Linux"

#: The kernel word that asks for it.
ACPI_TOKEN = f"acpi_osi={ACPI_OSI}"

#: Where a module parameter is set for every boot, and the file this script owns.
MODPROBE_FILE = Path("/etc/modprobe.d/gnuchan-fn-keys.conf")

# --- the machine -------------------------------------------------------------

#: The kernel's own description of the machine, one value per file.
DMI_DIR = Path("/sys/class/dmi/id")

#: The DMI files read here. sys_vendor is who built it, product_name is what it
#: calls the model, board_name is the mainboard and product_family is a coarser
#: label some vendors use for the line the machine belongs to.
DMI_FIELDS = ("sys_vendor", "product_name", "board_name", "product_family")

#: The vendor strings that make a machine one of these, matched against all four
#: DMI fields together so a machine whose sys_vendor is "ASUSTeK COMPUTER INC."
#: is still an asus and one whose product is "ThinkPad X1" is still a lenovo.
PLATFORM_MARKERS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("apple", ("apple",)),
    ("lenovo", ("lenovo", "thinkpad", "ideapad", "thinkcentre")),
    ("asus", ("asus", "asustek")),
    ("hp", ("hewlett", "packard", "hp inc")),
    ("dell", ("dell",)),
    ("acer", ("acer",)),
    ("msi", ("micro-star", "msi")),
    ("toshiba", ("toshiba", "dynabook")),
    ("samsung", ("samsung",)),
    ("sony", ("sony", "vaio")),
)

#: The ACPI module a platform's function keys go through, printed in the report
#: so a machine that is still dead afterwards has somewhere to look.
PLATFORM_MODULES = {
    "lenovo": "ideapad_laptop / thinkpad_acpi",
    "asus": "asus-nb-wmi / asus_wmi",
    "hp": "hp-wmi",
    "dell": "dell-wmi / dell_laptop",
    "acer": "acer_wmi",
    "msi": "msi-wmi",
    "toshiba": "toshiba_acpi",
    "samsung": "samsung-laptop",
    "sony": "sony-laptop",
    "other": "the vendor's own wmi module",
}

# --- the Apple parameter -----------------------------------------------------

#: The value written on an Apple machine: F1-F12 on the top row, Fn for the
#: special functions. This is what a person writing code on a Mac expects.
APPLE_FNMODE = 2

#: Where the loaded module's own value can be read back from. The path only
#: exists once hid_apple is loaded.
APPLE_FNMODE_PATH = Path("/sys/module/hid_apple/parameters/fnmode")

#: The value hid_apple is at when it is loaded and the file above is not there.
APPLE_FNMODE_DEFAULT = 1

#: How many bytes of /proc/cmdline to read.
CMDLINE_LIMIT = 65536


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


def run(command: list[str], capture: bool = False) -> subprocess.CompletedProcess[str]:
    """Run a command, with its output kept only when it is asked for.

    Nothing goes through a shell: every command is a list, so a path with a
    space in it cannot turn into two arguments.
    """
    if capture:
        return subprocess.run(command, check=False, capture_output=True, text=True)
    return subprocess.run(command, check=False, text=True)


def is_root() -> bool:
    """Whether this process can write /etc and /boot.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    install; reading it through ``getattr`` keeps the module importable there so
    the file can be linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """Be root, re-running the script through sudo when possible.

    Everything this script writes - /etc/default/grub, /etc/modprobe.d, grub.cfg
    - is root's, so there is no useful partial run as a normal user. sudo is used
    when it exists and the fact is announced rather than done silently; the
    environment variable stops a sudo that fails to change the user from looping.
    """
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit(
            "error: still not root after sudo; run the script as root "
            "(su -c 'python3 fix_fn_keys.py')"
        )
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: this writes /etc/default/grub and regenerates grub.cfg; "
            "run this script as root"
        )
    log.step("This changes the boot configuration; re-running it through sudo")
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

    The copy is taken once and kept: a second run must not overwrite it with this
    script's own output, because the file worth keeping is the one the machine
    had before the script ever touched it. It is copied rather than moved for the
    same reason - the caller merges into the file afterwards, and a moved file
    would be merged into nothing.
    """
    if not path.exists():
        return None
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return backup
    shutil.copy2(path, backup)
    return backup


# --- /etc/default/grub -------------------------------------------------------
# The line this script owns is GRUB_CMDLINE_LINUX_DEFAULT, which 00_header turns
# into the `linux` line of every menu entry. One word is added to it, and every
# other word, comment and blank line of the file is left exactly as found.


def assignment(line: str) -> tuple[str, str] | None:
    """The name and value of a shell assignment, or None.

    Only the exact shape ``NAME=value``, with an optional leading ``export``, is
    recognised, and a line that starts with a comment is not one. That is what
    makes the merge idempotent: a line the file no longer sees an assignment in
    is not touched twice.
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


def cmdline_value(text: str) -> str | None:
    """The value of GRUB_CMDLINE_LINUX_DEFAULT, unquoted, or None."""
    for line in text.splitlines():
        entry = assignment(line)
        if entry is not None and entry[0] == CMDLINE_KEY:
            return entry[1].strip().strip('"').strip("'")
    return None


def merge_cmdline(existing: str, token: str) -> str:
    """Add ``token`` to GRUB_CMDLINE_LINUX_DEFAULT, keeping every other line.

    The line is rewritten as ``KEY="word word word"``: Debian's own file writes
    it quoted, the header sources it as shell, and an unquoted kernel command
    line with a space in it would be two arguments.
    """
    lines = existing.splitlines()
    target = None
    value = ""
    for index, line in enumerate(lines):
        entry = assignment(line)
        if entry is not None and entry[0] == CMDLINE_KEY:
            target = index
            value = entry[1].strip().strip('"').strip("'")
            break

    words = value.split()
    if token not in words:
        words.append(token)
    joined = " ".join(words)
    if target is None:
        lines.append(f'{CMDLINE_KEY}="{joined}"')
    else:
        lines[target] = f'{CMDLINE_KEY}="{joined}"'
    return "\n".join(lines).rstrip("\n") + "\n"


def running_cmdline() -> str:
    """The kernel command line this machine booted with."""
    return read_text(Path("/proc/cmdline"))[:CMDLINE_LIMIT]


def install_cmdline(log: Log) -> None:
    """Add the ACPI token to GRUB_CMDLINE_LINUX_DEFAULT.

    The file as it was is copied aside once, before anything is merged into it,
    because that copy is the whole undo.
    """
    existing = read_text(GRUB_DEFAULT_FILE)
    if not existing:
        raise SystemExit(
            f"error: {GRUB_DEFAULT_FILE} does not exist or is empty; is GRUB "
            "installed as this machine's boot loader?"
        )
    merged = merge_cmdline(existing, ACPI_TOKEN)
    if merged == existing:
        log.detail(f"{CMDLINE_KEY} already contains {ACPI_TOKEN}")
        return
    backup = backup_once(GRUB_DEFAULT_FILE)
    if backup is not None:
        log.detail(f"backed up {GRUB_DEFAULT_FILE.name} to {backup.name}")
    write_text(GRUB_DEFAULT_FILE, merged)
    log.detail(f"added {ACPI_TOKEN} to {CMDLINE_KEY}")


# --- regenerating what reads the files ---------------------------------------


def update_grub_command() -> list[str] | None:
    """The command that regenerates grub.cfg here, or None.

    ``update-grub`` is Debian's wrapper and does the same thing with the right
    output path, so it is preferred where it exists.
    """
    update_grub = shutil.which("update-grub")
    if update_grub is None:
        return None
    return [update_grub]


def update_initramfs_command() -> list[str] | None:
    """The command that rebuilds the initramfs here, or None.

    It is only needed on an Apple machine: the fnmode parameter has to be set
    when hid_apple is first loaded, which on a machine with an encrypted root
    happens inside the initramfs, before /etc/modprobe.d is readable.
    """
    update = shutil.which("update-initramfs")
    if update is None:
        return None
    return [update, "-u"]


def refresh_grub(log: Log) -> bool:
    """Run update-grub, because /etc/default/grub does nothing until it is.

    Nothing is regenerated by hand: grub.cfg is generated, and one written by
    this script would be overwritten by the next package upgrade and would miss
    whichever entries the machine's own /etc/grub.d adds. The output is left on
    the terminal because it is the only sign of what the command did, and a run
    that printed nothing for a minute would look like a run that had stopped.
    """
    command = update_grub_command()
    if command is None:
        log.warn(
            "update-grub is not installed, so grub.cfg still holds the old "
            "command line; run it yourself, or the fix applies at the next "
            "kernel update"
        )
        return False
    log.detail("running: " + " ".join(command))
    if run(command).returncode != 0:
        log.warn("update-grub failed; grub.cfg was not regenerated")
        return False
    log.detail("regenerated /boot/grub/grub.cfg")
    return True


def refresh_initramfs(log: Log) -> bool:
    """Run update-initramfs -u, so the module parameter is set from the start."""
    command = update_initramfs_command()
    if command is None:
        log.detail(
            "update-initramfs is not installed; the module parameter applies at "
            "the next boot anyway on a machine with an unencrypted root"
        )
        return False
    log.detail("running: " + " ".join(command))
    if run(command).returncode != 0:
        log.warn("update-initramfs failed; the initramfs was not rebuilt")
        return False
    log.detail("rebuilt the initramfs")
    return True


# --- the machine -------------------------------------------------------------


def dmi_value(name: str) -> str:
    """One DMI field, or an empty string when the kernel did not fill it in."""
    try:
        return (DMI_DIR / name).read_text(encoding="utf-8").strip()
    except OSError:
        return ""


@dataclass(frozen=True)
class Machine:
    """What this machine says it is, and the family its fix belongs to."""

    vendor: str
    product: str
    board: str
    family: str

    def description(self) -> str:
        parts = [part for part in (self.vendor, self.product) if part]
        return " ".join(parts) if parts else "unknown machine"

    def is_apple(self) -> bool:
        return self.family == "apple"


def detect_machine() -> Machine:
    """Read the DMI fields and reduce them to one platform family.

    The four fields are joined before matching because no single one is
    reliable: sys_vendor is sometimes the ODM rather than the brand, and a
    machine whose vendor is "LENOVO" with a product of "ThinkPad X1" is only
    recognised as a ThinkPad by looking at both.
    """
    values = {field: dmi_value(field) for field in DMI_FIELDS}
    haystack = " ".join(values.values()).lower()
    family = "other"
    for name, markers in PLATFORM_MARKERS:
        if any(marker in haystack for marker in markers):
            family = name
            break
    return Machine(
        vendor=values.get("sys_vendor", ""),
        product=values.get("product_name", ""),
        board=values.get("board_name", ""),
        family=family,
    )


# --- the Apple parameter -----------------------------------------------------


def modprobe_text() -> str:
    """The contents of the hid_apple drop-in.

    Only an Apple machine needs one: everywhere else the top row is ACPI and is
    fixed by the kernel command line, and a drop-in that sets no parameter would
    be a file with nothing in it.
    """
    return "\n".join(
        [
            f"# {MARKER}",
            "#",
            "# The top row of an Apple keyboard sends the special function",
            "# (brightness, volume) by default, and F1-F12 need the Fn key held",
            "# down. fnmode swaps that around so the top row types F1-F12.",
            "#",
            f"# fnmode={APPLE_FNMODE}: F1-F12, Fn for the special functions.",
            "#",
            "",
            f"options hid_apple fnmode={APPLE_FNMODE}",
            "",
        ]
    )


def install_modprobe(log: Log, machine: Machine) -> bool:
    """Write the hid_apple parameter on an Apple machine, returning whether it was written."""
    if not machine.is_apple():
        return False
    text = modprobe_text()
    if read_text(MODPROBE_FILE) == text:
        log.detail(f"{MODPROBE_FILE} is already correct")
        return False
    backup = backup_once(MODPROBE_FILE)
    if backup is not None:
        log.detail(f"backed up {MODPROBE_FILE.name} to {backup.name}")
    write_text(MODPROBE_FILE, text)
    log.detail(f"wrote {MODPROBE_FILE} (hid_apple fnmode={APPLE_FNMODE})")
    return True


def live_fnmode() -> str | None:
    """The fnmode the loaded hid_apple module is using, or None when it is not loaded."""
    value = read_text(APPLE_FNMODE_PATH).strip()
    return value or None


# --- checking the result -----------------------------------------------------
# Everything here is read back from what was written rather than trusted, because
# every way this can fail ends in the same place: function keys that still do
# nothing after a reboot. A token that was never written, a token that was written
# but never regenerated into grub.cfg, and a token that is in grub.cfg but has not
# been booted yet all look identical from the keyboard, and this is the step that
# exists to tell them apart before a reboot tells the user instead.


def cmdline_problems() -> list[str]:
    """What is wrong with the kernel command line the machine is configured with."""
    text = read_text(GRUB_DEFAULT_FILE)
    if not text:
        return [f"{GRUB_DEFAULT_FILE} is missing or empty"]
    value = cmdline_value(text)
    if value is None:
        return [
            f"{GRUB_DEFAULT_FILE} does not set {CMDLINE_KEY}, so nothing this "
            "script writes would reach the kernel"
        ]
    if ACPI_TOKEN not in value.split():
        return [
            f"{CMDLINE_KEY} does not contain {ACPI_TOKEN}; run the script, then "
            "update-grub"
        ]
    return []


def applied_problems() -> list[str]:
    """Whether the running kernel actually booted with the token.

    A change to /etc/default/grub and a successful update-grub both take effect
    at the next boot, not now, so a machine that has not rebooted since the fix
    was written still has dead function keys - which is the difference between
    "the fix is wrong" and "the fix has not been read yet".
    """
    booted = running_cmdline()
    if not booted or ACPI_TOKEN in booted.split():
        return []
    return [
        f"the running kernel booted without {ACPI_TOKEN}, so the fix is written "
        "but not in force; reboot for it to take effect"
    ]


def modprobe_problems(machine: Machine) -> list[str]:
    """What is wrong with the hid_apple parameter, on an Apple machine."""
    if not machine.is_apple():
        return []
    problems: list[str] = []
    wanted = f"options hid_apple fnmode={APPLE_FNMODE}"
    if not MODPROBE_FILE.is_file():
        problems.append(f"{MODPROBE_FILE} is missing; run the script")
    elif wanted not in read_text(MODPROBE_FILE):
        problems.append(
            f"{MODPROBE_FILE} does not set {wanted!r}, so the top row still "
            "sends the special functions instead of F1-F12"
        )
    live = live_fnmode()
    if live is not None and live != str(APPLE_FNMODE):
        problems.append(
            f"the loaded hid_apple module is at fnmode={live}, not {APPLE_FNMODE}; "
            "it changes at the next boot"
        )
    return problems


def check_result(log: Log, machine: Machine) -> int:
    """Report what would stop the function keys working, returning how many things do."""
    problems = cmdline_problems() + applied_problems() + modprobe_problems(machine)

    log.step("Checking the result")
    if not problems:
        log.note("No problems found.")
        return 0
    log.note(f"{len(problems)} problem(s) found:")
    for problem in problems:
        log.note(f"  {problem}")
    return len(problems)


# --- entry point -------------------------------------------------------------


def main() -> int:
    """Fix the function keys, with no options to pass."""
    log = Log()
    ensure_root(log)

    machine = detect_machine()
    log.step(f"Fixing the function keys on {machine.description()} ({machine.family})")
    module = PLATFORM_MODULES.get(machine.family)
    if module is not None and not machine.is_apple():
        log.detail(f"this platform's function keys go through: {module}")

    wrote_modprobe = install_modprobe(log, machine)

    log.step("Configuring the kernel command line")
    install_cmdline(log)

    log.step("Regenerating grub.cfg")
    refresh_grub(log)

    if wrote_modprobe:
        log.step("Rebuilding the initramfs")
        refresh_initramfs(log)

    problems = check_result(log, machine)

    log.note("")
    if problems:
        log.note("The problems listed above have to be fixed before the function")
        log.note("keys will work; until then they do nothing.")
    else:
        log.note("Done. The fix is read when the machine boots, and not before:")
        log.note("reboot for the function keys to start working.")
    log.note("")
    if machine.is_apple():
        log.note(f"  module option  {MODPROBE_FILE} (hid_apple fnmode={APPLE_FNMODE})")
    log.note(f"  kernel option  {GRUB_DEFAULT_FILE} ({ACPI_TOKEN})")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
