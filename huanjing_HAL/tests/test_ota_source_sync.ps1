$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot '..\..\portable_ota\tools\sync_ota_core.ps1'
if (-not (Test-Path -LiteralPath $script)) {
    throw "OTA sync tool is missing: $script"
}
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -Check
if ($LASTEXITCODE -ne 0) {
    throw 'Portable OTA source differs from the canonical project source.'
}
Write-Host 'OTA source synchronization check passed.'
