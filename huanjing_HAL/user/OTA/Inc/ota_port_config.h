/*
 * STM32 OTA 移植时唯一需要优先修改的板级配置文件。
 * 先根据芯片数据手册确认 Flash/SRAM，再修改下面的数值。Bootloader 与两个
 * 应用 Target 必须共享布局；AppSlotA/AppSlotB 只通过编译宏覆盖当前槽位和版本。
 */
#ifndef OTA_PORT_CONFIG_H
#define OTA_PORT_CONFIG_H

/* STM32F103ZET6：Flash 512 KB，页大小 2 KB，SRAM 64 KB。 */
#define OTA_FLASH_BASE             0x08000000UL
#define OTA_FLASH_SIZE_BYTES       (512UL * 1024UL)
#define OTA_FLASH_PAGE_SIZE_BYTES  0x800UL
#define OTA_SRAM_BASE_ADDRESS      0x20000000UL
#define OTA_SRAM_SIZE_BYTES        (64UL * 1024UL)

/* Bootloader 28 KB，元数据 4 KB，剩余空间均分为两个 240 KB 槽。 */
#define OTA_BOOTLOADER_SIZE_BYTES  0x7000UL
#define OTA_METADATA_BASE_ADDRESS  0x08007000UL
#define OTA_METADATA_SIZE_BYTES   0x1000UL
#define OTA_SLOT_SIZE_BYTES       0x3C000UL
#define OTA_SLOT_A_ADDRESS        0x08008000UL
#define OTA_SLOT_B_ADDRESS        0x08044000UL

/* 应用工程按实际 CubeMX 生成的 UART 句柄修改，例如 huart2。 */
#ifndef OTA_UART_HANDLE
#define OTA_UART_HANDLE            huart2
#endif

/* 每个应用 Target 用编译选项覆盖这个宏：OTA_SLOT_A 或 OTA_SLOT_B。 */
#ifndef OTA_APPLICATION_SLOT
#define OTA_APPLICATION_SLOT       OTA_SLOT_A
#endif

/* 每个应用 Target 都要改成自己的版本号，例如 v1.2.1。 */
#ifndef OTA_CURRENT_VERSION
#define OTA_CURRENT_VERSION         "v1.0.0"
#endif

#endif /* OTA_PORT_CONFIG_H */
