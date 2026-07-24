# =============================================================================
# sync_to_opensource.ps1
# 同步开发仓库 view_and_data 的最新修改到开源仓库 opensource 分支
# 不在线拉取 submodule:checkout 只更新 gitlink SHA1,不触发网络
# =============================================================================
# 使用方式:
#   cd c:\Users\admin\Downloads\Downloads\DSView-main_2026_4_27cppnb
#   .\sync_to_opensource.ps1           # 同步 + commit
#   .\sync_to_opensource.ps1 -Push     # 同步 + commit + push
# =============================================================================

param(
    [switch]$Push
)

$ErrorActionPreference = "Stop"
$repoRoot = "c:\Users\admin\Downloads\Downloads\DSView-main_2026_4_27cppnb"
$worktreePath = "c:\Users\admin\Downloads\Downloads\PXView-sync"
# worktree 自动发现坏了(git 2.55 路径解析 bug),用显式 gitdir
$gitdir = "$repoRoot/.git/worktrees/PXView-sync"

# 白名单(与 migrate_to_worktree.ps1 保持一致)
$whitelistDirs = @(
    "CMake", "PXView", "common", "debian", "doc", "lang", "web", ".github",
    "libsigrok", "libsigrokdecode", "libusb",
    "sigrok-firmware", "sigrok-firmware-fx2lafw", "sigrok-util"
)
$whitelistFiles = @(
    "CMakeLists.txt", "COPYING", "INSTALL.md", "INSTALL_zh.md", "LICENSE",
    "PXView.icns", "README.md", "applogo.rc", "logo-win.ico",
    "mac_appbundle_template.plist.in",
    ".gitignore", ".gitmodules"
)

Set-Location $repoRoot

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " 同步开发仓库 → 开源仓库" -ForegroundColor Cyan
Write-Host " 源:   view_and_data ($repoRoot)"
Write-Host " 目标: opensource    ($worktreePath)"
Write-Host "================================================" -ForegroundColor Cyan

# 步骤 1:在主仓库 fetch 最新 view_and_data(本地操作,不需要网络)
Write-Host ""
Write-Host " [1/4] 确认 view_and_data 最新 commit..." -ForegroundColor Yellow
$devHead = git rev-parse view_and_data 2>&1
$devSubject = git log view_and_data --oneline -1 2>&1
Write-Host "   $devSubject" -ForegroundColor DarkGray

# 步骤 2:白名单 checkout 到 worktree
Write-Host ""
Write-Host " [2/4] 白名单 checkout 到 worktree..." -ForegroundColor Yellow

foreach ($d in $whitelistDirs) {
    Write-Host "   git checkout view_and_data -- $d" -ForegroundColor DarkGray
    git --git-dir="$gitdir" --work-tree="$worktreePath" checkout view_and_data -- $d 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   [警告] $d 检出失败(可能 view_and_data 没有此路径)" -ForegroundColor Red
    }
}

foreach ($f in $whitelistFiles) {
    git --git-dir="$gitdir" --work-tree="$worktreePath" checkout view_and_data -- $f 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   [警告] $f 检出失败" -ForegroundColor Red
    }
}
Write-Host "   [OK] 白名单文件已同步" -ForegroundColor Green

# 步骤 3:检查是否有变更
Write-Host ""
Write-Host " [3/4] 检查变更..." -ForegroundColor Yellow
$st = git --git-dir="$gitdir" --work-tree="$worktreePath" status --short 2>&1
$changes = $st | Where-Object { $_ -match "^[MADRC]" }
if ($changes.Count -eq 0) {
    Write-Host "   [无变更] worktree 与 view_and_data 白名单内容一致,无需 commit" -ForegroundColor Green
    if (-not $Push) {
        exit 0
    }
} else {
    Write-Host "   变更数: $($changes.Count)" -ForegroundColor DarkGray
    $changes | Select-Object -First 10 | ForEach-Object { Write-Host "   $_" -ForegroundColor DarkGray }

    # git add -A 暂存所有变更(含 submodule gitlink 更新)
    git --git-dir="$gitdir" --work-tree="$worktreePath" add -A 2>&1 | Out-Null

    # commit
    $commitMsg = "sync: 同步开发仓库最新修改 ($devSubject)"
    git --git-dir="$gitdir" --work-tree="$worktreePath" commit -m $commitMsg 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $newHead = git --git-dir="$gitdir" --work-tree="$worktreePath" log --oneline -1 2>&1
        Write-Host "   [OK] 已 commit: $newHead" -ForegroundColor Green
    } else {
        Write-Host "   [警告] commit 失败" -ForegroundColor Red
    }
}

# 步骤 4:push(可选)
if ($Push) {
    Write-Host ""
    Write-Host " [4/4] push 到 opensource:main..." -ForegroundColor Yellow
    git --git-dir="$gitdir" --work-tree="$worktreePath" push opensource opensource:main 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   [OK] push 成功" -ForegroundColor Green
    } else {
        Write-Host "   [错误] push 失败" -ForegroundColor Red
        Write-Host "   常见原因:" -ForegroundColor Yellow
        Write-Host "     1. 缺 workflow scope → 运行: gh auth refresh -h github.com -s workflow" -ForegroundColor Yellow
        Write-Host "     2. 鉴权失败 → 确认 gh auth status 正常" -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host " [4/4] 跳过 push(用 -Push 参数启用)" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host " 完成" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host " 手动 push:" -ForegroundColor White
Write-Host "   .\sync_to_opensource.ps1 -Push" -ForegroundColor DarkGray
Write-Host " 或只 push 不 sync:" -ForegroundColor White
Write-Host "   git --git-dir=`"$gitdir`" --work-tree=`"$worktreePath`" push opensource opensource:main" -ForegroundColor DarkGray
