# ~/.config/GnuChanDM/GnuChanDM.py
#
# WARNING: THIS IS NOT A REAL PYTHON SCRIPT. IT IS THE SETTINGS FILE OF THE
# LOGIN SCREEN, AND IT IS WRITTEN THE WAY GnuChanWM's AND GnuChanTerm's ARE
# WRITTEN BECAUSE THAT IS WHAT THIS DESKTOP'S SETTINGS LOOK LIKE. Nothing here
# is executed by Python.
#
# Every line below is OPTIONAL. A setting that is not written keeps the value
# the greeter shipped with, so this file can be one line long — and a file that
# is deleted entirely is the same as a file that sets nothing.
#
# The colours here are the same purple the rest of the desktop is themed with,
# and they are the greeter's own built-in palette written out in full. They are
# written down rather than left to the built-in values so that this file is
# also the answer to "what colour is what".

# --- the palette -------------------------------------------------------------
#
# Ten colours, each chosen for one surface. The names are the surfaces: a
# setting that is not written keeps its built-in value, so this section can be
# cut down to the two or three a person actually wants to change.
#
#   Background   the whole screen behind the panel
#   Panel        the login box itself
#   PanelEdge    the line around the panel
#   Field        an input box, unfocused
#   FieldFocus   the input box being typed in
#   Text         what the user reads
#   TextMuted    labels and hints
#   Accent       the sign-in button and the focus ring
#   AccentDim    the button when it is idle
#   Danger       an error message
gcl_DM.call(
    Background="#1a0b2e",
    Panel="#32143f",
    PanelEdge="#7b2cbf",
    Field="#241033",
    FieldFocus="#3a1a52",
    Text="#e0c3fc",
    TextMuted="#9d7bba",
    Accent="#c77dff",
    AccentDim="#7b2cbf",
    Danger="#ff5d8f",
)

# --- the fonts ---------------------------------------------------------------
#
# An X core font name, which is what this greeter draws with — its fields are
# XFontStruct and it draws with XDrawString, so the name is the server's own
# pattern and not an Xft description. The three below are the ones the greeter
# tries first by itself.
#
# A name the server cannot open is not fatal: the built-in list is tried after
# it, so a machine whose configured font is missing still draws its login
# screen in a font it has. A greeter that refused to start over a font would be
# a machine nobody can log into.
gcl_DM.call(
    FontLabel="-*-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1",
    FontField="-*-helvetica-medium-r-normal--18-*-*-*-*-*-iso8859-1",
    FontTitle="-*-helvetica-bold-r-normal--28-*-*-*-*-*-iso8859-1",
)

# --- the measurements --------------------------------------------------------
#
# Pixels. Zero means "not written" — a login panel with no margin is not a
# thing anyone asks for — so a value written here has to be a usable one, and a
# negative is refused with a note rather than drawn inside out.
gcl_DM.call(
    Margin=16,
    Gap=12,
    PanelWidth=420,
    FieldHeight=40,
    ButtonHeight=40,
)

# --- the same settings, written as assignments -------------------------------
#
# Both forms do the same thing and a file may use either, or both. A call is
# what a file with a handful of settings reads like; an assignment is what a
# file being edited one line at a time reads like.
#
# gcl_DM.background = "#1a0b2e"
# gcl_DM.panel = "#32143f"
# gcl_DM.accent = "#c77dff"
# gcl_DM.text = "#e0c3fc"
# gcl_DM.margin = 16
# gcl_DM.panel_width = 420

# --- what this file cannot say yet -------------------------------------------
#
# The session to start, the host name shown above the panel, and whether the
# power buttons appear at all are three things the greeter does not read from
# this file. That is not an oversight and it is not a promise: the session is
# whatever the user picks from the list scanned out of /usr/share/xsessions,
# the host name is read from the machine, and the two power buttons are always
# drawn. They will be added here when the code behind them is written — a
# setting that is read and then quietly ignored is worse than one that is not
# offered.
