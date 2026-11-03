# ESP32 UART module check: Arduino programs ESP32; STM32 only exchanges serial text.
$ErrorActionPreference = 'Stop'
$headerPath = "$PSScriptRoot\..\Core\Inc\esp32_uart.h"
$sourcePath = "$PSScriptRoot\..\Core\Src\esp32_uart.c"

if ((-not (Test-Path $headerPath)) -or (-not (Test-Path $sourcePath))) {
    throw 'Missing the standalone esp32_uart.c/.h module.'
}

$header = Get-Content -Raw $headerPath
$source = Get-Content -Raw $sourcePath

foreach ($api in @('Esp32Uart_Init', 'Esp32Uart_SendString', 'Esp32Uart_ReadByte')) {
    if ($header -notmatch $api) {
        throw "Missing public esp32_uart.h function: $api"
    }
}

if ($source -notmatch 'huart2') {
    throw 'ESP32 UART must use CubeMX configured huart2 (USART2).'
}
foreach ($removed in @('Esp32Uart_Process', 'Esp32Uart_ReadLine')) {
    if (($header -match $removed) -or ($source -match $removed)) {
        throw "Obsolete line-oriented UART API remains: $removed"
    }
}
if ($source -notmatch 'HAL_UART_Receive_IT' -or
    $source -notmatch 'ESP32_UART_RX_BUFFER_SIZE' -or
    $source -notmatch 'g_rx_write' -or
    $source -notmatch 'g_rx_read') {
    throw 'UART must use interrupt-driven ring-buffer reception.'
}
if (($source -notmatch 'HAL_UART_Transmit') -or ($source -match 'AT\+|MQTT')) {
    throw 'UART module must send plain text and cannot contain AT/MQTT commands.'
}

Write-Host 'ESP32 UART module check passed.'
