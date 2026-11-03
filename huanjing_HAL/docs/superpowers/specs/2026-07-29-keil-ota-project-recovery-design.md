# Keil OTA 三工程恢复设计

## 目标

将互相混编的 Keil 多 Target 工程改为三个独立的 Keil 工程文件。所有工程继续引用同一套现有 `Core`、`Drivers`、`Middlewares` 和 `portable_ota` 源文件；不复制、不删除用户源代码。

## 原因

当前 `.uvprojx` 的多 Target 文件清单在 Keil 中被合并，造成 Bootloader 链接应用的 `main.c`、A/B 链接 `boot_main.c`。虽然 Keil 支持按 Target 排除文件，但需要对每个分组或文件维护“Include in Target Build”，不适合当前已损坏的工程状态。

## 工程结构

| Keil 工程 | Flash 起始地址 | Flash 大小 | 编译内容 |
|---|---:|---:|---|
| `Bootloader.uvprojx` | `0x08000000` | `0x00007000` | 启动、HAL Flash、OTA 元数据与校验、`boot_main.c` |
| `AppSlotA.uvprojx` | `0x08008000` | `0x0003C000` | 完整环境监测应用、LCD、传感器、FreeRTOS、`main.c` |
| `AppSlotB.uvprojx` | `0x08044000` | `0x0003C000` | 与 A 相同的完整应用代码、`main.c` |

元数据区域 `0x08007000` 至 `0x08007FFF` 不属于任何工程的链接区域。

## 文件边界

Bootloader 工程包含 `startup_stm32f103xe.s`、`system_stm32f1xx.c`、最小 HAL Flash 相关源文件、`boot_main.c`、`ota_storage.c`、`ota_metadata.c`、`ota_integrity.c`、`sha256.c`。它不包含 `Core/Src/main.c`、FreeRTOS、LCD、传感器和 ESP32 UART。

A/B 工程均包含完整现有 `Core/Src` 应用源文件、所需 HAL 驱动和 FreeRTOS 源文件；它们不包含 `boot_main.c` 或 OTA Bootloader 的存储/元数据/完整性源文件。本次不加入应用 OTA 接收逻辑，避免与当前 ESP32 UART 模块抢占 USART2。

## 输出与安全

三个工程的输出目录分别为 `Objects/Bootloader`、`Objects/AppSlotA`、`Objects/AppSlotB`，输出名分别为 `bootloader`、`app_slot_a`、`app_slot_b`。

创建前保留当前 `42_lcd_hal.uvprojx` 及 `.uvoptx` 备份；旧工程不再用于编译或下载，以免意外覆盖 Bootloader 区。

## 验收

- 三个工程均能独立编译为 `0 Error(s)`；
- Bootloader 只含一个入口：`boot_main.c`；
- A/B 各只含一个入口：`main.c`；
- A/B 镜像均小于 `0x3C000`；
- 分别下载 Bootloader 与 A 槽后，复位能够从 Bootloader 跳转到 A 槽。
