# AppSlotA 环境监测仪表盘界面设计

## 目标

优化 STM32 AppSlotA 的 LCD 界面。默认页面用于清晰显示环境传感器数据；按下 KEY1 后切换到 ESP32 和 OTA 状态页。界面使用英文和数字，避免当前 LCD ASCII 字库无法稳定显示中文的问题。

## 范围

- 保留现有四项环境数据：温度、湿度、光照、空气质量。
- 环境页作为默认页，采用两行两列的大卡片布局。
- 页头显示 `ENV DASHBOARD`，并显示 ESP32 在线、离线或等待状态。
- 页尾提示 `KEY1: SYSTEM STATUS`。
- 系统状态页显示 ESP32、WiFi、MQTT、OTA 状态、OTA 进度条和当前 STM32 固件版本。
- 页尾提示 `KEY1: ENV DASHBOARD`。
- 无效传感器数据继续显示 `NO DATA`，不使用模拟数值。

## 不做的内容

- 不修改 CubeMX 生成的时钟、GPIO、FreeRTOS 任务或 LCD 底层驱动。
- 不改变 ESP32 通信协议、华为云逻辑、OTA 下载、校验或 Flash 写入逻辑。
- 不增加新按键，不增加自动翻页和动画。

## 模块与数据流

`freertos.c` 中现有 UI 任务每 500 ms 复制环境数据、读取 ESP32 状态，并调用 `AppUi_Render()`。

`app_ui.c` 只负责布局、字符串格式化和绘制：

1. 环境页读取 `EnvData_t`，在四张数据卡片中显示数值或 `NO DATA`。
2. 页头根据 `Esp32Status_t.received`、`wifi_connected`、`mqtt_connected` 显示简短连接状态。
3. 状态页读取 `Esp32Status_t`，显示连接状态和 OTA 进度；只有 `ota_active` 为真时绘制进度条。
4. 当前固件版本使用 `OTA_CURRENT_VERSION` 宏，因而会随 Keil 的 AppSlotA 编译配置自动变化。
5. `AppUi_TogglePage()` 仍由 KEY1 调用，仅切换环境页与系统状态页。

## 刷新与错误处理

- 固定布局仅在开机或切页时重绘，减少整屏清除造成的闪烁。
- 数据变化时只擦除并更新对应数值区域。
- 状态未知时显示 `WAITING` 或 `--`，不显示虚假的已连接、已升级状态。
- 文本格式化使用有长度限制的 `snprintf`，避免缓冲区溢出。

## CubeMX 兼容性

只修改 `Core/Src/app_ui.c` 和必要时的 `Core/Inc/app_ui.h`。两者是用户自建模块，不属于 CubeMX 生成文件；重新生成 `.ioc` 配置不会覆盖这些文件。`freertos.c` 不需要为本次界面布局新增代码。

## 验证方式

1. 编译 Keil 的 `AppSlotA` Target，应无新增错误或警告。
2. 上电后默认显示四项环境数据卡片；未接传感器时显示 `NO DATA`。
3. 按 KEY1，页面切换为系统状态页；再次按 KEY1，回到环境仪表盘。
4. ESP32 未上报状态时显示 `WAITING`；上报 WiFi、MQTT、OTA 状态后，页面对应内容更新。
5. OTA 下载期间，状态页应显示下载状态与正确百分比；结束后不再显示进度条。
