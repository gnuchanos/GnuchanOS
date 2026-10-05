#!/usr/bin/env python3
"""GnuchanOS - install the XLibre X server from the xlibre-debian repository.

    python3 Install_xlibre.py             add the repository, install XLibre
    python3 Install_xlibre.py --status    report what is there, change nothing
    python3 Install_xlibre.py --revert    remove only the repository files

What XLibre is, and why it is not installed with apt alone
----------------------------------------------------------
XLibre is a fork of the X.Org server. It is not in Debian, so it comes from the
unofficial xlibre-debian repository - the one at
https://github.com/xlibre-debian/debian, whose README this script follows. That
page installs it in three parts: the repository's signing key dearmored into
/usr/share/keyrings, a deb822 .sources file into /etc/apt/sources.list.d, then
the xlibre packages themselves. apt will not trust a third-party repository
without the key, so the key is fetched and dearmored here in the shape the
repository publishes it.

The repository's own shape
--------------------------
The repository keeps ONE suite, named "main", and publishes a COMPONENT per
Debian release channel: `stable` for Debian Stable and `testing` for Debian
Testing. A machine therefore reads `Components: stable` or
`Components: testing` according to the release it runs, and the codename in
/etc/os-release decides which. The codenames are named in CODENAME_COMPONENT;
a codename that is not there falls back to the stable component, which is what
most machines run, and the run says so.

Why a Stable machine needs the extra step
-----------------------------------------
The repository's README opens with a warning: on Debian Stable the backports
suite MUST be enabled, or the XLibre packages cannot be satisfied. The script
therefore adds <codename>-backports on the stable channel and lets apt resolve
the install against it; on Debian Testing the base distribution is already new
enough and nothing extra is added. The release channel is what is asked, and
only the stable one takes the second path.

What it writes
--------------
  /usr/share/keyrings/NexusSfan.pgp
      The repository's signing key, dearmored. This is the file the Signed-By
      line below names; apt refuses a repository whose signature it cannot
      check.

  /etc/apt/sources.list.d/xlibre-debian.sources
      The repository itself, in the deb822 format apt wants for a third-party
      source that brings its own key.

  /etc/apt/sources.list.d/debian-backports.sources       (Stable only)
      <codename>-backports, added so apt can find the newer libraries XLibre
      needs above the versions the base release ships.

Every file this script replaces is kept beside it as .gnuchan-backup and
--revert puts it back. Packages installed with apt are NOT removed by --revert:
another package may depend on them, and tearing a running X server out of a
machine is not something a reverted configuration file should do on its own.
The output names the packages to remove by hand if that is what is wanted.

License: GPL3
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

# --- the repository ---------------------------------------------------------

#: The unofficial XLibre repository, as its README gives it. It keeps a single
#: suite named "main"; the Debian release channel is a COMPONENT of that suite.
REPO_BASE = "https://xlibre-debian.github.io/debian/"
REPO_SUITE = "main"

#: The two components the repository publishes, one per release channel.
COMPONENT_STABLE = "stable"
COMPONENT_TESTING = "testing"

#: Which component a release codename belongs to. Debian Stable's codename and
#: Debian Testing's are the two named here; a codename that is not in this table
#: is assumed to be a stable one, which is what the great majority of machines
#: run, and the run warns that it assumed so.
CODENAME_COMPONENT = {
    "trixie": COMPONENT_STABLE,
    "forky": COMPONENT_TESTING,
}

#: The signing key: fetched ASCII-armored from the address the README names, and
#: dearmored with gpg into the keyring file. apt reads the keyring and not the
#: armored text, and the path is the one the README's Signed-By line names.
KEY_URL = "https://mrchicken.nexussfan.cz/publickey.asc"
KEY_PATH = Path("/usr/share/keyrings/NexusSfan.pgp")
SOURCES_FILE = Path("/etc/apt/sources.list.d/xlibre-debian.sources")

#: The package that pulls in the whole XLibre server and its drivers, and the
#: package that brings XLibre's own archive keyring alongside it as the README
#: installs them.
XLIBRE_PACKAGE = "xlibre"
XLIBRE_PACKAGES = ("xlibre", "xlibre-archive-keyring")

#: The tools the install path calls before anything else. gnupg is here because
#: the signing key is ASCII-armored and gpg --dearmor is what turns it into the
#: binary keyring apt reads; curl is what fetches it.
BOOTSTRAP_PACKAGES = ("ca-certificates", "curl", "gnupg")

#: The repository's README requires backports on Debian Stable, so the stable
#: channel also enables <codename>-backports; Debian Testing does not need it.
BACKPORTS_FILE = Path("/etc/apt/sources.list.d/debian-backports.sources")
BACKPORTS_URI = "http://deb.debian.org/debian"
BACKPORTS_COMPONENT = "main"
DEBIAN_KEYRING = "/usr/share/keyrings/debian-archive-keyring.gpg"

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
    """VERSION_CODENAME, which is the release the repository is keyed on."""
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


def component() -> str:
    """The repository component this release reads: stable or testing.

    A codename the table does not know is taken to be a stable one, which is
    what most machines run; report() and install() say when that assumption was
    made so it is never silent.
    """
    return CODENAME_COMPONENT.get(codename(), COMPONENT_STABLE)


def known_channel() -> bool:
    """Whether this release's codename is one the repository names."""
    return codename() in CODENAME_COMPONENT


def needs_backports() -> bool:
    """Whether this release needs the backports suite the README requires.

    Only the stable channel does; on testing the base distribution is new enough
    and the extra repository would be a second, unnecessary source to keep. A
    codename the table does not name cannot name a backports suite, so it is
    left alone - only a recognised stable codename enables it.
    """
    return known_channel() and component() == COMPONENT_STABLE


def backports_suite() -> str:
    """The backports suite for this release, e.g. "trixie-backports"."""
    return f"{codename()}-backports"


# --- the files apt reads ----------------------------------------------------


def sources_text(release_component: str, arch: str) -> str:
    """The deb822 source for XLibre, in the shape the repository publishes."""
    return (
        "Types: deb\n"
        f"URIs: {REPO_BASE}\n"
        f"Suites: {REPO_SUITE}\n"
        f"Components: {release_component}\n"
        f"Architectures: {arch}\n"
        f"Signed-By: {KEY_PATH}\n"
    )


def backports_text(suite: str) -> str:
    """<codename>-backports, signed by Debian's own keyring."""
    return (
        "Types: deb\n"
        f"URIs: {BACKPORTS_URI}\n"
        f"Suites: {suite}\n"
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


def install_packages(log: Log, packages: tuple[str, ...]) -> bool:
    """Install packages with apt."""
    if not packages:
        return True
    command = [tool("apt-get"), "install", "-y", "--no-install-recommends"]
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
    """Fetch the repository's signing key and dearmor it into the keyrings dir.

    The repository publishes the key ASCII-armored and apt wants the binary
    keyring, so gpg --dearmor is what the repository's own page pipes through.
    curl writes the armored key to a file beside the target, gpg turns it into
    the keyring, and the result is moved into place in one step - the ring apt
    is reading is never caught half-written. The key is world-readable, because
    apt reads it as the _apt user.
    """
    if which("curl") is None:
        log.warn("curl is not installed; it is needed to fetch the signing key")
        return False
    if which("gpg") is None:
        log.warn("gpg is not installed; it is needed to dearmor the signing key")
        return False

    keyrings = KEY_PATH.parent
    keyrings.mkdir(parents=True, exist_ok=True)
    armored = keyrings / (KEY_PATH.name + ".asc")
    dearmored = keyrings / (KEY_PATH.name + ".new")

    log.step("Fetching the XLibre signing key")
    result = run([tool("curl"), "-fsSL", KEY_URL, "-o", str(armored)],
                 capture=True)
    if result.returncode != 0:
        log.warn("curl could not fetch the key: " + result.stderr.strip())
        return False

    result = run([tool("gpg"), "--dearmor", "--output", str(dearmored),
                  str(armored)], capture=True)
    armored.unlink(missing_ok=True)
    if result.returncode != 0:
        log.warn("gpg could not dearmor the key: " + result.stderr.strip())
        return False

    os.replace(dearmored, KEY_PATH)
    KEY_PATH.chmod(0o644)
    log.detail(f"wrote: {KEY_PATH}")
    return True


def install_sources(log: Log) -> None:
    """Write the deb822 source for XLibre (and backports on the stable channel)."""
    release_component = component()
    write_file(log, SOURCES_FILE, sources_text(release_component, architecture()))
    log.detail(f"component: {release_component} (for {codename() or 'this release'})")

    if needs_backports():
        log.detail(f"stable: adding {backports_suite()} for the newer libraries")
        write_file(log, BACKPORTS_FILE, backports_text(backports_suite()))
    else:
        log.detail("this release needs no backports")


# --- install / status / revert ----------------------------------------------


class RepoState:
    """What this script has put on the machine."""

    def __init__(self) -> None:
        self.codename = codename()
        self.arch = architecture()
        self.component = component()
        self.known_channel = known_channel()
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
    log.detail(
        f"component         {state.component}"
        f"{'' if state.known_channel else ' (assumed; this codename is not one the repository names)'}"
    )
    log.detail(f"signing key       {'present' if state.key_present else 'MISSING'}")
    log.detail(f"repository file   {'present' if state.sources_present else 'missing'}")
    log.detail(
        f"backports file    {'present' if state.backports_present else 'not used'}"
        f"{' (stable needs it)' if state.needs_backports else ''}"
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

    # On the stable channel backports is already enabled above, so apt resolves
    # the install against it and takes the newer libraries xlibre needs from
    # there; installing xlibre alone is what the repository's README does.
    ok = install_packages(log, XLIBRE_PACKAGES)

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
    server. This only takes back the configuration, and names the packages the
    repository's README removes by hand if that is wanted.
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
    log.note("The repository is gone. The installed packages remain; to return to")
    log.note("the X.Org server by hand, the repository's README has:")
    log.note("")
    log.note("    sudo apt-get remove xlibre xserver-xlibre\\* nexussfan-archive-keyring")
    log.note("    sudo apt-get install xorg")
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
