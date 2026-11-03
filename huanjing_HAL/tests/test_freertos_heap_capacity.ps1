# A dynamic task must have room for its stack plus FreeRTOS control blocks.
$config = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\FreeRTOSConfig.h"
$freertos = Get-Content -Raw "$PSScriptRoot\..\Core\Src\freertos.c"

if ($config -notmatch 'configTOTAL_HEAP_SIZE\s*\(\(size_t\)(\d+)\)') {
    throw 'Cannot read configTOTAL_HEAP_SIZE.'
}
$heapBytes = [int]$Matches[1]

$stackMatches = [regex]::Matches($freertos, '\.stack_size\s*=\s*(\d+)\s*\*\s*(\d+)')
if ($stackMatches.Count -ne 4) {
    throw 'Expected the four dynamic task stack sizes.'
}

$stackBytes = 0
foreach ($match in $stackMatches) {
    $stackBytes += ([int]$match.Groups[1].Value * [int]$match.Groups[2].Value)
}

# 4 KB reserves room for four task control blocks and FreeRTOS runtime objects.
if ($heapBytes -lt ($stackBytes + 4096)) {
    throw "FreeRTOS heap is $heapBytes bytes, but tasks need $stackBytes bytes of stack plus control-block reserve."
}

Write-Host "FreeRTOS heap capacity check passed: $heapBytes bytes."
