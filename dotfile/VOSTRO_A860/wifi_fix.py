#!/usr/bin/env python3
# =============================================================================
# wifi_fix.py - Dell Vostro A860, Atheros AR242x (ath5k) radyo duzeltmesi.
# Sorun: kart ara ara "radyo kapali" haline geliyor ve yalnizca elle
# (fn+F11 + sudo rfkill unblock all + ath5k reload) geri geliyor. Bu script
# bunu KALICI hale getirir. Tek dosya, yalnizca standart kutuphane.
#
# KULLANIM
#     sudo python3 wifi_fix.py            # kalici cozumu kur
#     sudo python3 wifi_fix.py --now      # simdi uygula (ag kisa duser!)
#     python3 wifi_fix.py --status        # durum (hicbir sey degistirmez)
#     sudo python3 wifi_fix.py --revert   # her seyi kaldir
#
# TESHIS (bu makinede OLCULDU, tahmin degil)
#   * 168c:001c AR242x -> surucu ath5k.
#   * rfkill binary /usr/sbin/rfkill'te; normal kullanici PATH'inde
#     (/usr/local/bin:/usr/bin:/bin:/usr/games) /usr/sbin YOK. Bu yuzden
#     `rfkill` bazi kabuklarda "bulunamadi" gorunur; `sudo rfkill` hep calisir.
#   * /sys/module/ath5k/parameters/no_hw_rfkill_switch = N
#     ath5k DONANIM rfkill hattini OKUYOR. Hat, kart calisirken "kapali"
#     bildirince surucu radyoyu kapatiyor; fn+F11 o hatti temizliyor. Kok neden.
#   * /sys/devices/platform/dell-laptop/rfkill/ YOK -> tek wlan rfkill dugumu
#     ath5k'nin kendisi (phyN). Arada duzeltecek platform surucusu yok.
#
# COZUM - IKI BAGIMSIZ PARCA
#   1. /etc/modprobe.d/ath5k-fix.conf:
#          options ath5k no_hw_rfkill_switch=1
#      ath5k artik donanim hattini okumaz, radyo yazilimla kontrol edilir.
#      Yan etki: fn+F11 radyoyu kapatamaz. Modul YENIDEN YUKLENINCE etkin olur
#      -> bir kez reboot / --now.
#   2. gnuchan-wifi-recover: acilista ve askidan donuste calisir:
#        a. rfkill unblock all  (yazilim kilidi icin yeter, reload gerekmez);
#        b. wiphy YOKSA ath5k'yi yeniden yukler.
#      (b) yalnizca wiphy yokken yapilir; calisan karti bos yere yeniden
#      yuklemek baglantiyi saniyelerce dusurur.
#
# NE DEGISTIRIR
#     /etc/modprobe.d/ath5k-fix.conf
#     /usr/local/sbin/gnuchan-wifi-recover
#     /etc/systemd/system/gnuchan-wifi-recover.service
#     /etc/systemd/system-sleep/gnuchan-wifi-recover
# Var olan dosyalarin eski hali .gnuchan-backup olarak saklanir; --revert
# bunlari geri koyar.
#
# UYARI: --now ath5k'yi yeniden yukler, ag birkac saniye duser. SSH uzerinden
# baglanmissan baglanti kopar; yerel konsolda calistir ya da reboot bekle.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

MODPROBE_FILE = Path("/etc/modprobe.d/ath5k-fix.conf")
RECOVER_SCRIPT = Path("/usr/local/sbin/gnuchan-wifi-recover")
SERVICE_FILE = Path("/etc/systemd/system/gnuchan-wifi-recover.service")
SLEEP_HOOK = Path("/etc/systemd/system-sleep/gnuchan-wifi-recover")
BACKUP_SUFFIX = ".gnuchan-backup"

WIFI_MODULE = "ath5k"
WIPHY_DIR = Path("/sys/class/ieee80211")
RFKILL_CANDIDATES = (Path("/usr/sbin/rfkill"), Path("/sbin/rfkill"))

MANAGED_FILES = (MODPROBE_FILE, RECOVER_SCRIPT, SERVICE_FILE, SLEEP_HOOK)

MODPROBE_CONTENT = """\
# GnuChanWM / GnuchanOS: Atheros AR242x (ath5k) radyo duzeltmesi.
# Bu dosyayi wifi_fix.py yazdi. Kart, radyo calisirken donanim rfkill hattini
# "kapali" bildiriyor; ath5k o hatti okudugu icin radyoyu kapatiyordu ve
# yalnizca fn+F11 temizleyebiliyordu. Asagidaki ayar surucuye hatti YOK
# SAYMASINI soyler; radyo tamamen yazilimla kontrol edilir.
# Etkin olmasi icin modul yeniden yuklenmeli: reboot ya da
#     sudo python3 wifi_fix.py --now
# Geri almak icin:  sudo python3 wifi_fix.py --revert
options ath5k no_hw_rfkill_switch=1
"""

RECOVER_CONTENT = """\
#!/bin/bash
# GnuchanOS radyo kurtarma (Atheros AR242x / ath5k). Acilista ve uyanista
# calisir. Iki ayri ariza: (1) yazilim rfkill kilidi -> unblock yeter;
# (2) surucu radyosuz -> wiphy yoksa reload. Reload SADECE wiphy yokken.
set -u
RFKILL=""
for c in /usr/sbin/rfkill /sbin/rfkill; do
    [ -x "$c" ] && RFKILL="$c" && break
done
[ -n "$RFKILL" ] && "$RFKILL" unblock all 2>/dev/null || true
if ls /sys/class/ieee80211/phy* >/dev/null 2>&1; then exit 0; fi
/usr/sbin/modprobe -r ath5k 2>/dev/null || true
sleep 1
/usr/sbin/modprobe ath5k 2>/dev/null || true
exit 0
"""

SERVICE_CONTENT = """\
[Unit]
Description=GnuchanOS radio recovery (Atheros AR242x / ath5k)
After=NetworkManager.service
Wants=NetworkManager.service

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/gnuchan-wifi-recover

[Install]
WantedBy=multi-user.target
"""

SLEEP_CONTENT = """\
#!/bin/bash
# systemd: $1 = pre|post, $2 = suspend|hibernate|... Yalnizca uyanista gerekir.
case "${1:-}" in
    post) /usr/local/sbin/gnuchan-wifi-recover ;;
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


def run(command: list[str], capture: bool = False) -> subprocess.CompletedProcess[str]:
    """Komutu calistir; kabuk yok, her komut bir listedir."""
    if capture:
        return subprocess.run(command, check=False, capture_output=True, text=True)
    return subprocess.run(command, check=False, text=True)


def which(name: str) -> str | None:
    return shutil.which(name)


def is_root() -> bool:
    """`os.geteuid` Windows'ta yok; getattr ile okumak modulu her yerde import
    edilebilir kilar."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc ve /usr/local'e yazmak icin root ol; degilse sudo ile yeniden calis."""
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: bu script /etc ve /usr/local altina yazar; root olarak "
            "calistirin (su -c 'python3 wifi_fix.py')"
        )
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        dict(os.environ),
    )


# --- durum -------------------------------------------------------------------


class WifiState:
    """Bu makinenin wifi ve kilitleme durumu."""

    def __init__(self) -> None:
        self.device = self._device()
        self.rfkill_path = self._rfkill_path()
        self.blocks = self._blocks()
        self.hw_switch_ignored = self._hw_switch_ignored()
        self.wiphy_present = WIPHY_DIR.is_dir() and bool(list(WIPHY_DIR.glob("phy*")))
        self.service_enabled = self._service_enabled()

    @staticmethod
    def _device() -> str:
        if which("lspci") is None:
            return "(lspci yok)"
        result = run(["lspci"], capture=True)
        for line in result.stdout.splitlines():
            if "network" in line.lower() or "wireless" in line.lower():
                _, _, name = line.partition(": ")
                return name.strip() or line.strip()
        return "(kablosuz aygit bulunamadi)"

    @staticmethod
    def _rfkill_path() -> str:
        """Once /usr/sbin: normal PATH'te olmadigi icin `rfkill` bosuna 'yok'
        gorunebiliyor."""
        for candidate in RFKILL_CANDIDATES:
            if candidate.is_file():
                return str(candidate)
        return which("rfkill") or ""

    @staticmethod
    def _blocks() -> list[tuple[str, str, str, str]]:
        """Her rfkill aygiti: (ad, tur, hard, soft), /sys'ten."""
        found: list[tuple[str, str, str, str]] = []
        root = Path("/sys/class/rfkill")
        if not root.is_dir():
            return found
        for device in sorted(root.glob("rfkill*")):
            def read(name: str) -> str:
                try:
                    return (device / name).read_text(encoding="utf-8").strip()
                except OSError:
                    return "?"
            found.append((read("name"), read("type"), read("hard"), read("soft")))
        return found

    @staticmethod
    def _hw_switch_ignored() -> bool | None:
        """`no_hw_rfkill_switch=1` etkin mi; parametre okunamazsa None."""
        try:
            raw = Path(
                f"/sys/module/{WIFI_MODULE}/parameters/no_hw_rfkill_switch"
            ).read_text(encoding="utf-8").strip().lower()
        except OSError:
            return None
        return raw in ("y", "1")

    @staticmethod
    def _service_enabled() -> bool:
        if which("systemctl") is None:
            return False
        result = run(
            ["systemctl", "is-enabled", "gnuchan-wifi-recover.service"], capture=True
        )
        return result.stdout.strip() == "enabled"


def report(log: Log, state: WifiState) -> None:
    """Durumu yaz; hicbir sey degistirmez."""
    log.step("WiFi durumu")
    log.detail(f"aygit             {state.device}")
    log.detail(f"rfkill komutu     {state.rfkill_path or '(bulunamadi!)'}")
    log.detail(
        f"wiphy var mi      {'evet' if state.wiphy_present else 'HAYIR (surucu radyosuz)'}"
    )
    if state.hw_switch_ignored is None:
        log.detail("donanim rfkill    (ath5k yuklu degil, parametre okunamadi)")
    else:
        log.detail(
            "donanim rfkill    "
            + ("YOK SAYILIYOR (duzeltme etkin)" if state.hw_switch_ignored
               else "OKUNUYOR (duzeltme etkin degil)")
        )

    wlan_hard = wlan_soft = False
    log.detail("kilitler:")
    for name, kind, hard, soft in state.blocks:
        log.detail(f"    {name:<10} {kind:<6} hard={hard} soft={soft}")
        if kind == "wlan":
            wlan_hard = wlan_hard or hard == "1"
            wlan_soft = wlan_soft or soft == "1"

    log.detail("kurulu dosyalar:")
    for path in MANAGED_FILES:
        log.detail(f"    {'var ' if path.is_file() else 'yok '} {path}")
    log.detail(f"servis etkin      {'evet' if state.service_enabled else 'hayir'}")

    log.note("")
    if not MODPROBE_FILE.is_file():
        log.note("Kalici cozum kurulu degil. Kurmak icin:  sudo python3 wifi_fix.py")
    elif state.hw_switch_ignored is False:
        log.note("Ayar dosyasi var ama HENUZ ETKIN DEGIL: modul yeniden yuklenmeli.")
        log.note("Bir kez:  sudo python3 wifi_fix.py --now   (ya da reboot)")
    elif state.hw_switch_ignored is None:
        log.note("ath5k su an yuklu degil; reboot sonrasi ayar etkin olacak.")
    else:
        log.note("Kalici cozum kurulu ve etkin.")
    if wlan_hard:
        log.note("")
        log.note("DIKKAT: wlan HARD kilidi acik; no_hw_rfkill_switch=1 bunu yok")
        log.note("sayar ama fn+F11 bir kez gerekebilir.")
    if wlan_soft:
        log.note("")
        log.note("wlan yazilim kilidi acik. Acmak icin:  sudo rfkill unblock all")


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


# --- kurulum / geri alma -----------------------------------------------------


def install(log: Log, state: WifiState) -> int:
    """Kalici cozumu kur."""
    report(log, state)

    if which("systemctl") is None:
        log.warn("systemctl yok; servis kurulamaz, yalnizca betik yazilir")

    log.step("Modul ayari yaziliyor")
    write_file(log, MODPROBE_FILE, MODPROBE_CONTENT)

    log.step("Kurtarma betigi yaziliyor")
    write_file(log, RECOVER_SCRIPT, RECOVER_CONTENT, mode=0o755)

    if which("systemctl") is not None:
        log.step("Servis ve uyku kancasi yaziliyor")
        write_file(log, SERVICE_FILE, SERVICE_CONTENT)
        write_file(log, SLEEP_HOOK, SLEEP_CONTENT, mode=0o755)

        log.step("Servis etkinlestiriliyor")
        if run(["systemctl", "daemon-reload"], capture=True).returncode != 0:
            log.warn("daemon-reload basarisiz; sonraki acilista tekrar denenecek")
        result = run(
            ["systemctl", "enable", "gnuchan-wifi-recover.service"], capture=True
        )
        if result.returncode != 0:
            log.warn(f"servis etkinlestirilemedi: {result.stderr.strip()}")
        else:
            log.detail("gnuchan-wifi-recover.service etkin")

    log.note("")
    log.note("Kalici cozum kuruldu:")
    for path in MANAGED_FILES:
        log.note(f"  {path}")
    log.note("")
    if state.hw_switch_ignored:
        log.note("Donanim ayari zaten etkin; sonraki acilis/uyanis kurtarmayi calistirir.")
    else:
        log.note("Donanim ayari modul yeniden yuklenince etkin olur. Simdi icin:")
        log.note("    sudo python3 wifi_fix.py --now")
        log.note("(Ag birkac saniye duser; SSH'deysen baglanti kopar. Alternatif: reboot.)")
    return 0


def revert(log: Log) -> int:
    """Kurulan her seyi kaldir; varsa yedekleri geri koy."""
    log.step("Geri aliniyor")
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
        run(["systemctl", "disable", "gnuchan-wifi-recover.service"], capture=True)
        run(["systemctl", "daemon-reload"], capture=True)
        log.detail("servis devre disi")
    log.note("")
    log.note("Kalici cozum kaldirildi; elle mudahale yine gerekecek.")
    return 0


def apply_now(log: Log) -> int:
    """Modulu simdi yeniden yukleyerek ayari derhal etkinlestir.

    Ag birkac saniye duser; nedeni kullaniciya SOYLENIR - sessizce baglanti
    koparmak, scriptin bozuk oldugu izlenimi verir.
    """
    log.step("Uygulaniyor (ag kisa sure dusecek)")
    result = run(["modprobe", "-r", WIFI_MODULE], capture=True)
    if result.returncode != 0:
        log.warn(f"{WIFI_MODULE} kaldirilamadi: {result.stderr.strip()}")
    result = run(["modprobe", WIFI_MODULE], capture=True)
    if result.returncode != 0:
        log.warn(f"{WIFI_MODULE} yeniden yuklenemedi: {result.stderr.strip()}")
        return 1
    log.detail(f"{WIFI_MODULE} yeniden yuklendi")
    state = WifiState()
    log.detail(f"donanim rfkill    {'YOK SAYILIYOR' if state.hw_switch_ignored else 'OKUNUYOR'}")
    log.detail(f"wiphy var mi      {'evet' if state.wiphy_present else 'HAYIR'}")
    log.note("")
    log.note("Uygulandi; NetworkManager birkac saniye icinde baglanir.")
    return 0


def usage() -> None:
    print(
        "kullanim: python3 wifi_fix.py [--status | --now | --revert]\n"
        "\n"
        "  (parametresiz)  kalici cozumu kur (modul ayari + servis + uyku kancasi)\n"
        "  --now           modulu simdi yeniden yukle (ag kisa duser)\n"
        "  --status        durumu yaz, hicbir sey degistirme\n"
        "  --revert        kurulan her seyi kaldir\n"
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
