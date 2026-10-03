#!/usr/bin/env python3
# =============================================================================
# disable_gpu_for_chromium.py - Dell Vostro A860 (Intel GM965 / GMA X3100)
# uzerinde Chromium'un GPU'yu kullanmasini kapatir. Tek dosya, yalnizca
# standart kutuphane.
#
# =============================================================================
# NEDEN
# =============================================================================
# Bu makinede i915 surucusu ara ara takiliyor ve ekran kararip oturum
# dusuyordu:
#
#     i915 ... GPU HANG: ecode 4:1:87edfafe, in Xorg
#     i915 ... Xorg context reset due to GPU hang
#
# GERCEK COZUM gpu_fix.py'dedir: takilmalarin duzeltmesi Mesa 26'dadir ve
#     sudo python3 gpu_fix.py --mesa-upgrade
# bunu kurar; olculdu, takilmalar neredeyse tamamen biter ve HIZLANDIRMA
# korunur. BU DOSYA ONUN YERINE GECMEZ.
#
# Bu script, o duzeltme istenmedigi ya da yetmedigi durumda kullanilacak
# agir bir caredir: Chromium'un GPU'ya hic dokunmamasini saglar. Cizim
# yazilima (swiftshader / CPU) duser; Chromium yavaslar ama takilma uretmez.
# Yani: once gpu_fix.py, olmazsa bu.
#
# =============================================================================
# NASIL - ve neden ~/.config/chromium-flags.conf DEGIL
# =============================================================================
# Debian'in baslaticisi /usr/bin/chromium bir kabuk betigidir ve sunlari
# yapar (bu makinede okundu):
#
#     for file in /etc/chromium.d/*; do ... . $file; done
#     ...
#     exec $LIBDIR/$APPNAME $CHROMIUM_FLAGS "$@"
#
# Yani /etc/chromium.d/ altindaki her dosya SOURCE EDILIR ve orada
# `export CHROMIUM_FLAGS=...` yazan sey komut satirina eklenir. Kalici,
# dogru ve paketin KENDI destekledigi kanca budur.
#
# `~/.config/chromium-flags.conf` (Arch ve bazi dagitimlarin kullandigi yol)
# BU baslaticida ARANMAZ; buraya yazan bir cozum bu makinede sessizce hicbir
# sey yapmaz. Bu yuzden o dosya degil, /etc/chromium.d kullanilir.
#
# Dosya adi `zz-` ile baslar ve bu ONEMLIDIR: glob, bayt sirasina gore
# genisler ve `default-flags` zaten
#
#     export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --enable-gpu-rasterization"
#
# yazar. Ayni bayragin `--disable-...` hali ancak SONRA gelirse kazanir;
# `zz-` her mevcut dosyadan sonra siralanir (rakamlar harflerden once gelir,
# o yuzden `99-` YETMEZ).
#
# =============================================================================
# FLAGLAR
# =============================================================================
#   --disable-gpu                GPU surecini/kompozisyonunu kapatir.
#   --disable-gpu-compositing    Pencere kompozisyonunu CPU'ya alir.
#   --disable-gpu-rasterization  Sayfa rasterini CPU'ya alir (default-flags'in
#                                --enable-gpu-rasterization'ini ezer).
#   --disable-accelerated-2d-canvas  2B canvas'i yazilima alir.
#   --use-gl=angle --use-angle=swiftshader
#                                GL'yi ANGLE uzerinden swiftshader'a (yazilim)
#                                baglar; boylece WebGL tamamen olmez.
#   --enable-unsafe-swiftshader  Yeni Chromium swiftshader icin bunu ister;
#                                olmadan "yazilim WebGL guvensiz" diye kapatir.
#
# =============================================================================
# KULLANIM
# =============================================================================
#     sudo python3 disable_gpu_for_chromium.py          # kur
#     python3 disable_gpu_for_chromium.py --status      # durum (degistirmez)
#     python3 disable_gpu_for_chromium.py --flags       # etkin flaglari yaz
#     sudo python3 disable_gpu_for_chromium.py --revert # kaldir
#
# Kurduktan sonra acik Chromium pencereleri kapanip yeniden acilmalidir;
# flaglar yalnizca yeni baslatmada okunur.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

#: Bu scriptin sahip oldugu dosya. `zz-` oneki bilerek: glob bayt sirasina
#: gore genisler ve bu ad `default-flags` dahil her mevcut dosyadan SONRA
#: gelir, boylece --disable-gpu-rasterization kazanir.
CONF_PATH = Path("/etc/chromium.d/zz-gnuchan-disable-gpu")

#: Bu scriptin yazdigi dosyaya konan isaret; `--revert` bir yedegin gercek
#: makine hali mi yoksa bu scriptin kendi ciktisi mi oldugunu boyle ayirir.
MARKER = "Written by disable_gpu_for_chromium.py"

#: Baslaticinin source ettigi dizin ve baslaticinin kendisi.
CHROMIUM_D_DIR = Path("/etc/chromium.d")
LAUNCHER_PATHS = (Path("/usr/bin/chromium"), Path("/usr/bin/chromium-browser"))

#: Dosyanin eski hali buraya saklanir.
BACKUP_PATH = CONF_PATH.with_name(CONF_PATH.name + ".gnuchan-backup")

CONF_TEXT = f"""\
# GnuChanWM / GnuchanOS: Chromium'u bu makinede GPU'dan uzak tutar.
#
# {MARKER}
#
# Bu dosya /usr/bin/chromium tarafindan SOURCE EDILIR; buradaki
# CHROMIUM_FLAGS, baslaticinin komut satirina eklenir. Bkz. bu dosyayi
# yazan scriptin basligi: neden burada ve neden ~/.config/chromium-flags.conf
# DEGIL.
#
# ONCE gpu_fix.py --mesa-upgrade denenmelidir: takilmalarin gercek duzeltmesi
# odur ve HIZLANDIRMAYI korur. Bu dosya cizimi yazilima indiren agir caredir.
#
# Geri almak icin:  sudo python3 disable_gpu_for_chromium.py --revert

# GPU kompozisyonu ve surecini kapat.
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --disable-gpu"
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --disable-gpu-compositing"

# Sayfa rasterini CPU'ya al. Bu, /etc/chromium.d/default-flags'in
# --enable-gpu-rasterization'ini ezer, cunku bu dosya (zz-) ondan SONRA
# source edilir.
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --disable-gpu-rasterization"
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --disable-accelerated-2d-canvas"

# webgl/gl'yi yazilima (swiftshader) bagla, boylece WebGL tamamen olmez.
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --use-gl=angle"
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --use-angle=swiftshader"
export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --enable-unsafe-swiftshader"
"""


# --- cikti -------------------------------------------------------------------


class Log:
    """Ilerleme ciktisi."""

    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str = "") -> None:
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
    """`os.geteuid` Windows'ta yoktur; getattr ile okumak modulu orada da
    import edilebilir kilar."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc altina yazmak icin root ol; degilse sudo ile yeniden calistir."""
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit(
            "error: bu script /etc/chromium.d altina yazar; root olarak "
            "calistirin (su -c 'python3 disable_gpu_for_chromium.py')"
        )
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    os.execvpe(
        sudo,
        [sudo, sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
        dict(os.environ),
    )


def launcher_path() -> Path | None:
    """Baslatici betik, varsa."""
    for path in LAUNCHER_PATHS:
        if path.is_file():
            return path
    return None


def is_ours(path: Path) -> bool:
    """Dosyayi bu script mi yazdi."""
    try:
        return MARKER in path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False


# --- makinenin durumu --------------------------------------------------------


class Facts:
    """Bu makinenin Chromium ve yapilandirma durumu."""

    def __init__(self) -> None:
        self.launcher = launcher_path()
        self.conf_dir = CHROMIUM_D_DIR.is_dir()
        self.conf_present = CONF_PATH.is_file()
        self.conf_is_ours = is_ours(CONF_PATH) if self.conf_present else False
        self.backup_present = BACKUP_PATH.is_file()
        self.sorting_ok = self._sorts_last()
        self.effective_flags = self._effective_flags()

    @staticmethod
    def _sorts_last() -> bool:
        """zz- dosyasi, mevcut diger /etc/chromium.d dosyalarindan SONRA mi
        source ediliyor? Glob sirasini aynen kurarak olcer; boylece
        --disable-gpu-rasterization'in kazanacagi KANITLANIR, varsayilmaz.
        """
        if not CHROMIUM_D_DIR.is_dir():
            return False
        names = sorted(p.name for p in CHROMIUM_D_DIR.iterdir()
                       if p.name != "README")
        if CONF_PATH.name not in names:
            return False
        return names[-1] == CONF_PATH.name

    @staticmethod
    def _effective_flags() -> str:
        """Baslaticinin ulasacagi CHROMIUM_FLAGS'i, dosyalari AYNI sekilde
        source ederek olcer. Boylece 'gercekten uygulaniyor mu' sorusunun
        cevabi tahmin degil olcum olur."""
        if not CHROMIUM_D_DIR.is_dir():
            return ""
        # Baslaticinin dongusunun aynisi; `.dpkg` dosyalari ve README atlanir.
        script = (
            'CHROMIUM_FLAGS=""; '
            'for f in /etc/chromium.d/*; do '
            '  [ "$f" = /etc/chromium.d/README ] && continue; '
            '  case "$(basename "$f")" in *.dpkg) continue;; esac; '
            '  . "$f"; '
            'done; '
            'echo "$CHROMIUM_FLAGS"'
        )
        result = run(["sh", "-c", script], capture=True)
        return result.stdout.strip()


def report(log: Log, facts: Facts) -> None:
    """Durumu yaz; hicbir sey degistirmez."""
    log.step("Chromium GPU durumu")
    log.detail(f"baslatici        {facts.launcher or '(bulunamadi!)'}")
    log.detail(f"/etc/chromium.d  {'var' if facts.conf_dir else 'YOK'}")
    if facts.conf_present:
        owner = "bu script yazdi" if facts.conf_is_ours else "BASKASI yazmis"
        log.detail(f"yapilandirma    var ({owner})")
    else:
        log.detail("yapilandirma    yok (GPU kapatilmamis)")

    if facts.conf_present:
        log.detail(
            "siralama        "
            + ("en sonda (disable kazanir)" if facts.sorting_ok
               else "SONDA DEGIL - --enable-gpu-rasterization kazanabilir!")
        )

    log.note()
    log.step("Etkin CHROMIUM_FLAGS (olculdu)")
    if facts.effective_flags:
        for token in facts.effective_flags.split():
            log.detail(token)
    else:
        log.detail("(bos)")

    gpu_off = "--disable-gpu" in facts.effective_flags.split()
    log.note()
    if gpu_off:
        log.note("GPU Chromium icin KAPALI. Cizim yazilimda kosuyor.")
        log.note("Acik Chromium pencereleri bir kez kapanip acilmali.")
    else:
        log.note("GPU Chromium icin ACIK. Kapatmak icin:")
        log.note("    sudo python3 disable_gpu_for_chromium.py")


# --- dosya yazma -------------------------------------------------------------


def backup_once(log: Log, path: Path) -> None:
    """Var olan dosyayi BIR KEZ yanina kopyala; ikinci calistirma ezmesin."""
    if not path.exists():
        return
    if BACKUP_PATH.exists():
        log.detail(f"yedek zaten var: {BACKUP_PATH.name}")
        return
    shutil.copy2(path, BACKUP_PATH)
    log.detail(f"yedeklendi: {BACKUP_PATH.name}")


def write_conf(log: Log, text: str) -> None:
    CONF_PATH.parent.mkdir(parents=True, exist_ok=True)
    backup_once(log, CONF_PATH)
    CONF_PATH.write_text(text, encoding="utf-8")
    CONF_PATH.chmod(0o644)
    log.detail(f"yazildi: {CONF_PATH}")


# --- kurulum / geri alma -----------------------------------------------------


def install(log: Log, facts: Facts) -> int:
    """GPU'yu Chromium icin kapat."""
    report(log, facts)

    if not facts.conf_dir:
        log.warn(
            "/etc/chromium.d yok. Bu makinede Chromium'un baslaticisi orayi "
            "source etmiyor olabilir; bu cozum etkisiz kalir."
        )
        return 1

    log.note()
    log.step("Kuruluyor")
    write_conf(log, CONF_TEXT)

    after = Facts()
    if not after.sorting_ok:
        log.warn(
            "yapilandirma /etc/chromium.d icinde en sona siralamadi; "
            "default-flags'in --enable-gpu-rasterization'i kazanabilir."
        )

    log.note()
    log.note("Kuruldu. Etkin olmasi icin acik Chromium pencerelerini kapatip")
    log.note("yeniden acin. Dogrulamak icin:")
    log.note("    python3 disable_gpu_for_chromium.py --flags")
    log.note()
    log.note("NOT: Kalici ve hizlandirmayi KORUYAN cozum gpu_fix.py'dedir:")
    log.note("    sudo python3 gpu_fix.py --mesa-upgrade")
    return 0


def revert(log: Log) -> int:
    """Kurulan dosyayi kaldir; varsa gercek yedegi geri koy."""
    log.step("Geri aliniyor")

    if BACKUP_PATH.is_file() and not is_ours(BACKUP_PATH):
        shutil.copy2(BACKUP_PATH, CONF_PATH)
        CONF_PATH.chmod(0o644)
        BACKUP_PATH.unlink()
        log.detail(f"{CONF_PATH} -> yedeginden geri konuldu")
    else:
        if BACKUP_PATH.is_file():
            log.detail(f"{BACKUP_PATH.name} bu scriptin kendi eski ciktisi; "
                       "geri konulmuyor")
            BACKUP_PATH.unlink()
        try:
            CONF_PATH.unlink()
            log.detail(f"silindi: {CONF_PATH}")
        except FileNotFoundError:
            log.detail(f"{CONF_PATH} zaten yok")
        except OSError as error:
            log.warn(f"silinemedi: {error}")
            return 1

    log.note()
    log.note("GPU Chromium icin yine acik. Acik pencereler bir kez kapanip")
    log.note("acilinca hizlandirma geri gelir.")
    return 0


def print_flags(log: Log) -> int:
    """Baslaticinin ulasacagi CHROMIUM_FLAGS'i yaz. Hicbir sey degistirmez."""
    facts = Facts()
    flags = facts.effective_flags
    if not flags:
        log.note("(bos - /etc/chromium.d okunamadi ya da hicbir dosya yok)")
        return 0
    for token in flags.split():
        log.note(token)
    return 0


# --- giris -------------------------------------------------------------------


def usage() -> None:
    print(
        "kullanim: python3 disable_gpu_for_chromium.py "
        "[--status | --flags | --revert]\n"
        "\n"
        "  (parametresiz)  Chromium'un GPU kullanimini kapat (kalici)\n"
        "  --status        durumu yaz, hicbir sey degistirme\n"
        "  --flags         etkin CHROMIUM_FLAGS'i yaz\n"
        "  --revert        kaldir\n"
    )


def main() -> int:
    log = Log()
    arguments = sys.argv[1:]

    if "--help" in arguments or "-h" in arguments:
        usage()
        return 0
    if "--status" in arguments:
        report(log, Facts())
        return 0
    if "--flags" in arguments:
        return print_flags(log)
    if "--revert" in arguments:
        ensure_root(log)
        return revert(log)
    if arguments:
        usage()
        return 2

    ensure_root(log)
    return install(log, Facts())


if __name__ == "__main__":
    raise SystemExit(main())
