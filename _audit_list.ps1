$drivers = @('rigol-ds','siglent-sds','hantek-dso','hantek-6xxx','hantek-4032l','lecroy-xstream','yokogawa-dlm','gwinstek-gds-800','hung-chang-dso-2100','link-mso19','uni-t-ut181a','rohde-schwarz-sme-0x','hameg-hmo','rigol-dg')
foreach ($drv in $drivers) {
    Write-Host "=== OLD $drv ==="
    $oldPath = "C:\Users\admin\Downloads\old\libsigrok\src\hardware\$drv"
    if (Test-Path $oldPath) {
        Get-ChildItem $oldPath -File | Select-Object Name, Length | Format-Table -AutoSize
    } else {
        Write-Host "MISSING"
    }
    Write-Host "=== NEW $drv ==="
    $newPath = "c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\$drv"
    if (Test-Path $newPath) {
        Get-ChildItem $newPath -File | Select-Object Name, Length | Format-Table -AutoSize
    } else {
        Write-Host "MISSING"
    }
}
