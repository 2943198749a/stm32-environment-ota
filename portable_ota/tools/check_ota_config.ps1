param(
  [Parameter(Mandatory=$true)][string]$BinPath,
  [Parameter(Mandatory=$true)][string]$Slot,
  [Parameter(Mandatory=$true)][string]$Version,
  [string]$SourceVersion = ''
)
$ErrorActionPreference = 'Stop'
$slotSize = 0x3C000
if (-not (Test-Path -LiteralPath $BinPath)) { throw "BIN file not found: $BinPath" }
if ($Slot -notin @('A','B')) { throw 'Slot must be A or B.' }
if ([string]::IsNullOrWhiteSpace($Version)) { throw 'Version must not be empty.' }
if ($Version -notmatch '^[A-Za-z0-9._-]+$') { throw 'Version has invalid characters.' }
$length = (Get-Item -LiteralPath $BinPath).Length
if (($length -le 0) -or ($length -gt $slotSize)) { throw "BIN size $length exceeds slot size $slotSize." }
$hash = (Get-FileHash -LiteralPath $BinPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output 'OTA CONFIG PASS'
Write-Output "slot=$Slot version=$Version size=$length sha256=$hash"
if ($SourceVersion) { Write-Output "source_version=$SourceVersion" }
