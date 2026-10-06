#!/usr/bin/env python3
# =============================================================================
# GnuChanTop - build and install (Debian)
# -----------------------------------------------------------------------------
#     python3 makefile.py            build and install the program
#     python3 makefile.py build      compile only
#     python3 makefile.py run        compile and run it here
#     python3 makefile.py uninstall  remove the binary
#
# GnuChanTop is a C program with no library but libc. It reads /proc and sysfs
# directly and draws a btop-shaped monitor to the terminal: a CPU panel on the
# left of the top bar, a GPU panel on the right, and a process list below that
# the arrow keys walk and `k` kills from. Nothing is fetched and nothing is
# linked, so the build is one gcc line and the only dependency is a compiler.
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
BUILD = ROOT.parent.parent / "_temp" / "gcl_TOP-build"

PROGRAM = "GnuChanTop"
BIN_DIR = Path("/usr/local/bin")

# The settings script, installed into the user's own config directory. It is
# replaced on every install: the installer owns that file, and a machine
# installing a new build has to get the settings that build ships with.
CONFIG_SOURCE = ROOT / "GnuChanTop_config" / "GnuChanTop.py"
CONFIG_DIR_NAME = "GnuChanTop"
CONFIG_FILE_NAME = "GnuChanTop.py"

SOURCES = (
    "top_config.c",
    "top_cpu.c",
    "top_mem.c",
    "top_gpu.c",
    "top_proc.c",
    "top_term.c",
    "top_graph.c",
    "top_draw.c",
    "top_ui.c",
    "GnuChanTop.c",
)
HEADERS = (
    "top_common.h",
    "top_config.h",
    "top_cpu.h",
    "top_mem.h",
    "top_gpu.h",
    "top_proc.h",
    "top_term.h",
    "top_graph.h",
    "top_draw.h",
    "top_ui.h",
)

ELEVATED_VARIABLE = "GNUCHANTOP_ELEVATED"


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
    # that is missing the compiler does not come back as a full install.
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]],
               environment)


def is_debian() -> bool:
    return shutil.which("apt-get") is not None


def missing_build_dependencies() -> list[str]:
    """Which packages this machine is missing to build the program.

    There is one: a C compiler. The program links nothing but libc, so there is
    no header to probe for and no library to name — build-essential is the whole
    of it.
    """
    needed: list[str] = []
    if shutil.which("gcc") is None:
        needed.append("build-essential")
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


def check_sources() -> None:
    missing = [name for name in (*SOURCES, *HEADERS)
               if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("error: missing source files: " + ", ".join(missing))


def build() -> Path:
    check_sources()
    # The dependency is checked here and not only by install(): `build` is the
    # action a person runs to find out whether the code compiles, and an answer
    # of "cannot find gcc" is not an answer to that question.
    ensure_build_dependencies()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / PROGRAM

    command = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-O2", "-D_DEFAULT_SOURCE", "-D_POSIX_C_SOURCE=200809L",
        "-I", str(ROOT),
    ]
    command += [str(ROOT / name) for name in SOURCES]
    command += ["-o", str(output)]
    # -lm is needed for the graph drawing, which uses floor/round on a curve.
    command += ["-lm"]

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

    grant_perfmon(target)
    allow_perf_events()
    install_config()

# The sysctl the perf counters need. The i915 GPU-busy counter is a system-wide
# perf event, and the kernel refuses those to an unprivileged process while
# perf_event_paranoid is 3 or higher — even one holding CAP_PERFMON, as measured
# on the target machine. 2 is the value at which CAP_PERFMON is enough: it still
# keeps kernel profiling out of unprivileged hands, and it is the setting most
# distributions ship (Debian is the outlier at 3). The narrower alternative is
# CAP_SYS_ADMIN on the binary, which is root-equivalent and not something a
# monitor should be given; a sysctl that keeps the capability check in place is
# the smaller grant.
PERF_SYSCTL_PATH = Path("/etc/sysctl.d/99-gnuchantop.conf")
PERF_SYSCTL_VALUE = "kernel.perf_event_paranoid = 2\n"


def allow_perf_events() -> None:
    """Lower perf_event_paranoid to the value CAP_PERFMON needs, persistently."""
    try:
        PERF_SYSCTL_PATH.write_text(PERF_SYSCTL_VALUE)
    except OSError as problem:
        detail(f"could not write {PERF_SYSCTL_PATH}: {problem}")
        return
    detail(f"wrote {PERF_SYSCTL_PATH}")

    sysctl = shutil.which("sysctl")
    if sysctl is None:
        return
    if run([sysctl, "-w", "kernel.perf_event_paranoid=2"],
           capture=True).returncode == 0:
        detail("perf_event_paranoid is now 2")
    else:
        detail("the sysctl takes effect after a reboot")


def grant_perfmon(target: Path) -> None:
    """Give the binary CAP_PERFMON so the CPU and GPU perf counters can be read.

    The kernel hands /proc-adjacent perf events to root by default
    (perf_event_paranoid is 3 on Debian), and the i915 GPU-busy counter is one of
    those. A monitor that only read its numbers as root would ask for a password
    to see the GPU, which is not a monitor a person runs. The capability is the
    narrow grant the kernel provides for exactly this: it covers the performance
    counters and nothing else, so the binary does not run as root.

    setcap comes from libcap2-bin, which is pulled in when it is missing. A
    machine without it still gets a working program; only the GPU reading falls
    back to `n/a`.
    """
    setcap = shutil.which("setcap")
    if setcap is None:
        step("Installing libcap2-bin for setcap")
        run(["apt-get", "install", "-y", "--no-install-recommends",
             "libcap2-bin"], environment=apt_environment())
        setcap = shutil.which("setcap")
    if setcap is None:
        detail("setcap is unavailable; the GPU reading will show n/a")
        return

    result = run([setcap, "cap_perfmon+ep", str(target)], capture=True)
    if result.returncode == 0:
        detail(f"granted CAP_PERFMON to {target}")
    else:
        detail("could not grant CAP_PERFMON; the GPU reading will show n/a")
        if result.stderr:
            detail(result.stderr.strip())


def install_config() -> None:
    """Put the settings script where the program reads it.

    The old file is removed first and the shipped one written in its place, the
    same rule the other installers follow: the result is what this build ships
    and nothing of what was there before.
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
            f"error: GnuChanTop is a Debian program; this is "
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
    note("GnuChanTop is installed.")
    note(f"{BIN_DIR / PROGRAM} runs it from a shell.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
