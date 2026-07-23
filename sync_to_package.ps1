<#
.SYNOPSIS
    Package development directory into a compressed archive using Bandizip.
    Includes everything from sync_to_repo.ps1 PLUS build scripts, tests,
    package/ runtime assets, and macOS bundle template — giving the
    target archive the ability to produce a runnable distribution package.

.DESCRIPTION
    Instead of syncing to a folder (like sync_to_repo.ps1), this script
    stages all selected files/folders into a temp directory, then compresses
    it into a single .zip archive using Bandizip (bz.exe).

    The archive contains:
      - All source code (PXView, libsigrok, libsigrokdecode, common, etc.)
      - Build scripts (build_full.cmd, build_incremental.cmd/.sh, build_fx2lafw.sh)
      - Tests (tests/)
      - Runtime assets (package/)
      - macOS bundle template (mac_appbundle_template.plist.in)
      - Git submodule config (.gitmodules)
      - Firmware files (sigrok-firmware/, sigrok-firmware-fx2lafw/)
      - libusb submodule (libusb/)

    After extraction, the target repo can:
      1. Compile from source       → build_full.cmd  (or build_incremental.cmd)
      2. Install to install.dir/   → ninja install
      3. Run tests                 → cd build && ctest
      4. Produce macOS .app bundle → cmake --build . --target package

.PARAMETER OutputPath
    Path for the output .zip archive.
    Default: PXView-backup_<date>.zip in the script's directory.

.PARAMETER BandizipPath
    Path to Bandizip console tool (bz.exe).
    Default: C:\Program Files\Bandizip\bz.exe

.PARAMETER CompressionLevel
    Bandizip compression level 0-9 (0=store, 5=default, 9=maximum).
    Default: 5

.PARAMETER KeepStaging
    Keep the temp staging directory after compression (for debugging).
    Default: $false (cleaned up automatically)
#>

param (
    [string]$OutputPath = "",
    [string]$BandizipPath = "C:\Program Files\Bandizip\bz.exe",
    [int]$CompressionLevel = 5,
    [switch]$KeepStaging
)

$SourceDir = $PSScriptRoot

# --- Resolve output path ---
if ([string]::IsNullOrEmpty($OutputPath)) {
    $dateStr = Get-Date -Format "yyyy-MM-dd_HHmmss"
    $OutputPath = Join-Path $SourceDir "PXView-backup_$dateStr.zip"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

# --- Verify Bandizip ---
if (-Not (Test-Path $BandizipPath)) {
    # Try x86 path as fallback
    $x86Path = "C:\Program Files (x86)\Bandizip\bz.exe"
    if (Test-Path $x86Path) {
        $BandizipPath = $x86Path
    } else {
        Write-Host "ERROR: Bandizip (bz.exe) not found at '$BandizipPath'" -ForegroundColor Red
        Write-Host "Please install Bandizip or specify -BandizipPath" -ForegroundColor Red
        exit 1
    }
}

# === Folders ============================================================
$FoldersToCopy = @(
    # --- Base: compile-capable (same as sync_to_repo.ps1) ---
    "PXView", "libsigrok", "libsigrokdecode", "common", "lang", "CMake",
    "debian", "web", "doc",
    "libusb",                  # libusb submodule (Windows event-abstraction-v4)
    "sigrok-firmware",         # asix-sigma + sysclk-lwla firmware
    "sigrok-firmware-fx2lafw", # fx2lafw firmware (prebuilt or build via build_fx2lafw.sh)
    # --- Packaging-only: build + test + runtime assets ---
    "tests",                   # test suite (CMakeLists.txt + MCP JSON test cases)
    "package"                  # pre-built runtime: Python decoders, C decoder DLLs, Qt DLLs
)

# === Files ==============================================================
$FilesToCopy = @(
    # --- Base: compile-capable (same as sync_to_repo.ps1) ---
    "CMakeLists.txt", "logo-win.ico", "applogo.rc", "README.md",
    "INSTALL.md", "INSTALL_zh.md", "COPYING", "LICENSE",
    "PXView.icns", "clean", "cmake_clear", "build_linux",
    ".gitmodules",              # git submodule definitions
    "$([char]0x66F4)$([char]0x65B0)$([char]0x8BF4)$([char]0x660E).txt",
    # --- Packaging-only: build scripts + macOS template ---
    "build_incremental.cmd",   # Windows incremental build launcher
    "build_incremental.sh",    # MinGW64 incremental build script
    "build_full.cmd",          # Windows full clean build (cmake + ninja + install)
    "build_fx2lafw.sh",        # fx2lafw firmware builder (requires sdcc)
    "mac_appbundle_template.plist.in"  # macOS .app bundle Info.plist template
)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Packaging to ZIP Archive via Bandizip" -ForegroundColor Cyan
Write-Host " Source:  $SourceDir"
Write-Host " Output:  $OutputPath"
Write-Host " Bandizip: $BandizipPath"
Write-Host " Level:   $CompressionLevel (0=store, 5=default, 9=max)"
Write-Host "==========================================" -ForegroundColor Cyan

# --- Create staging directory ---
$stagingDir = Join-Path $env:TEMP "pxview_staging_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
Write-Host "`nStaging directory: $stagingDir" -ForegroundColor DarkGray
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

try {
    # --- Stage folders ---
    $copiedFolders = 0
    $skippedFolders = 0
    foreach ($folder in $FoldersToCopy) {
        $srcPath = Join-Path $SourceDir $folder
        if (Test-Path $srcPath) {
            Write-Host "  Staging folder: $folder" -ForegroundColor Green
            Copy-Item -Path $srcPath -Destination $stagingDir -Recurse -Force
            $copiedFolders++
        } else {
            Write-Host "  [SKIP] Folder not found: $folder" -ForegroundColor DarkGray
            $skippedFolders++
        }
    }

    # --- Stage files ---
    $copiedFiles = 0
    $skippedFiles = 0
    foreach ($file in $FilesToCopy) {
        $srcPath = Join-Path $SourceDir $file
        if (Test-Path $srcPath) {
            Write-Host "  Staging file:   $file" -ForegroundColor Green
            Copy-Item -Path $srcPath -Destination (Join-Path $stagingDir $file) -Force
            $copiedFiles++
        } else {
            Write-Host "  [SKIP] File not found:   $file" -ForegroundColor DarkGray
            $skippedFiles++
        }
    }

    Write-Host ""
    Write-Host "  Folders staged: $copiedFolders  (skipped: $skippedFolders)"
    Write-Host "  Files staged:   $copiedFiles  (skipped: $skippedFiles)"

    # --- Delete existing archive if present ---
    if (Test-Path $OutputPath) {
        Write-Host "`n  Removing existing archive: $OutputPath" -ForegroundColor Yellow
        Remove-Item -Path $OutputPath -Force
    }

    # --- Compress with Bandizip ---
    # bz c -l:<level> -y -fmt:zip <archive> <files...>
    # Using "c" (create new) with -fmt:zip, -y (assume yes), -r (recurse)
    Write-Host "`n  Compressing with Bandizip (level $CompressionLevel)..." -ForegroundColor Cyan

    $args = @(
        "c",                    # Create new archive
        "-l:$CompressionLevel", # Compression level
        "-fmt:zip",             # ZIP format
        "-y",                   # Assume Yes on all queries
        "-r",                   # Recurse subdirectories
        $OutputPath,            # Output archive path
        "$stagingDir\*"         # All staged content
    )

    & $BandizipPath @args
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        Write-Host "`n  ERROR: Bandizip failed with exit code $exitCode" -ForegroundColor Red
        exit $exitCode
    }

    # --- Verify archive ---
    if (Test-Path $OutputPath) {
        $archiveSize = (Get-Item $OutputPath).Length
        $archiveSizeMB = [math]::Round($archiveSize / 1MB, 1)
        Write-Host ""
        Write-Host "==========================================" -ForegroundColor Cyan
        Write-Host " Package Summary" -ForegroundColor Cyan
        Write-Host "==========================================" -ForegroundColor Cyan
        Write-Host "  Archive:       $OutputPath"
        Write-Host "  Size:          $archiveSizeMB MB"
        Write-Host "  Folders staged: $copiedFolders  (skipped: $skippedFolders)"
        Write-Host "  Files staged:   $copiedFiles  (skipped: $skippedFiles)"
        Write-Host ""
        Write-Host " Archive capabilities:" -ForegroundColor Yellow
        Write-Host "  [x] Compile from source     (build_full.cmd / build_incremental.cmd)"
        Write-Host "  [x] Install to install.dir/  (ninja install)"
        Write-Host "  [x] Run tests                (cd build && ctest)"
        Write-Host "  [x] macOS .app packaging     (mac_appbundle_template.plist.in)"
        Write-Host "  [x] Runtime assets included  (package/)"
        Write-Host ""
        Write-Host " Packaging completed successfully!" -ForegroundColor Green
    } else {
        Write-Host "`n  ERROR: Archive was not created: $OutputPath" -ForegroundColor Red
        exit 1
    }

} finally {
    # --- Clean up staging directory ---
    if (-Not $KeepStaging) {
        Write-Host "`n  Cleaning up staging directory..." -ForegroundColor DarkGray
        Remove-Item -Path $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "`n  Staging directory kept: $stagingDir" -ForegroundColor Yellow
    }
}
