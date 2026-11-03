# ESP32-C3 Arduino MQTT 设计

## 目标

不再让 STM32 发送 ESP-AT 指令。ESP32-C3 运行 Arduino 程序，接收 STM32 的环境数据后自行连接 Wi-Fi 和华为云 MQTT。

## 数据流

1. STM32 通过 USART2 发送一行 ASCII 数据：`ENV,温度,湿度,光照,空气质量\r\n`。
2. ESP32-C3 使用 UART0 接收并校验这一行数据。
3. ESP32-C3 组装华为云属性上报 JSON，并发布到设备属性上报主题。
4. ESP32-C3 的 USB 串口输出 Wi-Fi、MQTT 和上报结果，便于独立调试。

## 文件与依赖

- 新增 `arduino/esp32c3_huawei_cloud/esp32c3_huawei_cloud.ino`：ESP32-C3 Arduino 程序，含中文注释。
- STM32 的 `esp32_c3.c` 改为只发送 `ENV` 数据行，不再连接 Wi-Fi 或发送 AT 指令。
- Arduino IDE 安装 ESP32 开发板包与 PubSubClient 库。

## 验收

- ESP32 USB 串口显示收到一行 `ENV` 数据。
- ESP32 显示 Wi-Fi 已连接和 MQTT 已连接。
- 每收到一行有效数据，华为云设备详情页出现新的属性上报记录。
