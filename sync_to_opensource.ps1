# =============================================================================
# sync_to_opensource.ps1
# 同步开发仓库 view_and_data 的最新修改到开源仓库 opensource 分支
# =============================================================================

param(
    [switch]$Commit,
    [switch]$Push
)

$ErrorActionPreference = "Stop"
$repoRoot = "c:\Users\admin\Downloads\Downloads\DSView-main_2026_4_27cppnb"
$worktreePath = "c:\Users\admin\Downloads\Downloads\PXView-sync"
$gitdir = "$repoRoot/.git/worktrees/PXView-sync"

$whitelistDirs = @(
    "CMake", "PXView", "common", "debian", "doc", "lang", "tools", "web",
    "libsigrok", "libsigrokdecode", "libusb",
    "sigrok-firmware", "sigrok-firmware-fx2lafw", "sigrok-util"
)
$whitelistFiles = @(
    "CMakeLists.txt", "COPYING", "INSTALL.md", "INSTALL_zh.md", "LICENSE",
    "PXView.icns", "README.md", "applogo.rc", "logo-win.ico",
    "mac_appbundle_template.plist.in",
    ".gitignore", ".gitmodules",
    "windows.md",
    "build_fx2lafw.sh",
    "build_linux.sh",
    "build_macos.sh",
    "build_windows.sh",
    "window/package.sh",
    "window/copy-deps.sh",
    "window_nisi.nsi",
    ".github/workflows/build.yml"
)

# 所有子模块路径
$submoduleDirs = @(
    "sigrok-firmware", "sigrok-firmware-fx2lafw", "sigrok-util",
    "libsigrok", "libsigrokdecode", "libusb"
)

Set-Location $repoRoot

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " 同步开发仓库 -> 开源仓库" -ForegroundColor Cyan
Write-Host " 源:   view_and_data ($repoRoot)"
Write-Host " 目标: opensource    ($worktreePath)"
Write-Host "================================================" -ForegroundColor Cyan

# 步骤 1: 确认 view_and_data 最新 commit
Write-Host ""
Write-Host " [1/4] 确认 view_and_data 最新 commit..." -ForegroundColor Yellow
$devHead = git rev-parse view_and_data 2>&1
$devSubject = git log view_and_data --oneline -1 2>&1
Write-Host "   $devSubject" -ForegroundColor DarkGray

# 步骤 2: 白名单 checkout 到 worktree
Write-Host ""
Write-Host " [2/4] 白名单 checkout 到 worktree..." -ForegroundColor Yellow

foreach ($d in $whitelistDirs) {
    Write-Host "   git checkout view_and_data -- $d" -ForegroundColor DarkGray
    git --git-dir="$gitdir" --work-tree="$worktreePath" checkout view_and_data -- $d 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   [警告] $d 检出失败" -ForegroundColor Red
    }
}

foreach ($f in $whitelistFiles) {
    git --git-dir="$gitdir" --work-tree="$worktreePath" checkout view_and_data -- $f 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   [警告] $f 检出失败" -ForegroundColor Red
    }
}
Write-Host "   [OK] 白名单文件已同步" -ForegroundColor Green

# 步骤 2.5: 同步子模块 HEAD 到 view_and_data 的 gitlink SHA
# 关键修复: worktree 中的子模块实际 HEAD 可能是旧版本(如 upstream libsigrok),
# 如果不更新,后续 git add -A 会把旧 SHA 写入 index,覆盖正确的 gitlink。
Write-Host ""
Write-Host "   同步子模块 HEAD..." -ForegroundColor DarkGray
foreach ($sm in $submoduleDirs) {
    $smPath = Join-Path $worktreePath $sm
    if (-not (Test-Path $smPath)) { continue }

    # 读取 view_and_data 分支中该子模块的 gitlink SHA
    $treeLine = git ls-tree view_and_data $sm 2>&1
    if ($treeLine -match "160000 commit ([0-9a-f]+)") {
        $targetSha = $Matches[1]
        $curSha = git -C $smPath rev-parse HEAD 2>&1
        if ($curSha -ne $targetSha) {
            Write-Host "   $sm : $curSha -> $targetSha" -ForegroundColor Yellow
            git -C $smPath checkout $targetSha 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) {
                Write-Host "   [fetch] $sm 远程拉取..." -ForegroundColor DarkGray
                git -C $smPath fetch origin $targetSha 2>&1 | Out-Null
                git -C $smPath checkout $targetSha 2>&1 | Out-Null
            }
        }
    }

    # 修复 index 文件缺失 (Git 2.55 worktree bug)
    $smSt = git -C $smPath status --short 2>&1
    if ($smSt) {
        $needFix = $false
        foreach ($line in $smSt) {
            if ($line -match "^D ") { $needFix = $true; break }
        }
        if ($needFix) {
            Write-Host "   修复子模块 index: $sm" -ForegroundColor DarkGray
            git -C $smPath checkout HEAD -- . 2>&1 | Out-Null
        }
    }
}

# 步骤 3: 检查是否有变更
Write-Host ""
Write-Host " [3/4] 检查变更..." -ForegroundColor Yellow
$st = git --git-dir="$gitdir" --work-tree="$worktreePath" status --short 2>&1
$changes = $st | Where-Object { $_ -match "^[MADRC]" }
if ($changes.Count -eq 0) {
    Write-Host "   [无变更] worktree 与 view_and_data 白名单内容一致" -ForegroundColor Green
} else {
    Write-Host "   变更数: $($changes.Count)" -ForegroundColor DarkGray
    $changes | Select-Object -First 10 | ForEach-Object { Write-Host "   $_" -ForegroundColor DarkGray }

    if ($Commit -or $Push) {
        # git add -A 暂存所有变更
        git --git-dir="$gitdir" --work-tree="$worktreePath" add -A 2>&1 | Out-Null

        # git add -A 后,再次用 update-index 强制写入正确的 gitlink
        # (双保险: 即使子模块 HEAD 已更新,仍确保 index 中的 SHA 与 view_and_data 一致)
        foreach ($sm in $submoduleDirs) {
            $treeLine = git ls-tree view_and_data $sm 2>&1
            if ($treeLine -match "160000 commit ([0-9a-f]+)") {
                $targetSha = $Matches[1]
                git --git-dir="$gitdir" --work-tree="$worktreePath" update-index --cacheinfo 160000 $targetSha $sm 2>&1 | Out-Null
            }
        }

        # commit
        $commitMsg = "sync: 同步开发仓库最新修改 ($devSubject)"
        git --git-dir="$gitdir" --work-tree="$worktreePath" commit -m $commitMsg 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $newHead = git --git-dir="$gitdir" --work-tree="$worktreePath" log --oneline -1 2>&1
            Write-Host "   [OK] 已 commit: $newHead" -ForegroundColor Green
        } else {
            Write-Host "   [警告] commit 失败" -ForegroundColor Red
        }
    } else {
        Write-Host "   [预览模式] 未传 -Commit,仅显示变更不 commit" -ForegroundColor Yellow
    }
}

# 步骤 4: push
if ($Push) {
    Write-Host ""
    Write-Host " [4/4] push 到 opensource:main..." -ForegroundColor Yellow
    git --git-dir="$gitdir" --work-tree="$worktreePath" push opensource opensource:main 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   [OK] push 成功" -ForegroundColor Green
    } else {
        Write-Host "   [错误] push 失败" -ForegroundColor Red
    }
} else {
    Write-Host ""
    Write-Host " [4/4] 跳过 push(用 -Push 参数启用)" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host " 完成" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Cyan
