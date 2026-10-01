#!/usr/bin/env python3
# =============================================================================
# GnuChanOS Wine + wined3d - optimize et ve derle  (Debian, Intel GM965)
# -----------------------------------------------------------------------------
#     python3 makefile.py
#
# TEK KOMUT. Sirasiyla:
#
#     1. derleme kutuphanelerini kurar               (apt-get)
#     2. wine deposunu _temp/wine-src icine klonlar  (wine'in geri kalani)
#     3. dotfile/WINE icindeki wined3d'yi,           (bizim optimize ettigimiz)
#        _temp/wine-src/dlls/wined3d uzerine yazar
#     4. GM965 bayraklariyla configure eder
#     5. derler ve /opt/wine-gnuchan altina kurar
#     6. onegi (WINEPREFIX) olusturup Direct3D ayarlarini yazar
#
# =============================================================================
# NEDEN BOYLE: KAYNAK IKI YERDE
# =============================================================================
# Optimize edilen kod `dotfile/WINE/` icindedir ve PROJECT DEPOSUNDADIR: elle
# duzenlenir, git ile surumlenir, gozden gecirilir. Derleme ise Wine'in TAM
# agacini ister (configure, Makefile.in, diger binlerce dosya), ve o agac
# `_temp/wine-src/` altinda uretilir.
#
# makefile, dotfile/WINE icindeki dosyalari _temp/wine-src/dlls/wined3d uzerine
# KOPYALAR. Boylece:
#   * optimize edilen kaynak tek bir yerde durur (dotfile/WINE) ve okuyani
#     upstream Wine'in binlerce dosyasi arasinda kaybolmaz;
#   * _temp yalnizca "wine ve geri kalanlar" olur, atilabilir bir derleme
#     agacidir.
#
# =============================================================================
# "wined3d" NEDIR
# =============================================================================
# wined3d, Wine kaynak agacinin `dlls/wined3d/` dizinidir ve Wine ile BIRLIKTE
# derlenir. `build` zaten wined3d'yi derler. `wined3d` eylemi, o dizini TEK
# BASINA yeniden derleyip kurar — 3B kodunda degisiklik yapip denemek icin.
#
# =============================================================================
# GM965 ICIN OPTIMIZE: dotfile/WINE/wined3d_main.c
# =============================================================================
# Asil optimizasyon `wined3d_settings` blogundadir ve kaynakta YAPILMISTIR:
#
#     max_gl_version   4.4 -> 2.1    GM965 en fazla GL 2.1 verir
#     renderer         AUTO -> GL    GM965'te Vulkan yok
#     shader_backend   AUTO -> GLSL  d3d8/d3d9'un tek dogru yolu
#     multisample_textures TRUE -> FALSE   GM965 dolgu hizindan yoksun
#
# Ayrintili gerekce dotfile/WINE/wined3d_main.c icindedir. Bu makefile o kodu
# DEGISTIRMEZ, yalnizca derler.
#
# =============================================================================
# ONCE SU CALISTIRILMALI (ayri ariza)
# =============================================================================
# `glxinfo -B` "llvmpipe" diyorsa cizim CPU'dadir ve hicbir sey bunu kurtarmaz:
#     sudo python3 ../Vostro_A860/gpu_fix.py
# Beklenen: `Mesa Intel(R) 965GM (CL)`.
#
# Debian only, on purpose.  Lisans: GPL3
# =============================================================================

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent          # dotfile/WINE
REPO_ROOT = ROOT.parent.parent                   # proje koku

#: Bizim OPTIMIZE EDILMIS wined3d kaynagimiz: tam olarak bu dizin.
WINED3D_SOURCE = ROOT

#: Derleme agaci. _temp yalnizca "wine ve geri kalanlar" icin kullanilir.
TEMP = REPO_ROOT / "_temp"
SOURCE_DIR = TEMP / "wine-src"
BUILD_DIR = TEMP / "wine-build"

#: Wine kaynaginin indirilecegi adres.
REPOSITORY = "https://github.com/wine-mirror/wine.git"

#: wined3d'nin derleme agacindaki yeri.
WINED3D_DEST = SOURCE_DIR / "dlls" / "wined3d"

#: Kurulum oneki.
WINE_PREFIX = Path("/opt/wine-gnuchan")


def invoking_home() -> Path:
    """Kullaniciyi sudo ile calistiran KISININ ev dizini.

    `Path.home()` burada YANLIS olur: bu dosya /opt'a yazdigi icin sudo ile
    yeniden calisir ve root oldugunda `Path.home()` `/root` doner — yani onek
    `/root/.wine-gnuchan` olur, oysa oyunlari calistiracak kisi root degildir.
    `SUDO_USER` sudo'nun sakladigi tek gercek parcadir.
    """
    name = os.environ.get("SUDO_USER")
    if name:
        try:
            import pwd
            return Path(pwd.getpwnam(name).pw_dir)
        except (ImportError, KeyError):
            pass
    return Path.home()


PREFIX = invoking_home() / ".wine-gnuchan"
WINEARCH = "win32"
PROFILE_PATH = Path("/etc/profile.d/gnuchan-wine.sh")
ELEVATED_VARIABLE = "GNUCHANWINE_ELEVATED"

#: Derleme kutuphaneleri.
#:
#: Liste KISA tutulmustur ve bilerek: bir eski dizustunde her fazladan paket
#: diskte ve derleme suresinde yer kaplar. Oyunlarin ihtiyac duydugu sey
#: d3d8/d3d9 (wined3d) ve ses; geri kalani istege baglidir.
#: Vulkan paketleri YOKTUR ve olmamalidir: GM965'te Vulkan yok.
BUILD_PACKAGES = (
    "build-essential", "pkg-config", "flex", "bison", "git",
    "libx11-dev", "libxext-dev", "libxrandr-dev", "libxrender-dev",
    "libxfixes-dev", "libxi-dev", "libxcursor-dev", "libxcomposite-dev",
    "libxinerama-dev",
    "libgl1-mesa-dev", "libglu1-mesa-dev",
    "libfreetype-dev", "libfontconfig-dev",
    "libasound2-dev",
    "libncurses-dev", "libxml2-dev",
)

#: Direct3D kayit defteri ayarlari (onek acilirken yazilir). Kaynak yamasinin
#: AYNI ayarlari calisma zamaninda da durur; boylece hem derlenmis varsayilan
#: hem calisan onek ayni seyi soyler.
DIRECT3D_KEYS = (
    ("MaxVersionGL", "REG_DWORD", "0x00020001"),
    ("renderer", "REG_SZ", "gl"),
    ("shader_backend", "REG_SZ", "glsl"),
    ("VideoMemorySize", "REG_SZ", "256"),
    ("VideoPciVendorID", "REG_DWORD", "0x8086"),
    ("VideoPciDeviceID", "REG_DWORD", "0x2A02"),
    ("csmt", "REG_DWORD", "1"),
)
X11_KEYS = (("GrabFullscreen", "REG_SZ", "Y"),)

#: Mesa ve Wine ortam degiskenleri: (ad, deger, neden).
ENVIRONMENT = (
    ("vblank_mode", "0", "dikey esitleme kapali: entegre GPU'da FPS'i yariya dusurur"),
    ("mesa_glthread", "true", "GL cagrilari ayri is parcaciginda; Core2'de yarar"),
    ("MESA_NO_ERROR", "1", "Mesa hata denetimi kapali"),
    ("MESA_SHADER_CACHE_MAX_SIZE", "512M", "shader onbellegi buyuk"),
    ("WINEESYNC", "1", "esync: senkronizasyon gecikmesini dusurur"),
    ("WINEFSYNC", "1", "fsync: cekirdek futex'i ile daha hizli"),
    ("WINEDEBUG", "-all", "hata ayiklama ciktisi kapali"),
)


# --- cikti -------------------------------------------------------------------


class Log:
    def step(self, message: str) -> None:
        print(f"==> {message}", flush=True)

    def detail(self, message: str) -> None:
        print(f"    {message}", flush=True)

    def note(self, message: str = "") -> None:
        print(message, flush=True)

    def warn(self, message: str) -> None:
        print(f"  ! {message}", file=sys.stderr, flush=True)


# --- kabuk -------------------------------------------------------------------


def run(command: list[str], capture: bool = False,
        environment: dict | None = None,
        cwd: Path | None = None) -> subprocess.CompletedProcess:
    if capture:
        return subprocess.run(command, check=False, capture_output=True,
                              text=True, env=environment, cwd=cwd)
    return subprocess.run(command, check=False, text=True,
                          env=environment, cwd=cwd)


def which(name: str) -> str | None:
    return shutil.which(name)


def is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    return bool(geteuid is not None and geteuid() == 0)


def ensure_root(log: Log) -> None:
    if is_root():
        return
    sudo = which("sudo")
    if sudo is None:
        raise SystemExit("error: bu /opt ve apt-get'e yazar; root olarak calistirin")
    log.step("Sistem geneli bir degisiklik; sudo ile yeniden calistiriliyor")
    environment = dict(os.environ)
    environment[ELEVATED_VARIABLE] = "1"
    os.execvpe(sudo, [sudo, sys.executable,
                      str(Path(__file__).resolve()), *sys.argv[1:]], environment)


def cpu_count() -> int:
    return os.cpu_count() or 2


# --- bagimliliklar -----------------------------------------------------------


def apt_environment() -> dict:
    return {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}


def missing_build_dependencies() -> list[str]:
    if which("dpkg-query") is None:
        return []
    needed: list[str] = []
    for package in BUILD_PACKAGES:
        result = run(["dpkg-query", "-W", "-f=${Status}", package], capture=True)
        if result.returncode != 0 or "install ok installed" not in result.stdout:
            needed.append(package)
    return needed


def install_dependencies(log: Log) -> None:
    needed = missing_build_dependencies()
    if not needed:
        log.detail("butun derleme bagimliliklari kurulu")
        return
    log.step("Derleme kutuphaneleri kuruluyor")
    log.detail("eksik: " + ", ".join(needed))
    run(["apt-get", "update", "-o", "Acquire::Retries=3"],
        capture=True, environment=apt_environment())
    command = ["apt-get", "install", "-y", "--no-install-recommends", *needed]
    if run(command, environment=apt_environment()).returncode != 0:
        raise SystemExit("error: apt-get kuramadi: " + ", ".join(needed))
    log.detail("kuruldu: " + ", ".join(needed))


# --- kaynak ------------------------------------------------------------------


def fetch_source(log: Log) -> None:
    """Wine'in TAM agacini _temp/wine-src icine indir/guncelle."""
    git = which("git")
    if git is None:
        raise SystemExit("error: git gerekli; once: apt-get install git")

    if (SOURCE_DIR / ".git").is_dir():
        log.step("Wine kaynagi guncelleniyor")
        result = run([git, "pull", "--ff-only"], cwd=SOURCE_DIR)
        if result.returncode != 0:
            log.warn("kaynak guncellenemedi; mevcut agac kullanilacak")
        else:
            log.detail("guncellendi")
        return

    log.step("Wine kaynagi indiriliyor")
    log.detail(f"{REPOSITORY} -> {SOURCE_DIR}")
    log.detail("(Wine'in tam agaci gerekir; gerekli olan wined3d asagida)")
    TEMP.mkdir(parents=True, exist_ok=True)
    if run([git, "clone", "--depth", "1", REPOSITORY,
            str(SOURCE_DIR)]).returncode != 0:
        raise SystemExit("error: kaynak indirilemedi")


# --- optimize edilmis wined3d'yi derleme agacina yerlestir -------------------


def wined3d_sources() -> list[Path]:
    """dotfile/WINE icindeki, kopyalanacak wined3d dosyalari.

    Yalnizca dosyalar alinir (alt dizin yoktur) ve makefile.py'nin kendisi
    ATLANIR: o bir kaynak dosyasi degil, bu derleme betigidir.
    """
    if not WINED3D_SOURCE.is_dir():
        return []
    return sorted(p for p in WINED3D_SOURCE.iterdir()
                  if p.is_file() and p.name != Path(__file__).name)


def optimize(log: Log) -> int:
    """Bizim wined3d kaynagini Wine agacinin uzerine yaz.

    Bu adim, "optimize edilmis kodu derle" isteginin ta kendisidir: kaynak
    dotfile/WINE'de durur, burada derlenebilmesi icin Wine agacindaki yerine
    kopyalanir. Kopyalanan dosya sayisini doner ve beklenenlerin gercekten
    geldigini KONTROL eder.
    """
    sources = wined3d_sources()
    if not sources:
        raise SystemExit(
            f"error: {WINED3D_SOURCE} icinde wined3d kaynagi yok"
        )

    # Derleme icin Wine agaci sart; yoksa once indirilir.
    if not (SOURCE_DIR / ".git").is_dir() and not (SOURCE_DIR / "configure").exists():
        fetch_source(log)

    WINED3D_DEST.mkdir(parents=True, exist_ok=True)

    log.step("Optimize edilmis wined3d yerine konuluyor")
    log.detail(f"{WINED3D_SOURCE}  ->  {WINED3D_DEST}")
    copied = 0
    for source in sources:
        shutil.copyfile(source, WINED3D_DEST / source.name)
        copied += 1
    log.detail(f"{copied} dosya kopyalandi")

    # DOGRULA: optimize edilmis imzalar gercekten yerinde mi? Kopyalama
    # basarisiz olursa derleme upstream kaynagi derler ve degisiklik sessizce
    # kaybolur. Iki ayri imza kontrol edilir cunku duzeltme IKI dosyaya yayilir:
    #   wined3d_main.c  -> istenen GL surumu 2,1
    #   adapter_gl.c    -> supported_gl_versions listesinde 2,1 GIRDI
    # Ikincisi olmadan birincisi ise yaramaz: wined3d 2,1'i listede bulamayip
    # 1,0'a duser (bkz. adapter_gl.c, supported_gl_versions).
    verifications = (
        ("wined3d_main.c", "MAKEDWORD_VERSION(2, 1)",
         "istenen GL surumu 2,1"),
        ("adapter_gl.c", "MAKEDWORD_VERSION(2, 1),\n        MAKEDWORD_VERSION(1, 0),",
         "supported_gl_versions listesinde 2,1"),
    )
    for name, signature, description in verifications:
        path = WINED3D_DEST / name
        if not path.is_file():
            raise SystemExit(f"error: {name} yerine konulmadi")
        text = path.read_text(encoding="utf-8", errors="replace")
        # Satir sonlari CRLF olabilir; imzayi satir sonundan bagimsiz ara.
        if not (signature.replace("\n", "\r\n") in text or signature in text):
            raise SystemExit(
                f"error: kopyalanan {name} optimize edilmis surum DEGIL.\n"
                f"    {WINED3D_SOURCE / name} icinde {description} bekleniyordu."
            )
        log.detail(f"dogrulandi: {name} -> {description}")

    log.detail("dogrulandi: max_gl_version = 2,1 (GM965)")
    return copied


# --- configure ---------------------------------------------------------------


def configure_flags() -> list[str]:
    """Bu makine icin configure bayraklari ve C derleyici bayraklari."""
    flags = [
        f"--prefix={WINE_PREFIX}",
        # YENI WoW64: 32 ve 64 bit tek derlemede. Eski 32 bit oyunlar icin
        # gereken sey budur ve ayri bir derleme gerektirmez.
        "--enable-archs=i386,x86_64",
        "--disable-tests",
        "--without-vulkan",
        "--without-gstreamer",
        "--without-pulse",
        "--without-oss",
    ]
    # -O2 -march=native: Core2 icin dogru komut seti; -O3 DEGIL (Wine'in sicak
    # yollari bellek erisimine baglidir). -pipe eski bir diskte derlemeyi
    # hizlandirir.
    optimizations = "-O2 -march=native -fno-semantic-interposition -pipe"
    cflags = f"{optimizations} -D_FORTIFY_SOURCE=2"
    flags.append(f"CFLAGS={cflags}")
    flags.append(f"CXXFLAGS={cflags}")
    flags.append(f"LDFLAGS={optimizations}")
    return flags


def ensure_configured(log: Log) -> None:
    """Derleme agaci hazir degilse configure et. OUT-OF-TREE derleme."""
    if (BUILD_DIR / "Makefile").exists():
        log.detail("configure edilmis; atlaniyor")
        return

    source_configure = SOURCE_DIR / "configure"
    if not source_configure.exists():
        # Bir git klonunda `configure` YOKTUR: autoconf onu uretir. Uretimi
        # icin kaynakla gelen `autogen.sh` cagrilir.
        autogen = SOURCE_DIR / "autogen.sh"
        if not autogen.exists():
            raise SystemExit("error: ne configure ne autogen.sh var; kaynak eksik")
        log.step("autogen.sh calistiriliyor (configure uretiliyor)")
        if run([str(autogen)], cwd=SOURCE_DIR).returncode != 0:
            raise SystemExit("error: autogen.sh basarisiz")

    log.step("Configure")
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    flags = configure_flags()
    log.detail("bayraklar: " + " ".join(flags))
    if run([str(source_configure), *flags], cwd=BUILD_DIR).returncode != 0:
        raise SystemExit("error: configure basarisiz. "
                         "Ayrinti icin _temp/wine-build/config.log")


# --- derleme -----------------------------------------------------------------


def build_wine(log: Log) -> None:
    """TUM Wine'i derle; wined3d DAHIL. Ilk kurulum budur."""
    fetch_source(log)
    optimize(log)          # optimize edilmis wined3d'yi agaca koy
    install_dependencies(log)
    ensure_configured(log)

    jobs = cpu_count()
    log.step(f"Wine + wined3d derleniyor (-j{jobs}; uzun surebilir)")
    log.detail("Not: RAM azsa j sayisini dusurun; bazi dosyalar cok bellek yer.")
    if run(["make", f"-j{jobs}"], cwd=BUILD_DIR).returncode != 0:
        raise SystemExit("error: make basarisiz")
    log.detail("derlendi")


def build_wined3d(log: Log) -> None:
    """YALNIZCA dlls/wined3d'yi yeniden derle ve kur.

    wined3d kodunda bir degisiklik yaptiginda tam derlemeyi beklemeden denemen
    icin. Derleme agacinin zaten kurulu olmasi ve optimize edilmis kaynagin
    yerinde olmasi gerekir.
    """
    if not (BUILD_DIR / "Makefile").exists():
        raise SystemExit(
            "error: derleme agaci yok. Once tam derlemeyi yapin:\n"
            "    python3 makefile.py build"
        )

    optimize(log)          # degismis olabilir; her seferinde yerine koy
    jobs = cpu_count()
    log.step(f"Yalnizca wined3d derleniyor (-j{jobs})")
    log.detail("hedef: dlls/wined3d")
    if run(["make", f"-j{jobs}", "dlls/wined3d"], cwd=BUILD_DIR).returncode != 0:
        raise SystemExit("error: wined3d derlenemedi")
    # Kurulmazsa /opt altindaki kopya ESKI kalir ve degisiklik gorunmez:
    # calisan sey kurulu agactir, derleme agaci degil.
    if run(["make", "install"], cwd=BUILD_DIR).returncode != 0:
        raise SystemExit("error: wined3d kurulamadi")
    log.detail("wined3d derlendi ve kuruldu")


# --- kurulum -----------------------------------------------------------------


def install_wine(log: Log) -> None:
    if not (BUILD_DIR / "Makefile").exists():
        raise SystemExit("error: derlenmis agac yok. Once: python3 makefile.py build")
    log.step("Kuruluyor")
    log.detail(f"onek: {WINE_PREFIX}")
    if run(["make", "install"], cwd=BUILD_DIR).returncode != 0:
        raise SystemExit("error: make install basarisiz")
    run(["ldconfig"], capture=True)
    log.detail(f"kuruldu: {WINE_PREFIX / 'bin' / 'wine'}")


# --- onek ve ayarlar ---------------------------------------------------------


def wine_binary() -> str | None:
    candidate = WINE_PREFIX / "bin" / "wine"
    if os.access(str(candidate), os.X_OK):
        return str(candidate)
    return which("wine")


def wine_environment(prefix: Path) -> dict:
    environment = dict(os.environ)
    environment["WINEPREFIX"] = str(prefix)
    environment["WINEARCH"] = WINEARCH
    environment["WINEDEBUG"] = "-all"
    environment["WINEDLLOVERRIDES"] = "mscoree,mshtml="
    return environment


def ensure_prefix(log: Log, wine: str) -> bool:
    if (PREFIX / "system.reg").exists():
        log.detail(f"onek hazir: {PREFIX}")
        return True
    log.detail(f"onek olusturuluyor: {PREFIX} ({WINEARCH})")
    PREFIX.mkdir(parents=True, exist_ok=True)
    run([wine, "wineboot", "-u"], environment=wine_environment(PREFIX))
    if not (PREFIX / "system.reg").exists():
        log.warn("onek olusturulamadi")
        return False
    log.detail("onek olusturuldu")
    return True


def configure_prefix(log: Log) -> None:
    """Onegi olustur, Direct3D ayarlarini ve ortam degiskenlerini yaz."""
    wine = wine_binary()
    if wine is None:
        log.warn("wine bulunamadi; onek ayarlanmadi. Once: python3 makefile.py")
        return

    if not ensure_prefix(log, wine):
        return

    log.step("Direct3D kayit ayarlari (GM965)")
    environment = wine_environment(PREFIX)
    written = 0
    for path, keys in ((r"HKCU\Software\Wine\Direct3D", DIRECT3D_KEYS),
                       (r"HKCU\Software\Wine\X11 Driver", X11_KEYS)):
        for name, kind, value in keys:
            result = run([wine, "reg", "add", path, "/v", name,
                          "/t", kind, "/d", value, "/f"],
                         environment=environment, capture=True)
            if result.returncode == 0:
                written += 1
                log.detail(f"{path}\\{name} = {value}")
    log.detail(f"{written} anahtar yazildi")

    log.step("Mesa ve Wine ortam degiskenleri")
    write_profile(log)


def profile_text() -> str:
    # Yol, f-string'in ICINDE degil DISINDA hesaplanir: bir f-string icinde ayni
    # tirnagi kullanmak Python 3.12'de (PEP 701) gecerli, 3.11 ve oncesinde
    # SOZDIZIMI HATASIDIR. Debian 12'nin python3'u 3.11'dir; bu dosya orada da
    # calismalidir. Bu yuzden deger once degiskene alinir.
    bin_dir = WINE_PREFIX / "bin"
    lines = [
        "# GnuchanOS: Wine ve Mesa ayarlari (Intel GM965 / GMA X3100).",
        "#",
        "# Bu dosyadaki her satirin NEDEN orada oldugu yanindadir.",
        "# Geri almak icin:  sudo rm /etc/profile.d/gnuchan-wine.sh",
        "#",
        "# NOT: DXVK kullanilmaz. DXVK Vulkan ister, GM965'te Vulkan yoktur;",
        "#      kurulursa siyah ekran verir.",
        "",
        f'export PATH="{bin_dir}:$PATH"',
        "",
    ]
    for name, value, why in ENVIRONMENT:
        lines.append(f"# {why}")
        lines.append(f"export {name}={value}")
        lines.append("")
    return "\n".join(lines)


def write_profile(log: Log) -> None:
    PROFILE_PATH.parent.mkdir(parents=True, exist_ok=True)
    PROFILE_PATH.write_text(profile_text(), encoding="utf-8")
    PROFILE_PATH.chmod(0o644)
    log.detail(f"yazildi: {PROFILE_PATH}")


# --- durum / diger -----------------------------------------------------------


def status(log: Log) -> int:
    log.step("Durum")
    log.detail(f"wined3d kaynagi  {WINED3D_SOURCE}")
    log.detail(f"wine agaci       {SOURCE_DIR} "
               f"({'var' if SOURCE_DIR.exists() else 'yok'})")
    log.detail(f"derleme agaci    {BUILD_DIR} "
               f"({'var' if (BUILD_DIR / 'Makefile').exists() else 'yok'})")

    main = WINED3D_SOURCE / "wined3d_main.c"
    if main.is_file():
        text = main.read_text(encoding="utf-8", errors="replace")
        if "MAKEDWORD_VERSION(2, 1)" in text:
            log.detail("optimizasyon    max_gl_version = 2,1 (GM965)  EVET")
        else:
            log.warn("optimizasyon    wined3d_main.c'de 2,1 bulunamadi")
    log.detail(f"wine ikilisi     {wine_binary() or '(yok)'}")
    log.detail(f"onek             {PREFIX} "
               f"({'var' if (PREFIX / 'system.reg').exists() else 'yok'})")

    if which("glxinfo") is not None:
        result = run(["glxinfo", "-B"], capture=True)
        for line in result.stdout.splitlines():
            if "OpenGL renderer string" in line:
                log.detail(f"GL renderer      {line.partition(':')[2].strip()}")
    return 0


def run_program(log: Log, arguments: list[str]) -> int:
    binary = WINE_PREFIX / "bin" / "wine"
    if not os.access(str(binary), os.X_OK):
        raise SystemExit(f"error: {binary} yok. Once: python3 makefile.py")
    if not os.environ.get("DISPLAY"):
        raise SystemExit("error: DISPLAY ayarli degil; X sunucusu yok")
    environment = wine_environment(PREFIX)
    environment["PATH"] = f"{WINE_PREFIX / 'bin'}:{environment.get('PATH', '')}"
    if not arguments:
        log.step("winecfg")
        return run([str(binary), "winecfg"], environment=environment).returncode
    log.step("Program calistiriliyor")
    log.detail(" ".join(arguments))
    return run([str(binary), *arguments], environment=environment).returncode


def uninstall(log: Log) -> None:
    log.step("Kaldiriliyor")
    if WINE_PREFIX.exists():
        shutil.rmtree(WINE_PREFIX)
        log.detail(f"silindi: {WINE_PREFIX}")
        run(["ldconfig"], capture=True)
    if PROFILE_PATH.exists():
        PROFILE_PATH.unlink()
        log.detail(f"silindi: {PROFILE_PATH}")
    log.note(f"Onek SILINMEDI ({PREFIX}) ve kaynak SILINMEDI ({WINED3D_SOURCE}).")


def clean(log: Log) -> None:
    log.step("Temizleniyor")
    for directory in (BUILD_DIR, SOURCE_DIR):
        if directory.exists():
            shutil.rmtree(directory)
            log.detail(f"silindi: {directory}")
    log.detail(f"optimize edilmis kaynak DURUYOR: {WINED3D_SOURCE}")


# --- giris -------------------------------------------------------------------


def usage() -> None:
    print(
        "kullanim: python3 makefile.py [eylem]\n"
        "\n"
        "  (parametresiz)   deps + source + optimize + build + install + prefix\n"
        "  deps             yalnizca derleme kutuphanelerini kur\n"
        "  source           yalnizca wine agacini indir/guncelle\n"
        "  optimize         yalnizca optimize wined3d'yi agaca yerlestir\n"
        "  configure        optimize + configure\n"
        "  build            optimize + derle + kur (wined3d DAHIL)\n"
        "  wined3d          yalnizca wined3d'yi derle ve kur (hizli)\n"
        "  prefix           yalnizca onek + Direct3D ayarlari + ortam\n"
        "  status           durumu yaz, hicbir sey degistirme\n"
        "  run [program]    kurulu wine ile calistir (argumansiz: winecfg)\n"
        "  clean            derleme + kaynak agacini sil (optimize kaynak kalir)\n"
        "  uninstall        /opt kurulumunu sil (onek ve kaynak kalir)\n"
    )


def install_everything(log: Log) -> int:
    install_dependencies(log)
    fetch_source(log)
    optimize(log)
    ensure_configured(log)
    build_wine(log)
    install_wine(log)
    configure_prefix(log)
    log.note()
    log.note("Wine + optimize edilmis wined3d kuruldu.")
    log.note("Dogrulamak icin:  python3 makefile.py status")
    log.note(f"Oyun calistirmak icin:  {WINE_PREFIX / 'bin' / 'wine'} oyun.exe")
    return 0


def main() -> int:
    if platform.system().lower() != "linux":
        raise SystemExit(f"error: bu bir Debian X11 derlemesidir; bu makine "
                         f"{platform.system()}")
    if which("apt-get") is None:
        raise SystemExit("error: bu apt-get ile kurar; Debian'a ozeldir")

    log = Log()
    action = sys.argv[1] if len(sys.argv) > 1 else "install"
    known = ("install", "deps", "source", "optimize", "configure", "build",
             "wined3d", "prefix", "status", "run", "clean", "uninstall")

    if action in ("--help", "-h"):
        usage()
        return 0
    if action not in known:
        log.warn(f"bilinmeyen eylem '{action}'")
        usage()
        return 2

    # /opt ve apt-get root ister. `run` ve `status` istemez.
    if action in ("install", "deps", "uninstall"):
        ensure_root(log)

    if action == "status":
        return status(log)
    if action == "deps":
        install_dependencies(log)
        return 0
    if action == "source":
        fetch_source(log)
        return 0
    if action == "optimize":
        fetch_source(log)
        optimize(log)
        return 0
    if action == "configure":
        fetch_source(log)
        optimize(log)
        install_dependencies(log)
        ensure_configured(log)
        return 0
    if action == "build":
        build_wine(log)
        install_wine(log)
        return 0
    if action == "wined3d":
        build_wined3d(log)
        return 0
    if action == "prefix":
        configure_prefix(log)
        return 0
    if action == "run":
        return run_program(log, sys.argv[2:])
    if action == "clean":
        clean(log)
        return 0
    if action == "uninstall":
        uninstall(log)
        return 0

    return install_everything(log)


if __name__ == "__main__":
    raise SystemExit(main())
