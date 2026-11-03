# A failed osThreadNew() call must not silently leave any application task absent.
$source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\freertos.c"

foreach ($task in @('sensortaskHandle', 'uitaskHandle', 'uarttaskHandle', 'otaTaskHandle')) {
    if ($source -notmatch "$task\s*=\s*osThreadNew") {
        throw "$task must be created with osThreadNew()."
    }
    if ($source -notmatch "$task\s*!=\s*NULL") {
        throw "$task must be included in the consolidated creation check."
    }
}

if ($source -notmatch 'if\s*\(\s*!RtosTasks_AreCreated\s*\(\s*\)\s*\)\s*\{\s*Error_Handler\s*\(\s*\)') {
    throw 'MX_FREERTOS_Init must stop when any task creation fails.'
}

Write-Host 'FreeRTOS task-creation checks are present.'
