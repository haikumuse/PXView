$drivers = @('rigol-ds','siglent-sds','hantek-dso','hantek-6xxx','hantek-4032l','lecroy-xstream','yokogawa-dlm','gwinstek-gds-800','hung-chang-dso-2100','link-mso19','uni-t-ut181a','rohde-schwarz-sme-0x','hameg-hmo','rigol-dg')
$outDir = "c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\_diffs"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
foreach ($drv in $drivers) {
    $oldPath = "C:\Users\admin\Downloads\old\libsigrok\src\hardware\$drv"
    $newPath = "c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\$drv"
    $files = Get-ChildItem $oldPath -File | Select-Object -ExpandProperty Name
    foreach ($f in $files) {
        $oldFile = Join-Path $oldPath $f
        $newFile = Join-Path $newPath $f
        if (Test-Path $newFile) {
            $outFile = Join-Path $outDir ($drv + '__' + ($f -replace '[\\/]','_') + '.diff')
            & git diff --no-index --no-color $oldFile $newFile 2>&1 | Out-File -FilePath $outFile -Encoding utf8
            $len = (Get-Item $outFile).Length
            Write-Host "Diffed: $drv/$f -> $len bytes"
        } else {
            Write-Host "MISSING NEW: $drv/$f"
        }
    }
}
