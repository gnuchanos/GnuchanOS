# path ~/.config/GnuChanSS/GnuChanSS.py
#
# WARNING: bu gercek bir Python betigi degil, GnuChanWM'nin ayar dosyasiyla
# ayni tadda bir AYAR dosyasidir. GnuChanSS bunu okuyup efektini secer.
#
# Ekran koruyucunun ne gosterecegini ve ne zaman baslayacagini burada
# ayarlarsin. Begendigin degeri degistir, kaydet, bir sonraki calismada gecerli
# olur.

gcl_SS.Effect(
    # Efektin adi. Iki tane hazir geliyor:
    #   "pipe"    -> klasik boru/cubuk ekran koruyucu, mor tonlarda
    #   "3dwall"  -> donen 3 boyutlu duvar/tugla deseni
    Name="pipe",

    # Efektin ana rengi ve arkaplan rengi. Mor odakli varsayilanlar.
    PrimaryColor="#d400ff",
    BackgroundColor="#27022b",

    # Kac saniye fare/klavye bos kalinca baslasin. 300 = 5 dakika.
    IdleSeconds=300,

    # Video oynarken ekran koruyucunun calismamasi icin, oturum D-Bus'unda
    # org.freedesktop.ScreenSaver adini sahiplenir. Firefox / Chromium / mpv /
    # VLC video oynatirken bu ismi Inhibit ile kilitler; kilit aktifken SS
    # baslamaz. True birak. (D-Bus derlenmemisse yok sayilir.)
    Inhibit=True,
)
