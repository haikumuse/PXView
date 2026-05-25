$files = @('ieee488_c.c','miller_c.c','usb_signalling_c.c','i2c_c.c','timing_c.c','morse_c.c','sent_c.c','can_c.c','z80_c.c','opentherm_c.c','swim_c.c','ps2_c.c','graycode_c.c','swi_c.c','microwire_c.c')
$dir = 'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrokdecode\c_decoders'
foreach ($f in $files) {
    $p = Join-Path $dir $f
    if (Test-Path $p) {
        (Get-Item $p).LastWriteTime = Get-Date
        Write-Host "Updated: $f"
    }
}
