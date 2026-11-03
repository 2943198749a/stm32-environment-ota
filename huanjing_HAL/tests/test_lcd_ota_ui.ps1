# LCD environment dashboard and system-status page regression check.
$header = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\app_ui.h"
$source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\app_ui.c"

if (($header -match 'Esp32C3|AppUiOtaStatus') -or ($source -match 'APP_USE_ESP_AT')) {
    throw 'LCD cannot retain the obsolete ESP-AT implementation.'
}

if ($header -notmatch 'AppUi_Render\s*\(\s*const\s+EnvData_t\s*\*\s*environment\s*,\s*const\s+Esp32Status_t\s*\*\s*esp32_status\s*\)') {
    throw 'AppUi_Render must receive environment and ESP32 status snapshots.'
}

foreach ($required in @('SYSTEM STATUS', 'WIFI', 'MQTT', 'VERSION:', 'AppUi_DrawProgressBar')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "LCD system page is missing: $required"
    }
}
if ($source -match 'OTA RESERVED|Esp32C3|APP_USE_ESP_AT') {
    throw 'LCD cannot retain the obsolete OTA placeholder or ESP-AT implementation.'
}

if ($source -notmatch 'static\s+void\s+AppUi_DrawFooter\s*\(') {
    throw 'UI footer must be drawn by one readable helper function.'
}

if ($source -notmatch 'typedef\s+enum[\s\S]*APP_UI_PAGE_ENVIRONMENT[\s\S]*APP_UI_PAGE_SYSTEM_STATUS[\s\S]*AppUiPage_t') {
    throw 'UI pages must use the readable AppUiPage_t enum.'
}
if ($source -notmatch 'static\s+void\s+AppUi_WriteValueOrNoData\s*\(') {
    throw 'Sensor cards must share one valid/no-data drawing helper.'
}

Write-Host 'LCD dashboard and system-status page check passed.'
