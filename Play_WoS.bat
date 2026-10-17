@echo off
title Spider-Man: Web of Shadows — ReComp Modern Edition
cd /d "%~dp0"
set WOS_AUTOSKIP=0
set WOS_CAPTURE_TEST=0
if not exist "logs" mkdir "logs"
if exist "bin\wos_recomp.exe" (
    pushd "bin"
    start "" /high "wos_recomp.exe" --log_file=..\logs\wos_recomp_latest.log
    popd
) else (
    echo [ERROR] bin\wos_recomp.exe not found!
    pause
)
