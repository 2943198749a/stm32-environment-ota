# 智能环境检测 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 STM32F103ZET6 HAL/LCD 工程实现可模拟运行的两页环境检测与 ESP32-C3 上传框架。

**Architecture:** 环境采集、按键、页面显示和 ESP32 AT 通信分别封装。main.c 以毫秒节拍调用模块；模块间通过环境数据结构和状态结构传递信息，不互相访问对方私有状态。

**Tech Stack:** STM32CubeMX 6.10、STM32F1 HAL、Keil MDK-ARM、C99 子集。

## Global Constraints

- MCU 为 STM32F103ZET6，系统时钟 72 MHz。
- LCD 已使用 FSMC，禁止修改 Core/Src/lcd.c。
- ESP32-C3 固定 USART2：PA2 TX、PA3 RX，115200 8N1。
- KEY1 为 PF8，上拉输入、按下低电平；PF11 对应 KEY4。
- DHT11 PC3，BH1750 I2C1 PB6/PB7，MQ-135 AO PC0/ADC1_IN10。
- 当前 `ENV_SENSOR_USE_SIMULATOR` 必须为 1；新函数均写中文注释。

---

### Task 1: 配置 HAL 外设与应用头文件

**Files:**
- Modify: `42_lcd_hal.ioc`, `Core/Src/gpio.c`, `Core/Src/usart.c`, `Core/Inc/usart.h`
- Create: `Core/Inc/env_sensor.h`, `Core/Inc/app_key.h`, `Core/Inc/esp32_c3.h`, `Core/Inc/app_ui.h`

**Interfaces:**
- Produces `MX_USART2_UART_Init()`, `huart2` 和模块公开函数声明。

- [x] 配置 USART2、ADC1、I2C1 和 PF8 外部输入，保留 LCD/FSMC/USART1 配置。
- [x] 声明 `EnvData_t`、`EnvSensor_Init()`、`EnvSensor_Update()`；声明页面、按键和 ESP32 接口。
- [x] 在 Keil 重建，预期无未知外设句柄或引脚宏错误。

### Task 2: 传感器模拟层与按键层

**Files:**
- Create: `Core/Src/env_sensor.c`, `Core/Src/app_key.c`

**Interfaces:**
- Consumes: `EnvData_t` 和 `HAL_GetTick()`。
- Produces: `const EnvData_t *EnvSensor_GetData(void)` 与 `bool AppKey_PageTogglePressed(void)`。

- [x] 先建立模拟时间驱动的预期值：温度 24.0-29.0°C、湿度 45-65%RH、光照 300-800lx、空气质量 30-90ppm。
- [x] 用无动态内存的整数波形实现模拟值，间隔 1000ms 才更新一次。
- [x] 对 PF8 实现 30ms 消抖，只在一次按下时返回 true。
- [x] 在 Keil 重建，预期模块可独立编译。

### Task 3: ESP32-C3 AT 与华为云发布层

**Files:**
- Create: `Core/Src/esp32_c3.c`

**Interfaces:**
- Consumes: `huart2`, `EnvData_t`。
- Produces: `Esp32C3_Init()`, `Esp32C3_Process()`, `Esp32C3_RequestUpload()`, `const Esp32C3Status_t *Esp32C3_GetStatus()`。

- [x] 用固定 256 字节发送缓存拼出 JSON：`{\"temperature\":25.0,\"humidity\":55.0,\"light\":500,\"air\":60}`。
- [x] 模拟模式中每次请求均递增上传计数并记录成功；真实模式中依次发送 `AT`、`AT+CWJAP`、`AT+MQTTCONN`、`AT+MQTTPUB`。
- [x] 所有 AT 密钥采用 `ESP32_WIFI_SSID` 等宏占位，禁止写入真实凭据。
- [x] 在 Keil 重建，预期无阻塞式无限等待。

### Task 4: 两页 LCD 界面与主循环集成

**Files:**
- Create: `Core/Src/app_ui.c`
- Modify: `Core/Src/main.c`, `MDK-ARM/42_lcd_hal.uvprojx`

**Interfaces:**
- Consumes: 环境数据、ESP32 状态与 LCD `LCD_WriteAsciiString()`。
- Produces: `AppUi_Init()`、`AppUi_Render()`、`AppUi_TogglePage()`。

- [x] 用 `snprintf` 格式化每一行，显示第一页四种环境数据。
- [x] 显示第二页 Wi-Fi、MQTT、成功次数与 JSON/结果摘要。
- [x] main.c 初始化模块，按 1 秒刷新、5 秒上传、按键翻页调度。
- [x] 将新增源文件加入 Keil 项目并完整重建；预期生成新的 hex 且无编译错误。

### Task 5: 交付说明与验证

**Files:**
- Create: `README.md`

- [x] 记录所有引脚、接线、电压警告、CubeMX 外设配置和 ESP32 AT 固件前提。
- [x] 记录模拟模式切换到真实模式所需改动：将 `ENV_SENSOR_USE_SIMULATOR` 置 0，并实现真实读取函数。
- [x] 检查所有新增 C 函数前存在中文说明；人工确认没有遗留的未完成标记。
