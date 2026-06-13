@echo off
setlocal
echo ==========================================
echo    PXView Sync to Windows ^& WSL Script
echo ==========================================
echo.

REM 1. Run local Windows sync via PowerShell script
echo [1/2] Syncing to local Windows directory (C:\Users\admin\Downloads\PXView-master)...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0sync_to_repo.ps1" -TargetDir "C:\Users\admin\Downloads\PXView-master"

echo.
REM 2. Sync from Windows to WSL via rsync
echo [2/2] Syncing from Windows to WSL (Ubuntu-22.04)...

REM Ensure target directory exists in WSL
wsl -d Ubuntu-22.04 -- mkdir -p /home/muse/Downloads/PXView-build/PXView-master

REM Run rsync for incremental sync with deletion of extra files, but exclude build and local folders
wsl -d Ubuntu-22.04 -- rsync -av --delete --exclude="build/" --exclude="build.dir/" --exclude="install.dir/" --exclude=".git/" --exclude="node_modules/" /mnt/c/Users/admin/Downloads/PXView-master/ /home/muse/Downloads/PXView-build/PXView-master/

echo.
echo ==========================================
echo    Sync Complete!
echo ==========================================
pause
endlocal
