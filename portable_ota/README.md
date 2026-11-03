# 可移植 STM32 A/B OTA 项目

## 源码维护规则

STM32 通用 OTA 代码以 `huanjing_HAL/user/OTA` 为唯一维护源；
`portable_ota/stm32/ota_core` 是为了能够独立复制而保留的导出副本。

- 修改通用 OTA 逻辑后，运行 `tools/sync_ota_core.ps1` 刷新导出副本。
- 发布或构建前运行 `tools/sync_ota_core.ps1 -Check` 做只读一致性检查。
- `stm32/ota_core/Src/ota_port.c` 是目标板适配文件，不参与自动覆盖。
- 同步脚本只处理显式列出的文件，不会递归复制或删除其他内容。

这个目录把 OTA 从“智能环境检测工程”中独立出来。以后换成别的 STM32 HAL 工程时，不需要复制 LCD、传感器和页面代码，只复制本目录并按配置清单修改。

## 先看哪三个文件

1. `配置参数清单.md`：逐项确认哪些参数必须改。
2. `移植步骤.md`：按 CubeMX → Keil → Arduino → 华为云的顺序操作。
3. `stm32/ota_core/Inc/ota_port_config.h`：STM32 Flash、槽位、UART 和版本号的集中配置。

## 目录说明

```text
portable_ota/
├─ stm32/ota_core/                  # 与业务无关的 OTA 核心 C 代码
├─ stm32/stm32f103_example/         # A、B 应用和 Bootloader 的最小示例
├─ esp32c3_arduino_gateway/         # Arduino IDE 网关示例
├─ tools/check_ota_config.ps1       # 上传云端前检查 BIN
├─ 配置参数清单.md
└─ 移植步骤.md
```

## OTA 的一句话流程

ESP32 从华为云收到升级事件 → 下载 BIN → 通过 UART 分帧发送到未运行槽 → STM32 校验并写入 pending → 复位 → Bootloader 试启动新槽 → 新应用稳定 10 秒后确认 → ESP32 上报 100%。

## 第一次使用

先把 `ota_gateway_config.h` 中的 `CHANGE_ME` 替换为自己的配置，再编译。不要把 Wi-Fi 密码、设备密钥和 token 提交到代码仓库。上传固件前执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\portable_ota\tools\check_ota_config.ps1 `
  -BinPath .\app_slot_a.bin -Slot A -Version v1.2.1
```

看到“OTA 配置检查通过”后，才把 BIN 上传到云端。

### 主工程精简后的可选模块

`stm32/ota_core/Src/ota_cloud.c` 和 `Inc/ota_cloud.h` 仅保留为可选的 STM32 云端 JSON 解析工具，不参与当前主工程 A/B 应用编译，也不再列入 `tools/sync_ota_core.ps1` 的同步清单。当前 ESP32-C3 网关负责云端消息处理，STM32 使用串口 OTA 协议。

测试帧辅助函数已移至主工程 `huanjing_HAL/tests/test_ota_uart_compile.c`，正式 OTA 核心不再提供 `ForTest` 接口。复位统一使用 `OtaPort_ResetToBootloader`；下载状态清零统一使用 `OtaApp_Init`。
