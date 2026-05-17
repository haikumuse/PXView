@echo off
echo ==========================================
echo PXView Incremental Build Launcher
echo ==========================================

set MSYS2_PATH=D:\msys64

echo Starting MinGW64 environment...
cd /d "%~dp0"
%MSYS2_PATH%\msys2_shell.cmd -mingw64 -defterm -no-start -here -c "./build_incremental.sh"