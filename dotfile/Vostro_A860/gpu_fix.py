#!/usr/bin/env python3
# =============================================================================
# gpu_fix.py - donanim hizlandirmayi Dell Vostro A860 (Intel GMA X3100) uzerinde
# geri acar. Tek dosya, yalnizca standart kutuphane, parametresiz calistirma
# kurulumun tamamidir.
#
# SORUN
# -----
# Bu makinede oyun 2 FPS kosuyordu ve sebep yavas GPU degil, GPU'nun HIC
# KULLANILMAMASIYDI. Olculdu:
#
#   /var/log/Xorg.0.log
#       (**) modeset(0): glamor disabled
#       (II) AIGLX: Screen 0 is not DRI2 capable
#       (II) IGLX: Loaded and initialized swrast
#       (II) GLX: Initialized DRISWRAST GL provider for screen 0
#
#   glxinfo -B
#       OpenGL renderer string: llvmpipe (LLVM 19.1.7, 128 bits)
#
# `DRISWRAST` + `llvmpipe`, cizimin GPU'da degil CPU'da yapildigi anlamina
# gelir. glxinfo 4.5 gosterir ama bu llvmpipe'in SAHTE surumudur; yazilim
# rasterizer'i yeni GL surumlerini taklit edebildigi icin donanim varmis gibi
# gorunur. Gercek donanim yolu `crocus`tur ve o yol kullanilmiyordu.
#
# NEDEN KAPALI: /etc/X11/xorg.conf.d/99-gnuchan-glamor.conf dosyasi GLAMOR'u
# bilerek kapatir:
#
#       Option "AccelMethod" "none"
#
# Cunku Mesa 25 + 965GM ikilisinde X server, libglamoregl uzerinden
# libgallium'a girip signal 6 ile COKUYORDU; coken X server tum pencereleri
# birlikte goturur. O karar o gun DOGRUYDU: ayakta kalan yavas bir sistem,
# coken hizli bir sistemden iyidir.
#
# AMA SONUCU AGIR OLDU. `AccelMethod none` yalnizca 2B hizlandirmayi degil,
# DRI2 saglayicisini da kaldirir. DRI2 yoksa GLX istemcileri (yani bu oyun)
# donanim yolunu BULAMAZ ve llvmpipe'a duser. 2 FPS'in tek sebebi budur.
#
# COZUM
# -----
# GLAMOR'dan gecmeyen bir hizlandirma yolu kullanmak. xf86-video-intel surucusu
# SNA ve UXA adinda iki hizlandirma mimarisi tasir; ikisi de GALLIUM/GLAMOR'dan
# GECMEZ. Yani cokmeye sebep olan cagri zinciri (modesetting -> glamor ->
# gallium) tamamen devre disi kalir, ama surucu yine DRI2 saglar ve GLX
# istemcileri donanim `crocus` surucusunu kullanir.
#
# Bu makinede kurulu olan surucu tam olarak budur:
#       xserver-xorg-video-intel  2:2.99.917+git20210115-1
#
# Bu bir VARSAYIMDIR, kanit degil: cokme glamor yolundaydi, bu script o yolu
# kaldirir. Sonucu olcmek icin `--status` kullanin; acilmazsa `--revert` ile
# eski haline donulur ve cokme tekrarlarsa `--uxa` denenir.
#
# KULLANIM
# --------
#     sudo python3 gpu_fix.py            # donanim yolunu kur (SNA)
#     python3 gpu_fix.py --status        # simdi hangi yol kullaniliyor
#     sudo python3 gpu_fix.py --uxa      # SNA yerine UXA dene
#     sudo python3 gpu_fix.py --revert   # eski (yazilim) hale don
#
# KURULDUKTAN SONRA
# -----------------
# X yeniden baslatilmali. Sonra:
#
#     python3 gpu_fix.py --status
#
# Beklenen: `Mesa Intel(R) 965GM (CL)`. Hala `llvmpipe` yaziyorsa yeni config
# okunmamis demektir.
#
# X ACILMAZSA
# -----------
# Yanlis bir X config'i X'i hic baslatmayabilir ve ekran siyah kalabilir. Bu
# durumda grafik oturum degil, bir TTY gerekir:
#
#     Ctrl+Alt+F3        (F1..F6 arasi biri)
#     kullanici adi / sifre ile giris
#     sudo python3 /yol/gpu_fix.py --revert
#     sudo reboot
#
# Bu yuzden script, dosyayi yazmadan ONCE eski halini yanina `.backup` olarak
# kopyalar ve `--revert` o kopyayi geri koyar.
#
# NE DEGISTIRIR
# -------------
# Yazdigi TEK dosya:
#
#     /etc/X11/xorg.conf.d/99-gnuchan-glamor.conf
#
# Baska hicbir dosyaya, cekirdek parametresine ya da GRUB'a dokunmaz.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

#: Bu scriptin sahip oldugu X yapilandirmasi. GnuchanOS'un dotfile kurulumu da
#: bu dosyayi yazar; ikisi ayni dosya uzerinde calisir, cakismaz.
CONFIG_PATH = Path("/etc/X11/xorg.conf.d/99-gnuchan-glamor.conf")

#: Eski halin saklandigi yer. `--revert` buradan geri koyar.
BACKUP_PATH = CONFIG_PATH.with_name(CONFIG_PATH.name + ".backup")

#: Donanim yolunu acan config. `Driver "intel"` + SNA: Intel'in kendi
#: hizlandirmasi, gallium/glamor'dan gecmez, DRI2 saglar.
CONFIG_SNA = """\
# GnuChanWM / GnuchanOS: hardware acceleration on the Intel 965GM (GMA X3100).
#
# Bu dosyayi gpu_fix.py yazdi. Oyunun okudugu GL surucusu llvmpipe (yazilim)
# yerine crocus (donanim) oldugu surece burada kalmalidir.
#
# `AccelMethod sna` ONEMLIDIR: X server'in 2B hizlandirmasi GLAMOR yerine
# Intel'in kendi SNA mimarisiyle yapilir. GLAMOR, Mesa 25 ile bu GPU'da
# libgallium icinde cokuyordu; SNA o cagri zincirine hic girmez, ama surucu
# yine DRI2 saglar ve GLX istemcileri donanim surucusunu kullanir.
#
# Geri almak icin:  sudo python3 gpu_fix.py --revert
Section "Device"
    Identifier "GnuchanOS Intel"
    Driver "intel"
    Option "AccelMethod" "sna"
    Option "TearFree" "true"
EndSection
"""

#: SNA bu GPU'da sorun cikarirsa denenecek ikinci yol. UXA, SNA'dan onceki
#: hizlandirma mimarisidir ve 965GM'de daha yasli, daha cok sinanmistir.
CONFIG_UXA = """\
# GnuChanWM / GnuchanOS: hardware acceleration on the Intel 965GM (GMA X3100).
#
# Bu dosyayi gpu_fix.py yazdi (--uxa). SNA yerine UXA: ayni sekilde
# GLAMOR'dan gecmez, ama daha eski ve 965GM'de daha cok sinanmis bir yoldur.
#
# Geri almak icin:  sudo python3 gpu_fix.py --revert
Section "Device"
    Identifier "GnuchanOS Intel"
    Driver "intel"
    Option "AccelMethod" "uxa"
EndSection
"""

#: `--revert`in yazdigi hal: donanim yolunu bilerek kapatan yapilandirma.
#: Yedek bulunamazsa bilinen iyi hal olarak bu yazilir, cunku dosyayi silmek
#: X'i belirsiz bir varsayilana birakir; bu ise tam olarak olculmus haldir.
CONFIG_SOFTWARE = """\
# GnuChanWM / GnuchanOS: GLAMOR kapatildi.
#
# gpu_fix.py --revert bu dosyayi bu hale getirdi. X server, modesetting
# surucusu uzerinden libglamoregl'den libgallium'a girip signal 6 ile
# cokuyordu. `AccelMethod none` ile 2B isler yazilimda yapilir: yavas, ama
# server ayakta kalir.
#
# DIKKAT: bu ayar DRI2'yi de kaldirir, yani GLX istemcileri (oyun dahil)
# llvmpipe YAZILIM rasterizer'ina duser. Donanimi geri acmak icin:
#     sudo python3 gpu_fix.py
Section "Device"
    Identifier "GnuchanOS Intel"
    Driver "modesetting"
    Option "AccelMethod" "none"
EndSection
"""

#: DRI2 saglayicisini sunan, dolayisiyla GLX istemcilerine donanim yolunu acan
#: X surucu modulu. Yoksa `Driver "intel"` X'in acilmasini engeller.
INTEL_DDX = Path("/usr/lib/xorg/modules/drivers/intel_drv.so")


# --- cikti --------------------------------------------------------------------


class Log:
    """Ilerleme ciktisi. Sessiz mod yok: scriptin ayari yok, her calistirma ayni
    adimlari yazar."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- kabuk -------------------------------------------------------------------


def run(command: list[str], capture: bool = False) -> subprocess.CompletedProcess[str]:
    """Bir komutu calistir; cikti yalnizca istenirse tutulur.

    Hicbir sey kabuktan gecmez: her komut bir listedir, yani bosluklu bir yol
    iki argumana bolunemez.
    """
    if capture:
        return subprocess.run(command, check=False, capture_output=True, text=True)
    return subprocess.run(command, check=False, text=True)


def which(name: str) -> str | None:
    return shutil.which(name)


def is_root() -> bool:
    """Bu surec /etc'ye yazabilir mi.

    `os.geteuid` Windows'ta yoktur; `getattr` ile okunmasi modulu orada da
    import edilebilir kilar, ki bu scriptin mantigi her yerde incelenebilsin.
    """
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc'ye yazmak icin root ol; degilse sudo ile yeniden calistir.

    Yazilan her sey root'un oldugu icin normal kullanici olarak anlamli bir
    kismi calisma yoktur. sudo yoksa acik bir hata verilir, sessizce devam
    edilmez.
    """
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: bu script /etc/X11 altina yazar; root olarak calistirin "
            "(su -c 'python3 gpu_fix.py')"
        )
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        dict(os.environ),
    )


# --- durumu okuma -------------------------------------------------------------


class GpuState:
    """Bu makinenin GPU ve GL durumu."""

    def __init__(self) -> None:
        self.gpu_name = self._gpu_name()
        self.accel_disabled = self._accel_disabled()
        self.dri_driver = self._dri_driver()
        self.gl_renderer = self._gl_renderer()

    @staticmethod
    def _gpu_name() -> str:
        """lspci'den VGA satiri, yoksa bilinmiyor."""
        if which("lspci") is None:
            return "(lspci yok)"
        result = run(["lspci"], capture=True)
        for line in result.stdout.splitlines():
            lowered = line.lower()
            if "vga" in lowered or "display" in lowered:
                _, _, name = line.partition(": ")
                return name.strip() or line.strip()
        return "(VGA aygiti bulunamadi)"

    @staticmethod
    def _accel_disabled() -> bool:
        """Bizim config dosyamiz hizlandirmayi kapatiyor mu.

        Yalnizca bu scriptin sahip oldugu dosyaya bakilir; baska bir config'in
        ne yazdigi bu scripti ilgilendirmez, o kullanicinin kendi dosyasidir.
        """
        try:
            text = CONFIG_PATH.read_text(encoding="utf-8")
        except OSError:
            return False
        for line in text.splitlines():
            stripped = line.strip().lower()
            if stripped.startswith("option") and "accelmethod" in stripped:
                return "none" in stripped
        return False

    @staticmethod
    def _dri_driver() -> str:
        """Xorg log'undan yuklenen DRI surucusu (crocus / swrast).

        Log okunamazsa bos doner; cagiran taraf bunu "bilinmiyor" sayar.
        """
        for path in (Path("/var/log/Xorg.0.log"), Path("/var/log/Xorg.1.log")):
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for line in text.splitlines():
                if "DRI driver" in line or "AIGLX: Loaded and initialized" in line:
                    _, _, driver = line.rpartition(" ")
                    if driver:
                        return driver.strip()
        return ""

    @staticmethod
    def _display() -> str | None:
        """Calisan X ekraninin DISPLAY degeri, yoksa None.

        glxinfo bir X sunucusuna baglanmak ZORUNDADIR. SSH ile calistirilan bir
        script'te DISPLAY bos gelir ve glxinfo hicbir sey okuyamaz - "GL
        renderer okunamadi" cikmasinin tek sebebi budur. Soket dizinine
        bakmak, ortamdan bagimsiz dogru degeri verir.
        """
        existing = os.environ.get("DISPLAY")
        if existing:
            return existing
        socket_dir = Path("/tmp/.X11-unix")
        try:
            sockets = sorted(socket_dir.glob("X*"))
        except OSError:
            return None
        for socket in sockets:
            number = socket.name[1:]
            if number.isdigit():
                return f":{number}"
        return None

    def _gl_renderer(self) -> str:
        """glxinfo'nun bildirdigi renderer.

        Bu ONEMLI: `llvmpipe` gorunuyorsa GL yazilimda kosuyor demektir, yani
        donanim kullanilmiyor; `Mesa Intel` gorunuyorsa donanim yolundadir.
        """
        if which("glxinfo") is None:
            return "(glxinfo yok)"
        display = self._display()
        if display is None:
            return "(X ekrani bulunamadi)"
        environment = dict(os.environ)
        environment["DISPLAY"] = display
        result = subprocess.run(
            ["glxinfo", "-B"], check=False, capture_output=True, text=True, env=environment
        )
        for line in result.stdout.splitlines():
            if "OpenGL renderer string" in line:
                _, _, value = line.partition(":")
                return value.strip()
        return "(okunamadi)"


def report(log: Log, state: GpuState) -> None:
    """Durumu yaz. Hicbir sey degistirmez."""
    log.step("GPU durumu")
    log.detail(f"aygit          {state.gpu_name}")
    log.detail(
        f"hizlandirma    "
        f"{'KAPALI (AccelMethod none)' if state.accel_disabled else 'acik gorunuyor'}"
    )
    log.detail(f"DRI surucusu   {state.dri_driver or '(log okunamadi)'}")
    log.detail(f"GL renderer    {state.gl_renderer}")

    renderer = state.gl_renderer.lower()
    log.note("")
    if "llvmpipe" in renderer or "swrast" in renderer:
        log.note("GL YAZILIMDA kosuyor (llvmpipe). Donanim kullanilmiyor.")
        log.note("Donanim yolunu acmak icin:  sudo python3 gpu_fix.py")
    elif "intel" in renderer or "crocus" in renderer:
        log.note("GL DONANIMDA kosuyor. Kurulum yerinde.")
    else:
        log.note("GL saglayicisi taninmadi; yukaridaki degerlere bakin.")


# --- dosya yazma --------------------------------------------------------------


def backup(log: Log) -> None:
    """Mevcut config'i bir kez yanina kopyala.

    Kopya BIR KEZ alinir ve korunur: ikinci calistirma onu bu scriptin kendi
    ciktisiyla ezmemelidir, cunku saklanmaya deger olan makinenin scripti hic
    gormeden onceki halidir. Zaten varsa dokunulmaz.
    """
    if not CONFIG_PATH.exists():
        log.detail(f"{CONFIG_PATH} yok; yedek alinacak bir sey yok")
        return
    if BACKUP_PATH.exists():
        log.detail(f"yedek zaten var: {BACKUP_PATH}")
        return
    shutil.copy2(CONFIG_PATH, BACKUP_PATH)
    log.detail(f"yedeklendi: {BACKUP_PATH}")


def write_config(log: Log, text: str) -> None:
    """Config'i yaz ve okunabilir yap."""
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(text, encoding="utf-8")
    CONFIG_PATH.chmod(0o644)
    log.detail(f"yazildi: {CONFIG_PATH}")


# --- kurulum / geri alma ------------------------------------------------------


def install(log: Log, accel: str) -> int:
    """Donanim yolunu kur. 0 = basarili."""
    state = GpuState()
    report(log, state)

    # intel DDX yoksa `Driver "intel"` X'i acilmaz birakir; bu, siyah ekranla
    # biten tek hata sinifidir ve onceden haber verilmelidir.
    if not INTEL_DDX.exists() and which("dpkg") is not None:
        if "intel" not in state.gpu_name.lower() and "965" not in state.gpu_name.lower():
            log.warn(f"bu GPU Intel 965GM gibi gorunmuyor: {state.gpu_name!r}")
        log.warn(
            f"intel surucu modulu yok ({INTEL_DDX}). Once kurun:\n"
            "        sudo apt install xserver-xorg-video-intel\n"
            "    Kurulmadan devam etmek X'i acilmaz birakabilir."
        )
        return 1

    log.step("X yapilandirmasi yaziliyor")
    backup(log)
    write_config(log, CONFIG_UXA if accel == "uxa" else CONFIG_SNA)
    log.detail(f"AccelMethod {accel.upper()}")

    log.note("")
    log.note("Kurulum tamam. Simdi X'i yeniden baslatin:")
    log.note("    sudo systemctl restart lightdm     # ya da sddm/gdm, kurulu olan")
    log.note("    ya da:  sudo reboot")
    log.note("")
    log.note("Sonra dogrulayin:")
    log.note("    python3 gpu_fix.py --status")
    log.note("")
    log.note("X ACILMAZSA: Ctrl+Alt+F3 ile bir TTY'ye gecin ve")
    log.note(f"    sudo python3 {Path(__file__).resolve()} --revert")
    return 0


def revert(log: Log) -> int:
    """Eski hale don.

    Yedek varsa o geri konur. Yoksa bilinen iyi hal yazilir: hizlandirmayi
    kapatan, olculmus ve ayakta kalan yapilandirma. Dosyayi silmek de bir
    secenekti, ama o zaman X belirsiz bir varsayilana duser ve cokme geri
    gelirse sebebi okunacak bir dosya kalmaz.
    """
    log.step("Geri aliniyor")
    if BACKUP_PATH.exists():
        shutil.copy2(BACKUP_PATH, CONFIG_PATH)
        CONFIG_PATH.chmod(0o644)
        log.detail(f"{BACKUP_PATH} -> {CONFIG_PATH} geri konuldu")
    else:
        write_config(log, CONFIG_SOFTWARE)
        log.detail("yedek yok; bilinen yazilim yapilandirmasi yazildi")

    log.note("")
    log.note("Eski hale donuldu. X'i yeniden baslatin:")
    log.note("    sudo systemctl restart lightdm     # ya da reboot")
    log.note("Beklenen sonuc: GL yine llvmpipe (yazilim), yani yavas ama stabil.")
    return 0


def usage() -> None:
    print(
        "kullanim: python3 gpu_fix.py [--status | --revert | --uxa]\n"
        "\n"
        "  (parametresiz)  donanim yolunu kur (intel surucusu, SNA hizlandirma)\n"
        "  --status        simdiki GPU/GL durumunu yaz, hicbir sey degistirme\n"
        "  --revert        eski hale don (yazilim / llvmpipe)\n"
        "  --uxa           kurarken SNA yerine UXA hizlandirmasini sec\n"
    )


def main() -> int:
    log = Log()
    arguments = sys.argv[1:]

    if "--help" in arguments or "-h" in arguments:
        usage()
        return 0

    if "--status" in arguments:
        report(log, GpuState())
        return 0

    if "--revert" in arguments:
        ensure_root(log)
        return revert(log)

    unknown = [a for a in arguments if a != "--uxa"]
    if unknown:
        usage()
        return 2

    ensure_root(log)
    return install(log, "uxa" if "--uxa" in arguments else "sna")


if __name__ == "__main__":
    raise SystemExit(main())
