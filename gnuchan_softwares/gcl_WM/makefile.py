#!/usr/bin/env python3
# =============================================================================
# GnuChanWM - build and install everything (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# Installs the build dependencies, a terminal if the machine has none, builds
# the window manager, and installs it with a session entry so the display
# manager offers "GnuChanWM". It needs root and re-runs itself through sudo.
#
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and start on the current display
#     python3 makefile.py uninstall  remove the binary and the session entry
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
# Build output goes under _temp/, not into the source tree. A build is not a
# change to the project, and while the binary was written to ROOT/build/ every
# run left it behind for git to notice. _temp/ is ignored, so the working tree
# stays exactly as it was cloned.
BUILD = ROOT.parent.parent / "_temp" / "gcl_WM-build"

PROGRAM = "GnuChanWM"
BIN_DIR = Path("/usr/local/bin")
SESSION_DIR = Path("/usr/share/xsessions")
SESSION_FILE = SESSION_DIR / "gnuchanwm.desktop"

# The settings script, installed into the user's own config directory. The
# window manager reads it from there, so a machine that never had a script
# needs one put in place of it — otherwise the desktop falls back to its
# built-in defaults and looks nothing like what GnuchanOS shipped.
CONFIG_SOURCE = ROOT / "GnuChanWM_config" / "GnuChanWM.py"
CONFIG_DIR_NAME = "GnuChanWM"
CONFIG_FILE_NAME = "GnuChanWM.py"

SOURCES = (
    "wm_core.c",
    "wm_style.c",
    "wm_config_parser.c",
    "wm_config_file.c",
    "wm_config_apply.c",
    "wm_theme.c",
    "wm_desktop.c",
    "wm_frame_geometry.c",
    "wm_frame.c",
    "wm_frame_draw.c",
    "wm_frame_fullscreen.c",
    "wm_frame_interact.c",
    "wm_icon.c",
    "wm_image.c",
    "wm_manage.c",
    "wm_focus.c",
    "wm_switcher.c",
    "wm_switcher_view.c",
    "wm_compositor.c",
    "wm_randr.c",
    "wm_spawn.c",
    "wm_autostart.c",
    "wm_lid.c",
    "wm_idle.c",
    "wm_menu.c",
    "wm_input.c",
    "wm_keys.c",
    "wm_workspace.c",
    "wm_tray.c",
    "GnuChanWM.c",
)
HEADERS = (
    "wm_module.h", "wm_core.h", "wm_style.h", "wm_frame.h",
    "wm_frame_geometry.h", "wm_frame_internal.h", "wm_spawn.h",
    "wm_theme.h",
    "wm_config.h", "wm_config_parser.h",
    "wm_workspace.h", "wm_desktop.h", "wm_image.h",
    "wm_tray.h", "wm_switcher.h", "wm_compositor.h", "wm_randr.h",
)

FALLBACK_TERMINAL = "xterm"
TERMINAL_CANDIDATES = (
    "x-terminal-emulator", "gnome-terminal", "konsole",
    "xfce4-terminal", "alacritty", "kitty", "xterm",
)

ELEVATED_VARIABLE = "GNUCHANWM_ELEVATED"


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
) -> subprocess.CompletedProcess[str]:
    if capture:
        return subprocess.run(
            command, check=False, capture_output=True, text=True,
            env=environment, cwd=cwd,
        )
    return subprocess.run(command, check=False, text=True, env=environment, cwd=cwd)


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
        raise SystemExit("error: this installs packages and writes /usr; run as root")
    step("Needs root; re-running through sudo")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    # The arguments are carried across, and that is not tidiness: without them
    # the re-run falls back to the default action, so `build` asked for on a
    # machine that is missing a package would come back as a full install —
    # packages, binary and session entry — which is not what was asked for and
    # not something the person who typed it can tell has happened.
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def header_present(header: str, extra_includes: tuple[str, ...] = ()) -> bool:
    """Whether gcc can preprocess a header, which is how a machine is asked
    whether a development package is installed.

    The check is a real compile of a one-line translation unit rather than a
    test for a file on disk, because where a distribution keeps a header is
    not fixed and what matters is whether the compiler the build will use can
    find it. Xft pulls freetype in through its own header, so the freetype
    include directory is passed for it: the header is present exactly when
    both packages are.
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
    """Whether Xft's header can be preprocessed.

    The freetype include directory is passed because Xft's own header includes
    ft2build.h on its third line. That means a machine with libxft-dev and no
    freetype reports BOTH packages missing, and that is not a false report: no
    file of this window manager compiles in that state, and installing both is
    what fixes it. The two packages are still listed separately — see
    missing_build_dependencies() — so a machine that has one and not the other
    is told which.
    """
    return header_present("X11/Xft/Xft.h", freetype_includes())


def freetype_includes() -> tuple[str, ...]:
    """Where freetype keeps its headers, which is not /usr/include.

    freetype.pc is what knows, and the layout Debian uses is the fallback for a
    machine without pkg-config. The paths come back bare, the way
    header_present() wants them; pkg-config writes them with the -I attached,
    so it is taken off here rather than passed on and prepended a second time.
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        result = run([pkg_config, "--cflags-only-I", "freetype2"],
                     capture=True)
        if result.returncode == 0:
            paths = tuple(word[2:] for word in result.stdout.split()
                          if word.startswith("-I"))
            if paths:
                return paths
    return ("/usr/include/freetype2",)


def freetype_headers_present() -> bool:
    """Whether freetype's header is here.

    This is not optional and it is easy to be missing: Xft's own header
    includes ft2build.h, so a machine with libxft-dev and no freetype cannot
    compile a single file of this window manager — every one of them reaches
    Xft through wm_style.h. It is its own check because it is its own package,
    and it is the one whose headers do not live beside the others.
    """
    return header_present("ft2build.h", freetype_includes())


def xcursor_headers_present() -> bool:
    """Whether libXcursor's header is here.

    It is what loads a theme's cursor by name, which is the one thing plain X
    cannot do: the core protocol's cursors are the server's own shapes and know
    nothing about a theme. Without it the desktop falls back to drawing its own
    pointer, which is why it is a dependency of the build and not optional.
    """
    return header_present("X11/Xcursor/Xcursor.h")


def imlib2_headers_present() -> bool:
    """Whether Imlib2's header is here.

    Imlib2 is what reads the wallpaper and renders it onto a pixmap, the way
    feh does: it knows the file formats (through libpng, libjpeg and the rest,
    which it pulls in itself) and the drawing, so the window manager does not
    carry a decoder of its own. It is a dependency of the build because a
    desktop without it has only its flat colour; see dotfile/ or the README for
    the package that provides it.
    """
    return header_present("Imlib2.h")


def compositor_headers_present() -> bool:
    """Whether the compositor's three headers are here.

    One check for three packages, because the module cannot do anything with
    any two of them: taking a window's pixels away from the server (Composite)
    without being told when the program drew again (Damage) leaves the picture
    frozen, and without Render there is no way to draw it back scaled — which
    is what the window was taken for. Asking for all three is what keeps a
    machine from building a compositor that shows one frame and stops.

    They are the WM's own dependency and not the session's. A server that does
    not implement the extensions is found out at start-up, and every window
    then behaves as it did before the module existed — see wm_compositor.c.
    """
    return (header_present("X11/extensions/Xcomposite.h") and
            header_present("X11/extensions/Xdamage.h") and
            header_present("X11/extensions/Xrender.h"))


def randr_headers_present() -> bool:
    """Whether libXrandr's header is here.

    It is what the display guard in wm_randr.c is built against: the module
    remembers the desktop's CRTC mode at start-up and puts it back when a game
    changes it, which is how a Wine game going fullscreen is stopped from
    shrinking the whole screen. Without the header the guard does nothing, so
    it is a dependency of the build.
    """
    return header_present("X11/extensions/Xrandr.h")


def scrnsaver_headers_present() -> bool:
    """Whether libXss's header is here.

    It is what the idle module in wm_idle.c asks "how long has the keyboard and
    pointer been still?" with — the same idle measurement GnuChanSS reads on its
    own. Without it the module turns the server's own blanking and DPMS off (so
    the panel cannot go dark on its own) but cannot start the session's screen
    saver at the configured time, because it has no clock to watch. That is a
    degraded but not broken session, so it is a dependency of the build rather
    than an optional one.
    """
    return header_present("X11/extensions/scrnsaver.h")


def program_exists(name: str) -> bool:
    if "/" in name:
        return os.access(name, os.X_OK)
    return shutil.which(name) is not None


def apt_install(packages: tuple[str, ...]) -> bool:
    if not packages:
        return True
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *packages]
    detail("running: " + " ".join(command))
    return run(command, environment=apt_environment()).returncode == 0


def missing_build_dependencies() -> list[str]:
    """Which packages this machine is missing to build the window manager.

    Each entry is chosen by asking the compiler about the HEADER the package
    provides rather than by looking for a file, so a machine whose headers are
    somewhere unusual is judged by whether the build would actually work.

    The packages are listed separately and not bundled because they are needed
    for different things and a person reading the list wants to know which:
    libx11-dev is every window and every key, libxft-dev the text on the bar,
    libfreetype-dev is what Xft's own header includes — Xft cannot be compiled
    against without it, so it is not the optional extra its name suggests —
    libxcursor-dev the themed pointer, libimlib2-dev the wallpaper, and the
    last three are the compositor's, which is what one window's content is
    fitted into its frame with.
    """
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
    if shutil.which("pkg-config") is None:
        needed.append("pkg-config")
    if not x11_headers_present():
        needed.append("libx11-dev")
    if not xft_headers_present():
        needed.append("libxft-dev")
    # freetype is its own check and its own package because Xft's header
    # includes ft2build.h: without it NOT ONE file of this window manager
    # compiles, since every one of them reaches Xft through wm_style.h. It is
    # also the one whose headers do not live beside the others, so a machine
    # that looks complete can still be missing it.
    if not freetype_headers_present():
        needed.append("libfreetype-dev")
    if not xcursor_headers_present():
        needed.append("libxcursor-dev")
    if not imlib2_headers_present():
        needed.append("libimlib2-dev")
    # The compositor's three. They are asked for together because all three are
    # needed before any of it works — see compositor_headers_present().
    if not compositor_headers_present():
        needed.extend(("libxcomposite-dev", "libxdamage-dev",
                       "libxrender-dev"))
    # The display guard's, which is how a game is stopped from resizing the
    # whole desktop when it goes fullscreen — see wm_randr.c.
    if not randr_headers_present():
        needed.append("libxrandr-dev")
    # The idle module's: it is how the session answers "how long has the desk
    # been quiet?" and so when to run the screen saver — see wm_idle.c. Without
    # it the server's own blanking is still turned off (no more black screen)
    # but the saver cannot be started at the configured time.
    if not scrnsaver_headers_present():
        needed.append("libxss-dev")
    return needed


def ensure_build_dependencies() -> None:
    needed = missing_build_dependencies()
    if not needed:
        return
    step("Installing the build dependencies")
    detail("missing: " + ", ".join(needed))
    if not apt_install(tuple(needed)):
        raise SystemExit("error: apt-get could not install: " + ", ".join(needed))
    detail("installed: " + ", ".join(needed))


def ensure_terminal() -> None:
    for name in TERMINAL_CANDIDATES:
        if program_exists(name):
            detail(f"terminal: {name}")
            return
    step("Installing a terminal")
    if not apt_install((FALLBACK_TERMINAL,)):
        raise SystemExit(
            f"error: no terminal is installed and apt-get could not install "
            f"{FALLBACK_TERMINAL}"
        )
    detail(f"installed {FALLBACK_TERMINAL}")


def x11_flags() -> tuple[list[str], list[str]]:
    """The compiler and linker flags for the X libraries the WM uses.

    x11 is needed for everything and xft for the text, and the two are asked
    for together because Xft's own header includes freetype's: pkg-config is
    what knows where that lives, and a machine whose freetype is somewhere
    unusual is exactly the case these flags exist for. The fallback names the
    libraries and freetype's include directory directly, for a machine with no
    pkg-config, and is right on every Debian where the development packages are
    installed.

    The last three are the compositor's, and they are what one window's content
    is scaled with: xcomposite to take a window's pixels away from the server,
    xdamage to be told when the program has drawn again, and xrender to draw
    the picture at the size the frame is. They are dependencies of the build
    and not of the session — a server that lacks the extensions answers so at
    start-up and every window behaves as it did before (see wm_compositor.c).
    """
    pkg_config = shutil.which("pkg-config")
    if pkg_config is not None:
        # The screen-saver extension's module is named `xscrnsaver`, not `xss`
        # (its library is libXss, but that is not what pkg-config calls it),
        # and it needs `xext` beside it: XScreenSaverQueryInfo is an XExt call.
        cflags = run([pkg_config, "--cflags",
                      "x11", "xft", "xcursor", "imlib2",
                      "xcomposite", "xdamage", "xrender", "xrandr",
                      "xscrnsaver", "xext"],
                     capture=True)
        libs = run([pkg_config, "--libs",
                    "x11", "xft", "xcursor", "imlib2",
                    "xcomposite", "xdamage", "xrender", "xrandr",
                    "xscrnsaver", "xext"],
                   capture=True)
        if cflags.returncode == 0 and libs.returncode == 0:
            return cflags.stdout.split(), libs.stdout.split()
    fallback_includes = ["-I/usr/include"]
    fallback_includes += ["-I" + path for path in freetype_includes()]
    return (fallback_includes,
            ["-lX11", "-lXft", "-lXcursor", "-lImlib2",
             "-lXcomposite", "-lXdamage", "-lXrender", "-lXrandr",
             "-lXss", "-lXext"])


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS) if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    # The dependencies are checked here and not only by install_everything(),
    # and that is the whole point of `build`: it is the action a person runs to
    # find out whether the code compiles, and an answer of "cannot find
    # ft2build.h" is not an answer to that question. Asking for the packages it
    # needs is part of answering it. A machine that has them skips all of this.
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
    # -lm is for the arithmetic the frame and the icons do. It is named here
    # rather than through pkg-config because it has no include flags of its
    # own to find.
    command += ["-lm"]

    step("Building")
    if run(command, cwd=ROOT).returncode != 0:
        raise SystemExit("error: the build failed")
    detail(f"built {output}")
    return output


def session_entry() -> str:
    return "\n".join([
        "[Desktop Entry]",
        "Name=GnuChanWM",
        "Comment=GnuchanOS window manager",
        f"Exec={BIN_DIR / PROGRAM}",
        f"TryExec={BIN_DIR / PROGRAM}",
        "Type=Application",
        "DesktopNames=GnuChanWM",
        "",
    ])


def install(binary: Path) -> None:
    step("Installing")
    SESSION_DIR.mkdir(parents=True, exist_ok=True)

    # The window manager already running a session is the one this install is
    # replacing, so writing to its path would fail with ETXTBSY. The new
    # binary is written beside it and renamed over it: a rename swaps the
    # directory entry and never opens the file being replaced, and the running
    # session keeps its copy until it is logged out and back in.
    staged = BIN_DIR / (PROGRAM + ".new")
    shutil.copyfile(binary, staged)
    staged.chmod(0o755)
    os.replace(str(staged), str(BIN_DIR / PROGRAM))
    detail(f"installed {BIN_DIR / PROGRAM}")

    temporary = BUILD / "gnuchanwm.desktop.new"
    temporary.write_text(session_entry(), encoding="utf-8")
    os.replace(str(temporary), str(SESSION_FILE))
    SESSION_FILE.chmod(0o644)
    detail(f"installed {SESSION_FILE}")


def invoking_user() -> tuple[Path, int, int] | None:
    """The home and ids of the person who ran sudo, not root.

    The script belongs to the person who logs in, and the window manager reads
    it from their home. Under sudo, HOME and the ids are root's, so the
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
    """Put the settings script where the window manager will read it.

    The installed script is replaced on every run, not kept: this installer
    owns that file, and a machine that installs a new build has to get the
    settings that build ships with rather than whichever script happened to be
    there. The old file is removed first — an install that only copied over it
    would leave a script the window manager is watching change underneath it,
    and the copy is a fresh file either way.

    The window manager reads the script live, so a running session picks the
    new one up on its own; nothing here needs to reload it.
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

    # The source is shipped with the window manager, so it is never empty; a
    # check here is what turns "the desktop came up with no bar" into one line
    # naming the file that caused it, before the old one is touched.
    if CONFIG_SOURCE.stat().st_size == 0:
        raise SystemExit(
            f"error: {CONFIG_SOURCE} is empty, so the window manager would "
            f"have nothing to read"
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
    # or the window manager opens a file it can see but not change.
    try:
        os.chown(directory, uid, gid)
        os.chown(target, uid, gid)
    except OSError:
        pass

    detail(f"installed {target}")


def uninstall() -> None:
    step("Uninstalling")
    for target in (BIN_DIR / PROGRAM, SESSION_FILE):
        if target.exists():
            target.unlink()
            detail(f"removed {target}")


def run_session(binary: Path) -> int:
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY is not set, so there is no X server to run on")
    step("Starting on the current display")
    return run([str(binary)], cwd=ROOT).returncode


def install_everything() -> int:
    ensure_build_dependencies()
    ensure_terminal()
    binary = build()
    install(binary)
    install_config()
    note("")
    note("GnuChanWM is installed. It is in the display manager's session menu.")
    return 0


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(
            f"error: GnuChanWM is a Debian X11 window manager; this is "
            f"{platform.system()}"
        )
    if not is_debian():
        raise SystemExit(
            "error: this installs with apt-get and writes Debian's session "
            "directory; it is Debian-only by design"
        )

    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "build", "run", "uninstall")
    if action not in known:
        print(f"error: unknown action '{action}'", file=sys.stderr)
        print("usage: python3 makefile.py [" + "|".join(known) + "]", file=sys.stderr)
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
        return run_session(build())

    return install_everything()


if __name__ == "__main__":
    raise SystemExit(main())
