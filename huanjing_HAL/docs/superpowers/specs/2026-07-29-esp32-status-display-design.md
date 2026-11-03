# ESP32 状态显示设计

## 目标

让 STM32 通过 USART2 接收 ESP32-C3 网关发送的 `STATUS,...` 文本，并在 LCD 的 OTA 页面显示 ESP32、Wi-Fi、MQTT 与 OTA 的当前状态。

## 模块边界

新增 `Core/Inc/esp32_status.h` 与 `Core/Src/esp32_status.c`。

- `Esp32Status_ProcessLine()`：解析一行 ESP32 文本；仅接受 `STATUS,wifi,mqtt,publish,ota_active,progress,message`。
- `Esp32Status_Get()`：返回最新状态的副本，供 LCD 任务读取。
- `Esp32Status_Init()`：清空状态，初始显示为未连接。

UART 任务仍只负责 `Esp32Uart_Process()` 和读取完整文本行；它将文本交给状态模块。LCD 任务只读取状态模块并绘制 UI，不直接读取 USART2。

## 显示规则

当尚未收到合法状态行时，OTA 页显示：

```text
ESP32 : WAITING
WIFI  : --
MQTT  : --
OTA   : --
```

收到 `STATUS,1,1,1,0,0,ready` 后显示：

```text
ESP32 : CONNECTED
WIFI  : OK
MQTT  : OK
OTA   : READY
```

收到 OTA 传输状态时，显示 `OTA : DOWNLOADING 50%` 等状态。无效、过长或字段不完整的文本被忽略，不影响 FreeRTOS 任务运行。

## CubeMX 安全

状态模块是独立 `.c/.h` 文件，不会被 CubeMX 生成覆盖。`freertos.c` 只在现有 `USER CODE` 区添加初始化和 UART 状态处理调用；UI 改动位于用户自写的 `app_ui.c/.h`。

## 验收

- A/B 工程均可编译为 `0 Error(s)`；
- 发送合法 `STATUS` 行后，状态模块返回对应字段；
- OTA 页能显示等待、正常在线和下载进度三种状态。
