# 自写模块简化与中文注释设计

## 目标

保持现有硬件功能不变，把自写模块改成初学者容易阅读的结构，并补充中文注释。ESP32 保留普通 USART2 文本通信；不加入联网、云平台或复杂协议。

## 范围

本次只调整以下用户业务模块：

- `dht11.c/.h`：DHT11 温湿度读取。
- `bh1750.c/.h`：BH1750 光照读取。
- `mq135.c/.h`：MQ135 空气质量读取。
- `env_sensor.c/.h`：汇总三种传感器数据。
- `app_key.c/.h`：KEY1 消抖与翻页按键。
- `app_ui.c/.h`：环境页与 OTA 预留页显示。
- `esp32_uart.c/.h`：ESP32 普通 UART 文本收发。
- `freertos.c`：仅改善三项任务和共享数据辅助函数的中文注释。

不修改 CubeMX 管理的 `adc.c`、`i2c.c`、`usart.c`、`gpio.c`，也不重写底层 `lcd.c/.h` 驱动。

## 模块接口

每个传感器模块只公开“初始化”和“读取”两类函数；底层等待、引脚切换和寄存器细节只保留在对应 `.c` 文件中。

`env_sensor` 只公开 `EnvSensor_Init`、`EnvSensor_Update`、`EnvSensor_GetData`：初始化所有传感器、更新一帧汇总数据、获取最新数据。读取失败时对应 `*_valid` 标志为 `false`。

`app_key` 只公开 `AppKey_Init` 和 `AppKey_PageTogglePressed`。调用者不需要关心电平或消抖时间。

`esp32_uart` 保留四个函数：

- `Esp32Uart_Init`：清空软件接收缓存；不初始化 USART2 硬件。
- `Esp32Uart_Process`：零超时读取一个字符；收到 `\n` 后完成一行。
- `Esp32Uart_SendString`：发送一段字符串。
- `Esp32Uart_ReadLine`：读取一行完整文本。

## 任务数据流

传感器任务调用 `EnvSensor_Update`，然后复制最新环境数据。LCD 任务复制数据后调用 `AppUi_Render`。UART 任务每 10 ms 调用 `Esp32Uart_Process`。三个任务都不直接操作其他模块的内部变量。

## 简化原则

1. 函数名称表达动作，避免不必要的状态机和间接层。
2. 每个公开函数前写中文注释，说明“做什么、何时调用、返回值”。
3. 只对容易误解的步骤写行内中文注释，例如 DHT11 时序等待、串口零超时、临界区复制。
4. 不改变当前引脚、外设、任务周期和 LCD 页面功能。
5. 所有业务修改仍位于独立模块或 CubeMX 的 `USER CODE` 区；`.ioc` 保留三任务配置和 `KeepUserCode=true`。

## 验证

1. 更新静态检查，确认 ESP32 UART 仍是 `huart2`、零超时接收、无复杂协议。
2. 检查传感器任务仍在任务内部初始化传感器，且传感器模块保留真实 GPIO/I2C/ADC 读取。
3. 运行全部 PowerShell 检查。
4. Keil 重新构建，要求 `0 Error(s), 0 Warning(s)`。
