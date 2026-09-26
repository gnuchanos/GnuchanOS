# path ~/.config/GnuChanWM/GnuChanWM.py


gcl_Window.set_active_window_border_color("#d400ff")
gcl_Window.set_inactive_window_border_color("#450552")

# can be call multiple bar but for now it's just one bar
gcl_BAR.call(
    Position="top",
    Size=24,
    BackgroundColor="#27022b",
    BackgroundImage="BG.png", # if this place is empty, it will use BackgroundColor
    # Draw the whole bar off-screen and put it on the screen in one operation.
    # Without it the strip, then each widget's background, then its text reach
    # the screen in that order, and the clock redraws the bar once a second —
    # which is seen as a sweep of colour across the bar every second. True is
    # what you want; False is for a slow remote X link, where the extra copy
    # costs more than it saves.
    Vsync=True,
    Widgets=[
        # The workspaces, drawn as this range with the current one lit.
        #
        # This is the one place the number of workspaces is decided: the
        # highest number written here is the last workspace there is, and the
        # WM binds its switch keys from it. end_layout=5 means six workspaces
        # reached by Super+1 .. Super+6; write end_layout=3 and there are four,
        # with no Super+5 left over. Nothing else has to be kept in step.
        gcl_Widgets.CurrentLayout(
            start_layout=0,
            end_layout=5,
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
            Expanding=False
        ),
        gcl_Widgets.GroupBox(
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
            symbol="[]", # symbol size just fontsize
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
            Expanding=True,
            Horizontal=1
        ),
        gcl_Widgets.TextBox(
            text="No Gnu No Life",
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
            Expanding=True,
            Horizontal=1
        ),
        gcl_Widgets.Clock(
            format="%Y-%m-%d %H:%M:%S",
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
        ),
    ]
)

# system theme, icon theme, cursor theme
gcl_themes.Theme_gtk(
    ThemeName="GnuChanTheme",
    ThemePath="/usr/share/themes/GnuChanTheme",
)

# The icon theme is installed by dotfile/ICON_THEME/settings_icons.py under
# this name; the cursor theme by dotfile/ICON_MOUSE_THEME/settings_mouse_icon.py
# under this one. They are deliberately different names: both are read from
# ~/.local/share/icons/<name> and ~/.icons/<name>, so two themes sharing a name
# would be one directory and the second install would delete the first.
gcl_themes.Theme_icon(
    ThemeName="GnuChanIcon",
    ThemePath="/usr/share/icons/GnuChanIcon",
)

gcl_themes.Theme_cursor(
    ThemeName="GnuChanMouseIcons",
    ThemePath="/usr/share/icons/GnuChanMouseIcons",
)


default_terminal = "xterm"

# The modifier names are X11's, and they are not where a person expects them:
#   Mod1 = Alt          Mod4 = Super (the Windows key)
#   Mod2 = NumLock      Mod3 = unused on most keyboards
# "ctrl" is the one name that is its own thing; "super" is accepted for Mod4.
#
# super_key1 is the name this script uses, not a variable X reads: it is a
# value, and the last entry in a binding's key list is the key itself. So the
# first binding below is Alt+Enter, because super_key1 is "Mod1" and Mod1 is
# Alt. Write super_key1 = "ctrl" for Ctrl+Enter, or "super" for Super+Enter.
super_key1 = "Mod1"  # Alt
super_key2 = "Mod2"  # NumLock, on most keyboards
super_key3 = "Mod3"  # unused on most keyboards

# keys
#
# Every entry but the last in `keys` is a modifier and the last is the key.
# The action is what the WM does when the key is pressed. The ones it can
# answer are the terminal, close, switch-window and reload; anything else is
# reported once at start-up, so a key that does nothing says why.
#
# The workspace keys are deliberately not written here: Super+1..N are bound
# from the layout widget's range (see end_layout above), so how many there are
# is written once and the keys follow it.
gcl_keys.all = [
    gcl_key.MultiKey(
        keys=[super_key1, "return"],
        action="gcl_spawn.RunProgram(command=default_terminal)",
    ),
    gcl_key.MultiKey(keys=[super_key1, "F4"], action="gcl_window.Close()"),
    gcl_key.MultiKey(keys=[super_key1, "Tab"], action="gcl_window.Switch()"),
]

# fare behavior
#
# What each button does. The names are the ones wm_menu.c and wm_input.c
# understand; anything else is reported, and the button is then left to the
# program under the pointer, which is what it did before this block existed.
#
#   "select"          focus and raise what was clicked (the left button)
#   "context_menu"    the desktop's own menu, on the desktop itself — a
#                     right-click on a window belongs to the program
#   "paste"           send a middle click to the focused window
#   "none"            do nothing, so the program gets it
#   "scroll_up"       the wheel, scrolling: over a window it scrolls the
#   "scroll_down"     program, over the desktop it turns the workspace
#   "workspace_next"  the next workspace, from any button including the wheel
#   "workspace_prev"  the previous one
gcl_mouse.MouseBehavior(
    LeftClick="select",
    RightClick="context_menu",
    MiddleClick="paste",
    ScrollUp="scroll_up",
    ScrollDown="scroll_down",
)

# touchpad behavior
#
# Applied to the running X server with xinput, so they hold under this window
# manager and under any other — nothing is written to a desktop's own settings
# store. TapToClick and TwoFingerScroll map to libinput properties. The two
# swipe switches do not: on X11 a swipe is the compositor's to interpret and
# libinput exposes no property for it. They are kept because the config is
# shared with a runtime that gives them their meaning, and the log says so
# rather than the setting pretending to have been applied.
gcl_touchpad.TouchpadBehavior(
    TapToClick=True,
    TwoFingerScroll=True,
    ThreeFingerSwipe=False,
    FourFingerSwipe=False,
)
