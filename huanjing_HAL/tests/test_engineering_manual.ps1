# Engineering manual coverage check.
$ErrorActionPreference = 'Stop'
$manualFile = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot '..') -File -Filter '*.md' |
    Where-Object {
        $text = Get-Content -Raw -Encoding UTF8 $_.FullName
        ($text -match 'DHT11') -and ($text -match 'BH1750') -and
        ($text -match 'MQ135') -and ($text -match 'USER CODE')
    } |
    Select-Object -First 1
if ($null -eq $manualFile) {
    throw 'Engineering manual is missing.'
}
$manual = Get-Content -Raw -Encoding UTF8 $manualFile.FullName
foreach ($section in @('sensortask', 'uitask', 'uarttask', 'otaTask',
                        'DHT11', 'BH1750', 'MQ135', 'ESP32 UART',
                        'CubeMX', 'USER CODE', 'SYSTEM STATUS', 'A/B OTA')) {
    if ($manual -notmatch [regex]::Escape($section)) {
        throw "Engineering manual is missing section: $section"
    }
}

foreach ($api in @('EnvSensor_Update', 'AppUi_Render', 'Esp32Uart_ReadByte',
                    'Esp32Uart_SendString', 'OtaUart_Process')) {
    if ($manual -notmatch $api) {
        throw "Engineering manual is missing API explanation: $api"
    }
}

Write-Host 'Engineering manual coverage check passed.'
