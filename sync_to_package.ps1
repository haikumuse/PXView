<#
.SYNOPSIS
    Package development directory into TWO compressed archives using Bandizip:
      1. Source archive  — everything EXCEPT the package/ folder
      2. Package archive — the package/ folder only (runtime assets)

.DESCRIPTION
    Splits packaging into two ZIP archives so the large runtime-asset bundle
    (package/) can be distributed independently from the source code:

      - PXView-src_<date>.zip     — source code (PXView, libsigrok,
                                    libsigrokdecode, common, etc.), build
                                    scripts, tests, firmware, libusb submodule.
                                    Does NOT include package/.
      - PXView-package_<date>.zip — runtime assets (package/ folder only):
                                    Python decoders, C decoder DLLs, Qt DLLs,
                                    docs, LICENSE.

    After extraction:
      - Source archive can: compile (build_full.cmd / build_incremental.cmd),
        install to install.dir/ (ninja install), run tests (ctest), build
        macOS .app bundle (mac_appbundle_template.plist.in).
      - Package archive provides the runtime assets needed to run the built
        binary alongside the installed executable.

.PARAMETER OutputPath
    Path for the source archive (everything EXCEPT package/).
    Default: PXView-src_<date>.zip in the script's directory.

.PARAMETER OutputPathPackage
    Path for the package archive (package/ folder only).
    Default: PXView-package_<date>.zip in the script's directory.

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
    [string]$OutputPathPackage = "",
    [string]$BandizipPath = "C:\Program Files\Bandizip\bz.exe",
    [int]$CompressionLevel = 5,
    [switch]$KeepStaging
)

$SourceDir = $PSScriptRoot

# --- Resolve output paths ---
$dateStr = Get-Date -Format "yyyy-MM-dd_HHmmss"
if ([string]::IsNullOrEmpty($OutputPath)) {
    $OutputPath = Join-Path $SourceDir "PXView-src_$dateStr.zip"
}
if ([string]::IsNullOrEmpty($OutputPathPackage)) {
    $OutputPathPackage = Join-Path $SourceDir "PXView-package_$dateStr.zip"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$OutputPathPackage = [System.IO.Path]::GetFullPath($OutputPathPackage)

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

# --- Verify package/ folder exists (it is its own archive) ---
$PackageDir = Join-Path $SourceDir "package"
if (-Not (Test-Path $PackageDir)) {
    Write-Host "ERROR: package/ folder not found at '$PackageDir'" -ForegroundColor Red
    exit 1
}

# === Folders (source archive — package/ excluded, packaged separately) ===
$FoldersToCopy = @(
    # --- Base: compile-capable (same as sync_to_repo.ps1) ---
    "PXView", "libsigrok", "libsigrokdecode", "common", "lang", "CMake",
    "debian", "web", "doc",
    "libusb",                  # libusb submodule (Windows event-abstraction-v4)
    "sigrok-firmware",         # asix-sigma + sysclk-lwla firmware
    "sigrok-firmware-fx2lafw", # fx2lafw firmware (prebuilt or build via build_fx2lafw.sh)
    # --- Packaging-only: build + test ---
    "tests"                    # test suite (CMakeLists.txt + MCP JSON test cases)
    # NOTE: package/ is packaged separately into $OutputPathPackage
)

# === Files (source archive) =============================================
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
Write-Host " Packaging to TWO ZIP Archives via Bandizip" -ForegroundColor Cyan
Write-Host " Source:          $SourceDir"
Write-Host " Source archive:  $OutputPath  (excludes package/)"
Write-Host " Package archive: $OutputPathPackage  (package/ only)"
Write-Host " Bandizip:        $BandizipPath"
Write-Host " Level:           $CompressionLevel (0=store, 5=default, 9=max)"
Write-Host "==========================================" -ForegroundColor Cyan

# --- Helper: compress with Bandizip ---
function Invoke-BandizipCompress {
    param(
        [string]$ArchivePath,
        [string]$SourceToCompress,
        [int]$Level,
        [string]$BzPath
    )
    if (Test-Path $ArchivePath) {
        Write-Host "  Removing existing archive: $ArchivePath" -ForegroundColor Yellow
        Remove-Item -Path $ArchivePath -Force
    }
    $bzArgs = @(
        "c",                    # Create new archive
        "-l:$Level",            # Compression level
        "-fmt:zip",             # ZIP format
        "-y",                   # Assume Yes on all queries
        "-r",                   # Recurse subdirectories
        $ArchivePath,           # Output archive path
        $SourceToCompress       # Source to compress
    )
    # Out-Host: display stdout but keep it out of the function's return value,
    # so the function returns ONLY the numeric $LASTEXITCODE (not a stdout array).
    & $BzPath @bzArgs | Out-Host
    return $LASTEXITCODE
}

# --- Create staging directory (for source archive) ---
$stagingDir = Join-Path $env:TEMP "pxview_staging_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
Write-Host "`nStaging directory (source): $stagingDir" -ForegroundColor DarkGray
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

try {
    # --- Stage folders (excluding package/) ---
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

    # ============================================================
    # Archive 1: Source (everything EXCEPT package/)
    # ============================================================
    Write-Host "`n==========================================" -ForegroundColor Cyan
    Write-Host " [1/2] Compressing source archive (excludes package/)" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  Compressing with Bandizip (level $CompressionLevel)..." -ForegroundColor Cyan

    $exitCode = Invoke-BandizipCompress -ArchivePath $OutputPath `
        -SourceToCompress "$stagingDir\*" -Level $CompressionLevel -BzPath $BandizipPath

    if ($exitCode -ne 0) {
        Write-Host "`n  ERROR: Bandizip failed on source archive with exit code $exitCode" -ForegroundColor Red
        exit $exitCode
    }

    if (Test-Path $OutputPath) {
        $srcArchiveSizeMB = [math]::Round((Get-Item $OutputPath).Length / 1MB, 1)
        Write-Host "  Source archive created: $OutputPath ($srcArchiveSizeMB MB)" -ForegroundColor Green
    } else {
        Write-Host "`n  ERROR: Source archive was not created: $OutputPath" -ForegroundColor Red
        exit 1
    }

    # ============================================================
    # Archive 2: Package (package/ folder only, no staging needed)
    # ============================================================
    Write-Host "`n==========================================" -ForegroundColor Cyan
    Write-Host " [2/2] Compressing package archive (package/ only)" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  Compressing with Bandizip (level $CompressionLevel)..." -ForegroundColor Cyan

    $exitCode = Invoke-BandizipCompress -ArchivePath $OutputPathPackage `
        -SourceToCompress $PackageDir -Level $CompressionLevel -BzPath $BandizipPath

    if ($exitCode -ne 0) {
        Write-Host "`n  ERROR: Bandizip failed on package archive with exit code $exitCode" -ForegroundColor Red
        exit $exitCode
    }

    if (Test-Path $OutputPathPackage) {
        $pkgArchiveSizeMB = [math]::Round((Get-Item $OutputPathPackage).Length / 1MB, 1)
        Write-Host "  Package archive created: $OutputPathPackage ($pkgArchiveSizeMB MB)" -ForegroundColor Green
    } else {
        Write-Host "`n  ERROR: Package archive was not created: $OutputPathPackage" -ForegroundColor Red
        exit 1
    }

    # ============================================================
    # Summary
    # ============================================================
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host " Packaging Summary" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  [1] Source archive  (excludes package/)"
    Write-Host "      $OutputPath"
    Write-Host "      Size: $srcArchiveSizeMB MB"
    Write-Host "      Folders staged: $copiedFolders  (skipped: $skippedFolders)"
    Write-Host "      Files staged:   $copiedFiles  (skipped: $skippedFiles)"
    Write-Host ""
    Write-Host "  [2] Package archive (package/ only)"
    Write-Host "      $OutputPathPackage"
    Write-Host "      Size: $pkgArchiveSizeMB MB"
    Write-Host ""
    Write-Host " Capabilities (source archive):" -ForegroundColor Yellow
    Write-Host "  [x] Compile from source     (build_full.cmd / build_incremental.cmd)"
    Write-Host "  [x] Install to install.dir/  (ninja install)"
    Write-Host "  [x] Run tests                (cd build && ctest)"
    Write-Host "  [x] macOS .app packaging     (mac_appbundle_template.plist.in)"
    Write-Host ""
    Write-Host " Capabilities (package archive):" -ForegroundColor Yellow
    Write-Host "  [x] Runtime assets           (Python decoders, C decoder DLLs, Qt DLLs)"
    Write-Host "  [x] Documentation            (PXLogic docs, c-decoder guides)"
    Write-Host ""
    Write-Host " Packaging completed successfully!" -ForegroundColor Green

} finally {
    # --- Clean up staging directory ---
    if (-Not $KeepStaging) {
        Write-Host "`n  Cleaning up staging directory..." -ForegroundColor DarkGray
        Remove-Item -Path $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "`n  Staging directory kept: $stagingDir" -ForegroundColor Yellow
    }
}
