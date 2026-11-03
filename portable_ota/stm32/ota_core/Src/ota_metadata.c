/*
 * OTA 双副本启动记录的纯逻辑实现。
 *
 * 本文件不直接读写 Flash：它只负责创建记录、检查字段组合、刷新记录 CRC，
 * 以及在两份记录中选择 generation 更新的一份。纯逻辑与 Flash 驱动分离后，
 * 这些启动规则既便于单元测试，也不会被擦写步骤干扰。
 */
#include <stddef.h>
#include "ota_metadata.h"

/**
 * @brief 判断槽位编号是否是可启动的应用槽。
 * slot 为待检查编号；仅 OTA_SLOT_A 或 OTA_SLOT_B 返回 true。
 * OTA_SLOT_NONE 及其他值返回 false；纯逻辑函数，无状态修改。
 */
static bool OtaRecord_IsApplicationSlot(OtaSlot_t slot)
{
    return (slot == OTA_SLOT_A) || (slot == OTA_SLOT_B);
}

/**
 * @brief 计算启动记录校验字段之前所有字节的 CRC32。
 * record 必须指向有效可读结构体；覆盖范围由 record_crc32 的字段偏移确定。
 * 返回 CRC32，不包含 record_crc32 自身；不修改记录，也不检查字段语义。
 */
static uint32_t OtaRecord_CalculateCrc(const OtaBootRecord_t *record)
{
    return Ota_Crc32((const uint8_t *)record,
                     (uint32_t)offsetof(OtaBootRecord_t, record_crc32));
}

/**
 * @brief 构造启动记录并计算记录 CRC32，供后续保存或校验使用。
 * generation 为代数；active_slot 为已确认槽；pending_slot 为待试运行槽或 NONE。
 * pending_size 和 pending_crc32 描述待启动固件；无 pending 时调用方应传入两个零。
 * 按值返回记录，试启动次数初始为零；不校验参数组合，也不写 Flash。
 */
OtaBootRecord_t OtaRecord_Make(uint32_t generation,
                               OtaSlot_t active_slot,
                               OtaSlot_t pending_slot,
                               uint32_t pending_size,
                               uint32_t pending_crc32)
{
    OtaBootRecord_t record;

    /* 从完整的新记录开始构造，避免旧结构体的残留字段进入 CRC。 */
    record.magic = OTA_RECORD_MAGIC;
    record.generation = generation;
    record.active_slot = (uint32_t)active_slot;
    record.pending_slot = (uint32_t)pending_slot;
    record.pending_size = pending_size;
    record.pending_crc32 = pending_crc32;
    record.attempt_count = 0U;
    record.record_crc32 = OtaRecord_CalculateCrc(&record);

    return record;
}

/**
 * @brief 更新一份 RAM 启动记录的试运行次数，并重算记录 CRC。
 * record 为可写记录；attempt_count 为新的尝试次数，不在此限制上限。
 * 无返回值；record 为空时忽略，非空时就地修改，持久化需另行调用保存函数。
 */
void OtaRecord_SetAttemptCount(OtaBootRecord_t *record, uint32_t attempt_count)
{
    if (record == NULL)
    {
        return;
    }

    record->attempt_count = attempt_count;
    record->record_crc32 = OtaRecord_CalculateCrc(record);
}

/**
 * @brief 校验启动记录魔数、槽位组合、待启动信息和记录 CRC32。
 * record 可指向 RAM 或内存映射 Flash；空指针返回 false。
 * 无 pending 时相关字段必须全零；有 pending 时须为另一有效槽且镜像长度合法。
 * 全部检查通过返回 true；仅检查记录本身，不校验固件内容或向量表。
 */
bool OtaRecord_IsValid(const OtaBootRecord_t *record)
{
    OtaSlot_t active_slot;
    OtaSlot_t pending_slot;

    if (record == NULL)
    {
        return false;
    }

    /* 空白 Flash 通常全为 0xFF，先用魔数快速排除它。 */
    if (record->magic != OTA_RECORD_MAGIC)
    {
        return false;
    }

    active_slot = (OtaSlot_t)record->active_slot;
    pending_slot = (OtaSlot_t)record->pending_slot;
    if (!OtaRecord_IsApplicationSlot(active_slot))
    {
        return false;
    }

    /* 无待升级固件时，相关长度、CRC 和尝试次数必须全部清零。 */
    if (pending_slot == OTA_SLOT_NONE)
    {
        if ((record->pending_size != 0U) || (record->pending_crc32 != 0U) ||
            (record->attempt_count != 0U))
        {
            return false;
        }
    }
    else
    {
        /* 待试运行的必须是另一个槽位，并且具有一个合法的镜像长度。 */
        if ((!OtaRecord_IsApplicationSlot(pending_slot)) ||
            (pending_slot == active_slot) ||
            (!Ota_ImageSizeIsValid(record->pending_size)))
        {
            return false;
        }
    }

    /* 字段组合合理仍不够，最后确认记录保存后没有损坏。 */
    return record->record_crc32 == OtaRecord_CalculateCrc(record);
}

/**
 * @brief 从两个候选记录中选择有效且代数较新的一份。
 * first、second 可为空；先分别验证记录，均无效返回 NULL，仅一个有效就返回它。
 * 都有效时用有符号差值比较代数以处理通常的计数回绕；同代数优先 first。
 * 返回原记录指针，不复制或修改数据；代数比较要求两代距离小于 2^31。
 */
const OtaBootRecord_t *OtaRecord_SelectNewest(const OtaBootRecord_t *first,
                                              const OtaBootRecord_t *second)
{
    bool first_valid = OtaRecord_IsValid(first);
    bool second_valid = OtaRecord_IsValid(second);

    if (!first_valid)
    {
        return second_valid ? second : NULL;
    }
    if (!second_valid)
    {
        return first;
    }

    /* 有符号差值比较可正确处理 generation 从 0xFFFFFFFF 回绕到 0。 */
    if ((int32_t)(second->generation - first->generation) > 0)
    {
        return second;
    }
    return first;
}

/**
 * @brief 根据待启动槽和尝试次数决定本次优先启动哪个槽。
 * record 应为已验证的记录；空指针返回 OTA_SLOT_NONE，本函数不重复校验字段和 CRC。
 * 有 pending 且尝试未达上限时返回 pending，否则返回 active；不递增次数或保存记录。
 */
OtaSlot_t OtaRecord_BootSlot(const OtaBootRecord_t *record)
{
    OtaSlot_t active_slot;
    OtaSlot_t pending_slot;

    if (record == NULL)
    {
        return OTA_SLOT_NONE;
    }

    active_slot = (OtaSlot_t)record->active_slot;
    pending_slot = (OtaSlot_t)record->pending_slot;

    /* 新固件在规定次数内优先试运行；次数耗尽则立即选择旧固件。 */
    if ((pending_slot != OTA_SLOT_NONE) &&
        (record->attempt_count < OTA_BOOT_MAX_ATTEMPTS))
    {
        return pending_slot;
    }
    return active_slot;
}
