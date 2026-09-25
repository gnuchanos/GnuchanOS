from gcl_DM import gcl_BAR, gcl_Widgets, gcl_themes, gcl_key, gcl_keys, gcl_mouse, gcl_touchpad, gcl_Window

# path ~/.config/GnuChanWM/GnuChanWM.py


gcl_Window.set_active_window_border_color("#c369ff")
gcl_Window.set_inactive_window_border_color("#280440")

# can be call multiple bar but for now it's just one bar
gcl_BAR.call(
    Position="top",
    Size=24,
    BackgroundColor="#27022b",
    BackgroundImage="BG.png", # if this place is empty, it will use BackgroundColor
    Widgets=[
        gcl_Widgets.CurrentLayout(
            start_layout=0,
            end_layout=5,
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
        ),
        gcl_Widgets.GroupBox(
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
            symbol="", # symbol size just fontsize
        ),
        gcl_Widgets.EmptySpace(
            BackgroundColor="#940da3",
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
    ThemePath="" # D:\GnuchanOS\dotfile\GTK_THEME bunun kuruldugu yer
)

gcl_themes.Theme_icon(
    ThemeName="GnuChanIconTheme",
    ThemePath="" # D:\GnuchanOS\dotfile\ICON_THEME bunun kuruldugu yer
)

gcl_themes.Theme_cursor(
    ThemeName="GnuChanCursorTheme",
    ThemePath="" # D:\GnuchanOS\dotfile\CURSOR_THEME bunun kuruldugu yer
)


default_terminal = "xterm"
super_key1 = "Mod1" # ctrl
super_key2 = "Mod2" # middle super key
super_key3 = "Mod3" # alt

# keys
gcl_keys.all = [
    gcl_key.MultiKey(
        keys=[super_key1, "return"],
        action="gcl_spawn.RunProgram(command=default_terminal)",
    ),
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