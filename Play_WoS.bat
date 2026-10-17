@echo off
cd /d "%~dp0out\build\win-amd64-release"
title Spider-Man: Web of Shadows (PC Recomp)
start "" wos_recomp.exe --fullscreen=false --window_width=1280 --window_height=720
