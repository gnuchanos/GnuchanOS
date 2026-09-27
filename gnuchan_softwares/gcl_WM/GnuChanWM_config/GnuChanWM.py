# path ~/.config/GnuChanWM/GnuChanWM.py

# WARNING THIS IS NOT REAL PYTHON SCRIPT IT'S JUST CONFIG FILE I JUST DO THAT BECOUSE IT'S FUNNY

gcl_Window.set_active_window_border_color("#d400ff")
gcl_Window.set_inactive_window_border_color("#450552")
gcl_Window.set_window_border_width(2)

# The desktop wallpaper, drawn behind every window with Imlib2, the way feh
# draws one. This is not the bar's picture: a wallpaper belongs behind the
# whole desktop, not inside one strip of it. Empty keeps the flat desktop
# colour.
# gcl_Window.background_color = "#27022b"
gcl_Window.BackgroundImage = "~/.config/GnuChanWM/bg.png"

# gcl_BAR.call(...) may be written more than once: each call is one bar, so a
# script can put a bar along the top, another along the bottom, and a small one
# down the side. This one is the top bar.
gcl_BAR.call(
    # bar settings.
    # Position names the edge the bar hugs (top / bottom / left / right); X and
    # Y place the bar's own top-left corner when it should sit off the edge.
    # Size is its thickness.
    Position="top",
    Size=24,
    BackgroundColor="#27022b",
    Vsync=True,
    X=0,
    Y=10,
    # The room left at each end. There are two pairs because a horizontal bar
    # and a vertical bar have different ends: Left/Right belong to a horizontal
    # bar and Up/Down to a vertical one. Naming the pair that does not match
    # `pose` is a mistake the manager reports on screen rather than applies.
    Left_EmptySpace=5, # only for horizontal bar
    Right_EmptySpace=5,# only for horizontal bar
    Up_EmptySpace=0,   # only for vertical bar
    Down_EmptySpace=0, # only for vertical bar
    pose="horizontal", # horizontal / vertical


    Widgets=[
        gcl_Widgets.CurrentLayout(
            start_layout=0,
            end_layout=5,
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
            symbol="[●]",
            Gap=5,
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
            Expanding=False,
            Horizontal=1
        ),
        gcl_Widgets.GroupBox(
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
            Seperator="|",
            SeperatorColor="#f8b5ff",
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
            Expanding=True,
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

gcl_themes.Theme_icon(
    ThemeName="GnuChanIcon",
    ThemePath="/usr/share/icons/GnuChanIcon",
)

gcl_themes.Theme_cursor(
    ThemeName="GnuChanMouseIcons",
    ThemePath="/usr/share/icons/GnuChanMouseIcons",
)


default_terminal = "xterm"

super_key1 = "Mod1"  # Alt
super_key2 = "Mod2"  # NumLock, on most keyboards
super_key3 = "Mod3"  # unused on most keyboards


# The action is written as the call it is, not as a string. The window manager
# reads the call's own name and its arguments: RunProgram(command=...) opens
# the program named, Close() closes the focused window, Switch() goes back to
# the window used before this one.
gcl_keys.all = [
    gcl_key.MultiKey(
        keys=[super_key1, "return"],
        action=gcl_spawn.RunProgram(command=default_terminal),
    ),
    gcl_key.MultiKey(keys=[super_key1, "F4"], action=gcl_window.Close()),
    gcl_key.MultiKey(keys=[super_key1, "Tab"], action=gcl_window.Switch()),
]

# fare behavior
gcl_mouse.MouseBehavior(
    LeftClick="select",
    RightClick="context_menu",
    MiddleClick="paste",
    ScrollUp="scroll_up",
    ScrollDown="scroll_down",
)

# touchpad behavior
gcl_touchpad.TouchpadBehavior(
    TapToClick=True,
    TwoFingerScroll=True,
    ThreeFingerSwipe=False,
    FourFingerSwipe=False,
)
