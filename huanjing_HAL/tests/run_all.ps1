$ErrorActionPreference = 'Continue'
$failed = @()

Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'test_*.ps1' |
    Sort-Object Name |
    ForEach-Object {
        Write-Host "RUN $($_.Name)"
        & powershell -NoProfile -ExecutionPolicy Bypass -File $_.FullName
        if ($LASTEXITCODE -ne 0) {
            $failed += $_.Name
        }
    }

if ($failed.Count -ne 0) {
    throw "Failed tests: $($failed -join ', ')"
}

Write-Host 'ALL POWERSHELL TESTS PASSED'
