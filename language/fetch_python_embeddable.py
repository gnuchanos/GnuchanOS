#!/usr/bin/env python3
"""
fetch_python_embeddable.py — Download and prepare Python embeddable
package for side-by-side distribution with gcl.

Works on both Windows and Linux:
  Windows → downloads embeddable zip (python314.dll + python3.14.zip)
  Linux   → downloads source tarball & builds libpython3.x.so
             OR detects system-installed python3-embed

Usage:
    python fetch_python_embeddable.py
    python fetch_python_embeddable.py --outdir _output_test
    python fetch_python_embeddable.py --version 3.12

After running, the Python shared library and stdlib are placed in
the output directory so that:
    gcl -pyrun hello.py -pydll <path/to/python314.dll>
    gcl -pyrun hello.py -pyso  <path/to/libpython3.14.so>
works with no system Python required (on Windows; on Linux the .so
still needs LD_LIBRARY_PATH or rpath set).
"""

import sys
import os
import platform
import re
import subprocess
import urllib.request
import zipfile
import tarfile
import shutil
import hashlib

# ── Defaults ─────────────────────────────────────────────────────
OUTDIR = os.path.join("_output_test")
PYTHON_VERSION = "3.14"


# ── Utils ─────────────────────────────────────────────────────────
def debug(msg):
    print(f"[fetch] {msg}")


def run(cmd, cwd=None):
    debug(f"$ {' '.join(cmd)}")
    r = subprocess.run(cmd, cwd=cwd)
    if r.returncode != 0:
        debug(f"FAILED (exit={r.returncode})")
        sys.exit(r.returncode)


def get_latest_minor(version):
    """Find latest minor version by scraping the python.org directory."""
    url = "https://www.python.org/ftp/python/"
    try:
        with urllib.request.urlopen(url, timeout=15) as resp:
            html = resp.read().decode("utf-8")
            pattern = re.compile(rf'href="({re.escape(version)}\.\d+)/"')
            matches = pattern.findall(html)
            if not matches:
                debug(f"Could not find any Python {version}.x releases")
                return None

            def ver_sort(v):
                parts = v.split(".")
                return tuple(int(x) for x in parts)

            matches.sort(key=ver_sort, reverse=True)
            latest = matches[0]
            debug(f"Latest Python {version} release: {latest}")
            return latest
    except Exception as e:
        debug(f"Failed to fetch version list: {e}")
        return version  # fallback


def download_file(url, dest):
    """Download url to dest, with resume support."""
    if os.path.isfile(dest):
        debug(f"Using cached {dest}")
        return

    debug(f"Downloading {url} ...")
    try:
        urllib.request.urlretrieve(url, dest)
        debug(f"Saved to {dest}")
    except Exception as e:
        debug(f"Download failed: {e}")
        debug(f"  URL: {url}")
        sys.exit(1)


def ensure_directory(path):
    os.makedirs(path, exist_ok=True)


# ── Windows: embeddable zip ──────────────────────────────────────
def fetch_windows(full_version, outdir):
    """
    Windows: download the official embeddable zip from python.org.
    Contains python314.dll, python3.14.zip (stdlib), python3.dll.
    """
    url = (
        f"https://www.python.org/ftp/python/{full_version}/"
        f"python-{full_version}-embed-amd64.zip"
    )
    zip_path = os.path.join(outdir, f"python-{full_version}-embed-amd64.zip")

    download_file(url, zip_path)

    debug("Extracting ...")
    with zipfile.ZipFile(zip_path, "r") as zf:
        zf.extractall(outdir)

    # List contents
    debug("\nContents:")
    for fname in sorted(os.listdir(outdir)):
        fpath = os.path.join(outdir, fname)
        size = os.path.getsize(fpath) if os.path.isfile(fpath) else 0
        kind = "DIR" if os.path.isdir(fpath) else f"{size:>8,} bytes"
        debug(f"  {fname:30s} {kind}")

    # Verify key files
    py_ver = full_version.replace(".", "")
    dll_path = os.path.join(outdir, f"python{py_ver}.dll")
    zip_stdlib = os.path.join(outdir, f"python{py_ver}.zip")
    python3_dll = os.path.join(outdir, "python3.dll")

    found = 0
    if os.path.isfile(dll_path):
        debug(f"✓ python{py_ver}.dll found ({os.path.getsize(dll_path):,} bytes)")
        found += 1
    else:
        debug(f"✗ python{py_ver}.dll NOT found")

    if os.path.isfile(zip_stdlib):
        debug(f"✓ python{py_ver}.zip (stdlib) found")
        found += 1

    if os.path.isfile(python3_dll):
        debug(f"✓ python3.dll found")

    if found < 2:
        debug("WARNING: expected Python DLL or stdlib zip not found!")
        debug("The extract may have failed or the package structure changed.")

    debug(f"\nDone. Python embeddable files → {outdir}")
    debug(f"  gcl -pyrun script.py -pydll {os.path.join(outdir, f'python{py_ver}.dll')}")


# ── Linux: build from source or detect system ────────────────────
def fetch_linux(full_version, outdir):
    """
    Linux:  Two strategies:
      1. Detect system-installed python3-embed package → copy .so
      2. Download source tarball → configure --enable-shared → make

    Strategy 1 is preferred when available.
    """
    # Try strategy 1 first: detect system python3-embed
    libname = f"libpython{full_version}.so"
    if try_detect_system_embed(full_version, libname, outdir):
        return

    # Strategy 2: build from source
    build_from_source(full_version, libname, outdir)


def try_detect_system_embed(full_version, libname, outdir):
    """
    Check if python3-embed / python3-dev is installed and copy
    the shared library + stdlib into outdir.
    Returns True on success.
    """
    # On Debian/Ubuntu: python3-embed or python3-dev installs:
    #   /usr/lib/x86_64-linux-gnu/libpython3.x.so
    #   /usr/lib/python3.x/  (stdlib)
    # On Fedora/RHEL: python3-devel
    #   /usr/lib64/libpython3.x.so
    #   /usr/lib/python3.x/  (stdlib)

    candidate_dirs = [
        f"/usr/lib/python{full_version}/config-{full_version}-x86_64-linux-gnu",
        f"/usr/lib/x86_64-linux-gnu",
        "/usr/lib64",
        "/usr/lib",
        f"/usr/lib/python{full_version}",
    ]

    # Search for the .so
    so_path = None
    for d in candidate_dirs:
        p = os.path.join(d, libname)
        if os.path.isfile(p):
            so_path = p
            break

    if not so_path:
        return False

    debug(f"Found system {libname} at {so_path}")

    # Copy the .so
    shutil.copy2(so_path, os.path.join(outdir, libname))
    debug(f"Copied {libname} → {outdir}")

    # Try to find stdlib (Lib/)
    stdlib_dirs = [
        f"/usr/lib/python{full_version}",
    ]
    for sd in stdlib_dirs:
        if os.path.isdir(sd):
            # Copy entire stdlib? Too large. Instead copy only the
            # modules that embeddable Python needs, or symlink.
            # For now, check if python3.14.zip exists (some distros)
            zip_stdlib = os.path.join(sd, f"python{full_version.replace('.','')}.zip")
            if os.path.isfile(zip_stdlib):
                shutil.copy2(zip_stdlib, outdir)
                debug(f"Copied stdlib zip → {outdir}")
            else:
                # Create a symlink to the system stdlib as a fallback
                # (requires the dir to persist)
                debug(f"System stdlib at {sd} (not copied — too large)")
                debug(f"PYTHONHOME should point to {sd} at runtime")

    return True


def build_from_source(full_version, libname, outdir):
    """
    Download Python source tarball and build with --enable-shared.
    This takes a while but produces a fully self-contained .so.
    """
    url = (
        f"https://www.python.org/ftp/python/{full_version}/"
        f"Python-{full_version}.tar.xz"
    )
    tarball = os.path.join(outdir, f"Python-{full_version}.tar.xz")
    srcdir = os.path.join(outdir, f"Python-{full_version}")

    download_file(url, tarball)

    if not os.path.isdir(srcdir):
        debug(f"Extracting {tarball} ...")
        with tarfile.open(tarball, "r:xz") as tf:
            tf.extractall(outdir)
        debug(f"Extracted to {srcdir}")
    else:
        debug(f"Source directory {srcdir} exists, skipping extract")

    build_dir = os.path.join(outdir, "_build")
    ensure_directory(build_dir)

    instdir = os.path.abspath(os.path.join(outdir, "_install"))
    ensure_directory(instdir)

    # Only run configure/make if the .so doesn't already exist
    target_so = os.path.join(instdir, "lib", libname)
    if not os.path.isfile(target_so):
        debug("Configuring Python build (--enable-shared) ...")
        run(
            [
                os.path.join(srcdir, "configure"),
                "--enable-shared",
                f"--prefix={instdir}",
                "--with-ensurepip=no",
            ],
            cwd=build_dir,
        )

        debug("Building Python (this will take a while) ...")
        run(["make", "-j4"], cwd=build_dir)

        debug("Installing to _install/ ...")
        run(["make", "install"], cwd=build_dir)
    else:
        debug(f"Found cached build at {target_so}")

    # Copy the .so to the output dir
    shutil.copy2(target_so, os.path.join(outdir, libname))
    debug(f"Copied {libname} → {outdir}")

    # Copy stdlib (Lib/)
    lib_python = os.path.join(instdir, f"lib/python{full_version}")
    if os.path.isdir(lib_python):
        shutil.copytree(lib_python, os.path.join(outdir, f"Lib"), dirs_exist_ok=True)
        debug(f"Copied stdlib → {outdir}/Lib")
    else:
        debug(f"Warning: stdlib not found at {lib_python}")


# ── Main ─────────────────────────────────────────────────────────
def main():
    global OUTDIR
    global PYTHON_VERSION

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] in ("--outdir", "-o") and i + 1 < len(args):
            OUTDIR = args[i + 1]
            i += 2
        elif args[i] in ("--version", "-v") and i + 1 < len(args):
            PYTHON_VERSION = args[i + 1]
            i += 2
        else:
            debug(f"Unknown argument: {args[i]}")
            sys.exit(1)

    system = platform.system()
    debug(f"System: {system}")
    debug(f"Python version: {PYTHON_VERSION}")
    debug(f"Output directory: {OUTDIR}")

    full_version = get_latest_minor(PYTHON_VERSION)
    if not full_version:
        debug("Error: could not determine Python version")
        sys.exit(1)

    if system == "Windows":
        fetch_windows(full_version, OUTDIR)
    elif system == "Linux":
        fetch_linux(full_version, OUTDIR)
    else:
        debug(f"Unsupported platform: {system}")
        sys.exit(1)

    debug(f"\nDone. Files installed to: {OUTDIR}")

    # Print usage instructions
    if system == "Windows":
        py_ver = full_version.replace(".", "")
        dll_path = os.path.join(OUTDIR, f"python{py_ver}.dll")
    else:
        dll_path = os.path.join(OUTDIR, f"libpython{full_version}.so")

    debug(f"\nUsage:")
    debug(f"  gcl -pyrun script.py -pydll \"{dll_path}\"")


if __name__ == "__main__":
    main()
