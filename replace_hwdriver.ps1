$file = 'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hwdriver.c'
$content = [System.IO.File]::ReadAllText($file)

# Check line endings
if ($content.Contains("`r`n")) {
    Write-Host "File uses CRLF line endings"
    $nl = "`r`n"
} else {
    Write-Host "File uses LF line endings"
    $nl = "`n"
}

# Replacement 1: Add hameg_hmo extern declaration after lecroy_xstream extern
$old1 = "#ifdef HAVE_DRIVER_LECROY_XSTREAM${nl}extern SR_PRIV struct sr_dev_driver lecroy_xstream_driver_info;${nl}#endif${nl}#ifdef HAVE_DRIVER_UNI_T_UT181A${nl}extern SR_PRIV struct sr_dev_driver uni_t_ut181a_driver_info;${nl}#endif"
$new1 = "#ifdef HAVE_DRIVER_LECROY_XSTREAM${nl}extern SR_PRIV struct sr_dev_driver lecroy_xstream_driver_info;${nl}#endif${nl}#ifdef HAVE_DRIVER_HAMEG_HMO${nl}extern SR_PRIV struct sr_dev_driver hameg_hmo_driver_info;${nl}#endif${nl}#ifdef HAVE_DRIVER_UNI_T_UT181A${nl}extern SR_PRIV struct sr_dev_driver uni_t_ut181a_driver_info;${nl}#endif"

if ($content.Contains($old1)) {
    $content = $content.Replace($old1, $new1)
    Write-Host "Replacement 1 (extern declaration): SUCCESS"
} else {
    Write-Host "Replacement 1 (extern declaration): NOT FOUND"
}

# Replacement 2: Add hameg_hmo to drivers_list after lecroy_xstream
$old2 = "#ifdef HAVE_DRIVER_LECROY_XSTREAM${nl}    &lecroy_xstream_driver_info,${nl}#endif${nl}#ifdef HAVE_DRIVER_YOKOGAWA_DLM${nl}    &yokogawa_dlm_driver_info,${nl}#endif"
$new2 = "#ifdef HAVE_DRIVER_LECROY_XSTREAM${nl}    &lecroy_xstream_driver_info,${nl}#endif${nl}#ifdef HAVE_DRIVER_HAMEG_HMO${nl}    &hameg_hmo_driver_info,${nl}#endif${nl}#ifdef HAVE_DRIVER_YOKOGAWA_DLM${nl}    &yokogawa_dlm_driver_info,${nl}#endif"

if ($content.Contains($old2)) {
    $content = $content.Replace($old2, $new2)
    Write-Host "Replacement 2 (drivers_list entry): SUCCESS"
} else {
    Write-Host "Replacement 2 (drivers_list entry): NOT FOUND"
}

[System.IO.File]::WriteAllText($file, $content)
Write-Host "File saved."
