/*
 * OTA 元数据的 Flash 读写封装。
 *
 * A/B 两份记录分别占用一个 2KB Flash 页。保存新记录时只擦除较旧的那一页，
 * 因此即使突然断电，另一页的旧记录仍在，Bootloader 仍能找到可启动的应用。
 */
#ifndef OTA_STORAGE_H
#define OTA_STORAGE_H

#include <stdbool.h>
#include "stm32f1xx_hal.h"
#include "ota_metadata.h"

/*
 * 从两份 Flash 页中选择 generation 最新且 CRC 有效的记录并复制到 record。
 * record 不能为 NULL；没有有效记录时返回 false，输出不能用于启动决策。
 */
bool OtaStorage_Load(OtaBootRecord_t *record);

/*
 * 将完整记录写入较旧的 2 KB 元数据页，再读回校验。
 * record 不能为 NULL；函数会解锁、擦除和编程 Flash，只能在任务上下文调用。
 */
HAL_StatusTypeDef OtaStorage_Save(const OtaBootRecord_t *record);

#endif /* OTA_STORAGE_H */
