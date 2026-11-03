# 自写模块简化与中文注释 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变硬件引脚、任务周期和显示功能的前提下，简化所有自写业务模块并补充面向初学者的中文注释。

**Architecture:** 每个传感器模块只保留初始化和读取接口，`env_sensor` 负责汇总数据，按键、UI、UART 各自只处理单一职责。CubeMX 管理的外设初始化文件不加入业务代码；用户逻辑留在独立模块或 `USER CODE` 区。

**Tech Stack:** STM32 HAL、CMSIS-RTOS v2（FreeRTOS）、STM32CubeMX、Keil MDK、PowerShell。

## Global Constraints

- 保留 USART2 的 ESP32 普通文本通信，不加入网络、云平台或复杂协议。
- 不修改 `adc.c`、`i2c.c`、`usart.c`、`gpio.c` 的 CubeMX 生成逻辑。
- 不重写底层 `lcd.c/.h` 驱动。
- 传感器初始化仍只能在 `StartDefaultTask` 中调用。
- `.ioc` 保留三项任务与 `ProjectManager.KeepUserCode=true`。
- 所有新增或调整的注释使用中文；LCD 显示字符串继续使用 ASCII。

---

### Task 1: 为传感器模块建立简化接口检查

**Files:**
- Modify: `tests/test_real_sensor_configuration.ps1`
- Create: `tests/test_simple_sensor_modules.ps1`
- Modify: `Core/Inc/dht11.h`
- Modify: `Core/Inc/bh1750.h`
- Modify: `Core/Inc/mq135.h`
- Modify: `Core/Inc/env_sensor.h`
- Modify: `Core/Src/dht11.c`
- Modify: `Core/Src/bh1750.c`
- Modify: `Core/Src/mq135.c`
- Modify: `Core/Src/env_sensor.c`

**Interfaces:**
- Consumes: CubeMX 已生成的 `hi2c1`、`hadc1` 和 DHT11 GPIO 配置。
- Produces: `void Dht11_Init(void)`, `bool Dht11_Read(Dht11Data_t *data)`, `bool Bh1750_Init(void)`, `bool Bh1750_ReadLux(uint16_t *lux)`, `bool Mq135_Read(Mq135Data_t *data)`, `void EnvSensor_Init(void)`, `void EnvSensor_Update(void)`, `const EnvData_t *EnvSensor_GetData(void)`。

- [ ] **Step 1: 写入失败检查**

在 `test_simple_sensor_modules.ps1` 中读取四个头文件与源文件，要求每个头文件具有中文说明和上述公开函数；要求 `env_sensor.c` 调用三个真实传感器读取函数，且没有模拟数据关键字。

```powershell
foreach ($file in @('dht11', 'bh1750', 'mq135', 'env_sensor')) {
    $source = Get-Content -Raw "$PSScriptRoot\..\Core\Src\$file.c"
    $header = Get-Content -Raw "$PSScriptRoot\..\Core\Inc\$file.h"
    if ($header -notmatch '/\*' -or $source -notmatch '/\*') {
        throw "$file needs explanatory comments."
    }
}
```

- [ ] **Step 2: 运行检查并确认失败原因正确**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_simple_sensor_modules.ps1`

Expected: FAIL，因为现有模块注释或接口说明尚未达到检查条件。

- [ ] **Step 3: 简化并注释传感器模块**

保留现有公开函数签名。给结构体字段、初始化函数、读取函数和 DHT11 的三个时序辅助函数添加中文注释；删除无用的中间状态或重复注释。`EnvSensor_Update` 按下列顺序逐项读取并写有效标志：

```c
if (Dht11_Read(&dht_data)) {
    g_data.temperature_tenth = dht_data.temperature_tenth;
    g_data.humidity_tenth = dht_data.humidity_tenth;
    g_data.temperature_valid = true;
    g_data.humidity_valid = true;
}
```

读取失败时把相应有效标志置为 `false`，不伪造任何数值。

- [ ] **Step 4: 运行传感器检查**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_simple_sensor_modules.ps1; powershell -ExecutionPolicy Bypass -File .\tests\test_real_sensor_configuration.ps1`

Expected: 两项检查通过。

---

### Task 2: 简化按键和 ESP32 UART 模块说明

**Files:**
- Create: `tests/test_simple_app_modules.ps1`
- Modify: `Core/Inc/app_key.h`
- Modify: `Core/Src/app_key.c`
- Modify: `Core/Inc/esp32_uart.h`
- Modify: `Core/Src/esp32_uart.c`

**Interfaces:**
- Consumes: `HAL_GPIO_ReadPin`、`HAL_GetTick`、CubeMX 生成的 `huart2`。
- Produces: `void AppKey_Init(void)`, `bool AppKey_PageTogglePressed(void)`, `void Esp32Uart_Init(void)`, `void Esp32Uart_Process(void)`, `bool Esp32Uart_SendString(const char *text)`, `bool Esp32Uart_ReadLine(char *line, uint16_t line_size)`。

- [ ] **Step 1: 写入失败检查**

创建 `test_simple_app_modules.ps1`，要求按键模块只暴露两个公开函数、使用具名的 `APP_KEY_DEBOUNCE_MS` 消抖时间常量；UART 模块只暴露四个公开函数，并确认 UART 使用 `HAL_UART_Receive(&huart2, ..., 0)`。

```powershell
if ($uartSource -notmatch 'HAL_UART_Receive\s*\([^;]*?,\s*0\s*\)') {
    throw 'UART receive must stay non-blocking.'
}
if ($uartSource -match 'AT\+|MQTT|HTTP') {
    throw 'UART module must contain plain text communication only.'
}
if ($keySource -notmatch '#define\s+APP_KEY_DEBOUNCE_MS\s+30U') {
    throw 'Key debounce time must use a readable named constant.'
}
```

- [ ] **Step 2: 运行检查并确认失败原因正确**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_simple_app_modules.ps1`

Expected: FAIL，因为简化后的中文注释与接口限制尚未全部满足。

- [ ] **Step 3: 重写模块内注释，不改变公开接口**

`app_key.c` 用 `APP_KEY_DEBOUNCE_MS` 取代裸露的 `30U`，只保留“记录稳定电平”和“判断一次有效按下”的短流程，并解释 30 ms 消抖目的。`esp32_uart.c` 保留固定长度行缓存、零超时单字节接收、换行结束、字符串发送和安全复制；注释说明为什么 `Process` 不允许阻塞。

```c
/* 没有串口数据时立即返回，让 LCD 和传感器任务继续运行。 */
if (HAL_UART_Receive(&huart2, &received_byte, 1U, 0) != HAL_OK) {
    return;
}
```

- [ ] **Step 4: 运行模块检查**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_simple_app_modules.ps1; powershell -ExecutionPolicy Bypass -File .\tests\test_esp32_uart_module.ps1`

Expected: 两项检查通过。

---

### Task 3: 精简 UI 和 FreeRTOS 注释，并复核 CubeMX 安全位置

**Files:**
- Modify: `tests/test_beginner_rtos_uart.ps1`
- Modify: `tests/test_lcd_ota_ui.ps1`
- Modify: `Core/Inc/app_ui.h`
- Modify: `Core/Src/app_ui.c`
- Modify: `Core/Src/freertos.c`
- Verify: `42_lcd_hal.ioc`
- Verify: `Core/Src/main.c`

**Interfaces:**
- Consumes: `EnvData_t`、`AppKey_PageTogglePressed`、`Esp32Uart_Process`、LCD 底层绘图函数。
- Produces: `void AppUi_Init(void)`, `void AppUi_TogglePage(void)`, `void AppUi_Render(const EnvData_t *environment)`；三项 FreeRTOS 任务保持原有入口 `StartDefaultTask`、`StartTask02`、`StartTask03`。

- [ ] **Step 1: 写入失败检查**

修改 UI 与 FreeRTOS 检查，要求 `AppUi_Render` 只有一个 `EnvData_t` 参数、OTA 页面包含 `OTA RESERVED`，并要求 UI 通过 `AppUi_DrawFooter` 统一绘制页面底部提示；任务代码存在清晰的职责中文说明，并且 `main.c` 的 LCD 初始化仍位于 `USER CODE BEGIN 2`。

```powershell
if ($freertos -notmatch '任务一：采集' -or $freertos -notmatch '任务二：处理' -or $freertos -notmatch '任务三：与 Arduino ESP32') {
    throw 'FreeRTOS task roles need beginner-facing Chinese comments.'
}
if ($uiSource -notmatch 'static\s+void\s+AppUi_DrawFooter\s*\(') {
    throw 'UI footer must be drawn by one readable helper function.'
}
```

- [ ] **Step 2: 运行检查并确认失败原因正确**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_beginner_rtos_uart.ps1; powershell -ExecutionPolicy Bypass -File .\tests\test_lcd_ota_ui.ps1`

Expected: FAIL，仅当新增检查确实捕获缺失的简化说明时继续。

- [ ] **Step 3: 合并 UI 重复逻辑并补中文说明**

不改变 LCD 布局、按键切页或 OTA 预留页。新增 `AppUi_DrawFooter(const char *text)`，由环境页与 OTA 页共用，删除两处重复的底部灰条绘制代码；保留 `AppUi_FillRect`、`AppUi_WriteText`、`AppUi_DrawCard` 等必要私有函数；按“固定布局一次绘制、数值变化才刷新、切页时重画”的顺序重写注释。FreeRTOS 只调整注释和短辅助函数命名，不改变任务周期：传感器 1000 ms、UI 500 ms、UART 10 ms。

- [ ] **Step 4: 运行 UI 和任务检查**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_beginner_rtos_uart.ps1; powershell -ExecutionPolicy Bypass -File .\tests\test_lcd_ota_ui.ps1; powershell -ExecutionPolicy Bypass -File .\tests\test_sensor_init_in_task.ps1`

Expected: 三项检查通过。

---

### Task 4: 完整验证

**Files:**
- Verify: `tests/*.ps1`
- Verify: `MDK-ARM/42_lcd_hal.uvprojx`
- Verify: `42_lcd_hal.ioc`

**Interfaces:**
- Consumes: 前三项任务完成后的源码与检查脚本。
- Produces: 通过的完整检查结果与 Keil 构建日志。

- [ ] **Step 1: 运行所有 PowerShell 检查**

Run:

```powershell
Get-ChildItem .\tests\*.ps1 | Sort-Object Name | ForEach-Object {
    & powershell -ExecutionPolicy Bypass -File $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed: $($_.Name)" }
}
```

Expected: 每个检查脚本退出码为 0。

- [ ] **Step 2: 用 Keil 完整重建**

Run:

```powershell
Set-Location .\MDK-ARM
& 'C:\Keil_v5\UV4\UV4.exe' -r 42_lcd_hal.uvprojx -j0
```

Expected: 构建日志出现 `0 Error(s), 0 Warning(s)`。

- [ ] **Step 3: 复核 CubeMX 保护点**

Run:

```powershell
rg -n "ProjectManager.KeepUserCode=true|uarttask,8,256" .\42_lcd_hal.ioc
rg -n "USER CODE BEGIN Includes|USER CODE BEGIN 2" .\Core\Src\main.c
rg -n "USER CODE BEGIN Includes|USER CODE BEGIN StartDefaultTask|USER CODE BEGIN StartTask02|USER CODE BEGIN StartTask03" .\Core\Src\freertos.c
```

Expected: 三项命令均找到对应的保护点。
