$ErrorActionPreference = 'Stop'
$projectRoot = Join-Path $PSScriptRoot '..\MDK-ARM'
$targets = @(
    @{ File = 'Bootloader.uvprojx'; Name = 'Bootloader'; Address = '0x8000000'; Size = '0x7000' },
    @{ File = 'AppSlotA.uvprojx'; Name = 'AppSlotA'; Address = '0x8008000'; Size = '0x3c000' },
    @{ File = 'AppSlotB.uvprojx'; Name = 'AppSlotB'; Address = '0x8044000'; Size = '0x3c000' })

foreach ($target in $targets) {
    [xml]$project = Get-Content -Raw (Join-Path $projectRoot $target.File)
    $option = $project.Project.Targets.Target.TargetOption
    $common = $option.TargetCommonOption
    $arm = $option.TargetArmAds

    if ($common.Device -ne 'STM32F103ZE' -or
        $common.PackID -notmatch 'STM32F1xx_DFP' -or
        $common.Cpu -notmatch 'Cortex-M3' -or
        $common.Cpu -match 'FPU' -or
        $arm.ArmAdsMisc.AdsCpuType -ne '"Cortex-M3"') {
        throw "$($target.Name) must target STM32F103ZE Cortex-M3 without an FPU."
    }

    $rom = $arm.ArmAdsMisc.OnChipMemories.OCR_RVCT4
    if (($rom.StartAddress.ToLowerInvariant() -ne $target.Address) -or
        ($rom.Size.ToLowerInvariant() -ne $target.Size)) {
        throw "$($target.Name) has the wrong IROM1 address or size."
    }
}

Write-Host 'Keil CPU and OTA target memory check passed.'
