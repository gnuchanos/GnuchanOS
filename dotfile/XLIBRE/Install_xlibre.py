#!/usr/bin/env python3
"""GnuchanOS - install the XLibre X server from the xlibre-deb repository.

    python3 Install_xlibre.py             add the repository, install XLibre
    python3 Install_xlibre.py --status    report what is there, change nothing
    python3 Install_xlibre.py --revert    remove only the repository files

What XLibre is, and why it is not installed with apt alone
----------------------------------------------------------
XLibre is a fork of the X.Org server. It is not in Debian, so it comes from the
unofficial xlibre-deb repository - the one whose own install page this script
follows line for line: its signing key into /etc/apt/keyrings, a deb822 .sources
file into /etc/apt/sources.list.d, then the xlibre package itself. apt will not
trust a third-party repository without the key, and it will not read an
old-style one-line sources.list entry that carries its own Signed-By - so both
files are written here, in the shape the repository publishes.

Why a laptop, and Debian 12 in particular, needs the extra step
--------------------------------------------------------------
On Debian 12 (bookworm) the XLibre packages are built against a newer libdrm
than bookworm ships, so the repository's instructions also pull the libdrm
packages from bookworm-backports. On trixie and forky the libdrm in the base
distribution is already new enough and nothing extra is installed. The codename
is read out of /etc/os-release and only bookworm takes the second path - a
laptop on bookworm is exactly the machine this half exists for.

What it writes
--------------
  /etc/apt/keyrings/xlibre-deb.asc
      The repository's signing key. This is the file the Signed-By line below
      names; apt refuses a repository whose signature it cannot check.

  /etc/apt/sources.list.d/xlibre-deb.sources
      The repository itself, in the deb822 format apt wants for a third-party
      source that brings its own key.

  /etc/apt/sources.list.d/debian-backports.sources       (bookworm only)
      bookworm-backports, added so libdrm can be found above the base version.

Every file this script replaces is kept beside it as .gnuchan-backup and
--revert puts it back. Packages installed with apt are NOT removed by --revert:
another package may depend on them, and tearing a running X server out of a
machine is not something a reverted configuration file should do on its own.

License: GPL3
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

# --- the repository ---------------------------------------------------------

#: The unofficial XLibre repository, exactly as its install page gives it.
REPO_BASE = "https://xlibre-deb.github.io"
KEY_URL = f"{REPO_BASE}/key.asc"
REPO_URI = f"{REPO_BASE}/debian/"
REPO_COMPONENT = "main"

KEYRINGS_DIR = Path("/etc/apt/keyrings")
KEY_PATH = KEYRINGS_DIR / "xlibre-deb.asc"
SOURCES_FILE = Path("/etc/apt/sources.list.d/xlibre-deb.sources")
BACKPORTS_FILE = Path("/etc/apt/sources.list.d/debian-backports.sources")

#: The package that pulls in the whole XLibre server and its drivers.
XLIBRE_PACKAGE = "xlibre"

#: The two tools the install path calls before anything else.
BOOTSTRAP_PACKAGES = ("ca-certificates", "curl")

#: The one distribution whose XLibre build needs newer libdrm than it ships.
BACKPORTS_CODENAME = "bookworm"
BACKPORTS_URI = "http://deb.debian.org/debian"
BACKPORTS_SUITE = "bookworm-backports"
BACKPORTS_COMPONENT = "main"
DEBIAN_KEYRING = "/usr/share/keyrings/debian-archive-keyring.gpg"
#: The libdrm packages themselves, as the repository writes them (a glob, so
#: every libdrm package matching is upgraded together and they cannot drift).
BACKPORTS_PACKAGES = ("libdrm*",)

BACKUP_SUFFIX = ".gnuchan-backup"

MANAGED_FILES = (KEY_PATH, SOURCES_FILE, BACKPORTS_FILE)


# --- output -----------------------------------------------------------------


class Log:
    """Progress output; there is no quiet mode, a package install is not quiet."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- shell ------------------------------------------------------------------


def which(name: str) -> str | None:
    return shutil.which(name)


def tool(name: str) -> str:
    """The absolute path of a system tool, sbin first.

    apt and its helpers live under /usr/sbin as often as /usr/bin, and a normal
    user's PATH does not carry sbin; trying sbin first is what keeps a call from
    failing for a reason that has nothing to do with the machine's real state.
    """
    for directory in ("/usr/sbin", "/sbin", "/usr/bin", "/bin"):
        candidate = Path(directory, name)
        if candidate.is_file():
            return str(candidate)
    return name


def run(command: list[str], capture: bool = False,
        environment: dict | None = None) -> subprocess.CompletedProcess[str]:
    """Run one command; no shell, every command is a list.

    A command that is not installed returns 127 instead of raising, so every
    caller can look at returncode without a try around it.
    """
    try:
        if capture:
            return subprocess.run(command, check=False, capture_output=True,
                                  text=True, env=environment)
        return subprocess.run(command, check=False, text=True, env=environment)
    except FileNotFoundError:
        return subprocess.CompletedProcess(
            args=command, returncode=127, stdout="", stderr="command not found"
        )


def apt_environment() -> dict:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc/apt, /etc/apt/sources.list.d and apt all need root."""
    if is_root():
        return
    if os.environ.get("GNUCHAN_XLIBRE_ELEVATED") == "1":
        raise SystemExit("error: still not root after sudo")
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: this writes /etc/apt and installs packages; run it as root "
            "(su -c 'python3 Install_xlibre.py')"
        )
    log.step("This changes the system; re-running through sudo")
    environment = dict(os.environ)
    environment["GNUCHAN_XLIBRE_ELEVATED"] = "1"
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        environment,
    )


# --- the machine ------------------------------------------------------------


def os_release() -> dict[str, str]:
    """Parse /etc/os-release into a dictionary, empty when it is absent."""
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


def codename() -> str:
    """VERSION_CODENAME, which is the suite name the repository is keyed on."""
    release = os_release()
    return release.get("VERSION_CODENAME") or release.get("DEBIAN_CODENAME") or ""


def distro_description() -> str:
    release = os_release()
    return release.get("PRETTY_NAME") or release.get("NAME") or "unknown distribution"


def architecture() -> str:
    """dpkg's architecture, which is what the sources file's line must name."""
    result = run([tool("dpkg"), "--print-architecture"], capture=True)
    return result.stdout.strip() or "amd64"


def is_debian() -> bool:
    return which("apt-get") is not None


def needs_backports() -> bool:
    """Whether this distribution's XLibre build needs newer libdrm.

    Only bookworm does; on trixie and forky the base libdrm is new enough and
    the extra repository would be a second, unnecessary source to keep.
    """
    return codename() == BACKPORTS_CODENAME


# --- the files apt reads ----------------------------------------------------


def sources_text(suite: str, arch: str) -> str:
    """The deb822 source for XLibre, in the shape the repository publishes."""
    return (
        "Types: deb deb-src\n"
        f"URIs: {REPO_URI}\n"
        f"Suites: {suite}\n"
        f"Components: {REPO_COMPONENT}\n"
        f"Architectures: {arch}\n"
        f"Signed-By: {KEY_PATH}\n"
    )


def backports_text() -> str:
    """bookworm-backports, signed by Debian's own keyring."""
    return (
        "Types: deb deb-src\n"
        f"URIs: {BACKPORTS_URI}\n"
        f"Suites: {BACKPORTS_SUITE}\n"
        f"Components: {BACKPORTS_COMPONENT}\n"
        "Enabled: yes\n"
        f"Signed-By: {DEBIAN_KEYRING}\n"
    )


# --- file writing -----------------------------------------------------------


def backup_once(log: Log, path: Path) -> None:
    """Keep one copy of a file, taken before the first run that replaces it."""
    if not path.exists():
        return
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return
    shutil.copy2(path, backup)
    log.detail(f"backed up: {backup}")


def write_file(log: Log, path: Path, text: str, mode: int = 0o644) -> None:
    """Write a file, set its mode, keep one backup of what was there."""
    path.parent.mkdir(parents=True, exist_ok=True)
    backup_once(log, path)
    path.write_text(text, encoding="utf-8")
    path.chmod(mode)
    log.detail(f"wrote: {path}")


# --- apt --------------------------------------------------------------------


def package_installed(name: str) -> bool:
    return run([tool("dpkg"), "-s", name], capture=True).returncode == 0


def install_packages(log: Log, packages: tuple[str, ...],
                     release: str | None = None) -> bool:
    """Install packages with apt. `release` adds -t, for a backports install."""
    if not packages:
        return True
    command = [tool("apt-get"), "install", "-y", "--no-install-recommends"]
    if release is not None:
        command += ["-t", release]
    command += list(packages)
    log.step("Installing: " + " ".join(packages))
    if run(command, environment=apt_environment()).returncode != 0:
        log.warn("apt-get could not install: " + ", ".join(packages))
        return False
    log.detail("installed: " + ", ".join(packages))
    return True


def apt_update(log: Log) -> bool:
    log.step("apt-get update")
    result = run([tool("apt-get"), "update", "-o", "Acquire::Retries=3"],
                 capture=True, environment=apt_environment())
    if result.returncode != 0:
        log.warn("apt-get update failed; the repository was not read")
        for line in (result.stdout + result.stderr).splitlines()[-8:]:
            log.warn("  " + line)
        return False
    return True


# --- the repository ---------------------------------------------------------


def install_key(log: Log) -> bool:
    """Fetch the repository's signing key into /etc/apt/keyrings.

    curl is what the repository's own page uses, and it is the one tool that is
    certainly present once the bootstrap packages are in. The key is written
    world-readable, because apt reads it as the _apt user.
    """
    if which("curl") is None:
        log.warn("curl is not installed; it is needed to fetch the signing key")
        return False
    KEYRINGS_DIR.mkdir(parents=True, exist_ok=True)
    log.step("Fetching the XLibre signing key")
    result = run([tool("curl"), "-fsSL", KEY_URL, "-o", str(KEY_PATH)],
                 capture=True)
    if result.returncode != 0:
        log.warn("curl could not fetch the key: " + result.stderr.strip())
        return False
    KEY_PATH.chmod(0o644)
    log.detail(f"wrote: {KEY_PATH}")
    return True


def install_sources(log: Log) -> None:
    """Write the deb822 source for XLibre (and backports on bookworm)."""
    write_file(log, SOURCES_FILE, sources_text(codename(), architecture()))
    if needs_backports():
        log.detail("bookworm: adding bookworm-backports for the newer libdrm")
        write_file(log, BACKPORTS_FILE, backports_text())
    else:
        log.detail(f"{codename() or 'this'} release needs no backports for libdrm")


# --- install / status / revert ----------------------------------------------


class RepoState:
    """What this script has put on the machine."""

    def __init__(self) -> None:
        self.codename = codename()
        self.arch = architecture()
        self.key_present = KEY_PATH.is_file()
        self.sources_present = SOURCES_FILE.is_file()
        self.backports_present = BACKPORTS_FILE.is_file()
        self.xlibre_installed = package_installed(XLIBRE_PACKAGE)
        self.xorg_installed = package_installed("xserver-xorg-core")
        self.needs_backports = needs_backports()


def report(log: Log, state: RepoState) -> None:
    log.step(f"XLibre status ({distro_description()})")
    log.detail(f"codename          {state.codename or '(unknown)'}")
    log.detail(f"architecture      {state.arch}")
    log.detail(f"signing key       {'present' if state.key_present else 'MISSING'}")
    log.detail(f"repository file   {'present' if state.sources_present else 'missing'}")
    log.detail(
        f"backports file    {'present' if state.backports_present else 'not used'}"
        f"{' (bookworm needs it)' if state.needs_backports else ''}"
    )
    log.detail(f"xlibre package    {'installed' if state.xlibre_installed else 'NOT installed'}")
    log.detail(f"xorg server       {'installed' if state.xorg_installed else 'not installed'}")

    log.note("")
    if state.xlibre_installed:
        log.note("XLibre is installed.")
    elif state.sources_present and state.key_present:
        log.note("The repository is configured but XLibre is not installed.")
        log.note("Install it with:  sudo python3 Install_xlibre.py")
    else:
        log.note("XLibre is not set up. Run it with:")
        log.note("    sudo python3 Install_xlibre.py")


def install(log: Log, state: RepoState) -> int:
    report(log, state)

    missing = [p for p in BOOTSTRAP_PACKAGES if not package_installed(p)]
    if missing:
        if not apt_update(log):
            return 1
        install_packages(log, tuple(missing))

    if not install_key(log):
        return 1
    install_sources(log)

    if not apt_update(log):
        return 1

    ok = True

    # The newer libdrm first, so the xlibre packages can be satisfied against
    # it in the same run; on bookworm installing xlibre without this fails.
    if state.needs_backports:
        ok = install_packages(log, BACKPORTS_PACKAGES, release=BACKPORTS_SUITE) and ok

    ok = install_packages(log, (XLIBRE_PACKAGE,)) and ok

    final = RepoState()
    log.note("")
    if final.xlibre_installed:
        log.note("XLibre is installed.")
        log.note("Restart the X session (or the machine) for it to take over from")
        log.note("the X.Org server that was running before this.")
    elif ok:
        log.note("The repository is configured. If the install still did not")
        log.note("finish, run `sudo apt-get install xlibre` for the exact error.")
    else:
        log.note("The install did not complete. `--status` shows what is in place;")
        log.note("the apt output above names the reason.")
    log.note("")
    log.note(f"Repository file: {SOURCES_FILE}")
    log.note(f"Remove it with:  python3 Install_xlibre.py --revert")
    return 0 if final.xlibre_installed else 1


def revert(log: Log) -> int:
    """Remove the files this script wrote, restoring any backups.

    Packages are left installed on purpose: another package may depend on them,
    and a reverted *.sources file does not warrant uninstalling a running X
    server. This only takes back the configuration.
    """
    log.step("Removing the XLibre repository files")
    for path in MANAGED_FILES:
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if backup.exists():
            shutil.copy2(backup, path)
            backup.unlink()
            log.detail(f"{path} -> restored from its backup")
        elif path.exists():
            path.unlink()
            log.detail(f"removed: {path}")
        else:
            log.detail(f"not present: {path}")
    apt_update(log)
    log.note("")
    log.note("The repository is gone. The installed packages remain; remove them")
    log.note("with apt if you no longer want them.")
    return 0


# --- entry point ------------------------------------------------------------


def usage() -> None:
    print(
        "usage: python3 Install_xlibre.py [--status | --revert]\n"
        "\n"
        "  (no argument)   add the repository and install XLibre\n"
        "  --status        report what is in place, change nothing\n"
        "  --revert        remove the repository files this script wrote\n"
    )


def main() -> int:
    log = Log()
    arguments = sys.argv[1:]

    if not is_debian():
        raise SystemExit(
            "error: this installs with apt and writes Debian's apt directories; "
            "it is Debian-only by design"
        )

    if "--help" in arguments or "-h" in arguments:
        usage()
        return 0
    if "--status" in arguments:
        report(log, RepoState())
        return 0
    if "--revert" in arguments:
        ensure_root(log)
        return revert(log)
    if arguments:
        usage()
        return 2

    ensure_root(log)
    return install(log, RepoState())


if __name__ == "__main__":
    raise SystemExit(main())
