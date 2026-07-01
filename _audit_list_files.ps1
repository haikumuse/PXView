$drivers = @('fx2lafw','saleae-logic16','saleae-logic-pro','raspberrypi-pico','asix-sigma','chronovu-la','ftdi-la','kingst-la2016')
foreach ($d in $drivers) {
    Write-Host "=== OLD: $d ==="
    $oldPath = "C:\Users\admin\Downloads\old\libsigrok\src\hardware\$d"
    if (Test-Path $oldPath) {
        Get-ChildItem -Path $oldPath -File | ForEach-Object { Write-Host ("  {0} ({1} bytes)" -f $_.Name, $_.Length) }
    } else {
        Write-Host "  PATH NOT FOUND"
    }
    Write-Host "=== NEW: $d ==="
    $newPath = "c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\$d"
    if (Test-Path $newPath) {
        Get-ChildItem -Path $newPath -File | ForEach-Object { Write-Host ("  {0} ({1} bytes)" -f $_.Name, $_.Length) }
    } else {
        Write-Host "  PATH NOT FOUND"
    }
}
