@echo off
setlocal
cd /d "%~dp0"
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;%~dp0sdk\rexglue-sdk\win-amd64\bin;%~dp0sdk\msvc\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64;%~dp0sdk\msvc\Windows Kits\10\bin\10.0.28000.0\x64;%~dp0sdk\msvc\Windows Kits\10\bin\10.0.28000.0\x64\ucrt;%PATH%"
set "INCLUDE=%~dp0sdk\msvc\VC\Tools\MSVC\14.51.36231\include;%~dp0sdk\msvc\Windows Kits\10\include\10.0.28000.0\ucrt;%~dp0sdk\msvc\Windows Kits\10\include\10.0.28000.0\um;%~dp0sdk\msvc\Windows Kits\10\include\10.0.28000.0\shared;%~dp0sdk\msvc\Windows Kits\10\include\10.0.28000.0\winrt;%~dp0sdk\msvc\Windows Kits\10\include\10.0.28000.0\cppwinrt"
set "LIB=%~dp0sdk\msvc\VC\Tools\MSVC\14.51.36231\lib\x64;%~dp0sdk\msvc\Windows Kits\10\lib\10.0.28000.0\ucrt\x64;%~dp0sdk\msvc\Windows Kits\10\lib\10.0.28000.0\um\x64"
set "REXGLUE_SDK_DIR=%~dp0sdk\rexglue-sdk\win-amd64"
set "CMAKE_PREFIX_PATH=%~dp0sdk\rexglue-sdk\win-amd64"
ninja -C out\build\win-amd64-release
if %ERRORLEVEL% equ 0 (
    echo [WoS] Build successful!
) else (
    echo [WoS] Build failed!
    exit /b 1
)
endlocal
