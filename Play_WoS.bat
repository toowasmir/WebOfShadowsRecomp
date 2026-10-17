@echo off
title Spider-Man: Web of Shadows — ReComp Modern Edition
cd /d "%~dp0"
set WOS_AUTOSKIP=0
set WOS_CAPTURE_TEST=0
if exist "bin\wos_recomp.exe" (
    start "" /high "bin\wos_recomp.exe"
) else (
    echo [HATA] bin\wos_recomp.exe bulunamadi!
    pause
)
