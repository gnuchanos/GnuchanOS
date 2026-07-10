@echo off
chcp 65001 >nul
title GnuchanOS WSL2 Kurulum (D:\WSL\ dizinine)
echo.
echo === GnuchanOS WSL2 Kurulumu ===
echo.
echo Bu script WSL2'yi D:\WSL\ dizinine kurar.
echo C: dolu oldugu icin D: kullanilir.
echo.
echo [!] PowerShell'i YONETICI olarak calistir!
echo [!] Bilgisayari yeniden baslatman gerekebilir.
echo.
pause

echo [1/4] WSL ozellikleri etkinlestiriliyor...
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart >nul 2>&1
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart >nul 2>&1
echo.

echo [2/4] WSL2 varsayilan yapiliyor...
wsl --set-default-version 2 >nul 2>&1
echo.

echo [3/4] Debian C:\Users\btw\AppData\Local\Packages\ dizinine indiriliyor...
echo Bu islem 2-5 dk surebilir. Lutfen bekle...

REM Once eski kaydi sil
wsl --unregister Debian >nul 2>&1
wsl --unregister Ubuntu >nul 2>&1

REM Debian'i kur (C: gecici olarak kullanilir)
wsl --install -d Debian

REM Kullanici kurulumu tamamlamis mi kontrol et
echo.
echo ========================================================
echo WSL2 KURULDU. Ilk acilista kullanici adi/sifre isteyecek.
echo Lutfen:
echo   1. Kullanici adi: gnuchan  (veya istedigin)
echo   2. Sifre: bir seyler yaz
echo   3. Sonra CIKMA, su komutu gir:
echo      sudo apt update ^&^& sudo apt install -y debootstrap squashfs-tools xorriso grub-pc-bin grub-efi-amd64-bin isolinux syslinux-common git wget curl build-essential gcc make meson ninja-build python3 python3-pip python3-venv python3-setuptools python3-wheel libpixman-1-dev libepoxy-dev libdrm-dev libgbm-dev libxcb*-dev libxcvt-dev libxfont2-dev libxkbfile-dev libpciaccess-dev libudev-dev libdbus-1-dev pkg-config bison flex bc kmod cpio rsync dosfstools mtools
echo   4. exit yaz
echo ========================================================
echo.
echo WSL2 aciliyor...
wsl -d Debian

echo [4/4] WSL2 D:\WSL\ dizinine tasiniyor...
echo.
echo WSL image export ediliyor...
if not exist D:\WSL mkdir D:\WSL
wsl --export Debian D:\WSL\debian.tar
echo Eski kayit siliniyor...
wsl --unregister Debian
echo D:\WSL\debian\ dizinine import ediliyor...
wsl --import Debian D:\WSL\debian D:\WSL\debian.tar --version 2
echo Gecici dosya siliniyor...
del D:\WSL\debian.tar
echo.
echo Tasima tamam! WSL2 su anda D:\WSL\debian\ dizininde.
echo.
echo === WSL2 KURULUMU TAMAM ===
echo Simdi bu komutu calistir:
echo   build.bat install
echo.
