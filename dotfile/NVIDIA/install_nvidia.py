#!/usr/bin/env python3
# =============================================================================
# install_nvidia.py - NVIDIA ekran karti icin ozel (proprietary) surucu kurulumu
#
#     sudo python3 install_nvidia.py            kart icin dogru surucuyu kur
#     python3 install_nvidia.py --status        durumu yaz, hicbir sey degistir
#     sudo python3 install_nvidia.py --revert   ozel surucuyu kaldir
#
# NEDEN AYRI BIR SCRIPT
# ---------------------
# AMD ve Intel kartlar acik kaynak suruculerle (amdgpu, i915/xe) calisir ve o
# yollar live ISO'da HAZIRDIR: gerekli cekirdek firmware'i paket listesine
# eklenmistir (firmware-amd-graphics, firmware-intel-graphics). Bu iki uretici
# icin ek bir adim GEREKMEZ - kart takilir takilmaz tam hizda acilir.
#
# NVIDIA'nin TAM performans veren surucusu `nvidia-driver` ise OZELdir ve
# live ISO'ya gomulemez. Iki nedeni var ve ikisi de gercektir:
#
#   1. SURUM KART NESLINE BAGLIDIR. Tek bir sistemde ayni anda yalnizca BIR
#      NVIDIA surucu surumu olabilir; surumler birbirini dislar. Guncel surucu
#      Maxwell (GTX 900) ve sonrasini destekler - GTX 1050 Ti BURADADIR;
#      Kepler (GTX 600/700) icin nvidia-legacy-470xx, Fermi ve oncesi icin
#      nvidia-legacy-390xx gerekir. "GTX 1050 Ti ve oncesi" TEK bir paketle
#      karsilanamaz - hangisi oldugu karta gore degisir. Bu yuzden karari
#      Debian'in kendi araci `nvidia-detect` verir ve bu script onun soyledigini
#      kurar; kart listesi Debian ile birlikte guncellenir.
#
#   2. NOUVEAU ILE CATISIR. nvidia-driver kurulunca acik kaynak nouveau surucusu
#      blacklist edilir. Bir live ISO'da bu geri donusu olmayan bir bahistir:
#      surucunun kapsamadigi bir karta gomulurse siyah ekran kalir ve nouveau'ya
#      donus yoktur. Bu yuzden ISO nouveau ile acilir (kart HER ZAMAN goruntuye
#      gelir); ozel surucu, kullanicinin kendi makinesinde actigi bir adimdir.
#
# Ozel surucu kurulunca yalnizca o kart tam hiza cikmaz: script ayrica
#   - NVIDIA'nin ZORUNLU cekirdek modu ayarini yazar (`nvidia_drm modeset=1`),
#   - HIBRIT (Optimus) laptopta panelin hangi karta bagli oldugunu bilir ve
#     `fbdev=1` ile konsolu canli tutar; boylece GPU'lu bir laptop da tam
#     hizda acilir, yazilim render'a (llvmpipe) DUSMEZ.
#
# NE DEGISTIRIR
#     apt)  nvidia-detect, ardindan onun onerdigi surucu paketi
#           (nvidia-driver ya da nvidia-legacy-XXXX-driver) ve
#           linux-headers-amd64 (DKMS cekirdek modulunu derleyebilsin diye)
#     /etc/modprobe.d/gnuchan-nvidia.conf
#           `options nvidia_drm modeset=1 fbdev=1` - KMS. NVIDIA 495 ve
#           sonrasinda ZORUNLUDUR: yazilmazsa surucu yuklenir ama X/Wayland onu
#           kullanamaz ve masaustu llvmpipe'a duser.
#     nouveau otomatik blacklist edilir (surucu paketinin kendisi yapar)
#
# --revert ozel surucuyu ve bu scriptin yazdigi modprobe dosyasini kaldirir;
# nouveau geri gelir, kurulan cekirdek modulu de paketleriyle birlikte gider.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

#: Bir NVIDIA kartin PCI uretici kimligi; lspci -nn ciktisinda boyle gorunur.
NVIDIA_VENDOR_ID = "10de"

#: Diger iki ureticinin PCI kimlikleri. Bir makinede NVIDIA ile birlikte
#: gorunurlerse makine HIBRITtir (Optimus laptop): panel iGPU'ya baglidir,
#: NVIDIA kart PRIME offloading icin arkada render eder.
INTEL_VENDOR_ID = "8086"
AMD_VENDOR_ID = "1002"

#: Debian'in "bu karta hangi surucu" sorusunu yanitlayan araci. Karari bu verir:
#: guncel mi legacy mi, hangi legacy serisi. Cihaz kimligi tablosu Debian ile
#: guncellendigi icin yeni kartlar bu script degismeden taninir.
DETECT_TOOL = "nvidia-detect"

#: nvidia-detect cikitisindan cekilecek onerilen paket: "nvidia-driver",
#: "nvidia-legacy-470xx-driver", "nvidia-legacy-390xx-driver" gibi.
RECOMMENDATION = re.compile(r"\b(nvidia-(?:legacy-\d+xx-)?driver)\b")

#: DKMS'in ozel cekirdek modulunu derleyebilmesi icin. Meta paket: her cekirdek
#: guncellemesinde headers'in de gelmesini saglar.
HEADERS_PACKAGE = "linux-headers-amd64"

#: Bu scriptin yazdigi NVIDIA cekirdek-modu yapilandirmasi.
MODPROBE_PATH = Path("/etc/modprobe.d/gnuchan-nvidia.conf")

#: KMS ayari. `modeset=1` NVIDIA 495+ icin ZORUNLUDUR: olmadan surucu yuklenir
#: ama X/Wayland kullanamaz ve masaustu yazilim render'a duser. `fbdev=1` ise
#: karta bagli bir ekran olmadiginda bile calisan bir konsol/framebuffer birakir
#: - HIBRIT bir laptopta normal durum budur (panel iGPU'da, NVIDIA arkada
#: render eder). Ikisi birlikte hem tek-GPU hem Optimus makinede tam hizi verir.
MODPROBE_TEXT = """\
# GnuChanOS / GnuChanDM: NVIDIA cekirdek modu (KMS).
#
# Bu dosyayi install_nvidia.py yazar.
#
# `modeset=1` NVIDIA 495 ve sonrasinda ZORUNLUDUR: yazilmazsa surucu yuklenir
# ama X/Wayland onu kullanamaz ve masaustu yazilim render'a (llvmpipe) duser.
#
# `fbdev=1` karta bagli bir ekran olmadiginda da calisan bir framebuffer
# birakir. HIBRIT bir laptopta normal durum budur: panel Intel/AMD iGPU'ya
# baglidir, NVIDIA kart PRIME offloading icin arkada render eder. Bu olmadan
# konsol ve erken acilis boyle bir makinede bos gelebilir.
options nvidia_drm modeset=1 fbdev=1
"""

#: Dosyayi bu scriptin yazdigini isaretler; `--revert` yalnizca kendi yazdigini
#: silmek icin bunu arar ve bir kullanicinin kendi modprobe dosyasina dokunmaz.
MODPROBE_MARKER = "install_nvidia.py"


# --- cikti -------------------------------------------------------------------


class Log:
    """Ilerleme ciktisi; sessiz mod yok."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str = "") -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- kabuk -------------------------------------------------------------------


def which(name: str) -> str | None:
    return shutil.which(name)


def tool(name: str) -> str:
    """Bir sistem aracinin TAM yolu; sbin once.

    `apt-get`, `lspci` gibi araclar /usr/sbin ya da /usr/bin'de olabilir ve
    normal kullanicinin PATH'inde sbin yoktur; bu yuzden once sbin denenir.
    """
    for directory in ("/usr/sbin", "/sbin", "/usr/bin", "/bin"):
        candidate = Path(directory, name)
        if candidate.is_file():
            return str(candidate)
    return name


def run(command: list[str], capture: bool = False,
        environment: dict | None = None) -> subprocess.CompletedProcess[str]:
    """Komutu calistir; kabuk yok, her komut bir listedir."""
    try:
        if capture:
            return subprocess.run(command, check=False, capture_output=True,
                                  text=True, env=environment)
        return subprocess.run(command, check=False, text=True, env=environment)
    except FileNotFoundError:
        return subprocess.CompletedProcess(
            args=command, returncode=127, stdout="", stderr="command not found"
        )


def apt_environment() -> dict:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def is_root() -> bool:
    """`os.geteuid` Windows'ta yok; getattr ile okumak modulu her yerde import
    edilebilir kilar."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc ve apt ile paket kurmak icin root ol."""
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: bu script apt ve /etc kullanir; root olarak calistirin "
            "(su -c 'python3 install_nvidia.py')"
        )
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        dict(os.environ),
    )


# --- durum -------------------------------------------------------------------


class GpuState:
    """Bu makinenin NVIDIA karti ve surucu durumu."""

    def __init__(self) -> None:
        self.cards = self._cards()
        self.other_gpus = self._other_gpus()
        self.detect_present = which(DETECT_TOOL) is not None
        self.recommended = self._recommended()
        self.proprietary_installed = self._proprietary_installed()

    def _display_lines(self) -> list[str]:
        """lspci'nin ekran denetleyicisi satirlari (VGA/3D/Display)."""
        if which("lspci") is None:
            return []
        result = run([tool("lspci"), "-nn"], capture=True)
        return [
            line for line in result.stdout.splitlines()
            if "vga" in line.lower()
            or "3d" in line.lower()
            or "display" in line.lower()
        ]

    def _cards(self) -> list[str]:
        """lspci'nin NVIDIA satirlari; hic yoksa bos liste."""
        found: list[str] = []
        for line in self._display_lines():
            if NVIDIA_VENDOR_ID in line.lower():
                _, _, name = line.partition(": ")
                found.append(name.strip() or line.strip())
        return found

    def _other_gpus(self) -> list[str]:
        """NVIDIA disindaki ekran denetleyicileri (Intel/AMD).

        Bir tane varsa makine HIBRITtir (Optimus/PRIME): oturum iGPU uzerinde
        kosar, agir is NVIDIA'ya devredilir.
        """
        found: list[str] = []
        for line in self._display_lines():
            lowered = line.lower()
            if NVIDIA_VENDOR_ID in lowered:
                continue
            if INTEL_VENDOR_ID in lowered or AMD_VENDOR_ID in lowered:
                _, _, name = line.partition(": ")
                found.append(name.strip() or line.strip())
        return found

    @property
    def is_hybrid(self) -> bool:
        """NVIDIA ile birlikte bir iGPU var mi."""
        return bool(self.cards) and bool(self.other_gpus)

    def _recommended(self) -> str:
        """nvidia-detect'in onerdigi surucu paketi; yoksa bos dize.

        Arac kurulu degilse ya da karta NVIDIA demiyorsa bos doner; cagiran
        taraf bunu 'once nvidia-detect kur' adimina cevirir.
        """
        if which(DETECT_TOOL) is None:
            return ""
        result = run([tool(DETECT_TOOL)], capture=True)
        match = RECOMMENDATION.search(result.stdout)
        return match.group(1) if match else ""

    @staticmethod
    def _proprietary_installed() -> bool:
        """Ozel surucu kurulu mu. nvidia kernel modulunun varligina bakilir;
        paket adi surume gore degistigi icin isim yerine modul aranir."""
        result = run([tool("modinfo"), "nvidia"], capture=True)
        if result.returncode == 0:
            return True
        return bool(list(Path("/lib/modules").glob("*/kernel/drivers/video/nvidia*")))

    def kernel(self) -> str:
        return run(["uname", "-r"], capture=True).stdout.strip()


def report(log: Log, state: GpuState) -> None:
    """Durumu yaz; hicbir sey degistirmez."""
    log.step("NVIDIA durumu")
    if state.cards:
        for card in state.cards:
            log.detail(f"kart              {card}")
    else:
        log.detail("kart              NVIDIA ekran karti bulunamadi")
    if state.other_gpus:
        for gpu in state.other_gpus:
            log.detail(f"ikinci GPU        {gpu}")
        log.detail("makine            HIBRIT (Optimus/PRIME)")
    log.detail(f"cekirdek          {state.kernel()}")
    log.detail(f"nvidia-detect     {'var' if state.detect_present else 'yok'}")
    log.detail(f"onerilen surucu   {state.recommended or '(henuz bilinmiyor)'}")
    log.detail(
        f"ozel surucu       {'kurulu' if state.proprietary_installed else 'kurulu degil'}"
    )

    log.note()
    if not state.cards:
        log.note("Bu makinede NVIDIA karti yok; ozel surucuye gerek yok.")
        log.note("Intel/AMD kartlar acik kaynak suruculerle (i915/xe, amdgpu) tam")
        log.note("hizda calisir ve gerekli firmware live ISO'dadir.")
    elif state.proprietary_installed:
        log.note("Ozel NVIDIA surucusu kurulu. Tam performans icin etkindir.")
        if state.is_hybrid:
            log.note("Hibrit makine: oturum iGPU'da kosar; agir is icin PRIME")
            log.note("offloading kullanilir (asagida).")
    elif not state.detect_present:
        log.note("Kart var ama karar araci yok. Kurmak icin:")
        log.note("    sudo python3 install_nvidia.py")
    else:
        log.note(f"Kart icin onerilen surucu: {state.recommended or '?'}")
        log.note("Kurmak icin:  sudo python3 install_nvidia.py")


# --- apt ---------------------------------------------------------------------


def package_installed(name: str) -> bool:
    return run([tool("dpkg"), "-s", name], capture=True).returncode == 0


def apt_update(log: Log) -> bool:
    log.step("apt-get update")
    result = run([tool("apt-get"), "update", "-o", "Acquire::Retries=3"],
                 capture=True, environment=apt_environment())
    if result.returncode != 0:
        log.warn("apt-get update basarisiz")
        for line in (result.stdout + result.stderr).splitlines()[-8:]:
            log.warn("  " + line)
        return False
    return True


def install_packages(log: Log, packages: tuple[str, ...]) -> bool:
    if not packages:
        return True
    log.step("Kuruluyor: " + ", ".join(packages))
    command = [tool("apt-get"), "install", "-y", "--no-install-recommends"]
    command += list(packages)
    if run(command, environment=apt_environment()).returncode != 0:
        log.warn("apt-get kuramadi: " + ", ".join(packages))
        return False
    log.detail("kuruldu: " + ", ".join(packages))
    return True


# --- KMS yapilandirmasi ------------------------------------------------------


def write_kms_config(log: Log) -> bool:
    """`nvidia_drm modeset=1 fbdev=1` yaz. NVIDIA 495+ icin ZORUNLU.

    Bu dosya olmadan surucu yuklenir ama X/Wayland onu kullanamaz ve masaustu
    yazilim render'a (llvmpipe) duser - yani kart kurulu olmasina ragmen
    'calismiyor' gorunur. Bu, bu scriptin onledigi ana tuzaktir.
    """
    try:
        MODPROBE_PATH.parent.mkdir(parents=True, exist_ok=True)
        MODPROBE_PATH.write_text(MODPROBE_TEXT, encoding="utf-8")
        MODPROBE_PATH.chmod(0o644)
    except OSError as error:
        log.warn(f"modprobe yapilandirmasi yazilamadi: {error}")
        return False
    log.detail(f"yazildi: {MODPROBE_PATH} (nvidia_drm modeset=1 fbdev=1)")
    return True


def is_our_kms_config() -> bool:
    """modprobe dosyasini bu script mi yazdi."""
    try:
        return MODPROBE_MARKER in MODPROBE_PATH.read_text(
            encoding="utf-8", errors="replace"
        )
    except OSError:
        return False


def remove_kms_config(log: Log) -> None:
    """Bu scriptin yazdigi modprobe dosyasini sil; kullanicininki kalsin."""
    if not MODPROBE_PATH.exists():
        log.detail(f"{MODPROBE_PATH} zaten yok")
        return
    if not is_our_kms_config():
        log.detail(f"{MODPROBE_PATH} bu script yazmadi; dokunulmuyor")
        return
    try:
        MODPROBE_PATH.unlink()
        log.detail(f"silindi: {MODPROBE_PATH}")
    except OSError as error:
        log.warn(f"silinemedi: {error}")


# --- kurulum -----------------------------------------------------------------


def install(log: Log, state: GpuState) -> int:
    """Kart icin dogru ozel surucuyu kur."""
    if not state.cards:
        report(log, state)
        log.note()
        log.note("NVIDIA karti yok; yapilacak bir sey yok.")
        return 0

    # Karar araci yoksa once o kurulur: hangi surumun kurulacagina o karar verir.
    if not state.detect_present:
        if not apt_update(log):
            return 1
        if not install_packages(log, (DETECT_TOOL,)):
            return 1

    state = GpuState()
    report(log, state)

    if not state.recommended:
        log.warn(
            "nvidia-detect bir surucu onerisi vermedi. Kart cok yeni olabilir "
            "(surucu henuz yok) ya da taninmiyor."
        )
        return 1

    log.note()
    log.step(f"Onerilen surucu kuruluyor: {state.recommended}")

    # Cekirdek basliklari DKMS icin SART: nvidia kernel modulu bu basliklara
    # karsi derlenir. Meta paket oldugu icin her cekirdek guncellemesinde de
    # gelir; yoksa surucu bir sonraki cekirdekte derlenemez ve siyah ekran olur.
    needed = [state.recommended]
    if not package_installed(HEADERS_PACKAGE):
        needed.append(HEADERS_PACKAGE)

    if not apt_update(log):
        return 1
    if not install_packages(log, tuple(needed)):
        return 1

    # Paketler kuruldu; simdi KMS ayari. Bu olmadan surucu kurulu ama kullanilamaz.
    log.note()
    log.step("NVIDIA cekirdek modu (KMS) yapilandiriliyor")
    kms_written = write_kms_config(log)

    final = GpuState()
    log.note()
    if final.proprietary_installed:
        log.note("Ozel NVIDIA surucusu kuruldu. Tam performans etkin olacak.")
    else:
        log.note("Paketler kuruldu; cekirdek modulu bir sonraki acilista yuklenir.")
    if not kms_written:
        log.warn(
            "KMS ayari yazilamadi; surucu yuklenir ama X onu kullanamaz ve "
            "masaustu llvmpipe'a duser. Elle ekleyin:\n"
            "    echo 'options nvidia_drm modeset=1 fbdev=1' | "
            "sudo tee /etc/modprobe.d/gnuchan-nvidia.conf"
        )

    if final.is_hybrid:
        log.note()
        log.step("Hibrit (Optimus) makine")
        log.note("Panel Intel/AMD iGPU uzerinde kosar; NVIDIA kart PRIME ile")
        log.note("agir is yuklenince devreye girer. Bir uygulamayi NVIDIA'da")
        log.note("calistirmak icin:")
        log.note("    __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia <uygulama>")
        log.note("Tum GPU'lari listelemek icin:  xrandr --listproviders")

    log.note()
    log.note("ONEMLI: X yeniden baslatilmali (ya da makine).")
    log.note("    sudo systemctl restart gnuchandm     # ya da: sudo reboot")
    log.note()
    log.note("Geri almak icin:  sudo python3 install_nvidia.py --revert")
    return 0


def revert(log: Log) -> int:
    """Ozel surucuyu kaldir; nouveau geri gelir.

    Kaldirilacak paket adi surume gore degistigi icin once ne kurulu oldugu
    bulunur: kurulu nvidia surucu paketleri taranir. Ardindan o paketler apt
    ile kaldirilir; surucu paketi cekilince nouveau blacklist'i de kalkar ve
    bir sonraki oturumda nouveau yuklenir. Bu scriptin yazdigi KMS dosyasi da
    silinir.
    """
    log.step("Kurulu NVIDIA surucu paketleri bulunuyor")
    installed: list[str] = []
    result = run([tool("dpkg-query"), "-W", "-f=${Package}\n"], capture=True)
    for line in result.stdout.splitlines():
        name = line.strip()
        if name == "nvidia-detect":
            continue
        if re.fullmatch(r"nvidia-(?:legacy-\d+xx-)?driver", name) \
           or re.fullmatch(r"nvidia-kernel-dkms", name):
            installed.append(name)

    if not installed:
        log.detail("kurulu ozel surucu paketi yok")
    else:
        log.detail("kaldirilacak: " + ", ".join(installed))
        command = [tool("apt-get"), "purge", "-y", *installed]
        if run(command, environment=apt_environment()).returncode != 0:
            log.warn("paketler kaldirilamadi")
            return 1
        run([tool("depmod"), "-a"], capture=True)

    log.step("KMS yapilandirmasi kaldiriliyor")
    remove_kms_config(log)

    log.note()
    log.note("Ozel surucu kaldirildi; nouveau bir sonraki oturumda geri gelir.")
    log.note("X'i yeniden baslatin:  sudo systemctl restart gnuchandm")
    return 0


# --- giris -------------------------------------------------------------------


def usage() -> None:
    print(
        "kullanim: python3 install_nvidia.py [--status | --revert]\n"
        "\n"
        "  (parametresiz)  kart icin dogru ozel surucuyu kur (KMS dahil)\n"
        "  --status        durumu yaz, hicbir sey degistirme\n"
        "  --revert        ozel surucuyu kaldir, nouveau'ya don\n"
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
    if arguments:
        usage()
        return 2

    ensure_root(log)
    return install(log, GpuState())


if __name__ == "__main__":
    raise SystemExit(main())
