#chmod +x ~/.config/qtile/display.sh 
#chmod +x ~/.config/qtile/autostart.sh
#cat  ~/.local/share/qtile/qtile.log   #this is eror log

#qtile cmd-obj -o cmd -f debug
#qtile cmd-obj -o cmd -f warning

# All Library Here
import os, subprocess, socket
from typing import List

from libqtile import bar, layout, widget
from libqtile.config import Click, Drag, Group, Key, Match, Screen
from libqtile.lazy import lazy
from libqtile.utils import guess_terminal
from libqtile import hook
from libqtile import qtile

# Default Settings
mod = "mod4"
terminal = guess_terminal("xterm")
user_home = os.path.expanduser("~")
keyboardLang = "us"
InternetDeviceName = "enp37s0"





# Don't Touch This -> Floating window layout change
FloatingWindowIndex = 0
@lazy.function
def float_cycle(qtile):
    global FloatingWindowIndex

    floating_windows = []
    for window in qtile.current_group.windows:
        if window.floating:
            floating_windows.append(window)
    
    if not floating_windows:
        return
    
    FloatingWindowIndex = min(FloatingWindowIndex, len(floating_windows) -1)
    FloatingWindowIndex += 1
    
    if FloatingWindowIndex >= len(floating_windows):
        FloatingWindowIndex = 0
    if FloatingWindowIndex < 0:
        FloatingWindowIndex = len(floating_windows) - 1

    win = floating_windows[FloatingWindowIndex]
    win.cmd_bring_to_front()
    win.cmd_focus()

Key(["mod1"], "Tab", float_cycle),

# Don't Touch This




# All ShortCut Here
keys = [
    # Floating Window Layout Changing
    Key(["mod1"], "Tab", float_cycle),

    # move focus
    Key([mod], "Up", lazy.layout.up()),
    Key([mod], "Down", lazy.layout.down()),
    Key([mod], "Left", lazy.layout.left()),
    Key([mod], "Right", lazy.layout.right()),

    # move focus window
    Key([mod, "shift"], "Left", lazy.layout.shuffle_left()),
    Key([mod, "shift"], "Right", lazy.layout.shuffle_right()),
    Key([mod, "shift"], "Down", lazy.layout.shuffle_down()),
    Key([mod, "shift"], "Up", lazy.layout.shuffle_up()),

    # resize focus window
    Key([mod, "control"], "Left", lazy.layout.grow_left()),
    Key([mod, "control"], "Right", lazy.layout.grow_right()),
    Key([mod, "control"], "Down", lazy.layout.grow_down()),
    Key([mod, "control"], "Up", lazy.layout.grow_up()),
    Key([mod, "control"], "r", lazy.restart()),
    Key([mod, "control"], "q", lazy.shutdown()),

    # file manager and terminal
    Key([mod, "shift"], "Return", lazy.spawn("guake -e ranger")),
    Key([mod], "Return", lazy.spawn(terminal)),

    # full screen and window mode
    Key([mod], "Tab", lazy.next_layout()),

    # close window
    Key([mod], "w", lazy.window.kill()),

    # ekstra
    Key([mod, "shift"], "z", lazy.spawn("lxappearance")),
    Key([mod], "l", lazy.spawn("leafpad")),
    Key([mod], "p", lazy.spawn("pavucontrol")),

    # No Gui
    Key([mod], "s", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/screenShoot.py")),

    # GnuChanGUI
    Key([mod], "t", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/0_SimpleTimer.py")),
    Key([mod], "c", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/1_SimpleCalculator.py")),
    Key([mod, "shift"], "t", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/2_SimpleTextEditor.py")),
    Key([mod], "r", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/3_SimpleProgramRunner.py")),
    Key([mod], "i", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/4_SimpleImageViever.py")),
    Key([mod], "m", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/5_SimpleMusicPlayer.py")),
    Key([mod], "v", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/6_SimpleVideoPlayer.py")),
    Key([mod, "shift"], "r", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/7_SimpleSVAR.py")),
    Key([mod, "shift"], "d", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/8_DMV.py")),
    
    # this is not ready
    #Key([mod, "shift"], "Return", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/")),


    # you can use this program runners
    Key( [mod], "f", lazy.spawn("dmenu_run -i -b -p 'GnuChanOS'  -fn 'Sans Mono:bold:pixelsize=12' -nb '#240046' -nf '#9d4edd' -sf '#9d4edd' -sb '#5a189a' ") ),
]

# Top Bar Group Settings Max Group and Switch Window to Diffret Work group
# groups = [Group(i) for i in "1234"] this is old
groups = [Group(f"{i}", label="⬤") for i in range(6)]
for i in groups:
    keys.extend([
        Key(
            [mod], 
            i.name, 
            lazy.group[i.name].toscreen(), 
            desc="Switch to group {}".format(i.name)
        ),
        Key(
           [mod, "shift"], 
           i.name, 
           lazy.window.togroup(i.name, switch_group=True), 
           desc="Switch to & move focused window to group {}".format(i.name)
        )
    ])

# Window Layout 
border_focus="#9d4edd"
layouts = [ layout.Columns(margin=5, border_width=3, border_focus="#9d4edd", border_normal="#240046"), layout.Max() ]

# Widget System
widget_defaults = dict( font="Sans", fontsize=12, padding=3 )
extension_defaults = widget_defaults.copy()
colors = [["#240046", "#240046"], # 0: background color
          ["#5a189a", "#5a189a"], # 1: spacer background color
          ["#9d4edd", "#9d4edd"], # 2: text color
          ["#c77dff", "#c77dff"], # 3: inactive and not update text color
          ["#3c096c", "#3c096c"], # 4: other widget background color
          ["#0e0024", "#0e0024"], # 5: separator background color
]
prompt = "{0}@{1}: ".format(os.environ["USER"], socket.gethostname())

# This Config File Have 2 Bar
screens = [ Screen(
    top=bar.Bar([
        widget.TextBox(background = colors[0], text=":"),
        widget.Image(filename = "~/.config/qtile/img/ram.png", background = colors[0]),
        # Start Here

        # Left
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.GroupBox( background = colors[4], active = colors[2], inactive = colors[3], highlight_method="line", highlight_color="#c4a7e7", borderwidth=0 ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.Systray(padding=10,foreground=colors[2], background=colors[0] ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        # Left


        # Middle
        widget.Spacer( background = colors[1] ),
        widget.Image( filename = "~/.config/qtile/img/gnu.png", scale = "False", background = colors[0] ),
        widget.TextBox( background=colors[0], foreground=colors[2], text="(-Gnu/Linux's My Life-)" ),
        widget.Image( filename = "~/.config/qtile/img/gnu.png", scale = "False", background = colors[0] ), 
        widget.Spacer( background = colors[1] ),
        # Middle

        # Right
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.KeyboardLayout( configured_keyboards=keyboardLang, foreground = colors[2], background = colors[4] ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.Clock( foreground = colors[2], background = colors[4], format = "%A, %B %d - %H:%M " ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" "),
        # Right

        # for laptop
        #widget.Battery( format='{char} {percent:2.0%} {hour:d}:{min:02d}', charge_char='↑', discharge_char='↓', full_char='■', empty_char='□', background=colors[0], foreground=colors[2], low_percentage=0.10, low_foreground=colors[4], update_interval=60 ),

        # End Here
        widget.Image(filename = "~/.config/qtile/img/rem.png", background = colors[0]),
        widget.TextBox(background = colors[0], text=":"),
    ], 30, background = colors[0], margin=[5, 5, 5, 5]),

    bottom=bar.Bar([
        widget.TextBox(background = colors[0], text=":"),
        widget.Spacer(background = colors[1]),
        # Start Here


        # Middle
        widget.TextBox( background=colors[5],foreground=colors[5],text=" " ),
        widget.HDDBusyGraph( device = "sda", graph_color = colors[2], fill_color = colors[2], border_color = colors[1], background = colors[4] ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.ThermalSensor( foreground = colors[2], background = colors[4], threshold = 90, fmt = 'Temp: {}',  padding = 5 ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.CPU ( foreground=colors[2], background=colors[4] ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        widget.Memory( foreground = colors[2], background = colors[4], fmt = 'Ram: {}', padding = 5 ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" "),
        widget.Net( interface = InternetDeviceName, format = 'NET Speed: {down} ↓↑ {up}',  foreground = colors[2], background = colors[4], padding = 5 ),
        widget.TextBox( background=colors[5], foreground=colors[5], text=" "),
        #widget.Wlan( interface = InternetDeviceName, foreground = colors[2], background = colors[4]),
        #widget.TextBox( background=colors[5], foreground=colors[5], text=" " ),
        # Middle 

        # End Here
        widget.Spacer( background = colors[1]),
        widget.TextBox( background = colors[0], text=":" ),
    ], 30, background = colors[0], margin=[5, 5, 5, 5] ),
)]

# just click event
mouse = [
    Drag([mod, "shift"], "Button1", lazy.window.set_position_floating(),   start=lazy.window.get_position()),
    Drag([mod, "shift"], "Button3", lazy.window.set_size_floating(),       start=lazy.window.get_size()),
    Click(["mod1"], "Button1", lazy.window.bring_to_front())
]



# Start Extra Things Here in AutoStart.sh
os.popen(f"sh {user_home}/.config/qtile/autostart.sh")



bring_front_click = "floating_only"

# Don't Touch This if you don't know what it is'
dgroups_key_binder = None
dgroups_app_rules = []  # type: List
follow_mouse_focus = True
cursor_warp = False

floating_layout = layout.Floating(
	border_focus="#9d4edd",
	border_normal="#240046",
	border_width=3,
    float_rules=[
        # Run the utility of `xprop` to see the wm class and name of an X client.
        *layout.Floating.default_float_rules,
        Match(wm_class="confirmreset"),  # gitk
        Match(wm_class="makebranch"),  # gitk
        Match(wm_class="maketag"),  # gitk
        Match(wm_class="ssh-askpass"),  # ssh-askpass
        Match(title="branchdialog"),  # gitk
        Match(title="pinentry"),  # GPG key password entry
    ]
)




bring_front_click = True
auto_fullscreen = False # True Default
focus_on_window_activation = "focus" #smart , focus
reconfigure_screens = True
auto_minimize = True
wl_input_rules = None # wayland
wmname = "LG3D"  #
