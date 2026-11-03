# 智能环境检测（STM32F103ZET6 HAL）

本工程基于尚硅谷 STM32-F103ZET6（2024-04-29 原理图）、Keil MDK、STM32CubeMX 和 FreeRTOS。LCD 保留 FSMC 驱动，实现环境仪表盘与 `SYSTEM STATUS` 系统/OTA 状态页。

## 当前可直接运行的内容

- DHT11、BH1750 和 MQ135 使用真实 GPIO/I2C/ADC 驱动；读取失败显示 `NO DATA`。
- FreeRTOS 使用 `sensortask`、`uitask`、`uarttask` 和 `otaTask` 四个任务。
- ESP32-C3 Arduino 网关负责 Wi-Fi、MQTT、华为云上报和 STM32 OTA 固件下载。
- 按下板载 `KEY1(PF8)`，切换环境仪表盘和系统状态页。
- Bootloader、AppSlotA、AppSlotB 组成 A/B OTA，可在新固件未确认时回滚。

## 原理图对应接线

| 功能 | 模块端 | STM32 引脚 | 说明 |
| --- | --- | --- | --- |
| 温湿度 | DHT11 DATA | PC3 | 板载 18B20 座 DATA，可作普通 GPIO；3.3V/GND 共地 |
| 光照 | BH1750 SCL/SDA | PB6/PB7（I2C1） | I2C 总线可与 LCD 触摸共用，注意地址与上拉电阻 |
| 空气质量 | MQ-135 AO | PC0（ADC1_IN10） | AO 若为 5V，必须分压后再接 PC0 |
| 页面切换 | 板载 KEY1 | PF8 | 已配置上拉输入，按下为低电平；PF11 是 KEY4 |
| Wi-Fi/云上传 | 板载 ESP32-C3 | USART2：PA2/PA3 | PA2 到 C3 RX，PA3 来自 C3 TX，115200 8N1 |
| LCD | 板载 LCD | FSMC | 本工程不改动 `Core/Src/lcd.c` |

## 主要文件

| 文件 | 职责 |
| --- | --- |
| `Core/Src/env_sensor.c` | 汇总 DHT11、BH1750、MQ135 的真实数据和有效标志 |
| `Core/Src/app_key.c` | KEY1 消抖与一次按下事件 |
| `Core/Src/app_ui.c` | 环境仪表盘和系统/OTA 状态页的局部刷新 |
| `Core/Src/esp32_uart.c` | USART2 中断环形缓冲区、按字节读取和文本发送 |
| `Core/Src/freertos.c` | 四个任务、环境快照共享、ENV 上传和新固件确认 |
| `user/OTA` | STM32 侧 A/B OTA、Flash、元数据和完整性校验 |
| `../portable_ota/esp32c3_arduino_gateway` | ESP32-C3 华为云与固件传输网关 |

## Keil 编译与下载

1. 首次烧录时打开 `MDK-ARM/Bootloader.uvprojx`，编译并下载 Bootloader。
2. 打开 `MDK-ARM/AppSlotA.uvprojx`，编译 AppSlotA 并下载到 A 槽地址。
3. 后续 B 槽镜像通过 ESP32-C3 和华为云 OTA 下发。
4. 上电后默认显示环境页，按 KEY1 切换系统状态页。

## 接入真实传感器

当前工程已经包含 ADC1、I2C1 和三个真实传感器驱动。安装模块后，在 STM32CubeMX 打开 `42_lcd_hal.ioc` 并确认：

1. `ADC1`：Regular Channel 10，PC0，软件触发，右对齐。
2. `I2C1`：100 kHz，PB6=SCL、PB7=SDA。
3. `PC3`：GPIO 输入/输出，用于 DHT11 单总线时序。
4. `USART2`：115200、8N1、PA2/PA3。
5. `PF8`：上拉输入。

`EnvSensor_Update()` 会分别更新四个有效标志；未接线或读取超时时只让对应项目显示 `NO DATA`，不会生成模拟数据。

## 接入 ESP32-C3 与华为云

1. 使用 Arduino IDE 打开 `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway`。
2. 在本地配置中填写 Wi-Fi、IoTDA 设备信息和 STM32 串口引脚。
3. 烧录 ESP32-C3 网关；STM32 只通过 USART2 交换 `ENV`、`STATUS` 和 OTA 协议。
4. 华为云产品模型中的环境字段、版本上报和升级进度必须与网关代码保持一致。

不要将真实 Wi-Fi 密码或设备密钥提交到代码仓库。
## ESP32-C3 Arduino 联网方式

本项目不使用 ESP-AT 指令。ESP32-C3 在 Arduino IDE 运行独立网关，负责 Wi-Fi、MQTT、华为云上报、固件下载和向 STM32 传输 OTA 帧。

1. Arduino IDE 安装 **esp32 by Espressif Systems** 开发板包，并安装 **PubSubClient** 库。
2. 打开 `../portable_ota/esp32c3_arduino_gateway/esp32c3_ota_gateway/esp32c3_ota_gateway.ino`。
3. 选择开发板 **ESP32C3 Dev Module**，将 **USB CDC On Boot** 设为 **Enabled**。
4. 使用 ESP32-C3 的 USB 口烧录程序，并打开 USB 串口监视器，波特率 `115200`。
5. 首次使用时通过 Keil 下载 Bootloader 和 AppSlotA；之后可从华为云下发另一个槽位的固件。

ESP32 串口应依次显示 Wi-Fi、MQTT 和网关状态，随后显示环境上报或 OTA 传输进度。华为云设备详情页可以查看属性、当前版本和升级任务进度。

STM32 发送 `ENV,温度,湿度,光照,空气质量\r\n` 和启动确认；ESP32 返回六字段 `STATUS`，并使用 `OTA_BEGIN`、二进制数据帧、`OTA_END` 完成升级。STM32 的 `SYSTEM STATUS` 页面显示连接状态、进度和当前版本。
