#chmod +x ~/.config/qtile/display.sh 
#chmod +x ~/.config/qtile/autostart.sh
#cat  ~/.local/share/qtile/qtile.log   #this is eror log

#qtile cmd-obj -o cmd -f debug
#qtile cmd-obj -o cmd -f warning

#############################################
import os
import subprocess
import socket
from typing import List

from libqtile import bar, layout, widget
from libqtile.config import Click, Drag, Group, Key, Match, Screen
from libqtile.lazy import lazy
from libqtile.utils import guess_terminal
from libqtile import hook
from libqtile import qtile
#############################################

#############################################
mod = "mod4"
terminal = guess_terminal("cool-retro-term")
user_home = os.path.expanduser("~")
#############################################

#####################################################################################################################
keys = [
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

    Key([mod], "s", lazy.spawn(f"python3 {user_home}/.config/qtile/Programs/screenShoot.py")),
    Key([mod], "d", lazy.spawn("scrot -a 0,0,1024,768")),
    Key([mod, "shift"], "d", lazy.spawn("scrot -s")),

    # ekstra
    Key([mod, "shift"], "z", lazy.spawn("lxappearance")),
    Key([mod], "l", lazy.spawn("leafpad")),
    Key([mod], "p", lazy.spawn("pavucontrol")),
    
    # simple programs
    #Key([mod], "r", lazy.spawn(f"python3 {user_home}/.config/qtile/programs/prunner/prunner.py", shell=True)),

    # you can use this program runners
    Key([mod], "f", lazy.spawn("dmenu_run -i -b -p 'GnuChanOS'  -fn 'sans Mono:bold:pixelsize=20' -nb '#240046' -nf '#9d4edd' -sf '#9d4edd' -sb '#5a189a' ")),    
]
#####################################################################################################################

#####################################################################################################################
groups = [Group(i) for i in "1234567890"]

for i in groups:
    keys.extend([
       Key([mod], i.name, lazy.group[i.name].toscreen(),                             
       desc="Switch to group {}".format(i.name)),

       Key([mod, "shift"], i.name, lazy.window.togroup(i.name, switch_group=True),   
       desc="Switch to & move focused window to group {}".format(i.name)),
    ])
#####################################################################################################################

#####################################################################################################################
border_focus="#9d4edd"
layouts = [
    layout.Columns(margin=14, border_width=6, border_focus="#9d4edd", border_normal="#240046"),
    #layout.MonadTall(margin=8, border_width=6, border_focus="#08000d", border_normal="#08000d"),
    layout.Max(),
]
#####################################################################################################################

#####################################################################################################################
widget_defaults = dict(
    font='sans',
    fontsize=12,
    padding=8,
)
extension_defaults = widget_defaults.copy()
#####################################################################################################################

#####################################################################################################################
colors = [["#240046", "#240046"], # 0: background color
          ["#5a189a", "#5a189a"], # 1: spacer background color
          ["#9d4edd", "#9d4edd"], # 2: text color
          ["#c77dff", "#c77dff"], # 3: inactive and not update text color
          ["#3c096c", "#3c096c"] # 4: other widget back ground
          ]
prompt = "{0}@{1}: ".format(os.environ["USER"], socket.gethostname())
#####################################################################################################################

#####################################################################################################################
screens = [
    Screen(

#wallpaper="~/.config/qtile/img/wallpaper.jpg",
#wallpaper_mode="scale",

top=bar.Bar(
[
    widget.TextBox(background = colors[0], text=":"),
    widget.Image(filename = "~/.config/qtile/img/ram.png", background = colors[0]),

    widget.GroupBox(background = colors[4],active = colors[2], inactive = colors[3],),
    widget.Systray(padding=10,foreground=colors[2],background=colors[0],),  

    widget.Spacer(background = colors[1]),

    widget.Image(filename = "~/.config/qtile/img/gnu.png",scale = "False",background = colors[0],),
    widget.TextBox(background=colors[0],foreground=colors[2],text="(-Gnu/Linux's My Life-)",),
    widget.Image(filename = "~/.config/qtile/img/gnu.png",scale = "False",background = colors[0],),

    widget.Spacer(background = colors[1]),

    widget.KeyboardLayout(configured_keyboards="tr", foreground = colors[2],background = colors[4]),
    widget.Clock(foreground = colors[2],background = colors[4],format = "%A, %B %d - %H:%M "),

    widget.Image(filename = "~/.config/qtile/img/rem.png", background = colors[0]),
    widget.TextBox(background = colors[0], text=":"),
],
            35,background = colors[0],
            margin=[10, 10, 10, 10]
            
                       ),

bottom=bar.Bar(
[
     widget.TextBox(background = colors[0], text=":"),
    widget.Spacer(background = colors[1]),

    widget.HDDBusyGraph(device = "sda", graph_color = colors[2], fill_color = colors[2],  border_color = colors[1], background = colors[4]),
    widget.ThermalSensor(foreground = colors[2],background = colors[4],threshold = 90,fmt = 'Temp: {}',padding = 5),
    widget.CPU (foreground=colors[2],background=colors[4],),
    widget.Memory(
        foreground = colors[2],
        background = colors[4],
        mouse_callbacks = {'Button1': lambda: qtile.cmd_spawn(terminal + ' -e btop')},
        fmt = 'Ram: {}',padding = 5),
    widget.Net(interface = "enp2s0f5",format = 'Net: {down} ↓↑ {up}',foreground = colors[2],background = colors[4],padding = 5),
    widget.Battery(
            format = " {percent:2.0%} ({hour:d}:{min:02d})",
            **widget_defaults,foreground = colors[2],background = colors[4]
        ),
    widget.Wlan(interface = "wlp3s0", foreground = colors[2],background = colors[4]),

    widget.Spacer(background = colors[1]),
    widget.TextBox(background = colors[0], text=":"),

],
        35,background = colors[0],
        margin=[10, 10, 10, 10]
        ),
    ),
]
#####################################################################################################################

#####################################################################################################################
mouse = [
    Drag([mod, "shift"], "Button1", lazy.window.set_position_floating(),   start=lazy.window.get_position()),
    Drag([mod, "shift"], "Button3", lazy.window.set_size_floating(),       start=lazy.window.get_size()),
    Click([mod, "shift"], "Button2", lazy.window.bring_to_front())
]
#####################################################################################################################

#####################################################################################################################
dgroups_key_binder = None
dgroups_app_rules = []  # type: List
follow_mouse_focus = True
bring_front_click = False
cursor_warp = False

floating_layout = layout.Floating(
	float_rules=[
	    *layout.Floating.default_float_rules,
		  # gitk
		    Match(wm_class='confirmreset'),
		  # gitk
		    Match(wm_class='makebranch'),
		  # gitk
		    Match(wm_class='maketag'),
		  # ssh-askpass
		    Match(wm_class='ssh-askpass'),  
		  # gitk
		    Match(title='branchdialog'),
		  # GPG key password entry  
            Match(title='pinentry') ])
#####################################################################################################################

#####################################################################################################################
os.popen("sh /home/archkubi/.config/qtile/autostart.sh")
#####################################################################################################################

#####################################################################################################################
auto_fullscreen = True
focus_on_window_activation = "focus" #smart , focus
reconfigure_screens = True
auto_minimize = True
wmname = "LG3D"  #
#####################################################################################################################
