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
gcl_Terminal.call(Colors=[
    "#170a20",   # 0  black          - the theme's own dark
    "#c084fc",   # 1  red
    "#b56cff",   # 2  green
    "#d8a4ff",   # 3  yellow
    "#9d4edd",   # 4  blue
    "#c77dff",   # 5  magenta
    "#b76eff",   # 6  cyan
    "#d8a4ff",   # 7  white          - the theme's #ead7ff reads as white
    "#70458a",   # 8  bright black   - the theme's comment grey
    "#d8a4ff",   # 9  bright red
    "#c084fc",   # 10 bright green
    "#e0aaff",   # 11 bright yellow
    "#b76eff",   # 12 bright blue
    "#e0aaff",   # 13 bright magenta
    "#d8a4ff",   # 14 bright cyan
    "#e0aaff",   # 15 bright white   - the theme's #ffffff is white
    "#ddb3ff",   # 16 the terminal's own text - the theme's #ead7ff is white
    "#09030d",   # 17 the terminal's own background
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
# The cursor's COLOUR is read, and is the line above.
