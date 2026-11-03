/*
 * OTA 启动元数据。
 *
 * 这份小记录放在独立的 Flash 页中，用来记住：当前稳定运行哪个槽位、
 * 是否有一个刚下载完成且尚未确认的新固件。真正写 Flash 的函数放在
 * ota_flash.c；本文件只负责记录格式和纯逻辑，便于单独测试。
 */
#ifndef OTA_METADATA_H
#define OTA_METADATA_H

#include <stdbool.h>
#include <stdint.h>
#include "ota_integrity.h"

/* "OTA1" 的 ASCII 编码，用于判断这页 Flash 是否存放 OTA 记录。 */
#define OTA_RECORD_MAGIC       0x4F544131UL

/* 新固件最多试启动两次；两次均没有被应用确认，就回退旧固件。 */
#define OTA_BOOT_MAX_ATTEMPTS  2U

typedef struct
{
    /* 记录头：先确认“这是一条 OTA 记录”，再判断两份记录中谁更新。 */
    uint32_t magic;          /* 固定为 OTA_RECORD_MAGIC，即 ASCII 文本 "OTA1"。 */
    uint32_t generation;     /* 状态每变化一次就加 1；它不是应用版本号。 */

    /* 稳定状态：新固件没有确认前，回滚目标始终由 active_slot 保存。 */
    uint32_t active_slot;    /* 只能是 OTA_SLOT_A 或 OTA_SLOT_B。 */

    /* 试运行状态：四个字段共同描述一个刚下载、尚未确认的新镜像。 */
    uint32_t pending_slot;   /* 新镜像所在槽；没有新镜像时为 OTA_SLOT_NONE。 */
    uint32_t pending_size;   /* 新镜像真实字节数；没有 pending 时必须为 0。 */
    uint32_t pending_crc32;  /* 新镜像的 CRC32；Bootloader 启动前会重新计算。 */
    uint32_t attempt_count;  /* 已经试启动的次数；达到上限后回滚 active。 */

    /* 记录保护：校验从 magic 到 attempt_count 的前 28 字节。 */
    uint32_t record_crc32;   /* 不包含自身，必须在其他字段确定后最后计算。 */
} OtaBootRecord_t;

/* 按统一格式创建一份新的元数据记录。 */
OtaBootRecord_t OtaRecord_Make(uint32_t generation,
                               OtaSlot_t active_slot,
                               OtaSlot_t pending_slot,
                               uint32_t pending_size,
                               uint32_t pending_crc32);

/* 更新待试运行次数，并同步刷新记录 CRC。 */
void OtaRecord_SetAttemptCount(OtaBootRecord_t *record, uint32_t attempt_count);

/* 检查魔数、槽位、长度和记录 CRC 是否正确。 */
bool OtaRecord_IsValid(const OtaBootRecord_t *record);

/* 从两份冗余记录中选择有效且 generation 最新的一份；没有则返回 0。 */
const OtaBootRecord_t *OtaRecord_SelectNewest(const OtaBootRecord_t *first,
                                              const OtaBootRecord_t *second);

/* 根据可信记录决定这次应该从哪个应用槽位启动。 */
OtaSlot_t OtaRecord_BootSlot(const OtaBootRecord_t *record);

#endif /* OTA_METADATA_H */
