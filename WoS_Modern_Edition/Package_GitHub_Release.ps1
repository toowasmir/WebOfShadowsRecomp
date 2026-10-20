# Spider-Man: Web of Shadows — ReComp Modern Edition
# GitHub Release & Repository Packager (100% Legal / No Copyrighted Game Assets)

$ErrorActionPreference = "Stop"
$BaseDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ReleaseDir = Join-Path $BaseDir "dist_github_release\WoS_ReComp_Modern_v2.0.1"
$ZipOut = Join-Path $BaseDir "dist_github_release\WoS_ReComp_Modern_Edition_v2.0.1_Win64.zip"

Write-Host "[1/4] GitHub Release klasoru hazirlaniyor..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "bin") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "assets") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "tools") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "patches") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "src\hooks") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "src\stubs") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "saves_backup") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "logs") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseDir "game") | Out-Null

# Put a placeholder README inside game/ so GitHub users know where ISO extracts
Set-Content -Path (Join-Path $ReleaseDir "game\PUT_XBOX360_GAME_FILES_HERE.txt") -Value @"
Spider-Man: Web of Shadows (Xbox 360 Title ID: 41560815)
========================================================
[EN]
For legal reasons, no copyrighted Xbox 360 game assets (default.xex, .pak, .bik) are included in this package.
To install:
1. Run 'WoS_Launcher.exe' in the main directory.
2. Open the 'Files' tab on the left navigation rail.
3. Click 'Install from .ISO' and select your own legally acquired Xbox 360 .ISO image (or copy your extracted 'default.xex' and game files directly into this 'game/' folder).

[TR]
Telif haklari nedeniyle oyun varlik dosyalari (default.xex, .pak, .bik) bu pakete dahil edilmemistir.
Kurulum icin:
1. Ana klasordeki 'WoS_Launcher.exe' dosyasini calistirin.
2. Sol menuden 'Dosyalar' sekmesine gecin.
3. '.ISO Dosyasindan Kur' butonuna tiklayip kendi Xbox 360 .ISO dosyanizi secin.
"@

Write-Host "[2/4] Calistirilabilir motor (wos_recomp.exe + DLL), Launcher ve araclar kopyalaniyor..." -ForegroundColor Cyan
Copy-Item -Force (Join-Path $BaseDir "bin\wos_recomp.exe") (Join-Path $ReleaseDir "bin\")
Copy-Item -Force (Join-Path $BaseDir "bin\rexruntime.dll") (Join-Path $ReleaseDir "bin\")
Copy-Item -Force (Join-Path $BaseDir "bin\rexgpu-xenos.dll") (Join-Path $ReleaseDir "bin\")
Copy-Item -Force (Join-Path $BaseDir "recomp_config.json") (Join-Path $ReleaseDir "bin\")

if (Test-Path (Join-Path $BaseDir "WoS_Launcher.exe")) {
    Copy-Item -Force (Join-Path $BaseDir "WoS_Launcher.exe") $ReleaseDir
}
Copy-Item -Force (Join-Path $BaseDir "launcher.py") $ReleaseDir
Copy-Item -Force (Join-Path $BaseDir "assets\*.png") (Join-Path $ReleaseDir "assets\")
Copy-Item -Force (Join-Path $BaseDir "assets\*.ico") (Join-Path $ReleaseDir "assets\") -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BaseDir "Launcher.bat") $ReleaseDir
Copy-Item -Force (Join-Path $BaseDir "Play_WoS.bat") $ReleaseDir
Copy-Item -Force (Join-Path $BaseDir "recomp_config.json") $ReleaseDir
Copy-Item -Force (Join-Path $BaseDir "README.md") $ReleaseDir
Copy-Item -Force (Join-Path $BaseDir ".gitignore") $ReleaseDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BaseDir "logs\README.md") (Join-Path $ReleaseDir "logs\") -ErrorAction SilentlyContinue

if (Test-Path (Join-Path $BaseDir "tools\extract-xiso.exe")) {
    Copy-Item -Force (Join-Path $BaseDir "tools\extract-xiso.exe") (Join-Path $ReleaseDir "tools\")
}
Copy-Item -Force (Join-Path $BaseDir "patches\*") (Join-Path $ReleaseDir "patches\")
Copy-Item -Force (Join-Path $BaseDir "src\*.cpp") (Join-Path $ReleaseDir "src\") -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BaseDir "src\*.h") (Join-Path $ReleaseDir "src\") -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BaseDir "src\hooks\*") (Join-Path $ReleaseDir "src\hooks\") -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BaseDir "src\stubs\*") (Join-Path $ReleaseDir "src\stubs\") -ErrorAction SilentlyContinue

Write-Host "[3/4] Telifli oyun dosyasi (.xex / .iso / .pak) olmadigi dogrulaniyor..." -ForegroundColor Cyan
$Forbidden = Get-ChildItem -Path $ReleaseDir -Recurse -Include *.xex,*.iso,*.pak,*.bik
if ($Forbidden.Count -gt 0) {
    throw "HATA: Telifli oyun dosyasi tespit edildi! Paketleme durduruldu."
}

Write-Host "[4/4] GitHub Release .ZIP arsivi olusturuluyor: $ZipOut" -ForegroundColor Cyan
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipOut -Force

$ZipSizeMB = [math]::Round((Get-Item $ZipOut).Length / 1MB, 2)
Write-Host "TAMAMLANDI! GitHub Release paketi hazir ($ZipSizeMB MB): $ZipOut" -ForegroundColor Green
