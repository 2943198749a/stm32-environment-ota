# Regression check: environment data must come from configured real peripherals.
$ErrorActionPreference = 'Stop'

$root = Join-Path $PSScriptRoot '..'
$source = Join-Path $root 'Core\Src'
$include = Join-Path $root 'Core\Inc'

function Require-File([string]$path) {
    if (-not (Test-Path $path)) {
        throw "Missing required module: $path"
    }
    return Get-Content -Raw $path
}

$main = Require-File (Join-Path $source 'main.c')
$config = Require-File (Join-Path $include 'stm32f1xx_hal_conf.h')
$adc = Require-File (Join-Path $source 'adc.c')
$i2c = Require-File (Join-Path $source 'i2c.c')

if ($main -notmatch 'MX_ADC1_Init\(\)' -or $main -notmatch 'MX_I2C1_Init\(\)') {
    throw 'main.c must initialize ADC1 and I2C1.'
}
if ($config -notmatch '(?m)^\s*#define HAL_ADC_MODULE_ENABLED' -or
    $config -notmatch '(?m)^\s*#define HAL_I2C_MODULE_ENABLED') {
    throw 'ADC and I2C HAL modules must be enabled.'
}
if ($adc -notmatch 'ADC_CHANNEL_10' -or $i2c -notmatch 'I2C1') {
    throw 'ADC1 channel 10 and I2C1 must be configured.'
}

$gpio = Require-File (Join-Path $source 'gpio.c')
$dht = Require-File (Join-Path $source 'dht11.c')
$bh1750 = Require-File (Join-Path $source 'bh1750.c')
$mq135 = Require-File (Join-Path $source 'mq135.c')

if ($gpio -notmatch 'GPIO_PIN_3') {
    throw 'PC3 must be configured for DHT11.'
}
if ($dht -notmatch 'Dht11_Read' -or $bh1750 -notmatch 'Bh1750_ReadLux' -or $mq135 -notmatch 'Mq135_Read') {
    throw 'Every sensor must provide a read function.'
}

$environment = Require-File (Join-Path $source 'env_sensor.c')
$environmentHeader = Require-File (Join-Path $include 'env_sensor.h')
if ($environment -match 'GenerateSimulation|ENV_SENSOR_USE_SIMULATOR') {
    throw 'Simulation code must be removed from env_sensor.c.'
}
if ($environment -notmatch 'Dht11_Read' -or $environment -notmatch 'Bh1750_ReadLux' -or $environment -notmatch 'Mq135_Read') {
    throw 'env_sensor.c must combine the three real sensor drivers.'
}
if ($environmentHeader -notmatch 'temperature_valid' -or $environmentHeader -notmatch 'air_valid') {
    throw 'EnvData_t must expose validity flags.'
}

$project = Require-File (Join-Path $root 'MDK-ARM\42_lcd_hal.uvprojx')
foreach ($file in 'adc.c', 'i2c.c', 'dht11.c', 'bh1750.c', 'mq135.c',
                   'stm32f1xx_hal_adc.c', 'stm32f1xx_hal_adc_ex.c', 'stm32f1xx_hal_i2c.c') {
    if ($project -notmatch [regex]::Escape($file)) {
        throw "Keil project must contain $file."
    }
}

$freertos = Require-File (Join-Path $source 'freertos.c')
$ioc = Require-File (Join-Path $root '42_lcd_hal.ioc')

if ($freertos -notmatch 'latestEnvData' -or
    $freertos -notmatch 'taskENTER_CRITICAL\(' -or
    $freertos -notmatch 'taskEXIT_CRITICAL\(') {
    throw 'FreeRTOS tasks must protect the shared environment snapshot with a short critical section.'
}
if ($freertos -match 'AppUi_Render\(EnvSensor_GetData\(' -or
    $freertos -match 'Esp32C3|APP_USE_ESP_AT') {
    throw 'UI task must use a protected data copy and cannot contain old ESP-AT code.'
}
if ($freertos -match '(?s)USER CODE BEGIN Init.*?(EnvSensor_Init|AppUi_Render|Esp32Uart_Process).*?USER CODE END Init') {
    throw 'Sensor, LCD and UART work must run inside their tasks, not MX_FREERTOS_Init.'
}
if ($ioc -notmatch 'sensortask,24,256' -or
    $ioc -notmatch 'uitask,8,256' -or
    $ioc -notmatch 'uarttask,8,256') {
    throw 'CubeMX task stack sizes must be 256, 256, and 256 words.'
}

if ($main -notmatch '(?s)USER CODE BEGIN 2.*?LCD_Init\(\).*?USER CODE END 2') {
    throw 'LCD_Init must stay inside main.c USER CODE BEGIN 2.'
}

Write-Output 'REAL_SENSOR_CONFIGURATION_PRESENT'
