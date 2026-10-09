# path ~/.config/GnuChanWifi/config.py

# WARNING THIS IS NOT A REAL PYTHON SCRIPT, IT IS JUST A CONFIG FILE
# GnuChanWM and GnuChanRunner do the same thing, so this one does too. The
# reader understands one shape and one shape only:
#
#     Name = value
#
# `#` starts a comment. Blank lines, comments, and names this program does not
# know are skipped without a word, so a note kept in the file is not a mistake.
# A value may be quoted ("Wi-Fi") or bare (Wi-Fi); both are the same thing.

# --------------------------------------------------------------- program

# Which nmcli to run. Empty means "nmcli" on PATH.
NmcliPath = "nmcli"

# The wireless driver's module, for the "Restart" button. Empty means "ask the
# kernel" — the manager reads the driver's name off the interface itself, which
# is right on every machine. Only name one here if that read somehow fails; a
# Vostro's Atheros card is "ath5k".
RestartModule = ""

# The title drawn at the top of the window.
Title = "Wi-Fi"

# ---------------------------------------------------------------- window

# The window's width (clamped to the screen) and how many networks are shown at
# once before the list scrolls.
FontFamily = "monospace"
FontSize = 14
Width = 520
Rows = 10

# --------------------------------------------------------------- palette

# The same purple GnuChanWM and the greeter are drawn in. Change one here and it
# changes here only — this window does not read GnuChanWM's file, so the two are
# kept in step by hand.
Background = "#1a0b2e"
Panel = "#32143f"
PanelEdge = "#7b2cbf"
Field = "#241033"
Text = "#e0c3fc"
TextMuted = "#9d7bba"
Accent = "#c77dff"

# The marker on a network that needs a password, and the row of the network that
# is joined.
Secured = "#ff9e64"
Connected = "#9ece6a"
