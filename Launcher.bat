@echo off
title Spider-Man: Web of Shadows — ReComp Modern Edition Launcher
cd /d "%~dp0"
if exist "WoS_Launcher.exe" (
    start "" "WoS_Launcher.exe"
) else (
    start "" pythonw launcher.py 2>nul || python launcher.py
)
