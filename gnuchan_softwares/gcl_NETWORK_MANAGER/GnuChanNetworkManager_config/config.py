# path ~/.config/GnuChanNetworkManager/config.py

# WARNING THIS IS NOT A REAL PYTHON SCRIPT, IT IS JUST A CONFIG FILE
# GnuChanWifi and GnuChanWM do the same thing, so this one does too. The reader
# understands one shape and one shape only:
#
#     Name = value
#
# `#` starts a comment. Blank lines, comments, and names this program does not
# know are skipped without a word, so a note kept in the file is not a mistake.
# A value may be quoted ("Network") or bare (Network); both are the same thing.

# --------------------------------------------------------------- programs

# Which tools to run. Empty means the bare name on PATH.
NmcliPath = "nmcli"
IpPath = "ip"
ResolvectlPath = "resolvectl"

# Where zapret's own init script lives. The "DPI" button in the DNS panel runs
# it with "start" and "stop" to turn the DPI bypass on and off (see net_dpi.c).
# zapret's installer puts the script under /opt/zapret; point this elsewhere if
# it was unpacked somewhere else. When the script is not there the button is
# drawn as unavailable and does nothing.
DpiInitPath = "/opt/zapret/init.d/sysv/zapret"

# The program the "Open Wi-Fi" button runs. The wifi manager is the program that
# knows how to JOIN a wireless network; this manager shows the adapter and does
# the general network work. Point this at a different program to use another.
WifiManager = "GnuChanWifi"

# ----------------------------------------------------------------- window

Title = "Network"

FontFamily = "monospace"
FontSize = 14

# The window's width (clamped to the screen) and how many interface rows are
# shown at once before the list scrolls.
Width = 640
RunningRows = 6

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

# The state colours: a device that is up, and one that is down or blocked.
Connected = "#9ece6a"
Disabled = "#ff9e64"

# The fill of the row you have clicked. It is deliberately a clear step away
# from Background and Panel: a selection drawn in a colour close to the
# background is a selection nobody can see.
Selection = "#5a2a8f"
