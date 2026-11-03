/*
 * STM32F103 OTA Bootloader。
 *
 * 上电后本程序只做三件事：读取双副本启动记录、检查待升级镜像、跳转到 A 或 B
 * 应用槽位。它不连接 Wi-Fi，也不操作 LCD，因此代码可以保持很小。
 */
#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "bootloader.h"
#include "ota_integrity.h"
#include "ota_metadata.h"
#include "ota_storage.h"

/* 把向量表中的 Reset_Handler 地址转换成“无参数、无返回值”的函数入口。 */
typedef void (*BootloaderEntry_t)(void);

/* 初始化 USART1 的发送脚 PA9，用于在 115200 波特率打印简短启动信息。 */
static void Bootloader_DebugInit(void)
{
    /* 系统复位后的 HSI 为 8MHz；USART1 在 APB2 总线上，也得到 8MHz 时钟。 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* PA9: 复用推挽输出、最高 50MHz，作为 USART1_TX。 */
    GPIOA->CRH &= ~(0xFUL << 4U);
    GPIOA->CRH |= (0xBUL << 4U);

    /* 8MHz / 115200 的 BRR 近似值为 0x45。 */
    USART1->BRR = 0x0045U;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

/* 轮询发送一段固定调试文本；Bootloader 不使用 printf，节省代码空间。 */
static void Bootloader_DebugText(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    while (*text != '\0')
    {
        while ((USART1->SR & USART_SR_TXE) == 0U)
        {
        }
        USART1->DR = (uint8_t)*text;
        text++;
    }
}

/* 从指定槽位首地址读取 MSP 和 Reset_Handler，并检查两者是否属于该槽位。 */
static bool Bootloader_VectorIsValid(OtaSlot_t slot)
{
    uint32_t base = Ota_SlotBase(slot);
    uint32_t msp;
    uint32_t reset_handler;

    if (base == 0U)
    {
        return false;
    }

    msp = *(const uint32_t *)base;
    reset_handler = *(const uint32_t *)(base + sizeof(uint32_t));
    return Ota_VectorWordsAreValid(slot, msp, reset_handler);
}

/* 用下载结束时 STM32 保存的 CRC32 复查程序，不能只看启动入口。 */
static bool Bootloader_PendingImageIsValid(const OtaBootRecord_t *record)
{
    OtaSlot_t pending_slot;
    uint32_t base;

    if (!OtaRecord_IsValid(record))
    {
        return false;
    }

    pending_slot = (OtaSlot_t)record->pending_slot;
    if (pending_slot == OTA_SLOT_NONE)
    {
        return false;
    }

    base = Ota_SlotBase(pending_slot);
    if (!Bootloader_VectorIsValid(pending_slot))
    {
        return false;
    }

    return Ota_Crc32((const uint8_t *)base, record->pending_size) ==
           record->pending_crc32;
}

/* 清除校验失败或试启动次数耗尽的新程序标记，保留原先已确认区。 */
static void Bootloader_ClearPending(const OtaBootRecord_t *record)
{
    OtaBootRecord_t rollback_record;

    rollback_record = OtaRecord_Make(record->generation + 1U,
                                     (OtaSlot_t)record->active_slot,
                                     OTA_SLOT_NONE, 0U, 0U);
    (void)OtaStorage_Save(&rollback_record);
}

OtaSlot_t Bootloader_SelectSlot(void)
{
    OtaBootRecord_t record;
    OtaBootRecord_t next_record;
    OtaSlot_t active_slot;
    OtaSlot_t pending_slot;

    /*
     * 决策顺序：
     * 1. 没有元数据：仅尝试出厂槽 A；
     * 2. 有 pending：校验镜像和次数，合法则先记次数再试启动；
     * 3. 无 pending 或 pending 失败：验证并启动 active；
     * 4. 所有候选都无效：返回 NONE，main 留在 Bootloader。
     */
    /* 首次 ST-Link 下载尚未写元数据时，允许从槽位 A 进行一次出厂启动。 */
    if (!OtaStorage_Load(&record))
    {
        return Bootloader_VectorIsValid(OTA_SLOT_A) ? OTA_SLOT_A : OTA_SLOT_NONE;
    }

    active_slot = (OtaSlot_t)record.active_slot;
    pending_slot = (OtaSlot_t)record.pending_slot;

    if (pending_slot != OTA_SLOT_NONE)
    {
        /* 下载不完整、CRC 错误或试启动次数用尽，均回退到稳定槽位。 */
        if ((!Bootloader_PendingImageIsValid(&record)) ||
            (record.attempt_count >= OTA_BOOT_MAX_ATTEMPTS))
        {
            Bootloader_ClearPending(&record);
            return Bootloader_VectorIsValid(active_slot) ? active_slot : OTA_SLOT_NONE;
        }

        /* 先持久化“本次已尝试”，再跳新固件，掉电也不会无限循环试启动。 */
        next_record = record;
        next_record.generation++;
        OtaRecord_SetAttemptCount(&next_record, record.attempt_count + 1U);
        if (OtaStorage_Save(&next_record) != HAL_OK)
        {
            /* 无法安全记录次数时，不冒险启动待确认固件。 */
            return Bootloader_VectorIsValid(active_slot) ? active_slot : OTA_SLOT_NONE;
        }
        return pending_slot;
    }

    return Bootloader_VectorIsValid(active_slot) ? active_slot : OTA_SLOT_NONE;
}

void Bootloader_JumpToSlot(OtaSlot_t slot)
{
    uint32_t base = Ota_SlotBase(slot);
    uint32_t new_msp;
    uint32_t reset_handler;
    uint32_t index;

    if (!Bootloader_VectorIsValid(slot))
    {
        return;
    }

    /* Cortex-M 向量表第 0 项是初始 MSP，第 1 项是 Reset_Handler。 */
    new_msp = *(const uint32_t *)base;
    reset_handler = *(const uint32_t *)(base + sizeof(uint32_t));

    /*
     * 清理 Bootloader 留下的运行环境。若直接跳转，旧 SysTick 或挂起中断可能在
     * 新应用初始化完成前触发，并使用错误的向量表执行处理函数。
     */
    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    /* STM32F103 共使用两组 NVIC 中断寄存器，跳转前全部关闭并清除挂起位。 */
    for (index = 0U; index < 2U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /* 从此以后所有异常和中断都使用目标应用自己的向量表。 */
    SCB->VTOR = base;
    __DSB();
    __ISB();
    /* 最后才切换主栈；切换后不要再依赖 Bootloader 栈上的局部变量。 */
    __set_MSP(new_msp);
    /*
     * 前面为了安全切换程序关闭了全部中断。
     * 新应用的 HAL_Delay() 要依靠 SysTick 中断计时，
     * 所以进入新应用的 Reset_Handler 前必须重新打开中断。
     */
    __enable_irq();
    ((BootloaderEntry_t)reset_handler)();

    /* Reset_Handler 正常情况下永不返回；若异常返回则安全停在这里。 */
    while (1)
    {
    }
}

int main(void)
{
    OtaSlot_t slot;

    Bootloader_DebugInit();
    slot = Bootloader_SelectSlot();

    if (slot == OTA_SLOT_A)
    {
        Bootloader_DebugText("BOOT: slot A\r\n");
    }
    else if (slot == OTA_SLOT_B)
    {
        Bootloader_DebugText("BOOT: slot B\r\n");
    }
    else
    {
        Bootloader_DebugText("BOOT: no valid app\r\n");
    }

    Bootloader_JumpToSlot(slot);
    while (1)
    {
    }
}
