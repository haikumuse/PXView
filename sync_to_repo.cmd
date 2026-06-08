@echo off
setlocal
echo ==========================================
echo    PXView Sync Script
echo ==========================================
echo Default target: C:\Users\admin\Downloads\PXView-master

set "TARGET_DIR="
set /p "TARGET_DIR=Enter target path (Press Enter to use default): "

if "%TARGET_DIR%"=="" set "TARGET_DIR=C:\Users\admin\Downloads\PXView-master"

echo Syncing to %TARGET_DIR% via PowerShell...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0sync_to_repo.ps1" -TargetDir "%TARGET_DIR%"

echo.
pause
endlocal
