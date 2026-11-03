/*
 * OTA 完整性检查实现：不依赖 HAL，Bootloader 和应用程序都能复用。
 * CRC32 用来发现偶然损坏；向量表检查用来拒绝明显不是 STM32 应用的内容。
 * 二者都不是身份认证：抵御恶意替换还需要设备端公钥和数字签名验证。
 */
#include "ota_integrity.h"
#include <stddef.h>

/**
 * @brief 计算标准 IEEE CRC32：初值和末尾异或均为 0xFFFFFFFF，反射多项式 0xEDB88320。
 * data 指向 length 字节；空指针配非零长度返回 0，零长度输入也返回 0。
 * 返回整段数据的 CRC32，不修改输入；供固件完整性和启动记录校验复用。
 */
uint32_t Ota_Crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t index;

    /* 空指针只允许搭配零长度，避免错误访问内存。 */
    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        uint32_t bit;

        crc ^= (uint32_t)data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc >> 1U) ^ (((crc & 1U) != 0U) ? 0xEDB88320UL : 0U);
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/**
 * @brief 检查固件字节数是否在单个应用槽容量范围内。
 * image_size 为镜像长度；大于零且不超过 OTA_SLOT_SIZE 时返回 true，否则返回 false。
 * 只判断容量，不检查半字对齐、向量表或镜像内容，写入层另行检查偶数长度。
 */
bool Ota_ImageSizeIsValid(uint32_t image_size)
{
    return (image_size > 0U) && (image_size <= OTA_SLOT_SIZE);
}

/**
 * @brief 校验镜像向量表中的初始栈顶和复位入口是否符合槽位及 Cortex-M 要求。
 * slot 为目标应用槽；initial_msp 必须按 4 字节对齐且处于 SRAM 范围（包含顶端）。
 * reset_handler 必须带 Thumb 标志，去掉最低位后的地址必须位于目标槽内。
 * 全部满足返回 true，否则 false；只验证传入值，不访问镜像或计算摘要。
 */
bool Ota_VectorWordsAreValid(OtaSlot_t slot, uint32_t initial_msp, uint32_t reset_handler)
{
    uint32_t slot_base = Ota_SlotBase(slot);
    uint32_t reset_address = reset_handler & ~1UL;

    if (slot_base == 0U)
    {
        return false;
    }

    /* Cortex-M 只执行 Thumb 指令，所以函数入口最低位必须为 1。 */
    /* 主栈向低地址增长；初始 MSP 必须按 4 字节对齐并位于 SRAM。 */
    if (((reset_handler & 1U) == 0U) || ((initial_msp & 3U) != 0U))
    {
        return false;
    }

    if ((initial_msp < OTA_SRAM_BASE) || (initial_msp > OTA_SRAM_END))
    {
        return false;
    }

    /* 清除 Thumb 标志位后，真实入口地址必须落在所选应用槽位内。 */
    return (reset_address >= slot_base) && (reset_address < (slot_base + OTA_SLOT_SIZE));
}
