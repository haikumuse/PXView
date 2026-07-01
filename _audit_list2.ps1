$drivers = @('asix-sigma','chronovu-la','ftdi-la','kingst-la2016')
foreach ($d in $drivers) {
    Write-Host "=== $d ==="
    Write-Host "--- OLD ---"
    Get-ChildItem "C:\Users\admin\Downloads\old\libsigrok\src\hardware\$d" -File -ErrorAction SilentlyContinue | Select-Object Name,Length | Format-Table -AutoSize
    Write-Host "--- NEW ---"
    Get-ChildItem "c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\$d" -File -ErrorAction SilentlyContinue | Select-Object Name,Length | Format-Table -AutoSize
}
