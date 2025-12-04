#!/bin/bash

# Script: wine_dependencies.sh
# Wine oyunları için gerekli winetricks paketlerini yükleyen bir script.

# 1. DirectX ve Grafik Bileşenleri
echo "Yükleniyor: DirectX ve Grafik Bileşenleri..."
echo "Installing: DirectX and Graphics Components..."
winetricks -q d3dx9
winetricks -q d3dx10
winetricks -q dxvk
winetricks -q vkd3d
winetricks -q cnc_ddraw
#winetricks -q quartz this is shit don't install

# 2. Visual C++ Runtime Bileşenleri
echo "Yükleniyor: Visual C++ Runtime Bileşenleri..."
echo "Installing: Visual C++ Runtime Components..."
winetricks -q vcrun6
winetricks -q vcrun6sp6
winetricks -q vcrun2008
winetricks -q vcrun2010
winetricks -q vcrun2015
winetricks -q vcrun2019
winetricks -q dotnet20
winetricks -q dotnet40
winetricks -q dotnet45
winetricks -q dotnet48

# 3. Ses ve Video Codec Bileşenleri
echo "Yükleniyor: Ses ve Video Codec Bileşenleri..."
echo "Installing: Audio and Video Codec Components..."
winetricks -q faudio
winetricks -q icodecs
winetricks -q quicktime72
winetricks -q quicktime76

# 4. Windows ve Sistem Bileşenleri
echo "Yükleniyor: Windows ve Sistem Bileşenleri..."
echo "Installing: Windows and System Components..."
winetricks -q wmp9
winetricks -q wmp10
winetricks -q msxml3
winetricks -q mfc42
winetricks -q wsh57
winetricks -q devenum
#winetricks -q dinput8 this problem in oblivion game mouse stuck on window border

# 5. Yöneticiler ve Diğer Yardımcı Bileşenler
echo "Yükleniyor: Yöneticiler ve Diğer Yardımcı Bileşenler..."
echo "Installing: Managers and Other Helper Components..."
winetricks -q corefonts
winetricks -q cjkfonts
winetricks -q physx
winetricks -q dxdiag

# 6. Sistem ve Tanılama Bileşenleri
echo "Yükleniyor: Sistem ve Tanılama Bileşenleri..."
echo "Installing: System and Diagnostic Components..."
winetricks -q dxdiagn

echo "Tüm gerekli paketler yüklendi!"
echo "All required packages have been installed!"
