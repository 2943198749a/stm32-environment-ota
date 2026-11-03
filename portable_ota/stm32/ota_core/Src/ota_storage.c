/*
 * STM32F103 内部 Flash 中 OTA 元数据的读写实现。
 *
 * 元数据 A、B 各占一个完整 Flash 页，每页开头保存一条 32 字节记录。
 * 读取时检查两页；保存时总写到“当前最新记录的另一页”。新页擦写期间旧页
 * 保持不动，因此中途掉电后仍至少有一份可用于回滚的旧启动状态。
 */
#include <string.h>
#include "ota_storage.h"

/**
 * @brief 将元数据页的固定 Flash 地址转换为只读记录指针。
 * address 必须为调用方提供的有效且正确对齐的内存映射地址。
 * 返回指向 Flash 的指针，不复制内容、不校验记录，也不执行擦写。
 */
static const OtaBootRecord_t *OtaStorage_RecordAt(uint32_t address)
{
    return (const OtaBootRecord_t *)address;
}

/**
 * @brief 把一份启动记录依次按 16 位半字写入 Flash。
 * address 为已擦除且对齐的目标地址；record 指向有效记录，调用方负责提前解锁 Flash。
 * 全部半字写入成功返回 HAL_OK，任一步失败立即返回 HAL 状态；不负责锁定或读回验证。
 */
static HAL_StatusTypeDef OtaStorage_ProgramRecord(uint32_t address,
                                                   const OtaBootRecord_t *record)
{
    const uint16_t *halfword = (const uint16_t *)record;
    uint32_t index;
    HAL_StatusTypeDef status;

    /* STM32F1 最小编程单位是 16 位，因此 32 字节记录需要写 16 次。 */
    for (index = 0U; index < (sizeof(OtaBootRecord_t) / sizeof(uint16_t)); index++)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   address + index * sizeof(uint16_t),
                                   halfword[index]);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

/**
 * @brief 读取双副本元数据中代数最新的有效记录。
 * record 为 RAM 输出指针；为空或两个副本均无效时返回 false，不写输出。
 * 成功复制完整记录并返回 true；只读 Flash，副本选择及 CRC 校验由纯逻辑层完成。
 */
bool OtaStorage_Load(OtaBootRecord_t *record)
{
    const OtaBootRecord_t *newest;

    if (record == NULL)
    {
        return false;
    }

    /* Flash 是内存映射的，可直接把两个固定地址交给纯逻辑层检查。 */
    newest = OtaRecord_SelectNewest(OtaStorage_RecordAt(OTA_METADATA_RECORD_A_BASE),
                                    OtaStorage_RecordAt(OTA_METADATA_RECORD_B_BASE));
    if (newest == NULL)
    {
        return false;
    }

    /* 使用 memcpy，避免编译器因结构体对齐方式不同而生成不安全访问。 */
    memcpy(record, newest, sizeof(OtaBootRecord_t));
    return true;
}

/**
 * @brief 把合法启动记录保存到最新有效副本的另一页，保留旧页用于掉电恢复。
 * record 为待保存记录；必须通过字段与 CRC 校验，否则返回 HAL_ERROR。
 * 无有效副本时写 A 页；解锁后擦页、半字编程并重新锁定，再校验目标页记录。
 * 成功返回 HAL_OK，读回记录无效返回 HAL_ERROR，解锁、擦除或编程失败返回相应 HAL 状态。
 */
HAL_StatusTypeDef OtaStorage_Save(const OtaBootRecord_t *record)
{
    const OtaBootRecord_t *first = OtaStorage_RecordAt(OTA_METADATA_RECORD_A_BASE);
    const OtaBootRecord_t *second = OtaStorage_RecordAt(OTA_METADATA_RECORD_B_BASE);
    const OtaBootRecord_t *newest;
    uint32_t target_page;
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    if (!OtaRecord_IsValid(record))
    {
        return HAL_ERROR;
    }

    newest = OtaRecord_SelectNewest(first, second);
    /* 没有旧记录时先使用 A 页；有旧记录时总写入另一页。 */
    target_page = (newest == first) ? OTA_METADATA_RECORD_B_BASE :
                                    OTA_METADATA_RECORD_A_BASE;

    /* 只擦目标页；另一页在写入并校验成功前始终保留为安全副本。 */
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = target_page;
    erase.NbPages = 1U;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status == HAL_OK)
    {
        status = OtaStorage_ProgramRecord(target_page, record);
    }
    (void)HAL_FLASH_Lock();

    /* 重新按完整记录规则校验读回内容，编程异常不能被当成保存成功。 */
    if ((status == HAL_OK) &&
        (!OtaRecord_IsValid(OtaStorage_RecordAt(target_page))))
    {
        return HAL_ERROR;
    }
    return status;
}
