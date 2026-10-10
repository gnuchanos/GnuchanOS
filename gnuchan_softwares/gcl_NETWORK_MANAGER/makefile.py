#!/usr/bin/env python3
# =============================================================================
# GnuChanNetworkManager - build and install the network manager (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# Builds the manager and installs it to /usr/local/bin, with its settings file
# put where it will be read from — ~/.config/GnuChanNetworkManager/config.py. It
# needs root for the install and re-runs itself through sudo, exactly as the
# wifi manager's own makefile does.
#
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py list       compile and print the interfaces and DNS
#     python3 makefile.py uninstall  remove the binary
#
# Debian only, on purpose: it installs with apt-get and writes /usr/local.
#
# It is a separate program from the wifi manager and installs on its own. Where
# the wifi manager JOINS a wireless network, this one shows every interface and
# does the general work — DNS, up/down — and opens the wifi manager for the
# wireless one. The two are installed separately and neither needs the other to
# build.
#
# The manager talks to the system through nmcli (NetworkManager) and ip (the
# kernel), which come with the system, and through two things it DRIVES at run
# time: resolvectl (systemd-resolved), for the DNS panel, and zapret, for the
# DPI bypass. Neither is linked against, so neither is a build dependency; both
# are installed here, at install time, so that one `python3 makefile.py` leaves
# the machine able to do everything the manager offers. A failure installing
# either is reported and the install carries on — the manager still opens and
# shows every interface.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent
# Build output under _temp/, not in the source tree: a build is not a change to
# the project, and the manager's directory holds only sources.
BUILD = ROOT.parent.parent / "_temp" / "gnuchannetworkmanager-build"

PROGRAM = "GnuChanNetworkManager"
BIN_DIR = Path("/usr/local/bin")

# The desktop entry, which is what makes the manager FINDABLE. GnuChanRunner and
# any applications menu read the .desktop files under /usr/share/applications,
# and a program with none is one the launcher does not show.
APPS_DIR = Path("/usr/share/applications")
DESKTOP_FILE = APPS_DIR / "gnuchannetworkmanager.desktop"

# The settings file, installed into the user's own config directory.
CONFIG_SOURCE = ROOT / "GnuChanNetworkManager_config" / "config.py"
CONFIG_DIR_NAME = "GnuChanNetworkManager"
CONFIG_FILE_NAME = "config.py"

SOURCES = (
    "net_shell.c",
    "net_device.c",
    "net_dns.c",
    "net_dpi.c",
    "net_login.c",
    "net_privilege.c",
    "net_config.c",
    "net_style.c",
    "net_draw.c",
    "net_ui.c",
    "net_manager.c",
)
HEADERS = (
    "net_shell.h",
    "net_device.h",
    "net_dns.h",
    "net_dpi.h",
    "net_login.h",
    "net_privilege.h",
    "net_config.h",
    "net_style.h",
    "net_draw.h",
    "net_ui.h",
)

ELEVATED_VARIABLE = "GNUCHANNET_ELEVATED"

# zapret — the DPI bypass the manager's DPI button switches on and off. It is
# not linked against; like systemd-resolved it is installed at install time so
# the button has something to drive. The version is pinned so two machines get
# the same thing, and the paths are the ones net_dpi.c falls back to.
ZAPRET_VERSION = "72.13"
ZAPRET_DIR = Path("/opt/zapret")
ZAPRET_INIT = ZAPRET_DIR / "init.d" / "sysv" / "zapret"
ZAPRET_TARBALL = (
    f"https://github.com/bol-van/zapret/releases/download/"
    f"v{ZAPRET_VERSION}/zapret-v{ZAPRET_VERSION}.tar.gz"
)


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(
    command: list[str],
    capture: bool = False,
    environment: dict[str, str] | None = None,
    cwd: Path | None = None,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True,
            env=environment, cwd=cwd, input=input_text,
        )
    return subprocess.run(command, check=False, text=True, env=environment,
                          cwd=cwd, input=input_text)


def apt_environment() -> dict[str, str]:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root() -> None:
    if is_root():
        return
    if os.environ.get(ELEVATED_VARIABLE) == "1":
        raise SystemExit("error: still not root after sudo")
    sudo = shutil.which("sudo")
    if sudo is None:
        raise SystemExit("error: this installs into /usr/local; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable, str(Path(__file__).resolve())],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed. The check is a real compile of
    a one-line translation unit rather than a test for a file on disk, because
    where a distribution keeps a header is not fixed and what matters is whether
    the compiler the build will use can find it. Xft pulls freetype in through
    its own header, so freetype's include directory is passed for it.
    """
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(
        command, input=f"#include <{header}>\n",
        text=True, capture_output=True, check=False,
    )
    return result.returncode == 0


def x11_headers_present() -> bool:
    return header_present("X11/Xlib.h")


def xft_headers_present() -> bool:
    return header_present("X11/Xft/Xft.h", ("/usr/include/freetype2",))


def apt_install(packages: tuple[str, ...]) -> bool:
    if not packages:
        return True
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *packages]
    detail("running: " + " ".join(command))
    return run(command, environment=apt_environment()).returncode == 0


def ensure_build_dependencies() -> None:
    needed: list[str] = []
    if not x11_headers_present():
        needed.append("libx11-dev")
    if not xft_headers_present():
        needed.append("libxft-dev")
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not needed:
        return
    step("Installing the build dependencies")
    detail("missing: " + ", ".join(needed))
    if not apt_install(tuple(needed)):
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def resolvectl_present() -> bool:
    return shutil.which("resolvectl") is not None


def ensure_runtime_dependency() -> None:
    """Install what the manager DRIVES at run time — not what it is built from.

    Every DNS action this manager takes goes through resolvectl, which is
    systemd-resolved's own tool: setting the servers, reverting to automatic,
    and turning on the encrypted DNS that gets a query past a network which
    filters by reading the name inside a port-53 packet. A machine without it
    can still SHOW the interfaces but cannot change the resolver at all, and the
    person is left to discover and install it by hand — which is not the
    installer's job to push onto them.

    It is not a build dependency: nothing here links against it, so it is not in
    ensure_build_dependencies(). It is installed here, at install time, so that
    running `python3 makefile.py` once leaves the machine able to do everything
    the manager offers.
    """
    if resolvectl_present():
        return
    step("Installing systemd-resolved (the resolver the manager drives)")
    if not apt_install(("systemd-resolved",)):
        # Not fatal: the manager still opens and shows the interfaces. But the
        # person is told plainly, because the DNS buttons will refuse otherwise.
        detail("could not install systemd-resolved; the DNS buttons will "
               "report that resolvectl is missing")
        return
    detail("installed systemd-resolved")
    # The package ships the service; enabling and starting it is what actually
    # puts resolvectl's socket in place, so it is done here rather than left to
    # a reboot.
    run(["systemctl", "enable", "--now", "systemd-resolved"])
    # Point /etc/resolv.conf at the stub resolver so the rest of the system
    # (ping, dig, and every program on getaddrinfo) asks systemd-resolved, which
    # is where the manager's settings and encrypted DNS take effect. The link is
    # made only when the file is a plain file or already the stub link, so a
    # machine whose resolv.conf is managed some other way is left as it is.
    stub = "/run/systemd/resolve/stub-resolv.conf"
    conf = "/etc/resolv.conf"
    if Path(stub).exists():
        try:
            current = os.path.realpath(conf)
        except OSError:
            current = ""
        if current != stub:
            try:
                os.replace(conf, conf + ".gnuchan-backup")
            except OSError:
                pass
            try:
                os.symlink(stub, conf)
                detail(f"pointed {conf} at {stub}")
            except OSError:
                detail(f"could not point {conf} at {stub}; set it by hand")


def dpi_installed() -> bool:
    """Whether zapret is already in place, asked the same way the manager's own
    net_dpi_available() asks: the init script the DPI button runs is there."""
    return ZAPRET_INIT.is_file()


def download_zapret(destination: Path) -> bool:
    """Fetch zapret's release tarball to `destination`. urllib is used rather
    than wget/curl so this does not depend on a downloader being installed
    before it can install one. Returns whether the file arrived."""
    try:
        urllib.request.urlretrieve(ZAPRET_TARBALL, destination)
    except Exception as exc:  # URLError, HTTPError, timeouts, ...
        detail(f"could not download zapret: {exc}")
        return False
    return destination.is_file() and destination.stat().st_size > 0


def install_zapret_units() -> None:
    """Make sure systemd knows the service, so the init script the DPI button
    runs is wired into startup. zapret's easy installer normally does this; it
    is repeated here because it is cheap and a machine where that step was
    skipped would leave the button with nothing to start."""
    source = ZAPRET_DIR / "init.d" / "systemd" / "zapret.service"
    target = Path("/etc/systemd/system/zapret.service")
    if not source.is_file() or target.exists():
        return
    try:
        shutil.copyfile(source, target)
    except OSError as exc:
        detail(f"could not install the zapret service: {exc}")
        return
    run(["systemctl", "daemon-reload"])
    run(["systemctl", "enable", "zapret"])


def ensure_dpi_dependency() -> None:
    """Install zapret — the DPI bypass the manager's DPI button switches.

    The DNS half of this manager drives resolvectl, so
    ensure_runtime_dependency() installs systemd-resolved. This is the same
    idea for the DPI half: the button runs zapret's own init script (see
    net_dpi.c), so zapret has to be present for the button to do anything. A
    machine without it draws the button as unavailable and says why; installing
    it here is what makes the feature work from one `python3 makefile.py`.

    zapret comes from its official release at the pinned version, is unpacked
    to /opt/zapret — the path net_dpi.c expects — and is set up with its OWN
    easy installer, fed non-interactively, so the desync strategy is the one
    zapret's blockcheck chooses for this network rather than one baked in here.
    A failure at any step is reported and the rest of the install carries on:
    the manager still opens and shows every interface.
    """
    if dpi_installed():
        return

    step("Installing zapret (the DPI bypass the DPI button switches)")
    # What zapret's nfqws needs to sit in the packet path, plus the tools its
    # installer itself calls.
    apt_install(("nftables", "iptables", "curl", "wget", "tar", "gzip", "jq"))
    # What building it needs, in case the release's prebuilt binaries do not
    # match this kernel and zapret falls back to compiling.
    apt_install(("make", "gcc", "zlib1g-dev", "libcap-dev",
                 "libnetfilter-queue-dev", "libmnl-dev", "libsystemd-dev"))

    work = Path(tempfile.mkdtemp(prefix="gnuchan-zapret-"))
    tarball = work / f"zapret-v{ZAPRET_VERSION}.tar.gz"
    try:
        if not download_zapret(tarball):
            detail("zapret was not installed; the DPI button will report that "
                   "no bypass is installed until it is")
            return
        with tarfile.open(tarball) as archive:
            archive.extractall(work)
        source = work / f"zapret-v{ZAPRET_VERSION}"
        if not source.is_dir():
            detail("the zapret archive did not contain what was expected")
            return
        if ZAPRET_DIR.exists():
            shutil.rmtree(ZAPRET_DIR)
        shutil.copytree(source, ZAPRET_DIR)
    except (OSError, tarfile.TarError) as exc:
        detail(f"could not unpack zapret: {exc}")
        return
    finally:
        shutil.rmtree(work, ignore_errors=True)

    # The easy installer does the real setup: it places the binaries, works out
    # the firewall type, and runs blockcheck to choose a desync that gets past
    # this network. It is a prompt-driven script, so answers are fed the way its
    # own one-paste install lines feed them — accept the defaults, enable nfqws.
    # Run from /opt/zapret so it does not try to copy itself anywhere.
    answers = "\n\n\n4\n\n\nY\n\n\n\n\n\n"
    subprocess.run(["sh", str(ZAPRET_DIR / "install_prereq.sh")],
                   input="\n\n", text=True, check=False, cwd=ZAPRET_DIR)
    subprocess.run(["sh", str(ZAPRET_DIR / "install_bin.sh")],
                   text=True, check=False, cwd=ZAPRET_DIR)
    run(["sh", str(ZAPRET_DIR / "install_easy.sh")], input_text=answers,
        cwd=ZAPRET_DIR)

    # A belt for the braces above: if the easy installer did not get as far as
    # registering the service, register it so the init script the button runs is
    # wired into systemd.
    install_zapret_units()

    if dpi_installed():
        detail("installed zapret under /opt/zapret")
    else:
        detail("zapret was downloaded but not set up; the DPI button will "
               "report that no bypass is installed until it is")


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the X libraries the manager uses.

    x11 is needed for everything and xft for the text, and the two are asked for
    together because Xft's own header includes freetype's: pkg-config is what
    knows where that lives. The fallback names the two libraries and freetype's
    include directory directly, for a machine with no pkg-config.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", "x11", "xft"], capture=True)
        libs = run([pkg_config, "--libs", "x11", "xft"], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    return (["-I/usr/include", "-I/usr/include/freetype2"],
            ["-lX11", "-lXft"])


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS) if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    cflags, libs = x11_flags()
    command = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-O2", "-D_DEFAULT_SOURCE",
        "-I", str(ROOT),
        *cflags,
    ]
    command += [str(ROOT / name) for name in SOURCES]
    command += ["-o", str(output)]
    command += libs

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def desktop_entry() -> str:
    """The .desktop file, written the freedesktop way.

    Exec is the absolute path to the installed binary. Terminal=false because
    the manager is a window and asks for its own password; Categories puts it
    under System in a menu. NoDisplay is NOT set: this is exactly the entry that
    is meant to be seen.
    """
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        f"Name={PROGRAM}",
        "Comment=GnuchanOS network manager",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "Categories=System;Network;",
        "Icon=network-workgroup",
        "",
    ])


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    APPS_DIR.mkdir(parents=True, exist_ok=True)

    # The manager this install is replacing may be running — it is a program,
    # and the one open in another terminal is the one being replaced. Writing to
    # its path would fail with ETXTBSY, so the new binary is written beside it
    # and renamed over it.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
    detail(f"installed {BIN_DIR / PROGRAM}")

    temporary = BUILD / "gnuchannetworkmanager.desktop.new"
    temporary.write_text(desktop_entry(), encoding="utf-8")
    os.replace(str(temporary), str(DESKTOP_FILE))
    DESKTOP_FILE.chmod(0o644)
    detail(f"installed {DESKTOP_FILE}")


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root. The settings file
    belongs to the person who logs in, and the manager reads it from their home.
    Under sudo, HOME and the ids are root's, so the original user is read back
    from SUDO_USER.
    """
    name = os.environ.get("SUDO_USER")
    if not name:
        return None
    try:
        import pwd
        info = pwd.getpwnam(name)
        return Path(info.pw_dir), info.pw_uid, info.pw_gid
    except (ImportError, KeyError):
        return None


def install_config() -> None:
    """Put the settings file where the manager will read it.

    A settings file the user has edited is theirs to keep — the manager reads
    the file live — so an existing one is left alone and only a missing one is
    written.
    """
    if not CONFIG_SOURCE.is_file():
        return
    user = invoking_user()
    if user is None:
        detail("skipped the config: no login user to install it for")
        return

    home, uid, gid = user
    directory = home / ".config" / CONFIG_DIR_NAME
    target = directory / CONFIG_FILE_NAME

    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the manager would have "
            f"nothing to read"
        )

    directory.mkdir(parents=True, exist_ok=True)

    if target.exists():
        detail(f"kept the existing {target}")
        try:
            os.chown(directory, uid, gid)
        except OSError:
            pass
        return

    shutil.copyfile(CONFIG_SOURCE, target)
    try:
        os.chown(directory, uid, gid)
        os.chown(target, uid, gid)
    except OSError:
        pass

    detail(f"installed {target}")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, DESKTOP_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")
    note("The settings file under ~/.config/GnuChanNetworkManager/ was left "
         "alone.")


def run_program(binary: Path, arguments: list[str]) -> int:
    if not os.environ.get("DISPLAY") and not arguments:
        raise SystemExit("error: DISPLAY is not set, so there is no X server "
                         "to open on")
    return run([str(binary), *arguments], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanNetworkManager is a Debian X11 program; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes /usr/local; it is "
            "Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "list", "uninstall")
    if action not in known:
        print(f"error: unknown action '{action}'", file=sys.stderr)
        print("usage: python3 makefile.py [" + "|".join(known) + "]",
              file=sys.stderr)
        return 2

    if action in ("install", "uninstall"):
        ensure_root()

    if action == "uninstall":
        uninstall()
        return 0

    if action == "build":
        binary = build()
        note(f"Built {binary}.")
        return 0

    if action == "install":
        ensure_build_dependencies()
        binary = build()
        ensure_runtime_dependency()
        ensure_dpi_dependency()
        install(binary)
        install_config()
        note("")
        note("GnuChanNetworkManager is installed. Run GnuChanNetworkManager, or")
        note("bind it in the window manager's settings script.")
        return 0

    if action == "run":
        return run_program(build(), [])

    if action == "list":
        return run_program(build(), ["--list"])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
