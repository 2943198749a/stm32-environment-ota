/*
 * STM32 OTA 固定 Flash/SRAM 布局。
 *
 * Bootloader、AppSlotA、AppSlotB 和 ESP32 固件包配置必须使用同一组地址；
 * 这些常量属于持久化兼容契约，任意单边修改都可能擦除 Bootloader、元数据
 * 或正在运行的应用。板级容量和地址来自 ota_port_config.h。
 */
#ifndef __OTA_LAYOUT_H
#define __OTA_LAYOUT_H

#include <stdint.h>
#include "ota_port_config.h"

/* 两个应用槽位和“没有有效槽位”三种状态。 */
typedef enum
{
    OTA_SLOT_NONE = 0U,
    OTA_SLOT_A = 1U,
    OTA_SLOT_B = 2U
} OtaSlot_t;

/* STM32F103ZET6 的 512KB 内部 Flash 起始地址。 */
#ifndef OTA_FLASH_BASE
#define OTA_FLASH_BASE          0x08000000UL
#endif
/* Bootloader 代码只使用前 28KB，后 4KB 留给两份升级状态记录。 */
#define OTA_BOOTLOADER_SIZE     OTA_BOOTLOADER_SIZE_BYTES
#define OTA_METADATA_BASE       OTA_METADATA_BASE_ADDRESS
#define OTA_METADATA_SIZE       OTA_METADATA_SIZE_BYTES
#define OTA_FLASH_PAGE_SIZE     OTA_FLASH_PAGE_SIZE_BYTES

/* 元数据区刚好占两个 2KB Flash 页：每页放一份完整记录。 */
#define OTA_METADATA_RECORD_A_BASE  OTA_METADATA_BASE
#define OTA_METADATA_RECORD_B_BASE  (OTA_METADATA_BASE + OTA_FLASH_PAGE_SIZE)
/* 两个应用槽位各 240KB，刚好覆盖剩余 Flash。 */
#define OTA_SLOT_A_BASE         OTA_SLOT_A_ADDRESS
#define OTA_SLOT_B_BASE         OTA_SLOT_B_ADDRESS
#define OTA_SLOT_SIZE           OTA_SLOT_SIZE_BYTES
#define OTA_SLOT_A_END          (OTA_SLOT_A_BASE + OTA_SLOT_SIZE)
#define OTA_SLOT_B_END          (OTA_SLOT_B_BASE + OTA_SLOT_SIZE)
/* STM32F103ZET6 的 64KB SRAM 地址范围。 */
#define OTA_SRAM_BASE           OTA_SRAM_BASE_ADDRESS
#define OTA_SRAM_END            (OTA_SRAM_BASE_ADDRESS + OTA_SRAM_SIZE_BYTES)

/* 根据槽位编号返回应用向量表的起始地址。 */
static uint32_t Ota_SlotBase(OtaSlot_t slot)
{
    if (slot == OTA_SLOT_A)
    {
        return OTA_SLOT_A_BASE;
    }

    if (slot == OTA_SLOT_B)
    {
        return OTA_SLOT_B_BASE;
    }

    return 0U;
}

#endif
