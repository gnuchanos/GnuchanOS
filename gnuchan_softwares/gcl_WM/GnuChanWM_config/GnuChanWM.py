# path ~/.config/GnuChanWM/GnuChanWM.py

# WARNING THIS IS NOT REAL PYTHON SCRIPT IT'S JUST CONFIG FILE I JUST DO THAT BECOUSE IT'S FUNNY

gcl_Window.set_active_window_border_color("#d400ff")
gcl_Window.set_inactive_window_border_color("#450552")
gcl_Window.set_window_border_width(2)

# The border of a window that has just been sent to another workspace with
# Alt+Shift+N. It is a third colour and not a shade of either of the two above,
# because it says a third thing: "this window was moved, and you have not looked
# at it since". The accent above means "the keyboard is here" and the inactive
# colour means "nothing in particular", so a window marked with this one is
# neither of those and must not be mistakable for either.
#
# It stays on until the window is focused again, which is the act of having
# found it — so a hand that has sent three windows to three desks can still tell
# which three they were.
gcl_Window.set_moved_window_border_color("#52024d")

# The desktop wallpaper, drawn behind every window with Imlib2, the way feh
# draws one. This is not the bar's picture: a wallpaper belongs behind the
# whole desktop, not inside one strip of it. Empty keeps the flat desktop
# colour.
# gcl_Window.background_color = "#27022b"
gcl_Window.BackgroundImage = "~/.config/GnuChanWM/bg.png"

# The Alt+` window switcher, which is a surface of its own and is therefore
# coloured on its own. Every one of these falls back to the desktop's palette
# when it is not written, so commenting one out is how you get the default back.
#
# They are separate from the desktop's colours because the switcher is mostly a
# picture of a window: the colour around that picture has to be quiet enough for
# a terminal's own colours to still read as that terminal, which is not what a
# desktop background is chosen for.
gcl_Switcher.background    = "#0d0512"   # behind the whole grid
gcl_Switcher.panel         = "#1b0c22"   # the panel, and the cells' bodies
gcl_Switcher.cell_border   = "#7b2cbf"   # every cell's frame
gcl_Switcher.select_border = "#d400ff"   # the chosen cell's frame
gcl_Switcher.text          = "#e0c3fc"   # every cell's name
gcl_Switcher.select_text   = "#d400ff"   # the chosen cell's name
gcl_Switcher.field         = "#2a1035"   # a hovered caption, an empty cell

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


default_terminal = "GnuChanTerm"
GnuChanRunner = "GnuChanRunner"

super_key1 = "Mod1"  # Alt
super_key2 = "Mod2"  # NumLock, on most keyboards
super_key3 = "Mod3"  # unused on most keyboards


# The action is written as the call it is, not as a string. The window manager
# reads the call's own name and its arguments: RunProgram(command=...) opens
# the program named, Close() closes the focused window, Switch() goes back to
# the window used before this one.
gcl_keys.all = [
    gcl_key.MultiKey(keys=[super_key1, "return"], action=gcl_spawn.RunProgram(command=default_terminal)),
    gcl_key.MultiKey(keys=[super_key1, "r"], action=gcl_spawn.RunProgram(command=GnuChanRunner)),
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
