# path ~/.config/GnuChanWM/GnuChanWM.py


gcl_Window.set_active_window_border_color("#d400ff")
gcl_Window.set_inactive_window_border_color("#450552")

# can be call multiple bar but for now it's just one bar
gcl_BAR.call(
    Position="top",
    Size=24,
    BackgroundColor="#27022b",
    BackgroundImage="BG.png", # if this place is empty, it will use BackgroundColor
    Vsync=True,
    Widgets=[
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


gcl_keys.all = [
    gcl_key.MultiKey(
        keys=[super_key1, "return"],
        action="gcl_spawn.RunProgram(command=default_terminal)",
    ),
    gcl_key.MultiKey(keys=[super_key1, "F4"], action="gcl_window.Close()"),
    gcl_key.MultiKey(keys=[super_key1, "Tab"], action="gcl_window.Switch()"),
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
