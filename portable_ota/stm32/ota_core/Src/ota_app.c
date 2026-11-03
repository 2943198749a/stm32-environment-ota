/*
 * STM32 应用侧 A/B OTA Flash 写入实现。
 *
 * 第一次阅读只跟着四个动作走：
 *   1. Begin：检查能不能升级，然后擦除另一个区。
 *   2. Write：收到一块程序数据，就写入一块；会被反复调用。
 *   3. Finalize：检查是否收齐、是否完整，记录“下次试启动新程序”。
 *   4. ConfirmRunning：重启进入新程序后，确认采用新程序。
 * 注意：Finalize 不负责重启，也不表示新程序已经运行成功。
 *
 * 例如现在运行 A、准备升级 B：
 *   running_slot = A（当前正在运行），target_slot = B（本次写入目标）；
 *   下载完成后 active_slot 仍是 A，pending_slot 是 B（等待试运行）；
 *   B 启动并确认后，active_slot 才变成 B，pending_slot 清空。
 * SHA-256 可以先理解为整份程序的“摘要”，暂时不必阅读算法内部。
 */
#include <string.h>
#include "ota_app.h"
#include "ota_integrity.h"
#include "ota_metadata.h"
#include "ota_storage.h"
#include "sha256.h"

/* 一次下载的 RAM 上下文，仅由 OTA 任务访问；不改变 Flash 启动记录格式。 */
typedef struct
{
    bool busy;                         /* 已接受下载，尚未完成或中止。 */
    OtaSlot_t running_slot;             /* 正在运行、禁止擦写的槽位。 */
    OtaSlot_t target_slot;              /* 本次下载写入的槽位。 */
    uint32_t total_bytes;              /* 声明的固件总长度。 */
    uint32_t written_bytes;            /* 已写入且读回校验通过的长度。 */
    char expected_sha256[65];          /* 预期的小写十六进制摘要。 */
    Sha256Context_t sha256;            /* 分块更新的 SHA-256 计算状态。 */
} OtaDownloadContext_t;

static OtaDownloadContext_t download;

/* 辅助函数声明：这里只告诉编译器函数名称，具体实现放在主流程后。 */
static bool OtaApp_IsSha256TextValid(const char text[65]);
static HAL_StatusTypeDef OtaApp_EraseTarget(OtaSlot_t target_slot, uint32_t image_size);

/**
 * @brief 初始化下载上下文，清除进度、预期摘要和 SHA-256 中间状态。
 * 无参数、无返回值；清零后 busy=false，两个槽位均为 OTA_SLOT_NONE。
 * 仅修改 RAM，不擦写 Flash；在启动或中止下载时由 OTA 任务调用。
 */
void OtaApp_Init(void)
{
    (void)memset(&download, 0, sizeof(download));
}

/**
 * @brief 检查升级请求并擦除目标固件占用的 Flash 页，建立新的下载会话。
 * running_slot 为当前运行槽；target_slot 必须是另一有效槽；image_size 为非零偶数字节数。
 * expected_sha256 必须指向 64 个小写十六进制字符及结束符；成功返回 HAL_OK。
 * 忙碌或参数不合法返回 HAL_ERROR；擦除失败透传 HAL 状态，成功后初始化摘要计算。
 */
HAL_StatusTypeDef OtaApp_Begin(OtaSlot_t running_slot,
                               OtaSlot_t target_slot,
                               uint32_t image_size,
                               const char expected_sha256[65])
{
    HAL_StatusTypeDef status;

    /* 忙碌、目标分区或摘要不合法时，不得擦除 Flash。 */
    if (download.busy ||
        !OtaApp_TargetSlotIsAllowed(running_slot, target_slot, image_size) ||
        !OtaApp_IsSha256TextValid(expected_sha256))
    {
        return HAL_ERROR;
    }

    /* 检查通过后擦除目标区。例如运行 A，就只能擦除 B。 */
    status = OtaApp_EraseTarget(target_slot, image_size);
    if (status != HAL_OK)
    {
        return status;
    }

    /* 保存下载信息，后续 Write 会使用。 */
    OtaApp_Init();
    download.busy = true;
    download.running_slot = running_slot;
    download.target_slot = target_slot;
    download.total_bytes = image_size;
    memcpy(download.expected_sha256, expected_sha256,
           sizeof(download.expected_sha256));
    Sha256_Init(&download.sha256);
    return HAL_OK;
}

/**
 * @brief 按顺序写入一块固件，读回确认后更新摘要和下载进度。
 * offset 必须等于已写入字节数；data 指向 length 字节，长度非零、为偶数且不超过块长限制。
 * 成功返回 HAL_OK；参数、越界或读回不一致返回 HAL_ERROR，Flash 失败透传 HAL 状态。
 * 以半字编程并重新锁定 Flash；失败时不推进进度，但 Flash 可能已部分写入。
 */
HAL_StatusTypeDef OtaApp_Write(uint32_t offset,
                               const uint8_t *data,
                               uint32_t length)
{
    uint32_t target_address;
    uint32_t index;
    uint16_t halfword;
    HAL_StatusTypeDef status;

    /* 必须已开始下载，数据非空，块长不超过限制且为偶数。 */
    if (!download.busy || (data == NULL) || (length == 0U) ||
        (length > OTA_APP_WRITE_MAX_BYTES) || ((length & 1U) != 0U))
    {
        return HAL_ERROR;
    }
    /* 只能接着上一块写，且不能超过声明的总长度。 */
    if ((offset != download.written_bytes) ||
        (length > (download.total_bytes - download.written_bytes)))
    {
        return HAL_ERROR;
    }

    /* 二、找到实际写入地址。分区起点 + 已写入的字节数 = 本块起点。 */
    target_address = Ota_SlotBase(download.target_slot) + offset;
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return status;
    }

    for (index = 0U; index < length; index += 2U)
    {
        /* STM32F1 Flash 最小写入单位是 16 位，低字节先放在低位。 */
        halfword = (uint16_t)data[index] | ((uint16_t)data[index + 1U] << 8U);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   target_address + index, halfword);
        if (status != HAL_OK)
        {
            break;
        }
    }
    (void)HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        return status;
    }

    /* 三、读回比较，确认 Flash 里的数据和收到的数据相同。 */
    if (memcmp((const void *)target_address, data, length) != 0)
    {
        return HAL_ERROR;
    }

    /* 四、把这一块加入整包摘要，再累计已写入长度。 */
    Sha256_Update(&download.sha256, data, length);
    download.written_bytes += length;
    return HAL_OK;
}

/**
 * @brief 收尾下载：检查总长度、SHA-256、向量表，并保存待试启动记录。
 * 无参数；成功返回 HAL_OK 并清除 busy，实际重启由调用方安排。
 * 记录保留原 active_slot，把目标槽写入 pending_slot，供 Bootloader 试运行。
 * 未开始或未收齐时返回 HAL_ERROR；摘要、向量、活动槽或保存失败还会中止下载。
 */
HAL_StatusTypeDef OtaApp_Finalize(void)
{
    OtaBootRecord_t old_record;
    OtaBootRecord_t pending_record;
    uint8_t digest[32];
    uint32_t target_base;
    uint32_t initial_msp;
    uint32_t reset_handler;
    uint32_t image_crc32;
    uint32_t generation = 1U;

    if (!download.busy) /* 没有进行中的下载，不能结束下载。 */
    {
        return HAL_ERROR;
    }
    /* 一、必须收齐全部数据。少一个字节都不能标记成待启动程序。 */
    if (download.written_bytes != download.total_bytes)
    {
        return HAL_ERROR;
    }

    /* 二、比较整包摘要，判断收到的程序是否与预期一致。 */
    Sha256_Final(&download.sha256, digest);
    if (!Sha256_HexEquals(digest, download.expected_sha256))
    {
        OtaApp_Abort();
        return HAL_ERROR;
    }

    /* 三、检查程序开头的栈地址和启动入口，排除明显不能启动的内容。 */
    target_base = Ota_SlotBase(download.target_slot);
    initial_msp = *(const uint32_t *)target_base;
    reset_handler = *(const uint32_t *)(target_base + sizeof(uint32_t));
    if (!Ota_VectorWordsAreValid(download.target_slot, initial_msp, reset_handler))
    {
        OtaApp_Abort();
        return HAL_ERROR;
    }

    /* 四、为 Flash 中的程序计算 CRC32，存进记录，供 Bootloader 下次复查。 */
    image_crc32 = Ota_Crc32((const uint8_t *)target_base, download.total_bytes);

    if (OtaStorage_Load(&old_record))
    {
        /* 当前应用必须与已确认槽位一致，避免错误镜像修改启动记录。 */
        if ((OtaSlot_t)old_record.active_slot != download.running_slot)
        {
            OtaApp_Abort();
            return HAL_ERROR;
        }
        generation = old_record.generation + 1U;
    }

    /* 五、记录“下次试启动目标区”。旧的 active 仍保留，确认前不替换它。 */
    pending_record = OtaRecord_Make(generation, download.running_slot,
                                    download.target_slot,
                                    download.total_bytes, image_crc32);
    if (OtaStorage_Save(&pending_record) != HAL_OK)
    {
        OtaApp_Abort();
        return HAL_ERROR;
    }

    download.busy = false;
    return HAL_OK;
}

/**
 * @brief 取消当前下载并复用初始化逻辑清空全部 RAM 会话状态。
 * 无参数、无返回值；可用于接收中止命令或处理下载校验失败。
 * 不会擦除已写入的目标数据，也不会修改 Flash 中已有的启动记录。
 */
void OtaApp_Abort(void)
{
    /* 仅清 RAM 状态；被中断下载的非活动槽位不会被 Bootloader 采用。 */
    OtaApp_Init();
}

/**
 * @brief 判断当前运行镜像是否为启动记录中的待确认固件。
 * running_slot 为实际运行的 A/B 槽；仅当最新有效记录的 pending_slot 与其一致时返回 true。
 * 槽位无效或无有效记录返回 false；只读 Flash，不更新启动次数或记录。
 */
bool OtaApp_IsConfirmationRequired(OtaSlot_t running_slot)
{
    OtaBootRecord_t record;

    if ((running_slot != OTA_SLOT_A) && (running_slot != OTA_SLOT_B))
    {
        return false;
    }

    if (!OtaStorage_Load(&record))
    {
        return false;
    }

    return (OtaSlot_t)record.pending_slot == running_slot;
}

/**
 * @brief 在新固件完成自检后，将当前运行槽确认为正式活动槽。
 * running_slot 必须为 A/B；已确认则直接返回 HAL_OK，有 pending 时必须与当前槽一致。
 * 无记录时建立首条记录；保存时递增代数并清空 pending、长度、CRC 和试启动次数。
 * 槽位或确认关系不合法返回 HAL_ERROR；否则返回保存结果，可能擦写元数据页。
 */
HAL_StatusTypeDef OtaApp_ConfirmRunning(OtaSlot_t running_slot)
{
    OtaBootRecord_t old_record;
    OtaBootRecord_t confirmed_record;
    uint32_t generation = 1U;

    if ((running_slot != OTA_SLOT_A) && (running_slot != OTA_SLOT_B))
    {
        return HAL_ERROR;
    }

    if (OtaStorage_Load(&old_record))
    {
        if ((OtaSlot_t)old_record.active_slot == running_slot &&
            (OtaSlot_t)old_record.pending_slot == OTA_SLOT_NONE)
        {
            return HAL_OK;
        }

        /* 只有 Bootloader 刚试启动的 pending 槽位，才能把自己确认成 active。 */
        if ((OtaSlot_t)old_record.pending_slot != running_slot)
        {
            return HAL_ERROR;
        }
        generation = old_record.generation + 1U;
    }

    /* 确认的本质：把当前槽升级为 active，并一次性清空 pending 状态。 */
    confirmed_record = OtaRecord_Make(generation, running_slot,
                                      OTA_SLOT_NONE, 0U, 0U);
    return OtaStorage_Save(&confirmed_record);
}

/**
 * @brief 查询应用下载会话是否仍在进行。
 * 无参数；返回 true 表示 Begin 已成功且尚未成功 Finalize 或 Abort。
 * 只读 RAM；供任务协调使用，不提供锁或跨任务同步。
 */
bool OtaApp_IsBusy(void)
{
    return download.busy;
}

/**
 * @brief 检查固件尺寸及运行槽、目标槽组合是否允许进行升级。
 * running_slot 和 target_slot 必须是不同的 A/B 槽；image_size 必须为非零偶数且不超过槽容量。
 * 满足所有条件返回 true，否则返回 false；纯参数检查，不触碰 Flash。
 */
bool OtaApp_TargetSlotIsAllowed(OtaSlot_t running_slot,
                                OtaSlot_t target_slot,
                                uint32_t image_size)
{
    /* 程序非空、放得下且长度为偶数；两个分区编号都必须有效。 */
    if (!Ota_ImageSizeIsValid(image_size) || ((image_size & 1U) != 0U) ||
        ((running_slot != OTA_SLOT_A) && (running_slot != OTA_SLOT_B)) ||
        ((target_slot != OTA_SLOT_A) && (target_slot != OTA_SLOT_B)))
    {
        return false;
    }

    /* A 运行时只能写 B；B 运行时只能写 A。 */
    return running_slot != target_slot;
}

/**
 * @brief 检查预期 SHA-256 文本是否严格符合本模块格式。
 * text 为待检查字符串；有效文本为 64 个小写十六进制字符，之后紧跟结束符。
 * 空指针、非法字符或长度不符返回 false；不修改输入，正确时返回 true。
 */
static bool OtaApp_IsSha256TextValid(const char text[65])
{
    uint32_t index;
    char value;

    if (text == NULL)
    {
        return false;
    }

    for (index = 0U; index < 64U; index++)
    {
        value = text[index];
        if (!(((value >= '0') && (value <= '9')) ||
              ((value >= 'a') && (value <= 'f'))))
        {
            return false;
        }
    }
    return text[64] == '\0';
}

/**
 * @brief 按镜像长度向上取整到 Flash 页，擦除目标槽对应范围。
 * target_slot 和 image_size 须由调用方先校验；本函数不重复检查槽位和长度。
 * 返回解锁或擦除的 HAL 状态；成功解锁后无论擦除结果如何都会尝试重新锁定 Flash。
 */
static HAL_StatusTypeDef OtaApp_EraseTarget(OtaSlot_t target_slot, uint32_t image_size)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = Ota_SlotBase(target_slot);
    erase.NbPages = (image_size + OTA_FLASH_PAGE_SIZE - 1U) / OTA_FLASH_PAGE_SIZE;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_FLASHEx_Erase(&erase, &page_error);
    (void)HAL_FLASH_Lock();
    return status;
}

