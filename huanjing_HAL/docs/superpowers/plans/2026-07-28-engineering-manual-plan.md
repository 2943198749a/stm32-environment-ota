# 工程技术手册 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新建一份中文工程技术手册，既能指导初学者使用工程，也能解释当前架构的设计原因和作用。

**Architecture:** 手册以当前源码和 `.ioc` 为唯一事实来源，先说明工程和外设，再解释任务、模块和关键设计选择，最后给出 CubeMX 操作、调用示例和排错方法。文档不改变任何固件代码。

**Tech Stack:** Markdown、STM32 HAL、FreeRTOS、STM32CubeMX、Keil MDK。

## Global Constraints

- 正式文档必须创建在工程根目录，文件名为 `工程说明.md`。
- 只描述已经实现的功能；OTA 页面只能描述为预留页。
- ESP32 只能描述为 Arduino 程序通过 USART2 进行普通文本通信。
- 函数名、任务周期、引脚和配置必须与当前源码及 `.ioc` 一致。
- 明确区分 CubeMX 生成文件、独立用户模块和 `USER CODE` 区。

---

### Task 1: 建立手册内容检查

**Files:**
- Create: `tests/test_engineering_manual.ps1`
- Create: `工程说明.md`

**Interfaces:**
- Consumes: `Core/Src/freertos.c`、`42_lcd_hal.ioc`、自写模块头文件。
- Produces: 一份包含工程概览、三任务、传感器、LCD、按键、UART、CubeMX 说明的 Markdown 手册。

- [ ] **Step 1: 写入失败检查**

创建 `tests/test_engineering_manual.ps1`，检查根目录是否存在 `工程说明.md`，并要求文档含有以下文本：`FreeRTOS 三任务`、`DHT11`、`BH1750`、`MQ135`、`ESP32 UART`、`CubeMX`、`USER CODE`、`OTA RESERVED`。

```powershell
$manual = "$PSScriptRoot\..\工程说明.md"
if (-not (Test-Path $manual)) { throw 'Engineering manual is missing.' }
foreach ($section in @('FreeRTOS 三任务', 'DHT11', 'BH1750', 'MQ135', 'ESP32 UART', 'CubeMX', 'USER CODE', 'OTA RESERVED')) {
    if ((Get-Content -Raw $manual) -notmatch [regex]::Escape($section)) {
        throw "Engineering manual is missing section: $section"
    }
}
```

- [ ] **Step 2: 运行检查并确认失败原因正确**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_engineering_manual.ps1`

Expected: FAIL，因为根目录尚未创建 `工程说明.md`。

- [ ] **Step 3: 编写正式技术手册**

创建 `工程说明.md`，依次写入：工程概览、外设和引脚表、目录职责、快速使用、三任务数据流、临界区、三种传感器原理、LCD/按键/UART、CubeMX 安全修改、当前 API 示例、排错清单、OTA 扩展边界。说明三项任务周期分别为 1000 ms、500 ms、10 ms，并解释每一个周期的目的。

代码示例只使用现有 API：

```c
Esp32Uart_SendString("HELLO\n");
if (Esp32Uart_ReadLine(line, sizeof(line)))
{
    /* 使用 ESP32 返回的一行普通文本。 */
}
```

- [ ] **Step 4: 运行手册检查**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\test_engineering_manual.ps1`

Expected: PASS。

---

### Task 2: 交叉核对手册与工程配置

**Files:**
- Verify: `工程说明.md`
- Verify: `Core/Src/freertos.c`
- Verify: `42_lcd_hal.ioc`
- Verify: `Core/Inc/esp32_uart.h`

**Interfaces:**
- Consumes: Task 1 生成的手册与当前代码。
- Produces: 函数名、任务周期、CubeMX 保护点一致的手册。

- [ ] **Step 1: 检查任务和 UART 说明是否与源码一致**

Run:

```powershell
rg -n "osDelay\(1000\)|osDelay\(500\)|osDelay\(10\)|Esp32Uart_Process" .\Core\Src\freertos.c
rg -n "Esp32Uart_Init|Esp32Uart_Process|Esp32Uart_SendString|Esp32Uart_ReadLine" .\Core\Inc\esp32_uart.h
```

Expected: 三个延时和四个 UART 公开函数均被找到。

- [ ] **Step 2: 检查 CubeMX 保护点说明是否与配置一致**

Run:

```powershell
rg -n "ProjectManager.KeepUserCode=true|uarttask,8,256" .\42_lcd_hal.ioc
rg -n "USER CODE BEGIN Includes|USER CODE BEGIN 2" .\Core\Src\main.c
```

Expected: `.ioc` 与 `main.c` 均找到保护配置或用户代码区。

- [ ] **Step 3: 运行完整验证**

Run:

```powershell
Get-ChildItem .\tests\*.ps1 | Sort-Object Name | ForEach-Object {
    & powershell -ExecutionPolicy Bypass -File $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed: $($_.Name)" }
}
Set-Location .\MDK-ARM
& 'C:\Keil_v5\UV4\UV4.exe' -r 42_lcd_hal.uvprojx -j0
```

Expected: 所有检查通过，构建日志出现 `0 Error(s), 0 Warning(s)`。
