# ESP32 状态显示 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 STM32 LCD 的 OTA 页面显示 ESP32-C3 网关状态。

**Architecture:** `esp32_status` 负责解析与保存文本状态；FreeRTOS UART 任务提交接收行；`app_ui` 读取状态副本并显示。CubeMX 文件只在 USER CODE 区加入调用。

**Tech Stack:** STM32 HAL、FreeRTOS、Keil MDK-ARM、C99。

## Global Constraints

- 不保留任何恢复或测试脚本。
- `Esp32Status_ProcessLine` 只解析 ESP32 网关定义的 `STATUS` 文本。
- UART 任务不得阻塞；LCD 任务不得直接访问 USART2。
- `freertos.c` 改动只在 `USER CODE` 区。

---

### Task 1: 新增状态解析模块

**Files:**
- Create: `Core/Inc/esp32_status.h`
- Create: `Core/Src/esp32_status.c`

**Interfaces:**
- Produces: `Esp32Status_Init(void)`、`bool Esp32Status_ProcessLine(const char *)`、`Esp32Status_t Esp32Status_Get(void)`。

- [ ] 解析严格的六字段 `STATUS` 行，字段范围错误时返回 `false`。
- [ ] 保存联网状态、OTA 活动状态、进度与短状态文本。

### Task 2: 接入 UART 任务与 LCD 页面

**Files:**
- Modify: `Core/Src/freertos.c` 的 USER CODE 区
- Modify: `Core/Inc/app_ui.h`
- Modify: `Core/Src/app_ui.c`

**Interfaces:**
- Consumes: `Esp32Uart_ReadLine()`、`Esp32Status_ProcessLine()`、`Esp32Status_Get()`。

- [ ] UART 任务每轮读取完整文本行并交给状态模块。
- [ ] OTA 页面显示 WAITING、CONNECTED、Wi-Fi、MQTT、OTA 状态和进度。

### Task 3: 加入 A/B 工程并验证

**Files:**
- Modify: `MDK-ARM/AppSlotA.uvprojx`
- Modify: `MDK-ARM/AppSlotB.uvprojx`

- [ ] 将 `esp32_status.c` 加入两个应用工程的 `Application/User/Core` 分组。
- [ ] 使用 Keil 分别编译 A/B，要求均为 `0 Error(s)`。
