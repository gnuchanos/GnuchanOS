# path ~/.config/GnuChanRunner/config.py

# WARNING THIS IS NOT A REAL PYTHON SCRIPT, IT IS JUST A CONFIG FILE
# GnuChanWM does the same thing, so this one does too. Only the two shapes
# GnuChanWM's reader understands are read here:
#
#     Name = value                            a setting
#     GnuChanRunner.call(Argument=value, ...)  a call
#
# Anything else in the file is left alone, so a name kept for your own use is
# not a mistake. A value the launcher does not know is a value it does not use.

# ---------------------------------------------------------------- the words

Prompt = "Run:"
Placeholder = "Type to search programs"
EmptyMessage = "Nothing matches"

# ------------------------------------------------------------- the window

FontFamily = "monospace"
FontSize = 14

# How wide the window is and how many matches it shows at once. The width is
# clamped to the screen; the rows are how tall the list grows before it starts
# to scroll — the window is the same size whether a query found two programs or
# two hundred, which is what keeps it readable while you type.
Width = 520
Rows = 8

# Where it sits: "center", "top" or "bottom". Anything else is read as
# "center".
Position = "center"

# ------------------------------------------------------------ the palette

# The same purple GnuChanWM and the greeter are drawn in. Change one here and
# it changes here only — the launcher does not read GnuChanWM's file, so the
# two are kept in step by hand.
Background = "#1a0b2e"
Panel = "#32143f"
PanelEdge = "#7b2cbf"
Field = "#241033"
Text = "#e0c3fc"
TextMuted = "#9d7bba"
Accent = "#c77dff"

# -------------------------------------------------------------- behaviour

# A capital letter is not something a person remembers about a program's name,
# so it does not matter by default.
CaseSensitive = False

# "gimp" finds "GNU Image Manipulation Program" when this is on. Typing the
# initials of a program is how most people use a launcher, so it is on.
Fuzzy = True

# What the command line is for. "off" lists installed programs only; "typed"
# offers a command line once nothing matches what you typed; "always" offers
# one under the list on every query. The default is what a launcher wants:
# a command line when a program was not found, and not before.
CommandMode = "typed"

# Entries marked NoDisplay or Hidden are installed but are not meant to be
# shown, so they are left out unless you say otherwise here.
ShowNoDisplay = False
ShowHidden = False

# ------------------------------------------------ where the programs come from

# The directories the scan reads. Leave these out and it reads
# /usr/share/applications and ~/.local/share/applications, which is where
# installed programs put their entries. Uncomment to read somewhere else
# instead.
#
# DesktopDirs = ["/usr/share/applications", "~/.local/share/applications"]
#
# GnuChanRunner.add_desktop_dir(path="/opt/my-programs")

# Programs of your own that have no .desktop file anywhere. They are searched
# with everything else. The name is what is typed to find it; the command is
# what runs.

GnuChanRunner.add_program(name="Terminal", command="xterm")
# GnuChanRunner.add_program(name="Files", command="thunar ~/")
# GnuChanRunner.add_program(name="Editor", command="vscodium")
