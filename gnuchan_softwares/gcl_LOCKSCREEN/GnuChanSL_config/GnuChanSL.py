# path ~/.config/GnuChanSL/GnuChanSL.py
#
# WARNING: bu gercek bir Python betigi degil, GnuChanWM'nin ayar dosyasiyla
# ayni tadda bir AYAR dosyasidir. GnuChanSL bunu okuyup kilit ekranini boyar.
#
# Kilit ekraninin renklerini, yazilarini ve saat bicimini burada ayarlarsin.
# Begendigin degeri degistir, kaydet, bir sonraki kilitlemede gecerli olur.

gcl_SL.Lock(
    # Renkler. Mor odakli; oturumun geri kalaninin paletiyle ayni.
    Background="#0d0512",   # tum ekran
    Panel="#1b0c22",        # parolanin icinde oturdugu kutu
    PanelEdge="#7b2cbf",    # kutunun cercevesi
    Field="#241033",        # parola alani
    Text="#e0c3fc",         # baslik
    TextMuted="#9d7bba",    # alt yazi ve "Password:" etiketi
    Accent="#d400ff",       # saat ve alan cercevesi
    Wrong="#ff4d6d",        # yanlis parolada yanip sonen renk

    # Yazinin tipi ve boyutu.
    FontFamily="monospace",
    FontSize=14,

    # Yazilar. Title bos birakilirsa oturum sahibinin kullanici adi yazilir.
    Title="",               # bos = kullanici adi
    Subtitle="Enter your password",
    Prompt="Password:",

    # Saat. Bos birakilirsa hic saat gosterilmez (strftime bicimi).
    ClockFormat="%H:%M",

    # Parola yazilirken her karakter icin bir yildiz gosterilsin mi.
    ShowPasswordDots=True,

    # Yanlis parolada kirmizi vurgunun kac saniye kaldigi.
    WrongSeconds=2,
)
