#!/usr/bin/env python3
# =============================================================================
# GnuChanTerm - build and install (Debian X11)
# -----------------------------------------------------------------------------
#     python3 makefile.py            install the packages, build, install
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py uninstall  remove the binary and the desktop entry
#
# The same shape as gcl_WM's makefile, and for the same reasons: the packages
# are asked for by HEADER and not by looking for a file, the build output goes
# under _temp/ so a build never dirties the tree, and the whole thing re-runs
# itself through sudo rather than telling the user to.
#
# The install has FOUR parts and not one, exactly as the window manager's does:
# the binary, the terminfo entry, the desktop entry, and the settings script.
# The settings script is the one that was missing, and a terminal without it
# runs on the palette compiled into it (gcl_palette.h) — which is why a fresh
# install looked unchanged whatever the shipped GnuChanTerm.py said. See
# install_config() and term_config_path() in term_config.c.
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
REPO_ROOT = ROOT.parent.parent

# Build output goes under _temp/, not into the tree. A build is not a change to
# the project, and _temp/ is ignored by git — while the binary lived in the
# tree, every run left it behind for git to notice.
BUILD = REPO_ROOT / "_temp" / "gcl_terminal-build"

PROGRAM = "GnuChanTerm"
BIN_DIR = Path("/usr/local/bin")
APPS_DIR = Path("/usr/share/applications")
DESKTOP_FILE = APPS_DIR / "gnuchanterm.desktop"

# The settings script, installed into the user's own config directory. The
# terminal reads it from there — see term_config_path() in term_config.c — so a
# machine that never had one gets the shipped file written where it looks. This
# is the step that was missing: without it the terminal kept the built-in
# palette and looked identical to the terminal as it shipped, which reads as
# "the settings file does nothing".
CONFIG_SOURCE = ROOT / "GnuChanTerm_config" / "GnuChanTerm.py"
CONFIG_DIR_NAME = "GnuChanTerm"
CONFIG_FILE_NAME = "GnuChanTerm.py"

# The terminfo entry, which is what makes the terminal's name mean something.
# It goes in the system database so every program on the machine resolves
# `gcl-256color`; a terminal whose name has no entry is a terminal every
# program treats as a dumb one, which means no colour and no cursor keys.
TERMINFO_NAME = "gcl-256color"
TERMINFO_DIR = Path("/usr/share/terminfo")

SOURCES = (
    "term_grid.c",
    "term_scroll.c",
    "term_vt.c",
    "term_vt_osc.c",
    "term_config_parser.c",
    "term_config.c",
    "term_style.c",
    "term_image.c",
    "term_pty.c",
    "term_render.c",
    "term_input.c",
    "term_select.c",
    "term_core.c",
    "GnuChanTerm.c",
)

HEADERS = (
    "gcl_palette.h",
    "term_config.h",
    "term_config_parser.h",
    "term_module.h",
    "term_grid.h",
    "term_scroll.h",
    "term_vt.h",
    "term_vt_osc.h",
    "term_style.h",
    "term_image.h",
    "term_pty.h",
    "term_render.h",
    "term_render_internal.h",
    "term_input.h",
    "term_select.h",
    "term_core.h",
)

ELEVATED_VARIABLE = "GNUCHANTERM_ELEVATED"

# The headers each package provides, and the pkg-config name that knows where
# they live. The check is a real compile of a one-line translation unit rather
# than a test for a file on disk, because where a distribution keeps a header
# is not fixed and what matters is whether the compiler the build will use can
# find it.
#
# xft pulls freetype in through its own header, so freetype is its own check:
# Xft's header includes ft2build.h on its third line, and a machine with
# libxft-dev and no freetype cannot compile a single file of this program.
PACKAGE_CHECKS = (
    ("X11/Xlib.h", "libx11-dev"),
    ("X11/Xft/Xft.h", "libxft-dev"),
    ("ft2build.h", "libfreetype-dev"),
    ("X11/extensions/Xrender.h", "libxrender-dev"),
    # Imlib2 is what a picture is loaded and scaled with. The terminal draws a
    # picture a program places as PIXELS, and Imlib2 is the library that reads
    # the file and reduces it by area — a picture turned into characters is the
    # thing this replaced, and the one thing this needs a decoder for.
    ("Imlib2.h", "libimlib2-dev"),
)

# What the X libraries are asked for by. The fallback names them directly, for
# a machine without pkg-config, and is right on every Debian where the
# development packages are installed.
PKG_CONFIG_MODULES = ("x11", "xft", "xrender", "fontconfig", "freetype2",
                      "imlib2")
FALLBACK_LIBS = ["-lX11", "-lXft", "-lXrender", "-lfontconfig", "-lfreetype",
                 "-lImlib2"]


def step(message: str) -> None:
    print(f"==> {message}", flush=True)


def detail(message: str) -> None:
    print(f"    {message}", flush=True)


def note(message: str) -> None:
    print(message, flush=True)


def run(command: list[str], capture: bool = False,
        environment: dict | None = None,
        cwd: Path | None = None) -> subprocess.CompletedProcess:
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
    # The arguments are carried across, and that is not tidiness: without them
    # the re-run falls back to the default action, so `build` asked for on a
    # machine missing a package would come back as a full install.
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def freetype_includes() -> tuple[str, ...]:
    """Where freetype keeps its headers, which is not /usr/include.

    freetype.pc is what knows, and the layout Debian uses is the fallback for a
    machine without pkg-config.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        result = run([pkg_config, "--cflags-only-I", "freetype2"], capture=True)
        if result.returncode == 0:
            paths = tuple(word[2:] for word in result.stdout.split()
                          if word.startswith("-I"))
            if paths:
                return paths
    return ("/usr/include/freetype2",)


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header — how a machine is asked whether a
    development package is installed."""
    gcc = shutil.which("gcc")
    if gcc is None:
        return False
    command = [gcc, "-E", "-xc", "-"]
    for path in extra_includes:
        command += ["-I", path]
    result = subprocess.run(command, input=f"#include <{header}>\n",
                            text=True, capture_output=True, check=False)
    return result.returncode == 0


def missing_build_dependencies() -> list[str]:
    """Which packages this machine is missing to build the terminal.

    Each entry is chosen by asking the compiler about the HEADER the package
    provides, so a machine whose headers are somewhere unusual is judged by
    whether the build would actually work.
    """
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    for header, package in PACKAGE_CHECKS:
        if not header_present(header, freetype_includes()):
            needed.append(package)
    if not Path("/usr/share/terminfo").exists():
        # Not fatal: the install writes the entry and the directory is created
        # with it. It is listed so a machine without ncurses is told why its
        # terminal has no name.
        pass
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


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the libraries this uses.

    x11 is every window and every key, xft the text, xrender what the text is
    composited with, and fontconfig and freetype are what Xft is built on. They
    are asked for together because Xft's own header includes freetype's, and a
    machine whose freetype is somewhere unusual is exactly the case pkg-config
    exists for.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        cflags = run([pkg_config, "--cflags", *PKG_CONFIG_MODULES], capture=True)
        libs = run([pkg_config, "--libs", *PKG_CONFIG_MODULES], capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    fallback_includes = ["-I/usr/include"]
    fallback_includes += ["-I" + path for path in freetype_includes()]
    return fallback_includes, list(FALLBACK_LIBS)


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS)
               if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    # The dependencies are checked here and not only by the install path, and
    # that is the whole point of `build`: it is the action a person runs to find
    # out whether the code compiles, and an answer of "cannot find Xft.h" is not
    # an answer to that question.
    ensure_build_dependencies()

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
    # -lutil is forkpty(), which is what the whole child process rests on. It is
    # named here rather than through pkg-config because it has no include flags
    # of its own to find.
    command += ["-lutil", "-lm"]

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def desktop_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        "Name=GnuChanTerm",
        "Comment=GnuchanOS terminal",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Terminal=false",
        "Categories=System;TerminalEmulator;",
        "Icon=utilities-terminal",
        "",
    ])


def install(binary: Path) -> None:
    step("Installing")
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    APPS_DIR.mkdir(parents=True, exist_ok=True)

    # The terminal already running is the one this install is replacing, so
    # writing to its path would fail with ETXTBSY. The new binary is written
    # beside it and renamed over it: a rename swaps the directory entry and
    # never opens the file being replaced.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
    detail(f"installed {BIN_DIR / PROGRAM}")

    temporary = BUILD / "gnuchanterm.desktop.new"
    temporary.write_text(desktop_entry(), encoding="utf-8")
    os.replace(str(temporary), str(DESKTOP_FILE))
    DESKTOP_FILE.chmod(0o644)
    detail(f"installed {DESKTOP_FILE}")

    install_terminfo()


def install_terminfo() -> None:
    """Write the terminal's own terminfo entry.

    Without it the name this terminal answers to means nothing to any program on
    the machine, and `TERM=gcl-256color` resolves to the dumb terminal — no
    colour, no cursor keys, no screen handling. The entry is generated with
    `tic` from a source written here, so it describes exactly this terminal and
    not an approximation of another one.

    `tic` comes from ncurses-bin, which is present on every Debian; a machine
    without it is told so and the install continues, because a terminal with no
    entry still runs, badly.
    """
    tic = shutil.which("tic")
    if tic is None:
        detail("tic not found (ncurses-bin) - skipping the terminfo entry")
        return

    source = ROOT / "terminfo" / TERMINFO_NAME
    if not source.is_file():
        detail(f"no terminfo source at {source} - skipping")
        return

    TERMINFO_DIR.mkdir(parents=True, exist_ok=True)
    result = run([tic, "-x", "-o", str(TERMINFO_DIR), str(source)])
    if result.returncode == 0:
        detail(f"installed terminfo entry {TERMINFO_NAME}")
    else:
        detail(f"tic failed for {source} - the terminal will run without a name")


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root.

    The settings script belongs to the person who logs in, and the terminal
    reads it from their home. Under sudo, HOME and the ids are root's, so the
    original user is read back from SUDO_USER — the one piece of the invoking
    session sudo keeps. Returns None when there is nothing sensible to write
    to, which is a machine installing without a login user.
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
    """Put the settings script where the terminal will read it.

    The installed script is replaced on every run, not kept, exactly as the
    window manager's own installer replaces its script: this installer owns
    that file, and a machine that installs a new build has to get the settings
    that build ships with rather than whichever script happened to be there.
    The old file is removed first — an install that only copied over it would
    leave a script the terminal is reading change underneath it, and the copy
    is a fresh file either way.

    The terminal reads the script once, at start; nothing here needs to reload
    it, and the next launch picks the new one up.
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

    # The source is shipped with the terminal, so it is never empty; a check
    # here turns "the colours did not change" into one line naming the file
    # that caused it, before the old one is touched.
    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the terminal would have "
            f"nothing to read"
        )

    directory.mkdir(parents=True, exist_ok=True)

    # The old script is taken out before the new one is put in place, so the
    # result is the shipped file and nothing of what was there before. A
    # missing file is not an error: a first install has nothing to remove.
    if target.exists():
        target.unlink()
        detail(f"removed the old {target}")

    shutil.copyfile(CONFIG_SOURCE, target)

    # The file belongs to the user who will read it, not to root who wrote it,
    # or the terminal opens a file it can see but the user cannot change.
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
    # The terminfo entry is left behind on purpose: another terminal may have
    # been built from it, and removing a name that is in use is worse than a
    # stale entry.
    # The settings script is left behind for the same kind of reason: it is the
    # user's own colours, and an uninstall that took them away would undo an
    # edit the person made.
    note(f"The settings script under ~/.config/{CONFIG_DIR_NAME}/ was left alone.")


def run_terminal(binary: Path) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set, so there is no X server to run on")
    step("Starting on the current display")
    return run([str(binary)], cwd=ROOT).returncode


def install_everything() -> int:
    ensure_build_dependencies()
    binary = build()
    install(binary)
    install_config()
    print("")
    print("GnuChanTerm is installed. It is in the applications menu, and")
    print(f"{BIN_DIR / PROGRAM} runs it from a shell.")
    return 0


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanTerm is a Debian X11 terminal; this is "
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
        print(f"Built {binary}.")
        return 0

    if action == "run":
        return run_terminal(build())

    return install_everything()


if __name__ == "__main__":
    raise SystemExit(main())
