# Code Simplification and Detailed Comments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变硬件配置、运行时序、串口协议、Flash 布局和现有功能的前提下，整体精简项目自写代码，并补齐面向初学者的详细中文注释与有效回归测试。

**Architecture:** 保留现有四任务、共享环境快照、UART 中断环形缓冲区和显式 OTA 状态机。通过小型静态辅助函数收敛重复分支，以 `user/OTA` 为维护源并同步便携副本；测试先描述当前真实架构，再逐模块驱动重构。

**Tech Stack:** STM32F103 HAL、CMSIS-RTOS v2、FreeRTOS、Keil MDK 5.38 / ARMCC 5.06、C99 风格嵌入式 C、PowerShell 回归测试、Arduino ESP32-C3 网关。

**Spec:** `docs/superpowers/specs/2026-09-01-code-simplification-and-comments-design.md`

## Global Constraints

- 保留 DHT11、BH1750、MQ135、LCD 双页面、KEY1、ESP32 状态、环境上传、Bootloader 和 A/B OTA 全部功能。
- 不修改 STM32 HAL、CMSIS、FreeRTOS 内核和启动汇编源码。
- 不改变 GPIO、USART2、I2C1、ADC1、FSMC、系统时钟和传感器接线配置。
- 四个任务继续使用现有栈大小 `256*4`、`256*4`、`256*4`、`1024*4` 字节以及现有优先级。
- 任务周期继续保持传感器 1000 ms、UI 500 ms、环境上传 1000 ms、OTA 1 ms。
- OTA 新镜像继续在稳定运行 10000 ms 后确认。
- 不改变 `ENV`、`STATUS`、`OTA_BEGIN`、`OTA_END`、`OTA_ABORT`、`OTA_READY`、`OTA_ACK`、`OTA_NACK`、`OTA_OK`、`OTA_ERROR` 和 `BOOT_OK` 的协议格式。
- 不改变 OTA 同步字节 `0xA5 0x5A`、512 字节最大负载、CRC16/MODBUS、小端序或 Flash A/B 布局。
- CubeMX 生成文件优先只改 `USER CODE` 区域。
- 当前目录没有 `.git`，不得自行初始化 Git；下面每个“检查点”替代提交步骤，并记录该任务的测试输出。
- 不删除 Keil 用户配置、构建产物、临时文件或生成文档。

## File Responsibility Map

- `Core/Src/freertos.c`：四个任务入口、环境快照共享、遥测发送和新固件确认调度。
- `Core/Src/esp32_uart.c` / `Core/Inc/esp32_uart.h`：USART2 中断环形缓冲区、按字节读取和文本发送；不解析业务协议。
- `Core/Src/esp32_status.c` / `Core/Inc/esp32_status.h`：解析并并发保存一条合法 `STATUS` 状态。
- `Core/Src/app_ui.c` / `Core/Inc/app_ui.h`：LCD 两页面布局、数据格式化和变化检测。
- `Core/Src/env_sensor.c` / `Core/Inc/env_sensor.h`：汇总三个传感器的最新值及有效标志。
- `Core/Src/app_key.c`、`dht11.c`、`bh1750.c`、`mq135.c` 及对应头文件：板级输入和传感器驱动，逻辑保持不变，补充接口与时序注释。
- `user/OTA/Src/ota_uart.c`：STM32 侧 OTA 文本命令和二进制帧状态机。
- `user/OTA/Src/ota_app.c`、`ota_storage.c`、`ota_metadata.c`、`ota_integrity.c`、`ota_cloud.c`、`ota_port.c`：OTA 会话、Flash、元数据、完整性、云事件和板级适配。
- `portable_ota/stm32/ota_core`：从 `user/OTA` 导出的自包含便携副本。
- `portable_ota/tools/sync_ota_core.ps1`：通用 OTA 文件同步及一致性检查。
- `portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway`：ESP32-C3 华为云连接、固件下载和 STM32 OTA 传输。
- `tests/*.ps1`：结构、协议和工程配置回归检查。

---

### Task 1: Update Regression Tests to Describe the Current Four-Task Architecture

**Files:**
- Modify: `tests/test_beginner_rtos_uart.ps1`
- Modify: `tests/test_freertos_task_creation_check.ps1`
- Modify: `tests/test_freertos_heap_capacity.ps1`
- Modify: `tests/test_lcd_ota_ui.ps1`
- Modify: `tests/test_simple_app_modules.ps1`
- Modify: `tests/test_esp32_uart_module.ps1`
- Modify: `tests/test_engineering_manual.ps1`
- Create: `tests/run_all.ps1`

**Interfaces:**
- Consumes: current production APIs `Esp32Uart_Init`, `Esp32Uart_SendString`, `Esp32Uart_ReadByte`, `OtaUart_Process`, and `AppUi_Render(const EnvData_t *, const Esp32Status_t *)`.
- Produces: one deterministic test runner and red tests that require four task checks, a single byte-oriented UART receive path, the current system-status LCD page, and current documentation.

- [ ] **Step 1: Add a deterministic test runner**

Create `tests/run_all.ps1` with this exact control flow so one failing script does not hide later results:

```powershell
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
```

- [ ] **Step 2: Replace old three-task and old UART expectations**

In `test_beginner_rtos_uart.ps1`, require all four handles and current task calls:

```powershell
foreach ($task in @('sensortaskHandle', 'uitaskHandle', 'uarttaskHandle', 'otaTaskHandle')) {
    if ($source -notmatch [regex]::Escape($task)) {
        throw "Missing current FreeRTOS task handle: $task"
    }
}

foreach ($required in @(
    'EnvSensor_Init\s*\(\s*\)',
    'EnvSensor_Update\s*\(\s*\)',
    'AppUi_Render\s*\(\s*&environment\s*,\s*&esp32_status\s*\)',
    'OtaUart_Process\s*\(\s*\)',
    'osDelay\s*\(\s*1\s*\)',
    'osDelay\s*\(\s*500\s*\)',
    'osDelay\s*\(\s*1000\s*\)')) {
    if ($source -notmatch $required) {
        throw "Missing current FreeRTOS behavior: $required"
    }
}
```

In `test_freertos_task_creation_check.ps1`, change the handle list to all four handles and require each handle to appear in a null check followed by `Error_Handler()`.

In `test_freertos_heap_capacity.ps1`, require exactly four `.stack_size` matches and retain the existing 4096-byte control-block reserve. Update the diagnostic text from three tasks to four tasks.

- [ ] **Step 3: Replace old LCD and UART module expectations**

In `test_lcd_ota_ui.ps1`, require the current two-input render contract and system page:

```powershell
if ($header -notmatch 'AppUi_Render\s*\(\s*const\s+EnvData_t\s*\*\s*environment\s*,\s*const\s+Esp32Status_t\s*\*\s*esp32_status\s*\)') {
    throw 'AppUi_Render must receive environment and ESP32 status snapshots.'
}
foreach ($required in @('SYSTEM STATUS', 'WIFI', 'MQTT', 'VERSION:', 'AppUi_DrawProgressBar')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "LCD system page is missing: $required"
    }
}
if ($source -match 'OTA RESERVED|Esp32C3|APP_USE_ESP_AT') {
    throw 'LCD cannot retain the obsolete OTA placeholder or ESP-AT implementation.'
}
```

In both UART tests, require these three public APIs:

```powershell
foreach ($api in @('Esp32Uart_Init', 'Esp32Uart_SendString', 'Esp32Uart_ReadByte')) {
    if ($header -notmatch $api) {
        throw "Missing byte-oriented UART API: $api"
    }
}
foreach ($removed in @('Esp32Uart_Process', 'Esp32Uart_ReadLine')) {
    if (($header -match $removed) -or ($source -match $removed)) {
        throw "Obsolete line-oriented UART API remains: $removed"
    }
}
if ($source -notmatch 'HAL_UART_Receive_IT' -or
    $source -notmatch 'ESP32_UART_RX_BUFFER_SIZE' -or
    $source -notmatch 'g_rx_write' -or
    $source -notmatch 'g_rx_read') {
    throw 'UART must use interrupt-driven ring-buffer reception.'
}
```

- [ ] **Step 4: Point the documentation test at the actual engineering manual**

Replace the “first Markdown file larger than 8000 bytes” lookup with:

```powershell
$manualPath = Join-Path $PSScriptRoot '..\工程说明.md'
if (-not (Test-Path -LiteralPath $manualPath)) {
    throw "Engineering manual is missing: $manualPath"
}
$manual = Get-Content -Raw -Encoding UTF8 $manualPath

foreach ($section in @('FreeRTOS 四任务', 'DHT11', 'BH1750', 'MQ135',
                        'ESP32 UART', 'CubeMX', 'USER CODE', '系统状态页', 'A/B OTA')) {
    if ($manual -notmatch [regex]::Escape($section)) {
        throw "Engineering manual is missing section: $section"
    }
}

foreach ($api in @('EnvSensor_Update', 'AppUi_Render', 'Esp32Uart_ReadByte',
                    'Esp32Uart_SendString', 'OtaUart_Process')) {
    if ($manual -notmatch $api) {
        throw "Engineering manual is missing API explanation: $api"
    }
}
```

- [ ] **Step 5: Run the suite and record the expected red baseline**

Run from `huanjing_HAL`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run_all.ps1
```

Expected: the runner reports failures for the not-yet-implemented four-handle check, removal of `Esp32Uart_Process` / `Esp32Uart_ReadLine`, and updated engineering-manual content. Sensor configuration, sensor initialization, GPIO clock, and sensor-module checks remain green.

- [ ] **Step 6: Save a no-Git checkpoint**

Record the test names and failure reasons in the execution commentary. Do not initialize a repository and do not modify production code in this task.

---

### Task 2: Simplify ESP32 UART to One Interrupt-Driven Byte Stream

**Files:**
- Modify: `Core/Inc/esp32_uart.h`
- Modify: `Core/Src/esp32_uart.c`
- Verify: `user/OTA/Src/ota_port.c`
- Test: `tests/test_simple_app_modules.ps1`
- Test: `tests/test_esp32_uart_module.ps1`

**Interfaces:**
- Consumes: CubeMX global `huart2`, HAL APIs `HAL_UART_Receive_IT` and `HAL_UART_Transmit`.
- Produces: `void Esp32Uart_Init(void)`, `bool Esp32Uart_SendString(const char *text)`, and `bool Esp32Uart_ReadByte(uint8_t *byte)`; `OtaPort_ReadByte()` continues delegating to `Esp32Uart_ReadByte()`.

- [ ] **Step 1: Confirm old line APIs have no production callers**

Run:

```powershell
rg -n 'Esp32Uart_(Process|ReadLine)' . -g '*.c' -g '*.h' -g '*.cpp' -g '*.ino' -g '!tests/**' -g '!docs/**' -g '!output/**' -g '!tmp/**'
```

Expected: definitions and declarations only; no production caller. If a caller is found, stop this task and preserve that API until its behavior is routed through the OTA parser.

- [ ] **Step 2: Remove obsolete line-oriented tests by making production satisfy the red test**

Delete from `esp32_uart.c`:

- `ESP32_UART_LINE_SIZE`
- `g_received_line`
- `g_received_length`
- `g_line_ready`
- the complete `Esp32Uart_Process()` function
- the complete `Esp32Uart_ReadLine()` function

Remove the two matching declarations from `esp32_uart.h`.

- [ ] **Step 3: Rewrite the public header comments around the single receive path**

Use this public contract:

```c
/*
 * ESP32-C3 串口传输层。
 *
 * 本模块只负责 USART2 的字节收发，不解析 ENV、STATUS 或 OTA 协议。
 * 接收中断把字节写入环形缓冲区，OTA 任务通过 Esp32Uart_ReadByte()
 * 依次取走字节，因此系统中只有一个串口接收消费者。
 */
void Esp32Uart_Init(void);

/*
 * 发送以 '\0' 结尾的字符串，最长阻塞 100 ms。
 * text 不能为 NULL；成功完整交给 HAL 时返回 true。
 * 不能在中断服务函数中调用。
 */
bool Esp32Uart_SendString(const char *text);

/*
 * 非阻塞读取一个已接收字节。
 * byte 不能为 NULL；读到数据返回 true，没有数据立即返回 false。
 * 只能由一个任务消费，否则两个任务会分走同一条协议字节流。
 */
bool Esp32Uart_ReadByte(uint8_t *byte);
```

- [ ] **Step 4: Make the implementation small and consistently formatted**

Keep the existing 256-byte ring and overflow behavior. Normalize initialization and read-index code to:

```c
g_rx_write = 0U;
g_rx_read = 0U;
(void)HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1U);
```

```c
*byte = g_rx_buffer[g_rx_read];
g_rx_read = (uint16_t)((g_rx_read + 1U) % ESP32_UART_RX_BUFFER_SIZE);
return true;
```

Add comments explaining that the ISR is the only writer of `g_rx_write`, the OTA task is the only writer of `g_rx_read`, and the callback must immediately re-arm the one-byte interrupt.

- [ ] **Step 5: Run focused UART tests**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_simple_app_modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_esp32_uart_module.ps1
```

Expected: both pass; repository search finds no declarations or definitions of the removed line APIs outside historical design documents.

- [ ] **Step 6: Save a no-Git checkpoint**

Record the two passing test outputs and the production-reference search result.

---

### Task 3: Simplify FreeRTOS Task Bodies Without Changing Scheduling

**Files:**
- Modify: `Core/Src/freertos.c`
- Test: `tests/test_beginner_rtos_uart.ps1`
- Test: `tests/test_freertos_task_creation_check.ps1`
- Test: `tests/test_freertos_heap_capacity.ps1`
- Test: `tests/test_sensor_init_in_task.ps1`
- Test: `tests/test_real_sensor_configuration.ps1`

**Interfaces:**
- Consumes: `EnvData_t`, `Esp32Uart_SendString`, `OtaUart_IsBusy`, `OtaApp_IsConfirmationRequired`, `OtaApp_ConfirmRunning`, and `OtaPort_Send`.
- Produces: unchanged CMSIS task entry signatures and internal helpers `RtosData_IsEnvironmentReady`, `RtosData_SendEnvironment`, `RtosTasks_AreCreated`, and `RtosOta_ConfirmWhenStable`.

- [ ] **Step 1: Strengthen the task-creation test before changing production**

Ensure `test_freertos_task_creation_check.ps1` checks all four handles and requires `otaTaskHandle` to be validated. Run it:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_freertos_task_creation_check.ps1
```

Expected: fail because `otaTaskHandle` currently has no null check.

- [ ] **Step 2: Declare focused internal helpers in the FreeRTOS user-code prototype section**

Add these exact prototypes:

```c
static void RtosData_SaveEnvironment(void);
static void RtosData_CopyEnvironment(EnvData_t *environment);
static bool RtosData_IsEnvironmentReady(const EnvData_t *environment);
static void RtosData_SendEnvironment(const EnvData_t *environment);
static bool RtosTasks_AreCreated(void);
static bool RtosOta_ConfirmWhenStable(uint32_t boot_time);
```

- [ ] **Step 3: Replace four repeated task-handle checks with one readable guard**

Implement:

```c
static bool RtosTasks_AreCreated(void)
{
    return (sensortaskHandle != NULL) &&
           (uitaskHandle != NULL) &&
           (uarttaskHandle != NULL) &&
           (otaTaskHandle != NULL);
}
```

After all four `osThreadNew()` calls, use:

```c
if (!RtosTasks_AreCreated())
{
    Error_Handler();
}
```

Update the structural test to accept the consolidated helper by requiring all four handle comparisons inside `RtosTasks_AreCreated()` and one `if (!RtosTasks_AreCreated())` error guard.

- [ ] **Step 4: Extract environment readiness and ENV formatting from the UART task**

Implement readiness as:

```c
static bool RtosData_IsEnvironmentReady(const EnvData_t *environment)
{
    return (environment != NULL) &&
           environment->temperature_valid &&
           environment->humidity_valid &&
           environment->light_valid &&
           environment->air_valid;
}
```

Move the existing `temperature_abs`, `snprintf`, bounds check and `Esp32Uart_SendString()` into `RtosData_SendEnvironment()`. Preserve the exact format:

```c
"ENV,%s%ld.%ld,%u.%u,%u,%u\r\n"
```

The UART task loop becomes:

```c
for (;;)
{
    RtosData_CopyEnvironment(&environment);

    if (RtosData_IsEnvironmentReady(&environment) && !OtaUart_IsBusy())
    {
        RtosData_SendEnvironment(&environment);
    }

    osDelay(1000);
}
```

- [ ] **Step 5: Extract the 10-second OTA confirmation decision**

Implement `RtosOta_ConfirmWhenStable()` so it returns `true` only after a required image has run for 10000 ms and `OtaApp_ConfirmRunning()` succeeds. Preserve `BOOT_OK,%s\r\n` and `OTA_CURRENT_VERSION`. The OTA task keeps a local `confirmed` flag:

```c
if (!confirmed && RtosOta_ConfirmWhenStable(boot_time))
{
    confirmed = true;
}
```

Keep `OtaUart_Process()` before the confirmation check and keep `osDelay(1)` at the end of the loop.

- [ ] **Step 6: Add beginner-oriented concurrency and timing comments**

Document in `freertos.c`:

- why sensor hardware initialization occurs after scheduler start;
- why the shared `EnvData_t` copy uses a short critical section;
- why LCD drawing and UART formatting occur after copying to local variables;
- why ENV transmission is suppressed while OTA owns the protocol stream;
- why OTA processing uses 1 ms and why new firmware is confirmed after 10 seconds.

Do not change thread attributes, task delays, startup order, or critical-section boundaries.

- [ ] **Step 7: Run all focused FreeRTOS tests**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_beginner_rtos_uart.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_freertos_task_creation_check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_freertos_heap_capacity.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_sensor_init_in_task.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_real_sensor_configuration.ps1
```

Expected: all five pass. Confirm by search that stack sizes and delays retain their required numeric values.

- [ ] **Step 8: Save a no-Git checkpoint**

Record all five outputs and a concise before/after summary of the task loops.

---

### Task 4: Simplify LCD Rendering and Document the UI Contract

**Files:**
- Modify: `Core/Inc/app_ui.h`
- Modify: `Core/Src/app_ui.c`
- Test: `tests/test_lcd_ota_ui.ps1`

**Interfaces:**
- Consumes: immutable `EnvData_t` and `Esp32Status_t` snapshots.
- Produces: unchanged public APIs `AppUi_Init`, `AppUi_TogglePage`, and `AppUi_Render`.

- [ ] **Step 1: Add a structural test for a named page type and one no-data helper**

Append to `test_lcd_ota_ui.ps1`:

```powershell
if ($source -notmatch 'typedef\s+enum[\s\S]*APP_UI_PAGE_ENVIRONMENT[\s\S]*APP_UI_PAGE_SYSTEM_STATUS[\s\S]*AppUiPage_t') {
    throw 'UI pages must use the readable AppUiPage_t enum.'
}
if ($source -notmatch 'static\s+void\s+AppUi_WriteValueOrNoData\s*\(') {
    throw 'Sensor cards must share one valid/no-data drawing helper.'
}
```

Run the test and expect failure because pages are currently numeric macros and each card repeats an `else` branch.

- [ ] **Step 2: Replace page macros with an enum**

Use:

```c
typedef enum
{
    APP_UI_PAGE_ENVIRONMENT = 0,
    APP_UI_PAGE_SYSTEM_STATUS
} AppUiPage_t;

static AppUiPage_t g_current_page;
```

Preserve the existing toggle behavior and redraw flag.

- [ ] **Step 3: Centralize valid/no-data card drawing**

Add:

```c
static void AppUi_WriteValueOrNoData(uint16_t x, uint16_t y, bool valid,
                                     const char *value, uint16_t value_color)
{
    AppUi_WriteCardValue(x, y,
                         valid ? value : "NO DATA",
                         valid ? value_color : GRAY);
}
```

For each sensor, format `text` only when its valid flag is true, then call this helper exactly once. Preserve temperature `C`, humidity `%`, light `lx`, air `ppm`, card positions and colors.

- [ ] **Step 4: Expand file and public-interface comments**

In `app_ui.h`, document that LCD hardware must already be initialized, both input pointers must be non-null, the function is task-context only, and the caller owns both snapshots. In `app_ui.c`, explain fixed-layout drawing versus variable-value drawing, old-value caching, why `memcmp` is safe for initialized snapshots, and why the module avoids full-screen redraws.

- [ ] **Step 5: Run the UI test**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_lcd_ota_ui.ps1
```

Expected: pass with `SYSTEM STATUS`, Wi-Fi, MQTT, OTA progress, version, enum, footer helper and valid/no-data helper all present.

- [ ] **Step 6: Save a no-Git checkpoint**

Record the passing output and verify no LCD coordinate, color constant or user-facing status text was changed accidentally.

---

### Task 5: Simplify OTA UART Helpers While Preserving the Wire Protocol

**Files:**
- Modify: `user/OTA/Inc/ota_uart.h`
- Modify: `user/OTA/Src/ota_uart.c`
- Create: `tests/test_ota_uart_contract.ps1`
- Create: `tests/test_ota_uart_compile.c`

**Interfaces:**
- Consumes: `OtaApp_Begin`, `OtaApp_Write`, `OtaApp_Finalize`, `OtaApp_Abort`, `OtaPort_ReadByte`, `OtaPort_Send`, and `OtaPort_ResetToBootloader`.
- Produces: unchanged OTA public API and byte-for-byte-compatible control responses and binary frames.

- [ ] **Step 1: Write a protocol-contract test before editing the parser**

Create `tests/test_ota_uart_contract.ps1` that loads `ota_uart.h` and `ota_uart.c`, then requires:

```powershell
$requiredTokens = @(
    '0xA5U', '0x5AU', '512U', 'OTA_BEGIN,', 'OTA_END', 'OTA_ABORT',
    'OTA_READY', 'OTA_ACK', 'OTA_NACK', 'OTA_OK',
    'OTA_ERROR,begin', 'OTA_ERROR,slot', 'OTA_ERROR,verify', 'OTA_ERROR,abort')

foreach ($token in $requiredTokens) {
    if (($header + $source) -notmatch [regex]::Escape($token)) {
        throw "OTA wire-contract token is missing: $token"
    }
}

if ($source -match 'crc\s*=\s*OtaUart_Crc16\(bytes' -or
    $source -match 'crc\s*=\s*OtaUart_Crc16\(payload') {
    throw 'Dead intermediate CRC calculations remain in frame construction.'
}
if ($source -notmatch '"%s,%u"') {
    throw 'Sequence ACK/NACK responses must share the bounded formatter.'
}
```

Run it and expect failure on the two dead CRC assignments.

- [ ] **Step 2: Add an ARMCC compile test for host-test-visible CRC helpers**

Create `tests/test_ota_uart_compile.c`:

```c
#include <assert.h>
#include <stdint.h>
#include "ota_uart.h"

int main(void)
{
    static const uint8_t modbus_vector[] = "123456789";
    uint8_t payload[4] = {0x01U, 0x02U, 0x03U, 0x04U};
    uint8_t frame[OTA_UART_FRAME_OVERHEAD + sizeof(payload)];
    uint32_t size;

    assert(OtaUart_Crc16(modbus_vector, 9U) == 0x4B37U);
    size = OtaUart_BuildFrameForTest(7U, payload, sizeof(payload),
                                     frame, sizeof(frame));
    assert(size == sizeof(frame));
    assert(OtaUart_FrameIsValidForTest(frame, size));
    frame[6] ^= 0x01U;
    assert(!OtaUart_FrameIsValidForTest(frame, size));
    return 0;
}
```

Compile both translation units for ARM to validate declarations and warnings:

```powershell
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST `
  -I .\user\OTA\Inc .\user\OTA\Src\ota_uart.c -o .\tmp\ota_uart_host_test.o
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST `
  -I .\user\OTA\Inc .\tests\test_ota_uart_compile.c -o .\tmp\test_ota_uart_compile.o
```

Expected: both object files compile. The assertions are also protected by the full application build; ARMCC output is not executed on the Windows host.

- [ ] **Step 3: Remove dead CRC work and unused variables**

In `OtaUart_BuildFrameForTest()`, remove `uint8_t bytes[4]` and both overwritten CRC assignments. Keep only:

```c
crc = OtaUart_Crc16(&frame[2], (uint32_t)length + 4U);
OtaUart_WriteLe16(&frame[6U + length], crc);
```

- [ ] **Step 4: Centralize sequence responses without hiding state transitions**

Add:

```c
static void OtaUart_SendSequenceResponse(const char *name, uint16_t sequence)
{
    char response[32];
    int length;

    length = snprintf(response, sizeof(response), "%s,%u",
                      name, (unsigned int)sequence);
    if ((length > 0) && ((uint32_t)length < sizeof(response)))
    {
        OtaUart_SendLine(response);
    }
}
```

Use `OtaUart_SendSequenceResponse("OTA_NACK", g_ota_uart.expected_sequence)` for sequence/write/CRC failure and `OtaUart_SendSequenceResponse("OTA_ACK", g_ota_uart.expected_sequence)` after a successful write. Keep sequence increment after the ACK is sent.

- [ ] **Step 5: Add protocol and safety comments**

Document in the header and implementation:

- frame layout `[A5 5A][seq LE16][length LE16][payload][CRC LE16]`;
- CRC coverage begins at sequence and excludes sync and stored CRC;
- why image length and frame payload must be even for STM32F1 half-word writes;
- why control lines are accepted while idle and only `OTA_END` / `OTA_ABORT` are accepted while busy;
- why Flash writing and reset never occur in the UART interrupt;
- why `reset_pending` delays reset until after `OTA_OK` has been sent.

- [ ] **Step 6: Run OTA contract and compile checks**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_ota_uart_contract.ps1
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST -I .\user\OTA\Inc .\user\OTA\Src\ota_uart.c -o .\tmp\ota_uart_host_test.o
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST -I .\user\OTA\Inc .\tests\test_ota_uart_compile.c -o .\tmp\test_ota_uart_compile.o
```

Expected: protocol contract passes and both compilations finish without errors.

- [ ] **Step 7: Save a no-Git checkpoint**

Record the contract output, ARMCC output and exact protocol strings retained.

---

### Task 6: Add Detailed Module and Interface Comments Across Application-Owned Code

**Files:**
- Modify: `Core/Inc/env_sensor.h`, `Core/Src/env_sensor.c`
- Modify: `Core/Inc/esp32_status.h`, `Core/Src/esp32_status.c`
- Modify: `Core/Inc/app_key.h`, `Core/Src/app_key.c`
- Modify: `Core/Inc/dht11.h`, `Core/Src/dht11.c`
- Modify: `Core/Inc/bh1750.h`, `Core/Src/bh1750.c`
- Modify: `Core/Inc/mq135.h`, `Core/Src/mq135.c`
- Modify: `Core/Inc/lcd.h`, `Core/Src/lcd.c`
- Modify: `user/OTA/Inc/sha256.h`, `user/OTA/Src/sha256.c`
- Modify: `user/OTA/Inc/ota_layout.h`
- Modify: `user/OTA/Inc/ota_port_config.h`
- Modify: `user/OTA/Inc/ota_app.h`, `user/OTA/Src/ota_app.c`
- Modify: `user/OTA/Inc/ota_storage.h`, `user/OTA/Src/ota_storage.c`
- Modify: `user/OTA/Inc/ota_metadata.h`, `user/OTA/Src/ota_metadata.c`
- Modify: `user/OTA/Inc/ota_integrity.h`, `user/OTA/Src/ota_integrity.c`
- Modify: `user/OTA/Inc/ota_cloud.h`, `user/OTA/Src/ota_cloud.c`
- Modify: `user/OTA/Inc/ota_port.h`, `user/OTA/Src/ota_port.c`
- Test: `tests/test_simple_sensor_modules.ps1`
- Create: `tests/test_public_api_comments.ps1`

**Interfaces:**
- Consumes: all existing public signatures; no public signature or data layout changes are permitted in this task.
- Produces: module-level responsibility comments and public API contracts covering context, blocking, units, ownership and safety constraints.

- [ ] **Step 1: Create a comment-coverage structural test**

Create `tests/test_public_api_comments.ps1` with an explicit file/API map:

```powershell
$contracts = @(
    @{ File = 'env_sensor.h'; APIs = @('EnvSensor_Init', 'EnvSensor_Update', 'EnvSensor_GetData') },
    @{ File = 'esp32_status.h'; APIs = @('Esp32Status_Init', 'Esp32Status_ProcessLine', 'Esp32Status_Get') },
    @{ File = 'app_key.h'; APIs = @('AppKey_Init', 'AppKey_PageTogglePressed') },
    @{ File = 'dht11.h'; APIs = @('Dht11_Init', 'Dht11_Read') },
    @{ File = 'bh1750.h'; APIs = @('Bh1750_Init', 'Bh1750_ReadLux') },
    @{ File = 'mq135.h'; APIs = @('Mq135_Read') })

foreach ($contract in $contracts) {
    $text = Get-Content -Raw (Join-Path $PSScriptRoot "..\Core\Inc\$($contract.File)")
    foreach ($api in $contract.APIs) {
        $pattern = '(?s)/\*\*?.{20,400}?\*/\s*[^;]*\b' + [regex]::Escape($api) + '\s*\('
        if ($text -notmatch $pattern) {
            throw "$($contract.File) needs a detailed contract before $api."
        }
    }
}
```

Add an OTA header map for `OtaApp_`, `OtaStorage_`, `OtaMetadata_`, `OtaIntegrity_`, `OtaCloud_` and `OtaPort_` public declarations using the same 20-to-400-character preceding-comment rule.

Add `Sha256_Init`, `Sha256_Update`, `Sha256_Final` and `Sha256_HexEquals` to the OTA map. For `lcd.h`, check the foundational APIs `LCD_Init`, `LCD_SetArea`, `LCD_ClearAll`, `LCD_WriteAsciiString` and `LCD_DrawRectangle`; the remaining drawing declarations are reviewed manually because many share the same coordinate/color contract.

- [ ] **Step 2: Document sensor units, timing and ownership**

For `EnvData_t`, preserve field order and document temperature/humidity tenths, lux, ppm estimate and per-reading validity. For DHT11, explain the microsecond timing critical section and minimum sampling interval. For BH1750, explain I2C address/measurement conversion and false return on bus failure. For MQ135, explain ADC channel, estimated display value and that it is not a calibrated safety instrument.

Each public function comment must state whether it initializes hardware, may block, returns a copied value or internal pointer, and whether task context is required.

For `lcd.h` and `lcd.c`, replace the empty legacy file description with a module boundary explaining the FSMC memory-mapped command/data addresses, the 320×480 coordinate system, RGB565 colors, and task-context-only drawing. Document coordinate, width, height, line-width and foreground/background color units on the foundational public APIs. Keep every drawing algorithm and register value unchanged.

- [ ] **Step 3: Document key debounce and status concurrency**

For `AppKey_PageTogglePressed()`, explain active level, 30 ms debounce and one-event-per-press behavior. For `Esp32Status_ProcessLine()`, document the exact six fields, value ranges and rejection rules. For `Esp32Status_Get()`, explain that it returns a critical-section-protected copy safe for the UI task.

- [ ] **Step 4: Document OTA layers without changing behavior**

Add file-level boundary comments:

- `ota_app`: one upgrade transaction and the running/target-slot safety check;
- `ota_storage`: STM32F1 erase/program alignment and address-range enforcement;
- `ota_metadata`: redundant/persistent boot decision data and update ordering;
- `ota_integrity`: streamed SHA-256 verification of the written slot;
- `ota_cloud`: restricted JSON extraction rather than a general JSON parser;
- `ota_port`: the only board-specific UART/reset/status bridge.

For `sha256`, document the incremental context lifecycle (`Init` → one or more `Update` calls → `Final`) and the 32-byte binary versus 64-character hexadecimal representations. For `ota_layout.h`, document that addresses and sizes are a persistent compatibility contract shared by Bootloader and both application targets. For `ota_port_config.h`, document which macros vary between AppSlotA and AppSlotB builds.

Every public declaration receives parameter units, accepted values, return meaning, call context and ownership notes. Do not alter structs, enum numeric values, constants or function bodies except whitespace required to attach comments.

- [ ] **Step 5: Run comment and sensor/status tests**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_public_api_comments.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_simple_sensor_modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_real_sensor_configuration.ps1
```

Expected: all pass. Use `rg -n '^\s*//|/\*'` only as an inspection aid; acceptance depends on documented public contracts and human review, not raw comment count.

- [ ] **Step 6: Save a no-Git checkpoint**

Record passing tests and review the diff manually to ensure comments describe current code rather than intended future behavior.

---

### Task 7: Establish One OTA Maintenance Source and a Checked Portable Export

**Files:**
- Create: `portable_ota/tools/sync_ota_core.ps1`
- Modify: `portable_ota/README.md`
- Modify: files under `portable_ota/stm32/ota_core` through the sync script
- Create: `huanjing_HAL/tests/test_ota_source_sync.ps1`

**Interfaces:**
- Consumes: canonical files in `huanjing_HAL/user/OTA/Inc` and `huanjing_HAL/user/OTA/Src`.
- Produces: `sync_ota_core.ps1 [-Check]`, where default mode copies common files and `-Check` performs read-only SHA-256 comparison; `Src/ota_port.c` remains platform-specific and excluded.

- [ ] **Step 1: Write the failing consistency test**

Create `huanjing_HAL/tests/test_ota_source_sync.ps1`:

```powershell
$script = Join-Path $PSScriptRoot '..\..\portable_ota\tools\sync_ota_core.ps1'
if (-not (Test-Path -LiteralPath $script)) {
    throw "OTA sync tool is missing: $script"
}
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -Check
if ($LASTEXITCODE -ne 0) {
    throw 'Portable OTA source differs from the canonical project source.'
}
Write-Host 'OTA source synchronization check passed.'
```

Run it and expect failure because the sync tool does not exist yet.

- [ ] **Step 2: Implement explicit, safe source-to-export mapping**

Create `portable_ota/tools/sync_ota_core.ps1` with:

```powershell
param([switch]$Check)
$ErrorActionPreference = 'Stop'

$portableRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspaceRoot = (Resolve-Path (Join-Path $portableRoot '..')).Path
$sourceRoot = Join-Path $workspaceRoot 'huanjing_HAL\user\OTA'
$targetRoot = Join-Path $portableRoot 'stm32\ota_core'

$relativeFiles = @(
    'Inc\sha256.h', 'Inc\ota_uart.h', 'Inc\ota_storage.h',
    'Inc\ota_port_config.h', 'Inc\ota_port.h', 'Inc\ota_metadata.h',
    'Inc\ota_layout.h', 'Inc\ota_integrity.h', 'Inc\ota_cloud.h', 'Inc\ota_app.h',
    'Src\sha256.c', 'Src\ota_uart.c', 'Src\ota_storage.c',
    'Src\ota_metadata.c', 'Src\ota_integrity.c', 'Src\ota_cloud.c', 'Src\ota_app.c')

$different = @()
foreach ($relative in $relativeFiles) {
    $source = Join-Path $sourceRoot $relative
    $target = Join-Path $targetRoot $relative
    if (-not (Test-Path -LiteralPath $source) -or -not (Test-Path -LiteralPath $target)) {
        throw "Mapped OTA file is missing: $relative"
    }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) {
        if ($Check) {
            $different += $relative
        } else {
            Copy-Item -LiteralPath $source -Destination $target -Force
        }
    }
}

if ($different.Count -ne 0) {
    throw "Different OTA export files: $($different -join ', ')"
}

if ($Check) {
    Write-Host 'OTA export matches canonical source.'
} else {
    Write-Host 'OTA export synchronized.'
}
```

- [ ] **Step 3: Synchronize and verify**

Run from the workspace root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\portable_ota\tools\sync_ota_core.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\portable_ota\tools\sync_ota_core.ps1 -Check
powershell -NoProfile -ExecutionPolicy Bypass -File .\huanjing_HAL\tests\test_ota_source_sync.ps1
```

Expected: all report a matching export. Confirm `portable_ota/stm32/ota_core/Src/ota_port.c` was not overwritten.

- [ ] **Step 4: Document maintenance ownership**

Add a section to `portable_ota/README.md` stating that common OTA code is maintained in `huanjing_HAL/user/OTA`, the portable directory is an export, `sync_ota_core.ps1` refreshes it, `-Check` is read-only, and `Src/ota_port.c` is intentionally platform-specific.

- [ ] **Step 5: Save a no-Git checkpoint**

Record the synchronization outputs and hashes for one representative header and source file.

---

### Task 8: Simplify and Document the ESP32-C3 OTA Gateway Loop

**Files:**
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/esp32c3_ota_gateway.ino`
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/huawei_iotda.h`
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/huawei_iotda.cpp`
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/stm32_ota_link.h`
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/stm32_ota_link.cpp`
- Modify: `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/ota_gateway_config.h`
- Create: `tests/test_esp32_ota_gateway.ps1`

**Interfaces:**
- Consumes: existing `HuaweiIotda`, `Stm32OtaLink`, Arduino `HTTPClient`, `WiFiClientSecure`, UART1 and `millis()` APIs.
- Produces: unchanged cloud/UART behavior with a short Arduino `loop()` that calls `huawei.update()`, `processStm32Lines()`, `processPendingOta()` and `sendPeriodicStatus()`.

- [ ] **Step 1: Write a gateway structure and protocol test**

Create `tests/test_esp32_ota_gateway.ps1`:

```powershell
$gatewayRoot = Join-Path $PSScriptRoot '..\..\portable_ota\esp32c3_arduino_gateway\esp32c3_ota_gateway'
$sketch = Get-Content -Raw (Join-Path $gatewayRoot 'esp32c3_ota_gateway.ino')
$link = Get-Content -Raw (Join-Path $gatewayRoot 'stm32_ota_link.cpp')

foreach ($token in @('ENV,', 'BOOT_OK,', 'STATUS,%u,%u,%u,%u,%u,%s',
                      'OTA_BEGIN,', 'OTA_END', 'OTA_ABORT',
                      'OTA_READY', 'OTA_ACK,', 'OTA_OK')) {
    if (($sketch + $link) -notmatch [regex]::Escape($token)) {
        throw "ESP32/STM32 protocol token is missing: $token"
    }
}

foreach ($helper in @('processStm32Lines', 'processPendingOta', 'sendPeriodicStatus')) {
    if ($sketch -notmatch "void\s+$helper\s*\(") {
        throw "Gateway loop helper is missing: $helper"
    }
}

$loopMatch = [regex]::Match($sketch, '(?s)void\s+loop\s*\(\s*\)\s*\{(?<body>.*?)\n\}')
if (-not $loopMatch.Success) {
    throw 'Cannot locate the Arduino loop function.'
}
foreach ($call in @('huawei.update()', 'processStm32Lines()',
                     'processPendingOta()', 'sendPeriodicStatus()')) {
    if ($loopMatch.Groups['body'].Value -notmatch [regex]::Escape($call)) {
        throw "Arduino loop is missing orchestration call: $call"
    }
}
```

Run it and expect failure because the pending-package and periodic-status logic is currently embedded in `loop()`.

- [ ] **Step 2: Extract the pending OTA transaction without changing its sequence**

Move the existing `Stm32OtaPackage package` block into:

```cpp
void processPendingOta()
{
  Stm32OtaPackage package;

  if (otaActive || !huawei.takePendingPackage(package))
  {
    return;
  }

  String failureReason = "unknown";
  otaActive = true;
  otaProgress = 0U;
  otaTargetVersion = package.version;
  sendStatus("ota_start");

  if (downloadAndTransfer(package, failureReason))
  {
    otaProgress = 99U;
    otaWaitingForBoot = true;
    huawei.reportProgress(package.version, 99U, 0, "transferred_rebooting");
    sendStatus("rebooting");
  }
  else
  {
    huawei.reportProgress(package.version, otaProgress,
                          otaResultCodeForFailure(failureReason), failureReason);
    sendStatus(failureReason.c_str());
  }

  otaActive = false;
}
```

Retain comments explaining that 99% means transferred but not boot-confirmed, while 100% is reported only after the matching `BOOT_OK` line.

- [ ] **Step 3: Extract periodic status and reduce `loop()` to orchestration**

Add:

```cpp
void sendPeriodicStatus()
{
  if ((millis() - lastStatusMs) < 2000U)
  {
    return;
  }

  lastStatusMs = millis();
  sendStatus(otaActive ? "downloading" : "ready");
}
```

The final loop must be:

```cpp
void loop()
{
  huawei.update();
  processStm32Lines();
  processPendingOta();
  sendPeriodicStatus();
}
```

- [ ] **Step 4: Document the gateway class contracts and security boundary**

In `huawei_iotda.h`, document connection maintenance, pending-package ownership, environment-line format and progress result semantics. In `stm32_ota_link.h`, document UART pins/baud initialization, blocking transfer calls, frame retry behavior, and the lifetime of `Stm32OtaPackage` strings. In `ota_gateway_config.h`, state that Wi-Fi/cloud credentials are configuration data, must never be printed, and should be replaced before sharing a firmware package; do not change credential values or connection behavior in this refactor.

In the `.cpp` files, add comments for cloud event flow, HTTP stream ownership, frame retry/timeout boundaries and why TLS certificate policy is a deployment decision. Do not change MQTT topics, JSON fields, HTTP authorization, retry counts, timeouts, UART framing or cloud result codes.

- [ ] **Step 5: Run the structural gateway test and record toolchain limitation**

Run from `huanjing_HAL`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_esp32_ota_gateway.ps1
```

Expected: pass. `arduino-cli` is not installed in the current environment, so record that the sketch receives structural/protocol validation here and requires an Arduino IDE compile during hardware validation.

- [ ] **Step 6: Save a no-Git checkpoint**

Record the passing output and verify by text comparison that progress values 99/100, the 2000 ms status interval and all cloud result codes are unchanged.

---

### Task 9: Update Engineering Documentation to Match the Simplified Code

**Files:**
- Modify: `工程说明.md`
- Modify: `OTA_升级完整流程与问题排查手册.md`
- Modify: `README.md`
- Test: `tests/test_engineering_manual.ps1`

**Interfaces:**
- Consumes: final public APIs and task flow from Tasks 2–7.
- Produces: beginner-facing documentation that matches the code and no longer teaches the removed line-oriented UART API or old three-task architecture.

- [ ] **Step 1: Update the engineering manual architecture sections**

Rename “FreeRTOS 三任务” to “FreeRTOS 四任务” and document:

```text
传感器任务 -> 更新受保护的 EnvData_t 快照
UI 任务     -> 复制快照并刷新 LCD
遥测任务   -> 复制快照并发送 ENV；OTA 忙时暂停
OTA 任务   -> 消费 USART2 字节流、解析 STATUS/OTA、确认新固件
```

Replace `Esp32Uart_Process` and `Esp32Uart_ReadLine` explanations with `Esp32Uart_ReadByte` and the interrupt ring-buffer path. Replace the old OTA reserved-page section with the current system-status page and A/B OTA overview.

- [ ] **Step 2: Update the OTA troubleshooting manual**

Keep its existing “one USART2 consumer” explanation, but align names with final code: ISR writes the ring, `OtaPort_ReadByte()` delegates to `Esp32Uart_ReadByte()`, and `OtaUart_Process()` separates idle text lines from busy binary frames. Add a concise frame diagram and state that `STATUS` is handled while idle.

- [ ] **Step 3: Update the root README quick-start description**

List four tasks, two LCD pages, UART ring-buffer reception, A/B OTA, and the canonical/export OTA source relationship. Do not add new setup requirements or claim hardware validation that was not performed.

- [ ] **Step 4: Run the documentation test and stale-reference search**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\test_engineering_manual.ps1
rg -n 'FreeRTOS 三任务|OTA RESERVED|Esp32Uart_Process|Esp32Uart_ReadLine' .\工程说明.md .\OTA_升级完整流程与问题排查手册.md .\README.md
```

Expected: documentation test passes and the stale-reference search returns no matches.

- [ ] **Step 5: Save a no-Git checkpoint**

Record the passing output and list the renamed architecture sections.

---

### Task 10: Full Keil Builds, Regression Suite, and Final Safety Review

**Files:**
- Verify: all files changed in Tasks 1–9
- Generate: `MDK-ARM/AppSlotA_simplification_build.log`
- Generate: `MDK-ARM/AppSlotB_simplification_build.log`
- Generate: `MDK-ARM/Bootloader_simplification_build.log`

**Interfaces:**
- Consumes: completed simplified application and synchronized OTA export.
- Produces: passing PowerShell suite, successful Keil builds, protocol/reference audit, and a final change summary with any unverified hardware-only behavior explicitly identified.

- [ ] **Step 1: Run every PowerShell regression test**

Run from `huanjing_HAL`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\run_all.ps1
```

Expected: `ALL POWERSHELL TESTS PASSED`.

- [ ] **Step 2: Compile the OTA host-test translation units**

Run:

```powershell
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST -I .\user\OTA\Inc .\user\OTA\Src\ota_uart.c -o .\tmp\ota_uart_host_test.o
& 'C:\Keil_v5\ARM\ARMCC\bin\armcc.exe' -c --c99 --cpu Cortex-M3 -DOTA_UART_HOST_TEST -I .\user\OTA\Inc .\tests\test_ota_uart_compile.c -o .\tmp\test_ota_uart_compile.o
```

Expected: both return exit code 0.

- [ ] **Step 3: Build AppSlotA, AppSlotB and Bootloader**

Run from `huanjing_HAL/MDK-ARM`:

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -j0 -b .\AppSlotA.uvprojx -o .\AppSlotA_simplification_build.log
& 'C:\Keil_v5\UV4\UV4.exe' -j0 -b .\AppSlotB.uvprojx -o .\AppSlotB_simplification_build.log
& 'C:\Keil_v5\UV4\UV4.exe' -j0 -b .\Bootloader.uvprojx -o .\Bootloader_simplification_build.log
```

Inspect:

```powershell
rg -n 'Error:|Warning:|[1-9][0-9]* Error\(s\)|[1-9][0-9]* Warning\(s\)' `
  .\AppSlotA_simplification_build.log `
  .\AppSlotB_simplification_build.log `
  .\Bootloader_simplification_build.log
```

Expected: all projects build with zero errors and no new warnings attributable to changed files. Existing unrelated warnings, if any, must be quoted and distinguished from new warnings.

- [ ] **Step 4: Audit protected constants and protocol strings**

Run from `huanjing_HAL`:

```powershell
rg -n 'stack_size|osDelay\(|10000U|ENV,%s%ld\.%ld|BOOT_OK,%s' .\Core\Src\freertos.c
rg -n 'OTA_UART_SYNC_|OTA_UART_MAX_PAYLOAD|OTA_UART_FRAME_OVERHEAD' .\user\OTA\Inc\ota_uart.h
rg -n 'OTA_READY|OTA_ACK|OTA_NACK|OTA_OK|OTA_ERROR' .\user\OTA\Src\ota_uart.c
powershell -NoProfile -ExecutionPolicy Bypass -File ..\portable_ota\tools\sync_ota_core.ps1 -Check
```

Expected: all protected timing, stack and protocol constants remain present and OTA sources match.

- [ ] **Step 5: Perform final readability and comment review**

Inspect every changed project-owned `.c` and `.h` file and confirm:

- each file has one stated responsibility;
- public functions describe parameters, return values, blocking/context and ownership where relevant;
- comments explain constraints rather than restating assignments;
- no disabled old implementation remains in comments;
- no abstraction introduces more branches or indirection than it removes;
- CubeMX changes remain inside `USER CODE` regions except documented header/source modules that are entirely user-owned.

- [ ] **Step 6: Produce the final handoff**

Report modified files grouped by FreeRTOS, UART, UI, sensors/status, OTA, tests and documentation. Include exact test/build commands and outcomes. State that hardware-in-loop validation still requires flashing AppSlotA/AppSlotB and exercising sensors, KEY1, ESP32 status, ENV upload and one OTA upgrade if that physical test was not performed.
