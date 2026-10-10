#!/usr/bin/env python3
# =============================================================================
# gpu_menu.py - give the ISO's boot menus one entry per graphics driver.
#
# Each extra entry is a COPY of the ISO's own first entry with one thing
# changed: a `gnuchan.gpu=` word appended to its kernel command line. The live
# system reads that word from /proc/cmdline while it boots and brings up the
# matching driver.
#
#     Live System - NVIDIA (new)      gnuchan.gpu=nvidia-new
#     Live System - NVIDIA (legacy)   gnuchan.gpu=nvidia-legacy
#     Live System - AMD               gnuchan.gpu=amd
#     Live System - Intel             gnuchan.gpu=intel
#
# License: GPL3
# =============================================================================

from __future__ import annotations

import re
import sys
from pathlib import Path

GPU_CHOICES = (
    ("Live System - NVIDIA (new)",    "gnuchan.gpu=nvidia-new"),
    ("Live System - NVIDIA (legacy)", "gnuchan.gpu=nvidia-legacy"),
    ("Live System - AMD",             "gnuchan.gpu=amd"),
    ("Live System - Intel",           "gnuchan.gpu=intel"),
)

GPU_PARAMETER = "gnuchan.gpu="

ISOLINUX_LABEL = re.compile(r"^\s*label\s+(\S+)\s*$")
ISOLINUX_MENU_LABEL = re.compile(r"^(\s*)menu label\s+(.*)$")
ISOLINUX_MENU_DEFAULT = re.compile(r"^\s*menu default\s*$")
ISOLINUX_APPEND = re.compile(r"^(\s*)append\s+(.*)$")

GRUB_MENUENTRY = re.compile(r"^\s*menuentry\b")
GRUB_LINUX = re.compile(r"^(\s*(?:linux|linuxefi)\s+\S+)(\s+.*)?$")
GRUB_FIRST_QUOTED = re.compile(r"""(["\']).*?\1""")


def log(message: str) -> None:
    print(f"==> gpu-menu: {message}", flush=True)


def warn(message: str) -> None:
    print(f"  ! gpu-menu: {message}", file=sys.stderr, flush=True)


def isolinux_blocks(lines: list[str]) -> list[list[str]]:
    blocks: list[list[str]] = []
    current: list[str] = []
    for line in lines:
        if ISOLINUX_LABEL.match(line):
            if current:
                blocks.append(current)
            current = [line]
        elif current:
            current.append(line)
    if current:
        blocks.append(current)
    return blocks


def clone_isolinux_block(block: list[str], title: str, parameter: str,
                         suffix: str) -> list[str]:
    copied: list[str] = []
    for line in block:
        match = ISOLINUX_LABEL.match(line)
        if match:
            copied.append(line.replace(match.group(1), match.group(1) + suffix, 1))
            continue
        if ISOLINUX_MENU_DEFAULT.match(line):
            continue
        menu_label = ISOLINUX_MENU_LABEL.match(line)
        if menu_label:
            copied.append(f"{menu_label.group(1)}menu label {title}\n")
            continue
        append = ISOLINUX_APPEND.match(line)
        if append:
            body = append.group(2).rstrip("\n").rstrip()
            copied.append(f"{append.group(1)}append {body} {parameter}\n")
            continue
        copied.append(line)
    return copied


def edit_isolinux(text: str, suffix_base: str) -> tuple[str, int]:
    if GPU_PARAMETER in text:
        return text, 0
    lines = text.splitlines(keepends=True)
    blocks = isolinux_blocks(lines)
    if not blocks:
        return text, 0

    first = blocks[0]
    head = lines[: lines.index(first[0])]
    tail: list[str] = []
    for block in blocks[1:]:
        tail.extend(block)

    added: list[list[str]] = []
    for index, (title, parameter) in enumerate(GPU_CHOICES):
        added.append(clone_isolinux_block(first, title, parameter,
                                          f"{suffix_base}{index + 1}"))

    body: list[str] = []
    for block in added:
        body.extend(block)
    return "".join(head + first + body + tail), len(added)


def grub_block_spans(lines: list[str]) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    index = 0
    while index < len(lines):
        if not GRUB_MENUENTRY.match(lines[index]):
            index += 1
            continue
        depth = 0
        opened = False
        end = index
        while end < len(lines):
            depth += lines[end].count("{")
            if "{" in lines[end]:
                opened = True
            depth -= lines[end].count("}")
            if opened and depth <= 0:
                break
            end += 1
        spans.append((index, min(end, len(lines) - 1)))
        index = end + 1
    return spans


def clone_grub_block(block: list[str], title: str, parameter: str) -> list[str]:
    copied: list[str] = []
    for line in block:
        if GRUB_MENUENTRY.match(line):
            copied.append(GRUB_FIRST_QUOTED.sub(f'"{title}"', line, count=1))
            continue
        linux = GRUB_LINUX.match(line)
        if linux:
            rest = (linux.group(2) or "").rstrip("\n").rstrip()
            copied.append(f"{linux.group(1)}{rest} {parameter}\n")
            continue
        copied.append(line)
    return copied


def edit_grub(text: str) -> tuple[str, int]:
    if GPU_PARAMETER in text:
        return text, 0
    lines = text.splitlines(keepends=True)
    spans = grub_block_spans(lines)
    if not spans:
        return text, 0

    start, end = spans[0]
    clone = [line for line in lines[start:end + 1]]
    before = lines[:end + 1]
    after = lines[end + 1:]

    added: list[str] = []
    for title, parameter in GPU_CHOICES:
        added.extend(clone_grub_block(clone, title, parameter))

    return "".join(before + added + after), len(GPU_CHOICES)


def find_isolinux_config(root: Path) -> Path | None:
    directory = root / "isolinux"
    if not directory.is_dir():
        return None
    preferred = directory / "live.cfg"
    if preferred.is_file():
        return preferred
    for candidate in sorted(directory.glob("*.cfg")):
        try:
            text = candidate.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "label" in text and "append" in text:
            return candidate
    return None


def find_grub_config(root: Path) -> Path | None:
    candidate = root / "boot" / "grub" / "grub.cfg"
    return candidate if candidate.is_file() else None


def edit_file(path: Path, editor) -> int:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        warn(f"could not read {path}: {error}")
        return 0
    updated, count = editor(text)
    if count == 0:
        return 0
    try:
        path.write_text(updated, encoding="utf-8")
    except OSError as error:
        warn(f"could not write {path}: {error}")
        return 0
    log(f"added {count} entry(ies) to {path}")
    return count


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

    total = 0
    isolinux = find_isolinux_config(root)
    if isolinux is not None:
        total += edit_file(isolinux, lambda text: edit_isolinux(text, "-gpu"))
    else:
        log("no ISOLINUX configuration found; the BIOS menu is left as it is")

    grub = find_grub_config(root)
    if grub is not None:
        total += edit_file(grub, edit_grub)
    else:
        log("no GRUB configuration found; the EFI menu is left as it is")

    if total == 0:
        log("nothing added; the menus already name the GPU entries or have no entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
