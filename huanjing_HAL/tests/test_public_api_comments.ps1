$ErrorActionPreference = 'Stop'

$contracts = @(
    @{ Path = '..\Core\Inc\env_sensor.h'; APIs = @('EnvSensor_Init', 'EnvSensor_Update', 'EnvSensor_GetData') },
    @{ Path = '..\Core\Inc\esp32_status.h'; APIs = @('Esp32Status_Init', 'Esp32Status_ProcessLine', 'Esp32Status_Get') },
    @{ Path = '..\Core\Inc\app_key.h'; APIs = @('AppKey_Init', 'AppKey_PageTogglePressed') },
    @{ Path = '..\Core\Inc\dht11.h'; APIs = @('Dht11_Init', 'Dht11_Read') },
    @{ Path = '..\Core\Inc\bh1750.h'; APIs = @('Bh1750_Init', 'Bh1750_ReadLux') },
    @{ Path = '..\Core\Inc\mq135.h'; APIs = @('Mq135_Read') },
    @{ Path = '..\Core\Inc\lcd.h'; APIs = @('LCD_Init', 'LCD_SetArea', 'LCD_ClearAll', 'LCD_WriteAsciiString', 'LCD_DrawRectangle') },
    @{ Path = '..\user\OTA\Inc\sha256.h'; APIs = @('Sha256_Init', 'Sha256_Update', 'Sha256_Final', 'Sha256_HexEquals') },
    @{ Path = '..\user\OTA\Inc\ota_app.h'; APIs = @('OtaApp_Init', 'OtaApp_Begin', 'OtaApp_Write', 'OtaApp_Finalize', 'OtaApp_Abort', 'OtaApp_ConfirmRunning', 'OtaApp_IsConfirmationRequired') },
    @{ Path = '..\user\OTA\Inc\ota_storage.h'; APIs = @('OtaStorage_Load', 'OtaStorage_Save') },
    @{ Path = '..\user\OTA\Inc\ota_metadata.h'; APIs = @('OtaRecord_Make', 'OtaRecord_SetAttemptCount', 'OtaRecord_IsValid', 'OtaRecord_SelectNewest', 'OtaRecord_BootSlot') },
    @{ Path = '..\user\OTA\Inc\ota_integrity.h'; APIs = @('Ota_Crc32', 'Ota_ImageSizeIsValid', 'Ota_VectorWordsAreValid') },
    @{ Path = '..\..\portable_ota\stm32\ota_core\Inc\ota_cloud.h'; APIs = @('OtaCloud_ParseEvent', 'OtaCloud_GetLastParseError') },
    @{ Path = '..\user\OTA\Inc\ota_port.h'; APIs = @('OtaPort_ReadByte', 'OtaPort_Send', 'OtaPort_ResetToBootloader', 'OtaPort_HandleGatewayStatus') })

foreach ($contract in $contracts) {
    $path = Join-Path $PSScriptRoot $contract.Path
    $text = Get-Content -Raw $path
    foreach ($api in $contract.APIs) {
        $pattern = '(?s)/\*.{20,600}?\*/\s*[^;]*\b' + [regex]::Escape($api) + '\s*\('
        if ($text -notmatch $pattern) {
            throw "$($contract.Path) needs a detailed contract before $api."
        }
    }
}

Write-Host 'Public API comment coverage check passed.'
