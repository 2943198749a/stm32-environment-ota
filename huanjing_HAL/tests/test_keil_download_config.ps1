$ErrorActionPreference = 'Stop'

$projectRoot = Join-Path $PSScriptRoot '..\MDK-ARM'
$optionFile = Join-Path $projectRoot 'AppSlotA.uvoptx'
$optionText = Get-Content -Raw -LiteralPath $optionFile

# AppSlotA must use the STM32F103ZE 512 KB flash algorithm.
# Checking uvprojx alone is insufficient: a stale uvoptx can compile but fail to flash.
$containsWrongDevice = $optionText -match 'STM32F4(?:07|xx)'
$containsF1Algorithm = $optionText -match 'STM32F10x_512(?:\.FLM)?'
$containsF1Device = $optionText -match '\$\$Device:STM32F103ZE\$'
if ($containsWrongDevice -or (-not $containsF1Algorithm) -or (-not $containsF1Device)) {
    throw 'AppSlotA must use the STM32F103ZE STM32F10x_512 flash algorithm.'
}

[xml]$option = $optionText
$description = $option.ProjectOpt.Target.TargetOption.DebugDescription

# Keil must run its complete flash programming sequence for Download.
if ($description.Enable -ne '1' -or $description.EnableFlashSeq -ne '1') {
    throw 'AppSlotA flash download sequence must be enabled.'
}

Write-Host 'Keil AppSlotA download configuration check passed.'
