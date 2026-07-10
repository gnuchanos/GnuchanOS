@echo off
chcp 65001 >nul
title GnuchanOS Build Tool
setlocal enabledelayedexpansion

set TEMPWORK=D:\GnuchanOS\_tempWork
set TEMPLATE=D:\GnuchanOS\_template

if "%1"=="" goto :help

if "%1"=="install" goto :install
if "%1"=="build" goto :build
if "%1"=="build-full" goto :build-full
if "%1"=="clean" goto :clean
if "%1"=="distclean" goto :distclean
goto :help

:help
echo.
echo === GnuchanOS Build Tool ===
echo.
echo Kullanim: build.bat ^<komut^>
echo.
echo   install      Build ortamini kur (WSL2 Debian + araclar)
echo   build        Hizli ISO build (10-15 dk)
echo   build-full   Tam ISO build (kernel dahil, 1-2 saat)
echo   clean        Gecici dosyalari temizle
echo   distclean    TUMU sil (rootfs, iso, cache, tempWork)
echo.
echo ON KOSUL: WSL2 Debian kurulu olmali
echo   PowerShell (Admin): wsl --install -d Debian
echo.
goto :end

:install
echo.
echo === GnuchanOS Build Ortami Kurulumu ===
echo.
echo [1/4] WSL2 Debian kontrol...
wsl --list --verbose 2>nul | findstr Debian >nul
if %errorlevel% neq 0 (
    echo HATA: WSL2 Debian kurulu degil.
    echo Once su komutu calistir:
    echo   PowerShell Yonetici: wsl --install -d Debian
    goto :end
)
echo WSL2 Debian hazir.
echo.
echo [2/4] _tempWork dizinleri olusturuluyor...
if not exist "%TEMPWORK%\tmp" mkdir "%TEMPWORK%\tmp"
if not exist "%TEMPWORK%\ccache" mkdir "%TEMPWORK%\ccache"
if not exist "%TEMPWORK%\debootstrap" mkdir "%TEMPWORK%\debootstrap"
if not exist "%TEMPWORK%\pip" mkdir "%TEMPWORK%\pip"
if not exist "%TEMPWORK%\apt" mkdir "%TEMPWORK%\apt"
if not exist "%TEMPWORK%\git" mkdir "%TEMPWORK%\git"
if not exist "%TEMPWORK%\logs" mkdir "%TEMPWORK%\logs"
if not exist "%TEMPWORK%\downloads" mkdir "%TEMPWORK%\downloads"
echo OK.
echo.
echo [3/4] WSL2'de build araclari kuruluyor...
echo Bu islem 2-5 dk surebilir...
wsl -d Debian -e bash -c "cd /mnt/d/GnuchanOS/_template/scripts && sudo bash setup-build-env.sh"
if %errorlevel% neq 0 (
    echo HATA: Build araclari kurulamadi.
    goto :end
)
echo OK.
echo.
echo [4/4] env.bat olusturuluyor...
(
echo @echo off
echo rem GnuchanOS Build Env
echo set TMPDIR=%TEMPWORK%\tmp
echo set CCACHE_DIR=%TEMPWORK%\ccache
echo set PIP_CACHE_DIR=%TEMPWORK%\pip
echo set APT_CACHE_DIR=%TEMPWORK%\apt
echo set GIT_CLONE_DIR=%TEMPWORK%\git
echo set DEBOOTSTRAP_DIR=%TEMPWORK%\debootstrap
) > "%TEMPWORK%\env.bat"
echo OK.
echo.
echo === KURULUM TAMAM! ===
echo Simdi: build.bat build
goto :end

:build
echo.
echo === GnuchanOS ISO Build (Hizli) ===
echo WSL2'de build-all.sh baslatiliyor...
echo Bu islem 10-15 dk surebilir...
echo.
wsl -d Debian -e bash -c "^
cd /mnt/d/GnuchanOS/_template/scripts && ^
export TMPDIR=/mnt/d/GnuchanOS/_tempWork/tmp ^
CCACHE_DIR=/mnt/d/GnuchanOS/_tempWork/ccache ^
PIP_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/pip ^
APT_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/apt ^
GIT_CLONE_DIR=/mnt/d/GnuchanOS/_tempWork/git ^
DEBOOTSTRAP_DIR=/mnt/d/GnuchanOS/_tempWork/debootstrap ^
&& sudo bash build-all.sh --skip-kernel --skip-liberated --skip-xlibre"
echo.
echo === BUILD BITTI ===
wsl -d Debian -e bash -c "ls -lh /mnt/d/GnuchanOS/_template/iso/gnuchanos.iso"
goto :end

:build-full
echo.
echo === GnuchanOS ISO Build (Tam) ===
echo WSL2'de build-all.sh baslatiliyor...
echo Bu islem 1-2 saat surebilir...
echo.
wsl -d Debian -e bash -c "^
cd /mnt/d/GnuchanOS/_template/scripts && ^
export TMPDIR=/mnt/d/GnuchanOS/_tempWork/tmp ^
CCACHE_DIR=/mnt/d/GnuchanOS/_tempWork/ccache ^
PIP_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/pip ^
APT_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/apt ^
GIT_CLONE_DIR=/mnt/d/GnuchanOS/_tempWork/git ^
DEBOOTSTRAP_DIR=/mnt/d/GnuchanOS/_tempWork/debootstrap ^
&& sudo bash build-all.sh"
echo.
echo === BUILD BITTI ===
wsl -d Debian -e bash -c "ls -lh /mnt/d/GnuchanOS/_template/iso/gnuchanos.iso"
goto :end

:clean
echo Gecici dosyalar temizleniyor...
if exist "%TEMPWORK%\tmp" rmdir /s /q "%TEMPWORK%\tmp"
if exist "%TEMPWORK%\git" rmdir /s /q "%TEMPWORK%\git"
if exist "%TEMPWORK%\logs" rmdir /s /q "%TEMPWORK%\logs"
if exist "%TEMPWORK%\downloads" rmdir /s /q "%TEMPWORK%\downloads"
mkdir "%TEMPWORK%\tmp" "%TEMPWORK%\git" "%TEMPWORK%\logs" "%TEMPWORK%\downloads" 2>nul
echo Korundu: ccache, pip, apt, debootstrap
wsl -d Debian -e bash -c "rm -rf /mnt/d/GnuchanOS/_tempWork/tmp/* /mnt/d/GnuchanOS/_tempWork/git/* /mnt/d/GnuchanOS/_tempWork/logs/* /mnt/d/GnuchanOS/_tempWork/downloads/* 2>/dev/null"
echo Temizlik tamam.
goto :end

:distclean
echo TUM build dosyalari siliniyor!
if exist "%TEMPWORK%" rmdir /s /q "%TEMPWORK%"
if exist "%TEMPLATE%\rootfs" rmdir /s /q "%TEMPLATE%\rootfs"
if exist "%TEMPLATE%\iso" rmdir /s /q "%TEMPLATE%\iso"
if exist "%TEMPLATE%\cache" rmdir /s /q "%TEMPLATE%\cache"
wsl -d Debian -e bash -c "rm -rf /mnt/d/GnuchanOS/_template/rootfs /mnt/d/GnuchanOS/_template/iso /mnt/d/GnuchanOS/_template/cache /mnt/d/GnuchanOS/_tempWork 2>/dev/null"
echo Sifirlama tamam!
goto :end

:end
endlocal
