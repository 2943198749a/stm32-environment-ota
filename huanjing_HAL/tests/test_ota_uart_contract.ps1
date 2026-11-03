$ErrorActionPreference = 'Stop'
$header = Get-Content -Raw "$PSScriptRoot\..\user\OTA\Inc\ota_uart.h"
$source = Get-Content -Raw "$PSScriptRoot\..\user\OTA\Src\ota_uart.c"

$requiredTokens = @(
    '0xA5U', '0x5AU', '512U', 'OTA_BEGIN,', 'OTA_END', 'OTA_ABORT',
    'OTA_READY', 'OTA_ACK', 'OTA_NACK', 'OTA_OK',
    'OTA_ERROR,begin', 'OTA_ERROR,slot', 'OTA_ERROR,verify', 'OTA_ERROR,abort')

foreach ($token in $requiredTokens) {
    if (($header + $source) -notmatch [regex]::Escape($token)) {
        throw "OTA wire-contract token is missing: $token"
    }
}

if ($source -match 'crc\s*=\s*OtaUart_Crc16\(bytes' -or
    $source -match 'crc\s*=\s*OtaUart_Crc16\(payload') {
    throw 'Dead intermediate CRC calculations remain in frame construction.'
}
if ($source -notmatch '"%s,%u"') {
    throw 'Sequence ACK/NACK responses must share the bounded formatter.'
}

Write-Host 'OTA UART wire-contract check passed.'
