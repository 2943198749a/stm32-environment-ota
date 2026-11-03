# Key and ESP32 UART simplification regression check.
$ErrorActionPreference = 'Stop'

$keyHeader = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\app_key.h"
$keySource = Get-Content -Raw "$PSScriptRoot\..\Core\Src\app_key.c"
$uartHeader = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\esp32_uart.h"
$uartSource = Get-Content -Raw "$PSScriptRoot\..\Core\Src\esp32_uart.c"

foreach ($function in @('AppKey_Init', 'AppKey_PageTogglePressed')) {
    if ($keyHeader -notmatch $function) {
        throw "app_key.h is missing $function."
    }
}
if ($keySource -notmatch '#define\s+APP_KEY_DEBOUNCE_MS\s+30U') {
    throw 'Key debounce time must use the readable APP_KEY_DEBOUNCE_MS constant.'
}

foreach ($function in @('Esp32Uart_Init', 'Esp32Uart_SendString', 'Esp32Uart_ReadByte')) {
    if ($uartHeader -notmatch $function) {
        throw "esp32_uart.h is missing $function."
    }
}
foreach ($removed in @('Esp32Uart_Process', 'Esp32Uart_ReadLine')) {
    if (($uartHeader -match $removed) -or ($uartSource -match $removed)) {
        throw "Obsolete line-oriented UART API remains: $removed"
    }
}
if ($uartSource -notmatch 'HAL_UART_Receive_IT' -or
    $uartSource -notmatch 'ESP32_UART_RX_BUFFER_SIZE' -or
    $uartSource -notmatch 'g_rx_write' -or
    $uartSource -notmatch 'g_rx_read') {
    throw 'UART must use interrupt-driven ring-buffer reception.'
}
if ($uartSource -match 'AT\+|MQTT|HTTP') {
    throw 'UART module must contain plain text communication only.'
}

Write-Host 'Simple application module check passed.'
