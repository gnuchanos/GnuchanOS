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
# AMD, Intel ve ESKI NVIDIA kartlar acik kaynak suruculerle (amdgpu, i915,
# nouveau) calisir ve o yollar live ISO'da HAZIRDIR: gerekli cekirdek
# firmware'i paket listesine eklenmistir (firmware-amd-graphics,
# firmware-intel-graphics, firmware-nvidia-graphics). Bir NVIDIA karti takili
# bir makinede ISO bu haliyle acilir; masaustu nouveau ile gelir.
#
# Ama NVIDIA'nin TAM performans veren surucusu `nvidia-driver` ozeldir ve
# live ISO'ya gomulemez. Iki nedeni var ve ikisi de gercektir:
#
#   1. SURUM KART NESLINE BAGLIDIR. Tek bir sistemde ayni anda yalnizca BIR
#      NVIDIA surucu surumu olabilir; surumler birbirini dislar. Guncel surucu
#      Maxwell (GTX 900) ve sonrasini destekler; Kepler (GTX 600/700) icin
#      nvidia-legacy-470xx, Fermi ve oncesi icin nvidia-legacy-390xx gerekir.
#      "GTX 1050 Ti ve oncesi" tek bir paketle karsilanamaz - hangisi oldugu
#      karta gore degisir. Bu yuzden karari Debian'in kendi araci `nvidia-detect`
#      verir ve bu script onun soyledigini kurar.
#
#   2. NOUVEAU ILE CATISIR. nvidia-driver kurulunca nouveau'yu blacklist eder.
#      Bir live ISO'da bu geri donusu olmayan bir bahistir: kart listede yoksa
#      (cok yeni Blackwell ya da cok eski bir kart) siyah ekran kalir ve
#      nouveau'ya donus yoktur. Bu yuzden ISO'da VARSAYILAN nouveau'dur; ozel
#      surucu, kullanicinin kendi makinesinde actigi bir adimdir.
#
# Bu script tam olarak o adimdir: karti okur, Debian'in `nvidia-detect` araciyla
# DOGRU surumun hangisi oldugunu ogrenir ve onu kurar - boylece hem GTX 1050 Ti
# (Pascal, guncel surucu) hem de daha eski bir kart (legacy surucu) tek bir
# komutla dogru sekilde kurulur. Karari elle vermez, Debian'in tablosuna
# birakir: kart listesi Debian ile birlikte guncellenir.
#
# NE DEGISTIRIR
#     apt)  nvidia-detect, ardindan onun onerdigi surucu paketi
#           (nvidia-driver ya da nvidia-legacy-XXXX-driver) ve
#           linux-headers-amd64 (DKMS cekirdek modulunu derleyebilsin diye)
#     nouveau otomatik blacklist edilir (surucu paketinin kendisi yapar)
#
# --revert ozel surucuyu kaldirir ve nouveau geri gelir; kurulan cekirdek
# modulu de paketleriyle birlikte gider.
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
        self.detect_present = which(DETECT_TOOL) is not None
        self.recommended = self._recommended()
        self.proprietary_installed = self._proprietary_installed()

    def _cards(self) -> list[str]:
        """lspci'nin NVIDIA satirlari; hic yoksa bos liste."""
        if which("lspci") is None:
            return []
        result = run([tool("lspci"), "-nn"], capture=True)
        found: list[str] = []
        for line in result.stdout.splitlines():
            low = line.lower()
            if "vga" in low or "3d" in low or "display" in low:
                if NVIDIA_VENDOR_ID in line.lower():
                    _, _, name = line.partition(": ")
                    found.append(name.strip() or line.strip())
        return found

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
    log.detail(f"cekirdek          {state.kernel()}")
    log.detail(f"nvidia-detect     {'var' if state.detect_present else 'yok'}")
    log.detail(f"onerilen surucu   {state.recommended or '(henuz bilinmiyor)'}")
    log.detail(
        f"ozel surucu       {'kurulu' if state.proprietary_installed else 'kurulu degil'}"
    )

    log.note()
    if not state.cards:
        log.note("Bu makinede NVIDIA karti yok; ozel surucuye gerek yok.")
        log.note("AMD/Intel/NVIDIA-eski kartlar acik kaynak suruculerle calisir ve")
        log.note("gerekli firmware live ISO'dadir.")
    elif state.proprietary_installed:
        log.note("Ozel NVIDIA surucusu kurulu. Tam performans icin etkindir.")
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
            "(surucu henuz yok) ya da taninmiyor. Acik kaynak nouveau ile "
            "kullanmaya devam edebilirsiniz."
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

    final = GpuState()
    log.note()
    if final.proprietary_installed:
        log.note("Ozel NVIDIA surucusu kuruldu. Tam performans etkin olacak.")
    else:
        log.note("Paketler kuruldu; cekirdek modulu bir sonraki acilista yuklenir.")
    log.note()
    log.note("ONEMLI: X yeniden baslatilmali (ya da makine).")
    log.note("    sudo systemctl restart gnuchandm     # ya da: sudo reboot")
    log.note()
    log.note("Geri almak icin:  sudo python3 install_nvidia.py --revert")
    return 0


def revert(log: Log) -> int:
    """Ozel surucuyu kaldir; nouveau geri gelir.

    Kaldirilacak paket adi surume gore degistigi icin once ne kurulu oldugu
    bulunur: nvidia-detect'in onerisi ve kurulu nvidia surucu paketleri
    taranir. Ardindan o paketler apt ile kaldirilir; surucu paketi cekilince
    nouveau blacklist'i de kalkar ve bir sonraki oturumda nouveau yuklenir.
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
        log.note("Geri alinacak bir sey yok.")
        return 0

    log.detail("kaldirilacak: " + ", ".join(installed))
    command = [tool("apt-get"), "purge", "-y", *installed]
    if run(command, environment=apt_environment()).returncode != 0:
        log.warn("paketler kaldirilamadi")
        return 1

    run([tool("depmod"), "-a"], capture=True)
    log.note()
    log.note("Ozel surucu kaldirildi; nouveau bir sonraki oturumda geri gelir.")
    log.note("X'i yeniden baslatin:  sudo systemctl restart gnuchandm")
    return 0


# --- giris -------------------------------------------------------------------


def usage() -> None:
    print(
        "kullanim: python3 install_nvidia.py [--status | --revert]\n"
        "\n"
        "  (parametresiz)  kart icin dogru ozel surucuyu kur\n"
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
