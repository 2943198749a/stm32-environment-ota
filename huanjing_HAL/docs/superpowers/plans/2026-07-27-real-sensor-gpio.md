# Real Sensor GPIO Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the environmental-data simulator with real DHT11, BH1750, and MQ-135 hardware drivers and give each application/device module a dedicated `.c/.h` interface.

**Architecture:** ADC1 and I2C1 are initialized in dedicated generated-style modules. The DHT11, BH1750, and MQ-135 drivers own their respective electrical protocols. `env_sensor` schedules those drivers and retains the last successful reading; application UI and ESP32 code consume only `EnvData_t`.

**Tech Stack:** C11-compatible embedded C, STM32F1 HAL, STM32F103ZET6, Keil MDK project.

## Global Constraints

- Use DHT11 DATA on PC3, BH1750 I2C1 on PB6/PB7 at 100 kHz, and MQ-135 ADC1 channel 10 on PC0.
- Do not retain `ENV_SENSOR_USE_SIMULATOR` or generate environmental values from time/count state.
- Do not embed new credentials or alter existing ESP32 upload behavior.
- Preserve the last successful field value when an individual sensor read fails; expose validity flags in `EnvData_t`.
- No hardware is connected, so validate through host/static checks and a Keil build when the toolchain is available.

---

### Task 1: Add ADC1 and I2C1 peripheral modules

**Files:**
- Create: `Core/Src/adc.c`
- Create: `Core/Inc/adc.h`
- Create: `Core/Src/i2c.c`
- Create: `Core/Inc/i2c.h`
- Modify: `Core/Inc/stm32f1xx_hal_conf.h:37-49`
- Modify: `Core/Src/main.c:1-45`
- Test: `tests/test_real_sensor_configuration.ps1`

**Interfaces:**
- Consumes: `main.h`, STM32 HAL ADC/I2C APIs.
- Produces: `ADC_HandleTypeDef hadc1`, `I2C_HandleTypeDef hi2c1`, `void MX_ADC1_Init(void)`, and `void MX_I2C1_Init(void)`.

- [ ] **Step 1: Write the failing test**

```powershell
$main = Get-Content -Raw "$PSScriptRoot\..\Core\Src\main.c"
$config = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\stm32f1xx_hal_conf.h"
$adc = Get-Content -Raw "$PSScriptRoot\..\Core\Src\adc.c"
$i2c = Get-Content -Raw "$PSScriptRoot\..\Core\Src\i2c.c"
if ($main -notmatch 'MX_ADC1_Init\(\)' -or $main -notmatch 'MX_I2C1_Init\(\)') { throw 'main must initialize ADC1 and I2C1.' }
if ($config -notmatch '^#define HAL_ADC_MODULE_ENABLED' -or $config -notmatch '^#define HAL_I2C_MODULE_ENABLED') { throw 'ADC and I2C HAL modules must be enabled.' }
if ($adc -notmatch 'ADC_CHANNEL_10' -or $i2c -notmatch 'I2C1') { throw 'ADC1 channel 10 and I2C1 must be configured.' }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: FAIL because ADC/I2C files and initialization calls do not exist.

- [ ] **Step 3: Write minimal implementation**

```c
/* adc.h */
extern ADC_HandleTypeDef hadc1;
void MX_ADC1_Init(void);

/* i2c.h */
extern I2C_HandleTypeDef hi2c1;
void MX_I2C1_Init(void);
```

Configure ADC1 with independent mode, right-aligned 12-bit single conversion, channel 10, and software trigger. Configure I2C1 as 100 kHz I2C mode with 7-bit addressing. Enable the corresponding HAL module macros, HAL driver sources, and `main.c` init calls.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: PASS and prints `REAL_SENSOR_CONFIGURATION_PRESENT`.

- [ ] **Step 5: Commit**

```bash
git add Core/Src/adc.c Core/Inc/adc.h Core/Src/i2c.c Core/Inc/i2c.h Core/Inc/stm32f1xx_hal_conf.h Core/Src/main.c tests/test_real_sensor_configuration.ps1
git commit -m "feat: configure ADC1 and I2C1 peripherals"
```

### Task 2: Configure sensor GPIO pins and add device driver interfaces

**Files:**
- Create: `Core/Src/dht11.c`
- Create: `Core/Inc/dht11.h`
- Create: `Core/Src/bh1750.c`
- Create: `Core/Inc/bh1750.h`
- Create: `Core/Src/mq135.c`
- Create: `Core/Inc/mq135.h`
- Modify: `Core/Src/gpio.c:42-98`
- Modify: `Core/Inc/main.h:45-55`
- Test: `tests/test_real_sensor_configuration.ps1`

**Interfaces:**
- Consumes: `hi2c1` from `i2c.h`, `hadc1` from `adc.h`, `HAL_GPIO_ReadPin`, `HAL_I2C_Master_Transmit/Receive`, and `HAL_ADC_PollForConversion`.
- Produces: `bool Dht11_Read(Dht11Data_t *data)`, `bool Bh1750_ReadLux(uint16_t *lux)`, `bool Mq135_Read(Mq135Data_t *data)`.

- [ ] **Step 1: Extend the failing test**

```powershell
$gpio = Get-Content -Raw "$PSScriptRoot\..\Core\Src\gpio.c"
$dht = Get-Content -Raw "$PSScriptRoot\..\Core\Src\dht11.c"
$bh = Get-Content -Raw "$PSScriptRoot\..\Core\Src\bh1750.c"
$mq = Get-Content -Raw "$PSScriptRoot\..\Core\Src\mq135.c"
if ($gpio -notmatch 'GPIO_PIN_0' -or $gpio -notmatch 'GPIO_PIN_3' -or $gpio -notmatch 'GPIO_PIN_6\|GPIO_PIN_7') { throw 'PC0, PC3, and PB6/PB7 must be configured.' }
if ($dht -notmatch 'Dht11_Read' -or $bh -notmatch 'Bh1750_ReadLux' -or $mq -notmatch 'Mq135_Read') { throw 'Each physical sensor must export a read function.' }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: FAIL because sensor drivers do not exist.

- [ ] **Step 3: Write minimal implementation**

```c
typedef struct { int16_t temperature_tenth; uint16_t humidity_tenth; } Dht11Data_t;
bool Dht11_Read(Dht11Data_t *data);

bool Bh1750_ReadLux(uint16_t *lux);

typedef struct { uint16_t raw_adc; uint16_t estimated_ppm; } Mq135Data_t;
bool Mq135_Read(Mq135Data_t *data);
```

Configure PC0 as analog, PC3 as input with pull-up, and PB6/PB7 as I2C alternate-function open-drain. Implement DHT11's start signal, 40-bit read, and checksum check with a DWT-cycle microsecond delay. Implement BH1750 power-on/continuous high-resolution command and two-byte lux read. Implement MQ-135 channel-10 conversion and an explicitly uncalibrated `raw_adc * 1000 / 4095` display estimate.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: PASS and prints `REAL_SENSOR_CONFIGURATION_PRESENT`.

- [ ] **Step 5: Commit**

```bash
git add Core/Src/dht11.c Core/Inc/dht11.h Core/Src/bh1750.c Core/Inc/bh1750.h Core/Src/mq135.c Core/Inc/mq135.h Core/Src/gpio.c Core/Inc/main.h tests/test_real_sensor_configuration.ps1
git commit -m "feat: add real environmental sensor drivers"
```

### Task 3: Replace the simulator with environmental-data aggregation

**Files:**
- Modify: `Core/Src/env_sensor.c:1-94`
- Modify: `Core/Inc/env_sensor.h:1-47`
- Modify: `Core/Src/main.c:1-80`
- Modify: `Core/Src/app_ui.c:48-100`
- Test: `tests/test_real_sensor_configuration.ps1`

**Interfaces:**
- Consumes: `Dht11_Read`, `Bh1750_ReadLux`, `Mq135_Read`.
- Produces: `EnvData_t` with `temperature_valid`, `humidity_valid`, `light_valid`, `air_valid`, and non-simulated values through `EnvSensor_GetData()`.

- [ ] **Step 1: Extend the failing test**

```powershell
$env = Get-Content -Raw "$PSScriptRoot\..\Core\Src\env_sensor.c"
$header = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\env_sensor.h"
if ($env -match 'GenerateSimulation|ENV_SENSOR_USE_SIMULATOR|sample_count\s*\+') { throw 'Simulation code must be removed.' }
if ($env -notmatch 'Dht11_Read' -or $env -notmatch 'Bh1750_ReadLux' -or $env -notmatch 'Mq135_Read') { throw 'Environment module must aggregate real sensor drivers.' }
if ($header -notmatch 'temperature_valid' -or $header -notmatch 'air_valid') { throw 'Environment data must expose validity flags.' }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: FAIL because `env_sensor` still contains simulator state.

- [ ] **Step 3: Write minimal implementation**

```c
if (Dht11_Read(&dht)) {
    g_env_data.temperature_tenth = dht.temperature_tenth;
    g_env_data.humidity_tenth = dht.humidity_tenth;
    g_env_data.temperature_valid = true;
    g_env_data.humidity_valid = true;
}
```

Apply the same success-only update policy to BH1750 and MQ-135. Keep a one-second aggregation cadence. Initialize each driver in `EnvSensor_Init`; remove simulator macros, generator state, and descriptions. Update the UI to print `--` for an invalid measurement.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: PASS and prints `REAL_SENSOR_CONFIGURATION_PRESENT`.

- [ ] **Step 5: Commit**

```bash
git add Core/Src/env_sensor.c Core/Inc/env_sensor.h Core/Src/main.c Core/Src/app_ui.c tests/test_real_sensor_configuration.ps1
git commit -m "feat: aggregate real environmental sensor readings"
```

### Task 4: Register new files and verify the project

**Files:**
- Modify: `MDK-ARM/42_lcd_hal.uvprojx:398-545`
- Modify: `README.md:1-90`
- Modify: `tests/test_env_sensor.c:1-46`
- Test: `tests/test_real_sensor_configuration.ps1`
- Test: `tests/test_key_mapping.c`
- Test: `tests/test_esp32_at_receive.ps1`
- Test: `tests/test_key_gpio_clock.ps1`

**Interfaces:**
- Consumes: all production source and header files.
- Produces: a Keil project compiling ADC/I2C HAL drivers and all new modules; documentation with real sensor wiring and no simulator enablement instructions.

- [ ] **Step 1: Extend the failing test**

```powershell
$project = Get-Content -Raw "$PSScriptRoot\..\MDK-ARM\42_lcd_hal.uvprojx"
foreach ($file in 'adc.c', 'i2c.c', 'dht11.c', 'bh1750.c', 'mq135.c', 'stm32f1xx_hal_adc.c', 'stm32f1xx_hal_i2c.c') {
    if ($project -notmatch [regex]::Escape($file)) { throw "Keil project must contain $file." }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1`

Expected: FAIL because the Keil file list lacks the new modules and HAL drivers.

- [ ] **Step 3: Write minimal implementation**

Add the five application sources and the HAL ADC/I2C sources to the existing Keil source groups. Replace README simulation instructions with real-driver limitations and calibration notes. Replace the host simulator test with a source-level regression test that checks the aggregation API and simulator removal.

- [ ] **Step 4: Run verification**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File tests/test_real_sensor_configuration.ps1
powershell -ExecutionPolicy Bypass -File tests/test_key_gpio_clock.ps1
powershell -ExecutionPolicy Bypass -File tests/test_esp32_at_receive.ps1
```

Expected: all scripts pass. In Keil MDK, open `MDK-ARM/42_lcd_hal.uvprojx` and run Rebuild; expected result is zero errors.

- [ ] **Step 5: Commit**

```bash
git add MDK-ARM/42_lcd_hal.uvprojx README.md tests
git commit -m "build: register real sensor modules"
```
