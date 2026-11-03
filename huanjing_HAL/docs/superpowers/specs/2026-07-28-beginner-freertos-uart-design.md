# 初学者 FreeRTOS + ESP32 UART 设计

## 目标

将当前工程整理为容易阅读的三任务示例。STM32 不再发送 ESP-AT 或 MQTT 指令；ESP32-C3 继续由 Arduino IDE 烧录和运行。

## 任务划分

1. `sensortask`：初始化 DHT11、BH1750、MQ135；每秒调用一次 `EnvSensor_Update()`，把结果写入一个全局 `EnvData_t` 副本。
2. `uitask`：每 500ms 从该副本读取数据并调用 `AppUi_Render()`；KEY1 仅切换环境页和 OTA 预留页。
3. `uarttask`：每 100ms 调用新的 ESP32 UART 模块。当前只保留初始化、接收缓存和发送接口，不解释或发送 AT 指令。以后 Arduino 端定义文本或 JSON 协议后，只在该模块增加解析代码。

## 数据流

`传感器 -> sensortask -> latestEnvData -> uitask -> LCD`

`Arduino ESP32 <-> USART2 <-> uarttask`

目前 UART 数据不影响 LCD。后续需要显示 Wi-Fi、OTA 或 Arduino 上传状态时，再把 UART 模块的状态结构传给 UI。

## 模块边界

- 删除 `esp32_c3.c/.h` 和所有 ESP-AT、MQTT、华为云代码。
- 新增 `esp32_uart.c/.h`：只包含 `Esp32Uart_Init()`、`Esp32Uart_Process()`、`Esp32Uart_Send()`。
- `app_ui.c/.h` 保留仪表盘和 OTA 预留页，移除 AT 网络状态与启动调试界面。
- `freertos.c` 的业务代码只放在 CubeMX `USER CODE` 区域。

## 并发与错误处理

当前示例使用一个很短的临界区复制 `latestEnvData`，不向初学者暴露复杂的网络状态互斥锁。传感器未接线时，数据标记为无效，LCD 显示 `NO DATA`，任务继续运行。

UART 接收不等待超过 100ms；没有 ESP32 回复时不阻塞传感器或 LCD 任务。

## 验证

- 三个任务均由 CubeMX 创建并检查句柄。
- 静态测试验证无 ESP-AT/MQTT 调用、传感器初始化在任务内、UART 任务调用预留模块。
- Keil 构建必须为 0 error、0 warning。
