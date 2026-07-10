# GnuchanOS Build Script
# Kullanim: .\build.ps1 install
#          .\build.ps1 build
#          .\build.ps1 build-full
#          .\build.ps1 clean
#          .\build.ps1 distclean
#
# ON KOSUL: wsl --install -d Debian (PowerShell Admin)

param(
    [string]$Task = "help"
)

$TEMPWORK = "D:\GnuchanOS\_tempWork"
$TEMPLATE = "D:\GnuchanOS\_template"
$WSL_CMD = "cd /mnt/d/GnuchanOS/_template/scripts"
$WSL_ENV = "export TMPDIR=/mnt/d/GnuchanOS/_tempWork/tmp CCACHE_DIR=/mnt/d/GnuchanOS/_tempWork/ccache PIP_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/pip APT_CACHE_DIR=/mnt/d/GnuchanOS/_tempWork/apt GIT_CLONE_DIR=/mnt/d/GnuchanOS/_tempWork/git DEBOOTSTRAP_DIR=/mnt/d/GnuchanOS/_tempWork/debootstrap"

function Help {
    Write-Host "`n=== GnuchanOS Build Tool ==="
    Write-Host "`nKullanim: .\build.ps1 <komut>"
    Write-Host "`n  install      Build ortamini kur (WSL2 + araclar)"
    Write-Host "  build        Hizli ISO build (--skip, 10-15dk)"
    Write-Host "  build-full   Tam ISO build (kernel dahil, 1-2s)"
    Write-Host "  clean        Gecici dosyalari temizle"
    Write-Host "  distclean    TUMUNU sil"
    Write-Host "`nON KOSUL: wsl --install -d Debian (PowerShell Admin)"
    Write-Host ""
}

function Install-Env {
    Write-Host "`n=== GnuchanOS Build Ortami Kurulumu ==="
    
    # WSL check
    $wslList = wsl --list --verbose 2>$null
    if ($wslList -notmatch "Debian") {
        Write-Host "HATA: WSL2 Debian kurulu degil!"
        Write-Host "Once: wsl --install -d Debian (PowerShell Admin)"
        return
    }
    Write-Host "[OK] WSL2 Debian hazir"
    
    # Create tempwork dirs
    Write-Host "[1/3] _tempWork dizinleri..."
    @("tmp","ccache","debootstrap","pip","apt","git","logs","downloads") | ForEach-Object {
        $d = "$TEMPWORK\$_"
        if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
    }
    Write-Host "[OK] _tempWork hazir"
    
    # Install build tools
    Write-Host "[2/3] WSL2 build araclari kuruluyor (2-5dk)..."
    wsl -d Debian -e bash -c "$WSL_CMD && sudo bash setup-build-env.sh"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "HATA: Build araclari kurulamadi!"
        return
    }
    Write-Host "[OK] Build araclari kuruldu"
    
    # Create env.bat
    Write-Host "[3/3] env.bat..."
    @"
@echo off
rem GnuchanOS Build Env
set TMPDIR=$TEMPWORK\tmp
set CCACHE_DIR=$TEMPWORK\ccache
set PIP_CACHE_DIR=$TEMPWORK\pip
set APT_CACHE_DIR=$TEMPWORK\apt
set GIT_CLONE_DIR=$TEMPWORK\git
set DEBOOTSTRAP_DIR=$TEMPWORK\debootstrap
"@ | Out-File -FilePath "$TEMPWORK\env.bat" -Encoding ascii
    Write-Host "[OK] env.bat olusturuldu"
    
    Write-Host "`n=== KURULUM TAMAM ==="
    Write-Host "Simdi: .\build.ps1 build"
}

function Build-ISO {
    Write-Host "`n=== GnuchanOS ISO Build (Hizli) ==="
    Write-Host "WSL2 build basliyor (10-15dk)..."
    wsl -d Debian -e bash -c "$WSL_CMD && $WSL_ENV && sudo bash build-all.sh --skip-kernel --skip-liberated --skip-xlibre"
    Write-Host "`n=== BUILD BITTI ==="
    wsl -d Debian -e bash -c "ls -lh /mnt/d/GnuchanOS/_template/iso/gnuchanos.iso"
}

function Build-Full {
    Write-Host "`n=== GnuchanOS ISO Build (Tam) ==="
    Write-Host "WSL2 build basliyor (1-2s)..."
    wsl -d Debian -e bash -c "$WSL_CMD && $WSL_ENV && sudo bash build-all.sh"
    Write-Host "`n=== BUILD BITTI ==="
    wsl -d Debian -e bash -c "ls -lh /mnt/d/GnuchanOS/_template/iso/gnuchanos.iso"
}

function Clean-Temp {
    Write-Host "Gecici dosyalar temizleniyor..."
    @("tmp","git","logs","downloads") | ForEach-Object {
        $d = "$TEMPWORK\$_"
        if (Test-Path $d) { Remove-Item -Recurse -Force $d -ErrorAction SilentlyContinue }
    }
    @("tmp","git","logs","downloads") | ForEach-Object {
        $d = "$TEMPWORK\$_"
        if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
    }
    Write-Host "Korundu: ccache, pip, apt, debootstrap"
    wsl -d Debian -e bash -c "rm -rf /mnt/d/GnuchanOS/_tempWork/tmp/* /mnt/d/GnuchanOS/_tempWork/git/* /mnt/d/GnuchanOS/_tempWork/logs/* /mnt/d/GnuchanOS/_tempWork/downloads/* 2>/dev/null" 2>$null
    Write-Host "Temizlik tamam"
}

function DistClean {
    Write-Host "TUM build dosyalari siliniyor!"
    if (Test-Path $TEMPWORK) { Remove-Item -Recurse -Force $TEMPWORK -ErrorAction SilentlyContinue }
    if (Test-Path "$TEMPLATE\rootfs") { Remove-Item -Recurse -Force "$TEMPLATE\rootfs" -ErrorAction SilentlyContinue }
    if (Test-Path "$TEMPLATE\iso") { Remove-Item -Recurse -Force "$TEMPLATE\iso" -ErrorAction SilentlyContinue }
    if (Test-Path "$TEMPLATE\cache") { Remove-Item -Recurse -Force "$TEMPLATE\cache" -ErrorAction SilentlyContinue }
    wsl -d Debian -e bash -c "rm -rf /mnt/d/GnuchanOS/_template/rootfs /mnt/d/GnuchanOS/_template/iso /mnt/d/GnuchanOS/_template/cache /mnt/d/GnuchanOS/_tempWork 2>/dev/null" 2>$null
    Write-Host "Sifirlama tamam!"
}

switch ($Task) {
    "install"    { Install-Env }
    "build"      { Build-ISO }
    "build-full" { Build-Full }
    "clean"      { Clean-Temp }
    "distclean"  { DistClean }
    default      { Help }
}
