# path ~/.config/GnuChanWM/GnuChanWM.py


gcl_Window.set_active_window_border_color("#d400ff")
gcl_Window.set_inactive_window_border_color("#450552")
gcl_Window.set_window_border_width(2)

# can be call multiple bar but for now it's just one bar
gcl_BAR.call(
    Position="top",
    Size=24,
    BackgroundColor="#27022b",
    BackgroundImage="bg.png", # if this place is empty, it will use BackgroundColor
    Vsync=True,
    Widgets=[
        gcl_Widgets.CurrentLayout(
            start_layout=0,
            end_layout=5,
            BackgroundColor="#53055c",
            ForegroundColor="#f069ff",
            FontSize=12,
            FontFamily="monospace",
            symbol=" [●] ",
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


# Compasitor Settings D:\GnuchanOS\gnuchan_softwares\gcl_WM\Module
# this is extra x11 compasitor program for GnuChanWM and you can set this in here
# shader settings
gcl_compositor.Shadow(
    enable=True,
    shadow_color="#000000",
    shadow_opacity=0.5,
    shadow_offset_x=5,
    shadow_offset_y=5,
)

gcl_compositor.Transparency(
    enable=True,
    transparency_level=0.8,
)

gcl_compositor.TransparentWindowsList = [ # only these windows will be transparent, if you want all windows to be transparent, just set this to empty list
    "xterm",
]

gcl_compositor.Blur(
    enable=True,
    blur_radius=5,
)

gcl_compositor.VSync(
    fps=60,
    enable=True,
)

gcl_compositor.Burning( # if window closes, it will burn and fade out, if window opens, it will fade in and burn
    enable=True,
    burning_color="#ff0000",
    burning_opacity=0.5,
    burning_offset_x=5,
    burning_offset_y=5,
    fade_in_duration=0.5,
    fade_out_duration=0.5
)

# full screen shader
gcl_compositor.Crt_Effect(
    enable=True,
    type="crt",
    intensity=0.5,
    scanline_intensity=0.5,
    curvature=0.5,
    vignette_intensity=0.5,
    vignette_radius=0.5,
    vignette_softness=0.5,
    vignette_color="#000000",
    vignette_opacity=0.5,
    vignette_offset_x=0.0,
    vignette_offset_y=0.0,
    vignette_aspect_ratio=1.0,
    vignette_rotation=0.0,
    vignette_scale=1.0,
    vignette_blur=0.5,
    vignette_blur_radius=0.5,
    vignette_blur_softness=0.5,
)

gcl_compositor.Vhs_Effect(
    enable=True,
    type="vhs",
    intensity=0.5,
    scanline_intensity=0.5,
    curvature=0.5,
    vignette_intensity=0.5,
    vignette_radius=0.5,
    vignette_softness=0.5,
    vignette_color="#000000",
    vignette_opacity=0.5,
    vignette_offset_x=0.0,
    vignette_offset_y=0.0
)

# ctrl+middle mouse click cube rotate effect shows workspace
gcl_compositor.Cube_Rotate_Effect(
    enable=True,
    effect_type="cube_rotate",
    effect_rotation_speed=1.0,
)

# video settings, if you want to use video as wallpaper, you can set this to True
# not video exist -> use background image or not background image exist -> use background color
gcl_compasitor.ScreenVideo(
    enable=True,
    # optimize for performance but quality must be fine
    # if wine open disable video playback, you can set this to False
    optimize_for_performance=True, # quality will be fine not high quality but performance will be better
    video_path="/usr/share/videos/GnuChanWM.mp4",
)

# animation settings
gcl_compositor.Animation(
    enable=True, # if you want to enable animation, set this to True with Burning 
    animation_duration=0.3,
    animation_type="fade_in_out"
)

gcl_compositor.Animation(
    enable=True, # simple shake animation
    animation_duration=0.3,
    animation_type="shake_if_window_is_moving"
)


# screen saver settings --> this is extra x11 program but we setting in here
gcl_screensaver.ScreenSaver(
    enable=True,
    ready_animation_path="/usr/share/animations/GnuChanWM_ready_animation.gif",

)
