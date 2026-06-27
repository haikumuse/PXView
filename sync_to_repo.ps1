param (
    [string]$TargetDir = "C:\Users\admin\Downloads\PXView-master"
)

$SourceDir = $PSScriptRoot

if (-Not (Test-Path $TargetDir)) {
    Write-Host "Target directory '$TargetDir' does not exist. Creating it now..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
}

$FoldersToCopy = @("PXView", "libsigrok", "libsigrokdecode", "common", "lang", "CMake", "debian", "web", "doc")
$FilesToCopy = @(
    "CMakeLists.txt", "logo-win.ico", "applogo.rc", "README.md", "INSTALL.md", "INSTALL_zh.md", "COPYING", "LICENSE", 
    "PXView.icns", "clean", "cmake_clear", "build_linux", "$([char]0x66F4)$([char]0x65B0)$([char]0x8BF4)$([char]0x660E).txt"
)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Syncing Development Directory to Repo" -ForegroundColor Cyan
Write-Host " Source: $SourceDir"
Write-Host " Target: $TargetDir"
Write-Host "==========================================" -ForegroundColor Cyan

foreach ($folder in $FoldersToCopy) {
    if (Test-Path "$SourceDir\$folder") {
        Write-Host "Copying folder: $folder"
        Copy-Item -Path "$SourceDir\$folder" -Destination "$TargetDir" -Recurse -Force
    }
}



foreach ($file in $FilesToCopy) {
    if (Test-Path "$SourceDir\$file") {
        Write-Host "Copying file: $file"
        Copy-Item -Path "$SourceDir\$file" -Destination "$TargetDir\$file" -Force
    }
}

Write-Host "`nSync operation completed successfully!" -ForegroundColor Green
