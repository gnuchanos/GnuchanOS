# path ~/.config/GnuChanFetch/GnuChanFetch.py
#
# WARNING: bu gercek bir Python betigi degil, GnuChanWM'nin ayar dosyasiyla
# ayni tadda bir AYAR dosyasidir. GnuChanFetch bunu okuyup ne gosterecegini
# secer.
#
# GnuChanFetch, neofetch gibi calisir: makinenin bilgilerini yazar. Tek farki,
# bilgilerin sol tarafinda bir .png resmi gosterir — varsayilan olarak bu
# dosyanin yanindaki logo.png. Neyi, hangi sirayla ve hangi renklerle
# gosterecegini burada ayarlarsin. Begendigin degeri degistir, kaydet, bir
# sonraki calistirmada gecerli olur.
#
# DIKKAT: resmi cizmek icin 24-bit renk (terminalin gercek renkleri) gerekir.
# Her terminal bunu destekler; desteklemeyen bir terminalde renkler yakin
# tonlara duser.

gcl_Fetch.Main(
    # Solda gosterilecek resim. "~" ev dizinini temsil eder. Kurulum betigi
    # logo.png'yi tam bu yola koyar, o yuzden elle bir sey yapmana gerek yok.
    # Bos birakirsan ("") resim cizilmez, bilgiler tek basina yazilir.
    Image="~/.config/GnuChanFetch/logo.png",

    # Resmin kac satir yuksekliginde cizilecegi. Resim bu yukseklige gore
    # olceklenir, genisligi kendi oranindan gelir — yani hicbir zaman
    # gerdirilmez, sekli bozulmaz. 16 satir iyi bir baslangictir.
    ImageRows=16,

    # Resim ile bilgiler arasinda kac bos sutun birakilacagi.
    Gap=3,

    # Resmin saydam (seffaf) yerlerinin uzerine cizilecegi renk. Logonun
    # arkaplani saydamsa, terminalin kendi rengi yerine bu renk gorunur.
    # Masaustunun en koyu moru.
    ImageBackground="#1c0532",

    # Gosterilecek bilgi satirlari, tam bu sirayla. Kullanilabilecek isimler:
    #   "model"    -> bilgisayarin marka ve modeli (orn. Dell Inc. Vostro A860)
    #   "user"     -> kullanici adi
    #   "host"     -> makine adi
    #   "os"       -> isletim sistemi (Debian surumu vb.)
    #   "kernel"   -> cekirdek surumu
    #   "server"   -> goruntu sunucusu: protokol ve program
    #                 (orn. "Wayland (sway)" ya da "X11 (XLibre)")
    #   "uptime"   -> ne zamandir acik
    #   "packages" -> kurulu paket sayisi (dpkg)
    #   "shell"    -> kabuk
    #   "wm"       -> pencere yoneticisi (orn. GnuChanWM)
    #   "dm"       -> giris yoneticisi (orn. GnuChanDM / lightdm)
    #   "theme"    -> GTK tema adi (orn. GnuChanTheme)
    #   "icons"    -> ikon tema adi (orn. GnuChanIcon)
    #   "cursor"   -> fare imleci tema adi (orn. GnuChanMouseIcons)
    #   "terminal" -> terminal programi
    #   "cpu"      -> islemci
    #   "gpu"      -> ekran karti (orn. Intel Corporation Mobile GM965)
    #   "memory"   -> bellek kullanimi
    #   "disk"     -> disk kullanimi
    # Bir satiri buradan cikarirsan gosterilmez. Bilinmeyen bir isim yazarsan
    # o satir atlanir (ve hata stderr'e yazilir), program durmaz.
    Fields=[
        "model",
        "user",
        "host",
        "os",
        "kernel",
        "server",
        "uptime",
        "packages",
        "shell",
        "wm",
        "dm",
        "theme",
        "icons",
        "cursor",
        "terminal",
        "cpu",
        "gpu",
        "memory",
        "disk",
    ],

    # Basligin altindaki renk seridi. Her renk iki bosluk genisliginde bir blok
    # olarak cizilir. Masaustunun mor paleti.
    Colours=[
        "#1c0532",
        "#32143f",
        "#542080",
        "#6d28d9",
        "#9333ea",
        "#a855f7",
        "#c77dff",
        "#d8a4ff",
    ],
)
