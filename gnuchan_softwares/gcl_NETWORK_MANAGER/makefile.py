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
# wireless one.
#
# Besides the manager itself this installs the two things the manager DRIVES at
# run time, the way the well-known zapret one-paste installers do, so that ONE
# `python3 makefile.py` leaves a machine able to reach a site its network
# blocks:
#
#   * an ENCRYPTED resolver (DNS-over-TLS, through the systemd-resolved this
#     manager already drives), because the most common block is not DPI at all
#     but plain DNS rewriting — a name is answered with the ISP's own address
#     and every connection to it fails its certificate check. Nothing zapret
#     does can fix that; the answer arrived over port 53 before any TLS.
#     Encrypting the query is what fixes it.
#
#   * zapret (nfqws), the DPI desynchroniser, for the case where the block is
#     the TLS name read inside a TCP packet rather than the DNS answer. Its
#     desync is chosen by ITS OWN blockcheck for the network at hand and written
#     into its config, because one ISP's working method is another's no-op.
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
import threading
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

# --- encrypted DNS, the primary fix ---------------------------------------
#
# The most common way a network "blocks Discord" is not any kind of DPI: it
# answers the NAME with its own address, so every program connects to the ISP
# and fails the certificate check ("UnknownIssuer", an ISP hostname in the
# ping). zapret cannot help — it fragments TCP payloads, and the lie arrived in
# the DNS answer over UDP 53 before any TLS began. A query that leaves over an
# encrypted channel cannot be read, so it cannot be answered with a lie.
#
# systemd-resolved is already the resolver this manager drives (see net_dns.c)
# and it speaks DNS-over-TLS itself, so the encrypted resolver is a drop-in
# rather than a second daemon: one file, one restart, and the whole system —
# the manager, ping, the browser — asks it. The address#name form is what
# resolved checks the server's certificate against and what turns on SNI, so
# the TLS is real and not merely opportunistic.
RESOLVED_DROPIN = Path("/etc/systemd/resolved.conf.d/gnuchan-encrypted-dns.conf")
ENCRYPTED_DNS_SERVERS = ("1.1.1.1#cloudflare-dns.com",
                         "1.0.0.1#cloudflare-dns.com",
                         "2606:4700:4700::1111#cloudflare-dns.com")

# --- zapret, the DPI fix ---------------------------------------------------
ZAPRET_VERSION = "72.13"
ZAPRET_DIR = Path("/opt/zapret")
ZAPRET_INIT = ZAPRET_DIR / "init.d" / "sysv" / "zapret"
ZAPRET_TARBALL = (
    f"https://github.com/bol-van/zapret/releases/download/"
    f"v{ZAPRET_VERSION}/zapret-v{ZAPRET_VERSION}.tar.gz"
)

# The names blockcheck is run against to find a working bypass. updates.discord
# is first because it is the host the Discord installer actually fetches from,
# and the one observed being reset HERE: its name resolves correctly but the TLS
# handshake is cut, which is exactly the case a DPI desync is for.
BLOCKCHECK_DOMAINS = ("updates.discord.com", "discord.com", "instagram.com",
                      "youtube.com", "x.com", "tiktok.com")
# Used when blockcheck finds no nfqws method at all. The reference's own
# --dev default, and a desync that works on many networks.
FALLBACK_NFQWS_OPT = "--dpi-desync=fake --dpi-desync-ttl=3"

# blockcheck works through dozens of desync methods against the network, so it
# runs for minutes. This ceiling stops it ever sitting there forever, and its
# output is streamed (see run_streaming) so a person watching can tell it is
# working rather than frozen.
BLOCKCHECK_TIMEOUT = 1200


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
    """Install what the manager DRIVES for its DNS panel — not what it is built
    from. Every DNS action in the window goes through resolvectl, which is
    systemd-resolved's own tool: setting the servers, reverting to automatic,
    and the encrypted-DNS toggle. A machine without it can still SHOW the
    interfaces but cannot change the resolver, so it is installed here.
    """
    if resolvectl_present():
        return
    step("Installing systemd-resolved (the resolver the DNS panel drives)")
    if not apt_install(("systemd-resolved",)):
        detail("could not install systemd-resolved; the DNS panel will report "
               "that resolvectl is missing")
        return
    detail("installed systemd-resolved")
    run(["systemctl", "enable", "--now", "systemd-resolved"])


# --- encrypted DNS ---------------------------------------------------------

def point_resolv_conf_at_stub() -> None:
    """Point /etc/resolv.conf at systemd-resolved's stub, so every program —
    ping, the browser, the manager's own reader — asks the resolver that now
    holds the encrypted-DNS setting. A resolv.conf that is already the stub link
    is left alone; anything else (a plain file shipped by the distribution, most
    often) is replaced by the link. Without this the setting would apply to
    resolved but nothing would be asking resolved."""
    stub = "/run/systemd/resolve/stub-resolv.conf"
    conf = Path("/etc/resolv.conf")
    if not Path(stub).exists():
        detail(f"{stub} is missing, so {conf} is left as it is")
        return
    try:
        if conf.is_symlink() and os.path.realpath(conf) == stub:
            return
        if conf.exists() and not conf.is_symlink():
            shutil.copyfile(conf, str(conf) + ".gnuchan-backup")
        if conf.is_symlink() or conf.exists():
            conf.unlink()
        os.symlink(stub, conf)
        detail(f"pointed {conf} at {stub}")
    except OSError as exc:
        detail(f"could not point {conf} at the stub: {exc}")


def ensure_encrypted_dns() -> None:
    """Encrypt the machine's DNS with DNS-over-TLS.

    The reference installer's FIRST step, and the fix for the failure where a
    name resolves to the ISP's own address and every connection to it fails its
    certificate check. A query that leaves encrypted cannot be read, so it
    cannot be answered with a lie; zapret does nothing for this case, because
    the lie arrived over port 53 before any TLS.

    It is done through systemd-resolved — already the resolver the manager
    drives — rather than a second daemon, so it is one drop-in and one restart.
    The servers carry the hostname their certificate is checked against, which
    is what makes the TLS real rather than opportunistic. A failure is reported
    and the install carries on: the manager still opens and the plain resolver
    still answers, just not privately.
    """
    if not resolvectl_present():
        detail("systemd-resolved is not present, so DNS cannot be encrypted "
               "from here; a network that rewrites names will still misdirect "
               "them")
        return

    step("Encrypting DNS (DNS-over-TLS via systemd-resolved)")
    servers = " ".join(ENCRYPTED_DNS_SERVERS)
    try:
        RESOLVED_DROPIN.parent.mkdir(parents=True, exist_ok=True)
        RESOLVED_DROPIN.write_text(
            "[Resolve]\n"
            f"DNS={servers}\n"
            "Domains=~.\n"
            "DNSOverTLS=yes\n"
            "DNSSEC=allow-downgrade\n",
            encoding="utf-8",
        )
    except OSError as exc:
        detail(f"could not write {RESOLVED_DROPIN}: {exc}")
        return

    # Domains=~. makes resolved send EVERY name to these servers rather than
    # preferring whatever DHCP handed the link, so the encrypted servers are the
    # ones actually asked.
    run(["systemctl", "enable", "--now", "systemd-resolved"])
    run(["systemctl", "restart", "systemd-resolved"])
    run(["resolvectl", "flush-caches"])
    point_resolv_conf_at_stub()

    # Prove it: ask a name a hijacking resolver gets wrong and print what came
    # back. Not fatal if the check cannot be made.
    dig = shutil.which("dig")
    if dig is not None:
        probe = run([dig, "+short", "+time=2", "+tries=1", "discord.com"],
                    capture=True)
        answer = probe.stdout.strip().splitlines()
        if answer:
            detail(f"discord.com now resolves to {answer[0]}")
        else:
            detail("discord.com did not resolve; check /etc/resolv.conf and "
                   "systemctl status systemd-resolved")
    else:
        detail("DNS-over-TLS is on; no dig to confirm the answer with")


# --- zapret ----------------------------------------------------------------

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
    """Make sure systemd knows the zapret service, so the init script the DPI
    button runs is wired into startup. zapret's easy installer normally does
    this; it is repeated here because it is cheap and a machine where that step
    was skipped would leave the button with nothing to start."""
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


def run_streaming(
    command: list[str],
    cwd: Path,
    input_text: str,
    timeout: int,
) -> str:
    """Run a chatty program with its output shown as it arrives, and return all
    of it. blockcheck is the case this exists for: it prints a line per method
    it tries and runs for minutes, so capturing it silently is indistinguishable
    from a freeze. The output is therefore both echoed and kept, and a deadline
    is enforced so it can never sit there forever."""
    process = subprocess.Popen(
        command, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, bufsize=1,
    )
    if process.stdin is not None:
        process.stdin.write(input_text)
        process.stdin.close()

    collected: list[str] = []

    def pump() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            collected.append(line)
            sys.stdout.write("    " + line)
            sys.stdout.flush()

    reader = threading.Thread(target=pump, daemon=True)
    reader.start()
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        detail(f"blockcheck did not finish within {timeout}s and was stopped")
    reader.join(timeout=5)
    return "".join(collected)


def parse_blockcheck_nfqws(output: str, domain: str) -> str:
    """The nfqws options that WORKED, read from blockcheck's own summary.

    Only the `* SUMMARY` section is looked at, because it is the one that lists
    the methods that succeeded. The long log above it carries a
    "curl_test_https … : nfqws …" line for EVERY test, most of them failures
    ("Connection reset by peer"), and taking the first line from the whole
    output picks one of those — which is what happened: the method written out
    was `multisplit --dpi-desync-split-pos=2`, a line blockcheck had marked
    UNAVAILABLE, while the summary's first working method was
    `fake --dpi-desync-ttl=3`. The reference installer reads the summary for the
    same reason.

    A ttl-based method is preferred when the summary offers one, as the
    reference does: it needs no OS timestamp support and no particular server,
    so it is the most portable of the working set.
    """
    inside = False
    summary: list[str] = []
    for line in output.splitlines():
        if line.strip().startswith("* SUMMARY"):
            inside = True
            continue
        if inside and not line.strip():
            break
        if inside:
            summary.append(line)

    candidates = [
        line for line in summary
        if "curl_test_https" in line and "nfqws" in line and domain in line
    ]
    if not candidates:
        return ""
    with_ttl = [line for line in candidates if "ttl" in line.lower()]
    chosen = with_ttl[0] if with_ttl else candidates[0]
    _, _, tail = chosen.partition("nfqws")
    return tail.strip()


def stop_bypass() -> None:
    """Take the bypass down. Needed before measuring: with nfqws running every
    name comes back fine, so a probe made in that state sees no block at all."""
    if run(["systemctl", "stop", "zapret"], capture=True).returncode != 0:
        run(["sh", str(ZAPRET_INIT), "stop"], cwd=ZAPRET_DIR)


def start_bypass() -> None:
    """Bring the bypass back up."""
    if run(["systemctl", "start", "zapret"], capture=True).returncode != 0:
        run(["sh", str(ZAPRET_INIT), "start"], cwd=ZAPRET_DIR)


def write_nfqws_opt(method: str) -> None:
    """Write the chosen bypass into zapret's config the way the reference
    installer does: replace the whole (possibly multi-line, quote-delimited)
    NFQWS_OPT value with one line, then restart the service so it is live."""
    config = ZAPRET_DIR / "config"
    if not config.is_file():
        return
    try:
        lines = config.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        detail(f"could not read {config}: {exc}")
        return

    out: list[str] = []
    skipping = False
    replaced = False
    for line in lines:
        stripped = line.strip()
        if not replaced and stripped.startswith('NFQWS_OPT="'):
            out.append(f'NFQWS_OPT="{method} <HOSTLIST>"')
            replaced = True
            if stripped.count('"') < 2:
                skipping = True          # the value runs to a closing quote
            continue
        if skipping:
            if stripped == '"':
                skipping = False
            continue
        out.append(line)

    if not replaced:
        return
    try:
        config.write_text("\n".join(out) + "\n", encoding="utf-8")
    except OSError as exc:
        detail(f"could not write {config}: {exc}")
        return
    detail(f"bypass method set: {method}")
    start_bypass()


def choose_and_apply_bypass_method() -> None:
    """Ask zapret's own blockcheck which desync gets past this network.

    The easy installer leaves a generic desync in the config. The reference
    installer instead runs blockcheck against a blocked name and writes the
    method that actually worked into NFQWS_OPT — a desync that is right for one
    ISP is wrong for another, so this is what makes the bypass real rather than
    nominal. If no name looks blocked, or blockcheck finds no nfqws method, the
    easy installer's default is left alone.
    """
    blockcheck = ZAPRET_DIR / "blockcheck.sh"
    if not blockcheck.is_file():
        return

    # The probe is made with the bypass STOPPED. With nfqws running every name
    # comes back fine — that is its whole purpose — so a probe in that state
    # reports "no block" and this step silently does nothing, which is the trap
    # the earlier run fell into. The block is only visible with the bypass off.
    stop_bypass()

    domain = ""
    for candidate in BLOCKCHECK_DOMAINS:
        probe = run(["curl", "--max-time", "10", "-sSI",
                     f"https://{candidate}"], capture=True)
        if probe.returncode != 0:
            domain = candidate
            break
    if not domain:
        detail("this network resets none of the test names; the default "
               "bypass stays")
        start_bypass()
        return

    detail(f"\"{domain}\" is reset without the bypass, so blockcheck is run "
           f"against it")
    step("Choosing a bypass for this network")
    detail("this is zapret's own test and takes a few minutes; it prints as "
           "it goes")
    # blockcheck is prompt-driven; these are the answers the reference installer
    # feeds it — the name, then N to the extras it offers. Its output is shown
    # live (see run_streaming) so the wait is visibly doing something.
    answers = f"{domain}\n\nN\n\n\nN\n\n\n\n"
    output = run_streaming(["sh", str(blockcheck)], ZAPRET_DIR, answers,
                           BLOCKCHECK_TIMEOUT)
    method = parse_blockcheck_nfqws(output, domain)
    if not method:
        method = FALLBACK_NFQWS_OPT
        detail("blockcheck found no nfqws method; using the default")
    write_nfqws_opt(method)


def install_zapret() -> bool:
    """Fetch zapret and set it up with its own easy installer. Returns whether
    the init script ended up in place."""
    step("Installing zapret (the DPI bypass the DPI button switches)")
    # What zapret's nfqws needs to sit in the packet path, plus the tools its
    # installer itself calls.
    apt_install(("nftables", "iptables", "curl", "wget", "tar", "gzip", "jq",
                 "dnsutils"))

    work = Path(tempfile.mkdtemp(prefix="gnuchan-zapret-"))
    tarball = work / f"zapret-v{ZAPRET_VERSION}.tar.gz"
    try:
        if not download_zapret(tarball):
            detail("zapret could not be downloaded")
            return False
        with tarfile.open(tarball) as archive:
            archive.extractall(work)
        source = work / f"zapret-v{ZAPRET_VERSION}"
        if not source.is_dir():
            detail("the zapret archive did not contain what was expected")
            return False
        if ZAPRET_DIR.exists():
            shutil.rmtree(ZAPRET_DIR)
        shutil.copytree(source, ZAPRET_DIR)
    except (OSError, tarfile.TarError) as exc:
        detail(f"could not unpack zapret: {exc}")
        return False
    finally:
        shutil.rmtree(work, ignore_errors=True)

    # The easy installer does the real setup: it places the binaries, works out
    # the firewall type, and enables nfqws. It is prompt-driven, so answers are
    # fed the way the reference installer feeds them — accept the defaults.
    subprocess.run(["sh", str(ZAPRET_DIR / "install_prereq.sh")],
                   input="\n\n", text=True, check=False, cwd=ZAPRET_DIR)
    subprocess.run(["sh", str(ZAPRET_DIR / "install_bin.sh")],
                   text=True, check=False, cwd=ZAPRET_DIR)
    run(["sh", str(ZAPRET_DIR / "install_easy.sh")],
        input_text="\n\n\n4\n\n\nY\n\n\n\n\n\n", cwd=ZAPRET_DIR)
    install_zapret_units()
    return dpi_installed()


def ensure_dpi_dependency() -> None:
    """Make sure zapret is installed AND that its config holds a desync that
    actually works on this network.

    These are two separate jobs and both are done here, because the first is not
    sufficient without the second. The INSTALL puts zapret under /opt/zapret —
    the path net_dpi.c expects — and is skipped when it is already there. The
    DESYNC is chosen every run: it, not the install, is what decides whether a
    packet gets past the filter, and it has to be chosen against the network in
    the state it is in right now.
    """
    if dpi_installed():
        detail("zapret is already installed under /opt/zapret")
    elif install_zapret():
        detail("installed zapret under /opt/zapret")
    else:
        detail("zapret was not installed; the DPI button will report that no "
               "bypass is installed until it is")
        return

    configure_dpi_bypass()


def configure_dpi_bypass() -> None:
    """Choose this network's working desync, write it into the config, and
    confirm the daemon came up. Separate from the install so it runs even when
    zapret was already present."""
    if not dpi_installed():
        return
    choose_and_apply_bypass_method()

    if shutil.which("pgrep") is not None:
        probe = run(["pgrep", "-x", "nfqws"], capture=True)
        if probe.returncode == 0:
            detail("the DPI bypass is running (nfqws)")
        else:
            detail("nfqws is not running yet — press the DPI button in the "
                   "manager, or check: systemctl status zapret")


# --- building and installing ----------------------------------------------

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
        ensure_encrypted_dns()
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
