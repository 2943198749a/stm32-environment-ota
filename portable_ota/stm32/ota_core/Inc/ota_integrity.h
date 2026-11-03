/* OTA 固件长度、CRC32 和应用向量表的纯算法接口。 */
#ifndef __OTA_INTEGRITY_H
#define __OTA_INTEGRITY_H

#include <stdbool.h>
#include <stdint.h>
#include "ota_layout.h"

/* 计算 data[0..length) 的标准 IEEE CRC32；length 非零时 data 不能为 NULL。 */
uint32_t Ota_Crc32(const uint8_t *data, uint32_t length);
/* 固件长度必须非零、不超过 OTA_SLOT_SIZE，并满足 STM32F1 半字写入对齐。 */
bool Ota_ImageSizeIsValid(uint32_t image_size);
/*
 * 验证初始 MSP 位于 SRAM，Reset_Handler 带 Thumb 位且目标地址落在 slot 内。
 * 本函数只检查数值范围，不读取 Flash。
 */
bool Ota_VectorWordsAreValid(OtaSlot_t slot, uint32_t initial_msp, uint32_t reset_handler);

#endif
