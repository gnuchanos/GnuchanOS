"""The names that belong to the contexts the specification defines and the
theme had left empty, plus the stock ids the old toolkits ask for.

Five contexts — ``Emblems``, ``Emotes``, ``International``, ``UI`` and
``Animations`` — are listed by :mod:`icons.catalogue` and were populated by
nothing, so the generated ``index.theme`` had no directory for them at all. A
desktop that asks for ``emblem-important`` in the emblem context, or for
``window-close`` in the UI context, found nothing there and fell back to
whatever hicolor had.

The other half of the table is the GTK stock vocabulary: ``gtk-save``,
``gtk-find``, ``gtk-media-play`` and the rest of the ids a program written
against GTK 2 still puts in its ``Icon=`` line. They are aliases onto the
artwork the theme already has, so a program that has not been updated in fifteen
years draws the same picture as one that has.
"""

from __future__ import annotations

from .catalogue import (
    ACTIONS,
    ANIMATIONS,
    EMBLEMS,
    EMOTES,
    INTERNATIONAL,
    STATUS,
    UI,
    branch,
)
from .glyphs_flags import FLAGS

# --- Emblems -----------------------------------------------------------------
# The badges a file manager lays over a file. Every one of these is also written
# to Status, which is where the theme used to keep them, because a lookup is
# allowed to find a name in either and dropping one of the two would break the
# desktops that ask for the other.
#
# Most of the badges are artwork that already existed for another purpose — the
# padlock, the globe, the envelope, the chain link — and pointing at those is
# what keeps a badge and the thing it stands for recognisably the same object.

branch(EMBLEMS, "mail", "emblem-mail")
branch(EMBLEMS, "download", "emblem-downloads", "emblem-import")
branch(EMBLEMS, "emblem-export", "emblem-export")
branch(EMBLEMS, "link", "emblem-symbolic-link")
branch(EMBLEMS, "emblem-synchronizing", "emblem-synchronized", "emblem-synchronizing")
branch(EMBLEMS, "applications-internet", "emblem-web")
branch(EMBLEMS, "dialog-warning", "emblem-warning")
branch(EMBLEMS, "lock", "emblem-locked")
branch(EMBLEMS, "user-desktop", "emblem-desktop")
branch(EMBLEMS, "emblem-documents", "emblem-documents")
branch(EMBLEMS, "emblem-music", "emblem-music")
branch(EMBLEMS, "emblem-photos", "emblem-photos")
branch(EMBLEMS, "emblem-videos", "emblem-videos")
branch(EMBLEMS, "emblem-default", "emblem-default")
branch(EMBLEMS, "emblem-important", "emblem-important")
branch(EMBLEMS, "emblem-favorite", "emblem-favorite")
branch(EMBLEMS, "emblem-shared", "emblem-shared")
branch(EMBLEMS, "emblem-readonly", "emblem-readonly")
branch(EMBLEMS, "emblem-system", "emblem-system")
branch(EMBLEMS, "emblem-generic", "emblem-generic")
branch(EMBLEMS, "emblem-new", "emblem-new")
branch(EMBLEMS, "emblem-unreadable", "emblem-unreadable")
branch(EMBLEMS, "emblem-unlocked", "emblem-unlocked")
branch(EMBLEMS, "emblem-raised", "emblem-raised", "emblem-dropbox")
branch(EMBLEMS, "emblem-personal", "emblem-personal")
branch(EMBLEMS, "emblem-urgent", "emblem-urgent")
branch(EMBLEMS, "emblem-success", "emblem-success")
branch(EMBLEMS, "emblem-error", "emblem-error")
branch(EMBLEMS, "emblem-encrypted", "emblem-encrypted")

# The badges that live only in Emblems are repeated in Status, so a file manager
# that looks a badge up in the wrong context still finds it.
branch(STATUS, "emblem-generic", "emblem-generic")
branch(STATUS, "emblem-new", "emblem-new")
branch(STATUS, "emblem-unreadable", "emblem-unreadable")
branch(STATUS, "emblem-unlocked", "emblem-unlocked")
branch(STATUS, "emblem-raised", "emblem-raised", "emblem-dropbox")
branch(STATUS, "emblem-personal", "emblem-personal")
branch(STATUS, "emblem-urgent", "emblem-urgent")
branch(STATUS, "emblem-success", "emblem-success")
branch(STATUS, "emblem-error", "emblem-error")
branch(STATUS, "emblem-encrypted", "emblem-encrypted")

# The Apple-style names some applications still ask for. They are not emblems,
# but they are the same kind of legacy: a name that means an action the theme
# already draws, spelled the way one toolkit spelled it twenty years ago.
branch(EMBLEMS, "document-new", "stock_new")
branch(EMBLEMS, "save", "stock_save")
branch(EMBLEMS, "open", "stock_open")
branch(EMBLEMS, "emblem-shared", "stock_shared")

# --- Emotes ------------------------------------------------------------------
# One face shape and one helper per expression: the disc is shared, the eyes and
# the mouth are the only things that differ, which is what makes twenty-four
# expressions read as one set in a chat log rather than as twenty-four icons that
# happen to be round.
#
# The ``emote-`` and ``stock_smiley`` spellings are the ones older GTK clients
# and Pidgin ask for; they point at the same drawings as the ``face-`` names.

_FACES: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("face-smile", ("face-smile", "emote-smile", "stock_smiley-1")),
    ("face-sad", ("face-sad", "emote-sad", "stock_smiley-2")),
    ("face-plain", ("face-plain", "emote-plain", "stock_smiley-3")),
    ("face-wink", ("face-wink", "emote-wink")),
    ("face-laugh", ("face-laugh", "emote-laugh")),
    ("face-smile-big", ("face-smile-big", "emote-happy", "emote-smile-big")),
    ("face-surprise", ("face-surprise", "emote-surprise", "stock_smiley-4")),
    ("face-uncertain", ("face-uncertain", "emote-uncertain")),
    ("face-angel", ("face-angel", "emote-angel")),
    ("face-angry", ("face-angry", "emote-angry", "stock_smiley-5")),
    ("face-cool", ("face-cool", "emote-cool", "stock_smiley-6")),
    ("face-crying", ("face-crying", "emote-crying", "stock_smiley-7")),
    ("face-devilish", ("face-devilish", "emote-devilish")),
    ("face-embarrassed", ("face-embarrassed", "emote-embarrassed")),
    ("face-kiss", ("face-kiss", "emote-kiss")),
    ("face-raspberry", ("face-raspberry", "emote-raspberry", "stock_smiley-8")),
    ("face-sick", ("face-sick", "emote-sick")),
    ("face-smirk", ("face-smirk", "emote-smirk")),
    ("face-tired", ("face-tired", "emote-tired", "stock_smiley-9")),
    ("face-worried", ("face-worried", "emote-worried")),
    ("face-glasses", ("face-glasses", "emote-glasses")),
    ("face-ninja", ("face-ninja", "emote-ninja")),
    ("face-yawn", ("face-yawn", "emote-yawn")),
    ("face-monkey", ("face-monkey", "emote-monkey")),
)

for _glyph_name, _names in _FACES:
    branch(EMOTES, _glyph_name, *_names)

# --- International -----------------------------------------------------------
# Locale, character set and flags. The locale marks are the translation glyph the
# theme already draws, and the flags are generated from the band table in
# :mod:`icons.glyphs_flags`, so a language menu shows a flag whether or not
# anybody drew that country by hand.

branch(INTERNATIONAL, "translation", "locale", "language", "intl-locale")
branch(INTERNATIONAL, "font-x-generic", "charset")
branch(INTERNATIONAL, "keyboard", "input-method")

# A flag is one name and one glyph, both spelled after the ISO 3166-1 code, so
# the table below is walked rather than written out: a country added to the
# artwork table appears here without anyone remembering to add it twice.
for _code in sorted(FLAGS):
    branch(INTERNATIONAL, f"flag-{_code}", f"flag-{_code}")

# --- UI ----------------------------------------------------------------------
# The names that belong to a widget rather than to a document. The specification
# puts them in UI, and a theme that only writes them to Actions is the reason a
# GTK 4 header bar sometimes falls back: GTK 4 looks for them here first.

branch(UI, "window-close", "window-close", "window-close-others")
branch(UI, "go-down", "window-minimize")
branch(UI, "view-fullscreen", "window-maximize")
branch(UI, "view-restore", "window-restore", "window-unmaximize")
branch(UI, "window-new", "window-new")

branch(UI, "pan-down", "pan-down")
branch(UI, "pan-up", "pan-up")
branch(UI, "pan-start", "pan-start")
branch(UI, "pan-end", "pan-end")

branch(UI, "zoom-in", "zoom-in")
branch(UI, "zoom-out", "zoom-out")
branch(UI, "open", "zoom-original")
branch(UI, "view-more", "view-more", "view-more-horizontal")
branch(UI, "view-list", "view-list", "view-continuous")
branch(UI, "view-grid", "view-grid")

branch(UI, "list-drag-handle", "list-drag-handle")
branch(UI, "tab-drag", "tab-drag")
branch(UI, "sidebar-show", "sidebar-show")
branch(UI, "sidebar-hide", "sidebar-hide")
branch(UI, "sidebar-show-right", "sidebar-show-right")
branch(UI, "sidebar-hide-right", "sidebar-hide-right")
branch(UI, "slider-handle", "slider-handle")
branch(UI, "selection-mode", "selection-mode")
branch(UI, "open-menu", "open-menu")
branch(UI, "font-selector", "font-selector")
branch(UI, "color-selector", "color-selector")

# --- Animations --------------------------------------------------------------
# One animation, named the way every toolkit names it, plus its frames as names
# of their own: a toolkit that cannot animate a GIF can step through the static
# frames instead, and the two are guaranteed to be the same drawing because both
# come out of :mod:`icons.glyphs_ui`.

branch(
    ANIMATIONS,
    "process-working",
    "process-working",
    "spinner",
    "loading",
    "throbber",
    "activity-indicator",
)

for _step in range(1, 37):
    _frame = f"process-working-{_step:02d}"
    branch(ANIMATIONS, _frame, _frame)

# --- Actions: the GTK stock vocabulary ---------------------------------------
# Every id a program written against GTK 2 still names. They are aliases onto
# the artwork the theme has, not second drawings: ``gtk-save`` and
# ``document-save`` are the same floppy disk because they are the same action,
# and a menu that mixes the two should not look like it came from two themes.

branch(ACTIONS, "help", "gtk-about", "gtk-help", "gtk-info")
branch(ACTIONS, "list-add", "gtk-add")
branch(ACTIONS, "ok", "gtk-apply", "gtk-ok", "gtk-yes")
branch(ACTIONS, "text", "gtk-bold", "gtk-italic", "gtk-underline", "gtk-strikethrough", "gtk-spell-check")
branch(ACTIONS, "cancel", "gtk-cancel", "gtk-no", "gtk-disconnect")
branch(ACTIONS, "drive-optical", "gtk-cdrom")
branch(ACTIONS, "edit-delete", "gtk-clear", "gtk-delete", "gtk-remove")
branch(ACTIONS, "window-close", "gtk-close", "gtk-quit")
branch(ACTIONS, "panel-color", "gtk-color-picker", "gtk-select-color")
branch(ACTIONS, "link", "gtk-connect")
branch(ACTIONS, "refresh", "gtk-convert", "gtk-refresh", "gtk-revert-to-saved")
branch(ACTIONS, "edit-copy", "gtk-copy")
branch(ACTIONS, "edit-cut", "gtk-cut")
branch(ACTIONS, "dialog-question", "gtk-dialog-authentication")
branch(ACTIONS, "dialog-error", "gtk-dialog-error")
branch(ACTIONS, "dialog-information", "gtk-dialog-info")
branch(ACTIONS, "dialog-question", "gtk-dialog-question")
branch(ACTIONS, "dialog-warning", "gtk-dialog-warning")
branch(ACTIONS, "folder", "gtk-directory")
branch(ACTIONS, "edit", "gtk-edit")
branch(ACTIONS, "system-run", "gtk-execute")
branch(ACTIONS, "document", "gtk-file", "gtk-properties")
branch(ACTIONS, "search", "gtk-find", "gtk-zoom-100")
branch(ACTIONS, "edit-find-replace", "gtk-find-and-replace")
branch(ACTIONS, "media-floppy", "gtk-floppy")
branch(ACTIONS, "view-fullscreen", "gtk-fullscreen", "gtk-zoom-fit")
branch(ACTIONS, "go-down", "gtk-goto-bottom", "gtk-go-down")
branch(ACTIONS, "go-previous", "gtk-goto-first", "gtk-go-back")
branch(ACTIONS, "go-next", "gtk-goto-last", "gtk-go-forward")
branch(ACTIONS, "go-up", "gtk-goto-top", "gtk-go-up")
branch(ACTIONS, "drive-harddisk", "gtk-harddisk")
branch(ACTIONS, "go-home", "gtk-home")
branch(ACTIONS, "view-list", "gtk-index")
branch(ACTIONS, "go-jump", "gtk-jump-to")
branch(ACTIONS, "bars", "gtk-justify-center", "gtk-justify-fill", "gtk-justify-left", "gtk-justify-right")
branch(ACTIONS, "view-restore", "gtk-leave-fullscreen")
branch(ACTIONS, "media-skip-forward", "gtk-media-forward", "gtk-media-next")
branch(ACTIONS, "media-playback-pause", "gtk-media-pause")
branch(ACTIONS, "media-playback-start", "gtk-media-play")
branch(ACTIONS, "media-skip-backward", "gtk-media-previous", "gtk-media-rewind")
branch(ACTIONS, "microphone", "gtk-media-record")
branch(ACTIONS, "media-playback-stop", "gtk-media-stop")
branch(ACTIONS, "image-missing", "gtk-missing-image")
branch(ACTIONS, "network", "gtk-network")
branch(ACTIONS, "document-new", "gtk-new")
branch(ACTIONS, "open", "gtk-open")
branch(ACTIONS, "edit-paste", "gtk-paste")
branch(ACTIONS, "applications-system", "gtk-preferences")
branch(ACTIONS, "printer", "gtk-print-error", "gtk-print-paused", "gtk-print-preview", "gtk-print-report", "gtk-print-warning")
branch(ACTIONS, "edit-redo", "gtk-redo")
branch(ACTIONS, "save", "gtk-save")
branch(ACTIONS, "save-as", "gtk-save-as")
branch(ACTIONS, "edit-select-all", "gtk-select-all", "gtk-unselect-all")
branch(ACTIONS, "preferences-desktop-font", "gtk-select-font")
branch(ACTIONS, "sort", "gtk-sort-ascending", "gtk-sort-descending")
branch(ACTIONS, "process-stop", "gtk-stop")
branch(ACTIONS, "edit-undo", "gtk-undo")
branch(ACTIONS, "zoom-in", "gtk-zoom-in")
branch(ACTIONS, "zoom-out", "gtk-zoom-out")
