# KEY1 is on PF8, so GPIOF clock must be enabled in gpio.c.
$gpioFile = Get-Content -Raw 'Core/Src/gpio.c'

if ($gpioFile -notmatch '__HAL_RCC_GPIOF_CLK_ENABLE\(\)') {
    throw 'GPIOF clock is missing for KEY1.'
}

Write-Output 'GPIOF_CLOCK_CONFIGURATION_PRESENT'
