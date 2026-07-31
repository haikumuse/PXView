<#
.SYNOPSIS
    PXView 版本号一键修改脚本
.DESCRIPTION
    一键修改所有版本号相关文件，包括 CMakeLists.txt、window_nisi.nsi、README.md、
    更新说明.txt、doc/NEWS25、doc/NEWS31。
    用法: .\bump_version.ps1 <新版本号> [旧版本号]
    示例: .\bump_version.ps1 1.5.4           (自动检测旧版本号)
    示例: .\bump_version.ps1 1.5.4 1.5.3     (指定旧版本号)
.PARAMETER NewVersion
    新版本号，如 1.5.4
.PARAMETER OldVersion
    旧版本号，如 1.5.3。不提供则自动从 CMakeLists.txt 读取。
#>
param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$NewVersion,
    [Parameter(Position=1)]
    [string]$OldVersion
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# ── 自动检测旧版本号 ──
if (-not $OldVersion) {
    $cmakeFile = Join-Path $repoRoot "CMakeLists.txt"
    $cmakeContent = Get-Content $cmakeFile -Raw
    if ($cmakeContent -match 'set\(DS_VERSION_MAJOR\s+(\d+)\)\s*\n\s*set\(DS_VERSION_MINOR\s+(\d+)\)\s*\n\s*set\(DS_VERSION_MICRO\s+(\d+)\)') {
        $OldVersion = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
    } else {
        Write-Error "无法从 CMakeLists.txt 自动检测旧版本号，请手动指定: .\bump_version.ps1 $NewVersion <旧版本号>"
        exit 1
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  PXView 版本号修改" -ForegroundColor Cyan
Write-Host "  $OldVersion  ->  $NewVersion" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$files = @(
    "CMakeLists.txt",
    "window_nisi.nsi",
    "README.md",
    "更新说明.txt",
    "doc\NEWS25",
    "doc\NEWS31"
)

# ── 特殊处理: CMakeLists.txt 只改 DS_VERSION_MICRO ──
$cmakeFile = Join-Path $repoRoot "CMakeLists.txt"
$cmakeContent = Get-Content $cmakeFile -Raw
$parts = $OldVersion -split '\.'
$newParts = $NewVersion -split '\.'

if ($parts.Count -eq 3 -and $newParts.Count -eq 3) {
    # 逐个替换 MAJOR、MINOR、MICRO
    $cmakeContent = $cmakeContent -replace 'set\(DS_VERSION_MAJOR\s+\d+\)', "set(DS_VERSION_MAJOR $($newParts[0]))"
    $cmakeContent = $cmakeContent -replace 'set\(DS_VERSION_MINOR\s+\d+\)', "set(DS_VERSION_MINOR $($newParts[1]))"
    $cmakeContent = $cmakeContent -replace 'set\(DS_VERSION_MICRO\s+\d+\)', "set(DS_VERSION_MICRO $($newParts[2]))"
    Set-Content -Path $cmakeFile -Value $cmakeContent -NoNewline
    Write-Host "[OK] CMakeLists.txt           DS_VERSION_$($newParts[0]).$($newParts[1]).$($newParts[2])" -ForegroundColor Green
} else {
    # fallback: 直接替换版本字符串
    $cmakeContent = $cmakeContent -replace [regex]::Escape($OldVersion), $NewVersion
    Set-Content -Path $cmakeFile -Value $cmakeContent -NoNewline
    Write-Host "[OK] CMakeLists.txt           $OldVersion -> $NewVersion" -ForegroundColor Green
}

# ── 特殊处理: 更新说明.txt 和 doc/NEWS 文件 ──
# 这些文件需要同时替换 V1.5.3 和 1.5.3 两种格式
$changelogFiles = @("更新说明.txt", "doc\NEWS25", "doc\NEWS31")
foreach ($f in $changelogFiles) {
    $filePath = Join-Path $repoRoot $f
    if (Test-Path $filePath) {
        $content = Get-Content $filePath -Raw -Encoding UTF8
        # 替换 PXView V1.5.3 -> PXView V1.5.4
        $content = $content -replace "V$([regex]::Escape($OldVersion))", "V$NewVersion"
        # 替换 standalone 1.5.3 -> 1.5.4 (在 changelog 中通常不需要替换独立的版本号引用，
        # 因为历史记录应该保持不变。只替换最新的那个版本号。)
        # 实际上 changelog 中每个版本号都是历史记录，不应批量替换。
        # 但如果用户想在顶部添加新版本的占位符，那是另一个功能。
        # 这里只替换 "PXView V旧版本" 为 "PXView V新版本"
        Set-Content -Path $filePath -Value $content -NoNewline -Encoding UTF8
        Write-Host "[OK] $f".PadRight(28) + "V$OldVersion -> V$NewVersion" -ForegroundColor Green
    }
}

# ── 处理 window_nisi.nsi ──
$nsiFile = Join-Path $repoRoot "window_nisi.nsi"
$nsiContent = Get-Content $nsiFile -Raw
$nsiContent = $nsiContent -replace "!define PRODUCT_VERSION `"$([regex]::Escape($OldVersion))`"", "!define PRODUCT_VERSION `"$NewVersion`""
Set-Content -Path $nsiFile -Value $nsiContent -NoNewline
Write-Host "[OK] window_nisi.nsi          PRODUCT_VERSION $OldVersion -> $NewVersion" -ForegroundColor Green

# ── 处理 README.md ──
$readmeFile = Join-Path $repoRoot "README.md"
$readmeContent = Get-Content $readmeFile -Raw
$readmeContent = $readmeContent -replace "version-$([regex]::Escape($OldVersion))-", "version-$NewVersion-"
Set-Content -Path $readmeFile -Value $readmeContent -NoNewline
Write-Host "[OK] README.md               badge version-$OldVersion -> version-$NewVersion" -ForegroundColor Green

# ── 检查 PXView-build build.yml 中的默认版本号 ──
$buildYml = Join-Path $repoRoot "PXView-build\.github\workflows\build.yml"
if (Test-Path $buildYml) {
    $ymlContent = Get-Content $buildYml -Raw
    $ymlContent = $ymlContent -replace "default: '$([regex]::Escape($OldVersion))'", "default: '$NewVersion'"
    Set-Content -Path $buildYml -Value $ymlContent -NoNewline
    Write-Host "[OK] build.yml               default version $OldVersion -> $NewVersion" -ForegroundColor Green
} else {
    Write-Host "[SKIP] build.yml             (PXView-build not found)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  完成! 版本号已从 $OldVersion 改为 $NewVersion" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "提示: 请检查 git diff 确认所有修改正确" -ForegroundColor Yellow
Write-Host ""
