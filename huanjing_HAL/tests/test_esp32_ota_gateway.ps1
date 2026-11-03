$ErrorActionPreference = 'Stop'
$gatewayRoot = Join-Path $PSScriptRoot '..\..\portable_ota\esp32c3_arduino_gateway\esp32c3_ota_gateway'
$sketch = Get-Content -Raw (Join-Path $gatewayRoot 'esp32c3_ota_gateway.ino')
$link = Get-Content -Raw (Join-Path $gatewayRoot 'stm32_ota_link.cpp')

foreach ($token in @('ENV,', 'BOOT_OK,', 'STATUS,%u,%u,%u,%u,%u,%s',
                      'OTA_BEGIN,', 'OTA_END', 'OTA_ABORT',
                      'OTA_READY', 'OTA_ACK,', 'OTA_OK')) {
    if (($sketch + $link) -notmatch [regex]::Escape($token)) {
        throw "ESP32/STM32 protocol token is missing: $token"
    }
}

foreach ($helper in @('processStm32Lines', 'processPendingOta', 'sendPeriodicStatus')) {
    if ($sketch -notmatch "void\s+$helper\s*\(") {
        throw "Gateway loop helper is missing: $helper"
    }
}

$loopMatch = [regex]::Match($sketch, '(?s)void\s+loop\s*\(\s*\)\s*\{(?<body>.*?)\n\}')
if (-not $loopMatch.Success) {
    throw 'Cannot locate the Arduino loop function.'
}
foreach ($call in @('huawei.update()', 'processStm32Lines()',
                     'processPendingOta()', 'sendPeriodicStatus()')) {
    if ($loopMatch.Groups['body'].Value -notmatch [regex]::Escape($call)) {
        throw "Arduino loop is missing orchestration call: $call"
    }
}

Write-Host 'ESP32 OTA gateway structure and protocol check passed.'
