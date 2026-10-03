#!/usr/bin/env python3
# =============================================================================
# GnuChanFetch - build and install (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py            build and install the program
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and run it here
#     python3 makefile.py uninstall  remove the binary
#
# GnuChanFetch is a C program. It prints facts about the machine with the
# GnuchanOS logo drawn beside them, in the shape of neofetch. Its one library is
# Imlib2, which reads the picture — the same library the window manager draws
# its wallpaper with, so a machine that runs this desktop already has it.
#
# Debian only, on purpose.
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent
# Build output goes under _temp/, not into the source tree: a build is not a
# change to the project, and _temp/ is ignored, so the working tree stays as it
# was cloned.
BUILD = ROOT.parent.parent / "_temp" / "gcl_FETCH-build"

PROGRAM = "GnuChanFetch"
BIN_DIR = Path("/usr/local/bin")

# The settings script, installed into the user's own config directory, beside
# the logo. It is replaced on every install: the installer owns that file, and a
# machine installing a new build has to get the settings that build ships with.
CONFIG_SOURCE = ROOT / "GnuChanFetch_config" / "GnuChanFetch.py"
CONFIG_DIR_NAME = "GnuChanFetch"
CONFIG_FILE_NAME = "GnuChanFetch.py"

# The logo the settings script points at. It is installed beside the script so
# the default Image path resolves without anything else being configured.
LOGO_SOURCE = ROOT.parent.parent / "assets" / "logo.png"
LOGO_FILE_NAME = "logo.png"

SOURCES = (
    "fetch_parser.c",
    "fetch_config.c",
    "fetch_color.c",
    "fetch_image.c",
    "fetch_info.c",
    "fetch_draw.c",
    "GnuChanFetch.c",
)
HEADERS = (
    "fetch_config.h",
    "fetch_parser.h",
    "fetch_color.h",
    "fetch_image.h",
    "fetch_info.h",
    "fetch_draw.h",
)

ELEVATED_VARIABLE = "GNUCHANFETCH_ELEVATED"


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(command: list[str], capture: bool = False,
        environment: dict | None = None, cwd: Path | None = None):
    if capture:
        return subprocess.run(command, check=False, capture_output=True,
                              text=True, env=environment, cwd=cwd)
    return subprocess.run(command, check=False, text=True, env=environment,
                          cwd=cwd)


def apt_environment() -> dict:
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
        raise SystemExit("error: this installs packages and writes /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    # The arguments are carried across so that `build` asked for on a machine
    # that is missing the library does not come back as a full install.
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed.

    The check is a real compile of a one-line translation unit rather than a
    test for a file on disk, because where a distribution keeps a header is not
    fixed and what matters is whether the compiler the build will use can find
    it. Imlib2's own header includes nothing unusual, so no extra include path
    is needed for it.
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


def imlib2_headers_present() -> bool:
    return header_present("Imlib2.h")


def missing_build_dependencies() -> list[str]:
    """Which packages this machine is missing to build the program.

    The one library is Imlib2, which reads the picture. It is named by the
    header the package provides rather than by looking for a file, so a machine
    whose headers are somewhere unusual is judged by whether the build would
    actually work.
    """
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not imlib2_headers_present():
        needed.append("libimlib2-dev")
    return needed


def ensure_build_dependencies() -> None:
    needed = missing_build_dependencies()
    if not needed:
        return
    step("Installing the build dependencies")
    detail("missing: " + ", ".join(needed))
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *needed]
    detail("running: " + " ".join(command))
    if run(command, environment=apt_environment()).returncode != 0:
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def imlib2_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for Imlib2.

    pkg-config is what knows where its includes live; the fallback names the
    library and the standard include directory directly, for a machine without
    pkg-config. Imlib2 pulls in its own image-format libraries (libpng, libjpeg
    and the rest), so nothing else has to be named here.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", "imlib2"], capture=True)
        libs = run([pkg_config, "--libs", "imlib2"], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    return (["-I/usr/include"], ["-lImlib2"])


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS)
               if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    # The dependency is checked here and not only by install(): `build` is the
    # action a person runs to find out whether the code compiles, and an answer
    # of "cannot find Imlib2.h" is not an answer to that question.
    ensure_build_dependencies()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    cflags, libs = imlib2_flags()
    command = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-O2", "-D_DEFAULT_SOURCE",
        "-I", str(ROOT),
        *cflags,
    ]
    command += [str(ROOT / name) for name in SOURCES]
    command += ["-o", str(output)]
    command += libs
    # -lm is not needed by anything here, and is left out on purpose: every
    # number this program prints comes from a file or an integer count.

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root. See the window
    manager's installer for the same reasoning."""
    name = os.environ.get("SUDO_USER")
    if not name:
        return None
    try:
        import pwd
        info = pwd.getpwnam(name)
        return Path(info.pw_dir), info.pw_uid, info.pw_gid
    except (ImportError, KeyError):
        return None


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    target = BIN_DIR / PROGRAM
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(target))
    detail(f"installed {target}")

    install_config()


def install_config() -> None:
    """Put the settings script and the logo where the program reads them.

    The old files are removed first and the shipped ones written in their place,
    the same rule the other installers follow: the result is what this build
    ships and nothing of what was there before. Both land in the user's config
    directory, the script and the picture together, which is what the script's
    own default Image path expects.
    """
    if not CONFIG_SOURCE.is_file():
        return
    user = invoking_user()
    if user is None:
        detail("skipped the config: no login user to install it for")
        return

    home, uid, gid = user
    directory = home / ".config" / CONFIG_DIR_NAME
    directory.mkdir(parents=True, exist_ok=True)

    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the program would have "
            f"nothing to read"
        )

    script = directory / CONFIG_FILE_NAME
    if script.exists():
        script.unlink()
        detail(f"removed the old {script}")
    shutil.copyfile(CONFIG_SOURCE, script)
    try:
        os.chown(script, uid, gid)
    except OSError:
        pass
    detail(f"installed {script}")

    # The logo, if the repository has one. A machine with no logo still works —
    # the program prints the facts on their own — but the shipped desktop has
    # one, and the script points at it by this name.
    if LOGO_SOURCE.is_file():
        logo = directory / LOGO_FILE_NAME
        shutil.copyfile(LOGO_SOURCE, logo)
        try:
            os.chown(logo, uid, gid)
        except OSError:
            pass
        detail(f"installed {logo}")
    else:
        detail(f"no logo at {LOGO_SOURCE}; the facts are printed on their own")

    try:
        os.chown(directory, uid, gid)
    except OSError:
        pass


def uninstall() -> None:
    step("Uninstalling")
    target = BIN_DIR / PROGRAM
    if target.exists():
        target.unlink()
        detail(f"removed {target}")
    note(f"The settings under ~/.config/{CONFIG_DIR_NAME}/ were left alone.")


def run_program(binary: Path) -> int:
    step("Running")
    return run([str(binary)], cwd=ROOT).returncode


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanFetch is a Debian program; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's system "
            "directories; it is Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "uninstall")
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

    if action == "run":
        return run_program(build())

    binary = build()
    install(binary)
    note("")
    note("GnuChanFetch is installed.")
    note(f"{BIN_DIR / PROGRAM} runs it from a shell.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
