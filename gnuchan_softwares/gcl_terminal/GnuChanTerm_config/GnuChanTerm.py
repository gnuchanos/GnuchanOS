# ~/.config/GnuChanTerm/GnuChanTerm.py
#
# WARNING: THIS IS NOT A REAL PYTHON SCRIPT. IT IS THE SETTINGS FILE OF THE
# TERMINAL, AND IT IS WRITTEN THE WAY GnuChanWM's IS WRITTEN BECAUSE THAT IS
# WHAT THIS DESKTOP'S SETTINGS LOOK LIKE. Nothing here is executed by Python.
#
# Every line below is OPTIONAL. A setting that is not written keeps the value
# the terminal shipped with, so this file can be one line long — and a file
# that is deleted entirely is the same as a file that sets nothing.
#
# The colours here are the same purple the rest of the desktop is themed with.
# They are written out in full and not left to the built-in values, so that
# this file is also the answer to "what colour is what".

# --- the font ----------------------------------------------------------------
#
# An Xft font description: family-size, with optional settings after a colon.
# `monospace` is whatever the system's fixed-width font is.
#
# This is the one setting read BEFORE the window is made, because the cell size
# comes from the font and the window is sized in cells.
gcl_Terminal.call(Font="monospace-11")

# A larger font, antialiased, with the family named directly:
# gcl_Terminal.call(Font="DejaVu Sans Mono-13:antialias=true:hinting=true")

# --- the prompt --------------------------------------------------------------
#
# The shell's PS1, written the way the shell writes it. The \u, \h, \w and \n
# below are BASH'S OWN escapes and are passed through untouched — it is bash
# that expands them: \u is the user, \h the host, \w the working directory and
# \n a line break. The \[ and \] around each colour tell bash that what is
# between them takes no room on the screen, which is what keeps a long line
# wrapping where it should instead of wrapping early.
#
# It is handed to the shell in its environment, and that is as far as a
# terminal can go without editing a file of yours. Three things go over:
#
#     PS1               the text, for a shell that runs no PROMPT_COMMAND
#     GCL_TERM_PROMPT   the same text, so the command below never quotes it
#     PROMPT_COMMAND    PS1="$GCL_TERM_PROMPT", re-asserted before each prompt
#
# The third is what makes it stick. A shell reads the environment BEFORE its
# own startup files, so a bare PS1 loses to the PS1 in ~/.bashrc on any machine
# that has one. PROMPT_COMMAND runs after those files and before every prompt,
# so the configured prompt is put back each time.
#
# A user whose own startup file sets PROMPT_COMMAND keeps theirs — it is read
# later and replaces this one — and with it their own prompt, which is theirs
# to choose. Nothing of the user's is read or written either way.
#
# The colours are the theme's: the frame in the lighter purple, the parts that
# change (user, host, directory) in the paler one.
gcl_Terminal.call(Prompt="\[\e[38;5;141m\]┌─[\[\e[38;5;183m\]\u\[\e[38;5;141m\]@\[\e[38;5;183m\]\h\[\e[38;5;141m\]]─[\[\e[38;5;183m\]\w\[\e[38;5;141m\]]\n\[\e[38;5;141m\]└──\[\e[38;5;183m\]❯ \[\e[0m\]")

# A plainer one, one line and no colour, which is what to write while checking
# that the prompt mechanism itself works:
# gcl_Terminal.call(Prompt="\u@\h:\w\$ ")

# --- the palette -------------------------------------------------------------
#
# The sixteen colours a program names by number, in the order every terminal
# has used since the eighties:
#
#     0 black    1 red      2 green   3 yellow
#     4 blue     5 magenta  6 cyan    7 white
#     8-15 the same eight, bright
#
# and then the terminal's own text and background, which a program cannot name.
# A shorter list sets the colours it has and leaves the rest, so naming only
# the sixteen does not mean repeating the last two.
#
# These are the theme's terminal.ansi* values, unchanged. The whole range is
# one hue, which makes telling two of them apart harder than sixteen hues would
# — that is the cost of a purple system, and the entries are spread across
# lightness so a directory and a file are still clearly different colours.
# The order is the sixteen names followed by the terminal's own text and
# background:
#
#     0 black    1 red      2 green   3 yellow
#     4 blue     5 magenta  6 cyan    7 white
#     8-15 the same eight, bright
#     16 text    17 background
#
# The numbers are written beside the values, and the comments are OUTSIDE the
# list on purpose: a comment inside it is one more thing the reader of this
# file has to understand, and every colour here is already named by its place.
gcl_Terminal.call(Colors=[
    "#16051f",
    "#8b2fc9",
    "#9d3fe0",
    "#b14cff",
    "#7b2cbf",
    "#c05cff",
    "#a83ee6",
    "#c77dff",
    "#54206f",
    "#b65cff",
    "#9635d0",
    "#d08aff",
    "#a946e8",
    "#c77dff",
    "#b967f5",
    "#d28cff",
    "#c98cff",
    "#0b0310",
])
# --- the bar -----------------------------------------------------------------
#
# The strip along the bottom holding the child's working directory. It is the
# terminal's own furniture and no program can draw on it.
#
# Its background is the same colour as the terminal's background on purpose: a
# strip in another colour reads as a frame around the text, and a strip in the
# same colour reads as part of the window. The same is true of the margin down
# each side. The one row of text is what marks it, and that is enough.
gcl_Terminal.call(BarBackground="#09030d")
gcl_Terminal.call(BarForeground="#ddb3ff")

# --- the cursor --------------------------------------------------------------
#
# The block drawn over the cell the cursor sits on. It belongs to the terminal
# and not to the program: a program cannot name it, because it is the terminal
# saying where the cursor is and not the program drawing.
gcl_Terminal.call(Cursor="#ddb3ff")

# --- the same settings, written as assignments -------------------------------
#
# Both forms do the same thing and a file may use either, or both. A call is
# what a file with a handful of settings reads like; an assignment is what a
# file being edited one line at a time reads like.
#
# gcl_Terminal.font = "monospace-11"
# gcl_Terminal.prompt = "\u@\h:\w\$ "
# gcl_Terminal.colors = ["#170a20", "#c084fc"]
# gcl_Terminal.bar_background = "#09030d"
# gcl_Terminal.bar_foreground = "#ead7ff"
# gcl_Terminal.cursor = "#ddb3ff"

# --- what this file cannot say yet -------------------------------------------
#
# The cursor's SHAPE, the height of the bar, the margin down the sides, the
# size the window opens at, and the program to run are all things the terminal
# does not read from this file. That is not an oversight and it is not a
# promise: the code behind each of them is not written, and a setting that is
# read and then quietly ignored would be a line here that does nothing and
# says nothing about why. They will be added here when they work.
#
# The cursor's COLOUR is read, and is the line above. The prompt is read too,
# and is the line further up.
