# STM32F103 示例

这是一个不包含 LCD、按键和传感器的最小示例。把 CubeMX 生成的 `main.h`、`gpio.c`、`usart.c`、启动文件和链接脚本放入对应 Target，然后：

1. AppSlotA 的 Flash 起始地址设为 `0x08008000`，定义 `OTA_APPLICATION_SLOT=OTA_SLOT_A`。
2. AppSlotB 的 Flash 起始地址设为 `0x08044000`，定义 `OTA_APPLICATION_SLOT=OTA_SLOT_B`。
3. Bootloader 起始地址设为 `0x08000000`，大小不超过 28 KB。
4. 两个应用都链接 `../ota_core/Inc` 和 `../ota_core/Src`。
5. A/B 只允许写入当前运行槽的另一槽，不能把 A 包下发给正在运行 A 的设备。
