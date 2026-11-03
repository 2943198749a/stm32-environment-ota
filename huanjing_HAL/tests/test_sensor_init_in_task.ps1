# A missing sensor must not block the scheduler or the LCD before tasks begin.
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\freertos.c"

$mxStart = $source.IndexOf('void MX_FREERTOS_Init(void)')
$sensorTaskStart = $source.IndexOf('void StartDefaultTask(void *argument)', $mxStart)
if (($mxStart -lt 0) -or ($sensorTaskStart -lt 0)) {
    throw 'Cannot locate FreeRTOS initialization or sensor task.'
}

$beforeScheduler = $source.Substring($mxStart, $sensorTaskStart - $mxStart)
if ($beforeScheduler -match 'EnvSensor_Init\s*\(') {
    throw 'EnvSensor_Init must not execute before the scheduler starts.'
}

$sensorTask = $source.Substring($sensorTaskStart)
if ($sensorTask -notmatch '(?s)EnvSensor_Init\s*\(\).*?for\s*\(\s*;;\s*\)') {
    throw 'Sensor task must initialize sensors before its update loop.'
}

Write-Host 'Sensor initialization runs inside the sensor task.'
