#!/usr/bin/env python3
"""GnuchanOS - full touchpad support for Debian, independent of any WM or DE.

    python3 settings_touchpad.py              # install and check
    python3 settings_touchpad.py --check      # report problems only
    python3 settings_touchpad.py --uninstall  # remove everything it wrote

What "full touchpad support" means here
---------------------------------------
tap to click, tap and drag, drag lock, two finger scrolling, horizontal
scrolling, natural (or classic) scrolling direction, scroll distance, click
methods (finger clicks or button areas), middle button emulation, disable
while typing (palm rejection), disabling the pad while an external mouse is
plugged in, pointer speed and the adaptive acceleration profile, left handed
orientation, and high resolution wheel scrolling.

Why there is no window manager or desktop in this file
------------------------------------------------------
A setting written through a desktop's own panel is a setting of that desktop:
it lands in GSettings, kcminputrc or an XFCE channel, and a session that is not
that desktop reads none of it. On a bare window manager - i3, openbox, xfwm4,
dwm, fluxbox - those files are never written at all and there is no panel to
write them from.

Everything this script writes therefore lives below the desktop:

  /etc/X11/xorg.conf.d/90-gnuchan-touchpad.conf
      An InputClass section for the X server itself. The X server reads it
      before any client exists, matches it against every touchpad the kernel
      reports, and hands the driver the options below - so a touchpad
      configured here behaves the same under i3 as it does under XFCE, and
      "which desktop" is not a question the driver is ever asked. It survives
      a reboot, it needs the settings applied nowhere else, and no session
      startup file, autostart entry or daemon is involved.

  xinput, at install time
      The X server reads its configuration once, when it starts. Writing the
      file above changes the next session, not the one running now, so the
      same settings are pushed into the live server through xinput - which
      talks to the X server, not to a desktop, and works identically whatever
      window manager is drawing the frames around the pointer.

Wayland
-------
On Wayland there is no such layer: the compositor owns the input devices and
its configuration is its own. GNOME reads GSettings, wlroots compositors read
their own text configuration, KDE reads kcminputrc, and none of them read each
other or the X server file above. That is a design decision of Wayland and not
something a script can paper over. This one detects a Wayland session, writes
the X11 file anyway - so switching to an X11 session on the same machine is
already configured - and prints the exact command for the compositor it found
instead of pretending to have set it.

Distribution
------------
Debian and what is derived from it. The driver (xserver-xorg-input-libinput)
and the tools this uses (xinput, libinput-tools) are installed with apt when
they are missing and this script is allowed to install packages.

License: GPL3
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, replace
from pathlib import Path

# --- names and locations -----------------------------------------------------

SCRIPT_NAME = "settings_touchpad.py"
MARKER = f"written by {SCRIPT_NAME}"
BACKUP_SUFFIX = ".gnuchan-backup"

#: The section identifier, which is also what shows up in `xinput list-props`
#: output and in the X server log when the file is read.
SECTION_IDENTIFIER = "GnuchanOS touchpad"

#: Where the X server looks for configuration of its own. /etc is the one that
#: is meant for the machine, and is read ahead of the files the packages put in
#: /usr/share; the name sorts late so that anything already there is overridden
#: rather than the other way round.
CONFIG_DIR = Path("/etc/X11/xorg.conf.d")
CONFIG_FILE = CONFIG_DIR / "90-gnuchan-touchpad.conf"

#: The file an older install may have left in the distribution directory, which
#: is never touched: it belongs to the package manager.
DISTRO_CONFIG_DIR = Path("/usr/share/X11/xorg.conf.d")

# --- the settings ------------------------------------------------------------
# The names in the tuples are the ones the two interfaces use for the same three
# way switch: the Option value in xorg.conf, and the order of the boolean bundle
# the X property is read as. They are not interchangeable and the order is what
# makes the property mean what the option says.

CLICK_METHODS = ("buttonareas", "clickfinger")
SCROLL_METHODS = ("twofinger", "edge", "button")
SEND_EVENT_MODES = ("disabled", "disabled-on-external-mouse")
ACCEL_PROFILES = ("adaptive", "flat")
TAP_BUTTON_MAPS = ("lrm", "lmr")


@dataclass(frozen=True)
class Settings:
    """One touchpad configuration, in the terms both interfaces understand.

    The defaults are the ones a modern laptop touchpad is expected to have:
    tapping on, two finger scrolling, finger based clicks, palm rejection while
    typing and the pad asleep while an external mouse is in use. Natural
    scrolling is off because that is the direction a desktop PC has always
    scrolled in and this is the setting people notice most - ``--natural-scroll``
    turns it on.
    """

    tap: bool = True
    tap_drag: bool = True
    tap_drag_lock: bool = False
    tap_button_map: str = "lrm"
    natural_scroll: bool = False
    scroll_method: str = "twofinger"
    horizontal_scroll: bool = True
    scroll_pixel_distance: int = 26
    click_method: str = "clickfinger"
    middle_emulation: bool = True
    disable_while_typing: bool = True
    send_events_mode: str = "enabled"
    high_res_wheel: bool = True
    accel_profile: str = "adaptive"
    accel_speed: float = 0.0
    left_handed: bool = False

    #: Range libinput accepts for the pointer speed, and the one the property
    #: is described with. Kept here so the CLI and the check agree.
    SPEED_MIN = -1.0
    SPEED_MAX = 1.0

    def xorg_options(self) -> list[tuple[str, str]]:
        """The section's Option lines, in the order they are written.

        Booleans are written as on and off: xorg.conf's parser accepts true and
        false as well, but on and off is what the libinput man page uses and
        what a person reading the file expects.
        """
        return [
            ("AccelProfile", self.accel_profile),
            ("AccelSpeed", f"{self.accel_speed:g}"),
            ("ClickMethod", self.click_method),
            ("DisableWhileTyping", on_off(self.disable_while_typing)),
            ("HighResolutionWheelScrolling", on_off(self.high_res_wheel)),
            ("HorizontalScrolling", on_off(self.horizontal_scroll)),
            ("LeftHanded", on_off(self.left_handed)),
            ("MiddleEmulation", on_off(self.middle_emulation)),
            ("NaturalScrolling", on_off(self.natural_scroll)),
            ("ScrollMethod", self.scroll_method),
            ("ScrollPixelDistance", str(self.scroll_pixel_distance)),
            ("SendEventsMode", self.send_events_mode),
            ("Tapping", on_off(self.tap)),
            ("TappingButtonMap", self.tap_button_map),
            ("TappingDrag", on_off(self.tap_drag)),
            ("TappingDragLock", on_off(self.tap_drag_lock)),
        ]

    def runtime_properties(self) -> list[tuple[str, list[str]]]:
        """The same settings as X properties, with the arguments xinput takes.

        Each value is the list of arguments that follows the property name on
        the command line. The three boolean bundles are the reason this is not
        a single string: the property carries one boolean per permitted value,
        in the order the tuple at the top of the file lists them, and exactly
        one of them is a 1 - so "which scroll method" becomes "1 0 0" and the
        reader can line that up with the property description.
        """
        return [
            ("libinput Accel Profile Enabled", bundle(self.accel_profile, ACCEL_PROFILES)),
            ("libinput Accel Speed", [f"{self.accel_speed:g}"]),
            ("libinput Click Methods Enabled", bundle(self.click_method, CLICK_METHODS)),
            ("libinput Disable While Typing Enabled", bool_arg(self.disable_while_typing)),
            ("libinput High Resolution Wheel Scroll Enabled", bool_arg(self.high_res_wheel)),
            ("libinput Horizontal Scroll Enabled", bool_arg(self.horizontal_scroll)),
            ("libinput Left Handed Enabled", bool_arg(self.left_handed)),
            ("libinput Middle Emulation Enabled", bool_arg(self.middle_emulation)),
            ("libinput Natural Scrolling Enabled", bool_arg(self.natural_scroll)),
            ("libinput Scroll Method Enabled", bundle(self.scroll_method, SCROLL_METHODS)),
            ("libinput Scroll Pixel Distance", [str(self.scroll_pixel_distance)]),
            ("libinput Send Events Mode Enabled", bundle(self.send_events_mode, SEND_EVENT_MODES)),
            ("libinput Tapping Enabled", bool_arg(self.tap)),
            ("libinput Tapping Button Mapping Enabled", bundle(self.tap_button_map, TAP_BUTTON_MAPS)),
            ("libinput Tapping Drag Enabled", bool_arg(self.tap_drag)),
            ("libinput Tapping Drag Lock Enabled", bool_arg(self.tap_drag_lock)),
        ]


# --- the two interfaces' spellings of the same thing -------------------------


def on_off(value: bool) -> str:
    """A boolean as an xorg.conf Option value."""
    return "on" if value else "off"


def bool_arg(value: bool) -> list[str]:
    """A boolean as the argument an X property expects."""
    return ["1" if value else "0"]


def bundle(value: str, order: tuple[str, ...]) -> list[str]:
    """A one of several setting as the boolean bundle the property reads.

    ``libinput Scroll Method Enabled`` is not a number, it is three booleans -
    two-finger, edge, button - of which the enabled one is the 1. This turns
    the name back into that list.
    """
    if value not in order:
        value = order[0]
    return ["1" if item == value else "0" for item in order]


# --- output ------------------------------------------------------------------


class Log:
    """Progress output, honouring --quiet.

    Warnings go to stderr and are not suppressed by --quiet: a run that could
    not do something has to say so even when the caller only wanted the result.
    """

    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet

    def step(self, message: str) -> None:
        if not self.quiet:
            print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        if not self.quiet:
            print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        if not self.quiet:
            print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- the machine -------------------------------------------------------------


def os_release() -> dict[str, str]:
    """Parse ``/etc/os-release`` into a dictionary, empty when it is absent."""
    result: dict[str, str] = {}
    try:
        text = Path("/etc/os-release").read_text(encoding="utf-8")
    except OSError:
        return result
    for line in text.splitlines():
        name, separator, value = line.partition("=")
        if separator:
            result[name.strip()] = value.strip().strip('"').strip("'")
    return result


def distro_description() -> str:
    """What this distribution calls itself, for the first line of output."""
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def session_type() -> str:
    """The session this is running in: wayland, x11 or unknown.

    ``XDG_SESSION_TYPE`` is what the display manager sets and what every
    session uses to describe itself; the two fallbacks are there because a
    session started from a plain ``startx`` may not have it set at all.
    """
    value = os.environ.get("XDG_SESSION_TYPE", "").strip().lower()
    if value in ("wayland", "x11"):
        return value
    if os.environ.get("WAYLAND_DISPLAY"):
        return "wayland"
    if os.environ.get("DISPLAY"):
        return "x11"
    return "unknown"


def compositor_name() -> str:
    """The compositor a Wayland session is running, as far as it can be told.

    Wayland does not report this in any standard variable, so it is read from
    the environment the compositor is known to export and from the process
    list. The answer only selects which hint is printed; nothing is written
    from it.
    """
    if os.environ.get("SWAYSOCK"):
        return "sway"
    if os.environ.get("HYPRLAND_INSTANCE_SIGNATURE"):
        return "hyprland"
    if os.environ.get("GNOME_SHELL_SESSION_MODE") or os.environ.get(
        "XDG_CURRENT_DESKTOP", ""
    ).upper().startswith("GNOME"):
        return "gnome"
    if "KDE" in os.environ.get("XDG_CURRENT_DESKTOP", "").upper():
        return "kde"
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "")
    return desktop.split(":")[0].lower() if desktop else "unknown"


# --- running commands, with root when it is needed ---------------------------


def is_root() -> bool:
    """Whether this process can write /etc without help.

    ``os.geteuid`` does not exist on Windows, where this script has nothing to
    configure; reading it through ``getattr`` keeps the module importable there
    so the file can be exercised and linted anywhere.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def _writable(path: Path) -> bool:
    """Whether this process may create or replace things in ``path``.

    A directory that does not exist is judged by its nearest existing ancestor,
    because that is the one that has to be writable to make it.
    """
    probe = path
    while not probe.exists() and probe != probe.parent:
        probe = probe.parent
    return os.access(probe, os.W_OK)


def run_as_root(
    command: list[str], parent: Path, input_text: str | None = None
) -> bool:
    """Run one command, through sudo only when writing to ``parent`` needs it.

    The command is a list rather than a shell string, so a path with a space in
    it cannot become two arguments, and the sudo prompt is left where it is: a
    script that needs a password should ask for it where the user can answer,
    which is their terminal. A machine whose /etc is writable - which is every
    test - never reaches sudo at all.
    """
    if not is_root() and not _writable(parent):
        sudo = shutil.which("sudo")
        if sudo is None:
            return False
        command = [sudo, *command]
    if input_text is None:
        result = subprocess.run(command, check=False)
    else:
        result = subprocess.run(command, check=False, input=input_text, text=True)
    return result.returncode == 0


def run_capture(command: list[str]) -> tuple[int, str]:
    """Run a command and return its exit status and its combined output.

    Nothing goes through a shell, so the arguments are exactly the ones given.
    """
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    return result.returncode, (result.stdout or "") + (result.stderr or "")


# --- writing the configuration -----------------------------------------------


def config_text(settings: Settings) -> str:
    """The xorg.conf.d file, as one string.

    The MatchIsTouchpad line is what makes it apply to every touchpad the
    kernel reports without naming one, so a machine whose pad is replaced, or
    which has two of them, is still covered. There is no MatchDriver: the
    driver is named in the section instead, and a machine with no libinput
    installed gets an error in the log rather than a silently ignored file.
    """
    lines = [
        f"# {MARKER}",
        "#",
        "# Touchpad settings for the X server itself, so they apply under every",
        "# window manager and desktop - i3, openbox, xfwm4, XFCE, dwm - and not",
        "# just the one whose settings panel wrote them.",
        "#",
        "# Replacing this file and restarting the X session is the whole edit;",
        f"# {SCRIPT_NAME} --uninstall removes it.",
        "#",
        'Section "InputClass"',
        f'    Identifier "{SECTION_IDENTIFIER}"',
        '    MatchIsTouchpad "on"',
        '    Driver "libinput"',
    ]
    lines += [
        f'    Option "{name}" "{value}"' for name, value in settings.xorg_options()
    ]
    lines.append("EndSection")
    return "\n".join(lines) + "\n"


def backup_once(log: Log, path: Path) -> None:
    """Keep one copy of a file, taken before the first run that replaces it.

    A second run must not overwrite the copy with this script's own output, or
    --uninstall would restore the very file it is removing. The copy is done in
    Python when the directory allows it, and through sudo when it does not -
    which is the same rule every file operation here follows, and the reason
    none of this needs a shell.
    """
    if not path.exists():
        return
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return
    try:
        shutil.copy2(path, backup)
    except OSError:
        if not run_as_root(["cp", "-a", str(path), str(backup)], path.parent):
            log.detail(f"could not back up {path.name}")
            return
    log.detail(f"backed up {path.name} to {backup.name}")


def install_config(log: Log, settings: Settings) -> bool:
    """Write the input class the X server reads at startup.

    The write is Python's own when /etc is writable - which it is when the
    script runs as root, on a machine where the user owns /etc, and in every
    test - and falls back to sudo for a machine where it is not. Doing it in
    Python rather than by piping to tee is what lets the whole install,
    backup and uninstall path be exercised without a Debian machine under it.
    """
    backup_once(log, CONFIG_FILE)
    text = config_text(settings)
    try:
        CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        CONFIG_FILE.write_text(text, encoding="utf-8")
        CONFIG_FILE.chmod(0o644)
    except OSError:
        if not run_as_root(["mkdir", "-p", str(CONFIG_DIR)], CONFIG_DIR):
            log.warn(f"could not create {CONFIG_DIR}; run this script as root")
            return False
        if not run_as_root(
            ["tee", str(CONFIG_FILE)], CONFIG_FILE.parent, input_text=text
        ):
            log.warn(f"could not write {CONFIG_FILE}; run this script as root")
            return False
    log.detail(f"wrote {CONFIG_FILE}")
    return True


# --- applying to the running session -----------------------------------------
# The X server reads its configuration once, at startup. Everything below is
# what makes the settings apply to the session that is running now, through
# xinput, which talks to the X server and to nothing else.


def xinput_path() -> str | None:
    return shutil.which("xinput")


def touchpad_devices(log: Log) -> list[str]:
    """The names of the touchpads the running X server knows about.

    A device is a touchpad when it carries a property only a touchpad has -
    tapping, or disable while typing. Matching on the name would be wrong: the
    word touchpad is a manufacturer's habit, and pads named TrackPoint,
    ClickPad or Synaptics exist, as do mice with the word in their name.
    """
    xinput = xinput_path()
    if xinput is None or not os.environ.get("DISPLAY"):
        return []
    status, names = run_capture([xinput, "list", "--name-only"])
    if status != 0:
        log.detail("xinput could not list the devices")
        return []
    found: list[str] = []
    for name in (line.strip() for line in names.splitlines()):
        if not name:
            continue
        status, props = run_capture([xinput, "list-props", name])
        if status != 0:
            continue
        if "libinput Tapping Enabled" in props or "libinput Disable While Typing Enabled" in props:
            found.append(name)
    return found


def apply_to_running_session(log: Log, settings: Settings) -> int:
    """Push the settings into the running X server, returning how many failed.

    Every property is set on its own so that one the pad does not have - a
    touchpad without middle button emulation, an old driver - is reported
    against its own name instead of stopping the rest, which is the difference
    between a pad missing one feature and a pad missing all of them.
    """
    xinput = xinput_path()
    if xinput is None:
        log.detail("xinput is not installed; the configuration applies at the next login")
        return 0
    if session_type() == "wayland":
        log.detail("this is a Wayland session; xinput cannot reach it")
        return 0
    if not os.environ.get("DISPLAY"):
        log.detail("DISPLAY is not set; the configuration applies at the next X session")
        return 0

    devices = touchpad_devices(log)
    if not devices:
        log.detail("the running X server reports no touchpad")
        return 0

    failures = 0
    for device in devices:
        log.detail(f"applying to {device}")
        for name, arguments in settings.runtime_properties():
            status, output = run_capture([xinput, "set-prop", device, name, *arguments])
            if status != 0:
                failures += 1
                log.detail(f"  {name}: not supported by this device")
    return failures


def reset_running_session(log: Log) -> None:
    """Put the running session's touchpad back to the driver's defaults.

    Used by --uninstall. The configuration file is the only thing this script
    owns, but leaving the live session holding settings no longer written down
    anywhere is how a pad ends up behaving in a way nothing on disk explains,
    so the driver is asked for its defaults again. ``libinput`` exports a
    ``Default`` property for each setting, which is exactly what is wanted.
    """
    xinput = xinput_path()
    if xinput is None or not os.environ.get("DISPLAY") or session_type() != "x11":
        return
    for device in touchpad_devices(log):
        status, props = run_capture([xinput, "list-props", device])
        if status != 0:
            continue
        for line in props.splitlines():
            name, separator, _ = line.partition(":")
            name = name.strip()
            if not separator or not name.startswith("libinput ") or name.endswith(" Default"):
                continue
            default = f"{name} Default"
            if default not in props:
                continue
            # The default property exists next to the value property; one
            # property cannot be read to set another, so the value is copied
            # with a command that asks the server for it and writes it back.
            value = _property_value(props, default)
            if value is None:
                continue
            run_capture([xinput, "set-prop", device, name, *value])
        log.detail(f"reset {device} to the driver defaults")


def _property_value(props: str, name: str) -> list[str] | None:
    """The numbers on the ``name`` line of ``xinput list-props`` output.

    The line is indented and looks like ``    name (123): 1 0``; everything
    after the last colon is the value.
    """
    for line in props.splitlines():
        label, separator, value = line.partition(":")
        if not separator or label.strip() != name:
            continue
        arguments = value.split()
        return arguments or None
    return None


# --- packages ----------------------------------------------------------------


def apt_available() -> bool:
    return shutil.which("apt-get") is not None


def can_install_packages() -> bool:
    """Whether an apt install here could run without stopping to ask.

    A passwordless sudo is what makes that possible; without one the command is
    printed for the user to run instead of the script waiting on a prompt that
    is not attached to a terminal.
    """
    if apt_available() is False:
        return False
    if is_root():
        return True
    sudo = shutil.which("sudo")
    if sudo is None:
        return False
    return subprocess.run([sudo, "-n", "true"], check=False).returncode == 0


def missing_tools() -> list[str]:
    """The commands this script uses that are not installed."""
    return [name for name in ("xinput",) if shutil.which(name) is None]


def libinput_driver_installed() -> bool:
    """Whether the X input driver this configuration names is present.

    The driver is not a command, so it is looked for where the packages put it.
    A machine that only ever runs Wayland has no need of it and this check is
    reported rather than acted on.
    """
    candidates = (
        Path("/usr/lib/xorg/modules/input/libinput_drv.so"),
        Path("/usr/lib64/xorg/modules/input/libinput_drv.so"),
        Path("/usr/lib/xorg/modules/input/libinput_drv.la"),
    )
    return any(path.exists() for path in candidates)


def install_packages(log: Log, packages: tuple[str, ...]) -> bool:
    """Install packages with apt, returning whether it worked."""
    if not packages:
        return True
    command = ["apt-get", "install", "-y", "--no-install-recommends", *packages]
    if not is_root():
        sudo = shutil.which("sudo")
        if sudo is None:
            return False
        command = [sudo, *command]
    log.detail("running: " + " ".join(command))
    return subprocess.run(command, check=False).returncode == 0


def ensure_tools(log: Log, allow_packages: bool) -> None:
    """Make sure the driver and the tools are installed.

    Nothing is fatal: a machine with no xinput still gets the configuration
    file, which is the part that lasts, and the applied-now half is skipped
    with a word about it.
    """
    needed: list[str] = []
    if not libinput_driver_installed():
        needed.append("xserver-xorg-input-libinput")
    if missing_tools():
        needed.append("xinput")
    if not needed:
        return
    if not allow_packages or not can_install_packages():
        log.detail("not installed: " + ", ".join(needed))
        log.detail("run: sudo apt-get install " + " ".join(needed))
        return
    log.step("Installing the touchpad driver and tools")
    if install_packages(log, tuple(needed)):
        log.detail("installed: " + ", ".join(needed))
    else:
        log.warn("apt-get could not install: " + ", ".join(needed))


# --- checking ----------------------------------------------------------------


def config_problems() -> list[str]:
    """What is wrong with the installed configuration file, or nothing."""
    problems: list[str] = []
    if not CONFIG_FILE.is_file():
        problems.append(f"{CONFIG_FILE} is missing; run the script")
        return problems
    try:
        text = CONFIG_FILE.read_text(encoding="utf-8")
    except OSError:
        problems.append(f"{CONFIG_FILE} could not be read")
        return problems
    required = (
        'Section "InputClass"',
        'MatchIsTouchpad "on"',
        'Driver "libinput"',
        f'Identifier "{SECTION_IDENTIFIER}"',
        "EndSection",
    )
    for line in required:
        if line not in text:
            problems.append(f"{CONFIG_FILE} is missing {line}")
    for name in ("Tapping", "NaturalScrolling", "DisableWhileTyping", "ScrollMethod"):
        if f'Option "{name}"' not in text:
            problems.append(f"{CONFIG_FILE} has no {name} option")
    return problems


def running_problems(settings: Settings) -> list[str]:
    """Settings the live session disagrees with the file about.

    The X server reads its configuration at startup and never again, so the
    live session is a second, independent statement of the same settings - and
    a session that has not had them applied behaves exactly like a machine
    where the file was never written. That is the failure this check exists to
    tell apart.
    """
    xinput = xinput_path()
    if xinput is None or not os.environ.get("DISPLAY") or session_type() != "x11":
        return []
    problems: list[str] = []
    for device in touchpad_devices(Log(quiet=True)):
        status, props = run_capture([xinput, "list-props", device])
        if status != 0:
            continue
        for name, arguments in settings.runtime_properties():
            value = _property_value(props, name)
            if value is None:
                continue
            if value != arguments:
                problems.append(
                    f"{device}: {name} is {' '.join(value)}, not {' '.join(arguments)}"
                )
    return problems


def check_environment(log: Log, settings: Settings) -> int:
    """Report everything that would keep the touchpad from behaving as asked."""
    problems: list[str] = []

    if not libinput_driver_installed():
        problems.append(
            "the libinput X driver is not installed, so the configuration file "
            "is never read; run: sudo apt-get install xserver-xorg-input-libinput"
        )

    session = session_type()
    if session == "wayland":
        log.note(
            "Wayland session: the file below is read only by X11, so these "
            "settings are not in force here."
        )
        log.note(wayland_hint(compositor_name()))
    elif session == "unknown":
        log.note("no X or Wayland session was detected; the file applies at the next login")

    problems += config_problems()
    problems += running_problems(settings)

    if xinput_path() is None:
        problems.append(
            "xinput is not installed, so the settings cannot be applied to the "
            "session that is running; run: sudo apt-get install xinput"
        )

    if problems:
        log.note(f"{len(problems)} problem(s) found:")
        for problem in problems:
            log.note(f"  {problem}")
    else:
        log.note("No problems found.")
    return len(problems)


# --- uninstalling ------------------------------------------------------------


def uninstall(log: Log, settings: Settings) -> None:
    """Remove everything this script wrote, and nothing else.

    A file this script replaced is put back from its backup; one it created
    where nothing was is removed, rather than left naming settings that are no
    longer written down anywhere. Both are done in Python when /etc allows it
    and through sudo when it does not, as the install is.
    """
    reset_running_session(log)

    backup = CONFIG_FILE.with_name(CONFIG_FILE.name + BACKUP_SUFFIX)
    if backup.is_file():
        try:
            shutil.move(str(backup), str(CONFIG_FILE))
        except OSError:
            if not run_as_root(["mv", str(backup), str(CONFIG_FILE)], CONFIG_FILE.parent):
                log.warn(f"could not restore {CONFIG_FILE}; put it back as root")
                return
        log.detail(f"restored {CONFIG_FILE} from its backup")
        return
    if CONFIG_FILE.exists():
        try:
            CONFIG_FILE.unlink()
        except OSError:
            if not run_as_root(["rm", "-f", str(CONFIG_FILE)], CONFIG_FILE.parent):
                log.warn(f"could not remove {CONFIG_FILE}; remove it as root")
                return
        log.detail(f"removed {CONFIG_FILE}")
        return
    log.detail(f"{CONFIG_FILE} was not installed")


# --- the Wayland answer ------------------------------------------------------


def wayland_hint(compositor: str) -> str:
    """What to run, by hand, for the compositor that was detected.

    Wayland gives the compositor the input devices and gives nobody else a way
    to configure them, so there is no file this script can write that a
    compositor would read. What it can do is name the command that works
    instead of leaving the user to find it.
    """
    if compositor in ("sway", "hyprland", "wayfire"):
        return (
            "  for a wlroots compositor, add to its configuration:\n"
            "    input type:touchpad {\n"
            "        tap enabled\n"
            "        tap_button_map lrm\n"
            "        natural_scroll disabled\n"
            "        dwt enabled\n"
            "        click_method clickfinger\n"
            "        scroll_method two_finger\n"
            "        accel_profile adaptive\n"
            "    }"
        )
    if compositor == "gnome":
        return (
            "  for GNOME:\n"
            "    gsettings set org.gnome.desktop.peripherals.touchpad tap-to-click true\n"
            "    gsettings set org.gnome.desktop.peripherals.touchpad disable-while-typing true\n"
            "    gsettings set org.gnome.desktop.peripherals.touchpad natural-scroll false"
        )
    if compositor == "kde":
        return (
            "  for KDE, use System Settings > Input Devices > Touchpad, which\n"
            "  writes kcminputrc and is the only supported way to set it there"
        )
    return (
        "  set it in the compositor's own configuration; Wayland has no shared\n"
        "  touchpad configuration file for a script to write"
    )


# --- entry point -------------------------------------------------------------


def settings_from_args(args: argparse.Namespace) -> Settings:
    """The settings, with the command line's overrides applied."""
    settings = Settings()
    if args.natural_scroll:
        settings = replace(settings, natural_scroll=True)
    if args.no_tap_to_click:
        settings = replace(settings, tap=False, tap_drag=False, tap_drag_lock=False)
    if args.speed is not None:
        settings = replace(settings, accel_speed=args.speed)
    if args.left_handed:
        settings = replace(settings, left_handed=True)
    if args.classic_scroll == "buttonareas":
        settings = replace(settings, click_method="buttonareas")
    return settings


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=SCRIPT_NAME,
        description=(
            "Configure a touchpad for the whole machine, independently of any "
            "window manager or desktop."
        ),
    )
    parser.add_argument(
        "--check", action="store_true", help="report problems, then stop"
    )
    parser.add_argument(
        "--uninstall", action="store_true", help="remove everything this script wrote"
    )
    parser.add_argument(
        "--natural-scroll",
        action="store_true",
        help="scroll content with the fingers, the way a phone does",
    )
    parser.add_argument(
        "--no-tap-to-click",
        action="store_true",
        help="use the physical buttons only, with no tapping at all",
    )
    parser.add_argument(
        "--left-handed",
        action="store_true",
        help="swap the left and right buttons",
    )
    parser.add_argument(
        "--button-areas",
        dest="classic_scroll",
        action="store_const",
        const="buttonareas",
        default="clickfinger",
        help="click with the bottom of the pad instead of with one finger",
    )
    parser.add_argument(
        "--speed",
        type=float,
        default=None,
        help=f"pointer speed, {Settings.SPEED_MIN} to {Settings.SPEED_MAX}",
    )
    parser.add_argument(
        "--no-packages",
        action="store_true",
        help="never run a package manager; print the command instead",
    )
    parser.add_argument(
        "--no-apply",
        action="store_true",
        help="write the configuration without touching the running session",
    )
    parser.add_argument("--quiet", action="store_true", help="only print problems")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.speed is not None and not (
        Settings.SPEED_MIN <= args.speed <= Settings.SPEED_MAX
    ):
        parser.error(
            f"--speed must be between {Settings.SPEED_MIN} and {Settings.SPEED_MAX}"
        )
    log = Log(quiet=args.quiet)
    settings = settings_from_args(args)

    if args.uninstall:
        log.step("Removing the touchpad configuration")
        uninstall(log, settings)
        log.note("")
        log.note("Removed. Log out and back in to be sure the running session")
        log.note("has no settings left from it.")
        return 0

    if args.check:
        log.step("Checking the touchpad configuration")
        return 1 if check_environment(log, settings) else 0

    log.step(f"Configuring the touchpad ({distro_description()})")
    ensure_tools(log, allow_packages=not args.no_packages)
    if not install_config(log, settings):
        return 1

    if not args.no_apply:
        log.step("Applying to the running session")
        apply_to_running_session(log, settings)

    log.step("Checking the result")
    problems = check_environment(log, settings)

    log.note("")
    if session_type() == "wayland":
        log.note("Done, for X11 sessions. On this Wayland session the settings")
        log.note("above have to be made in the compositor itself.")
    else:
        log.note("Done. The settings are in force now; the file makes them apply")
        log.note("again at every login, under any window manager or desktop.")
    log.note("")
    log.note(f"Configuration: {CONFIG_FILE}")
    log.note(f"Remove it with: {SCRIPT_NAME} --uninstall")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
