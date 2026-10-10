# path ~/.config/GnuChanDock/GnuChanDock.py
#
# WARNING: bu gercek bir Python betigi degil, bir AYAR dosyasidir.
#
# GnuChanDock, ekranin altinda duran bir ikon serididir; Apple'in dock'u gibi
# calisir. Soldan saga:
#   1. AYAR ikonu    — her zaman ilk. Su an bir sey YAPMIYOR.
#   2. TERMINAL      — her zaman ikinci. Ikonu logo.png, altinda "terminal".
#   3. ACIK PROGRAMLAR — o an acik pencereler, kendi adlari ve ikonlariyla.

gcl_Dock.Main(
    Background="#1a0b2e",
    BackgroundEdge="#7b2cbf",
    Field="#241033",
    Text="#e0c3fc",
    Accent="#c77dff",

    IconSize=48,
    Gap=30,
    Padding=10,
    Margin=6,
    Corner=16,

    Magnify=18,
    MagnifyReach=1,

    Font="monospace:pixelsize=15",
    LabelEnabled=True,

    SettingsEnabled=True,
    SettingsIcon="~/.config/GnuChanDock/settings.png",
    SettingsLabel="settings",
    SettingsCommand="GnuChanSettings",

    TerminalEnabled=True,
    TerminalIcon="~/.config/GnuChanDock/logo.png",
    TerminalLabel="terminal",
    TerminalCommand="GnuChanTerm",

    ShowRunning=True,
)
