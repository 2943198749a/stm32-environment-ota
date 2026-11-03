# Beginner four-task structure check.
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\freertos.c"

foreach ($task in @('sensortaskHandle', 'uitaskHandle', 'uarttaskHandle', 'otaTaskHandle')) {
    if ($source -notmatch [regex]::Escape($task)) {
        throw "Missing $task in the three-task structure."
    }
}

foreach ($required in @(
    'EnvSensor_Init\s*\(\s*\)',
    'EnvSensor_Update\s*\(\s*\)',
    'AppUi_Render\s*\(\s*&environment\s*,\s*&esp32_status\s*\)',
    'Esp32Uart_Init\s*\(\s*\)',
    'OtaUart_Process\s*\(\s*\)',
    'osDelay\s*\(\s*1\s*\)',
    'osDelay\s*\(\s*500\s*\)',
    'osDelay\s*\(\s*1000\s*\)')) {
    if ($source -notmatch $required) {
        throw "Missing required FreeRTOS call: $required"
    }
}

if ($source -match 'Esp32C3|APP_USE_ESP_AT|osMutex') {
    throw 'Beginner FreeRTOS tasks cannot retain old ESP-AT or mutex code.'
}

Write-Host 'Beginner FreeRTOS four-task check passed.'
