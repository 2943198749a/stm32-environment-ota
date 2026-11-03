param([switch]$Check)
$ErrorActionPreference = 'Stop'

# Resolve both roots before copying so the script cannot drift outside this workspace.
$portableRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspaceRoot = (Resolve-Path (Join-Path $portableRoot '..')).Path
$sourceRoot = Join-Path $workspaceRoot 'huanjing_HAL\user\OTA'
$targetRoot = Join-Path $portableRoot 'stm32\ota_core'

# ota_port.c is board-specific. ota_cloud.c/h remain portable-only optional helpers.
$relativeFiles = @(
    'Inc\sha256.h',
    'Inc\ota_uart.h',
    'Inc\ota_storage.h',
    'Inc\ota_port_config.h',
    'Inc\ota_port.h',
    'Inc\ota_metadata.h',
    'Inc\ota_layout.h',
    'Inc\ota_integrity.h',
    'Inc\ota_app.h',
    'Src\sha256.c',
    'Src\ota_uart.c',
    'Src\ota_storage.c',
    'Src\ota_metadata.c',
    'Src\ota_integrity.c',
    'Src\ota_app.c'
)

$different = @()
foreach ($relative in $relativeFiles) {
    $source = Join-Path $sourceRoot $relative
    $target = Join-Path $targetRoot $relative

    if (-not (Test-Path -LiteralPath $source) -or
        -not (Test-Path -LiteralPath $target)) {
        throw "Mapped OTA file is missing: $relative"
    }

    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) {
        if ($Check) {
            $different += $relative
        }
        else {
            Copy-Item -LiteralPath $source -Destination $target -Force
        }
    }
}

if ($different.Count -ne 0) {
    throw "Different OTA export files: $($different -join ', ')"
}

if ($Check) {
    Write-Host 'OTA export matches canonical source.'
}
else {
    Write-Host 'OTA export synchronized.'
}
