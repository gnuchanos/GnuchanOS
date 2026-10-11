"""Which icon names exist, what artwork each one uses, and in which context.

This is the table that decides what "the theme supports every program" means in
practice. A desktop asks for an icon by name — ``document-save``,
``applications-system``, ``text-plain``, ``emblem-important`` — and this module
answers with a glyph from :mod:`icons.glyphs_base` and a freedesktop context.

Names are grouped by glyph rather than listed one by one, because that is the
relationship that matters: forty different names are the same folder, and one
line here is what keeps them all in step. The list follows the Icon Naming
Specification plus the extras that GTK, Qt and the common desktops ask for in
practice, including every ``*-symbolic`` name, which are generated separately as
scalable SVGs because that is what a GTK style context recolours.
"""

from __future__ import annotations

from dataclasses import dataclass

# Contexts, in the order they are written to index.theme. The names are the
# ones the specification defines, so a tool that reads the theme's directories
# can tell an action from a file type without guessing.
ACTIONS = "Actions"
ANIMATIONS = "Animations"
APPLICATIONS = "Applications"
CATEGORIES = "Categories"
DEVICES = "Devices"
EMBLEMS = "Emblems"
EMOTES = "Emotes"
INTERNATIONAL = "International"
MIMETYPES = "MimeTypes"
PLACES = "Places"
STATUS = "Status"
UI = "UI"

CONTEXTS: tuple[str, ...] = (
    ACTIONS,
    ANIMATIONS,
    APPLICATIONS,
    CATEGORIES,
    DEVICES,
    EMBLEMS,
    EMOTES,
    INTERNATIONAL,
    MIMETYPES,
    PLACES,
    STATUS,
    UI,
)

# Contexts that also get a monochrome, scalable variant under the name
# "<icon>-symbolic". GTK asks for these when it draws a menu, a toolbar or a
# header bar button and recolours them to the widget's text colour, so they are
# the names a themed desktop actually hits most often.
SYMBOLIC_CONTEXTS: frozenset[str] = frozenset(
    {ACTIONS, APPLICATIONS, CATEGORIES, DEVICES, EMBLEMS, PLACES, STATUS, UI}
)

# Applications and MimeTypes are drawn at full colour in a file manager and in a
# launcher; everything else is a glyph. Both are in the catalogue so that a
# program with no icon of its own still gets one.
DEFAULT_CONTEXT_TINT: dict[str, str] = {}


@dataclass(frozen=True)
class IconEntry:
    """One icon name, the glyph that draws it, and where it belongs."""

    name: str
    glyph: str
    context: str
    symbolic: bool = False


_BRANCHES: list[tuple[str, dict[str, tuple[str, ...]]]] = []


def branch(context: str, glyph: str, *names: str) -> None:
    """Register ``names`` in ``context``, all drawn by the glyph ``glyph``.

    Branches accumulate. One glyph is registered many times on purpose — a
    folder is forty different icon names, and they are listed in the groups
    where they belong rather than crammed onto one line — so names are appended
    and never replace what an earlier call registered.
    """
    if not names:
        raise ValueError(f"{glyph!r} in {context!r} has no names")
    for existing_context, groups in _BRANCHES:
        if existing_context == context:
            groups[glyph] = groups.get(glyph, ()) + tuple(names)
            return
    _BRANCHES.append((context, {glyph: names}))


def registered(context: str) -> set[str]:
    """The names ``context`` already has.

    The desktop, program and extra tables overlap by design: ``dolphin`` is a
    KDE program and a file manager, ``openbox`` is a window manager and an LXDE
    component, and ``swayidle`` is both a tiling utility and a locker. A reader
    should not have to work out which module registered a shared name first, so
    a table that adds to a shared context asks this instead.
    """
    return {entry.name for entry in entries() if entry.context == context}


def alias(context: str, glyph: str, *names: str) -> None:
    """:func:`branch`, minus the names ``context`` already defines.

    A name written twice in one context is not two icons: it is one file and one
    silently overwritten sibling, which is why the build reports it. The tables
    that share names use this rather than keeping four lists in step by hand —
    and unlike :func:`branch`, it is a no-op when everything it was given is
    already there.
    """
    known = registered(context)
    fresh = [name for name in names if name not in known]
    if fresh:
        branch(context, glyph, *fresh)


def entries() -> list[IconEntry]:
    """Every icon the theme defines, in a stable order.

    What is de-duplicated here is the pair of context and name, not the name on
    its own: a name may legitimately belong to more than one context — a
    settings panel is both an action and a category, and a lookup is allowed to
    find it in either — and collapsing those would silently drop one of them.
    Within a single context the first registration still wins, because the file
    system cannot hold two files with the same name and a second one would
    overwrite the first.
    """
    result: list[IconEntry] = []
    seen: set[tuple[str, str]] = set()
    for context, groups in _BRANCHES:
        for glyph_name, names in groups.items():
            for name in names:
                key = (context, name)
                if key in seen:
                    continue
                seen.add(key)
                result.append(IconEntry(name, glyph_name, context))
    return result


def unknown_glyphs() -> list[str]:
    """Glyph names this catalogue refers to that no module defines.

    Called by the build and by --check: a typo here produces an icon that is
    never written, which is invisible until something asks for it.
    """
    from .glyphs_base import GLYPHS

    missing: list[str] = []
    for _context, groups in _BRANCHES:
        for glyph_name in groups:
            if glyph_name not in GLYPHS and glyph_name not in missing:
                missing.append(glyph_name)
    return sorted(missing)


def duplicate_names() -> list[str]:
    """Names registered twice in the same context, for the same check.

    Only a repeat within one context is a fault: those two registrations want
    the same file, so one drawing would silently replace the other. The same
    name in two contexts is two files in two directories, which is allowed.
    """
    counts: dict[tuple[str, str], int] = {}
    for context, groups in _BRANCHES:
        for names in groups.values():
            for name in names:
                key = (context, name)
                counts[key] = counts.get(key, 0) + 1
    return sorted({name for (_context, name), count in counts.items() if count > 1})


# Every icon name becomes a file name in the generated tree, so a name carrying
# a path separator or a wildcard is not a style problem: it is a build that
# stops halfway through with an OSError from the filesystem. Checking the shape
# of the name here turns that into an error that says which name is wrong.
_FORBIDDEN_IN_NAME = frozenset('*/\\:<>|?"')


def unusable_names() -> list[str]:
    """Names that could not be written as files, for the build to refuse early."""
    offenders: set[str] = set()
    for entry in entries():
        if not entry.name or any(
            character in _FORBIDDEN_IN_NAME for character in entry.name
        ):
            offenders.add(entry.name)
    return sorted(offenders)


def symbolic_entries() -> list[IconEntry]:
    """The ``*-symbolic`` names, for the contexts that carry them."""
    result: list[IconEntry] = []
    for entry in entries():
        if entry.context in SYMBOLIC_CONTEXTS:
            result.append(
                IconEntry(f"{entry.name}-symbolic", entry.glyph, entry.context, symbolic=True)
            )
    return result


def names() -> list[str]:
    """Every icon name, including the symbolic variants, each listed once."""
    collected = [entry.name for entry in entries()]
    collected += [entry.name for entry in symbolic_entries()]
    return list(dict.fromkeys(collected))


# --- Actions -----------------------------------------------------------------
# The toolbar and menu vocabulary. These names are what GTK, Qt, LibreOffice,
# and every GTK-based toolbox ask for, and the reason a menu looks themed at
# all: a missing action icon shows as an unthemed fallback in the middle of a
# themed row.

branch(ACTIONS, "document-new", "document-new")
branch(ACTIONS, "open", "document-open", "folder-open", "open")
branch(ACTIONS, "open", "document-open-recent")
branch(ACTIONS, "save", "document-save")
branch(ACTIONS, "save-as", "document-save-as")
branch(ACTIONS, "print", "document-print", "document-print-preview", "x-office-document-print")
branch(ACTIONS, "document", "document-properties", "document-edit", "document-page-setup")
branch(ACTIONS, "mail", "document-send", "mail-send", "mail-forward", "mail-reply-all", "mail-reply-sender")
branch(ACTIONS, "refresh", "document-revert", "view-reload")
branch(ACTIONS, "edit-copy", "edit-copy", "copy")
branch(ACTIONS, "edit-cut", "edit-cut", "cut")
branch(ACTIONS, "edit-paste", "edit-paste", "paste")
branch(ACTIONS, "edit-delete", "edit-delete", "edit-clear", "delete")
branch(ACTIONS, "search", "edit-find", "search", "system-search", "find")
branch(ACTIONS, "edit-find-replace", "edit-find-replace")
branch(ACTIONS, "edit-undo", "edit-undo")
branch(ACTIONS, "edit-redo", "edit-redo")
branch(ACTIONS, "edit-select-all", "edit-select-all", "edit-select-none")
branch(ACTIONS, "edit", "edit-rename", "text-editor", "document-edit-symbolic")
branch(ACTIONS, "window-close", "window-close", "close", "dialog-close")
branch(ACTIONS, "view-fullscreen", "window-maximize", "view-fullscreen", "zoom-fit-best")
branch(ACTIONS, "view-restore", "window-restore", "window-unmaximize", "view-restore")
branch(ACTIONS, "go-down", "window-minimize", "go-bottom")
branch(ACTIONS, "go-up", "go-up", "go-top")
branch(ACTIONS, "go-down", "go-down")
branch(ACTIONS, "go-next", "go-next", "go-forward", "go-last")
branch(ACTIONS, "go-previous", "go-previous", "go-back", "go-first")
branch(ACTIONS, "go-home", "go-home")
branch(ACTIONS, "go-jump", "go-jump")
branch(ACTIONS, "refresh", "view-refresh")
branch(ACTIONS, "zoom-in", "view-zoom-in", "zoom-in")
branch(ACTIONS, "zoom-out", "view-zoom-out", "zoom-out")
branch(ACTIONS, "open", "zoom-original")
branch(ACTIONS, "view-list", "view-list", "view-list-symbolic", "view-continuous")
branch(ACTIONS, "view-grid", "view-grid", "view-app-grid")
# The object-order names are the z-order operations, which reorder things the
# same way sorting does, so they are drawn with the same mark.
branch(ACTIONS, "sort", "view-sort-ascending", "view-sort-descending", "object-order-ascending", "object-order-descending", "object-order-front", "object-order-back", "object-order-raise", "object-order-lower")
branch(ACTIONS, "view-more", "view-more", "view-more-horizontal")
branch(ACTIONS, "list-add", "list-add", "list-new")
branch(ACTIONS, "list-remove", "list-remove", "list-remove-all")
branch(ACTIONS, "filter", "object-filter", "view-filter")
branch(ACTIONS, "link", "insert-link", "link", "edit-link", "emblem-link")
branch(ACTIONS, "edit-delete", "edit-clear-all")
branch(ACTIONS, "ok", "object-select", "edit-mark", "select")
branch(ACTIONS, "ok", "ok", "dialog-ok")
branch(ACTIONS, "cancel", "dialog-cancel", "cancel", "edit-unmark")
branch(ACTIONS, "refresh", "media-playlist-repeat")
branch(ACTIONS, "share", "media-playlist-shuffle")
branch(ACTIONS, "media-playback-start", "media-playback-start", "media-play", "player_play")
branch(ACTIONS, "media-playback-pause", "media-playback-pause", "media-pause", "player_pause")
branch(ACTIONS, "media-playback-stop", "media-playback-stop", "media-stop", "player_stop")
branch(ACTIONS, "media-skip-forward", "media-skip-forward", "player_end", "media-seek-forward")
branch(ACTIONS, "media-skip-backward", "media-skip-backward", "player_start", "media-seek-backward")
branch(ACTIONS, "ok", "object-select-all")
branch(ACTIONS, "edit", "object-rotate-left", "object-rotate-right")
branch(ACTIONS, "share", "object-flip-horizontal", "object-flip-vertical")
branch(ACTIONS, "grid", "format-list-unordered", "format-list-ordered", "format-list-checked")
branch(ACTIONS, "text", "format-text-bold", "format-text-italic", "format-text-underline", "format-text-strikethrough", "format-text-superscript", "format-text-subscript")
branch(ACTIONS, "bars", "format-justify-left", "format-justify-center", "format-justify-right", "format-justify-fill")
branch(ACTIONS, "text", "format-indent-less", "format-indent-more", "format-line-spacing", "format-font-size", "format-text-color", "format-fill-color", "format-stroke-color")
branch(ACTIONS, "star", "star-new", "non-starred", "semi-starred", "starred", "rating")
branch(ACTIONS, "bookmark", "bookmark-new", "bookmark-remove", "bookmark-add", "non-starred-symbolic")
branch(ACTIONS, "mark-location", "mark-location", "find-location", "location")
branch(ACTIONS, "folder", "folder-new", "folder-create")
branch(ACTIONS, "view-list", "tab-new", "tab-close", "tab-close-others", "tab-detach", "tab-move-left", "tab-move-right")
branch(ACTIONS, "applications-system", "preferences-system", "preferences-other")
branch(ACTIONS, "preferences-desktop", "preferences-desktop")
branch(ACTIONS, "applications-graphics", "preferences-desktop-theme", "preferences-color", "preferences-desktop-wallpaper", "preferences-desktop-appearance")
branch(ACTIONS, "preferences-desktop-font", "preferences-desktop-font", "preferences-desktop-locale", "preferences-desktop-keyboard", "preferences-desktop-keyboard-shortcuts", "preferences-desktop-accessibility")
branch(ACTIONS, "network", "preferences-system-network", "preferences-system-network-proxy", "preferences-system-network-sharing")
branch(ACTIONS, "clock", "preferences-system-time", "appointment-new", "appointment-soon", "x-office-appointment")
branch(ACTIONS, "preferences-system-windows", "preferences-system-windows", "preferences-system-windows-move", "preferences-system-windows-screen")
branch(ACTIONS, "utilities-terminal", "utilities-terminal", "terminal", "utilities-system-monitor")
branch(ACTIONS, "system-run", "system-run", "application-x-executable-launch")
branch(ACTIONS, "system-shutdown", "system-shutdown", "system-reboot")
branch(ACTIONS, "system-log-out", "system-log-out", "system-switch-user", "system-lock-screen")
branch(ACTIONS, "system-shutdown", "system-suspend", "system-hibernate")
branch(ACTIONS, "process-stop", "process-stop", "process-working")
branch(ACTIONS, "help", "help-about", "help-browser", "help-contents", "help-faq", "help-info")
branch(ACTIONS, "chat", "call-start", "call-stop", "call-missed")
branch(ACTIONS, "audio-volume-high", "audio-volume-high")
branch(ACTIONS, "audio-volume-low", "audio-volume-low", "audio-volume-medium", "audio-volume-muted")

# --- Places ------------------------------------------------------------------

branch(PLACES, "user-home", "user-home", "home", "folder-home")
branch(PLACES, "user-desktop", "user-desktop", "desktop")
branch(PLACES, "folder", "folder", "inode-directory", "folder-documents", "folder-download", "folder-music", "folder-pictures", "folder-videos", "folder-public", "folder-templates")
branch(PLACES, "user-trash", "user-trash", "trash-empty", "user-trash-full")
branch(PLACES, "bookmark", "user-bookmarks", "bookmarks")
branch(PLACES, "network-server", "network-server", "folder-remote")
branch(PLACES, "network-workgroup", "network-workgroup", "network-workgroup-symbolic")
branch(PLACES, "network", "network", "folder-network", "network-server-symbolic")

# --- Devices -----------------------------------------------------------------

branch(DEVICES, "computer", "computer", "video-display", "computer-symbolic")
branch(DEVICES, "laptop", "laptop")
branch(DEVICES, "phone", "phone", "phone-symbolic")
branch(DEVICES, "tablet", "tablet")
branch(DEVICES, "keyboard", "input-keyboard", "keyboard")
branch(DEVICES, "mouse", "input-mouse", "mouse")
branch(DEVICES, "input-tablet", "input-tablet", "tablet-input")
branch(DEVICES, "camera", "camera-photo", "camera")
branch(DEVICES, "camera-web", "camera-web", "webcam")
branch(DEVICES, "video-display", "video-display-symbolic")
branch(DEVICES, "tv", "tv", "video-television")
branch(DEVICES, "audio-headphones", "audio-headphones", "audio-headset")
branch(DEVICES, "audio-speakers", "audio-speakers")
branch(DEVICES, "scanner", "scanner")
branch(DEVICES, "printer", "printer", "printer-network")
branch(DEVICES, "drive-harddisk", "drive-harddisk", "drive-harddisk-symbolic")
branch(DEVICES, "drive-ssd", "drive-ssd")
branch(DEVICES, "drive-optical", "drive-optical", "media-optical", "media-cdrom")
branch(DEVICES, "drive-removable-media", "drive-removable-media", "media-removable", "drive-removable-media-usb")
branch(DEVICES, "media-flash", "media-flash", "media-flash-sd-mmc", "media-removable-flash")
branch(DEVICES, "media-floppy", "media-floppy")
branch(DEVICES, "usb", "usb", "media-usb")
branch(DEVICES, "memory", "media-memory", "memory")
branch(DEVICES, "cpu", "cpu", "computer-cpu")

# --- Status ------------------------------------------------------------------

branch(STATUS, "dialog-information", "dialog-information", "dialog-info", "info")
branch(STATUS, "dialog-warning", "dialog-warning", "warning", "dialog-warning-symbolic")
branch(STATUS, "dialog-error", "dialog-error", "error", "dialog-error-symbolic")
branch(STATUS, "dialog-question", "dialog-question", "question", "system-help")
branch(STATUS, "security-high", "security-high", "channel-secure")
branch(STATUS, "security-medium", "security-medium", "channel-insecure")
branch(STATUS, "security-low", "security-low")
branch(STATUS, "user-available", "user-available", "presence-available")
branch(STATUS, "user-away", "user-away", "presence-away")
branch(STATUS, "user-busy", "user-busy", "presence-busy")
branch(STATUS, "user-offline", "user-offline", "presence-offline", "user-status")
branch(STATUS, "user", "user", "avatar-default", "stock_person")
branch(STATUS, "user-group", "user-group", "system-users")
branch(STATUS, "battery-full", "battery-full", "battery-level-100", "battery-level-90")
branch(STATUS, "battery-good", "battery-good", "battery-level-50", "battery-level-70")
branch(STATUS, "battery-low", "battery-low", "battery-level-10", "battery-level-20")
branch(STATUS, "battery-charging", "battery-charging", "battery-ac-adapter")
branch(STATUS, "network-wireless", "network-wireless", "network-wireless-hotspot", "network-wireless-encrypted")
branch(STATUS, "network-wired", "network-wired", "network-wired-disconnected")
branch(STATUS, "bluetooth", "bluetooth", "bluetooth-active")
branch(STATUS, "image-loading", "image-loading", "process-working-symbolic")
branch(STATUS, "image-missing", "image-missing", "missing-image")
branch(STATUS, "audio-volume-high", "audio-volume-high-symbolic")
branch(STATUS, "microphone", "audio-input-microphone", "microphone", "audio-input-microphone-muted")
branch(STATUS, "emblem-default", "emblem-default")
branch(STATUS, "emblem-important", "emblem-important")
branch(STATUS, "emblem-favorite", "emblem-favorite")
branch(STATUS, "emblem-photos", "emblem-photos")
branch(STATUS, "emblem-documents", "emblem-documents")
branch(STATUS, "emblem-music", "emblem-music")
branch(STATUS, "emblem-videos", "emblem-videos")
branch(STATUS, "emblem-readonly", "emblem-readonly")
branch(STATUS, "emblem-synchronizing", "emblem-synchronizing")
branch(STATUS, "emblem-shared", "emblem-shared")
branch(STATUS, "emblem-system", "emblem-system")

# --- the gap fill ---------------------------------------------------------------
# The names the Icon Naming Specification defines that the tables above had not
# reached. They are registered with :func:`alias` rather than :func:`branch` so
# that a name another table already claimed keeps the picture it already has, and
# so that a later table adding to a shared context cannot silently overwrite an
# earlier one. The point of this section is coverage, not a second opinion about
# the artwork.

# --- Actions: the rest of the specification's action list -----------------------
# The z-order, playlist and remote-document names, each pointed at the mark the
# theme already draws for that idea.

alias(ACTIONS, "bookmark", "bookmark_add")
alias(ACTIONS, "user", "contact-new", "user-info")
alias(ACTIONS, "emblem-export", "document-export")
alias(ACTIONS, "download", "document-import")
alias(ACTIONS, "network", "document-open-remote")
alias(ACTIONS, "print", "document-print-direct", "document-print-preview")
alias(ACTIONS, "selection-mode", "edit-select-region")
alias(ACTIONS, "selection-start", "selection-start")
alias(ACTIONS, "selection-end", "selection-end")
alias(ACTIONS, "folder-copy", "folder-copy")
alias(ACTIONS, "folder-move", "folder-move")
alias(ACTIONS, "media-eject", "media-eject")
alias(ACTIONS, "microphone", "media-record")
alias(
    ACTIONS,
    "refresh",
    "media-playlist-consecutive",
    "media-playlist-no-repeat",
    "media-playlist-repeat-song",
)
alias(ACTIONS, "object-group", "object-group")
alias(ACTIONS, "object-ungroup", "object-ungroup")
alias(ACTIONS, "edit", "object-straighten")
alias(ACTIONS, "system-software-install", "system-software-update", "system-upgrade")
alias(ACTIONS, "text", "tools-check-spelling")
alias(ACTIONS, "view-conceal", "view-conceal")
alias(ACTIONS, "view-reveal", "view-reveal")
alias(ACTIONS, "pan-start", "format-indent-less")
alias(ACTIONS, "pan-end", "format-indent-more")

# --- Places: the specification's remaining place names --------------------------
# The user directories, the libraries a media application lists, and the marks a
# launcher's places menu shows.

alias(
    PLACES,
    "folder",
    "folder-downloads",
    "folder-publicshare",
    "folder-saved-search",
    "folder-visiting",
    "folder-drag-accept",
    "folder-burn",
    "library-places",
)
alias(PLACES, "clock", "folder-recent", "recent")
alias(PLACES, "applications-multimedia", "library-audio", "library-music")
alias(PLACES, "book", "library-books")
alias(PLACES, "image-x-generic", "library-pictures")
alias(PLACES, "video-x-generic", "library-videos")
alias(PLACES, "software-centre", "library-software")
alias(PLACES, "applications-internet", "applications-internet")
alias(PLACES, "applications-accessories", "applications-other")
alias(PLACES, "applications-system", "applications-system")
alias(PLACES, "system-file-manager", "file-manager")

# --- Devices: the specification's remaining device names ------------------------
# One line per picture, many names per line: a hard disk is a hard disk whether
# it is solid state, on the system bus or in a USB enclosure, and a DVD is a DVD
# whether it holds a film or an audio disc.

alias(DEVICES, "drive-ssd", "drive-harddisk-solidstate")
alias(
    DEVICES,
    "drive-harddisk",
    "drive-harddisk-usb",
    "drive-harddisk-ieee1394",
    "drive-harddisk-system",
    "drive-multidisk",
)
alias(
    DEVICES,
    "drive-removable-media",
    "drive-removable-media-usb-pendrive",
    "drive-removable-media-ieee1394",
    "media-removable-media",
    "media-removable-zip",
    "media-removable-optical",
    "media-zip",
    "media-jaz",
    "media-mo",
)
alias(DEVICES, "media-flash", "drive-removable-media-flash")
alias(
    DEVICES,
    "media-optical",
    "media-cdrom-audio",
    "media-cdrom-data",
    "media-dvd",
    "media-optical-bd",
    "media-optical-dvd",
    "media-optical-cd-audio",
    "media-optical-cd-video",
)
alias(DEVICES, "media-tape", "media-tape")
alias(DEVICES, "audio-card", "audio-card")
alias(DEVICES, "audio-speakers", "audio-speakers-bluetooth")
alias(DEVICES, "camera", "camera-photo-burst")
alias(DEVICES, "camera-video", "camera-video")
alias(DEVICES, "phone", "phone-pda")
alias(DEVICES, "pda", "pda")
alias(DEVICES, "ipod", "ipod")
alias(DEVICES, "video-player", "multimedia-player")
alias(DEVICES, "video-projector", "video-projector")
alias(DEVICES, "input-gaming", "input-gaming")
alias(DEVICES, "input-dialpad", "input-dialpad")
alias(DEVICES, "keyboard", "input-keyboard-virtual")
alias(DEVICES, "modem", "modem")
alias(DEVICES, "ups", "ups")
alias(DEVICES, "network-router", "network-router", "network-wireless-router")
alias(DEVICES, "smartcard", "smartcard")
alias(DEVICES, "usb-drive", "usb-drive")
alias(DEVICES, "usb-hub", "usb-hub")
alias(DEVICES, "laptop", "computer-laptop")
alias(DEVICES, "tablet", "computer-tablet")
alias(DEVICES, "computer-server", "computer-server")
alias(DEVICES, "computer-workstation", "computer-workstation")
alias(DEVICES, "microphone", "audio-input-microphone")
alias(DEVICES, "printer", "printer-multifunction")


# --- Status: the busses, the batteries and the badges ---------------------------
# The Status context is the one a desktop asks for most often while it is
# running: a battery level, a signal strength, a package state, a weather mark.
# Every one of them is a name a panel looks up on a timer, so a missing one is
# not a blank square nobody notices — it is a blank square in the corner of the
# screen, redrawn every second.

alias(STATUS, "battery-low", "battery-missing", "battery-empty", "battery-caution")


def _battery_glyph(level: int) -> str:
    """The picture a battery charge of ``level`` percent should be drawn with.

    The artwork has three fill levels rather than eleven, so the ladder below
    steps between them: a nearly flat cell, a half-full one and a full one. That
    is the right resolution for a mark nineteen pixels wide in a panel, and it is
    why the names that mean "no charge at all" — ``battery-empty``,
    ``battery-level-0``, ``battery-000`` and ``battery-missing`` — draw the same
    nearly flat cell as ``battery-low``.
    """
    if level <= 30:
        return "battery-low"
    if level <= 70:
        return "battery-good"
    return "battery-full"


# The two spellings panels use: ``battery-level-40`` from the specification and
# ``battery-040`` from the icon themes that came before it. Both are generated
# from one loop so the two ladders cannot drift apart.
for _level in range(0, 101, 10):
    alias(
        STATUS,
        _battery_glyph(_level),
        f"battery-level-{_level}",
        f"battery-{_level:03d}",
    )

# NetworkManager's own vocabulary. The device names are the type of interface,
# the signal names are the five bars, and the tech names are the radio the
# connection is carried over.
alias(STATUS, "network-wired", "nm-device-wired", "nm-device-wired-secure")
alias(STATUS, "network-wireless", "nm-device-wireless", "nm-device-wireless-secure", "nm-adhoc")
alias(STATUS, "phone", "nm-device-wwan")
alias(STATUS, "bluetooth", "nm-device-bt")

_SIGNAL_GLYPHS: tuple[tuple[int, str], ...] = (
    (0, "network-wireless-signal-none"),
    (25, "network-wireless-signal-weak"),
    (50, "network-wireless-signal-ok"),
    (75, "network-wireless-signal-good"),
    (100, "network-wireless-signal-excellent"),
)

for _level, _glyph_name in _SIGNAL_GLYPHS:
    alias(STATUS, _glyph_name, f"nm-signal-{_level:02d}")

alias(STATUS, "network-wireless-signal-weak", "nm-signal-weak")
alias(STATUS, "network-wireless-signal-ok", "nm-signal-ok")
alias(STATUS, "network-wireless-signal-good", "nm-signal-good")
alias(STATUS, "network-wireless-signal-excellent", "nm-signal-excellent")

alias(STATUS, "lock", "nm-vpn-active-lock", "nm-vpn-standalone-lock", "nm-secure-lock")
alias(STATUS, "network", "nm-vpn-offline")

# The twelve connecting frames are the same spinner the Animations context
# carries, named the way NetworkManager names them.
for _step in range(1, 13):
    alias(STATUS, "image-loading", f"nm-vpn-connecting{_step:02d}")

alias(STATUS, "network-wireless-signal-none", "nm-no-connection")
alias(STATUS, "network-wireless", "nm-dialup")
alias(STATUS, "phone", "nm-tech-3g", "nm-tech-4g", "nm-tech-5g")
alias(STATUS, "network-wireless", "nm-tech-gprs", "nm-tech-edge", "nm-tech-hspa", "nm-tech-lte", "nm-tech-umts")

# --- Status: the network marks the specification defines ------------------------

alias(STATUS, "network-wireless", "network-wireless-offline", "network-wireless-disconnected")
alias(STATUS, "network-wired", "network-wired-offline")
alias(STATUS, "dialog-error", "network-error")
alias(STATUS, "network", "network-offline", "network-idle")
alias(STATUS, "go-up", "network-transmit")
alias(STATUS, "go-down", "network-receive")
alias(STATUS, "refresh", "network-transmit-receive")

alias(STATUS, "bluetooth", "bluetooth-paired", "bluetooth-disabled")

alias(STATUS, "audio-volume-low", "audio-volume-muted-blocking")
alias(
    STATUS,
    "microphone",
    "microphone-sensitivity-muted",
    "microphone-sensitivity-low",
    "microphone-sensitivity-medium",
    "microphone-sensitivity-high",
)

alias(STATUS, "user-away", "user-away-extended", "user-idle", "user-status-pending")
alias(STATUS, "user-offline", "user-invisible")
alias(STATUS, "lock", "dialog-password")

alias(
    STATUS,
    "system-software-install",
    "software-update-available",
    "software-update-urgent",
)
alias(
    STATUS,
    "package-x-generic",
    "package-available",
    "package-installed-updated",
    "package-installed-outdated",
    "package-broken",
    "package-downgrade",
    "package-new",
    "package-purge",
    "package-reinstall",
    "package-supported",
    "package-upgrade",
)
alias(STATUS, "printer", "printer-error", "printer-printing", "printer-warning")
alias(STATUS, "printer-network", "printer-network-error")

alias(STATUS, "checkbox-checked", "checkbox-checked")
alias(STATUS, "checkbox-mixed", "checkbox-mixed")
alias(STATUS, "checkbox-unchecked", "checkbox-unchecked")
alias(STATUS, "radio-checked", "radio-checked")
alias(STATUS, "radio-mixed", "radio-mixed")
alias(STATUS, "radio-unchecked", "radio-unchecked")
alias(STATUS, "star", "starred", "non-starred", "semi-starred")

# The weather marks, which the artwork has carried since the first pass and
# which nothing had asked for by name.
_WEATHER: tuple[str, ...] = (
    "clear",
    "clear-night",
    "clouds",
    "few-clouds",
    "fog",
    "overcast",
    "showers",
    "showers-scattered",
    "snow",
    "storm",
    "severe-alert",
)

for _weather in _WEATHER:
    alias(STATUS, f"weather-{_weather}", f"weather-{_weather}")

alias(STATUS, "clock", "alarm", "appointment-missed", "task-due", "task-past-due")


# The five signal names as names of their own. ``nm-signal-25`` and
# ``network-wireless-signal-weak`` are the same picture under the two spellings
# two different panels use, and a theme that only answered the first would leave
# the second to hicolor.
for _signal_level, _signal_glyph in _SIGNAL_GLYPHS:
    alias(STATUS, _signal_glyph, _signal_glyph)
