#!/usr/bin/env python3
# =============================================================================
# wifi_fix.py - ASUS X550WAK, Broadcom BCM43142 (14e4:4365) wifi duzeltmesi.
#
# SORUN (kullanici): "wifi ip address gozukmuyor" - makinede kablosuz arayuz HIC
# yok; yalnizca kablo (enp2s0) ile calisiyor. Elle duzeltilemiyor cunku ortada
# yuklenecek bir surucu yok.
#
# TESHIS (bu makinede OLCULDU, tahmin degil)
#   * lspci: 01:00.0 Broadcom BCM43142 802.11b/g/n [14e4:4365] (rev 01).
#   * Bu cip ACIK KAYNAK suruculerle CALISMAZ: b43, brcmsmac ve brcmfmac onu
#     desteklemez. Tek calisan surucu Broadcom'un ozel `wl` surucusudur ve o da
#     `broadcom-sta-dkms` paketinden gelir.
#   * `broadcom-sta-dkms` KURULUDUR (kaynak: /usr/src/broadcom-sta-6.30.223.271)
#     ve /etc/modprobe.d/broadcom-sta-dkms.conf b43/b43legacy/bcma/brcm80211/
#     brcmsmac/ssb modullerini blacklist'e alir. Yani acik kaynak surucu BILEREK
#     kapalidir; geriye yalnizca `wl` kalir.
#   * ARIZANIN KOK NEDENI: calisan cekirdek icin LINUX-HEADERS YOK.
#     uname -r = 6.12.107+deb13-amd64; /usr/src/linux-headers-* hic yok.
#     `dkms` ve `build-essential` kurulu oldugu halde DKMS `wl` modulunu
#     derleyememistir. Sonuc:
#         modinfo wl         -> absent
#         /lib/modules/<K>/updates/dkms/wl.ko  -> yok
#         cfg80211/mac80211  -> absent
#     ve hicbir wlan arayuzu olusmaz. Kok neden budur.
#
#   * NOT: `modinfo`, `lsmod`, `depmod`, `dkms`, `modprobe` /usr/sbin'dedir ve
#     normal kullanicinin PATH'inde DEGILDIR. Bu script hepsini MUTLAK yoldan
#     cagirir; "komut bulunamadi" hatasi bu yuzden olmaz.
#
# COZUM - IKI PARCA
#   1. Cekirdek basliklarini kur (ve meta paketi), sonra DKMS ile `wl`'yi
#      derle. Meta paket (linux-headers-amd64) ONEMLIDIR: her cekirdek
#      guncellemesinde headers'in de gelmesini saglar; yoksa DKMS bir sonraki
#      cekirdekte yine derleyemez ve wifi yine kaybolur.
#         apt-get install -y linux-headers-amd64 dkms build-essential
#         dkms autoinstall
#         modprobe wl
#   2. Kurtarma servisi: acilista ve uykudan donuste (a) wl.ko yoksa
#      `dkms autoinstall` ile yeniden derler, (b) `wl` yuklu degilse yukler.
#      `wl` surucusu askidan sonra bazen radyoyu geri getirmez; bu servis onu
#      geri getirir.
#
# NE DEGISTIRIR
#     (apt)  linux-headers-amd64, dkms, build-essential kurulur
#     /usr/local/sbin/gnuchan-wifi-broadcom-recover
#     /etc/systemd/system/gnuchan-wifi-broadcom-recover.service
#     /etc/systemd/system-sleep/gnuchan-wifi-broadcom-recover
# Var olan dosyalarin eski hali .gnuchan-backup olarak saklanir; --revert
# bunlari geri koyar. APT ile kurulan paketler --revert ile KALDIRILMAZ (baska
# bir cipin de ihtiyaci olabilir); yalnizca bizim yazdigimiz dosyalar gider.
#
# UYARI: --now `wl`'yi derler ve yukler. SSH uzerinden baglanmissan KABLO
# baglantisi (enp2s0) etkilenmez; wifi arayuzu birkac saniye sonra belirir.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

RECOVER_SCRIPT = Path("/usr/local/sbin/gnuchan-wifi-broadcom-recover")
SERVICE_FILE = Path("/etc/systemd/system/gnuchan-wifi-broadcom-recover.service")
SLEEP_HOOK = Path("/etc/systemd/system-sleep/gnuchan-wifi-broadcom-recover")
BACKUP_SUFFIX = ".gnuchan-backup"

# The driver this chip needs, and the DKMS package that provides it.
WIFI_MODULE = "wl"
DKMS_NAME = "broadcom-sta"
DKMS_VERSION = "6.30.223.271"

# Packages the build needs. The meta header package is the important one: it
# pulls the pointer that makes every future kernel ship its headers too.
APT_PACKAGES = ("linux-headers-amd64", "dkms", "build-essential")

WIPHY_DIR = Path("/sys/class/ieee80211")

MANAGED_FILES = (RECOVER_SCRIPT, SERVICE_FILE, SLEEP_HOOK)

RECOVER_CONTENT = """\
#!/bin/bash
# GnuchanOS Broadcom BCM43142 (wl) kurtarma. Acilista ve uyanista calisir.
# Iki ayri is:
#   (1) wl.ko calisan cekirdek icin YOKSA `dkms autoinstall` ile derle;
#   (2) wl yuklu degilse `modprobe wl`.
# Amac: cekirdek guncellemesinden sonra veya askidan donuste wifi'nin kendini
# geri getirmesi. Basarisizlik sessizdir; kablo baglantisi etkilenmez.
set -u

K="$(uname -r)"

# (1) wl.ko var mi? yoksa derlemeyi dene.
if ! ls /lib/modules/"$K"/updates/dkms/wl.ko* >/dev/null 2>&1; then
    /usr/sbin/dkms autoinstall -k "$K" >/dev/null 2>&1 || true
    /usr/sbin/depmod -a "$K" >/dev/null 2>&1 || true
fi

# (2) wl yuklu degilse yukle.
if ! /usr/sbin/lsmod | grep -q '^wl '; then
    /usr/sbin/modprobe wl >/dev/null 2>&1 || true
fi

exit 0
"""

SERVICE_CONTENT = """\
[Unit]
Description=GnuchanOS Broadcom BCM43142 (wl) recovery
After=NetworkManager.service
Wants=NetworkManager.service

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/gnuchan-wifi-broadcom-recover

[Install]
WantedBy=multi-user.target
"""

SLEEP_CONTENT = """\
#!/bin/bash
# systemd: $1 = pre|post, $2 = suspend|hibernate|... Yalnizca uyanista gerekir.
case "${1:-}" in
    post) /usr/local/sbin/gnuchan-wifi-broadcom-recover ;;
esac
exit 0
"""


# --- cikti -------------------------------------------------------------------


class Log:
    """Ilerleme ciktisi; sessiz mod yok."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str) -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- kabuk -------------------------------------------------------------------


def tool(name: str) -> str:
    """Bir sistem aracinin TAM yolu; sbin once, sonra PATH, en son ad.

    `dkms`, `modinfo`, `modprobe` gibi araclar /usr/sbin'dedir ve normal
    kullanicinin PATH'inde bulunmaz. Bosuna "komut yok" hatasi almamak icin
    once sbin denenir. Hicbiri yoksa ad dondurulur ve cagri basarisiz olur —
    ki bu dogru sonucu verir.
    """
    for directory in ("/usr/sbin", "/sbin", "/usr/bin", "/bin"):
        candidate = Path(directory, name)
        if candidate.is_file():
            return str(candidate)
    return name


def run(command: list[str], capture: bool = False,
        environment: dict | None = None) -> subprocess.CompletedProcess[str]:
    """Komutu calistir; kabuk yok, her komut bir listedir.

    Bir komut hic bulunamazsa cokme yerine 127 dondurur; her cagiran
    returncode'a bakiyor. `environment` apt icin gerekir (DEBIAN_FRONTEND).
    """
    try:
        if capture:
            return subprocess.run(command, check=False, capture_output=True,
                                  text=True, env=environment)
        return subprocess.run(command, check=False, text=True, env=environment)
    except FileNotFoundError:
        return subprocess.CompletedProcess(
            args=command, returncode=127, stdout="", stderr="command not found"
        )


def which(name: str) -> str | None:
    return shutil.which(name)


def is_root() -> bool:
    """`os.geteuid` Windows'ta yok; getattr ile okumak modulu her yerde import
    edilebilir kilar."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc ve /usr/local'e yazmak, apt ile paket kurmak icin root ol."""
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: bu script apt ve /etc kullanir; root olarak calistirin "
            "(su -c 'python3 wifi_fix.py')"
        )
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        dict(os.environ),
    )


# --- durum -------------------------------------------------------------------


class WifiState:
    """Bu makinenin wifi ve surucu durumu."""

    def __init__(self) -> None:
        self.kernel = self._kernel()
        self.device = self._device()
        self.headers_present = self._headers_present()
        self.wl_module_present = self._wl_module_present()
        self.wl_ko_present = self._wl_ko_present()
        self.wl_loaded = self._wl_loaded()
        self.wiphy_present = WIPHY_DIR.is_dir() and bool(list(WIPHY_DIR.glob("phy*")))
        self.interface = self._interface()
        self.service_enabled = self._service_enabled()

    @staticmethod
    def _kernel() -> str:
        return run(["uname", "-r"], capture=True).stdout.strip()

    @staticmethod
    def _device() -> str:
        if which("lspci") is None:
            return "(lspci yok)"
        result = run(["lspci", "-nn"], capture=True)
        for line in result.stdout.splitlines():
            low = line.lower()
            if "network" in low or "wireless" in low:
                _, _, name = line.partition(": ")
                return name.strip() or line.strip()
        return "(kablosuz aygit bulunamadi)"

    def _headers_present(self) -> bool:
        """Cekirdek basliklari var mi: ya tam surum ya da en az bir meta.

        DKMS'in derleyebilmesi icin TAM surum basliklari gerekir; meta paket
        (linux-headers-amd64) gelecekteki cekirdekler icin bunu garanti eder.
        """
        exact = Path(f"/lib/modules/{self.kernel}/build")
        if exact.is_dir():
            return True
        return bool(list(Path("/usr/src").glob("linux-headers-*")))

    def _wl_module_present(self) -> bool:
        result = run([tool("modinfo"), WIFI_MODULE], capture=True)
        return result.returncode == 0

    def _wl_ko_present(self) -> bool:
        """wl.ko calisan cekirdek icin derlenmis mi?"""
        updates = Path(f"/lib/modules/{self.kernel}/updates/dkms")
        if not updates.is_dir():
            return False
        return bool(list(updates.glob("wl.ko*")))

    def _wl_loaded(self) -> bool:
        result = run([tool("lsmod")], capture=True)
        return any(line.startswith("wl ") for line in result.stdout.splitlines())

    def _interface(self) -> str:
        net_dir = Path("/sys/class/net")
        if net_dir.is_dir():
            for entry in sorted(net_dir.iterdir()):
                if entry.name.startswith("wl"):
                    return entry.name
        return ""

    @staticmethod
    def _service_enabled() -> bool:
        if which("systemctl") is None:
            return False
        result = run(
            ["systemctl", "is-enabled", "gnuchan-wifi-broadcom-recover.service"],
            capture=True,
        )
        return result.stdout.strip() == "enabled"


def report(log: Log, state: WifiState) -> None:
    """Durumu yaz; hicbir sey degistirmez."""
    log.step("WiFi durumu (Broadcom BCM43142)")
    log.detail(f"cekirdek          {state.kernel}")
    log.detail(f"aygit             {state.device}")
    log.detail(
        f"headers var mi    {'evet' if state.headers_present else 'HAYIR (DKMS derleyemez!)'}"
    )
    log.detail(f"wl modulu         {'var' if state.wl_module_present else 'YOK'}")
    log.detail(
        f"wl.ko (bu kern.)  {'var' if state.wl_ko_present else 'YOK (derlenmemis)'}"
    )
    log.detail(f"wl yuklu mu       {'evet' if state.wl_loaded else 'hayir'}")
    log.detail(f"wlan arayuzu      {state.interface or 'YOK'}")
    log.detail(f"wiphy             {'var' if state.wiphy_present else 'YOK'}")
    log.detail("kurulu dosyalar:")
    for path in MANAGED_FILES:
        log.detail(f"    {'var ' if path.is_file() else 'yok '} {path}")
    log.detail(f"servis etkin      {'evet' if state.service_enabled else 'hayir'}")

    log.note("")
    if not state.headers_present:
        log.note("KOK NEDEN: cekirdek basliklari yok, `wl` hic derlenmemis.")
        log.note("Kurmak icin:  sudo python3 wifi_fix.py")
    elif not state.wl_ko_present and not state.wl_module_present:
        log.note("Headers var ama `wl` hala derlenmemis. Kurmak icin:")
        log.note("    sudo python3 wifi_fix.py")
    elif state.interface:
        log.note(f"WiFi surucusu etkin; arayuz: {state.interface}")
    else:
        log.note("Kurulum tamam gorunuyor; arayuz icin bir kez:")
        log.note("    sudo python3 wifi_fix.py --now")


# --- dosya yazma -------------------------------------------------------------


def backup_once(log: Log, path: Path) -> None:
    """Var olan dosyayi BIR KEZ yanina kopyala; ikinci calistirma ezmesin."""
    if not path.exists():
        return
    backup = path.with_name(path.name + BACKUP_SUFFIX)
    if backup.exists():
        return
    shutil.copy2(path, backup)
    log.detail(f"yedeklendi: {backup}")


def write_file(log: Log, path: Path, text: str, mode: int = 0o644) -> None:
    """Dosyayi yaz, izni ayarla, gerekirse yedegini al."""
    path.parent.mkdir(parents=True, exist_ok=True)
    backup_once(log, path)
    path.write_text(text, encoding="utf-8")
    path.chmod(mode)
    log.detail(f"yazildi: {path}")


# --- apt ---------------------------------------------------------------------


def apt_environment() -> dict:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def install_packages(log: Log) -> bool:
    """Gerekli paketleri kur. Basariliysa True."""
    if which("apt-get") is None:
        log.warn("apt-get yok; paketler kurulamaz")
        return False

    # Zaten kurulu olanlari atla; boylece bir kez kurulmus makinede apt
    # gereksiz yere cagrilmaz.
    missing: list[str] = []
    for package in APT_PACKAGES:
        result = run(["dpkg", "-s", package], capture=True)
        if result.returncode != 0:
            missing.append(package)

    if missing:
        log.step("Paketler kuruluyor: " + ", ".join(missing))
        run(["apt-get", "update", "-o", "Acquire::Retries=3"],
            capture=True, environment=apt_environment())
        command = ["apt-get", "install", "-y", "--no-install-recommends",
                   *missing]
        if run(command, environment=apt_environment()).returncode != 0:
            log.warn("apt-get kurulum basarisiz: " + ", ".join(missing))
            return False
        log.detail("kuruldu: " + ", ".join(missing))
    else:
        log.detail("paketler zaten kurulu: " + ", ".join(APT_PACKAGES))
    return True


def build_driver(log: Log) -> bool:
    """DKMS ile `wl` modulunu derle. Basariliysa True."""
    dkms = tool("dkms")
    if dkms == "dkms" and which("dkms") is None:
        log.warn("dkms yok; surucu derlenemez")
        return False

    log.step("wl surucusu DKMS ile derleniyor")
    result = run([dkms, "autoinstall"], capture=True)
    if result.returncode != 0:
        # Derleme ciktisinin SON satirlari teshis icin yazilir; tam cikti
        # kullaniciya gerekmez ama nedenini gormek isteyebilir.
        log.warn("dkms autoinstall basarisiz; son satirlar:")
        for line in (result.stdout + result.stderr).splitlines()[-8:]:
            log.warn("  " + line)
        # Ikinci deneme: tam surum ile dogrudan kurulum.
        log.step("Tam surum ile yeniden deneniyor")
        result = run([dkms, "install", f"{DKMS_NAME}/{DKMS_VERSION}",
                      "-k", WifiState().kernel], capture=True)
        if result.returncode != 0:
            log.warn("dkms install de basarisiz; surucu derlenemedi")
            return False

    run([tool("depmod"), "-a"], capture=True)
    log.detail("wl derlendi")
    return True


def load_driver(log: Log) -> bool:
    """`wl` modulunu yukle. Basariliysa True."""
    modprobe = tool("modprobe")
    log.step("wl modulu yukleniyor")
    run([modprobe, "-r", WIFI_MODULE], capture=True)   # eski kalinti varsa indir
    result = run([modprobe, WIFI_MODULE], capture=True)
    if result.returncode != 0:
        log.warn(f"modprobe {WIFI_MODULE} basarisiz: {result.stderr.strip()}")
        return False
    return True


# --- kurulum / geri alma -----------------------------------------------------


def install(log: Log, state: WifiState) -> int:
    """Kalici cozumu kur: paketler + surucu + kurtarma servisi."""
    report(log, state)

    if not install_packages(log):
        log.warn("paketler kurulamadi; devam ediliyor (surucu zaten derliyse)")

    built = build_driver(log)
    if built:
        load_driver(log)

    if which("systemctl") is not None:
        log.step("Kurtarma servisi ve uyku kancasi yaziliyor")
        write_file(log, RECOVER_SCRIPT, RECOVER_CONTENT, mode=0o755)
        write_file(log, SERVICE_FILE, SERVICE_CONTENT)
        write_file(log, SLEEP_HOOK, SLEEP_CONTENT, mode=0o755)

        log.step("Servis etkinlestiriliyor")
        if run(["systemctl", "daemon-reload"], capture=True).returncode != 0:
            log.warn("daemon-reload basarisiz; sonraki acilista tekrar denenecek")
        result = run(
            ["systemctl", "enable", "gnuchan-wifi-broadcom-recover.service"],
            capture=True,
        )
        if result.returncode != 0:
            log.warn(f"servis etkinlestirilemedi: {result.stderr.strip()}")
        else:
            log.detail("gnuchan-wifi-broadcom-recover.service etkin")

    final = WifiState()
    log.note("")
    if final.interface:
        log.note(f"WiFi hazir; arayuz: {final.interface}")
        log.note("NetworkManager birkac saniye icinde aglari listeler.")
    elif built:
        log.note("Surucu derlendi ama arayuz henuz gorunmuyor; makineyi yeniden")
        log.note("baslatmak kesin sonucu verir.")
    else:
        log.note("Surucu derlenemedi. `--status` ile ayrintiya bakin; basliklar")
        log.note("kurulduysa bir kez yeniden baslatmak gerekebilir.")
    log.note("")
    log.note("Kalici cozum kuruldu; bir sonraki cekirdek guncellemesinde DKMS")
    log.note("meta headers sayesinde surucuyu kendisi yeniden derler.")
    return 0 if (built or final.interface) else 1


def revert(log: Log) -> int:
    """Bizim yazdigimiz dosyalari kaldir; varsa yedekleri geri koy.

    APT ile kurulan paketler KALDIRILMAZ: onlar sistemin ve baska bir cipin de
    isine yarayabilir. Yalnizca bu script'in yazdigi dosyalar gider.
    """
    log.step("Geri aliniyor (yalnizca bu script'in dosyalari)")
    for path in MANAGED_FILES:
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if backup.exists():
            shutil.copy2(backup, path)
            backup.unlink()
            log.detail(f"{path} -> yedeginden geri konuldu")
        elif path.exists():
            path.unlink()
            log.detail(f"silindi: {path}")
        else:
            log.detail(f"zaten yok: {path}")
    if which("systemctl") is not None:
        run(["systemctl", "disable", "gnuchan-wifi-broadcom-recover.service"],
            capture=True)
        run(["systemctl", "daemon-reload"], capture=True)
        log.detail("servis devre disi")
    log.note("")
    log.note("Kurtarma servisi kaldirildi. APT ile kurulan paketler ve derlenen")
    log.note("wl surucusu yerinde birakildi; wifi calismaya devam eder.")
    return 0


def apply_now(log: Log) -> int:
    """Surucuyu simdi derle ve yukle. Kablo baglantisini etkilemez."""
    ko = Path(f"/lib/modules/{WifiState().kernel}/updates/dkms/wl.ko")
    if not ko.exists():
        build_driver(log)
    else:
        log.step("wl zaten derlenmis; yukleniyor")
    if not load_driver(log):
        return 1

    state = WifiState()
    log.detail(f"wlan arayuzu      {state.interface or 'henuz YOK'}")
    log.note("")
    if state.interface:
        log.note("WiFi surucusu yuklendi. NetworkManager birkac saniye icinde")
        log.note("aglari listeler; `nmcli dev wifi list` ile bakilabilir.")
    else:
        log.note("Surucu yuklendi ama arayuz gorunmuyor; makineyi yeniden")
        log.note("baslatmak kesin sonucu verir.")
    return 0


def usage() -> None:
    print(
        "kullanim: python3 wifi_fix.py [--status | --now | --revert]\n"
        "\n"
        "  (parametresiz)  kalici cozumu kur (paketler + surucu + servis)\n"
        "  --now           surucuyu simdi derle ve yukle\n"
        "  --status        durumu yaz, hicbir sey degistirme\n"
        "  --revert        kurtarma dosyalarini kaldir (paketler kalir)\n"
    )


def main() -> int:
    log = Log()
    arguments = sys.argv[1:]

    if "--help" in arguments or "-h" in arguments:
        usage()
        return 0
    if "--status" in arguments:
        report(log, WifiState())
        return 0
    if "--now" in arguments:
        ensure_root(log)
        return apply_now(log)
    if "--revert" in arguments:
        ensure_root(log)
        return revert(log)
    if arguments:
        usage()
        return 2
    ensure_root(log)
    return install(log, WifiState())


if __name__ == "__main__":
    raise SystemExit(main())
