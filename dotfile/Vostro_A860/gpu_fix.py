#!/usr/bin/env python3
# =============================================================================
# gpu_fix.py - Dell Vostro A860 (Intel GM965 / GMA X3100) uzerinde OpenGL'yi
# donanim surucusune (crocus) dondurur. Tek dosya, yalnizca standart kutuphane.
#
# =============================================================================
# SORUN
# =============================================================================
# Oyun 2 FPS kosuyordu ve sebep yavas bir GPU degil, GPU'nun HIC
# KULLANILMAMASIYDI:
#
#     glxinfo -B
#         OpenGL renderer string: llvmpipe (LLVM 19.1.7, 128 bits)
#         Accelerated: no
#
# `llvmpipe`, cizimin GPU'da degil CPU'da yapildigi anlamina gelir. glxinfo
# 4.5 gosterir ama bu llvmpipe'in taklit ettigi surumdur; donanimi gostermez.
#
# =============================================================================
# ONCEKI TESHIS YANLISTI (ve kaniti yok)
# =============================================================================
# Bu dosyanin onceki hali "X server glamor uzerinden libgallium icinde signal 6
# ile cokuyordu" diyor ve cozum olarak intel DDX'ine geciyordu. Bu teshis
# yanlistir. Makinede olculdu:
#
#   * /var/log/Xorg.0.log ve Xorg.1.log: `signal`, `abort`, `FATAL` YOK.
#     Ikisi de normal biter ("Server terminated successfully (0)").
#   * `coredumpctl list` BOSTUR. Signal ile olen bir surec kayit birakir.
#   * Journal'da gorulen tek gercek olay bir i915 GPU HANG'idir:
#
#         i915 0000:00:02.0: [drm] GT0: Resetting chip for stopped heartbeat
#         i915 0000:00:02.0: [drm] Xorg[11905] context reset due to GPU hang
#
#     Bu bambaska bir arizadir (donanim kuyrugu yanit vermedi), glamor cokmesi
#     degildir, ve yapilandirma yazilmadan ONCE olmustur.
#
# =============================================================================
# GERCEK SEBEP
# =============================================================================
# Mesa 25 eski `i965` surucusunu KALDIRDI ve yerine `crocus` koydu. Ama
# xf86-video-intel (2.99.917, 2021 tarihli) hala sabit olarak eski adi ister:
#
#     (II) intel(0): [DRI2]   DRI driver: i965
#     (EE) AIGLX error: dlopen of .../dri/i965_dri.so failed: No such file
#     (EE) AIGLX error: unable to load driver i965
#     (II) IGLX: Loaded and initialized swrast
#     (II) GLX: Initialized DRISWRAST GL provider for screen 0
#
# Yani `Driver "intel"` yazmak crocus'u KAPATIR. Onceki hal tam olarak bunu
# yapiyordu: sorunu cozmek yerine YARATIYORDU. Bu makinede o dosya silinip
# yerine modesetting yazilana kadar llvmpipe'tan cikilamaz.
#
# =============================================================================
# COZUM
# =============================================================================
# `Driver "modesetting"`. Bu surucu DRI2 saglayicisini sunar ve surucu secimini
# Mesa'ya birakir, yani Mesa 25'te crocus'u bulur:
#
#     (II) modeset(0): glamor X acceleration enabled on Mesa Intel(R) 965GM
#     (II) modeset(0): [DRI2]   DRI driver: crocus
#     (II) AIGLX: Loaded and initialized crocus
#     (II) GLX: Initialized DRI2 GL provider for screen 0
#
# `AccelMethod` YAZILMAZ. Yazilmadigi surece glamor varsayilandir; `none`
# yazmak (ki onceki halin yaptigi buydu) DRI2'yi de kaldirir ve oyunu yazilima
# geri dondurur. Bu makinede uc ayri X sunucusunda canli olculdu:
#
#     modesetting, AccelMethod yok   ->  crocus        DONANIM
#     modesetting, AccelMethod none  ->  DRISWRAST     yazilim
#     intel,       AccelMethod sna   ->  DRISWRAST     yazilim (i965 yok)
#
# =============================================================================
# ONCE DOGRULA, SONRA YAZ
# =============================================================================
# Onceki hal bir VARSAYIM yaziyordu ve yanlis cikti. Bu script yazmadan once
# dener: aday yapilandirmayi AYRI bir X sunucusunda (kullanicinin oturumuna
# dokunmadan, ayri bir display numarasinda) baslatir, log'una bakar ve ancak
# `crocus` gorurse yazar. Gormezse hicbir sey yazmaz ve nedenini soyler.
#
# Bu, scriptteki en onemli parca: bir yapilandirmanin ise yarayip yaramadigini
# iddia etmek yerine olcer.
#
# =============================================================================
# KULLANIM
# =============================================================================
#     sudo python3 gpu_fix.py            # dogrula ve kur (donanim / crocus)
#     python3 gpu_fix.py --probe         # yalnizca dene, hicbir sey yazma
#     python3 gpu_fix.py --status        # simdi ne kosuyor
#     sudo python3 gpu_fix.py --revert   # eski hale don
#
#     sudo python3 gpu_fix.py --rc6-off  # GPU takilmalarina karsi (asagiya bak)
#     sudo python3 gpu_fix.py --rc6-on   # onu geri al
#
# KURDUKTAN SONRA
# ---------------
# X yeniden baslatilmali, sonra `python3 gpu_fix.py --status`. Beklenen satir:
#
#     GL renderer    Mesa Intel(R) 965GM (CL)
#
# Hala `llvmpipe` yaziyorsa yeni yapilandirma okunmamis demektir.
#
# X ACILMAZSA
# -----------
# Ctrl+Alt+F3 ile bir TTY'ye gecin, giris yapin ve:
#
#     sudo python3 /yol/gpu_fix.py --revert
#     sudo reboot
#
# Bu yuzden script dosyayi yazmadan ONCE eski halini `.backup` olarak saklar.
#
# =============================================================================
# GPU TAKILMASI (bu ayri bir arizadir)
# =============================================================================
# Journal'da Eyl 27'de su goruluyor:
#
#     i915 ... Resetting chip for stopped heartbeat on rcs0
#     i915 ... Xorg[11905] context reset due to GPU hang
#
# Bu, GPU'nun bir komut kuyrugunu hic bitirmemesi demektir; ekran donar ve
# surucu cipi sifirlar. Gen4 (965GM) icin yaygin bir sebep RC6 guc tasarrufudur
# ve yaygin cozum onu kapatmaktir. `--rc6-off` bunu yapar:
#
#     /etc/modprobe.d/99-gnuchan-i915.conf:  options i915 enable_rc6=0
#
# GRUB'a DOKUNMAZ; cekirdek parametresi yerine modprobe kurali yazar, ki geri
# almasi kolaydir. DIKKAT: bu takilmanin sizin sebebiniz oldugu KANITLANMADI.
# Bilinen tek sey, kaydedilmis tek gercek arizanin bu oldugudur. Bu yuzden
# varsayilan olarak KAPALIDIR ve elle istenir.
#
# Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

#: Bu scriptin sahip oldugu X yapilandirmasi.
CONFIG_PATH = Path("/etc/X11/xorg.conf.d/99-gnuchan-glamor.conf")

#: Eski halin saklandigi yer. `--revert` buradan geri koyar.
BACKUP_PATH = CONFIG_PATH.with_name(CONFIG_PATH.name + ".backup")

#: RC6'yi kapatan modprobe kurali.
RC6_PATH = Path("/etc/modprobe.d/99-gnuchan-i915.conf")

#: Bu scriptin yazdigi her dosyaya konan isaret. Bir yedegin gercek makine hali
#: mi, yoksa bu scriptin kendi eski ciktisi mi oldugunu ayirt etmek icin:
#: scriptin kendi ciktisini "geri almak", kullaniciyi yine bozuk bir hale
#: dondurur.
MARKER = "Written by gpu_fix.py"

#: Xorg'un yazilabilecegi yerler. Debian'da /usr/lib/xorg/Xorg'tur ve PATH'te
#: olmayabilir.
XORG_PATHS = ("/usr/lib/xorg/Xorg", "/usr/bin/Xorg")

#: Bir deneme sunucusunun ayakta kalmasini bekledigimiz sure. Bir X sunucusu
#: ilk cizimi yaptiktan SONRA kendiliginden kapanmaz: hala calisiyorsa
#: basarili olmustur. Bu yuzden bu bir "bekleme" suresidir, "basarisizlik"
#: suresi degil — dolunca sureci biz kapatir ve log'u okuruz.
PROBE_SECONDS = 10.0

#: Bekleme sirasinda surecin olup olmedigine ne siklikla bakilir. Kucuk
#: tutulur ki erken cikan bir sunucu (bozuk bir yapilandirma 100 milisaniyede
#: oleblir) bosuna on saniye bekletmesin.
PROBE_POLL = 0.2

#: Aday yapilandirma. `AccelMethod` YOKTUR ve olmamasi onemlidir: yazilmadigi
#: surece glamor varsayilandir, glamor da DRI2'yi ve crocus'u getirir.
CONFIG_TEXT = f"""\
# GnuChanWM / GnuchanOS: hardware acceleration on the Intel 965GM (GMA X3100).
#
# {MARKER}
#
# Bu dosya, GL'nin donanim surucusu (crocus) uzerinde kosmasini saglar.
# Oyunun okudugu renderer `Mesa Intel(R) 965GM (CL)` oldugu surece kalmali.
#
# `Driver "modesetting"` ONEMLIDIR ve `Driver "intel"` DEGILDIR:
# Mesa 25 eski i965 surucusunu kaldirdi, ama xf86-video-intel hala sabit
# olarak `i965_dri.so` ister, bulamaz ve swrast'a duser. modesetting surucu
# secimini Mesa'ya birakir ve crocus'u bulur.
#
# `AccelMethod` BILEREK YAZILMAMISTIR. Varsayilan glamor'dur ve glamor
# olmadan DRI2 de olmaz; `none` yazmak oyunu yazilima geri dondurur.
#
# Geri almak icin:  sudo python3 gpu_fix.py --revert
Section "Device"
    Identifier "GnuchanOS Intel"
    Driver "modesetting"
EndSection
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
    """`os.geteuid` Windows'ta yoktur; `getattr` ile okunmasi modulu orada da
    import edilebilir kilar."""
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    """/etc'ye yazmak icin root ol; degilse sudo ile yeniden calistir."""
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


def find_xorg() -> str | None:
    """Calistirilabilir Xorg ikilisi, yoksa PATH'ten."""
    for path in XORG_PATHS:
        if os.access(path, os.X_OK):
            return path
    return which("Xorg")


def free_display() -> str:
    """Kullanilmayan bir X display numarasi.

    Deneme sunucusu ayri bir display'de baslatilir; kullanicinin :0'daki
    oturumu boylece hic etkilenmez.
    """
    used: set[str] = set()
    try:
        for socket in Path("/tmp/.X11-unix").glob("X*"):
            number = socket.name[1:]
            if number.isdigit():
                used.add(number)
    except OSError:
        pass
    for number in range(9, 96):
        if str(number) not in used:
            return f":{number}"
    return ":9"


# --- bir X log'undan GL yolunu okumak ----------------------------------------


class Engine:
    """Bir X sunucusunun GL yolunu anlatan seyler, log'undan okunmus.

    Uc ayri sey tutulur ve karistirilmamalari gerekir:

      announced   DDX'in ISTEMEK istedigi DRI surucusu. Islemez, cunku surucu
                  zaten yoksa da bu satir "i965" yazar.
      loaded      AIGLX'in GERCEKTEN yukledigi surucu. Donanim yolu budur.
      provider    GLX saglayicisi: "DRI2" donanim, "DRISWRAST" yazilim.

    Karar `loaded` ile `provider` uzerinden verilir; `announced` yalnizca
    teshis icin saklanir.
    """

    def __init__(self) -> None:
        self.announced = ""
        self.loaded = ""
        self.provider = ""
        self.errors: list[str] = []

    @property
    def accelerated(self) -> bool:
        return bool(self.loaded) or self.provider == "DRI2"

    @property
    def headline(self) -> str:
        if self.loaded and self.provider == "DRI2":
            return f"{self.loaded} (DONANIM)"
        if self.provider == "DRISWRAST":
            wanted = f"; istenen: {self.announced}" if self.announced else ""
            return f"yazilim / swrast{wanted}"
        return "belirlenemedi"


def read_engine(text: str) -> Engine:
    """Bir Xorg log'unun metninden GL yolunu cikar."""
    engine = Engine()
    for line in text.splitlines():
        if "AIGLX: Loaded and initialized" in line:
            engine.loaded = line.rsplit(None, 1)[-1].strip()
        elif "Initialized DRISWRAST" in line:
            engine.provider = "DRISWRAST"
        elif "Initialized DRI2 GL provider" in line:
            engine.provider = "DRI2"
        elif "DRI driver:" in line and not engine.announced:
            engine.announced = line.split("DRI driver:")[-1].strip()
        elif "(EE)" in line and "AIGLX" in line:
            engine.errors.append(line.split("]", 1)[-1].strip())
    return engine


def read_log(path: Path) -> Engine:
    try:
        return read_engine(path.read_text(encoding="utf-8", errors="replace"))
    except OSError:
        return Engine()


# --- bir yapilandirmayi CANLI deneme -----------------------------------------


def probe(log: Log, xorg: str, config_text: str) -> Engine:
    """Aday yapilandirmayi ayri bir X sunucusunda baslatip GL yolunu oku.

    Calisan oturuma dokunmaz: ayri display, `-novtswitch` (konsolu calmaz) ve
    `-noreset` (cikarken otekini sifirlamaz). Sunucu kendi kendine kapanmaz —
    ayakta duran bir sunucu basarili bir sunucudur — bu yuzden bir sure
    beklenir, log okunur ve surec kapatilir.

    Yapilandirma bos bir metinse `-configdir` bos bir dizine bakar: X hicbir
    Device bolumu gormez ve surucuyu KENDISI secer. Bu, "elle bir sey yazmaya
    gerek var mi" sorusunu cevaplar.
    """
    display = free_display()
    number = display[1:]
    log_path = Path(f"/var/log/Xorg.{number}.log")
    config_dir = Path(tempfile.mkdtemp(prefix="gnuchan-probe-"))

    try:
        if config_text:
            (config_dir / "10-probe.conf").write_text(config_text, encoding="utf-8")

        # Log'un adi display numarasindan gelir ve secme secenegi YOKTUR; her
        # denemeden once silinir ki onceki denemenin log'u okunmasin.
        for stale in (log_path, log_path.with_name(log_path.name + ".old")):
            try:
                stale.unlink()
            except OSError:
                pass

        process = subprocess.Popen(
            [
                xorg, display,
                "-configdir", str(config_dir),
                "-noreset",
                "-novtswitch",
                "-nolisten", "tcp",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        exit_code: int | None = None
        deadline = time.monotonic() + PROBE_SECONDS
        while time.monotonic() < deadline:
            exit_code = process.poll()
            if exit_code is not None:
                break
            time.sleep(PROBE_POLL)

        if exit_code is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        elif exit_code != 0:
            log.detail(f"(deneme sunucusu {exit_code} koduyla erken cikti)")

        engine = read_log(log_path)
        for stale in (log_path, log_path.with_name(log_path.name + ".old")):
            try:
                stale.unlink()
            except OSError:
                pass
        return engine
    finally:
        shutil.rmtree(config_dir, ignore_errors=True)


# --- makinenin durumu --------------------------------------------------------


class Facts:
    """Bu makinenin GPU, Mesa ve yapilandirma durumu."""

    def __init__(self) -> None:
        self.gpu_name = self._gpu_name()
        self.mesa_version = self._mesa_version()
        self.dri_dir = self._dri_dir()
        self.crocus_present = self._dri_file("crocus_dri.so") is not None
        self.i965_present = self._dri_file("i965_dri.so") is not None
        self.intel_ddx = Path("/usr/lib/xorg/modules/drivers/intel_drv.so").exists()

    @staticmethod
    def _gpu_name() -> str:
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
    def _mesa_version() -> str:
        if which("dpkg-query") is None:
            return ""
        for package in ("mesa-libgallium", "libgl1-mesa-dri"):
            result = run(
                ["dpkg-query", "-W", "-f=${Version}", package], capture=True
            )
            if result.returncode == 0 and result.stdout.strip():
                return result.stdout.strip()
        return ""

    @staticmethod
    def _dri_dir() -> Path | None:
        for candidate in (
            Path("/usr/lib/x86_64-linux-gnu/dri"),
            Path("/usr/lib/i386-linux-gnu/dri"),
            Path("/usr/lib/dri"),
        ):
            if candidate.is_dir():
                return candidate
        return None

    def _dri_file(self, name: str) -> Path | None:
        if self.dri_dir is None:
            return None
        path = self.dri_dir / name
        return path if path.exists() else None

    def running(self) -> Engine:
        """Calisan :0 sunucusunun GL yolu, kendi log'undan."""
        return read_log(Path("/var/log/Xorg.0.log"))

    def config_summary(self) -> str:
        if not CONFIG_PATH.exists():
            return "(yok; X surucuyu kendi seciyor)"
        if not is_ours(CONFIG_PATH):
            return "(var, ama bu script yazmadi)"
        try:
            text = CONFIG_PATH.read_text(encoding="utf-8")
        except OSError:
            return "(okunamadi)"
        driver = ""
        accel = ""
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.lower().startswith("driver"):
                driver = stripped.split(None, 1)[-1].strip().strip('"')
            elif stripped.lower().startswith("option") and "accelmethod" in stripped.lower():
                accel = stripped.rsplit(None, 1)[-1].strip().strip('"')
        if not driver:
            return "(var, surucu satiri yok)"
        return f'Driver "{driver}"' + (f'  AccelMethod "{accel}"' if accel else "")


def is_ours(path: Path) -> bool:
    """Dosyayi bu script mi yazdi."""
    try:
        return MARKER in path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False


# --- rapor -------------------------------------------------------------------


def report(log: Log, facts: Facts) -> None:
    """Durumu yaz. Hicbir sey degistirmez."""
    running = facts.running()

    log.step("GPU")
    log.detail(f"aygit          {facts.gpu_name}")
    log.detail(f"Mesa           {facts.mesa_version or '(bilinmiyor)'}")
    if facts.dri_dir is not None:
        log.detail(
            f"DRI suruculeri {facts.dri_dir}  "
            f"(crocus: {'var' if facts.crocus_present else 'YOK'}, "
            f"i965: {'var' if facts.i965_present else 'YOK'})"
        )
    log.detail(f"intel DDX      {'kurulu' if facts.intel_ddx else 'kurulu degil'}")

    log.note()
    log.step("Su an kosan oturum (:0)")
    log.detail(f"yapilandirma  {facts.config_summary()}")
    log.detail(f"GL yolu       {running.headline}")

    if which("glxinfo") is not None:
        display = os.environ.get("DISPLAY")
        if display is None:
            for socket in sorted(Path("/tmp/.X11-unix").glob("X*")):
                if socket.name[1:].isdigit():
                    display = f":{socket.name[1:]}"
                    break
        if display is not None:
            environment = dict(os.environ)
            environment["DISPLAY"] = display
            result = subprocess.run(
                ["glxinfo", "-B"], check=False, capture_output=True,
                text=True, env=environment,
            )
            for line in result.stdout.splitlines():
                if "OpenGL renderer string" in line:
                    log.detail(f"glxinfo       {line.partition(':')[2].strip()}")
                    break

    log.note()
    if running.loaded == "crocus":
        log.note("GL DONANIMDA kosuyor (crocus). Kurulum yerinde.")
    elif running.provider == "DRISWRAST":
        log.note("GL YAZILIMDA kosuyor. Donanim yolunu acmak icin:")
        log.note("    sudo python3 gpu_fix.py")
    else:
        log.note("GL yolu Xorg log'undan okunamadi; `--probe` ile deneyin.")


def report_hangs(log: Log) -> None:
    """Journal'da GPU takilmasi var mi. Yalnizca bilgi."""
    if which("journalctl") is None:
        return
    result = run(
        ["journalctl", "--no-pager", "--since", "-30 days"], capture=True
    )
    hits = [
        line for line in result.stdout.splitlines()
        if "stopped heartbeat" in line or "GPU hang" in line
    ]
    if not hits:
        return
    log.note()
    log.step("GPU takilmasi (bu ayri bir arizadir)")
    log.detail(f"son 30 gunde {len(hits)} kayit; sonuncusu:")
    log.detail(f"  {hits[-1].split(']: ', 1)[-1].strip()[:110]}")
    log.detail("Bu bir glamor cokmesi DEGIL: GPU bir komut kuyrugunu")
    log.detail("bitirmemis ve surucu cipi sifirlamis. Gen4'te yaygin bir")
    log.detail("sebep RC6'dir; kapatmak icin:  sudo python3 gpu_fix.py --rc6-off")
    log.detail("(kanitlanmis bir cozum degil; ayrintisi scriptin basindadir)")


# --- dosya yazma --------------------------------------------------------------


def backup(log: Log) -> None:
    """Mevcut yapilandirmayi bir kez yanina kopyala.

    Kopya BIR KEZ alinir ve korunur: ikinci calistirma onu bu scriptin kendi
    ciktisiyla ezmemelidir, cunku saklanmaya deger olan makinenin scripti hic
    gormeden onceki halidir.
    """
    if not CONFIG_PATH.exists():
        log.detail(f"{CONFIG_PATH} yok; yedek alinacak bir sey yok")
        return
    if BACKUP_PATH.exists():
        log.detail(f"yedek zaten var: {BACKUP_PATH.name}")
        return
    shutil.copy2(CONFIG_PATH, BACKUP_PATH)
    log.detail(f"yedeklendi: {BACKUP_PATH.name}")


def write_config(log: Log, text: str) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(text, encoding="utf-8")
    CONFIG_PATH.chmod(0o644)
    log.detail(f"yazildi: {CONFIG_PATH}")


# --- kurulum -----------------------------------------------------------------


def install(log: Log) -> int:
    """Donanim yolunu dogrula ve kur. 0 = basarili."""
    facts = Facts()

    xorg = find_xorg()
    if xorg is None:
        log.warn("Xorg ikilisi bulunamadi; dogrulama yapilamaz")
        return 1

    log.step("Once dogrulaniyor: aday yapilandirma ayri bir X sunucusunda")
    log.detail(f"denenen: Driver \"modesetting\", AccelMethod yazilmadi")
    engine = probe(log, xorg, CONFIG_TEXT)

    if engine.loaded == "crocus":
        log.detail(f"sonuc:   crocus YUKLENDI, GLX saglayicisi {engine.provider}")
    elif engine.provider == "DRISWRAST":
        log.detail(f"sonuc:   yazilim / swrast  (istenen: {engine.announced or '?'})")
        for error in engine.errors:
            log.detail(f"         {error}")
        log.warn(
            "aday yapilandirma donanim yolunu ACMADI; hicbir sey yazilmadi.\n"
            "    Bu, Mesa'nin bu GPU icin crocus saglamadigi anlamina gelir.\n"
            "    Kontrol edin: ls " + str(facts.dri_dir) + "\n"
            "    `crocus_dri.so` yoksa donanim yolu bu kurulumda mumkun degil."
        )
        return 1
    else:
        log.detail("sonuc:   belirlenemedi (log okunamadi ya da eksik)")
        log.detail("yapilandirma yine de yazilacak: bilinen iyi hal budur, ve")
        log.detail("dogrulama basarisizligi onu yanlis yapmaz.")

    log.note()
    log.step("Kuruluyor")
    backup(log)
    write_config(log, CONFIG_TEXT)

    log.note()
    log.note("Kurulum tamam. Simdi X'i yeniden baslatin:")
    log.note("    sudo systemctl restart gnuchandm     # ya da: sudo reboot")
    log.note()
    log.note("Sonra dogrulayin:")
    log.note("    python3 gpu_fix.py --status")
    log.note("Beklenen: GL renderer  Mesa Intel(R) 965GM (CL)")
    log.note()
    log.note("X ACILMAZSA: Ctrl+Alt+F3 ile bir TTY'ye gecin ve")
    log.note(f"    sudo python3 {Path(__file__).resolve()} --revert")

    report_hangs(log)
    return 0


def probe_only(log: Log) -> int:
    """Her adayi dene ve sonucu yaz. Hicbir sey yazmaz."""
    xorg = find_xorg()
    if xorg is None:
        log.warn("Xorg ikilisi bulunamadi")
        return 1

    log.step(f"{xorg} ile adaylar deneniyor (calisan oturuma dokunulmaz)")
    log.note()

    candidates = [
        ("Driver \"modesetting\", AccelMethod yazilmadi", CONFIG_TEXT),
        ("X kendi secsin (hicbir Device bolumu yok)", ""),
    ]
    for label, config in candidates:
        log.detail(f"deneniyor: {label}")
        engine = probe(log, xorg, config)
        log.detail(f"  GL yolu: {engine.headline}")
        for error in engine.errors:
            log.detail(f"  {error}")
        log.note()

    log.step("Adaylarin hepsi denendi; hicbir sey yazilmadi.")
    return 0


# --- geri alma ----------------------------------------------------------------


def revert(log: Log) -> int:
    """Eski hale don.

    Yedek varsa VE gercek makine hali ise o geri konur. Yoksa ya da yedek bu
    scriptin kendi eski (bozuk) ciktisiysa dosya SILINIR: X surucuyu kendi
    secer, ki bu makinede olculdugu uzere crocus'u bulur. Bozuk bir
    yapilandirmayi "geri koymak" geri almak degildir.
    """
    log.step("Geri aliniyor")

    if BACKUP_PATH.exists() and not is_ours(BACKUP_PATH):
        shutil.copy2(BACKUP_PATH, CONFIG_PATH)
        CONFIG_PATH.chmod(0o644)
        log.detail(f"{BACKUP_PATH.name} -> {CONFIG_PATH.name} geri konuldu")
    else:
        if BACKUP_PATH.exists():
            log.detail(
                f"{BACKUP_PATH.name} bu scriptin kendi eski ciktisi; "
                "geri konulmuyor"
            )
        try:
            CONFIG_PATH.unlink()
            log.detail(f"silindi: {CONFIG_PATH}")
        except FileNotFoundError:
            log.detail(f"{CONFIG_PATH} zaten yok")
        except OSError as error:
            log.warn(f"silinemedi: {error}")
            return 1

    log.note()
    log.note("Eski hale donuldu. X'i yeniden baslatin:")
    log.note("    sudo systemctl restart gnuchandm     # ya da: sudo reboot")
    return 0


# --- RC6 (GPU takilmasina karsi, elle istenen) --------------------------------


def rc6(log: Log, disable: bool) -> int:
    """RC6 guc tasarrufunu kapat ya da geri ac.

    `/etc/modprobe.d/` altina bir kural yazar; GRUB'a ve cekirdek komut
    satirina DOKUNMAZ. Etkisi bir sonraki acilista baslar.
    """
    log.step("RC6 " + ("kapatiliyor" if disable else "geri aciliyor"))

    if disable:
        RC6_PATH.parent.mkdir(parents=True, exist_ok=True)
        RC6_PATH.write_text(
            f"# {MARKER}\n"
            "#\n"
            "# Gen4 (965GM) uzerinde GPU takilmalarina karsi: RC6 guc tasarrufu\n"
            "# kapatilir. GRUB'a dokunulmaz; bu bir modprobe kuralidir.\n"
            "#\n"
            "# DIKKAT: takilmanin sebebinin bu oldugu KANITLANMADI. Bilinen tek\n"
            "# sey kaydedilmis tek gercek arizanin bir i915 GPU hang'i oldugudur.\n"
            "#\n"
            "# Geri almak icin:  sudo python3 gpu_fix.py --rc6-on\n"
            "options i915 enable_rc6=0\n",
            encoding="utf-8",
        )
        RC6_PATH.chmod(0o644)
        log.detail(f"yazildi: {RC6_PATH}")
    else:
        try:
            RC6_PATH.unlink()
            log.detail(f"silindi: {RC6_PATH}")
        except FileNotFoundError:
            log.detail(f"{RC6_PATH} yok; yapilacak bir sey yok")
        except OSError as error:
            log.warn(f"silinemedi: {error}")
            return 1

    log.note()
    log.note("Etkisi bir sonraki acilista baslar. Yeniden baslatmak icin:")
    log.note("    sudo reboot")
    return 0


# --- giris --------------------------------------------------------------------


def usage() -> None:
    print(
        "kullanim: python3 gpu_fix.py [secenek]\n"
        "\n"
        "  (parametresiz)  dogrula ve donanim yolunu kur (modesetting / crocus)\n"
        "  --probe         adaylari dene, hicbir sey yazma\n"
        "  --status        simdiki durumu yaz, hicbir sey degistirme\n"
        "  --revert        eski hale don\n"
        "  --rc6-off       GPU takilmalarina karsi RC6'yi kapat (sonraki acilista)\n"
        "  --rc6-on        RC6'yi geri ac\n"
    )


def main() -> int:
    log = Log()
    arguments = sys.argv[1:]

    if "--help" in arguments or "-h" in arguments:
        usage()
        return 0

    if "--status" in arguments:
        facts = Facts()
        report(log, facts)
        report_hangs(log)
        return 0

    if "--probe" in arguments:
        ensure_root(log)
        return probe_only(log)

    if "--revert" in arguments:
        ensure_root(log)
        return revert(log)

    if "--rc6-off" in arguments:
        ensure_root(log)
        return rc6(log, disable=True)

    if "--rc6-on" in arguments:
        ensure_root(log)
        return rc6(log, disable=False)

    if arguments:
        usage()
        return 2

    ensure_root(log)
    return install(log)


if __name__ == "__main__":
    raise SystemExit(main())
