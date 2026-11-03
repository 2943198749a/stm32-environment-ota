# Sensor module interface and implementation regression check.
$ErrorActionPreference = 'Stop'

$modules = @(
    @{ Name = 'dht11'; Required = @('Dht11_Init', 'Dht11_Read') },
    @{ Name = 'bh1750'; Required = @('Bh1750_Init', 'Bh1750_ReadLux') },
    @{ Name = 'mq135'; Required = @('Mq135_Read') },
    @{ Name = 'env_sensor'; Required = @('EnvSensor_Init', 'EnvSensor_Update', 'EnvSensor_GetData') }
)

foreach ($module in $modules) {
    $header = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\$($module.Name).h"
    $source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\$($module.Name).c"

    if ($header -notmatch '/\*') {
        throw "$($module.Name).h needs a documented public interface."
    }

    foreach ($function in $module.Required) {
        if (($header -notmatch $function) -or ($source -notmatch $function)) {
            throw "$($module.Name) is missing the public function $function."
        }
    }
}

$environment = Get-Content -Raw "$PSScriptRoot\..\Core\Src\env_sensor.c"
foreach ($reader in @('Dht11_Read', 'Bh1750_ReadLux', 'Mq135_Read')) {
    if ($environment -notmatch $reader) {
        throw "env_sensor.c must collect data from $reader."
    }
}
if ($environment -match 'GenerateSimulation|ENV_SENSOR_USE_SIMULATOR') {
    throw 'env_sensor.c cannot contain simulated sensor data.'
}

Write-Host 'Simple sensor module check passed.'
